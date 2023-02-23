#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>

#ifdef WITH_SYSTEMD
#include <systemd/sd-bus.h>
#endif
#ifdef WITH_ELOGIND
#include <elogind/sd-bus.h>
#endif
#ifdef WITH_BASU
#include <basu/sd-bus.h>
#endif

#define SIGPLUS SIGRTMIN
#define SIGMINUS SIGRTMIN
#define LENGTH(X) (sizeof(X) / sizeof (X[0]))
#define CMDLENGTH 50
#define MIN( a, b ) ( ( a < b) ? a : b )
#define STATUSLENGTH (LENGTH(blocks) * CMDLENGTH + 1)

typedef struct {
    char* icon;
    char* command;
    unsigned int interval;
} Block;

typedef struct node_ {
    char *name;
    struct node_ *next;
} node;

static void list_add(node **root, const char* str);
static const char* list_get(node *root, int index);
static void list_clear(node *root);


void getcmds(int time);
int getstatus(char *str, char *last);
void statusloop();
int setupdbus();
void sendstatus();
void termhandler();

#include "blocks.h"

static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[2][STATUSLENGTH];
static int statusContinue = 1;
static int returnStatus = 0;


sd_bus *dbus;


void list_add(node **root, const char* str)
{
    node *current = *root;
    node *new = calloc(1, sizeof(node));
    new->name = strdup(str);

    if (!current)
    {
        *root = new;
        return;
    }

    while (current->next) current = current->next;
    current->next = new;
}

const char* list_get(node *root, int index)
{
    node *current = root;

    while (index && current)
    {
        current = current->next;
        --index;
    }

    if (index)
        return NULL;

    return current->name;
}

void list_clear(node *root)
{
    node *current = root;
    node *tmp;

    while (current)
    {
        tmp = current;
        current = current->next;

        free(tmp->name);
        free(tmp);
    }
}




void getcmd(const Block *block, char *output)
{
    int i;
    FILE *cmdf;
   
    strcpy(output, block->icon);
    cmdf = popen(block->command, "r");
    if (!cmdf)
        return;

    i = strlen(block->icon);
    fgets(output+i, CMDLENGTH-i-delimLen, cmdf);
    i = strlen(output);

    if (i == 0)
        return;

    if (delim[0] != '\0')
    {
        i = output[i-1] == '\n' ? i-1 : i;
        strncpy(output+i, delim, delimLen);
    }
    else
        output[i++] = '\0';

    pclose(cmdf);
}

void getcmds(int time)
{
    const Block* current;
    for (unsigned int i = 0; i < LENGTH(blocks); i++)
    {
        current = blocks + i;
        if ((current->interval != 0 && time % current->interval == 0) || time == -1)
            getcmd(current,statusbar[i]);
    }
}

int getstatus(char *str, char *last)
{
    strcpy(last, str);
    str[0] = '\0';

    for (unsigned int i = 0; i < LENGTH(blocks); i++)
        strcat(str, statusbar[i]);

    str[strlen(str)-strlen(delim)] = '\0';
    return strcmp(str, last);
}

void sendstatus()
{
    int r = 0;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *message = NULL;

    /*  Update if text has changed */
    if (!getstatus(statusstr[0], statusstr[1]))
        return;

    /* Hardcoded in edwl */
    r = sd_bus_call_method(dbus, "net.wm.edwl", "/net/wm/edwl", "net.wm.edwl", "SetStatus", &error, &message, "s", statusstr[0]);

    if (r < 0)
    {
        printf("Error on sending %s\n", error.message);
        sd_bus_error_free(&error);
        sd_bus_message_unref(message);
    }
}

void sendrandomimageonbackground()
{
    DIR *d;
    struct dirent *dir;
    const char *extension;
    char* full_path;
    node *png_list = NULL;
    int amount_of_pngs = 0;
    int string_length = 0;
    int r = 0;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *message = NULL;

    d = opendir(backgroundimagesdir);

    if (d)
    {
        /* If dir exists - scaning */
        dir = readdir(d);
        while (dir != NULL)
        {
            if ( dir->d_type == DT_REG )
            {
                /* looking for files with extension .png */
                extension = ((char*)dir->d_name) + (strlen(dir->d_name) - 4);
                if (!strcmp(extension, ".png") || !strcmp(extension, ".jpg") || !strcmp(extension, "jpeg"))
                {
                    list_add(&png_list, dir->d_name);
                    ++amount_of_pngs;
                }
            }
            dir = readdir(d);
        }
        closedir(d);

        /* If we have at least 1 */
        if (amount_of_pngs)
        {
            /* Getting random png */
            int random_image_index = rand() % amount_of_pngs;

            /* Forming a full path on it */
            if (backgroundimagesdir[strlen(backgroundimagesdir)] == '/')
            {
                string_length = snprintf(NULL, 0, "%s%s", backgroundimagesdir, list_get(png_list, random_image_index)) + 1;
                full_path = malloc(string_length);

                snprintf(full_path, string_length, "%s%s", backgroundimagesdir, list_get(png_list, random_image_index));
            } else
            {
                string_length = snprintf(NULL, 0, "%s/%s", backgroundimagesdir, list_get(png_list, random_image_index)) + 1;
                full_path = malloc(string_length);

                snprintf(full_path, string_length, "%s/%s", backgroundimagesdir, list_get(png_list, random_image_index));
            }

            /* Sending to edwl */
            r = sd_bus_call_method(dbus, "net.wm.edwl", "/net/wm/edwl", "net.wm.edwl", "SetBackgroundPath", &error, &message, "s", full_path);

            if (r < 0)
            {
                sd_bus_error_free(&error);
                sd_bus_message_unref(message);
            }

            free(full_path);
            list_clear(png_list);
        }
    }
}


void statusloop()
{
    int i = 0;
    getcmds(-1);
    while (1)
    {
        getcmds(i++);
        sendstatus();

        if (backgroundinterval && i % backgroundinterval == 0)
            sendrandomimageonbackground();

        if (!statusContinue)
            break;
        sleep(1.0);
    }
}

void termhandler()
{
    statusContinue = 0;
}

int main(int argc, char** argv)
{
    for (int i = 0; i < argc; i++)
    {
        if (!strcmp("-d",argv[i]))
            strncpy(delim, argv[++i], delimLen);
    }

    srand(time(NULL));

    delimLen = MIN(delimLen, strlen(delim));
    delim[delimLen++] = '\0';
    signal(SIGTERM, termhandler);
    signal(SIGINT, termhandler);

    /* Message sending via dbus */
    if (sd_bus_open_user(&dbus) < 0)
        return 1;

    statusloop();

    sd_bus_unref(dbus);
    return 0;
}
