/*
 *  Marlin
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *            Roth Robert <evfool@gmail.com>
 *            ammonkey <am.monkeyd@gmail.com>
 *
 */

#include <config.h>

/*#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-preferences.h>
#include <eel/eel-string.h>
#include <eel/eel-stock-dialogs.h>*/
#include "eel-fcts.h"
#include "eel-gtk-extensions.h"
#include "eel-gdk-pixbuf-extensions.h"
#include "eel-gio-extensions.h"
#include "gossip-cell-renderer-expander.h"
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
/*#include <libmarlin-private/nautilus-debug-log.h>
#include <libmarlin-private/nautilus-dnd.h>
#include <libmarlin-private/nautilus-bookmark.h>
#include <libmarlin-private/nautilus-global-preferences.h>
#include <libmarlin-private/nautilus-sidebar-provider.h>
#include <libmarlin-private/nautilus-module.h>
#include <libmarlin-private/nautilus-file.h>
#include <libmarlin-private/nautilus-file-utilities.h>
#include <libmarlin-private/nautilus-file-operations.h>
#include <libmarlin-private/nautilus-trash-monitor.h>
#include <libmarlin-private/nautilus-icon-names.h>
#include <libmarlin-private/nautilus-autorun.h>
#include <libmarlin-private/nautilus-window-info.h>
#include <libmarlin-private/nautilus-window-slot-info.h>*/
#include <gio/gio.h>
#include "gof-file.h"
#include "gof-window-slot.h"
#include "nautilus-icon-info.h"
#include "marlin-icons.h"
#include "marlin-places-sidebar.h"
#include "marlin-vala.h"
#include "marlin-view-window.h"
#include "marlin-global-preferences.h"
#include "marlin-bookmark.h"
#include "marlin-trash-monitor.h"
#include "marlin-dnd.h"

/*#include "marlin-bookmark-list.h"
#include "marlin-places-sidebar.h"
#include "marlin-window.h"*/

#define EJECT_BUTTON_XPAD 0
#define TEXT_XPAD 5
#define ICON_XPAD 6

enum {
    PLACES_SIDEBAR_COLUMN_ROW_TYPE,
    PLACES_SIDEBAR_COLUMN_URI,
    PLACES_SIDEBAR_COLUMN_DRIVE,
    PLACES_SIDEBAR_COLUMN_VOLUME,
    PLACES_SIDEBAR_COLUMN_MOUNT,
    PLACES_SIDEBAR_COLUMN_NAME,
    PLACES_SIDEBAR_COLUMN_ICON,
    PLACES_SIDEBAR_COLUMN_INDEX,
    PLACES_SIDEBAR_COLUMN_EJECT,
    PLACES_SIDEBAR_COLUMN_NO_EJECT,
    PLACES_SIDEBAR_COLUMN_BOOKMARK,
    PLACES_SIDEBAR_COLUMN_TOOLTIP,
    PLACES_SIDEBAR_COLUMN_EJECT_ICON,

    PLACES_SIDEBAR_COLUMN_COUNT
};

typedef enum {
    PLACES_BUILT_IN,
    PLACES_MOUNTED_VOLUME,
    PLACES_BOOKMARK,
    PLACES_BOOKMARKS_CATEGORY,
    PLACES_PERSONAL_CATEGORY,
    PLACES_STORAGE_CATEGORY
} PlaceType;

static GType marlin_places_sidebar_provider_get_type   (void);
static void  open_selected_bookmark                    (MarlinPlacesSidebar         *sidebar,
                                                        GtkTreeModel                *model,
                                                        GtkTreePath                 *path,
                                                        MarlinViewWindowOpenFlags   flags);
static void  marlin_places_sidebar_style_set           (GtkWidget                   *widget,
                                                        GtkStyle                    *previous_style);
static gboolean eject_or_unmount_bookmark              (MarlinPlacesSidebar *sidebar,
                                                        GtkTreePath *path);
static gboolean eject_or_unmount_selection             (MarlinPlacesSidebar *sidebar);
static void  check_unmount_and_eject                   (GMount *mount,
                                                        GVolume *volume,
                                                        GDrive *drive,
                                                        gboolean *show_unmount,
                                                        gboolean *show_eject);

static void bookmarks_check_popup_sensitivity          (MarlinPlacesSidebar *sidebar);
static void expander_init_pref_state                   (GtkTreeView *tree_view);

/* Identifiers for target types */
enum {
    GTK_TREE_MODEL_ROW,
    TEXT_URI_LIST
};

/* Target types for dragging from the shortcuts list */
static const GtkTargetEntry marlin_shortcuts_source_targets[] = {
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW }
};

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry marlin_shortcuts_drop_targets [] = {
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
    { "text/uri-list", 0, TEXT_URI_LIST }
};

G_DEFINE_TYPE (MarlinPlacesSidebar, marlin_places_sidebar, GTK_TYPE_SCROLLED_WINDOW);

static GdkPixbuf *
get_eject_icon (gboolean highlighted)
{
    GdkPixbuf *eject;
    NautilusIconInfo *eject_icon_info;
    int icon_size;

    //TODO
    //icon_size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
    icon_size = 16;

    eject_icon_info = nautilus_icon_info_lookup_from_name ("media-eject", icon_size);
    eject = nautilus_icon_info_get_pixbuf_at_size (eject_icon_info, icon_size);

    if (highlighted) {
        GdkPixbuf *high;
        high = eel_gdk_pixbuf_render (eject, 1, 255, 255, 0, 0);
        g_object_unref (eject);
        eject = high;
    }

    g_object_unref (eject_icon_info);

    return eject;
}

static void
category_renderer_func (GtkTreeViewColumn *column,
                        GtkCellRenderer *renderer,
                        GtkTreeModel *model,
                        GtkTreeIter *iter,
                        gpointer data)
{
    PlaceType	 	type; 

    gtk_tree_model_get (model, iter, PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type, -1);

    if (type == PLACES_PERSONAL_CATEGORY || type == PLACES_STORAGE_CATEGORY || type == PLACES_BOOKMARKS_CATEGORY) {
        /*g_object_set (renderer, "weight", 900, "weight-set", TRUE, "height", 30, "xpad", TEXT_XPAD, NULL);*/
        g_object_set (renderer, "weight", 900, "weight-set", TRUE, "height", 20, NULL);
        //g_object_set (renderer, "weight", 900, "weight-set", TRUE, "height", 20, "xpad", TEXT_XPAD, NULL);
        /*g_object_set (renderer, "weight", PANGO_WEIGHT_NORMAL, "height", 20, NULL);*/
    } else {
        //g_object_set (renderer, "weight-set", FALSE, "height", -1, "xpad", 2, NULL);
        g_object_set (renderer, "weight-set", FALSE, "height", -1, NULL);
    }

}

static GtkTreeIter
add_place (MarlinPlacesSidebar *sidebar,
           PlaceType place_type,
           GtkTreeIter *parent,
           const char *name,
           GIcon *icon,
           const char *uri,
           GDrive *drive,
           GVolume *volume,
           GMount *mount,
           const int index,
           const char *tooltip)
{
    GdkPixbuf           *pixbuf;
    GtkTreeIter          iter, child_iter;
    GdkPixbuf	        *eject;
    NautilusIconInfo    *icon_info;
    gint icon_size;
    gboolean show_eject, show_unmount;
    gboolean show_eject_button;

    pixbuf = NULL;
    icon_size = g_settings_get_int (settings, MARLIN_PREFERENCES_SIDEBAR_ICON_SIZE);
    if (icon_size <= 0)
        icon_size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
    if (icon) {
        icon_info = nautilus_icon_info_lookup (icon, icon_size);

        pixbuf = nautilus_icon_info_get_pixbuf_nodefault (icon_info);
        g_object_unref (icon_info);
    }

    check_unmount_and_eject (mount, volume, drive,
                             &show_unmount, &show_eject);

    if (show_unmount || show_eject) {
        g_assert (place_type != PLACES_BOOKMARK);
    }

    if (mount == NULL) {
        show_eject_button = FALSE;
    } else {
        show_eject_button = (show_unmount || show_eject);
    }

    if (show_eject_button) {
        eject = get_eject_icon (FALSE);
    } else {
        eject = NULL;
    }

    gtk_tree_store_append (sidebar->store, &iter, parent);
    gtk_tree_store_set (sidebar->store, &iter,
                        PLACES_SIDEBAR_COLUMN_ICON, pixbuf,
                        PLACES_SIDEBAR_COLUMN_NAME, name,
                        PLACES_SIDEBAR_COLUMN_URI, uri,
                        PLACES_SIDEBAR_COLUMN_DRIVE, drive,
                        PLACES_SIDEBAR_COLUMN_VOLUME, volume,
                        PLACES_SIDEBAR_COLUMN_MOUNT, mount,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
                        PLACES_SIDEBAR_COLUMN_INDEX, index,
                        PLACES_SIDEBAR_COLUMN_EJECT, show_eject_button,
                        PLACES_SIDEBAR_COLUMN_NO_EJECT, !show_eject_button,
                        PLACES_SIDEBAR_COLUMN_BOOKMARK, place_type != PLACES_BOOKMARK,
                        PLACES_SIDEBAR_COLUMN_TOOLTIP, tooltip,
                        PLACES_SIDEBAR_COLUMN_EJECT_ICON, eject,
                        -1);

    if (pixbuf != NULL) {
        g_object_unref (pixbuf);
    }
    return iter;
}

static void
compare_for_selection (MarlinPlacesSidebar *sidebar,
                       const gchar *location,
                       const gchar *added_uri,
                       const gchar *last_uri,
                       GtkTreeIter *iter,
                       GtkTreePath **path)
{
    int res;

    res = eel_strcmp (added_uri, last_uri);

    if (res == 0) {
        /* last_uri always comes first */
        if (*path != NULL) {
            gtk_tree_path_free (*path);
        }
        *path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store),
                                         iter);
    } else if (eel_strcmp (location, added_uri) == 0) {
        if (*path == NULL) {
            *path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store),
                                             iter);
        }
    }
}

static void
update_places (MarlinPlacesSidebar *sidebar)
{
    //amtest
    printf ("%s\n", G_STRFUNC);

    MarlinBookmark *bookmark;
    GtkTreeSelection *selection;
    GtkTreeIter iter, last_iter;
    GtkTreePath *select_path;
    GtkTreeModel *model;
    GVolumeMonitor *volume_monitor;
    GList *mounts, *l, *ll;
    GMount *mount;
    GList *drives;
    GDrive *drive;
    GList *volumes;
    GVolume *volume;
    int bookmark_count, index;
    char *location, *mount_uri, *name, *desktop_path, *last_uri;
    GIcon *icon;
    GFile *root;
    GOFWindowSlot *slot;
    char *tooltip;
    GList *network_mounts;

    model = NULL;
    last_uri = NULL;
    select_path = NULL;
    location = NULL;

    sidebar->n_builtins_before = 0;
    selection = gtk_tree_view_get_selection (sidebar->tree_view);
    if (gtk_tree_selection_get_selected (selection, &model, &last_iter)) {
        gtk_tree_model_get (model,
                            &last_iter,
                            PLACES_SIDEBAR_COLUMN_URI, &last_uri, -1);
    }
    gtk_tree_store_clear (sidebar->store);

    slot = marlin_view_window_get_active_slot (MARLIN_VIEW_WINDOW (sidebar->window));
    if (slot)
        location = g_file_get_uri(slot->location);

    /* add bookmarks category */

    gtk_tree_store_append (sidebar->store, &iter, NULL);
    gtk_tree_store_set (sidebar->store, &iter,
                        PLACES_SIDEBAR_COLUMN_ICON, NULL,
                        PLACES_SIDEBAR_COLUMN_NAME, _("Personal"),
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_BOOKMARKS_CATEGORY,
                        PLACES_SIDEBAR_COLUMN_EJECT, FALSE,
                        PLACES_SIDEBAR_COLUMN_NO_EJECT, TRUE,
                        PLACES_SIDEBAR_COLUMN_BOOKMARK, FALSE,
                        PLACES_SIDEBAR_COLUMN_TOOLTIP, _("Your common places and bookmarks"),
                        -1);

    /* add built in bookmarks */

    /* home folder if different from desktop directory */
    /*desktop_path = marlin_get_desktop_directory ();
      if (strcmp (g_get_home_dir(), desktop_path) != 0) {*/
    char *display_name;

    mount_uri = g_filename_to_uri (g_get_home_dir (), NULL, NULL);
    display_name = g_filename_display_basename (g_get_home_dir ());
    icon = g_themed_icon_new (MARLIN_ICON_HOME);
    last_iter = add_place (sidebar, PLACES_BUILT_IN, &iter,
                           display_name, icon, mount_uri, 
                           NULL, NULL, NULL, 0,
                           _("Open your personal folder"));
    compare_for_selection (sidebar,
                           location, mount_uri, last_uri,
                           &last_iter, &select_path);
    sidebar->n_builtins_before++;
    g_object_unref (icon);
    g_free (display_name);
    g_free (mount_uri);
    /*}*/

    /* desktop directory if show_desktop */
    /*if (eel_preferences_get_boolean (MARLIN_PREFERENCES_SHOW_DESKTOP))
      {
      mount_uri = g_filename_to_uri (desktop_path, NULL, NULL);
      icon = g_themed_icon_new (MARLIN_ICON_DESKTOP);
      last_iter = add_place (sidebar, PLACES_BUILT_IN, &iter,
      _("Desktop"), icon, mount_uri, 
      NULL, NULL, NULL, 1,
      _       ("Open the contents of your desktop in a folder"));
      g_object_unref (icon);
      compare_for_selection (sidebar,
      location, mount_uri, last_uri,
      &last_iter, &select_path);
      sidebar->n_builtins_before++;
      g_free (mount_uri);
      }
      g_free (desktop_path);*/

    /* add bookmarks */

    bookmark_count = marlin_bookmark_list_length (sidebar->bookmarks);
    for (index = 0; index < bookmark_count; index++) {
        bookmark = marlin_bookmark_list_item_at (sidebar->bookmarks, index);

        if (marlin_bookmark_uri_known_not_to_exist (bookmark)) {
            continue;
        }

        name = marlin_bookmark_get_name (bookmark);
        icon = marlin_bookmark_get_icon (bookmark);
        mount_uri = marlin_bookmark_get_uri (bookmark);
        root = marlin_bookmark_get_location (bookmark);
        tooltip = g_file_get_parse_name (root);
        last_iter = add_place (sidebar, PLACES_BOOKMARK, &iter,
                               name, icon, mount_uri,
                               NULL, NULL, NULL, index + sidebar->n_builtins_before, 
                               tooltip);
        compare_for_selection (sidebar,
                               location, mount_uri, last_uri,
                               &last_iter, &select_path);
        //printf ("bookmark: %d %s\n", index, mount_uri);
        g_free (name);
        g_object_unref (root);
        g_object_unref (icon);
        g_free (mount_uri);
        g_free (tooltip);
    }

    /* add trash */

    mount_uri = MARLIN_TRASH_URI; /* No need to strdup */
    icon = marlin_trash_monitor_get_icon ();
    last_iter = add_place (sidebar, PLACES_BUILT_IN, &iter,
                           _("Trash"), icon, mount_uri,
                           NULL, NULL, NULL, index + sidebar->n_builtins_before,
                           _("Open the trash"));
    compare_for_selection (sidebar,
                           location, mount_uri, last_uri,
                           &last_iter, &select_path);
    g_object_unref (icon);

    /* add storage category */

    gtk_tree_store_append (sidebar->store, &iter, NULL);
    gtk_tree_store_set (sidebar->store, &iter,
                        PLACES_SIDEBAR_COLUMN_ICON, NULL,
                        PLACES_SIDEBAR_COLUMN_NAME, _("Devices"),
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_STORAGE_CATEGORY,
                        PLACES_SIDEBAR_COLUMN_EJECT, FALSE,
                        PLACES_SIDEBAR_COLUMN_NO_EJECT, TRUE,
                        PLACES_SIDEBAR_COLUMN_BOOKMARK, FALSE,
                        PLACES_SIDEBAR_COLUMN_TOOLTIP, _("Your local partitions and devices"),
                        -1);


    mount_uri = "file:///"; /* No need to strdup */
    icon = g_themed_icon_new (MARLIN_ICON_FILESYSTEM);
    last_iter = add_place (sidebar, PLACES_BUILT_IN, &iter,
                           _("File System"), icon,
                           mount_uri, NULL, NULL, NULL, 0,
                           _("Open the contents of the File System"));
    g_object_unref (icon);
    compare_for_selection (sidebar,
                           location, mount_uri, last_uri,
                           &last_iter, &select_path);

    volume_monitor = sidebar->volume_monitor;

    /* first go through all connected drives */
    drives = g_volume_monitor_get_connected_drives (volume_monitor);
    for (l = drives; l != NULL; l = l->next) {
        drive = l->data;

        volumes = g_drive_get_volumes (drive);
        if (volumes != NULL) {
            for (ll = volumes; ll != NULL; ll = ll->next) {
                volume = ll->data;
                mount = g_volume_get_mount (volume);
                if (mount != NULL) {
                    /* Show mounted volume in the sidebar */
                    icon = g_mount_get_icon (mount);
                    root = g_mount_get_default_location (mount);
                    mount_uri = g_file_get_uri (root);
                    name = g_mount_get_name (mount);
                    tooltip = g_file_get_parse_name (root);
                    last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME, &iter,
                                           name, icon, mount_uri,
                                           drive, volume, mount, 0, tooltip);
                    compare_for_selection (sidebar,
                                           location, mount_uri, last_uri,
                                           &last_iter, &select_path);
                    g_object_unref (root);
                    g_object_unref (mount);
                    g_object_unref (icon);
                    g_free (tooltip);
                    g_free (name);
                    g_free (mount_uri);
                } else {
                    /* Do show the unmounted volumes in the sidebar;
                     * this is so the user can mount it (in case automounting
                     * is off).
                     *
                     * Also, even if automounting is enabled, this gives a visual
                     * cue that the user should remember to yank out the media if
                     * he just unmounted it.
                     */
                    icon = g_volume_get_icon (volume);
                    name = g_volume_get_name (volume);
                    tooltip = g_strdup_printf (_("Mount and open %s"), name);
                    last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME, &iter,
                                           name, icon, NULL,
                                           drive, volume, NULL, 0, tooltip);
                    g_object_unref (icon);
                    g_free (name);
                    g_free (tooltip);
                }
                g_object_unref (volume);
            }
            g_list_free (volumes);
        } else {
            if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive)) {
                /* If the drive has no mountable volumes and we cannot detect media change.. we
                 * display the drive in the sidebar so the user can manually poll the drive by
                 * right clicking and selecting "Rescan..."
                 *
                 * This is mainly for drives like floppies where media detection doesn't
                 * work.. but it's also for human beings who like to turn off media detection
                 * in the OS to save battery juice.
                 */
                icon = g_drive_get_icon (drive);
                name = g_drive_get_name (drive);
                tooltip = g_strdup_printf (_("Mount and open %s"), name);
                last_iter = add_place (sidebar, PLACES_BUILT_IN, &iter,
                                       name, icon, NULL,
                                       drive, NULL, NULL, 0, tooltip);
                g_object_unref (icon);
                g_free (tooltip);
                g_free (name);
            }
        }
        g_object_unref (drive);
    }
    g_list_free (drives);

    /* add all volumes that is not associated with a drive */
    volumes = g_volume_monitor_get_volumes (volume_monitor);
    for (l = volumes; l != NULL; l = l->next) {
        volume = l->data;
        drive = g_volume_get_drive (volume);
        if (drive != NULL) {
            g_object_unref (volume);
            g_object_unref (drive);
            continue;
        }
        mount = g_volume_get_mount (volume);
        if (mount != NULL) {
            icon = g_mount_get_icon (mount);
            root = g_mount_get_default_location (mount);
            mount_uri = g_file_get_uri (root);
            tooltip = g_file_get_parse_name (root);
            g_object_unref (root);
            name = g_mount_get_name (mount);
            last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME, &iter,
                                   name, icon, mount_uri,
                                   NULL, volume, mount, 0, tooltip);
            compare_for_selection (sidebar,
                                   location, mount_uri, last_uri,
                                   &last_iter, &select_path);
            g_object_unref (mount);
            g_object_unref (icon);
            g_free (name);
            g_free (tooltip);
            g_free (mount_uri);
        } else {
            /* see comment above in why we add an icon for an unmounted mountable volume */
            icon = g_volume_get_icon (volume);
            name = g_volume_get_name (volume);
            last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME, &iter,
                                   name, icon, NULL,
                                   NULL, volume, NULL, 0, name);
            g_object_unref (icon);
            g_free (name);
        }
        g_object_unref (volume);
    }
    g_list_free (volumes);

    /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
    network_mounts = NULL;
    mounts = g_volume_monitor_get_mounts (volume_monitor);
    for (l = mounts; l != NULL; l = l->next) {
        mount = l->data;
        if (g_mount_is_shadowed (mount)) {
            g_object_unref (mount);
            continue;
        }
        volume = g_mount_get_volume (mount);
        if (volume != NULL) {
            g_object_unref (volume);
            g_object_unref (mount);
            continue;
        }
        root = g_mount_get_default_location (mount);

        if (!g_file_is_native (root)) {
            char *scheme = g_file_get_uri_scheme(root);
            //printf ("scheme: %s\n", scheme);
            if (strcmp(scheme, "archive"))
            {
                network_mounts = g_list_prepend (network_mounts, g_object_ref (mount));
                continue;
            }
        }

        icon = g_mount_get_icon (mount);
        mount_uri = g_file_get_uri (root);
        name = g_mount_get_name (mount);
        tooltip = g_file_get_parse_name (root);
        last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME, &iter,
                               name, icon, mount_uri,
                               NULL, NULL, mount, 0, tooltip);
        compare_for_selection (sidebar,
                               location, mount_uri, last_uri,
                               &last_iter, &select_path);
        g_object_unref (root);
        g_object_unref (mount);
        g_object_unref (icon);
        g_free (name);
        g_free (mount_uri);
        g_free (tooltip);
    }
    g_list_free (mounts);

    /* add network category */

    gtk_tree_store_append (sidebar->store, &iter, NULL);
    gtk_tree_store_set (sidebar->store, &iter,
                        PLACES_SIDEBAR_COLUMN_ICON, NULL,
                        PLACES_SIDEBAR_COLUMN_NAME, _("Network"),
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_PERSONAL_CATEGORY,
                        PLACES_SIDEBAR_COLUMN_EJECT, FALSE,
                        PLACES_SIDEBAR_COLUMN_NO_EJECT, TRUE,
                        PLACES_SIDEBAR_COLUMN_BOOKMARK, FALSE,
                        PLACES_SIDEBAR_COLUMN_TOOLTIP, _("Your network places"),
                        -1);

    network_mounts = g_list_reverse (network_mounts);
    for (l = network_mounts; l != NULL; l = l->next) {
        mount = l->data;
        root = g_mount_get_default_location (mount);
        icon = g_mount_get_icon (mount);
        mount_uri = g_file_get_uri (root);
        name = g_mount_get_name (mount);
        tooltip = g_file_get_parse_name (root);
        last_iter = add_place (sidebar, PLACES_BUILT_IN, &iter,
                               name, icon, mount_uri,
                               NULL, NULL, mount, 0, tooltip);
        compare_for_selection (sidebar,
                               location, mount_uri, last_uri,
                               &last_iter, &select_path);
        g_object_unref (root);
        g_object_unref (mount);
        g_object_unref (icon);
        g_free (name);
        g_free (mount_uri);
        g_free (tooltip);
    }

    g_list_foreach (network_mounts, (GFunc) g_object_unref, NULL);
    g_list_free (network_mounts);

    mount_uri = "network:///"; /* No need to strdup */
    icon = g_themed_icon_new (MARLIN_ICON_NETWORK);
    last_iter = add_place (sidebar, PLACES_BUILT_IN, &iter,
                           _("Entire network"), icon,
                           mount_uri, NULL, NULL, NULL, 0,
                           _("Browse the contents of the network"));
    g_object_unref (icon);

    expander_init_pref_state (sidebar->tree_view);

    if (eel_strcmp (location, mount_uri) == 0) {
        gtk_tree_selection_select_iter (selection, &last_iter);
    }
    g_free (location);

    if (select_path != NULL) {
        gtk_tree_selection_select_path (selection, select_path);
        gtk_tree_path_free (select_path);
    }
    g_free (last_uri);
}

static void
mount_added_callback (GVolumeMonitor *volume_monitor,
                      GMount *mount,
                      MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
                        GMount *mount,
                        MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
mount_changed_callback (GVolumeMonitor *volume_monitor,
                        GMount *mount,
                        MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
volume_added_callback (GVolumeMonitor *volume_monitor,
                       GVolume *volume,
                       MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
volume_removed_callback (GVolumeMonitor *volume_monitor,
                         GVolume *volume,
                         MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
volume_changed_callback (GVolumeMonitor *volume_monitor,
                         GVolume *volume,
                         MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
drive_disconnected_callback (GVolumeMonitor *volume_monitor,
                             GDrive         *drive,
                             MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
drive_connected_callback (GVolumeMonitor *volume_monitor,
                          GDrive         *drive,
                          MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static void
drive_changed_callback (GVolumeMonitor *volume_monitor,
                        GDrive         *drive,
                        MarlinPlacesSidebar *sidebar)
{
    update_places (sidebar);
}

static gboolean
over_eject_button (MarlinPlacesSidebar *sidebar,
                   gint x,
                   gint y,
                   GtkTreePath **path)
{
    GtkTreeViewColumn *column;
    GtkTextDirection direction;
    int width, total_width;
    int eject_button_size;
    gboolean show_eject;
    GtkTreeIter iter;

    *path = NULL;

    if (gtk_tree_view_get_path_at_pos (sidebar->tree_view,
                                       x, y,
                                       path, &column, NULL, NULL)) {

        gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store), &iter, *path);
        gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                            PLACES_SIDEBAR_COLUMN_EJECT, &show_eject,
                            -1);

        if (!show_eject) {
            goto out;
        }

        total_width = 0;

        gtk_widget_style_get (GTK_WIDGET (sidebar->tree_view),
                              "horizontal-separator", &width,
                              NULL);
        total_width += width;

        direction = gtk_widget_get_direction (GTK_WIDGET (sidebar->tree_view));
        if (direction != GTK_TEXT_DIR_RTL) {
            gtk_tree_view_column_cell_get_position (column,
                                                    sidebar->icon_cell_renderer,
                                                    NULL, &width);
            total_width += width;

            gtk_tree_view_column_cell_get_position (column,
                                                    sidebar->eject_text_cell_renderer,
                                                    NULL, &width);
            total_width += width;
        }

        total_width += EJECT_BUTTON_XPAD + TEXT_XPAD + ICON_XPAD;

        //TODO
        //eject_button_size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
        eject_button_size = 16;

        if (x - total_width >= 0 && x - total_width <= eject_button_size) {
            return TRUE;
        }
    }

out:
    if (*path != NULL) {
        gtk_tree_path_free (*path);
        *path = NULL;
    }

    return FALSE;
}

static gboolean
clicked_eject_button (MarlinPlacesSidebar *sidebar,
                      GtkTreePath **path)
{
    GdkEvent *event = gtk_get_current_event ();
    GdkEventButton *button_event = (GdkEventButton *) event;

    if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) &&
        over_eject_button (sidebar, button_event->x, button_event->y, path)) {
        return TRUE;
    }

    return FALSE;
}

static void
row_activated_callback (GtkTreeView *tree_view,
                        GtkTreePath *path,
                        GtkTreeViewColumn *column,
                        gpointer user_data)
{
    MarlinPlacesSidebar *sidebar = MARLIN_PLACES_SIDEBAR (user_data);

    //amtest
    printf ("%s\n", G_STRFUNC);
    open_selected_bookmark (sidebar, GTK_TREE_MODEL (sidebar->store), path, 0);
}

/*
   static void
   desktop_location_changed_callback (gpointer user_data)
   {
   MarlinPlacesSidebar *sidebar;

   sidebar = MARLIN_PLACES_SIDEBAR (user_data);

   update_places (sidebar);
   }*/

static void
loading_uri_callback (GtkWidget *window,
                      char *location,
                      MarlinPlacesSidebar *sidebar)
{
    GtkTreeSelection *selection;
    GtkTreeIter 	 iter;
    GtkTreeIter      child_iter;
    gboolean 	 valid;
    gboolean 	 child_valid;
    char  		 *uri;

    if (strcmp (sidebar->uri, location) != 0) {
        g_free (sidebar->uri);
        sidebar->uri = g_strdup (location);

        //amtest
        //printf ("%s %s\n", G_STRFUNC, sidebar->uri);

        /* set selection if any place matches location */
        selection = gtk_tree_view_get_selection (sidebar->tree_view);
        gtk_tree_selection_unselect_all (selection);
        valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sidebar->store), &iter);

        while (valid) {
            child_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (sidebar->store), &child_iter, &iter);
            while (child_valid)
            {
                //printf ("test child: %s\n", gtk_tree_model_get_string_from_iter (GTK_TREE_MODEL (sidebar->store), &child_iter));
                gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &child_iter, 
                                    PLACES_SIDEBAR_COLUMN_URI, &uri,
                                    -1);
                if (uri != NULL) {
                    if (strcmp (uri, location) == 0) {
                        g_free (uri);
                        gtk_tree_selection_select_iter (selection, &child_iter);
                        break;
                    }
                    g_free (uri);
                }
                child_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (sidebar->store), &child_iter);
            }
            valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (sidebar->store), &iter);
        }
    }
}


static unsigned int
get_bookmark_index (GtkTreeView *tree_view)
{
    GtkTreeModel *model;
    GtkTreePath *p;
    GtkTreeIter iter;
    PlaceType place_type;
    int bookmark_index;

    model = gtk_tree_view_get_model (tree_view);

    bookmark_index = -1;

    /* find separator */
    p = gtk_tree_path_new_first ();
    while (p != NULL) {
        gtk_tree_model_get_iter (model, &iter, p);
        gtk_tree_model_get (model, &iter,
                            PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
                            -1);

        if (place_type == PLACES_BOOKMARKS_CATEGORY) {
            bookmark_index = *gtk_tree_path_get_indices (p) + 1;
            break;
        }

        gtk_tree_path_next (p);
    }
    gtk_tree_path_free (p);

    g_assert (bookmark_index >= 0);

    return bookmark_index;
}

/* Computes the appropriate row and position for dropping */
static void
compute_drop_position (GtkTreeView *tree_view,
                       int                      x,
                       int                      y,
                       GtkTreePath            **path,
                       GtkTreeViewDropPosition *pos,
                       MarlinPlacesSidebar *sidebar)
{
    int bookmarks_index;
    int num_rows;
    int row;
    PlaceType place_type;
    GtkTreeIter iter;

    //amtest
    num_rows = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (sidebar->store), NULL);
    /*gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sidebar->store), &iter);
      num_rows = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (sidebar->store), &iter);*/
    //printf ("num rows %d\n", num_rows);

    if (!gtk_tree_view_get_dest_row_at_pos (tree_view,
                                            x,
                                            y,
                                            path,
                                            pos)) {
        //printf ("dest_row_at_pos UNKNOWN\n");
        //row = num_rows - 1;
        //		*path = gtk_tree_path_new_from_indices (row, -1);
        /*gtk_tree_view_get_path_at_pos(tree_view,  x, y, path, NULL, NULL, NULL);

         *pos = GTK_TREE_VIEW_DROP_AFTER;*/
        *pos = -1;
        return;
    }
    //printf ("TEST path %s\n", gtk_tree_path_to_string (*path));
    row = *gtk_tree_path_get_indices (*path);
    /*gint *idxs = gtk_tree_path_get_indices (*path);
      row = idxs[1];*/
    //printf ("row indice %d\n", row);

    gtk_tree_path_free (*path);

    gtk_tree_view_get_path_at_pos(tree_view,  x, y, path, NULL, NULL, NULL);

    if (row == 1 || row == 2) {
        /* Hardcoded shortcuts can only be dragged into */
        *pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
    } else if (row >= num_rows) {
        row = num_rows - 1;
        *pos = GTK_TREE_VIEW_DROP_AFTER;
    } else if (*pos != GTK_TREE_VIEW_DROP_BEFORE &&
               sidebar->drag_data_received &&
               sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
        /* bookmark rows are never dragged into other bookmark rows */
        *pos = GTK_TREE_VIEW_DROP_AFTER;
    }
    if (gtk_tree_path_get_depth(*path) == 1) {
        *pos = -1;
    }
}


static gboolean
get_drag_data (GtkTreeView *tree_view,
               GdkDragContext *context, 
               unsigned int time)
{
    GdkAtom target;

    target = gtk_drag_dest_find_target (GTK_WIDGET (tree_view), context, NULL);

    if (target == GDK_NONE) {
        return FALSE;
    }
    gtk_drag_get_data (GTK_WIDGET (tree_view), context, target, time);

    return TRUE;
}

static void
free_drag_data (MarlinPlacesSidebar *sidebar)
{
    sidebar->drag_data_received = FALSE;

    if (sidebar->drag_list != NULL) {
        g_list_foreach (sidebar->drag_list, (GFunc) g_object_unref, NULL);
        g_list_free (sidebar->drag_list);
        sidebar->drag_list = NULL;
    }
}

static gboolean
can_accept_file_as_bookmark (GOFFile *file)
{
    return (file->is_directory);
}

static gboolean
can_accept_items_as_bookmarks (const GList *items)
{
    int max;
    char *uri;
    GOFFile *file;

    /* Iterate through selection checking if item will get accepted as a bookmark.
     * If more than 100 items selected, return an over-optimistic result.
     */
    for (max = 100; items != NULL && max >= 0; items = items->next, max--) {
        file = gof_file_get (items->data);
        if (!can_accept_file_as_bookmark (file)) {
            gof_file_unref (file);
            return FALSE;
        }
        gof_file_unref (file);
    }

    return TRUE;
}

static gboolean
drag_motion_callback (GtkTreeView *tree_view,
                      GdkDragContext *context,
                      int x,
                      int y,
                      unsigned int time,
                      MarlinPlacesSidebar *sidebar)
{
    //amtest
    GtkTreePath *path;
    GtkTreeViewDropPosition pos;
    //int action;
    GdkDragAction action;
    //GtkTreeIter child_iter;
    GtkTreeIter iter;
    char *uri;

    if (!sidebar->drag_data_received) {
        if (!get_drag_data (tree_view, context, time)) {
            return FALSE;
        }
    }

    compute_drop_position (tree_view, x, y, &path, &pos, sidebar);

    if (pos == GTK_TREE_VIEW_DROP_BEFORE ||
        pos == GTK_TREE_VIEW_DROP_AFTER ) {
        if (sidebar->drag_data_received &&
            sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
            action = GDK_ACTION_MOVE;
        } else if (can_accept_items_as_bookmarks (sidebar->drag_list)) {
            action = GDK_ACTION_COPY;
        } else {
            action = 0;
        }
    } else {
        if (sidebar->drag_list == NULL) {
            action = 0;
        } else {
            //printf ("huuuuuuuuuu?\n");
            if (path != NULL)
            {
                //printf ("huuuuuuuuuu %s path %s\n", G_STRFUNC, gtk_tree_path_to_string (path));
                gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
                                         &iter, path);
                gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store),
                                    &iter,
                                    PLACES_SIDEBAR_COLUMN_URI, &uri,
                                    -1);
                printf ("%s %s\n", G_STRFUNC, uri);
                //printf ("test child: %s\n", gtk_tree_model_get_string_from_iter (GTK_TREE_MODEL (sidebar->store), &child_iter));
                /*marlin_drag_default_drop_action_for_icons (context, uri,
                  sidebar->drag_list,
                  &action);*/
                //amtest
                //action = 0;
                //TODO use GOFFILE instead of uri
                if (uri != NULL) {
                    gof_file_accepts_drop (gof_file_get_by_uri (uri), sidebar->drag_list, context, &action);
                    g_free (uri);
                }
            }
        }
    }

    gtk_tree_view_set_drag_dest_row(tree_view, path, pos);
    gtk_tree_path_free (path);
    g_signal_stop_emission_by_name (tree_view, "drag-motion");

    if (action != 0) {
        gdk_drag_status (context, action, time);
    } else {
        gdk_drag_status (context, 0, time);
    }

    return TRUE;
}

static void
drag_leave_callback (GtkTreeView *tree_view,
                     GdkDragContext *context,
                     unsigned int time,
                     MarlinPlacesSidebar *sidebar)
{
    //amtest drag
    printf ("%s\n", G_STRFUNC);
    free_drag_data (sidebar);
    gtk_tree_view_set_drag_dest_row (tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
    g_signal_stop_emission_by_name (tree_view, "drag-leave");
}

/* Parses a "text/uri-list" string and inserts its URIs as bookmarks */
static void
bookmarks_drop_uris (MarlinPlacesSidebar    *sidebar,
                     GtkSelectionData       *selection_data,
                     int                    position)
{
    MarlinBookmark *bookmark;
    GOFFile *file;
    char *uri;
    char **uris;
    int i;

    uris = gtk_selection_data_get_uris (selection_data);
    if (!uris)
        return;

    if (position < 0)
        position = 0;
    printf ("%s\n", G_STRFUNC);

    for (i = 0; uris[i]; i++) {
        uri = uris[i];
        file = gof_file_get_by_uri (uri);

        if (!can_accept_file_as_bookmark (file)) {
            gof_file_unref (file);
            continue;
        }

        bookmark = marlin_bookmark_new (file->location, file->display_name, TRUE, file->icon);
        if (!marlin_bookmark_list_contains (sidebar->bookmarks, bookmark)) {
            marlin_bookmark_list_insert_item (sidebar->bookmarks, bookmark, position++);
        }

        g_object_unref (bookmark);
    }

    g_strfreev (uris);
}

static gboolean
get_selected_iter (MarlinPlacesSidebar *sidebar,
                   GtkTreeIter *iter)
{
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (sidebar->tree_view);
    if (!gtk_tree_selection_get_selected (selection, NULL, iter)) {
        return FALSE;
    }
    //amtest
    //printf ("TEST %s: %s\n", G_STRFUNC, gtk_tree_model_get_string_from_iter (GTK_TREE_MODEL (sidebar->store), iter));
    return TRUE;
}

/* Reorders the selected bookmark to the specified position */
static void
reorder_bookmarks (MarlinPlacesSidebar *sidebar, int new_position)
{
    GtkTreeIter iter;
    PlaceType type; 
    int old_position;
    GtkTreeSelection *selection;

    /* Get the selected path */
    if (!get_selected_iter (sidebar, &iter)) {
        return;
        //g_assert_not_reached ();
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                        PLACES_SIDEBAR_COLUMN_INDEX, &old_position,
                        -1);

    //printf("%s: old_pos: %d new_pos: %d\n", G_STRFUNC, old_position, new_position);
    old_position = old_position -sidebar->n_builtins_before;
    if (type != PLACES_BOOKMARK ||
        old_position < 0 ||
        old_position >= marlin_bookmark_list_length (sidebar->bookmarks)) {
        return;
    }

    if (new_position < 0)
        new_position = 0;
    marlin_bookmark_list_move_item (sidebar->bookmarks, old_position,
                                    new_position);
}

static void
drag_data_received_callback (GtkWidget *widget,
                             GdkDragContext *context,
                             int x,
                             int y,
                             GtkSelectionData *selection_data,
                             unsigned int info,
                             unsigned int time,
                             MarlinPlacesSidebar *sidebar)
{
    //amtest
    printf ("%s\n", G_STRFUNC);

    GtkTreeView *tree_view;
    GtkTreePath *tree_path;
    GtkTreeViewDropPosition tree_pos;
    GtkTreeIter iter;
    int position;
    GtkTreeModel *model;
    char *drop_uri;
    PlaceType type; 
    gboolean success;

    tree_view = GTK_TREE_VIEW (widget);

    if (!sidebar->drag_data_received) {
        if (gtk_selection_data_get_target (selection_data) != GDK_NONE &&
            info == TEXT_URI_LIST) {
            sidebar->drag_list = eel_g_file_list_new_from_string ((gchar *) gtk_selection_data_get_data (selection_data));
        } else {
            sidebar->drag_list = NULL;
        }
        sidebar->drag_data_received = TRUE;
        sidebar->drag_data_info = info;
    }

    g_signal_stop_emission_by_name (widget, "drag-data-received");

    if (!sidebar->drop_occured) {
        return;
    }

    /* Compute position */
    compute_drop_position (tree_view, x, y, &tree_path, &tree_pos, sidebar);

    success = FALSE;

    if (tree_pos == GTK_TREE_VIEW_DROP_BEFORE ||
        tree_pos == GTK_TREE_VIEW_DROP_AFTER) {
        model = gtk_tree_view_get_model (tree_view);

        if (!gtk_tree_model_get_iter (model, &iter, tree_path)) {
            goto out;
        }

        gtk_tree_model_get (model, &iter,
                            PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                            PLACES_SIDEBAR_COLUMN_INDEX, &position,
                            -1);

        if (!(type == PLACES_BOOKMARK || type == PLACES_BUILT_IN)) {
            goto out;
        }

        if (type == PLACES_BOOKMARK &&
            tree_pos == GTK_TREE_VIEW_DROP_AFTER) {
            position++;
        }

        switch (info) {
        case TEXT_URI_LIST:
            bookmarks_drop_uris (sidebar, selection_data, position - sidebar->n_builtins_before);
            success = TRUE;
            break;
        case GTK_TREE_MODEL_ROW:
            reorder_bookmarks (sidebar, position - sidebar->n_builtins_before);
            success = TRUE;
            break;
        default:
            g_assert_not_reached ();
            break;
        }
    }
    else {
        GdkDragAction real_action;

        /* file transfer requested */
        real_action = gdk_drag_context_get_selected_action (context);

        if (real_action == GDK_ACTION_ASK) {
            real_action =
                marlin_drag_drop_action_ask (GTK_WIDGET (tree_view),
                                             gdk_drag_context_get_actions (context));
        }

        if (real_action > 0) {
            model = gtk_tree_view_get_model (tree_view);

            gtk_tree_model_get_iter (model, &iter, tree_path);
            gtk_tree_model_get (model, &iter,
                                PLACES_SIDEBAR_COLUMN_URI, &drop_uri,
                                -1);

            switch (info) {
            case TEXT_URI_LIST:
                //TODO file_operation
                printf ("file_operation_copy_move: drop_uri %s action %d\n", drop_uri, real_action);
                printf ("%s %s\n", G_STRFUNC, g_file_get_uri (sidebar->drag_list->data));
                GFile *drop_file = g_file_new_for_uri (drop_uri);
                marlin_file_operations_copy_move (sidebar->drag_list, NULL, drop_file,
                                                  real_action, GTK_WIDGET (tree_view),
                                                  NULL, NULL);
                g_object_unref (drop_file);
                success = TRUE;
                break;
            case GTK_TREE_MODEL_ROW:
                success = FALSE;
                break;
            default:
                g_assert_not_reached ();
                break;
            }

            g_free (drop_uri);
        }
    }

out:
    sidebar->drop_occured = FALSE;
    free_drag_data (sidebar);
    gtk_drag_finish (context, success, FALSE, time);

    gtk_tree_path_free (tree_path);
}

static gboolean
drag_drop_callback (GtkTreeView *tree_view,
                    GdkDragContext *context,
                    int x,
                    int y,
                    unsigned int time,
                    MarlinPlacesSidebar *sidebar)
{
    //amtest
    printf ("%s\n", G_STRFUNC);

    gboolean retval = FALSE;
    sidebar->drop_occured = TRUE;
    retval = get_drag_data (tree_view, context, time);
    g_signal_stop_emission_by_name (tree_view, "drag-drop");
    return retval;
}

/* Callback used when the file list's popup menu is detached */
static void
bookmarks_popup_menu_detach_cb (GtkWidget *attach_widget,
                                GtkMenu   *menu)
{
    MarlinPlacesSidebar *sidebar;

    sidebar = MARLIN_PLACES_SIDEBAR (attach_widget);
    g_assert (MARLIN_IS_PLACES_SIDEBAR (sidebar));

    sidebar->popup_menu = NULL;
    sidebar->popup_menu_remove_item = NULL;
    sidebar->popup_menu_rename_item = NULL;
    sidebar->popup_menu_separator_item1 = NULL;
    sidebar->popup_menu_separator_item2 = NULL;
    sidebar->popup_menu_mount_item = NULL;
    sidebar->popup_menu_unmount_item = NULL;
    sidebar->popup_menu_eject_item = NULL;
    sidebar->popup_menu_rescan_item = NULL;
    sidebar->popup_menu_format_item = NULL;
    sidebar->popup_menu_start_item = NULL;
    sidebar->popup_menu_stop_item = NULL;
    sidebar->popup_menu_empty_trash_item = NULL;
}

static void
check_unmount_and_eject (GMount *mount,
                         GVolume *volume,
                         GDrive *drive,
                         gboolean *show_unmount,
                         gboolean *show_eject)
{
    *show_unmount = FALSE;
    *show_eject = FALSE;

    if (drive != NULL) {
        *show_eject = g_drive_can_eject (drive);
    }

    if (volume != NULL) {
        *show_eject |= g_volume_can_eject (volume);
    }
    if (mount != NULL) {
        *show_eject |= g_mount_can_eject (mount);
        *show_unmount = g_mount_can_unmount (mount) && !*show_eject;
    }
}

static void
check_visibility (GMount           *mount,
                  GVolume          *volume,
                  GDrive           *drive,
                  gboolean         *show_mount,
                  gboolean         *show_unmount,
                  gboolean         *show_eject,
                  gboolean         *show_rescan,
                  gboolean         *show_format,
                  gboolean         *show_start,
                  gboolean         *show_stop)
{
    *show_mount = FALSE;
    *show_format = FALSE;
    *show_rescan = FALSE;
    *show_start = FALSE;
    *show_stop = FALSE;

    check_unmount_and_eject (mount, volume, drive, show_unmount, show_eject);

    if (drive != NULL) {
        if (g_drive_is_media_removable (drive) &&
            !g_drive_is_media_check_automatic (drive) && 
            g_drive_can_poll_for_media (drive))
            *show_rescan = TRUE;

        *show_start = g_drive_can_start (drive) || g_drive_can_start_degraded (drive);
        *show_stop  = g_drive_can_stop (drive);

        if (*show_stop)
            *show_unmount = FALSE;
    }

    if (volume != NULL) {
        if (mount == NULL)
            *show_mount = g_volume_can_mount (volume);
    }
}

static void
bookmarks_check_popup_sensitivity (MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    PlaceType type; 
    GDrive *drive = NULL;
    GVolume *volume = NULL;
    GMount *mount = NULL;
    gboolean show_mount;
    gboolean show_unmount;
    gboolean show_eject;
    gboolean show_rescan;
    gboolean show_format;
    gboolean show_start;
    gboolean show_stop;
    gboolean show_empty_trash;
    char *uri = NULL;

    type = PLACES_BUILT_IN;

    if (sidebar->popup_menu == NULL) {
        return;
    }

    if (get_selected_iter (sidebar, &iter)) {
        gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                            PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                            PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                            PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                            PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                            PLACES_SIDEBAR_COLUMN_URI, &uri,
                            -1);
    }

    gtk_widget_show (sidebar->popup_menu_open_in_new_tab_item);

    /*gtk_widget_set_sensitive (sidebar->popup_menu_remove_item, (type == PLACES_BOOKMARK));
      gtk_widget_set_sensitive (sidebar->popup_menu_rename_item, (type == PLACES_BOOKMARK));*/
    eel_gtk_widget_set_shown (sidebar->popup_menu_remove_item, (type == PLACES_BOOKMARK));
    //TODO add the possibility to rename volume later
    eel_gtk_widget_set_shown (sidebar->popup_menu_rename_item, (type == PLACES_BOOKMARK));
    eel_gtk_widget_set_shown (sidebar->popup_menu_separator_item1, (type == PLACES_BOOKMARK));

    gtk_widget_set_sensitive (sidebar->popup_menu_empty_trash_item, !marlin_trash_monitor_is_empty ());

    check_visibility (mount, volume, drive,
                      &show_mount, &show_unmount, &show_eject, &show_rescan, &show_format, &show_start, &show_stop);

    /* We actually want both eject and unmount since eject will unmount all volumes. 
     * TODO: hide unmount if the drive only has a single mountable volume 
     */

    show_empty_trash = (uri != NULL) &&
        (!strcmp (uri, "trash:///"));

    eel_gtk_widget_set_shown (sidebar->popup_menu_mount_item, show_mount);
    eel_gtk_widget_set_shown (sidebar->popup_menu_unmount_item, show_unmount);
    eel_gtk_widget_set_shown (sidebar->popup_menu_eject_item, show_eject);
    /*    eel_gtk_widget_set_shown (sidebar->popup_menu_rescan_item, show_rescan);
          eel_gtk_widget_set_shown (sidebar->popup_menu_format_item, show_format);
          eel_gtk_widget_set_shown (sidebar->popup_menu_start_item, show_start);
          eel_gtk_widget_set_shown (sidebar->popup_menu_stop_item, show_stop);*/
    eel_gtk_widget_set_shown (sidebar->popup_menu_empty_trash_item, show_empty_trash);

    //TODO check this
#if 0
    /* Adjust start/stop items to reflect the type of the drive */
    gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Start"));
    gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Stop"));
    if ((show_start || show_stop) && drive != NULL) {
        switch (g_drive_get_start_stop_type (drive)) {
        case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
            /* start() for type G_DRIVE_START_STOP_TYPE_SHUTDOWN is normally not used */
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Power On"));
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Safely Remove Drive"));
            break;
        case G_DRIVE_START_STOP_TYPE_NETWORK:
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Connect Drive"));
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Disconnect Drive"));
            break;
        case G_DRIVE_START_STOP_TYPE_MULTIDISK:
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Start Multi-disk Device"));
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Stop Multi-disk Device"));
            break;
        case G_DRIVE_START_STOP_TYPE_PASSWORD:
            /* stop() for type G_DRIVE_START_STOP_TYPE_PASSWORD is normally not used */
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Unlock Drive"));
            gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Lock Drive"));
            break;

        default:
        case G_DRIVE_START_STOP_TYPE_UNKNOWN:
            /* uses defaults set above */
            break;
        }
    }
#endif

    g_free (uri);
}

/* Callback used when the selection in the shortcuts tree changes */
static void
bookmarks_selection_changed_cb (GtkTreeSelection      *selection,
                                MarlinPlacesSidebar *sidebar)
{
    bookmarks_check_popup_sensitivity (sidebar);
}

static void
volume_mounted_cb (GVolume *volume,
                   GObject *user_data)
{
    GMount *mount;
    MarlinPlacesSidebar *sidebar;
    GFile *location;

    sidebar = MARLIN_PLACES_SIDEBAR (user_data);

    sidebar->mounting = FALSE;

    mount = g_volume_get_mount (volume);
    if (mount != NULL) {
        location = g_mount_get_default_location (mount);

        if (sidebar->go_to_after_mount_slot != NULL) {
            //TODO
            printf("%s: %s\n", G_STRFUNC, g_file_get_uri (location));
#if 0
            if ((sidebar->go_to_after_mount_flags & MARLIN_WINDOW_OPEN_FLAG_NEW_WINDOW) == 0) {
                /*marlin_window_slot_info_open_location (sidebar->go_to_after_mount_slot, location,
                  MARLIN_WINDOW_OPEN_ACCORDING_TO_MODE,
                  sidebar->go_to_after_mount_flags, NULL);*/
            } else {
                MarlinViewWindow *cur, *new;

                cur = MARLIN_WINDOW (sidebar->window);
                new = marlin_application_create_navigation_window (cur->application,
                                                                   NULL,
                                                                   gtk_window_get_screen (GTK_WINDOW (cur)));
                marlin_window_go_to (new, location);
            }
#endif
        }

        g_object_unref (G_OBJECT (location));
        g_object_unref (G_OBJECT (mount));
    }


    eel_remove_weak_pointer (&(sidebar->go_to_after_mount_slot));
}

static void
drive_start_from_bookmark_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
    GError *error;
    char *primary;
    char *name;

    error = NULL;
    if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
        if (error->code != G_IO_ERROR_FAILED_HANDLED) {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to start %s"), name);
            g_free (name);
            eel_show_error_dialog (primary,
                                   error->message,
                                   NULL);
            g_free (primary);
        }
        g_error_free (error);
    }
}

static void
open_selected_bookmark (MarlinPlacesSidebar         *sidebar,
                        GtkTreeModel	            *model,
                        GtkTreePath	            *path,
                        MarlinViewWindowOpenFlags   flags)
{
    GOFWindowSlot *slot;
    GtkTreeIter iter;
    GFile *location;
    char *uri;

    if (!path) {
        return;
    }

    if (!gtk_tree_model_get_iter (model, &iter, path)) {
        return;
    }

    gtk_tree_model_get (model, &iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);

    if (uri != NULL) {
        printf ("%s: uri: %s\n", G_STRFUNC, uri);
        //amtest
        //#if 0
        /*marlin_debug_log (FALSE, MARLIN_DEBUG_LOG_DOMAIN_USER,
          "activate from places sidebar window=%p: %s",
          sidebar->window, uri);*/
        location = g_file_new_for_uri (uri);
        /* Navigate to the clicked location */
        if ((flags & MARLIN_WINDOW_OPEN_FLAG_NEW_WINDOW) == 0) {
            slot = marlin_view_window_get_active_slot (MARLIN_VIEW_WINDOW (sidebar->window));
            //amtest sidebar load uri
            g_signal_emit_by_name (slot->ctab, "path-changed", location);

            /*marlin_window_slot_info_open_location (slot, location,
              MARLIN_WINDOW_OPEN_ACCORDING_TO_MODE,
              flags, NULL);*/
        } else {
            //TODO once we ll have marlin-application class for managing windows / application
            printf ("%s: uri: %s FLAG_NEW_WINDOW\n", G_STRFUNC, uri);
            /*MarlinViewWindow *cur, *new;

              cur = MARLIN_WINDOW (sidebar->window);
              new = marlin_application_create_navigation_window (cur->application,
              NULL,
              gtk_window_get_screen (GTK_WINDOW (cur)));
              marlin_window_go_to (new, location);*/
        }
        g_object_unref (location);
        //#endif
        g_free (uri);
    } else {
        GDrive *drive;
        GVolume *volume;
        GOFWindowSlot *slot;

        gtk_tree_model_get (model, &iter,
                            PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                            PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                            -1);

        if (volume != NULL && !sidebar->mounting) {
            sidebar->mounting = TRUE;

            g_assert (sidebar->go_to_after_mount_slot == NULL);

            slot = marlin_view_window_get_active_slot (MARLIN_VIEW_WINDOW (sidebar->window));
            sidebar->go_to_after_mount_slot = slot;
            eel_add_weak_pointer (&(sidebar->go_to_after_mount_slot));

            sidebar->go_to_after_mount_flags = flags;

            /* TODO file_operation */
            //amtest
            printf ("%s: marlin_file_operations_mount_volume_full\n", G_STRFUNC);
            marlin_file_operations_mount_volume_full (NULL, volume, FALSE,
                                                      volume_mounted_cb,
                                                      G_OBJECT (sidebar));
        } else if (volume == NULL && drive != NULL &&
                   (g_drive_can_start (drive) || g_drive_can_start_degraded (drive))) {
            GMountOperation *mount_op;

            printf ("%s: gtk_mount_operation_new\n", G_STRFUNC);
            mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
            g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_from_bookmark_cb, NULL);
            g_object_unref (mount_op);
        }

        if (drive != NULL)
            g_object_unref (drive);
        if (volume != NULL)
            g_object_unref (volume);
    }
}

static void
open_shortcut_from_menu (MarlinPlacesSidebar *sidebar, MarlinViewWindowOpenFlags flags)
{
    GtkTreeModel *model;
    GtkTreePath *path;

    model = gtk_tree_view_get_model (sidebar->tree_view);
    gtk_tree_view_get_cursor (sidebar->tree_view, &path, NULL);

    open_selected_bookmark (sidebar, model, path, flags);

    gtk_tree_path_free (path);
}

static void
open_shortcut_cb (GtkMenuItem *item, MarlinPlacesSidebar *sidebar)
{
    open_shortcut_from_menu (sidebar, 0);
}

static void
open_shortcut_in_new_window_cb (GtkMenuItem *item, MarlinPlacesSidebar *sidebar)
{
    open_shortcut_from_menu (sidebar, MARLIN_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
open_shortcut_in_new_tab_cb (GtkMenuItem *item, MarlinPlacesSidebar *sidebar)
{
    open_shortcut_from_menu (sidebar, MARLIN_WINDOW_OPEN_FLAG_NEW_TAB);
}

/* Rename the selected bookmark */
static void
rename_selected_bookmark (MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;
    GList *renderers;

    if (get_selected_iter (sidebar, &iter)) {
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
        column = gtk_tree_view_get_column (GTK_TREE_VIEW (sidebar->tree_view), 0);
        renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
        cell = g_list_nth_data (renderers, 4);
        g_list_free (renderers);
        g_object_set (cell, "editable", TRUE, NULL);
        gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (sidebar->tree_view),
                                          path, column, cell, TRUE);
        gtk_tree_path_free (path);
    }
}

static void
rename_shortcut_cb (GtkMenuItem           *item,
                    MarlinPlacesSidebar *sidebar)
{
    rename_selected_bookmark (sidebar);
}

/* Removes the selected bookmarks */
static void
remove_selected_bookmarks (MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    PlaceType type; 
    int index;

    if (!get_selected_iter (sidebar, &iter)) {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                        -1);

    if (type != PLACES_BOOKMARK) {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_INDEX, &index,
                        -1);
    index = index - sidebar->n_builtins_before;
    marlin_bookmark_list_delete_item_at (sidebar->bookmarks, index);
}

static void
remove_shortcut_cb (GtkMenuItem           *item,
                    MarlinPlacesSidebar *sidebar)
{
    remove_selected_bookmarks (sidebar);
}

static void
mount_shortcut_cb (GtkMenuItem           *item,
                   MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GVolume *volume;

    if (!get_selected_iter (sidebar, &iter)) {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                        -1);

    if (volume != NULL) {
        //amtest
        printf ("%s: marlin_file_operations_mount_volume\n", G_STRFUNC);
        marlin_file_operations_mount_volume (NULL, volume, FALSE);
        g_object_unref (volume);
    }
}

static void
unmount_done (gpointer data)
{
    MarlinViewWindow *window;

    printf("%s\n", G_STRFUNC);
    window = data;
    //TODO
    //marlin_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);
}

static void
do_unmount (GMount *mount,
            MarlinPlacesSidebar *sidebar)
{
    if (mount != NULL) {
        //marlin_window_info_set_initiated_unmount (sidebar->window, TRUE);
        //TODO file_operation
        //amtest
        printf ("%s: marlin_file_operations_unmount_mount_full\n", G_STRFUNC);
        marlin_file_operations_unmount_mount_full (NULL, mount, FALSE, TRUE,
                                                   unmount_done,
                                                   g_object_ref (sidebar->window));
    }
}

static void
do_unmount_selection (MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GMount *mount;

    if (!get_selected_iter (sidebar, &iter)) {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                        -1);

    if (mount != NULL) {
        do_unmount (mount, sidebar);
        g_object_unref (mount);
    }
}

static void
unmount_shortcut_cb (GtkMenuItem           *item,
                     MarlinPlacesSidebar *sidebar)
{
    do_unmount_selection (sidebar);
}

static void
drive_eject_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
    MarlinViewWindow *window;
    GError *error;
    char *primary;
    char *name;

    window = user_data;
    //TODO
    //marlin_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);

    error = NULL;
    if (!g_drive_eject_with_operation_finish (G_DRIVE (source_object), res, &error)) {
        if (error->code != G_IO_ERROR_FAILED_HANDLED) {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to eject %s"), name);
            g_free (name);
            eel_show_error_dialog (primary,
                                   error->message,
                                   NULL);
            g_free (primary);
        }
        g_error_free (error);
    }
}

static void
volume_eject_cb (GObject *source_object,
                 GAsyncResult *res,
                 gpointer user_data)
{
    MarlinViewWindow *window;
    GError *error;
    char *primary;
    char *name;

    window = user_data;
    //marlin_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);

    error = NULL;
    if (!g_volume_eject_with_operation_finish (G_VOLUME (source_object), res, &error)) {
        if (error->code != G_IO_ERROR_FAILED_HANDLED) {
            name = g_volume_get_name (G_VOLUME (source_object));
            primary = g_strdup_printf (_("Unable to eject %s"), name);
            g_free (name);
            eel_show_error_dialog (primary,
                                   error->message,
                                   NULL);
            g_free (primary);
        }
        g_error_free (error);
    }
}

static void
mount_eject_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
    MarlinViewWindow *window;
    GError *error;
    char *primary;
    char *name;

    window = user_data;
    //marlin_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);

    error = NULL;
    if (!g_mount_eject_with_operation_finish (G_MOUNT (source_object), res, &error)) {
        if (error->code != G_IO_ERROR_FAILED_HANDLED) {
            name = g_mount_get_name (G_MOUNT (source_object));
            primary = g_strdup_printf (_("Unable to eject %s"), name);
            g_free (name);
            eel_show_error_dialog (primary,
                                   error->message,
                                   NULL);
            g_free (primary);
        }
        g_error_free (error);
    }
}

static void
do_eject (GMount *mount,
          GVolume *volume,
          GDrive *drive,
          MarlinPlacesSidebar *sidebar)
{
    GMountOperation *mount_op;

    //TODO
    printf ("%s\n", G_STRFUNC);
    mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
    if (mount != NULL) {
        //marlin_window_info_set_initiated_unmount (sidebar->window, TRUE);
        g_mount_eject_with_operation (mount, 0, mount_op, NULL, mount_eject_cb,
                                      g_object_ref (sidebar->window));
    } else if (volume != NULL) {
        //marlin_window_info_set_initiated_unmount (sidebar->window, TRUE);
        g_volume_eject_with_operation (volume, 0, mount_op, NULL, volume_eject_cb,
                                       g_object_ref (sidebar->window));
    } else if (drive != NULL) {
        //marlin_window_info_set_initiated_unmount (sidebar->window, TRUE);
        g_drive_eject_with_operation (drive, 0, mount_op, NULL, drive_eject_cb,
                                      g_object_ref (sidebar->window));
    }
    g_object_unref (mount_op);
}

static void
eject_shortcut_cb (GtkMenuItem           *item,
                   MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GMount *mount;
    GVolume *volume;
    GDrive *drive;

    if (!get_selected_iter (sidebar, &iter)) {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                        PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    do_eject (mount, volume, drive, sidebar);
}

static gboolean
eject_or_unmount_bookmark (MarlinPlacesSidebar *sidebar,
                           GtkTreePath *path)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean can_unmount, can_eject;
    GMount *mount;
    GVolume *volume;
    GDrive *drive;
    gboolean ret;

    model = GTK_TREE_MODEL (sidebar->store);

    if (!path) {
        return FALSE;
    }
    if (!gtk_tree_model_get_iter (model, &iter, path)) {
        return FALSE;
    }

    gtk_tree_model_get (model, &iter,
                        PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                        PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    ret = FALSE;

    check_unmount_and_eject (mount, volume, drive, &can_unmount, &can_eject);
    /* if we can eject, it has priority over unmount */
    if (can_eject) {
        do_eject (mount, volume, drive, sidebar);
        ret = TRUE;
    } else if (can_unmount) {
        do_unmount (mount, sidebar);
        ret = TRUE;
    }

    if (mount != NULL)
        g_object_unref (mount);
    if (volume != NULL)
        g_object_unref (volume);
    if (drive != NULL)
        g_object_unref (drive);

    return ret;
}

static gboolean
eject_or_unmount_selection (MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    gboolean ret;

    if (!get_selected_iter (sidebar, &iter)) {
        return FALSE;
    }

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
    if (path == NULL) {
        return FALSE;
    }

    ret = eject_or_unmount_bookmark (sidebar, path);

    gtk_tree_path_free (path);

    return ret;
}

static void
drive_poll_for_media_cb (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
    GError *error;
    char *primary;
    char *name;

    error = NULL;
    if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
        if (error->code != G_IO_ERROR_FAILED_HANDLED) {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to poll %s for media changes"), name);
            g_free (name);
            eel_show_error_dialog (primary,
                                   error->message,
                                   NULL);
            g_free (primary);
        }
        g_error_free (error);
    }
}

//TODO remove?
#if 0
static void
rescan_shortcut_cb (GtkMenuItem           *item,
                    MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GDrive  *drive;

    if (!get_selected_iter (sidebar, &iter)) {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    if (drive != NULL) {
        g_drive_poll_for_media (drive, NULL, drive_poll_for_media_cb, NULL);
    }
    g_object_unref (drive);
}

static void
format_shortcut_cb (GtkMenuItem           *item,
                    MarlinPlacesSidebar *sidebar)
{
    g_spawn_command_line_async ("gfloppy", NULL);
}

static void
drive_start_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
    GError *error;
    char *primary;
    char *name;

    error = NULL;
    if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
        if (error->code != G_IO_ERROR_FAILED_HANDLED) {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to start %s"), name);
            g_free (name);
            eel_show_error_dialog (primary,
                                   error->message,
                                   NULL);
            g_free (primary);
        }
        g_error_free (error);
    }
}

static void
start_shortcut_cb (GtkMenuItem           *item,
                   MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GDrive  *drive;

    if (!get_selected_iter (sidebar, &iter)) {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    if (drive != NULL) {
        GMountOperation *mount_op;

        mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));

        g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_cb, NULL);

        g_object_unref (mount_op);
    }
    g_object_unref (drive);
}

static void
drive_stop_cb (GObject *source_object,
               GAsyncResult *res,
               gpointer user_data)
{
    MarlinViewWindow *window;
    GError *error;
    char *primary;
    char *name;

    window = user_data;
    //marlin_window_info_set_initiated_unmount (window, FALSE);
    g_object_unref (window);

    error = NULL;
    if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
        if (error->code != G_IO_ERROR_FAILED_HANDLED) {
            name = g_drive_get_name (G_DRIVE (source_object));
            primary = g_strdup_printf (_("Unable to stop %s"), name);
            g_free (name);
            eel_show_error_dialog (primary,
                                   error->message,
                                   NULL);
            g_free (primary);
        }
        g_error_free (error);
    }
}

static void
stop_shortcut_cb (GtkMenuItem           *item,
                  MarlinPlacesSidebar *sidebar)
{
    GtkTreeIter iter;
    GDrive  *drive;

    if (!get_selected_iter (sidebar, &iter)) {
        return;
    }

    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                        -1);

    if (drive != NULL) {
        GMountOperation *mount_op;

        mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
        //marlin_window_info_set_initiated_unmount (sidebar->window, TRUE);
        g_drive_stop (drive, G_MOUNT_UNMOUNT_NONE, mount_op, NULL, drive_stop_cb,
                      g_object_ref (sidebar->window));
        g_object_unref (mount_op);
    }
    g_object_unref (drive);
}
#endif

static void
empty_trash_cb (GtkMenuItem           *item,
                MarlinPlacesSidebar *sidebar)
{
    //TODO file_operation
    printf ("%s\n", G_STRFUNC);
    marlin_file_operations_empty_trash (GTK_WIDGET (sidebar->window));
}

/* Handler for GtkWidget::key-press-event on the shortcuts list */
static gboolean
bookmarks_key_press_event_cb (GtkWidget             *widget,
                              GdkEventKey           *event,
                              MarlinPlacesSidebar *sidebar)
{
    guint modifiers;

    modifiers = gtk_accelerator_get_default_mod_mask ();

    if (event->keyval == GDK_KEY_Down &&
        (event->state & modifiers) == GDK_MOD1_MASK) {
        return eject_or_unmount_selection (sidebar);
    }

    if ((event->keyval == GDK_KEY_Delete
         || event->keyval == GDK_KEY_KP_Delete)
        && (event->state & modifiers) == 0) {
        remove_selected_bookmarks (sidebar);
        return TRUE;
    }

    if ((event->keyval == GDK_KEY_F2)
        && (event->state & modifiers) == 0) {
        rename_selected_bookmark (sidebar);
        return TRUE;
    }

    return FALSE;
}

/* Constructs the popup menu for the file list if needed */
static void
bookmarks_build_popup_menu (MarlinPlacesSidebar *sidebar)
{
    GtkWidget *item;

    if (sidebar->popup_menu) {
        return;
    }

    sidebar->popup_menu = gtk_menu_new ();
    gtk_menu_attach_to_widget (GTK_MENU (sidebar->popup_menu),
                               GTK_WIDGET (sidebar),
                               bookmarks_popup_menu_detach_cb);

    item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                   gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
    g_signal_connect (item, "activate",
                      G_CALLBACK (open_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
    sidebar->popup_menu_open_in_new_tab_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (open_shortcut_in_new_tab_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("Open in New _Window"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (open_shortcut_in_new_window_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    sidebar->popup_menu_separator_item1 =
        GTK_WIDGET (eel_gtk_menu_append_separator (GTK_MENU (sidebar->popup_menu)));

    item = gtk_image_menu_item_new_with_label (_("Remove"));
    sidebar->popup_menu_remove_item = item;
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                   gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
    g_signal_connect (item, "activate",
                      G_CALLBACK (remove_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_label (_("Rename..."));
    sidebar->popup_menu_rename_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (rename_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    /* Mount/Unmount/Eject menu items */

    sidebar->popup_menu_separator_item2 =
        GTK_WIDGET (eel_gtk_menu_append_separator (GTK_MENU (sidebar->popup_menu)));

    item = gtk_menu_item_new_with_mnemonic (_("_Mount"));
    sidebar->popup_menu_mount_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (mount_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("_Unmount"));
    sidebar->popup_menu_unmount_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (unmount_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    item = gtk_menu_item_new_with_mnemonic (_("_Eject"));
    sidebar->popup_menu_eject_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (eject_shortcut_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    /*item = gtk_menu_item_new_with_mnemonic (_("_Detect Media"));
      sidebar->popup_menu_rescan_item = item;
      g_signal_connect (item, "activate",
      G_CALLBACK (rescan_shortcut_cb), sidebar);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

      item = gtk_menu_item_new_with_mnemonic (_("_Format"));
      sidebar->popup_menu_format_item = item;
      g_signal_connect (item, "activate",
      G_CALLBACK (format_shortcut_cb), sidebar);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

      item = gtk_menu_item_new_with_mnemonic (_("_Start"));
      sidebar->popup_menu_start_item = item;
      g_signal_connect (item, "activate",
      G_CALLBACK (start_shortcut_cb), sidebar);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

      item = gtk_menu_item_new_with_mnemonic (_("_Stop"));
      sidebar->popup_menu_stop_item = item;
      g_signal_connect (item, "activate",
      G_CALLBACK (stop_shortcut_cb), sidebar);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);*/

    /* Empty Trash menu item */

    item = gtk_menu_item_new_with_mnemonic (_("Empty _Trash"));
    sidebar->popup_menu_empty_trash_item = item;
    g_signal_connect (item, "activate",
                      G_CALLBACK (empty_trash_cb), sidebar);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

    bookmarks_check_popup_sensitivity (sidebar);
}

static void
bookmarks_update_popup_menu (MarlinPlacesSidebar *sidebar)
{
    bookmarks_build_popup_menu (sidebar);  
}

static void
bookmarks_popup_menu (MarlinPlacesSidebar *sidebar,
                      GdkEventButton        *event)
{
    bookmarks_update_popup_menu (sidebar);
    eel_pop_up_context_menu (GTK_MENU(sidebar->popup_menu),
                             EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
                             EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
                             event);
}

/* Callback used for the GtkWidget::popup-menu signal of the shortcuts list */
static gboolean
bookmarks_popup_menu_cb (GtkWidget *widget,
                         MarlinPlacesSidebar *sidebar)
{
    bookmarks_popup_menu (sidebar, NULL);
    return TRUE;
}

static gboolean
bookmarks_button_release_event_cb (GtkWidget *widget,
                                   GdkEventButton *event,
                                   MarlinPlacesSidebar *sidebar)
{
    //amtest
    printf ("%s\n", G_STRFUNC);
    GtkTreePath *path;
    GtkTreeModel *model;
    GtkTreeView *tree_view;

    if (event->type != GDK_BUTTON_RELEASE) {
        return TRUE;
    }

    if (clicked_eject_button (sidebar, &path)) {
        eject_or_unmount_bookmark (sidebar, path);
        gtk_tree_path_free (path);
        return FALSE;
    }

    tree_view = GTK_TREE_VIEW (widget);
    model = gtk_tree_view_get_model (tree_view);

    if (event->button == 1) {

        if (event->window != gtk_tree_view_get_bin_window (tree_view)) {
            return FALSE;
        }

        gtk_tree_view_get_path_at_pos (tree_view, (int) event->x, (int) event->y,
                                       &path, NULL, NULL, NULL);

        open_selected_bookmark (sidebar, model, path, 0);

        gtk_tree_path_free (path);
    }

    return FALSE;
}

/* Callback used when a button is pressed on the shortcuts list.  
 * We trap button 3 to bring up a popup menu, and button 2 to
 * open in a new tab.
 */
static gboolean
bookmarks_button_press_event_cb (GtkWidget             *widget,
                                 GdkEventButton        *event,
                                 MarlinPlacesSidebar *sidebar)
{
    //amtest
    printf ("%s\n", G_STRFUNC);
    if (event->type != GDK_BUTTON_PRESS) {
        /* ignore multiple clicks */
        return TRUE;
    }

    if (event->button == 3) {
        bookmarks_popup_menu (sidebar, event);
    } else if (event->button == 2) {
        GtkTreeModel *model;
        GtkTreePath *path;
        GtkTreeView *tree_view;

        tree_view = GTK_TREE_VIEW (widget);
        g_assert (tree_view == sidebar->tree_view);

        model = gtk_tree_view_get_model (tree_view);

        gtk_tree_view_get_path_at_pos (tree_view, (int) event->x, (int) event->y, 
                                       &path, NULL, NULL, NULL);
        //printf ("selected path %s\n", gtk_tree_path_to_string (path));
        //printf ("%s open_selected_bookmark ...\n", G_STRFUNC);
        open_selected_bookmark (sidebar, model, path,
                                event->state & GDK_CONTROL_MASK ?
                                MARLIN_WINDOW_OPEN_FLAG_NEW_WINDOW :
                                MARLIN_WINDOW_OPEN_FLAG_NEW_TAB);

        if (path != NULL) {
            gtk_tree_path_free (path);
            return TRUE;
        }
    }

    return FALSE;
}

static void
update_eject_buttons (MarlinPlacesSidebar   *sidebar,
                      GtkTreePath 	    *path)
{
    GtkTreeIter iter;
    gboolean icon_visible, path_same;

    icon_visible = TRUE;
    if (path == NULL && sidebar->eject_highlight_path == NULL) {
        /* Both are null - highlight up to date */
        return;
    }

    path_same = (path != NULL) &&
        (sidebar->eject_highlight_path != NULL) &&
        (gtk_tree_path_compare (sidebar->eject_highlight_path, path) == 0);

    if (path_same) {
        /* Same path - highlight up to date */
        return;
    }

    if (path) {
        gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
                                 &iter,
                                 path);

        gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store),
                            &iter,
                            PLACES_SIDEBAR_COLUMN_EJECT, &icon_visible,
                            -1);
    }

    if (!icon_visible || path == NULL || !path_same) {
        /* remove highlighting and reset the saved path, as we are leaving
         * an eject button area.
         */
        if (sidebar->eject_highlight_path) {
            gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
                                     &iter,
                                     sidebar->eject_highlight_path);

            gtk_tree_store_set (sidebar->store,
                                &iter,
                                PLACES_SIDEBAR_COLUMN_EJECT_ICON, get_eject_icon (FALSE),
                                -1);
            //gtk_tree_model_row_changed (GTK_TREE_MODEL (sidebar->store), path, &iter);

            gtk_tree_path_free (sidebar->eject_highlight_path);
            sidebar->eject_highlight_path = NULL;
        }

        if (!icon_visible) {
            return;
        }
    }

    if (path != NULL) {
        /* add highlighting to the selected path, as the icon is visible and
         * we're hovering it.
         */
        gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
                                 &iter,
                                 path);
        gtk_tree_store_set (sidebar->store,
                            &iter,
                            PLACES_SIDEBAR_COLUMN_EJECT_ICON, get_eject_icon (TRUE),
                            -1);
        //gtk_tree_model_row_changed (GTK_TREE_MODEL (sidebar->store), path, &iter);

        sidebar->eject_highlight_path = gtk_tree_path_copy (path);
    }
}

static gboolean
bookmarks_motion_event_cb (GtkWidget            *widget,
                           GdkEventMotion       *event,
                           MarlinPlacesSidebar  *sidebar)
{
    GtkTreePath *path;
    GtkTreeModel *model;

    model = GTK_TREE_MODEL (sidebar->store);
    path = NULL;

    if (over_eject_button (sidebar, event->x, event->y, &path)) {
        update_eject_buttons (sidebar, path);
        gtk_tree_path_free (path);

        return TRUE;
    }

    update_eject_buttons (sidebar, NULL);

    return FALSE;
}

static void
bookmarks_edited (GtkCellRenderer       *cell,
                  gchar                 *path_string,
                  gchar                 *new_text,
                  MarlinPlacesSidebar *sidebar)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    MarlinBookmark *bookmark;
    int index;

    g_object_set (cell, "editable", FALSE, NULL);

    path = gtk_tree_path_new_from_string (path_string);
    gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store), &iter, path);
    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                        PLACES_SIDEBAR_COLUMN_INDEX, &index,
                        -1);
    index = index - sidebar->n_builtins_before;
    gtk_tree_path_free (path);
    bookmark = marlin_bookmark_list_item_at (sidebar->bookmarks, index);

    if (bookmark != NULL) {
        marlin_bookmark_set_name (bookmark, new_text);
        update_places (sidebar);
    }
}

static void
bookmarks_editing_canceled (GtkCellRenderer       *cell,
                            MarlinPlacesSidebar *sidebar)
{
    g_object_set (cell, "editable", FALSE, NULL);
}

static void
trash_state_changed_cb (MarlinTrashMonitor *trash_monitor,
                        gboolean             state,
                        gpointer             data)
{
    MarlinPlacesSidebar *sidebar;

    sidebar = MARLIN_PLACES_SIDEBAR (data);

    /* The trash icon changed, update the sidebar */
    update_places (sidebar);

    bookmarks_check_popup_sensitivity (sidebar);
}

static void
icon_cell_data_func (GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     MarlinPlacesSidebar *sidebar)
{
    if (!gtk_tree_model_iter_has_child (model, iter)) {
        g_object_set (cell, "visible", TRUE, NULL);
    } else {
        g_object_set (cell, "visible", FALSE, NULL);
    }
}

static void
indent_cell_data_func (GtkTreeViewColumn *tree_column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       MarlinPlacesSidebar *sidebar)
{
    GtkTreePath *path;
    int          depth;

    path = gtk_tree_model_get_path (model, iter);
    depth = gtk_tree_path_get_depth (path);
    gtk_tree_path_free (path);
    g_object_set (cell,
                  "visible", depth>1,
                  "xpad", ICON_XPAD,
                  NULL);

}

static void
expander_update_pref_state (PlaceType type, gboolean flag)
{
    switch (type) {
    case PLACES_PERSONAL_CATEGORY:
        /* Network */
        g_settings_set_boolean (settings, MARLIN_PREFERENCES_SIDEBAR_CAT_NETWORK_EXPANDER, flag);
        break;
    case PLACES_STORAGE_CATEGORY:
        /* Devices */
        g_settings_set_boolean (settings, MARLIN_PREFERENCES_SIDEBAR_CAT_DEVICES_EXPANDER, flag);
        break;
    case PLACES_BOOKMARKS_CATEGORY:
        /* Personal */
        g_settings_set_boolean (settings, MARLIN_PREFERENCES_SIDEBAR_CAT_PERSONAL_EXPANDER, flag);
        break;
    }
}

static void
expander_init_pref_state (GtkTreeView *tree_view)
{
    GtkTreePath *path;

    path = gtk_tree_path_new_from_indices (0, -1);
    if (g_settings_get_boolean (settings, MARLIN_PREFERENCES_SIDEBAR_CAT_PERSONAL_EXPANDER))
        gtk_tree_view_expand_row (tree_view, path, FALSE);
    else
        gtk_tree_view_collapse_row (tree_view, path);
    gtk_tree_path_free (path);

    path = gtk_tree_path_new_from_indices (1, -1);
    if (g_settings_get_boolean (settings, MARLIN_PREFERENCES_SIDEBAR_CAT_DEVICES_EXPANDER))
        gtk_tree_view_expand_row (tree_view, path, FALSE);
    else
        gtk_tree_view_collapse_row (tree_view, path);
    gtk_tree_path_free (path);

    path = gtk_tree_path_new_from_indices (2, -1);
    if (g_settings_get_boolean (settings, MARLIN_PREFERENCES_SIDEBAR_CAT_NETWORK_EXPANDER))
        gtk_tree_view_expand_row (tree_view, path, FALSE);
    else
        gtk_tree_view_collapse_row (tree_view, path);
    gtk_tree_path_free (path);
}

static void
expander_cell_data_func (GtkTreeViewColumn *tree_column,
                         GtkCellRenderer *cell,
                         GtkTreeModel *model,
                         GtkTreeIter *iter,
                         MarlinPlacesSidebar *sidebar)
{
    PlaceType	 	type; 

    gtk_tree_model_get (model, iter, PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type, -1);

    if (type == PLACES_PERSONAL_CATEGORY || type == PLACES_STORAGE_CATEGORY || type == PLACES_BOOKMARKS_CATEGORY) {
        GtkTreePath *path;
        gboolean     row_expanded;

        path = gtk_tree_model_get_path (model, iter);
        row_expanded = gtk_tree_view_row_expanded (GTK_TREE_VIEW (gtk_tree_view_column_get_tree_view (tree_column)), path);
        gtk_tree_path_free (path);

        g_object_set (sidebar->expander_renderer,
                      "visible", TRUE,
                      "expander-style", row_expanded ? GTK_EXPANDER_EXPANDED : GTK_EXPANDER_COLLAPSED,
                      NULL);
    } else {
        g_object_set (sidebar->expander_renderer, "visible", FALSE, NULL);
    }
}

static void
category_row_expanded_event_cb (GtkTreeView             *tree,
                                GtkTreeIter             *iter,
                                GtkTreePath             *path,
                                MarlinPlacesSidebar *sidebar)
{
    PlaceType type;
    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), iter, PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type, -1);
    expander_update_pref_state (type, TRUE);
}

static void
category_row_collapsed_event_cb (GtkTreeView             *tree,
                                 GtkTreeIter             *iter,
                                 GtkTreePath             *path,
                                 MarlinPlacesSidebar *sidebar)
{
    PlaceType type;
    gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), iter, PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type, -1);
    expander_update_pref_state (type, FALSE);
}

static void
marlin_places_sidebar_init (MarlinPlacesSidebar *sidebar)
{
    GtkTreeView       *tree_view;
    GtkTreeViewColumn *col;
    GtkTreeViewColumn *expcol;
    GtkCellRenderer   *cell;
    GtkTreeSelection  *selection;

    sidebar->volume_monitor = g_volume_monitor_get ();

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
    gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
    /* remove ugly shadow from Places */
    //gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sidebar), GTK_SHADOW_IN);

    /* tree view */
    tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
    gtk_tree_view_set_headers_visible (tree_view, FALSE);

    col = GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new ());

    cell = gtk_cell_renderer_text_new ();
    sidebar->indent_renderer = cell;
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (col, cell,
                                             (GtkTreeCellDataFunc) indent_cell_data_func,
                                             sidebar,
                                             NULL);

    cell = gtk_cell_renderer_pixbuf_new ();
    sidebar->icon_cell_renderer = cell;
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "pixbuf", PLACES_SIDEBAR_COLUMN_ICON,
                                         NULL);
    gtk_tree_view_column_set_cell_data_func (col, cell,
                                             (GtkTreeCellDataFunc) icon_cell_data_func,
                                             sidebar,
                                             NULL);

    cell = gtk_cell_renderer_text_new ();
    sidebar->eject_text_cell_renderer = cell;
    gtk_tree_view_column_pack_start (col, cell, TRUE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "text", PLACES_SIDEBAR_COLUMN_NAME,
                                         "visible", PLACES_SIDEBAR_COLUMN_EJECT,
                                         NULL);
    g_object_set (cell,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  "ellipsize-set", TRUE,
                  NULL);


    cell = gtk_cell_renderer_pixbuf_new ();
    g_object_set (cell,
                  "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                  //TODO
                  "stock-size", 16,
                  "xpad", EJECT_BUTTON_XPAD,
                  NULL);
    gtk_tree_view_column_pack_start (col, cell, FALSE);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "visible", PLACES_SIDEBAR_COLUMN_EJECT,
                                         "pixbuf", PLACES_SIDEBAR_COLUMN_EJECT_ICON,
                                         NULL);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (col, cell, TRUE);
    g_object_set (G_OBJECT (cell), "editable", FALSE, NULL);
    gtk_tree_view_column_set_attributes (col, cell,
                                         "text", PLACES_SIDEBAR_COLUMN_NAME,
                                         "visible", PLACES_SIDEBAR_COLUMN_NO_EJECT,
                                         "editable-set", PLACES_SIDEBAR_COLUMN_BOOKMARK,
                                         NULL);
    g_object_set (cell,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  "ellipsize-set", TRUE,
                  NULL);

    gtk_tree_view_column_set_cell_data_func (col, cell,
                                             category_renderer_func, NULL, NULL);

    g_signal_connect (cell, "edited", 
                      G_CALLBACK (bookmarks_edited), sidebar);
    g_signal_connect (cell, "editing-canceled", 
                      G_CALLBACK (bookmarks_editing_canceled), sidebar);

    g_object_set (tree_view, "show-expanders", FALSE, NULL);

    /* Expander */

    cell = gossip_cell_renderer_expander_new ();
    sidebar->expander_renderer = cell;
    gtk_tree_view_column_pack_end (col, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (col, 
                                             cell, 
                                             (GtkTreeCellDataFunc) expander_cell_data_func,
                                             sidebar,
                                             NULL);

    /* this is required to align the eject buttons to the right */
    gtk_tree_view_column_set_max_width (GTK_TREE_VIEW_COLUMN (col), NAUTILUS_ICON_SIZE_SMALLER);
    gtk_tree_view_column_set_expand(col, TRUE);
    gtk_tree_view_append_column (tree_view, col);

    /* this is required to set the category cells to bold and higher than the other ones */
    sidebar->store = gtk_tree_store_new (PLACES_SIDEBAR_COLUMN_COUNT,
                                         G_TYPE_INT, 
                                         G_TYPE_STRING,
                                         G_TYPE_DRIVE,
                                         G_TYPE_VOLUME,
                                         G_TYPE_MOUNT,
                                         G_TYPE_STRING,
                                         GDK_TYPE_PIXBUF,
                                         G_TYPE_INT,
                                         G_TYPE_BOOLEAN,
                                         G_TYPE_BOOLEAN,
                                         G_TYPE_BOOLEAN,
                                         G_TYPE_STRING,
                                         GDK_TYPE_PIXBUF
                                        );

    gtk_tree_view_set_tooltip_column (tree_view, PLACES_SIDEBAR_COLUMN_TOOLTIP);

    gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (sidebar->store));

    gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));

    gtk_widget_show (GTK_WIDGET (tree_view));
    gtk_widget_show (GTK_WIDGET (sidebar));
    sidebar->tree_view = tree_view;

    gtk_tree_view_set_search_column (tree_view, PLACES_SIDEBAR_COLUMN_NAME);
    selection = gtk_tree_view_get_selection (tree_view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

    g_signal_connect_object (tree_view, "row_activated", 
                             G_CALLBACK (row_activated_callback), sidebar, 0);

    gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tree_view),
                                            GDK_BUTTON1_MASK,
                                            marlin_shortcuts_source_targets,
                                            G_N_ELEMENTS (marlin_shortcuts_source_targets),
                                            GDK_ACTION_MOVE);
    gtk_drag_dest_set (GTK_WIDGET (tree_view),
                       0,
                       marlin_shortcuts_drop_targets, G_N_ELEMENTS (marlin_shortcuts_drop_targets),
                       GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);

    g_signal_connect (tree_view, "key-press-event",
                      G_CALLBACK (bookmarks_key_press_event_cb), sidebar);

    g_signal_connect (tree_view, "drag-motion",
                      G_CALLBACK (drag_motion_callback), sidebar);
    g_signal_connect (tree_view, "drag-leave",
                      G_CALLBACK (drag_leave_callback), sidebar);
    g_signal_connect (tree_view, "drag-data-received",
                      G_CALLBACK (drag_data_received_callback), sidebar);
    g_signal_connect (tree_view, "drag-drop",
                      G_CALLBACK (drag_drop_callback), sidebar);

    g_signal_connect (selection, "changed",
                      G_CALLBACK (bookmarks_selection_changed_cb), sidebar);
    g_signal_connect (tree_view, "popup-menu",
                      G_CALLBACK (bookmarks_popup_menu_cb), sidebar);
    g_signal_connect (tree_view, "button-press-event",
                      G_CALLBACK (bookmarks_button_press_event_cb), sidebar);
    g_signal_connect (tree_view, "button-release-event",
                      G_CALLBACK (bookmarks_button_release_event_cb), sidebar);
    g_signal_connect (tree_view, "motion-notify-event",
                      G_CALLBACK (bookmarks_motion_event_cb), sidebar);
    g_signal_connect (tree_view, "row-expanded",
                      G_CALLBACK (category_row_expanded_event_cb), sidebar);
    g_signal_connect (tree_view, "row-collapsed",
                      G_CALLBACK (category_row_collapsed_event_cb), sidebar);

    /* TODO remove/keep? using this prohibit us to use the row_activated signal 
       no keyboard abilities in the sidebar */
    //eel_gtk_tree_view_set_activate_on_single_click (sidebar->tree_view, TRUE);

    /*g_signal_connect_swapped (marlin_preferences, "changed::" MARLIN_PREFERENCES_DESKTOP_IS_HOME_DIR,
      G_CALLBACK(desktop_location_changed_callback),
      sidebar);*/
    /*eel_preferences_add_callback_while_alive (MARLIN_PREFERENCES_DESKTOP_IS_HOME_DIR,
      desktop_location_changed_callback,
      sidebar,
      G_OBJECT (sidebar));*/

    g_signal_connect_object (marlin_trash_monitor_get (),
                             "trash_state_changed",
                             G_CALLBACK (trash_state_changed_cb),
                             sidebar, 0);
}

static void
marlin_places_sidebar_dispose (GObject *object)
{
    MarlinPlacesSidebar *sidebar;

    sidebar = MARLIN_PLACES_SIDEBAR (object);

    sidebar->window = NULL;
    sidebar->tree_view = NULL;

    g_free (sidebar->uri);
    sidebar->uri = NULL;

    free_drag_data (sidebar);

    if (sidebar->eject_highlight_path != NULL) {
        gtk_tree_path_free (sidebar->eject_highlight_path);
        sidebar->eject_highlight_path = NULL;
    }

    if (sidebar->store != NULL) {
        g_object_unref (sidebar->store);
        sidebar->store = NULL;
    }

    if (sidebar->volume_monitor != NULL) {
        g_object_unref (sidebar->volume_monitor);
        sidebar->volume_monitor = NULL;
    }

    if (sidebar->bookmarks != NULL) {
        g_object_unref (sidebar->bookmarks);
        sidebar->bookmarks = NULL;
    }

    eel_remove_weak_pointer (&(sidebar->go_to_after_mount_slot));

    /*g_signal_handlers_disconnect_by_func (marlin_preferences,
      desktop_location_changed_callback,
      sidebar);*/

    G_OBJECT_CLASS (marlin_places_sidebar_parent_class)->dispose (object);
}

static void
marlin_places_sidebar_class_init (MarlinPlacesSidebarClass *class)
{
    G_OBJECT_CLASS (class)->dispose = marlin_places_sidebar_dispose;

    GTK_WIDGET_CLASS (class)->style_set = marlin_places_sidebar_style_set;
}

static void
marlin_places_sidebar_set_parent_window (MarlinPlacesSidebar *sidebar,
                                         GtkWidget *window)
{
    GOFWindowSlot *slot;

    sidebar->window = window;

    slot = marlin_view_window_get_active_slot (MARLIN_VIEW_WINDOW (window));

    //TODO
    sidebar->bookmarks = marlin_bookmark_list_new ();
    /* maybe store the uri in slot structure */
    if (slot)
        sidebar->uri = g_file_get_uri (slot->location);

    g_signal_connect_object (sidebar->bookmarks, "contents_changed",
                             G_CALLBACK (update_places),
                             sidebar, G_CONNECT_SWAPPED);

    /*g_signal_connect_object (window, "loading_uri",
      G_CALLBACK (loading_uri_callback),
      sidebar, 0);*/

    g_signal_connect_object (sidebar->volume_monitor, "volume_added",
                             G_CALLBACK (volume_added_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "volume_removed",
                             G_CALLBACK (volume_removed_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "volume_changed",
                             G_CALLBACK (volume_changed_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "mount_added",
                             G_CALLBACK (mount_added_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "mount_removed",
                             G_CALLBACK (mount_removed_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "mount_changed",
                             G_CALLBACK (mount_changed_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "drive_disconnected",
                             G_CALLBACK (drive_disconnected_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "drive_connected",
                             G_CALLBACK (drive_connected_callback), sidebar, 0);
    g_signal_connect_object (sidebar->volume_monitor, "drive_changed",
                             G_CALLBACK (drive_changed_callback), sidebar, 0);

    //TODO check this
    //update_places (sidebar);
}

static void
marlin_places_sidebar_style_set (GtkWidget *widget,
                                 GtkStyle  *previous_style)
{
    MarlinPlacesSidebar *sidebar;

    sidebar = MARLIN_PLACES_SIDEBAR (widget);

    update_places (sidebar);
}

MarlinPlacesSidebar *
marlin_places_sidebar_new (GtkWidget *window)
{
    MarlinPlacesSidebar *sidebar;

    sidebar = g_object_new (MARLIN_TYPE_PLACES_SIDEBAR, NULL);
    marlin_places_sidebar_set_parent_window (sidebar, window);
    //g_object_ref_sink (sidebar);

    return (sidebar);
}

