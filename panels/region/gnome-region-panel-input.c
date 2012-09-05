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

#include "gnome-input-source.h"
#include "gnome-input-source-multiprovider.h"
#include "gnome-region-panel-input.h"

#define WID(s) GTK_WIDGET(gtk_builder_get_object (builder, s))

enum {
  NAME_COLUMN,
  SOURCE_COLUMN,
  ID_COLUMN,
  N_COLUMNS
};

static GnomeInputSourceProvider *provider = NULL;

static GtkWidget *input_chooser_new          (GtkWindow     *main_window,
                                              GtkListStore  *active_sources);
static gboolean   input_chooser_get_selected (GtkWidget     *chooser,
                                              GtkTreeModel **model,
                                              GtkTreeIter   *iter);
static GtkTreeModel *tree_view_get_actual_model (GtkTreeView *tv);

static void
populate_model (GtkListStore *store,
                GtkListStore *active_sources_store)
{
  GtkTreeIter iter;
  GList *inactive_sources, *tmp;
  GnomeInputSource *source;
  inactive_sources = gnome_input_source_provider_get_inactive_sources (provider);
  for (tmp = inactive_sources; tmp; tmp = tmp->next)
    {
      source = GNOME_INPUT_SOURCE (tmp->data);
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          NAME_COLUMN, gnome_input_source_get_name (source),
                          SOURCE_COLUMN, source,
                          -1);
    }
  g_list_free_full (inactive_sources, g_object_unref);
}

static void
populate_with_active_sources (GtkListStore *store)
{
  GtkTreeIter tree_iter;
  GList *sources, *tmp;
  GnomeInputSource *source;

  sources = gnome_input_source_provider_get_active_sources (provider);
  for (tmp = sources; tmp; tmp = tmp->next)
    {
      source = tmp->data;
      gtk_list_store_append (store, &tree_iter);
      gtk_list_store_set (store, &tree_iter,
                          NAME_COLUMN, gnome_input_source_get_name (source),
                          SOURCE_COLUMN, source,
                          -1);
    }
}

static void
update_configuration (GtkTreeModel *model)
{
  GtkTreeIter iter;
  GList *new_sources = NULL;
  GnomeInputSource *source;
  gtk_tree_model_get_iter_first (model, &iter);
  do
    {
      gtk_tree_model_get (model, &iter,
                          SOURCE_COLUMN, &source,
                          -1);
      new_sources = g_list_prepend (new_sources, source);
    }
  while (gtk_tree_model_iter_next (model, &iter));
  new_sources = g_list_reverse (new_sources);
  gnome_input_source_provider_set_active_sources (provider, new_sources);
  g_list_free_full (new_sources, g_object_unref);
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
  GnomeInputSource *source = NULL;

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
      gtk_tree_model_get (model, &iter, SOURCE_COLUMN, &source, -1);
    }
  else
    {
      index = -1;
    }

  settings_sensitive = (index >= 0 && source != NULL &&
                        gnome_input_source_has_preference (source));

  if (source)
    g_object_unref (source);

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

static GtkTreeModel *
tree_view_get_actual_model (GtkTreeView *tv)
{
  GtkTreeModel *filtered_store;

  filtered_store = gtk_tree_view_get_model (tv);

  return gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (filtered_store));
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
          GtkListStore *child_model;
          GtkTreeIter child_iter, filter_iter;
          GnomeInputSource *source;
          gchar *name;

          gtk_tree_model_get (model, &iter,
                              NAME_COLUMN, &name,
                              SOURCE_COLUMN, &source,
                              -1);
          tv = GTK_TREE_VIEW (WID ("active_input_sources"));
          child_model = GTK_LIST_STORE (tree_view_get_actual_model (tv));

          gtk_list_store_append (child_model, &child_iter);

          gtk_list_store_set (child_model, &child_iter,
                              NAME_COLUMN, gnome_input_source_get_name (source),
                              SOURCE_COLUMN, source,
                              -1);
          gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (gtk_tree_view_get_model (tv)),
                                                            &filter_iter,
                                                            &child_iter);
          gtk_tree_selection_select_iter (gtk_tree_view_get_selection (tv), &filter_iter);

          update_button_sensitivity (builder);
          update_configuration (GTK_TREE_MODEL (child_model));
          g_object_unref (source);
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
  active_sources = GTK_LIST_STORE (tree_view_get_actual_model (GTK_TREE_VIEW (treeview)));

  chooser = input_chooser_new (GTK_WINDOW (toplevel), active_sources);
  g_signal_connect (chooser, "response",
                    G_CALLBACK (chooser_response), builder);
}

static void
remove_selected_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter;
  GtkTreeIter child_iter;
  GtkTreePath *path;

  g_debug ("remove selected input source");

  if (get_selected_iter (builder, &model, &iter) == FALSE)
    return;

  path = gtk_tree_model_get_path (model, &iter);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_list_store_remove (GTK_LIST_STORE (child_model), &child_iter);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    gtk_tree_path_prev (path);

  set_selected_path (builder, path);

  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
move_selected_input_up (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter, prev;
  GtkTreeIter child_iter, child_prev;
  GtkTreePath *path;

  g_debug ("move selected input source up");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  prev = iter;
  if (!gtk_tree_model_iter_previous (model, &prev))
    return;

  path = gtk_tree_model_get_path (model, &prev);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_prev,
                                                    &prev);
  gtk_list_store_swap (GTK_LIST_STORE (child_model), &child_iter, &child_prev);

  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
move_selected_input_down (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter, next;
  GtkTreeIter child_iter, child_next;
  GtkTreePath *path;

  g_debug ("move selected input source down");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  next = iter;
  if (!gtk_tree_model_iter_next (model, &next))
    return;

  path = gtk_tree_model_get_path (model, &next);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_next,
                                                    &next);
  gtk_list_store_swap (GTK_LIST_STORE (child_model), &child_iter, &child_next);

  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
show_selected_layout (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GnomeInputSource *source;
  gchar *kbd_viewer_args;
  const gchar *layout;
  const gchar *variant;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      SOURCE_COLUMN, &source,
                      -1);

  layout = gnome_input_source_get_layout (source);
  variant = gnome_input_source_get_layout_variant (source);
  if (layout && layout[0])
    {
      if (variant && variant[0])
        kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l \"%s\t%s\"",
                                           layout, variant);
      else
        kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l \"%s\"",
                                           layout);
      g_spawn_command_line_async (kbd_viewer_args, NULL);
      g_free (kbd_viewer_args);
    }
  else
    {
      g_warning ("Unknown layout");
    }
}

static void
show_selected_settings (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GnomeInputSource *source = NULL;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, SOURCE_COLUMN, &source, -1);

  if (!source)
    return;

  if (gnome_input_source_has_preference (source))
    gnome_input_source_show_preference (source);

  g_object_unref (source);
}

static gboolean
go_to_shortcuts (GtkLinkButton *button,
                 CcRegionPanel *panel)
{
  gnome_input_source_provider_show_settings_dialog (provider,
                                                    GNOME_INPUT_SOURCE_PROVIDER_SETTING_NEXT_SOURCE);
  return TRUE;
}

static void
input_sources_changed (GnomeInputSourceProvider *provider,
                       GtkBuilder *builder)
{
  GtkWidget *treeview;
  GtkTreeModel *store;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeModel *model;

  treeview = WID("active_input_sources");
  store = tree_view_get_actual_model (GTK_TREE_VIEW (treeview));

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
  if (value == NULL || *value == '\0')
    {
      gtk_label_set_text (GTK_LABEL (widget), "\342\200\224");
      return;
    }
  gtk_label_set_text (GTK_LABEL (widget), value);
}

static void
update_shortcuts (GtkBuilder *builder)
{
  char *previous, *next;
  next = gnome_input_source_provider_get_next_source_key (provider);
  previous = gnome_input_source_provider_get_previous_source_key (provider);

  update_shortcut_label (WID ("prev-source-shortcut-label"), previous);
  update_shortcut_label (WID ("next-source-shortcut-label"), next);

  g_free (previous);
  g_free (next);
}

static gboolean
active_sources_visible_func (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gpointer      data)
{
  gchar *display_name;

  gtk_tree_model_get (model, iter, NAME_COLUMN, &display_name, -1);

  if (!display_name)
    return FALSE;

  g_free (display_name);

  return TRUE;
}

void
setup_input_tabs (GtkBuilder    *builder,
                  CcRegionPanel *panel)
{
  GtkWidget *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkListStore *store;
  GtkTreeModel *filtered_store;
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
                              GNOME_TYPE_INPUT_SOURCE,
                              G_TYPE_STRING);

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));

  provider = GNOME_INPUT_SOURCE_PROVIDER (gnome_input_source_multiprovider_new ());
  g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) g_object_unref, provider);

  populate_with_active_sources (store);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect_swapped (selection, "changed",
                            G_CALLBACK (update_button_sensitivity), builder);

  /* Some input source types might have their info loaded
   * asynchronously. In that case we don't want to show them
   * immediately so we use a filter model on top of the real model
   * which mirrors the GSettings key. */
  filtered_store = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filtered_store),
                                          active_sources_visible_func,
                                          NULL,
                                          NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), filtered_store);

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

  g_signal_connect (G_OBJECT (provider),
                    "sources-changed",
                    G_CALLBACK (input_sources_changed),
                    builder);
  g_signal_connect_swapped (G_OBJECT (provider),
                            "settings-changed",
                            G_CALLBACK (update_shortcuts),
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
