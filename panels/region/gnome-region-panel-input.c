/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#endif

#ifdef HAVE_FCITX
#include <fcitx-gclient/fcitxinputmethod.h>
#include <fcitx-utils/utils.h>
#endif

#include "gdm-languages.h"
#include "gnome-region-panel-input.h"

#define WID(s) GTK_WIDGET(gtk_builder_get_object (builder, s))

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"

#define KEY_CURRENT_INPUT_SOURCE "current"
#define KEY_INPUT_SOURCES        "sources"

#define INPUT_SOURCE_TYPE_XKB  "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

enum {
  NAME_COLUMN,
  TYPE_COLUMN,
  ID_COLUMN,
  SETUP_COLUMN,
  N_COLUMNS
};

static GSettings *input_sources_settings = NULL;
static GnomeXkbInfo *xkb_info = NULL;

#ifdef HAVE_IBUS
static IBusBus *ibus = NULL;
static GHashTable *ibus_engines = NULL;

static const gchar *supported_ibus_engines[] = {
  "bopomofo",
  "pinyin",
  "chewing",
  "anthy",
  "hangul",
  NULL
};
#endif  /* HAVE_IBUS */

#ifdef HAVE_FCITX
#define INPUT_SOURCE_TYPE_FCITX "fcitx"

static FcitxInputMethod *fcitx_im = NULL;
static GPtrArray *fcitx_imlist = NULL;
static gchar *fcitx_selected_im = NULL;

typedef struct {
  GtkTreeView *treeview;
  gchar *unique_name;
} FcitxForeachContext;

static void       clear_fcitx                  (void);
static void       fcitx_imlist_changed_cb      (FcitxInputMethod *fcitx_im,
                                                GtkBuilder       *builder);
static gint       fcitx_get_im_index           (GtkTreeModel     *model,
                                                GtkTreeIter      *iter);
static gint       fcitx_get_selected_im_index  (GtkBuilder       *builder);
static void       fcitx_move_selected_im_up    (GtkBuilder       *builder);
static void       fcitx_move_selected_im_down  (GtkBuilder       *builder);
static gboolean   fcitx_tree_modle_foreach_cb  (GtkTreeModel     *model,
                                                GtkTreePath      *path,
                                                GtkTreeIter      *iter,
                                                gpointer          data);
static gboolean   fcitx_is_connected           (void);
#endif  /* HAVE_FCITX */

static gboolean   get_selected_iter            (GtkBuilder       *builder,
                                                GtkTreeModel    **model,
                                                GtkTreeIter      *iter);
static void       set_selected_path            (GtkBuilder       *builder,
                                                GtkTreePath      *path);
static void       populate_with_active_sources (GtkListStore     *store);
static GtkWidget *input_chooser_new          (GtkWindow     *main_window,
                                              GtkListStore  *active_sources);
static gboolean   input_chooser_get_selected (GtkWidget     *chooser,
                                              GtkTreeModel **model,
                                              GtkTreeIter   *iter);

#ifdef HAVE_FCITX
static void
clear_fcitx (void)
{
  if (fcitx_im)
    {
      g_signal_handlers_disconnect_by_func (fcitx_im,
                                            "imlist-changed",
                                            G_CALLBACK (fcitx_imlist_changed_cb));
      g_clear_object (&fcitx_im);
    }
  if (fcitx_imlist)
    {
      g_ptr_array_set_free_func (fcitx_imlist, fcitx_im_item_free);
      g_ptr_array_free (fcitx_imlist, TRUE);
      fcitx_imlist = NULL;
    }
  g_clear_pointer (&fcitx_selected_im, g_free);
}

static gboolean
fcitx_is_connected (void)
{
  gchar *name_owner;
  gboolean connected;
  if (!FCITX_IS_INPUT_METHOD (fcitx_im))
    return FALSE;
  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (fcitx_im));
  connected = (name_owner != NULL);
  if (name_owner)
    g_free (name_owner);
  return connected;
}

static gboolean
fcitx_tree_modle_foreach_cb  (GtkTreeModel *model,
                              GtkTreePath  *path,
                              GtkTreeIter  *iter,
                              gpointer      data)
{
  FcitxIMItem *imitem;
  GtkTreeSelection *selection;
  FcitxForeachContext *context;
  gint index = fcitx_get_im_index (model, iter);
  if (index < 0)
    return FALSE;
  context = data;
  imitem = g_ptr_array_index (fcitx_imlist, index);
  if (g_str_equal (context->unique_name, imitem->unique_name))
    {
      selection = gtk_tree_view_get_selection (context->treeview);
      gtk_tree_selection_select_iter (selection, iter);
      return TRUE;
    }
  return FALSE;
}

static void
fcitx_imlist_changed_cb (FcitxInputMethod *fcitx_im,
                         GtkBuilder       *builder)
{
  GtkWidget *treeview;
  GtkTreeModel *store;
  GtkTreePath *path;
  gint index;
  FcitxIMItem *imitem;
  GtkTreeIter iter;
  GtkTreeModel *model;
  FcitxForeachContext foreach_context;

  treeview = WID("active_input_sources");
  store = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  if (!fcitx_selected_im && fcitx_imlist)
    {
      index = fcitx_get_selected_im_index (builder);
      if (index >= 0)
      {
        imitem = g_ptr_array_index (fcitx_imlist, index);
        fcitx_selected_im = g_strdup (imitem->unique_name);
      }
    }

  if (get_selected_iter (builder, &model, &iter))
    path = gtk_tree_model_get_path (model, &iter);
  else
    path = NULL;

  if (fcitx_imlist)
    {
      g_ptr_array_set_free_func (fcitx_imlist, fcitx_im_item_free);
      g_ptr_array_free (fcitx_imlist, TRUE);
    }
  fcitx_imlist = fcitx_input_method_get_imlist (fcitx_im);

  gtk_list_store_clear (GTK_LIST_STORE (store));
  populate_with_active_sources (GTK_LIST_STORE (store));

  if (fcitx_imlist && fcitx_selected_im)
    {
      foreach_context.treeview = GTK_TREE_VIEW (treeview);
      foreach_context.unique_name = fcitx_selected_im;
      gtk_tree_model_foreach (model,
                              fcitx_tree_modle_foreach_cb,
                              (gpointer) &foreach_context);
      g_clear_pointer (&fcitx_selected_im, g_free);
    }

  if (path)
    {
      if (!fcitx_imlist || fcitx_get_selected_im_index (builder) < 0)
        set_selected_path (builder, path);
      gtk_tree_path_free (path);
    }
}

static gint
fcitx_get_im_index (GtkTreeModel *model,
                    GtkTreeIter  *iter)
{
  gchar *id;
  gchar *type;
  gint index = -1;
  if (!fcitx_imlist)
    return -1;
  gtk_tree_model_get (model, iter, TYPE_COLUMN, &type, ID_COLUMN, &id, -1);
  if (!g_str_equal (type, INPUT_SOURCE_TYPE_FCITX))
    goto exit;
  index = atoi (id);
  if (index >= fcitx_imlist->len)
    {
      g_warning ("Index of selected IM out of range, the index is %u", index);
      index = -1;
    }
 exit:
  g_free (id);
  g_free (type);
  return index;
}

static gint
fcitx_get_selected_im_index (GtkBuilder *builder)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (get_selected_iter (builder, &model, &iter) == FALSE)
    return -1;
  return fcitx_get_im_index (model, &iter);
}

static void
fcitx_move_selected_im_up (GtkBuilder *builder)
{
  gint index = fcitx_get_selected_im_index (builder);
  gint i;
  if (index < 0)
    return;
  for (i = index - 1; i >= 0; i--)
    {
      FcitxIMItem *imitem = g_ptr_array_index (fcitx_imlist, i);
      if (imitem->enable)
        break;
    }
  if (i >= 0)
    {
      g_clear_pointer (&fcitx_selected_im, g_free);
      FcitxIMItem *temp = g_ptr_array_index (fcitx_imlist, index);
      fcitx_selected_im = g_strdup (temp->unique_name);
      g_ptr_array_index (fcitx_imlist, index) = g_ptr_array_index (fcitx_imlist, i);
      g_ptr_array_index (fcitx_imlist, i) = temp;
      fcitx_input_method_set_imlist (fcitx_im, fcitx_imlist);
    }
}

static void
fcitx_move_selected_im_down (GtkBuilder *builder)
{
  gint index = fcitx_get_selected_im_index (builder);
  gint i;
  if (index < 0)
    return;
  for (i = index + 1; i < fcitx_imlist->len; i++)
    {
      FcitxIMItem *imitem = g_ptr_array_index (fcitx_imlist, i);
      if (imitem->enable)
        break;
    }
  if (i < fcitx_imlist->len)
    {
      g_clear_pointer (&fcitx_selected_im, g_free);
      FcitxIMItem *temp = g_ptr_array_index (fcitx_imlist, index);
      fcitx_selected_im = g_strdup (temp->unique_name);
      g_ptr_array_index (fcitx_imlist, index) = g_ptr_array_index (fcitx_imlist, i);
      g_ptr_array_index (fcitx_imlist, i) = temp;
      fcitx_input_method_set_imlist (fcitx_im, fcitx_imlist);
    }
}
#endif  /* HAVE_FCITX */

#ifdef HAVE_IBUS
static void
clear_ibus (void)
{
  g_clear_pointer (&ibus_engines, g_hash_table_destroy);
  g_clear_object (&ibus);
}

static gchar *
engine_get_display_name (IBusEngineDesc *engine_desc)
{
  const gchar *name;
  const gchar *language_code;
  gchar *language;
  gchar *display_name;

  name = ibus_engine_desc_get_longname (engine_desc);
  language_code = ibus_engine_desc_get_language (engine_desc);

  language = gdm_get_language_from_name (language_code, NULL);

  display_name = g_strdup_printf ("%s (%s)", language, name);

  g_free (language);

  return display_name;
}

static GDesktopAppInfo *
setup_app_info_for_id (const gchar *id)
{
  GDesktopAppInfo *app_info;
  gchar *desktop_file_name;

  desktop_file_name = g_strdup_printf ("ibus-setup-%s.desktop", id);
  app_info = g_desktop_app_info_new (desktop_file_name);
  g_free (desktop_file_name);

  return app_info;
}

static void
update_ibus_active_sources (GtkBuilder *builder)
{
  GtkTreeView *tv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *type, *id;
  gboolean ret;

  tv = GTK_TREE_VIEW (WID ("active_input_sources"));
  model = gtk_tree_view_get_model (tv);

  ret = gtk_tree_model_get_iter_first (model, &iter);
  while (ret)
    {
      gtk_tree_model_get (model, &iter,
                          TYPE_COLUMN, &type,
                          ID_COLUMN, &id,
                          -1);

      if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
        {
          IBusEngineDesc *engine_desc = NULL;
          gchar *display_name = NULL;

          engine_desc = g_hash_table_lookup (ibus_engines, id);
          if (engine_desc)
            {
              display_name = engine_get_display_name (engine_desc);

              gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                  NAME_COLUMN, display_name,
                                  -1);
              g_free (display_name);
            }
        }

      g_free (type);
      g_free (id);

      ret = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
fetch_ibus_engines (GtkBuilder *builder)
{
  IBusEngineDesc **engines, **iter;
  const gchar *name;

  ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  engines = ibus_bus_get_engines_by_names (ibus, supported_ibus_engines);
  for (iter = engines; *iter; ++iter)
    {
      name = ibus_engine_desc_get_name (*iter);
      g_hash_table_replace (ibus_engines, (gpointer)name, *iter);
    }

  g_free (engines);

  update_ibus_active_sources (builder);

  /* We've got everything we needed, don't want to be called again. */
  g_signal_handlers_disconnect_by_func (ibus, fetch_ibus_engines, builder);
}

static void
maybe_start_ibus (void)
{
  /* IBus doesn't export API in the session bus. The only thing
   * we have there is a well known name which we can use as a
   * sure-fire way to activate it. */
  g_bus_unwatch_name (g_bus_watch_name (G_BUS_TYPE_SESSION,
                                        IBUS_SERVICE_IBUS,
                                        G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL));
}
#endif  /* HAVE_IBUS */

static gboolean
add_source_to_table (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      data)
{
  GHashTable *hash = data;
  gchar *type;
  gchar *id;

  gtk_tree_model_get (model, iter,
                      TYPE_COLUMN, &type,
                      ID_COLUMN, &id,
                      -1);

  g_hash_table_add (hash, g_strconcat (type, id, NULL));

  g_free (type);
  g_free (id);

  return FALSE;
}

static void
populate_model (GtkListStore *store,
                GtkListStore *active_sources_store)
{
  GHashTable *active_sources_table;
  GtkTreeIter iter;
  const gchar *name;
  GList *sources, *tmp;
  gchar *source_id = NULL;

  active_sources_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  gtk_tree_model_foreach (GTK_TREE_MODEL (active_sources_store),
                          add_source_to_table,
                          active_sources_table);

  sources = gnome_xkb_info_get_all_layouts (xkb_info);

  for (tmp = sources; tmp; tmp = tmp->next)
    {
      g_free (source_id);
      source_id = g_strconcat (INPUT_SOURCE_TYPE_XKB, tmp->data, NULL);

      if (g_hash_table_contains (active_sources_table, source_id))
        continue;

      gnome_xkb_info_get_layout_info (xkb_info, (const gchar *)tmp->data,
                                      &name, NULL, NULL, NULL);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          NAME_COLUMN, name,
                          TYPE_COLUMN, INPUT_SOURCE_TYPE_XKB,
                          ID_COLUMN, tmp->data,
                          -1);
    }
  g_free (source_id);

  g_list_free (sources);

#ifdef HAVE_IBUS
  if (ibus_engines)
    {
      gchar *display_name;

      sources = g_hash_table_get_keys (ibus_engines);

      source_id = NULL;
      for (tmp = sources; tmp; tmp = tmp->next)
        {
          g_free (source_id);
          source_id = g_strconcat (INPUT_SOURCE_TYPE_IBUS, tmp->data, NULL);

          if (g_hash_table_contains (active_sources_table, source_id))
            continue;

          display_name = engine_get_display_name (g_hash_table_lookup (ibus_engines, tmp->data));

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              NAME_COLUMN, display_name,
                              TYPE_COLUMN, INPUT_SOURCE_TYPE_IBUS,
                              ID_COLUMN, tmp->data,
                              -1);
          g_free (display_name);
        }
      g_free (source_id);

      g_list_free (sources);
    }
#endif

  g_hash_table_destroy (active_sources_table);
}

static void
populate_with_active_sources (GtkListStore *store)
{
  GVariant *sources;
  GVariantIter iter;
  const gchar *name;
  const gchar *type;
  const gchar *id;
  gchar *display_name;
  GDesktopAppInfo *app_info;
  GtkTreeIter tree_iter;

#ifdef HAVE_FCITX
  if (fcitx_imlist)
    {
      guint i;
      for (i = 0; i < fcitx_imlist->len; i++)
      {
        FcitxIMItem *imitem;
        gchar *id;
        imitem = g_ptr_array_index (fcitx_imlist, i);
        if (!imitem->enable)
          continue;
        id = g_strdup_printf ("%u", i);
        gtk_list_store_append (store, &tree_iter);
        gtk_list_store_set (store, &tree_iter,
                            NAME_COLUMN, imitem->name,
                            TYPE_COLUMN, INPUT_SOURCE_TYPE_FCITX,
                            ID_COLUMN, id,
                            SETUP_COLUMN, NULL,
                            -1);
        g_free (id);
      }
      return;
    }
#endif  /* HAVE_FCITX */

  sources = g_settings_get_value (input_sources_settings, KEY_INPUT_SOURCES);

  g_variant_iter_init (&iter, sources);
  while (g_variant_iter_next (&iter, "(&s&s)", &type, &id))
    {
      display_name = NULL;
      app_info = NULL;

      if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
        {
          gnome_xkb_info_get_layout_info (xkb_info, id, &name, NULL, NULL, NULL);
          if (!name)
            {
              g_warning ("Couldn't find XKB input source '%s'", id);
              continue;
            }
          display_name = g_strdup (name);
        }
      else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
        {
#ifdef HAVE_IBUS
          IBusEngineDesc *engine_desc = NULL;

          if (ibus_engines)
            engine_desc = g_hash_table_lookup (ibus_engines, id);

          if (engine_desc)
            display_name = engine_get_display_name (engine_desc);

          app_info = setup_app_info_for_id (id);
#else
          g_warning ("IBus input source type specified but IBus support was not compiled");
          continue;
#endif
        }
      else
        {
          g_warning ("Unknown input source type '%s'", type);
          continue;
        }

      gtk_list_store_append (store, &tree_iter);
      gtk_list_store_set (store, &tree_iter,
                          NAME_COLUMN, display_name ? display_name : id,
                          TYPE_COLUMN, type,
                          ID_COLUMN, id,
                          SETUP_COLUMN, app_info,
                          -1);
      g_free (display_name);
      if (app_info)
        g_object_unref (app_info);
    }

  g_variant_unref (sources);
}

static void
update_configuration (GtkTreeModel *model)
{
  GtkTreeIter iter;
  gchar *type;
  gchar *id;
  GVariantBuilder builder;
  GVariant *old_sources;
  const gchar *old_current_type;
  const gchar *old_current_id;
  guint old_current_index;
  guint index;

#ifdef HAVE_FCITX
  if (fcitx_is_connected ())
    return;
#endif

  old_sources = g_settings_get_value (input_sources_settings, KEY_INPUT_SOURCES);
  old_current_index = g_settings_get_uint (input_sources_settings, KEY_CURRENT_INPUT_SOURCE);
  g_variant_get_child (old_sources,
                       old_current_index,
                       "(&s&s)",
                       &old_current_type,
                       &old_current_id);
  if (g_variant_n_children (old_sources) < 1)
    {
      g_warning ("No input source configured, resetting");
      g_settings_reset (input_sources_settings, KEY_INPUT_SOURCES);
      goto exit;
    }
  if (old_current_index >= g_variant_n_children (old_sources))
    {
      g_settings_set_uint (input_sources_settings,
                           KEY_CURRENT_INPUT_SOURCE,
                           g_variant_n_children (old_sources) - 1);
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  index = 0;
  gtk_tree_model_get_iter_first (model, &iter);
  do
    {
      gtk_tree_model_get (model, &iter,
                          TYPE_COLUMN, &type,
                          ID_COLUMN, &id,
                          -1);
      if (index != old_current_index &&
          g_str_equal (type, old_current_type) &&
          g_str_equal (id, old_current_id))
        {
          g_settings_set_uint (input_sources_settings, KEY_CURRENT_INPUT_SOURCE, index);
        }
      g_variant_builder_add (&builder, "(ss)", type, id);
      g_free (type);
      g_free (id);
      index += 1;
    }
  while (gtk_tree_model_iter_next (model, &iter));

  g_settings_set_value (input_sources_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));

 exit:
  g_settings_apply (input_sources_settings);
  g_variant_unref (old_sources);
}

static gboolean
get_selected_iter (GtkBuilder    *builder,
                   GtkTreeModel **model,
                   GtkTreeIter   *iter)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("active_input_sources")));

  return gtk_tree_selection_get_selected (selection, model, iter);
}

static gint
idx_from_model_iter (GtkTreeModel *model,
                     GtkTreeIter  *iter)
{
  GtkTreePath *path;
  gint idx;

  path = gtk_tree_model_get_path (model, iter);
  if (path == NULL)
    return -1;

  idx = gtk_tree_path_get_indices (path)[0];
  gtk_tree_path_free (path);

  return idx;
}

static void
update_button_sensitivity (GtkBuilder *builder)
{
  GtkWidget *remove_button;
  GtkWidget *up_button;
  GtkWidget *down_button;
  GtkWidget *show_button;
  GtkWidget *settings_button;
  GtkTreeView *tv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint n_active;
  gint index;
  gboolean settings_sensitive;
  GDesktopAppInfo *app_info;

  remove_button = WID("input_source_remove");
  show_button = WID("input_source_show");
  up_button = WID("input_source_move_up");
  down_button = WID("input_source_move_down");
  settings_button = WID("input_source_settings");

  tv = GTK_TREE_VIEW (WID ("active_input_sources"));
  n_active = gtk_tree_model_iter_n_children (gtk_tree_view_get_model (tv), NULL);

  if (get_selected_iter (builder, &model, &iter))
    {
      index = idx_from_model_iter (model, &iter);
      gtk_tree_model_get (model, &iter, SETUP_COLUMN, &app_info, -1);
    }
  else
    {
      index = -1;
      app_info = NULL;
    }

  settings_sensitive = (index >= 0 && app_info != NULL);

  if (app_info)
    g_object_unref (app_info);

  gtk_widget_set_sensitive (remove_button, index >= 0 && n_active > 1);
  gtk_widget_set_sensitive (show_button, index >= 0);
  gtk_widget_set_sensitive (up_button, index > 0);
  gtk_widget_set_sensitive (down_button, index >= 0 && index < n_active - 1);
  gtk_widget_set_sensitive (settings_button, settings_sensitive);
}

static void
set_selected_path (GtkBuilder  *builder,
                   GtkTreePath *path)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("active_input_sources")));

  gtk_tree_selection_select_path (selection, path);
}

static void
chooser_response (GtkWidget *chooser, gint response_id, gpointer data)
{
  GtkBuilder *builder = data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GtkTreeModel *model;
      GtkTreeIter iter;

      if (input_chooser_get_selected (chooser, &model, &iter))
        {
          GtkTreeView *tv;
          GtkListStore *my_model;
          GtkTreeIter child_iter;
          gchar *name;
          gchar *type;
          gchar *id;
          GDesktopAppInfo *app_info = NULL;

          gtk_tree_model_get (model, &iter,
                              NAME_COLUMN, &name,
                              TYPE_COLUMN, &type,
                              ID_COLUMN, &id,
                              -1);

#ifdef HAVE_IBUS
          if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
            app_info = setup_app_info_for_id (id);
#endif

          tv = GTK_TREE_VIEW (WID ("active_input_sources"));
          my_model = GTK_LIST_STORE (gtk_tree_view_get_model (tv));

          gtk_list_store_insert_with_values (my_model, &child_iter, -1,
                                             NAME_COLUMN, name,
                                             TYPE_COLUMN, type,
                                             ID_COLUMN, id,
                                             SETUP_COLUMN, app_info,
                                             -1);
          g_free (name);
          g_free (type);
          g_free (id);
          if (app_info)
            g_object_unref (app_info);

          gtk_tree_selection_select_iter (gtk_tree_view_get_selection (tv), &child_iter);

          update_button_sensitivity (builder);
          update_configuration (GTK_TREE_MODEL (my_model));
        }
      else
        {
          g_debug ("nothing selected, nothing added");
        }
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
add_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkWidget *chooser;
  GtkWidget *toplevel;
  GtkWidget *treeview;
  GtkListStore *active_sources;

  g_debug ("add an input source");

  toplevel = gtk_widget_get_toplevel (WID ("region_notebook"));
  treeview = WID ("active_input_sources");
  active_sources = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));

  chooser = input_chooser_new (GTK_WINDOW (toplevel), active_sources);
  g_signal_connect (chooser, "response",
                    G_CALLBACK (chooser_response), builder);
}

static void
remove_selected_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  g_debug ("remove selected input source");

  if (get_selected_iter (builder, &model, &iter) == FALSE)
    return;

  path = gtk_tree_model_get_path (model, &iter);

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    gtk_tree_path_prev (path);

  set_selected_path (builder, path);

  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (model);
}

static void
move_selected_input_up (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter, prev;
  GtkTreePath *path;

  g_debug ("move selected input source up");

#ifdef HAVE_FCITX
  if (fcitx_is_connected ())
    {
      fcitx_move_selected_im_up (builder);
      return;
    }
#endif

  if (!get_selected_iter (builder, &model, &iter))
    return;

  prev = iter;
  if (!gtk_tree_model_iter_previous (model, &prev))
    return;

  path = gtk_tree_model_get_path (model, &prev);

  gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &prev);

  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (model);
}

static void
move_selected_input_down (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter, next;
  GtkTreePath *path;

  g_debug ("move selected input source down");

#ifdef HAVE_FCITX
  if (fcitx_is_connected ())
    {
      fcitx_move_selected_im_down (builder);
      return;
    }
#endif

  if (!get_selected_iter (builder, &model, &iter))
    return;

  next = iter;
  if (!gtk_tree_model_iter_next (model, &next))
    return;

  path = gtk_tree_model_get_path (model, &next);

  gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &next);

  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (model);
}

static void
show_selected_layout (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *type;
  gchar *id;
  gchar *kbd_viewer_args;
  const gchar *xkb_layout;
  const gchar *xkb_variant;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      TYPE_COLUMN, &type,
                      ID_COLUMN, &id,
                      -1);

  if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
    {
      gnome_xkb_info_get_layout_info (xkb_info, id, NULL, NULL, &xkb_layout, &xkb_variant);

      if (!xkb_layout || !xkb_layout[0])
        {
          g_warning ("Couldn't find XKB input source '%s'", id);
          goto exit;
        }
    }
  else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
    {
#ifdef HAVE_IBUS
      IBusEngineDesc *engine_desc = NULL;

      if (ibus_engines)
        engine_desc = g_hash_table_lookup (ibus_engines, id);

      if (engine_desc)
        {
          xkb_layout = ibus_engine_desc_get_layout (engine_desc);
          xkb_variant = "";
        }
      else
        {
          g_warning ("Couldn't find IBus input source '%s'", id);
          goto exit;
        }
#else
      g_warning ("IBus input source type specified but IBus support was not compiled");
      goto exit;
#endif
    }
  else
    {
      g_warning ("Unknown input source type '%s'", type);
      goto exit;
    }

  if (xkb_variant[0])
    kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l \"%s\t%s\"",
                                       xkb_layout, xkb_variant);
  else
    kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l %s",
                                       xkb_layout);

  g_spawn_command_line_async (kbd_viewer_args, NULL);

  g_free (kbd_viewer_args);
 exit:
  g_free (type);
  g_free (id);
}

static void
show_selected_settings (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GdkAppLaunchContext *ctx;
  GDesktopAppInfo *app_info;
  GError *error = NULL;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, SETUP_COLUMN, &app_info, -1);

  if (!app_info)
    return;

  ctx = gdk_display_get_app_launch_context (gdk_display_get_default ());
  gdk_app_launch_context_set_timestamp (ctx, gtk_get_current_event_time ());

  if (!g_app_info_launch (G_APP_INFO (app_info), NULL, G_APP_LAUNCH_CONTEXT (ctx), &error))
    {
      g_warning ("Failed to launch input source setup: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (ctx);
  g_object_unref (app_info);
}

static gboolean
go_to_shortcuts (GtkLinkButton *button,
                 CcRegionPanel *panel)
{
  CcShell *shell;
  const gchar *argv[] = { "shortcuts", NULL };
  GError *error = NULL;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  if (!cc_shell_set_active_panel_from_id (shell, "keyboard", argv, &error))
    {
      g_warning ("Failed to activate Keyboard panel: %s", error->message);
      g_error_free (error);
    }

  return TRUE;
}

static void
input_sources_changed (GSettings  *settings,
                       gchar      *key,
                       GtkBuilder *builder)
{
  GtkWidget *treeview;
  GtkTreeModel *store;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeModel *model;

#ifdef HAVE_FCITX
  if (fcitx_is_connected ())
    return;
#endif

  treeview = WID("active_input_sources");
  store = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  if (get_selected_iter (builder, &model, &iter))
    path = gtk_tree_model_get_path (model, &iter);
  else
    path = NULL;

  gtk_list_store_clear (GTK_LIST_STORE (store));
  populate_with_active_sources (GTK_LIST_STORE (store));

  if (path)
    {
      set_selected_path (builder, path);
      gtk_tree_path_free (path);
    }
}

static void
update_shortcut_label (GtkWidget  *widget,
		       const char *value)
{
  char *text;
  guint accel_key, *keycode;
  GdkModifierType mods;

  if (value == NULL || *value == '\0')
    {
      gtk_label_set_text (GTK_LABEL (widget), "\342\200\224");
      return;
    }
  gtk_accelerator_parse_with_keycode (value, &accel_key, &keycode, &mods);
  if (accel_key == 0 && keycode == NULL && mods == 0)
    {
      gtk_label_set_text (GTK_LABEL (widget), "\342\200\224");
      g_warning ("Failed to parse keyboard shortcut: '%s'", value);
      return;
    }

  text = gtk_accelerator_get_label_with_keycode (gtk_widget_get_display (widget), accel_key, *keycode, mods);
  g_free (keycode);
  gtk_label_set_text (GTK_LABEL (widget), text);
  g_free (text);
}

static void
update_shortcuts (GtkBuilder *builder)
{
  char *previous, *next;
  GSettings *settings;

  settings = g_settings_new ("org.gnome.settings-daemon.plugins.media-keys");

  previous = g_settings_get_string (settings, "switch-input-source-backward");
  next = g_settings_get_string (settings, "switch-input-source");

  update_shortcut_label (WID ("prev-source-shortcut-label"), previous);
  update_shortcut_label (WID ("next-source-shortcut-label"), next);

  g_free (previous);
  g_free (next);
}

void
setup_input_tabs (GtkBuilder    *builder,
                  CcRegionPanel *panel)
{
  GtkWidget *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkListStore *store;
  GtkTreeSelection *selection;

  /* set up the list of active inputs */
  treeview = WID("active_input_sources");
  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_add_attribute (column, cell, "text", NAME_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  store = gtk_list_store_new (N_COLUMNS,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_DESKTOP_APP_INFO);

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));

  input_sources_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
  g_settings_delay (input_sources_settings);
  g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) g_object_unref, input_sources_settings);

  if (!xkb_info)
    xkb_info = gnome_xkb_info_new ();

#ifdef HAVE_IBUS
  ibus_init ();
  if (!ibus)
    {
      ibus = ibus_bus_new_async ();
      if (ibus_bus_is_connected (ibus))
        fetch_ibus_engines (builder);
      else
        g_signal_connect_swapped (ibus, "connected",
                                  G_CALLBACK (fetch_ibus_engines), builder);
      g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) clear_ibus, NULL);
    }
  maybe_start_ibus ();
#endif

#ifdef HAVE_FCITX
  {
    GError *error = NULL;
    if (!fcitx_im)
      {
        fcitx_im = fcitx_input_method_new (G_BUS_TYPE_SESSION,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           fcitx_utils_get_display_number (),
                                           NULL,
                                           &error);
        if (!fcitx_im)
          {
            g_warning ("Failed to create FcitxInputMethod instance: %s",
                       error->message);
            g_error_free (error);
          }
        else
          {
            g_signal_connect (G_OBJECT (fcitx_im),
                              "imlist-changed",
                              G_CALLBACK (fcitx_imlist_changed_cb),
                              builder);
            fcitx_imlist = fcitx_input_method_get_imlist (fcitx_im);
            g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) clear_fcitx, NULL);
          }
      }
  }
#endif

  populate_with_active_sources (store);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect_swapped (selection, "changed",
                            G_CALLBACK (update_button_sensitivity), builder);

  /* set up the buttons */
  g_signal_connect (WID("input_source_add"), "clicked",
                    G_CALLBACK (add_input), builder);
  g_signal_connect (WID("input_source_remove"), "clicked",
                    G_CALLBACK (remove_selected_input), builder);
  g_signal_connect (WID("input_source_move_up"), "clicked",
                    G_CALLBACK (move_selected_input_up), builder);
  g_signal_connect (WID("input_source_move_down"), "clicked",
                    G_CALLBACK (move_selected_input_down), builder);
  g_signal_connect (WID("input_source_show"), "clicked",
                    G_CALLBACK (show_selected_layout), builder);
  g_signal_connect (WID("input_source_settings"), "clicked",
                    G_CALLBACK (show_selected_settings), builder);

  /* use an em dash is no shortcut */
  update_shortcuts (builder);

  g_signal_connect (WID("jump-to-shortcuts"), "activate-link",
                    G_CALLBACK (go_to_shortcuts), panel);

  g_signal_connect (G_OBJECT (input_sources_settings),
                    "changed::" KEY_INPUT_SOURCES,
                    G_CALLBACK (input_sources_changed),
                    builder);
}

static void
filter_clear (GtkEntry             *entry,
              GtkEntryIconPosition  icon_pos,
              GdkEvent             *event,
              gpointer              user_data)
{
  gtk_entry_set_text (entry, "");
}

static gchar **search_pattern_list;

static void
filter_changed (GtkBuilder *builder)
{
  GtkTreeModelFilter *filtered_model;
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;
  GtkTreeIter selected_iter;
  GtkWidget *filter_entry;
  const gchar *pattern;
  gchar *upattern;

  filter_entry = WID ("input_source_filter");
  pattern = gtk_entry_get_text (GTK_ENTRY (filter_entry));
  upattern = g_utf8_strup (pattern, -1);
  if (!g_strcmp0 (pattern, ""))
    g_object_set (G_OBJECT (filter_entry),
                  "secondary-icon-name", "edit-find-symbolic",
                  "secondary-icon-activatable", FALSE,
                  "secondary-icon-sensitive", FALSE,
                  NULL);
  else
    g_object_set (G_OBJECT (filter_entry),
                  "secondary-icon-name", "edit-clear-symbolic",
                  "secondary-icon-activatable", TRUE,
                  "secondary-icon-sensitive", TRUE,
                  NULL);

  if (search_pattern_list != NULL)
    g_strfreev (search_pattern_list);

  search_pattern_list = g_strsplit (upattern, " ", -1);
  g_free (upattern);

  filtered_model = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "filtered_input_source_model"));
  gtk_tree_model_filter_refilter (filtered_model);

  tree_view = GTK_TREE_VIEW (WID ("filtered_input_source_list"));
  selection = gtk_tree_view_get_selection (tree_view);
  if (gtk_tree_selection_get_selected (selection, NULL, &selected_iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (filtered_model),
                                                   &selected_iter);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.5);
      gtk_tree_path_free (path);
    }
  else
    {
      GtkTreeIter iter;
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filtered_model), &iter))
        gtk_tree_selection_select_iter (selection, &iter);
    }
}

static void
selection_changed (GtkTreeSelection *selection,
                   GtkBuilder       *builder)
{
  gtk_widget_set_sensitive (WID ("ok-button"),
                            gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
row_activated (GtkTreeView       *tree_view,
               GtkTreePath       *path,
               GtkTreeViewColumn *column,
               GtkBuilder        *builder)
{
  GtkWidget *add_button;
  GtkWidget *dialog;

  add_button = WID ("ok-button");
  dialog = WID ("input_source_chooser");
  if (gtk_widget_is_sensitive (add_button))
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
entry_activated (GtkBuilder *builder,
                 gpointer    data)
{
  row_activated (NULL, NULL, NULL, builder);
}

static gboolean
filter_func (GtkTreeModel *model,
             GtkTreeIter  *iter,
             gpointer      data)
{
  gchar *name = NULL;
  gchar **pattern;
  gboolean rv = TRUE;

  if (search_pattern_list == NULL || search_pattern_list[0] == NULL)
    return TRUE;

  gtk_tree_model_get (model, iter,
                      NAME_COLUMN, &name,
                      -1);

  pattern = search_pattern_list;
  do {
    gboolean is_pattern_found = FALSE;
    gchar *udesc = g_utf8_strup (name, -1);
    if (udesc != NULL && g_strstr_len (udesc, -1, *pattern))
      {
        is_pattern_found = TRUE;
      }
    g_free (udesc);

    if (!is_pattern_found)
      {
        rv = FALSE;
        break;
      }

  } while (*++pattern != NULL);

  g_free (name);

  return rv;
}

static GtkWidget *
input_chooser_new (GtkWindow    *main_window,
                   GtkListStore *active_sources)
{
  GtkBuilder *builder;
  GtkWidget *chooser;
  GtkWidget *filtered_list;
  GtkWidget *filter_entry;
  GtkTreeViewColumn *visible_column;
  GtkTreeSelection *selection;
  GtkListStore *model;
  GtkTreeModelFilter *filtered_model;
  GtkTreeIter iter;

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder,
                             GNOMECC_UI_DIR "/gnome-region-panel-input-chooser.ui",
                             NULL);
  chooser = WID ("input_source_chooser");
  g_object_set_data_full (G_OBJECT (chooser), "builder", builder, g_object_unref);

  filtered_list = WID ("filtered_input_source_list");
  filter_entry = WID ("input_source_filter");

  g_object_set_data (G_OBJECT (chooser),
                     "filtered_input_source_list", filtered_list);
  visible_column =
    gtk_tree_view_column_new_with_attributes ("Input Sources",
                                              gtk_cell_renderer_text_new (),
                                              "text", NAME_COLUMN,
                                              NULL);

  gtk_window_set_transient_for (GTK_WINDOW (chooser), main_window);

  gtk_tree_view_append_column (GTK_TREE_VIEW (filtered_list),
                               visible_column);
  /* We handle searching ourselves, thank you. */
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (filtered_list), FALSE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (filtered_list), -1);

  g_signal_connect_swapped (G_OBJECT (filter_entry), "activate",
                            G_CALLBACK (entry_activated), builder);
  g_signal_connect_swapped (G_OBJECT (filter_entry), "notify::text",
                            G_CALLBACK (filter_changed), builder);

  g_signal_connect (G_OBJECT (filter_entry), "icon-release",
                    G_CALLBACK (filter_clear), NULL);

  filtered_model = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "filtered_input_source_model"));
  model = GTK_LIST_STORE (gtk_builder_get_object (builder, "input_source_model"));

  populate_model (model, active_sources);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
                                        NAME_COLUMN, GTK_SORT_ASCENDING);

  gtk_tree_model_filter_set_visible_func (filtered_model,
                                          filter_func,
                                          NULL, NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (filtered_list));

  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (selection_changed), builder);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filtered_model), &iter))
    gtk_tree_selection_select_iter (selection, &iter);

  g_signal_connect (G_OBJECT (filtered_list), "row-activated",
                    G_CALLBACK (row_activated), builder);

  gtk_widget_grab_focus (filter_entry);

  gtk_widget_show (chooser);

  return chooser;
}

static gboolean
input_chooser_get_selected (GtkWidget     *dialog,
                            GtkTreeModel **model,
                            GtkTreeIter   *iter)
{
  GtkWidget *tv;
  GtkTreeSelection *selection;

  tv = g_object_get_data (G_OBJECT (dialog), "filtered_input_source_list");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));

  return gtk_tree_selection_get_selected (selection, model, iter);
}
