#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/stat.h>
#include <limits.h>
#include <arpa/inet.h>
#include <jpeglib.h>
#ifndef __FreeBSD__
#include <linux/input-event-codes.h>
#endif

#include "tray.h"

#ifdef __FreeBSD__
#define BTN_LEFT    0x110
#define BTN_RIGHT   0x111
#define BTN_FORWARD 0x115
#define BTN_BACK 0x116
#endif

/** NECESSARY CONSTANTS **/
sd_bus *__dbus = NULL;
int continue_process = 1;
const char *obj_path = "/StatusNotifierWatcher";
Tray *__tray = NULL;
char *default_theme_name = "default";

typedef struct {
    const char header[4];
    cairo_surface_t *(*func)(const char *path);
} ImageProcessingByHeaders;

/** Image load utils **/
cairo_surface_t *load_image_jpg(const char *path)
{
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr error;
    cairo_format_t format;
    cairo_surface_t *surface = NULL;
    FILE *f;
    unsigned char *surface_buffer = NULL;
    JSAMPARRAY buffer;
    int row_stride, actual_stride;
    int current_row = 0;

    f = fopen(path, "rb");

    if (!f)
        return NULL;

    /* Init jpeg structs */
    info.err = jpeg_std_error(&error);
    jpeg_create_decompress(&info);
    jpeg_stdio_src(&info, f);

    /* Reading headers and starting decompression*/
    jpeg_read_header(&info, 0);

    if (!jpeg_start_decompress(&info))
        goto unsuccess;

    /* defining main variables  */
    row_stride = info.output_width * info.output_components;
    format = info.output_components == 4 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    surface = cairo_image_surface_create(format, info.output_width, info.output_height);
    /*  Actual stride may not be equal to the surface's, because of align */
    actual_stride = cairo_format_stride_for_width(format, info.output_width);

    buffer = (*info.mem->alloc_sarray)((j_common_ptr)&info, JPOOL_IMAGE, row_stride, 1);

    while (info.output_scanline < info.output_height)
    {
        /* Reading the line */
        jpeg_read_scanlines(&info, buffer, 1);
        surface_buffer = (unsigned char*)(cairo_image_surface_get_data(surface) + actual_stride * current_row);

        /* Puting into the surface a line*/
        for (int i = 0, src_offset = 0, dst_offset = 0; i < info.output_width; i++, src_offset += info.output_components, dst_offset += 4)
        {
            /* Data is stored in BGR format */
            for (int j = 0; j < info.output_components; j++)
            {
                surface_buffer[dst_offset + j] = buffer[0][src_offset + info.output_components - j - 1];
            }
        }
        current_row += 1;
    }

    /* Converting to ARGB */
    if (format == CAIRO_FORMAT_RGB24)
    {
        cairo_surface_t *finished_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, info.output_width, info.output_height);
        cairo_t *cairo = cairo_create(finished_surface);
        cairo_set_source_surface(cairo, surface, 0, 0);
        cairo_paint(cairo);
        cairo_surface_flush(finished_surface);

        cairo_destroy(cairo);
        cairo_surface_destroy(surface);
        surface = finished_surface;
    }

unsuccess:
    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
    fclose(f);

    return surface;
}

cairo_surface_t *load_image_png(const char *path)
{
    return cairo_image_surface_create_from_png(path);
}

const ImageProcessingByHeaders image_processing_by_header[] = {
    {{0xFF, 0xD8, 0xFF, 0xD8}, load_image_jpg},
    {{0xFF, 0xD8, 0xFF, 0xE0}, load_image_jpg},
    {{0xFF, 0xD8, 0xFF, 0xE1}, load_image_jpg},
    {{0x89, 0x50, 0x4E, 0x47}, load_image_png},
};
const int ImageFormatsAmount = 4;

cairo_surface_t *load_image(const char *path)
{
    FILE *f;
    unsigned char magick[4];
    size_t red;

    f = fopen(path, "rb");

    if (!f)
        return NULL;

    red = fread(magick, 1, 4, f);
    fclose(f);

    if (red != 4)
      return NULL;

    for (int i = 0; i < ImageFormatsAmount; i++)
    {
        if (!strncmp(image_processing_by_header[i].header, (char*)magick, 4))
        {
            return image_processing_by_header[i].func(path);
        }
    }   

    return NULL;
}



/** List Utils **/
typedef int list_cmp_function(const void *data_from_list, const void *passed_data);

list_node *list_search_using_function(list_node *start, list_cmp_function *cmp, void *passed_data)
{
    list_node *current_node = start;
    while (current_node)
    {
        if (cmp(current_node->data, passed_data))
            return current_node;

        current_node = current_node->next;
    }

    return NULL;
}

list_node *list_append(list_node **start, void *data)
{
    list_node *last_node_in_list;

    if ((*start) == NULL)
    {
        *start = calloc(1, sizeof(list_node));
        (*start)->data = data;
        return *start;
    }

    last_node_in_list = *start;
    while (last_node_in_list->next != NULL)
        last_node_in_list = last_node_in_list->next;

    last_node_in_list->next = calloc(1, sizeof(list_node));
    last_node_in_list->next->data = data;
    return last_node_in_list->next;
}


list_node *list_last(list_node *start)
{
    if (!start)
        return NULL;

    while (start->next) { start = start->next; }

    return start;
}

void list_concatinate(list_node **first, list_node *second)
{
    list_node *last_in_first = list_last(*first);

    if (last_in_first == NULL)
        *first = second;
    else
        last_in_first->next = second;
}

void list_remove(list_node **start, list_node *node)
{
    list_node *prev_node;

    if (!*start || !node)
        return;

    if (*start == node)
    {
        prev_node = (*start)->next;
        free((*start)->data);
        free(*start);
        *start = prev_node;
        return;
    }

    prev_node = *start;
    for (list_node* current = prev_node->next; current != NULL;)
    {
        if (current == node)
        {
            prev_node->next = current->next;
            free(current->data);
            free(current);
            return;
        }
        prev_node = current;
        current = current->next;
    }
}


void list_exclude(list_node **start, list_node *node)
{
    list_node *prev_node;

    if (!*start || !node)
        return;

    if (*start == node)
    {
        prev_node = (*start)->next;
        free(*start);
        *start = prev_node;
        return;
    }

    prev_node = *start;
    for (list_node* current = prev_node->next; current != NULL;)
    {
        if (current == node)
        {
            prev_node->next = current->next;
            free(current);
            return;
        }
        prev_node = current;
        current = current->next;
    }
}

char **list_of_strings_to_array(list_node *start)
{
    int length = 0, i = 0;
    list_node *current = start;
    char **result = NULL;

    while (current)
    {
        ++length;
        current = current->next;
    }

    if (!length)
        return NULL;

    result = (char**)malloc(sizeof(char*) * (length + 1));
    current = start;

    while (current)
    {
        result[i++] = (char*)current->data;
        current = current->next;
    }
    result[i] = NULL;

    return result;
}

void list_destroy(list_node **list)
{
    list_node *current = NULL, *next = NULL;

    if (!(*list))
        return;

    while (current)
    {
        next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
    *list = NULL;
}

void list_clear(list_node **list)
{
    list_node *current = NULL, *next = NULL;

    if (!(*list))
        return;

    while (current)
    {
        next = current->next;
        free(current);
        current = next;
    }
    *list = NULL;
}

int list_length(list_node *start)
{
    int length = 0;
    while (start)
    {
        length += 1;
        start = start->next;
    }
    return length;
}


/*  Helper function  */
int cmp_strings(const void *s1, const void *s2) {return !strcmp((const char*)s1, (const char*)s2);}
int cmp_strings_kde(const void *s1, const void *s2) {return !strncmp((const char*)s1, (const char*)s2, strlen((const char*)s2));}



/** Icons searcher **/

int cmp_group_id(const void *item, const void *cmp_to) { return !strcmp(item, cmp_to); }

list_node* split_into_list(char *line, char d)
{
    list_node *r = NULL;
    char *current = line;
    char *part;
    int i;

    for (i = 0; line[i] != '\0'; i++)
    {
        if (line[i] == d)
        {
            part = strndup(current, ((&line[i]) - current));
            current = &line[i+1];
            list_append(&r, part);
        }
    }

    part = strndup(current, ((&line[i]) - current));
    list_append(&r, part);

    return r;
}

list_node *default_icon_dirs(void)
{
    list_node *result = NULL, *result_expanded = NULL;
    char *data_home = getenv("XDG_DATA_HOME");
    char *data_dirs = getenv("XDG_DATA_DIRS");
    char *dir = NULL, *path = NULL;
    size_t path_len;
    wordexp_t p;
    struct stat sb;

    list_append(&result, strdup("$HOME/.icons"));
    list_append(&result, strdup(data_home && *data_home ? "$XDG_DATA_HOME/icons" : "$HOME/.local/share/icons"));
    list_append(&result, strdup("/usr/share/pixmaps"));

    if (!(data_dirs && *data_dirs))
        data_dirs = "/usr/local/share:/usr/share";

    data_dirs = strdup(data_dirs);
    dir = strtok(data_dirs, ":");
    do {
        path_len = snprintf(NULL, 0, "%s/icons", dir) + 1;
        path = malloc(path_len);
        snprintf(path, path_len, "%s/icons", dir);
        list_append(&result, path);
    } while ((dir = strtok(NULL, ":")));
    free(data_dirs);

    for_each_list_element(result, current)
    {
        if (!wordexp(current->data, &p, WRDE_UNDEF))
        {
            if (stat(p.we_wordv[0], &sb) == 0 && S_ISDIR(sb.st_mode))
            {
                list_append(&result_expanded, strdup(p.we_wordv[0]));
            }
            wordfree(&p);
        }
    }

    list_destroy(&result);

    return result_expanded;
}

void theme_destroy(theme *t)
{
    if (!t)
        return;

    free(t->name);
    list_destroy(&t->inherits);
    list_destroy(&t->dirs);
    free(t->dir);

    for_each_list_element(t->subdirs, subdir_node)
    {
        theme_subdir *subdir = (theme_subdir*)subdir_node->data;
        free(subdir->name);
    }
    list_destroy(&t->subdirs);
    free(t);

}

int process_group(char *old_group, char *new_group, theme *t)
{
    theme_subdir *subdir;
    if (!old_group)
        return strcmp(new_group, "Icon Theme") ? 1 : 0; //first must be Icon Theme


    if (!strcmp(old_group, "Icon Theme"))
    {
        if (!t->name)
            return 2;
        if (!t->dirs)
            return 3;

        for (char *c = t->name; *c; ++c)
            if (*c == ',' || *c == ' ')
                return 4;
    } else {
        if (!t->subdirs)
            return 0;

        subdir = (theme_subdir*)list_last(t->subdirs)->data;
        if (!subdir->size)
            return 5;

        switch (subdir->type)
        {
            case Fixed:
                subdir->max_size = subdir->min_size = subdir->size;
                break;
            case Scalable:
                if (!subdir->max_size)
                    subdir->max_size = subdir->size;
                if (!subdir->min_size)
                    subdir->min_size = subdir->size;
                break;

            case Threshold:
                subdir->max_size = subdir->size + subdir->threshold;
                subdir->min_size = subdir->size - subdir->threshold;
                break;
        }
    }

    if (new_group && list_search_using_function(t->dirs, &cmp_group_id, new_group))
    {
        subdir = calloc(1, sizeof(theme_subdir));
        subdir->name = strdup(new_group);
        subdir->threshold = 2;
        list_append(&t->subdirs, (void*)subdir);
    }

    return 0;
}

int process_value(char *group, char *key, char *value, theme* t)
{
    theme_subdir *subdir = NULL;
    char *end;
    int n;

    if (!strcmp(group, "Icon Theme"))
    {
        if (!strcmp(key, "Name"))
            t->name = strdup(value);

        if (!strcmp(key, "Inherits"))
            t->inherits = split_into_list(value, ',');

        if (!strcmp(key, "Directories"))
            t->dirs = split_into_list(value, ',');
        // Ignoring everything else
    } else {
        if (!t->dirs) // skip
            return 0;

        subdir = (theme_subdir*)list_last(t->subdirs)->data;
        if (strcmp(subdir->name, group))
            return 0;

        if (!strcmp(key, "Context"))
            return 0;

        if (!strcmp(key, "Type"))
        {
            if (!strcmp(value, "Fixed"))
                subdir->type = Fixed;
            if (!strcmp(value, "Scalable"))
                subdir->type = Scalable;
            if (!strcmp(value, "Threshold"))
                subdir->type = Threshold;

            return 1;
        }

        n = strtol(value, &end, 10);
        if (*end != '\0')
            return 2;

        if (!strcmp(key, "Size"))
            subdir->size = n;
        if (!strcmp(key, "MaxSize"))
            subdir->max_size = n;
        if (!strcmp(key, "MinSize"))
            subdir->min_size = n;
        if (!strcmp(key, "Threshold"))
            subdir->threshold = n;
    }

    return 0;
}

theme *parse_file(const char *base_dir_name, const char *file_name)
{
    FILE *theme_file = NULL;
    theme *t = NULL;
    list_node *groups = NULL, *last_group = NULL;
    char *line = NULL, *sub_line = NULL;
    char *last_group_name= NULL;
    const char *error = NULL;
    int padding = 0, actual_line_length = 0;
    size_t length = 0;
    ssize_t amount_read;
    int error_code;

    int path_length = snprintf(NULL, 0, "%s/%s/index.theme", base_dir_name, file_name) + 1;
    char *path = (char*)malloc(path_length);

    if (!path)
        return NULL;

    snprintf(path, path_length, "%s/%s/index.theme", base_dir_name, file_name);
    theme_file = fopen(path, "r");

    if (!theme_file)
        return NULL;

    free(path);

    t = (theme*)calloc(1, sizeof(theme));

    while ((amount_read = getline(&line, &length, theme_file)) != -1)
    {
        sub_line = line;

        while (isspace(*sub_line)) { ++sub_line; }  // skip leading whitespaces

        if (!(*sub_line) || sub_line[0] == '#') continue;  // skip comments and blank lines

        padding = sub_line - line;

        sub_line = line + amount_read;

        while (isspace(*sub_line) || (*sub_line) == '\0') { --sub_line; } // skip whitespaces at the end
        *(++sub_line) = '\0';

        actual_line_length = (sub_line - line) - padding;
        sub_line = line + padding;

        if (sub_line[0] == '[') // group
        {
            int i = 1;
            while (!iscntrl(sub_line[i]) && line[i] != '[' && line[i] != ']') { ++i; }

            if ((i+1) != actual_line_length || line[i] != ']')
            {
                goto unsuccess;
            }

            sub_line[actual_line_length - 1] = '\0';

            // duplicate check
            if (list_search_using_function(groups, &cmp_group_id, &sub_line[1]))
            {
                goto unsuccess;
            }

            last_group_name = last_group ? (char*)last_group->data : NULL;
            error_code = process_group(last_group_name, &sub_line[1], t);

            if (error_code)
            {
                // TODO print error
                goto unsuccess;
            }

            last_group = list_append(&groups, strdup(&sub_line[1]));
        } else
        {
            int i = 1, j = 0;
            if (!groups)
            {
                goto unsuccess;
            }

            while (isalnum(sub_line[j]) || sub_line[j] == '-') { ++j; } // skiping first word
            i = j;

            while (isspace(sub_line[i])) { ++i; }

            if (sub_line[i] != '=')
            {
                goto unsuccess;
            }

            sub_line[j] = '\0';
            ++j;

            while (isspace(sub_line[j])) { ++j; }

            error_code = process_value((char*)last_group->data, sub_line, &sub_line[j], t);
            if (error)
            {
                // TODO print error
                goto unsuccess;
            }
        }
    }

    t->dir = strdup(file_name);

    if (!t->name)
        t->name = strdup(file_name);

    goto cleanup;

unsuccess:
    theme_destroy(t);
    t = NULL;

cleanup:
    free(line);
    list_destroy(&groups);
    fclose(theme_file);

    return t;
}

list_node *load_themes_from_dir(const char *dir_name)
{
    DIR *dir = opendir(dir_name);
    list_node *themes = NULL;
    struct dirent *file_info;
    theme *t;

    if (!dir)
        return NULL;

    while ((file_info = readdir(dir)))
    {
        /* Hidden dir */
        if (file_info->d_name[0] == '.')
            continue;

        t = parse_file(dir_name, file_info->d_name);

        if (t) {
            list_append(&themes, (void*)t);
        }
    }
    closedir(dir);

    return themes;
}

void themes_load(list_node **themes, list_node **from_dirs)
{
    list_node *dir_themes = NULL;
    *from_dirs = default_icon_dirs();

    for_each_list_element(*from_dirs, current)
    {
        dir_themes = load_themes_from_dir((const char*)current->data);
        if (dir_themes == NULL)
            continue;

        list_concatinate(themes, dir_themes);
    }
}

void themes_destroy(list_node **themes, list_node **from_dirs)
{
    for_each_list_element(*themes, current)
    {
        theme_destroy((theme*)current->data);
    }

    list_clear(themes);
    list_destroy(from_dirs);
}

char *find_icon_and_get_name(char *name, char *base_dir, char *theme_dir, char *sub_dir)
{
    size_t path_len = snprintf(NULL, 0, "%s/%s/%s/%s.png", base_dir, theme_dir, sub_dir, name) + 1;
    char *path = malloc(path_len);

    snprintf(path, path_len, "%s/%s/%s/%s.png", base_dir, theme_dir, sub_dir, name);

    if (!access(path, R_OK))
    {
        return path;
    }

    free(path);
    return NULL;
}

char *find_icon_and_get_name_in_dir(char *name, char *base_dir)
{
    size_t path_len = snprintf(NULL, 0, "%s/%s.png", base_dir, name) + 1;
    char *path = malloc(path_len);

    snprintf(path, path_len, "%s/%s.png", base_dir, name);

    if (!access(path, R_OK))
        return path;

    free(path);
    return NULL;
}

int theme_exists_in_dir(const char *theme_, const char* dir)
{
    size_t path_len = snprintf(NULL, 0, "%s/%s", dir, theme_) + 1;
    char *path = malloc(path_len);
    struct stat sb;
    int r;

    snprintf(path, path_len, "%s/%s", dir, theme_);

    r = stat(path, &sb) == 0 && S_ISDIR(sb.st_mode);
    free(path);

    return r;
}


int cmp_theme_name(const void *item, const void *cmp_to) {
    const theme *t = (const theme*)item;
    if (!t->name)
        return 0;
    return strcmp(t->name, cmp_to) == 0 ? 1 : 0;
}

char *get_item_with_theme(list_node *dirs, list_node *themes, char *name, int size, char *theme_name, int *min_size, int *max_size)
{
    char *icon = NULL, *potential_icon = NULL;
    list_node *theme_node =  NULL;
    unsigned int diff, smallest_diff = UINT_MAX;
    theme_subdir *subdir = NULL;
    theme *t = NULL;

    theme_node = list_search_using_function(themes, &cmp_theme_name, theme_name);

    if (!theme_node)
    {
        return NULL;
    }

    t = (theme*)theme_node->data;
    // Exact match | Iterating on basedir to find theme
    for_each_list_element(dirs, current_dir)
    {
        if (!theme_exists_in_dir(t->dir, current_dir->data))
            continue;

        // If a theme in one of this dir, iterating over subdirs
        for_each_list_element(t->subdirs, subdir_node)
        {
            subdir = (theme_subdir*)subdir_node->data;

            // If size is good
            if ((subdir->min_size == subdir->max_size && abs(size - subdir->min_size) <= size * 0.3) || (size >= subdir->min_size && size <= subdir->max_size))
            {
                icon = find_icon_and_get_name(name, current_dir->data, t->dir, subdir->name);
                if (icon)
                {
                    *min_size = subdir->min_size;
                    *max_size = subdir->max_size;
                    return icon;
                }
            }
        }
    }

    // Inexact match | Iterating on basedir to find theme
    for_each_list_element(dirs, current_dir)
    {
        if (!theme_exists_in_dir(t->dir, current_dir->data))
            continue;

        // If a theme in one of this dir, iterating over subdirs
        for_each_list_element(t->subdirs, subdir_node)
        {
            subdir = (theme_subdir*)subdir_node->data;
            diff = (size > subdir->max_size ? size - subdir->max_size : 0) + (size < subdir->min_size ? subdir->min_size - size : 0);

            if (diff < smallest_diff)
            {
                potential_icon = find_icon_and_get_name(name, current_dir->data, t->dir, subdir->name);
                if (potential_icon)
                {
                    icon = potential_icon;
                    smallest_diff = diff;
                    *min_size = subdir->min_size;
                    *max_size = subdir->max_size;
                }
            }
        }
    }

    if (!icon && t->inherits)
    {
        for_each_list_element(t->inherits, subdir_node)
        {
            icon = get_item_with_theme(dirs, themes, name, size, subdir_node->data, min_size, max_size);

            if (icon)
                break;
        }
    }

    return icon;
}

char *get_fallback_icon(list_node *dirs, char *name, int *min_size, int *max_size)
{
    char *icon = NULL;
    for_each_list_element(dirs, dir_node)
    {
        icon = find_icon_and_get_name(name, dir_node->data, "", "");
        if (icon)
        {
            *min_size = 1;
            *max_size = 512;

            return icon;
        }
    }

    return NULL;
}

char *get_icon(list_node *themes, list_node *dirs, char *name, int size, char *theme_name, int *min_size, int *max_size)
{
    char *icon = NULL;
    if (theme_name)
        icon = get_item_with_theme(dirs, themes, name, size, theme_name, min_size, max_size);

    if (!icon && !(theme_name && strcmp(theme_name, "Hicolor") == 0))
        icon = get_item_with_theme(dirs, themes, name, size, "Hicolor", min_size, max_size);

    if (!icon)
        icon = get_fallback_icon(dirs, name, min_size, max_size);

    return icon;
}




/** Watcher functions **/
int stwatcher_register_notifier_item(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    STWatcher *watcher;
    char *service_or_path, *id;
    const char *service, *path;
    list_cmp_function *func_pointer;
    int ret = sd_bus_message_read(msg, "s", &service_or_path);

    if (ret < 0)
        return ret;

    /** Getting id **/
    watcher = (STWatcher*) data;
    /* Using freedesktop protocol */
    if (!strncmp(watcher->interface, "org.freedesktop", 15))
    {
        id = strdup(service_or_path);
        func_pointer = &cmp_strings;
    }
    else
    {
        int id_len;
        /* Creating id */
        if (service_or_path[0] == '/')
        {
            service = sd_bus_message_get_sender(msg);
            path = service_or_path;
        } else
        {
            service = service_or_path;
            path = "/StatusNotifierItem";
        }
        id_len = snprintf(NULL, 0, "%s%s", service, path) + 1;
        id = malloc(id_len);
        snprintf(id, id_len, "%s%s", service, path);
        func_pointer = &cmp_strings_kde;
    }

    if (list_search_using_function(watcher->items, func_pointer, (void*)id))
    {
        // Item is already installed
        free(id);
    } else
    {
        // Adding new item
        list_append(&watcher->items, (void*)id);
        sd_bus_emit_signal(__dbus, obj_path, watcher->interface, "StatusNotifierItemRegistered", "s", id);
    }

    return sd_bus_reply_method_return(msg, "");
}

int stwatcher_on_application_close(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    STWatcher *watcher = (STWatcher*)data;
    list_node *item_node = NULL;
    list_cmp_function *func_pointer;
    char *service, *old_owner, *new_owner;
    char *id;
    int is_freedesktop_app = 0;
    int ret;

    ret = sd_bus_message_read(msg, "sss", &service, &old_owner, &new_owner);

    if (ret < 0)
        return ret;

    if (!*new_owner)
    {
        func_pointer = !strncmp(watcher->interface, "org.freedesktop", 15) ? &cmp_strings : &cmp_strings_kde;
        item_node = list_search_using_function(watcher->items, func_pointer, service);
        while (item_node)
        {
            id = (char*)item_node->data;
            list_exclude(&watcher->items, item_node);

            sd_bus_emit_signal(__dbus, obj_path, watcher->interface, "StatusNotifierItemUnregistered", "s", id);

            free(id);
            item_node = list_search_using_function(watcher->items, func_pointer, service);
        }

        /*TODO unregister host*/
    }

    return 0;
}

int stwatcher_register_notifier_host(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    char *service;
    int ret = sd_bus_message_read(msg, "s", &service);
    STWatcher *watcher;

    if (ret < 0)
        return ret;

    watcher = (STWatcher*)data;
    if (!list_search_using_function(watcher->hosts, &cmp_strings, (void*)service))
    {
        // Adding new host
        list_append(&watcher->hosts, (void*)service);
        sd_bus_emit_signal(__dbus, obj_path, watcher->interface, "StatusNotifierHostRegistered", "s", service);
    }

    return sd_bus_reply_method_return(msg, "");
}

int stwatcher_property_registered_items(sd_bus *bus, const char *obj_path, const char *interface, const char *property, sd_bus_message *reply, void *data, sd_bus_error *error)
{
    STWatcher *watcher = (STWatcher *)data;
    char **items = list_of_strings_to_array(watcher->items);

    if (items)
    {
        int ret = sd_bus_message_append_strv(reply, items);
        free(items);

        return ret;
    }

    return ENXIO;
}

int stwatcher_property_is_host_registrated(sd_bus *bus, const char *obj_path, const char *interface, const char *property, sd_bus_message *reply, void *data, sd_bus_error *error)
{
    STWatcher *watcher = (STWatcher *)data;
    int ret = watcher->hosts != NULL;

    return sd_bus_message_append_basic(reply, 'b', &ret);
}



static const sd_bus_vtable watcher_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("RegisterStatusNotifierItem", "s", "", stwatcher_register_notifier_item, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("RegisterStatusNotifierHost", "s", "", stwatcher_register_notifier_host, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_PROPERTY("RegisteredStatusNotifierItems", "as", stwatcher_property_registered_items, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("IsStatusNotifierHostRegistered", "b", stwatcher_property_is_host_registrated, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("ProtocolVersion", "i", NULL, offsetof(STWatcher, protocol_version), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_SIGNAL("StatusNotifierItemRegistered", "s", 0),
    SD_BUS_SIGNAL("StatusNotifierItemUnregistered", "s", 0),
    SD_BUS_SIGNAL("StatusNotifierHostRegistered", NULL, 0),
    SD_BUS_VTABLE_END
};

STWatcher *stwatcher_create(const char *interface)
{
    int ret;
    sd_bus_slot *signal_slot = NULL, *vtable_slot = NULL;
    STWatcher *watcher = (STWatcher*)calloc(1, sizeof(STWatcher));
    watcher->interface = strdup(interface);

    if ((ret = sd_bus_add_object_vtable(__dbus, &vtable_slot, obj_path, interface, watcher_vtable, (void*)watcher)) < 0)
        goto unsuccess;

    if ((ret = sd_bus_match_signal(__dbus, &signal_slot, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged", stwatcher_on_application_close, (void*)watcher)) < 0)
        goto unsuccess;

    if ((ret = sd_bus_request_name(__dbus, watcher->interface, 0)) < 0)
    {
        if (-ret == EEXIST)
        goto unsuccess;
    }

    sd_bus_slot_set_floating(signal_slot, 0);
    sd_bus_slot_set_floating(vtable_slot, 0);
    return watcher;
unsuccess:
    sd_bus_slot_unref(signal_slot);
    sd_bus_slot_unref(vtable_slot);
    stwatcher_destroy(watcher);
    return NULL;
}

void stwatcher_destroy(STWatcher* watcher)
{
    if (!watcher)
        return;

    list_destroy(&watcher->items);
    list_destroy(&watcher->hosts);
    free(watcher->interface);
    free(watcher);
}



/**  Items **/
/* Helper functions */
int read_pixmap(sd_bus_message *msg, list_node **dest)
{
    int width, height;
    size_t npixels;
    const void *pixels = NULL;
    list_node *pixmaps = NULL;
    pixmap *map;
    int pixmaps_amount = 0;
    int ret = sd_bus_message_enter_container(msg, 'a', "(iiay)");

    if (ret < 0)
        return ret;

    if (sd_bus_message_at_end(msg, 0))
        return ret;

    pixmaps = calloc(1, sizeof(list_node));
    if (!pixmaps) {
        return -12; // -ENOMEM
    }

    while (!sd_bus_message_at_end(msg, 0))
    {
        ret = sd_bus_message_enter_container(msg, 'r', "iiay");
        if (ret < 0)
            goto unsuccess;

        ret = sd_bus_message_read(msg, "ii", &width, &height);
        if (ret < 0)
            goto unsuccess;

        ret = sd_bus_message_read_array(msg, 'y', &pixels, &npixels);
        if (ret < 0)
            goto unsuccess;

        if (height > 0 && width == height)
        {
            map = malloc(sizeof(pixmap) + npixels);
            map->size = height;

            // convert from network byte order to host byte order
            for (int i = 0; i < height * width; ++i) {
                ((uint32_t *)map->data)[i] = ntohl(((uint32_t *)pixels)[i]);
            }

            ++pixmaps_amount;
            list_append(&pixmaps, map);
        }

        sd_bus_message_exit_container(msg);
    }

    if (pixmaps_amount < 1) {
        goto unsuccess;
    }

    list_destroy(dest);
    *dest = pixmaps;

    return ret;
unsuccess:
    list_destroy(&pixmaps);

    return ret;
}

cairo_surface_t *load_and_render_icon(const char* path, int target_size)
{
    cairo_matrix_t scale;
    cairo_surface_t *icon = NULL, *scaled_icon = NULL;
    cairo_pattern_t *pattern;
    cairo_t *cairo = NULL;

    scaled_icon = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, target_size, target_size);
    cairo = cairo_create(scaled_icon);
    icon = load_image(path);

    cairo_matrix_init_scale(&scale, cairo_image_surface_get_width(icon) / ((double)target_size), cairo_image_surface_get_height(icon) / ((double)target_size));
    pattern = cairo_pattern_create_for_surface(icon);
    cairo_pattern_set_matrix(pattern, &scale);

    cairo_set_source(cairo, pattern);
    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cairo);

    cairo_destroy(cairo);
    cairo_pattern_destroy(pattern);
    cairo_surface_destroy(icon);

    return scaled_icon;
}

cairo_surface_t *render_most_suitable_pixmap(list_node *pixmaps, int target_size)
{
    cairo_surface_t *icon = NULL;
    list_node *current, *start;
    pixmap *best, *new = NULL;
    int best_difference, current_difference;

    if (!pixmaps)
        goto unsuccess;

    start = pixmaps;
    while (!start->data) {
        if (!start->next)
            return NULL;
        start = start->next;
    }
    best = (pixmap*)start->data;
    best_difference = abs(best->size - target_size);

    for (current = start->next; current;)
    {
        new = (pixmap*)current->data;
        if (new)
        {
            current_difference = abs(new->size - target_size);
            if (current_difference < best_difference)
            {
                best_difference = current_difference;
                best = new;
            }

            current = current->next;
        }
    }

    return cairo_image_surface_create_for_data(
        best->data, CAIRO_FORMAT_ARGB32, best->size, best->size, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, best->size)
    );

unsuccess:
    return NULL;
}

/* Item functions */
int stitem_ready(STItem *item) {
    return item->status && (item->status[0] == 'N' ?
            item->attention_icon_name || item->attention_icon_pixmaps :
            item->icon_name || item->icon_pixmaps);
}

void stitem_handle_click(STItem *item, int x, int y, int button)
{
    if (button == BTN_LEFT)
    {
        if (item->item_is_menu)
            sd_bus_call_method_async(__dbus, NULL, item->service, item->path, item->interface, "ContextMenu", NULL, NULL, "ii", x, y);
        else
            sd_bus_call_method_async(__dbus, NULL, item->service, item->path, item->interface, "Activate", NULL, NULL, "ii", x, y);
        return;
    }

    if (button == BTN_FORWARD || button == BTN_BACK)
    {
        int sign = button == BTN_FORWARD ? 1 : -1;
        sd_bus_call_method_async(__dbus, NULL, item->service, item->path, item->interface, "Scroll", NULL, NULL, "is", sign, "vertical");
        return;
    }

    if (button == BTN_RIGHT)
    {
        if (item->process_id)
          kill(item->process_id, SIGTERM);
        /* sd_bus_call_method_async(__dbus, NULL, item->service, item->path, item->interface, "ContextMenu", NULL, NULL, "ii", x, y); */
    }
}

int get_property_callback(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    STItemSlot *slot = data;
    STItem *item = slot->item;
    const char *prop = slot->prop;
    const char *type = slot->type;
    void *dest = slot->dest;

    int ret;
    if (sd_bus_message_is_method_error(msg, NULL)) {
        ret = sd_bus_message_get_errno(msg);
        goto unsuccess;
    }

    ret = sd_bus_message_enter_container(msg, 'v', type);
    if (ret < 0)
        goto unsuccess;

    // List
    if (!type)
    {
        ret = read_pixmap(msg, (list_node**)dest);
        if (ret < 0)
            goto unsuccess;
    } else
    {
        if (*type == 's' || *type == 'o')
            free(*(char **)dest);

        ret = sd_bus_message_read(msg, type, dest);
        if (ret < 0)
            goto unsuccess;

        if (*type == 's' || *type == 'o')
        {
            char **str = dest;
            *str = strdup(*str);
        }
    }

    if (strcmp(prop, "Status") == 0 || (item->status && (item->status[0] == 'N' ? prop[0] == 'A' : strncmp(prop, "Icon", 4) == 0)))
    {
        if (stitem_ready(item))
        {
            item->target_size = item->min_size = item->max_size = 0;
            /* set bar dirty */
            tray_update(__tray);
        }
    }

unsuccess:
    wl_list_remove(&slot->link);
    free(data);
    return ret;
}

void request_item_property(STItem *item, const char *prop, const char *type, void *dest)
{
    STItemSlot *slot = calloc(1, sizeof(STItemSlot));
    int ret;
    slot->item = item;
    slot->prop = prop;
    slot->type = type;
    slot->dest = dest;
    ret = sd_bus_call_method_async(
        __dbus,
        &slot->slot,
        item->service,
        item->path,
        "org.freedesktop.DBus.Properties",
        "Get",
        get_property_callback,
        slot,
        "ss",
        item->interface,
        prop
    );
    if (ret >= 0) {
        wl_list_insert(&item->slots, &slot->link);
    } else {
        free(slot);
    }
}

int check_message_sender(STItem *item, sd_bus_message *msg, const char *signal)
{
    bool has_well_known_names = sd_bus_creds_get_mask(sd_bus_message_get_creds(msg)) & SD_BUS_CREDS_WELL_KNOWN_NAMES;
    return item->service[0] == ':' || has_well_known_names;
}

int on_new_icon(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    STItem *item = data;
    request_item_property(item, "IconName", "s", &item->icon_name);
    request_item_property(item, "IconPixmap", NULL, &item->icon_pixmaps);
    if (!strcmp(item->interface, "org.kde.StatusNotifierItem"))
    {
        request_item_property(item, "IconThemePath", "s", &item->icon_theme_path);
    }
    return check_message_sender(item, msg, "icon");
}

int on_new_attention_icon(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    STItem *item = data;
    request_item_property(item, "AttentionIconName", "s", &item->attention_icon_name);
    request_item_property(item, "AttentionIconPixmap", NULL, &item->attention_icon_pixmaps);
    return check_message_sender(item, msg, "attention icon");
}

int on_new_status(sd_bus_message *msg, void *data, sd_bus_error *error) {
    STItem *item = data;
    int ret = check_message_sender(item, msg, "status");

    if (ret == 1) {
        char *status;
        int r = sd_bus_message_read(msg, "s", &status);
        if (r < 0) {
            ret = r;
        } else {
            free(item->status);
            item->status = strdup(status);

            if (stitem_ready(item))
            {
                item->target_size = item->min_size = item->max_size = 0;

                /* set bar dirty */
                tray_update(__tray);
            }
        }
    } else {
        request_item_property(item, "Status", "s", &item->status);
    }

    return ret;
}

void subscribe_on_object_change(STItem *item, char *signal, sd_bus_message_handler_t callback)
{
    STItemSlot *slot = calloc(1, sizeof(STItemSlot));
    int ret = sd_bus_match_signal_async(
        __dbus,
        &slot->slot,
        item->service,
        item->path,
        item->interface,
        signal,
        callback,
        NULL,
        item
    );

    if (ret >= 0) {
        wl_list_insert(&item->slots, &slot->link);
    } else {
        free(slot);
    }
}

cairo_surface_t *stitem_get_rendered_icon(STItem *item, int target_size)
{
    char *icon_name = NULL;
    list_node *dirs = NULL;
    char *found_icon_path = NULL;
    cairo_surface_t *rendered = NULL;

    if (!item)
        return NULL;

    if (item->status[0] == 'N' && item->prerendered_attention_icon)
        return item->prerendered_attention_icon;
    else if (item->prerendered_icon)
        return item->prerendered_icon;

    icon_name = item->status[0] == 'N' ? item->attention_icon_name : item->icon_name;
    /* Searching icon in path - recomended to search first */
    if (icon_name && strlen(icon_name))
    {
        list_append(&dirs, strdup(icon_name));
        if (item->icon_theme_path)
        {
            found_icon_path = find_icon_and_get_name_in_dir(item->icon_name, item->icon_theme_path);

            if (found_icon_path)
            {
                rendered = load_and_render_icon(found_icon_path, target_size);
                list_destroy(&dirs);
                free(found_icon_path);

                if (item->status[0] == 'N')
                    item->prerendered_attention_icon = rendered;
                else
                    item->prerendered_icon = rendered;

                return rendered;
            }
        }

        list_concatinate(&dirs, __tray->icon_dirs);
        found_icon_path = get_item_with_theme(dirs, __tray->themes, icon_name, target_size, default_theme_name, &item->min_size, &item->max_size);
        list_remove(&dirs, dirs);

        if (found_icon_path)
        {
            rendered = load_and_render_icon(found_icon_path, target_size);
            free(found_icon_path);

            if (item->status[0] == 'N')
                item->prerendered_attention_icon = rendered;
            else
                item->prerendered_icon = rendered;

            return rendered;
        }
    }

    /* Searching in pixmaps */
    if (item->status[0] == 'N')
    {
        item->prerendered_attention_icon = render_most_suitable_pixmap(item->attention_icon_pixmaps, target_size);
        return item->prerendered_attention_icon;
    } else
    {
        item->prerendered_icon = render_most_suitable_pixmap(item->icon_pixmaps, target_size);
        return item->prerendered_icon;
    }
}

STItem *stitem_create(char *id, STHost *host)
{
    char *path;
    sd_bus_creds *creds;
    STItem *item = calloc(1, sizeof(STItem));
    if (!item)
        return NULL;

    wl_list_init(&item->slots);
    item->watcher = strdup(id);
    path = strchr(id, '/');
    if (!path)
    {
        item->service = strdup(id);
        item->path = strdup("/StatusNotifierItem");
        item->interface = strdup("org.freedesktop.StatusNotifierItem");
    } else
    {
        item->service = strndup(id, path - id);
        item->path = strdup(path);
        item->interface = strdup("org.kde.StatusNotifierItem");

        request_item_property(item, "IconThemePath", "s", &item->icon_theme_path);
    }
    request_item_property(item, "Status", "s", &item->status);
    request_item_property(item, "IconName", "s", &item->icon_name);
    request_item_property(item, "IconPixmap", NULL, &item->icon_pixmaps);
    request_item_property(item, "AttentionIconName", "s", &item->attention_icon_name);
    request_item_property(item, "AttentionIconPixmap", NULL, &item->attention_icon_pixmaps);
    request_item_property(item, "ItemIsMenu", "b", &item->item_is_menu);
    request_item_property(item, "Menu", "o", &item->menu);

    subscribe_on_object_change(item, "NewIcon", on_new_icon);
    subscribe_on_object_change(item, "NewAttentionIcon", on_new_attention_icon);
    subscribe_on_object_change(item, "NewStatus", on_new_status);

    sd_bus_get_name_creds(__dbus, item->service, SD_BUS_CREDS_PID, &creds);
    if (creds)
    {
      sd_bus_creds_get_pid(creds, &item->process_id);
      sd_bus_creds_unref(creds);
    }

    return item;
}

void stitem_destroy(STItem *item)
{
    STItemSlot *slot, *slot_tmp;

    if (!item) {
        return;
    }

    free(item->watcher);
    free(item->service);
    free(item->path);
    free(item->status);
    free(item->icon_name);
    list_destroy(&item->icon_pixmaps);
    free(item->attention_icon_name);
    list_destroy(&item->attention_icon_pixmaps);
    free(item->menu);
    free(item->icon_theme_path);
    if (item->prerendered_icon)
        cairo_surface_destroy(item->prerendered_icon);
    if (item->prerendered_attention_icon)
        cairo_surface_destroy(item->prerendered_attention_icon);

    wl_list_for_each_safe(slot, slot_tmp, &item->slots, link) {
        sd_bus_slot_unref(slot->slot);
        free(slot);
    }

    free(item);
}


/** Host functions **/
/*  Helper functions */
int cmp_sni_id(const void* from_list, const void *cmp_item)
{
    STItem *item = (STItem*)from_list;
    return strcmp(item->watcher, cmp_item) == 0 ? 1 : 0;
}

/* Host functions*/
int on_stitem_registered(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    char *id = NULL;
    int ret = sd_bus_message_read(msg, "s", &id);
    STHost *host = (STHost*)data;
    STItem *new_item = NULL;

    if (ret < 0)
        return ret;

    if (!list_search_using_function(host->items, &cmp_sni_id, id))
    {
        new_item = stitem_create(id, host);
        if (new_item) {
            list_append(&host->items, new_item);
            __tray->applications_amount += 1;
        }
    }

    return ret;
}

int on_stitem_unregistered(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    char *id = NULL;
    STItem *item = NULL;
    STHost *host = data;
    list_node *node_with_item = NULL;
    int ret = sd_bus_message_read(msg, "s", &id);

    if (ret < 0)
        return ret;

    node_with_item = list_search_using_function(host->items, &cmp_sni_id, id);
   
    if (!node_with_item)
        return ret;
    item = (STItem*)node_with_item->data;

    list_exclude(&host->items, node_with_item);
    stitem_destroy(item);

    __tray->applications_amount -= 1;
    tray_update(__tray);
    return ret;
}

int get_registered_items(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    char **ids;
    int ret;
    if (sd_bus_message_is_method_error(msg, NULL)) {
        const sd_bus_error *err = sd_bus_message_get_error(msg);
        return -sd_bus_error_get_errno(err);
    }

    ret = sd_bus_message_enter_container(msg, 'v', NULL);
    if (ret < 0)
        return ret;

    ret = sd_bus_message_read_strv(msg, &ids);
    if (ret < 0)
        return ret;

    if (ids) {
        STHost *host = data;
        for (char **id = ids; *id; ++id)
        {
            STItem *new_item = stitem_create(*id, host);
            if (new_item)
                list_append(&host->items, new_item);
            free(*id);
        }
    }

    free(ids);
    return ret;
}

int on_new_watcher(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    char *service, *old_owner, *new_owner;
    int ret = sd_bus_message_read(msg, "sss", &service, &old_owner, &new_owner);
    if (ret < 0)
        return ret;

    if (!*old_owner)
    {
        STHost *host = data;
        if (strcmp(service, host->watcher_interface) == 0)
        {
            ret = sd_bus_call_method_async(__dbus, NULL, host->watcher_interface, obj_path, host->watcher_interface, "RegisterStatusNotifierHost", NULL, NULL, "s", host->service_name);

            if (ret < 0)
                return 0;

            ret = sd_bus_call_method_async(
                __dbus,
                NULL,
                host->watcher_interface,
                obj_path,
                "org.freedesktop.DBus.Properties",
                "Get",
                get_registered_items,
                host,
                "ss",
                host->watcher_interface,
                "RegisteredStatusNotifierItems"
            );
        }
    }

    return 0;
}

STHost* sthost_create(const char* interface)
{
    STHost *host;
    pid_t pid;
    int watcher_end_size, name_size, ret;
    char* notifier_part;
    sd_bus_slot *reg_slot = NULL, *unreg_slot = NULL, *watcher_slot = NULL;

    /*  Create host */
    host = (STHost*)malloc(sizeof(STHost));
    host->items = NULL; host->service_name = NULL;
    host->watcher_interface = strdup(interface);

    /*  Host init */
    if ((ret = sd_bus_match_signal(__dbus, &reg_slot, interface, obj_path, interface, "StatusNotifierItemRegistered", on_stitem_registered, host)) < 0)
        goto unsuccess;

    if (sd_bus_match_signal(__dbus, &unreg_slot, interface, obj_path, interface, "StatusNotifierItemUnregistered", on_stitem_unregistered, host) < 0)
        goto unsuccess;

    if (sd_bus_match_signal(__dbus, &watcher_slot, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged", on_new_watcher, host) < 0)
        goto unsuccess;

    /* Generating name */
    pid = getpid();
    watcher_end_size = strstr(interface, "Watcher") - interface;
    notifier_part = strndup(interface, watcher_end_size);
    name_size = snprintf(NULL, 0, "%sHost-%d", notifier_part, pid) + 1;

    host->service_name = (char*)malloc(name_size);
    snprintf(host->service_name, name_size, "%sHost-%d", notifier_part, pid);
    free(notifier_part);

    /* Registraiting */
    if (sd_bus_request_name(__dbus, host->service_name, 0) < 0)
        goto unsuccess;

    ret = sd_bus_call_method_async(__dbus, NULL, interface, obj_path, interface, "RegisterStatusNotifierHost", NULL, NULL, "s", host->service_name);
    if (ret < 0)
        goto unsuccess;

    ret = sd_bus_call_method_async(__dbus, NULL, interface, obj_path, "org.freedesktop.DBus.Properties", "Get", get_registered_items, host, "ss", interface, "RegisteredStatusNotifierItems");
    if (ret < 0)
        goto unsuccess;

    sd_bus_slot_set_floating(reg_slot, 0);
    sd_bus_slot_set_floating(unreg_slot, 0);
    sd_bus_slot_set_floating(watcher_slot, 0);

    return host;
unsuccess:
    sd_bus_slot_unref(reg_slot);
    sd_bus_slot_unref(unreg_slot);
    sd_bus_slot_unref(watcher_slot);

    sthost_destroy(host);
    return NULL;
}

void sthost_destroy(STHost *host)
{
    sd_bus_release_name(__dbus, host->service_name);
    if (host->service_name)
        free(host->service_name);
    if (host->service_name)
        free(host->watcher_interface);
    list_destroy(&host->items);
    free(host);
}

/** Tray functions **/
int tray_on_watcher_lost(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    Tray* tray = (Tray*)data;
    char *service, *old_owner, *new_owner;
    int ret = sd_bus_message_read(msg, "sss", &service, &old_owner, &new_owner);

    if (ret < 0)
        return ret;

    if (!*new_owner) {
        if (strcmp(service, "org.kde.StatusNotifierWatcher") == 0)
            tray->w_kde = stwatcher_create("org.kde.StatusNotifierWatcher");

        if (strcmp(service, "org.freedesktop.StatusNotifierWatcher") == 0)
            tray->w_kde = stwatcher_create("org.freedesktop.StatusNotifierWatcher");
    }

    return 0;
}


/** Edwl utils for dbus **/
int set_status_message(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    char *status;
    int ret = sd_bus_message_read(msg, "s", &status);

    if (ret < 0)
        return ret;

    if (__tray->status_message)
        free(__tray->status_message);

    __tray->status_message = strdup(status);

    /*  Notify  */
    wl_signal_emit(&__tray->events.message_update, (void*)__tray);

    return sd_bus_reply_method_return(msg, "");
}

int set_background_path(sd_bus_message *msg, void *data, sd_bus_error *error)
{
    char *background_image_path;
    int ret = sd_bus_message_read(msg, "s", &background_image_path);

    if (ret < 0)
        return ret;

    if (__tray->background_path)
        free(__tray->background_path);

    __tray->background_path = strdup(background_image_path);
   
    /*  Notify  */
    wl_signal_emit(&__tray->events.background_update, (void*)__tray);

    return sd_bus_reply_method_return(msg, "");
}

#ifdef WITH_SYSTEMD
static const sd_bus_vtable edwl_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS("SetStatus", "s", SD_BUS_NO_RESULT, set_status_message, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("SetBackgroundPath", "s", SD_BUS_NO_RESULT, set_background_path, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};
#endif
#ifdef WITH_ELOGIND
/* Tested on Artix with elogind 246.10-8. For some reason, the macroses for 
 * SD_BUS_METHOD_WITH_ARGS generates wrong strings for signatures and
 * names attributes, so I'm forced to write it post-macro like
 * */
static const sd_bus_vtable edwl_vtable[] = {
    SD_BUS_VTABLE_START(0),
    // SD_BUS_METHOD_WITH_ARGS("SetStatus", "s", SD_BUS_NO_RESULT, set_status_message, SD_BUS_VTABLE_UNPRIVILEGED),
    { 
     .type = _SD_BUS_VTABLE_METHOD,
     .flags = SD_BUS_VTABLE_UNPRIVILEGED, 
     .x = { 
       .method = { 
         .member = "SetStatus", 
         .signature = "s",
         .result = ((void *)0), 
         .handler = set_status_message, 
         .offset = 0, 
         .names = "\0",
       },
     },
    },
    // SD_BUS_METHOD_WITH_ARGS("SetBackgroundPath", "s", SD_BUS_NO_RESULT, set_background_path, SD_BUS_VTABLE_UNPRIVILEGED),
    { 
     .type = _SD_BUS_VTABLE_METHOD,
     .flags = SD_BUS_VTABLE_UNPRIVILEGED, 
     .x = { 
       .method = { 
         .member = "SetBackgroundPath", 
         .signature = "s",
         .result = ((void *)0), 
         .handler = set_background_path, 
         .offset = 0, 
         .names = "\0",
       },
     },
    },
    SD_BUS_VTABLE_END
};
#endif
#ifdef WITH_BASU
static const sd_bus_vtable edwl_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("SetStatus", "s", 0, set_status_message, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetBackgroundPath", "s", 0, set_background_path, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};
#endif

int dbus_processing_function(void *)
{
    sd_bus_process(__dbus, NULL);

    if (continue_process)
        wl_event_source_timer_update(__tray->dbus_update_timer, 100);  // processing messsages every 100 ms

    return 0;
}

/* Create init */
Tray* tray_init(int height, struct wl_event_loop *loop)
{
    cairo_t *cairo;
    sd_bus_slot *edwl_slot = NULL;

    if (__tray)
        return __tray;
   
    if (!__dbus)
    {
        if (sd_bus_open_user(&__dbus) < 0)
            return NULL;
    }

    __tray = calloc(1, sizeof(Tray));
    __tray->w_kde = stwatcher_create("org.kde.StatusNotifierWatcher");
    __tray->kde = sthost_create("org.kde.StatusNotifierWatcher");

    __tray->w_freedesktop = stwatcher_create("org.freedesktop.StatusNotifierWatcher");
    __tray->freedesktop = sthost_create("org.freedesktop.StatusNotifierWatcher");

    __tray->width = height * 10; // magick number
    __tray->height = height;
    __tray->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, __tray->width, height);

    /* Signals */
    wl_signal_init(&__tray->events.message_update);
    wl_signal_init(&__tray->events.applications_update);
    wl_signal_init(&__tray->events.background_update);

    /* Creating an object where we could receive messages */
    if (sd_bus_add_object_vtable(__dbus, &edwl_slot, "/net/wm/edwl", "net.wm.edwl", edwl_vtable, NULL) < 0)
        sd_bus_slot_unref(edwl_slot);
    else {
        if (sd_bus_request_name(__dbus, "net.wm.edwl", 0) < 0)
            sd_bus_slot_set_floating(edwl_slot, 0);
    }

    /* Getting themes list from basedirs */
    themes_load(&__tray->themes, &__tray->icon_dirs);

    /* Processing by timer*/
    __tray->dbus_update_timer = wl_event_loop_add_timer(loop, dbus_processing_function, NULL);
    wl_event_source_timer_update(__tray->dbus_update_timer, 1000);

    return __tray;
}

void tray_destroy()
{
    /* Thread drop */
    continue_process = 0;
    wl_event_source_remove(__tray->dbus_update_timer);

    themes_destroy(&__tray->themes, &__tray->icon_dirs);

    sthost_destroy(__tray->kde);
    stwatcher_destroy(__tray->w_kde);

    sthost_destroy(__tray->freedesktop);
    stwatcher_destroy(__tray->w_freedesktop);

    cairo_surface_destroy(__tray->surface);

    if (__tray->background_path)
        free(__tray->background_path);

    if (__tray->status_message)
        free(__tray->status_message);

    free(__tray);
    sd_bus_flush_close_unrefp(&__dbus);

    __tray = NULL;
    __dbus = NULL;
}

void tray_update(Tray* tray)
{
    STItem *item = NULL;
    cairo_surface_t *icon = NULL;
    cairo_t *cairo = NULL;
    cairo_pattern_t *pattern;
    cairo_matrix_t scale;
    list_node *icon_node;
    list_node *items_lists[] = {tray->kde->items, tray->freedesktop->items};
    struct wlr_texture *texture;
    int icon_size = tray->height;
    int icons_printed = 0;

    if (!tray->kde->items && !tray->freedesktop->items)
    {
        cairo = cairo_create(tray->surface);
        cairo_save(cairo);
        cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cairo);
        cairo_restore(cairo);

        /*  Notify  */
        wl_signal_emit(&tray->events.applications_update, (void*)tray);

        return;
    }

    cairo = cairo_create(tray->surface);

    /* clear surface from previous use */
    cairo_save(cairo);
    cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cairo);
    cairo_restore(cairo);

    /*  Render on surface */
    for (int i = 0; i < 2; i++)
    {
        for (list_node* node = items_lists[i]; node;)
        {
            item = (STItem*)node->data;
            icon = stitem_get_rendered_icon(item, icon_size);

            if (icon)
            {
                cairo_matrix_init_scale(
                    &scale,
                    (double)cairo_image_surface_get_width(icon) / icon_size,
                    (double)cairo_image_surface_get_height(icon) / icon_size
                );
                cairo_matrix_translate(&scale, -icons_printed * icon_size, 0);
                pattern = cairo_pattern_create_for_surface(icon);
                cairo_pattern_set_matrix(pattern, &scale);

                cairo_set_source(cairo, pattern);
                cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
                cairo_paint(cairo);
            } else
            {
               cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
               cairo_rectangle(cairo, -icons_printed * icon_size, 0, icon_size, icon_size);
            }

            ++icons_printed;

            node = node->next;
        }
    }

    cairo_destroy(cairo);

    /*  Notify  */
    wl_signal_emit(&tray->events.applications_update, (void*)tray);
}

void tray_process_click(int position, int button, int x, int y)
{
    STItem *item = NULL;
    list_node *item_node = NULL;
    int index = position;

    if (position >= 0 && (button == BTN_RIGHT || button == BTN_LEFT))
    {
        int kde_length = list_length(__tray->kde->items);

        if (position >= kde_length)
        {
            position -= kde_length;
            item_node = __tray->freedesktop->items;
        }
        else
            item_node = __tray->kde->items;

        while (index > 0)
        {
            item_node = item_node->next;
            --index;
        }

        if (item_node)
        {
            item = (STItem*)item_node->data;
            stitem_handle_click(item, x, y, button);
        }
    }
}
