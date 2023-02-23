#ifndef __TRAY_H_
#define __TRAY_H_

#include <stdlib.h>
#include <wayland-server-core.h>
#include <cairo.h>
#ifdef WITH_SYSTEMD
#include <systemd/sd-bus.h>
#endif
#ifdef WITH_ELOGIND
#include <elogind/sd-bus.h>
#endif
#ifdef WITH_BASU
#include <basu/sd-bus.h>
#endif

/** Image loading utils **/
cairo_surface_t *load_image(const char *path);

/** Small single-linked list **/
/*  List node */
typedef struct _list_node_ {
    void *data;
    struct _list_node_ *next;
} list_node;

/*  List utils */
list_node *list_search_using_function(list_node *start, int (*cmp)(const void *data_from_list, const void *passed_data), void *passed_data);
list_node *list_append(list_node **start, void *data);
list_node *list_last(list_node *start);
void list_remove(list_node **start, list_node *node);
void list_exclude(list_node **start, list_node *node);
void list_concatinate(list_node **first, list_node *second);
char **list_of_strings_to_array(list_node *start);
void list_destroy(list_node **list);
void list_clear(list_node **list);
int list_length(list_node *start);

#define for_each_list_element(LIST_START, CURRENT) for (list_node* CURRENT = LIST_START; CURRENT; CURRENT = CURRENT->next)


/** List of dir grabing from systen **/
enum SubDirType {
    Threshold = 1,
    Scalable = 2,
    Fixed = 3
};

typedef struct {
    char *name;
    int size;

    enum SubDirType type;

    int max_size;
    int min_size;
    int threshold;
} theme_subdir;

typedef struct {
    char* name;
    list_node *inherits;
    list_node *dirs;

    char* dir;
    list_node *subdirs;
} theme;

/** Watcher object **/
/* https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/StatusNotifierWatcher/ */
typedef struct
{
    char *interface;
    list_node *items;
    list_node *hosts;
    int protocol_version;
} STWatcher;

STWatcher *stwatcher_create(const char *interface);
void stwatcher_destroy(STWatcher* watcher);


/** Notifier Item **/
typedef struct {
    int size;
    unsigned char data[];
} pixmap;

typedef struct {
    int min_size, max_size, target_size;
    char *watcher, *service, *path, *interface;

    char *status;
    char *icon_name;
    list_node *icon_pixmaps;
    char *attention_icon_name;
    list_node *attention_icon_pixmaps;
    int item_is_menu;
    char *menu;
    char *icon_theme_path;
    pid_t process_id;

    cairo_surface_t *prerendered_icon, *prerendered_attention_icon;
    int tray_x_position;

    struct wl_list slots;
} STItem;

typedef struct {
    struct wl_list link;
    STItem *item;
    const char *prop;
    const char *type;
    void *dest;
    sd_bus_slot *slot;
} STItemSlot;

typedef struct  {
    char *watcher_interface;
    char *service_name;
    list_node *items;
} STHost;

/** Item's functions **/

cairo_surface_t *stitem_get_rendered_icon(STItem *item, int target_size);
STItem *stitem_create(char *id, STHost *host);
void stitem_destroy(STItem *item);


/** Host functions **/
STHost* sthost_create(const char* interface);
void sthost_destroy(STHost *host);


typedef struct {
    STHost *freedesktop, *kde;
    STWatcher *w_freedesktop, *w_kde;
    cairo_surface_t *surface;

    struct wl_event_source *dbus_update_timer;
    struct {
        struct wl_signal message_update;
        struct wl_signal applications_update;
        struct wl_signal background_update;
    } events;

    list_node *themes;
    list_node *icon_dirs;

    char* status_message;
    char* background_path;
   
    int width, height;
    int applications_amount;
} Tray;

Tray* tray_init(int height, struct wl_event_loop *loop);
void tray_destroy();
void tray_update(Tray* tray);
void tray_process_click(int position, int button, int x, int y);

#endif // __TRAY_H_
