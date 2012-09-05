/*
 * Copyright (C) 2012 Tiger Soldier <tigersoldi@gmail.com>
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
#include "gnome-input-source-xkb.h"
#include <gtk/gtk.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"

#define KEY_CURRENT_INPUT_SOURCE        "current"
#define KEY_INPUT_SOURCES               "sources"

#define INPUT_SOURCE_TYPE_XKB           "xkb"
#define IMMODULE_SIMPLE                 "gtk-im-context-simple"

#define GNOME_INPUT_SOURCE_XKB_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_INPUT_SOURCE_XKB, GnomeInputSourceXkbPrivate))
#define GNOME_XKB_SOURCE_PRIVATE(o)       (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_XKB_SOURCE, GnomeXkbSourcePrivate))

typedef struct _GnomeInputSourceXkbPrivate GnomeInputSourceXkbPrivate;
struct _GnomeInputSourceXkbPrivate {
  GHashTable *source_handlers;
  GSettings *settings;
  GnomeXkbInfo *xkb_info;
};

typedef struct _GnomeXkbSourcePrivate GnomeXkbSourcePrivate;
struct _GnomeXkbSourcePrivate {
  gchar *id;
  gchar *provider;
  gchar *desktop_file_name;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_PROVIDER,
  PROP_DESKTOP_FILE_NAME,
};

struct SourceHander {
  GnomeInputSourceXkbCreateFunc func;
  gpointer userdata;
};

G_DEFINE_TYPE (GnomeInputSourceXkb, gnome_input_source_xkb, GNOME_TYPE_INPUT_SOURCE_PROVIDER)
G_DEFINE_TYPE (GnomeXkbSource, gnome_xkb_source, GNOME_TYPE_INPUT_SOURCE)

static gboolean xkb_get_activated (GnomeInputSourceProvider *provider);
static GList *xkb_get_active_sources (GnomeInputSourceProvider *provider);
static GList *xkb_get_inactive_sources (GnomeInputSourceProvider *provider);
static void xkb_set_active_sources (GnomeInputSourceProvider *provider,
                                    GList *sources);
static gchar *xkb_get_setting (GnomeInputSourceProvider *provider,
                               guint setting_id);
static void xkb_show_settings_dialog (GnomeInputSourceProvider *provider,
                                      guint setting_id);
static void xkb_finalize (GObject *object);
static void xkb_settings_changed (GSettings  *settings,
                                  gchar      *key,
                                  GnomeInputSourceXkb *xkb);
static GnomeXkbSource *xkb_source_handler (const gchar *provider,
                                           const gchar *id,
                                           gpointer userdata);

static void xkb_source_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec);
static void xkb_source_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec);
static void xkb_source_finalize (GObject *object);

static void
gnome_input_source_xkb_class_init (GnomeInputSourceXkbClass *klass)
{
  GObjectClass *object_class;
  GnomeInputSourceProviderClass *provider_class;
  object_class = G_OBJECT_CLASS (klass);
  provider_class = GNOME_INPUT_SOURCE_PROVIDER_CLASS (klass);
  object_class->finalize = xkb_finalize;
  provider_class->get_activated = xkb_get_activated;
  provider_class->get_active_sources = xkb_get_active_sources;
  provider_class->get_inactive_sources = xkb_get_inactive_sources;
  provider_class->set_active_sources = xkb_set_active_sources;
  provider_class->get_setting = xkb_get_setting;
  provider_class->show_settings_dialog = xkb_show_settings_dialog;
  g_type_class_add_private (klass, sizeof (GnomeInputSourceXkbPrivate));
}

static void
gnome_input_source_xkb_init (GnomeInputSourceXkb *xkb)
{
  GnomeInputSourceXkbPrivate *priv;
  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (xkb);
  priv->settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
  g_settings_delay (priv->settings);
  priv->source_handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, g_free);
  priv->xkb_info = gnome_xkb_info_new ();
  gnome_input_source_xkb_add_source_handler (xkb,
                                             INPUT_SOURCE_TYPE_XKB,
                                             xkb_source_handler,
                                             xkb);
  g_signal_connect (G_OBJECT (priv->settings),
                    "changed::" KEY_INPUT_SOURCES,
                    G_CALLBACK (xkb_settings_changed),
                    xkb);
}

static void
xkb_finalize (GObject *object)
{
  GnomeInputSourceXkbPrivate *priv;
  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (object);
  g_signal_handlers_disconnect_by_func (G_OBJECT (priv->settings),
                                        G_CALLBACK (xkb_settings_changed),
                                        object);
  g_clear_pointer (&priv->source_handlers, g_hash_table_destroy);
  g_clear_object (&priv->settings);
  g_clear_object (&priv->xkb_info);
  G_OBJECT_CLASS (gnome_input_source_xkb_parent_class)->finalize (object);
}

GnomeInputSourceXkb *
gnome_input_source_xkb_new (void)
{
  const gchar *supported_immodules[] = { IMMODULE_SIMPLE, NULL };
  return GNOME_INPUT_SOURCE_XKB (g_object_new (GNOME_TYPE_INPUT_SOURCE_XKB,
                                               "immodule-name", IMMODULE_SIMPLE,
                                               "supported-immodules", supported_immodules,
                                               NULL));
}

static gboolean
xkb_get_activated (GnomeInputSourceProvider *provider)
{
  return TRUE;
}

static GList *
xkb_get_active_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceXkbPrivate *priv;
  struct SourceHander *handler;
  GVariant *sources;
  GVariantIter iter;
  const gchar *provider_name;
  const gchar *id;
  GList *active_sources;
  GnomeXkbSource *source;
  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (provider);
  sources = g_settings_get_value (priv->settings, KEY_INPUT_SOURCES);
  active_sources = NULL;

  g_variant_iter_init (&iter, sources);
  while (g_variant_iter_next (&iter, "(&s&s)", &provider_name, &id))
    {
      handler = g_hash_table_lookup (priv->source_handlers, provider_name);
      if (handler)
        {
          source = handler->func (provider_name, id, handler->userdata);
          if (source)
            active_sources = g_list_prepend (active_sources, source);
        }
    }
  g_variant_unref (sources);
  return g_list_reverse (active_sources);
}

GVariant *
gnome_input_source_xkb_get_active_sources (GnomeInputSourceXkb *xkb)
{
  GnomeInputSourceXkbPrivate *priv;
  g_return_val_if_fail (GNOME_IS_INPUT_SOURCE_XKB (xkb), NULL);

  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (xkb);
  return g_settings_get_value (priv->settings, KEY_INPUT_SOURCES);
}

static GList *
xkb_get_inactive_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceXkbPrivate *priv;
  GHashTable *active_sources_table;
  GVariant *active_sources;
  GVariantIter iter;
  const gchar *provider_name;
  const gchar *id;
  GList *all_sources, *tmp, *inactive_sources = NULL;
  GnomeXkbSource *source;

  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (provider);
  active_sources_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  active_sources = gnome_input_source_xkb_get_active_sources (GNOME_INPUT_SOURCE_XKB (provider));
  g_variant_iter_init (&iter, active_sources);
  while (g_variant_iter_next (&iter, "(&s&s)", &provider_name, &id))
    {
      if (g_str_equal (provider_name, INPUT_SOURCE_TYPE_XKB))
        {
          g_hash_table_replace (active_sources_table, g_strdup (id), NULL);
        }
    }
  g_variant_unref (active_sources);

  all_sources = gnome_xkb_info_get_all_layouts (priv->xkb_info);
  for (tmp = all_sources; tmp; tmp = tmp->next)
    {
      if (g_hash_table_contains (active_sources_table, tmp->data))
        continue;
      source = xkb_source_handler (INPUT_SOURCE_TYPE_XKB,
                                   tmp->data,
                                   provider);
      if (source)
        inactive_sources = g_list_prepend (inactive_sources, source);
    }
  g_list_free (all_sources);
  g_hash_table_destroy (active_sources_table);
  return g_list_reverse (inactive_sources);
}

static void
xkb_set_active_sources (GnomeInputSourceProvider *provider,
                        GList *new_sources)
{
  GnomeInputSourceXkbPrivate *priv;
  GList *tmp;
  GVariantBuilder builder;
  GVariant *old_sources;
  const gchar *old_current_type;
  const gchar *old_current_id;
  const gchar *id;
  const gchar *type;
  guint old_current_index;
  guint index;
  GnomeXkbSource *source;

  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (provider);

  old_sources = g_settings_get_value (priv->settings, KEY_INPUT_SOURCES);
  old_current_index = g_settings_get_uint (priv->settings, KEY_CURRENT_INPUT_SOURCE);
  g_variant_get_child (old_sources,
                       old_current_index,
                       "(&s&s)",
                       &old_current_type,
                       &old_current_id);
  if (g_variant_n_children (old_sources) < 1)
    {
      g_warning ("No input source configured, resetting");
      g_settings_reset (priv->settings, KEY_INPUT_SOURCES);
      goto exit;
    }
  if (old_current_index >= g_variant_n_children (old_sources))
    {
      g_settings_set_uint (priv->settings,
                           KEY_CURRENT_INPUT_SOURCE,
                           g_variant_n_children (old_sources) - 1);
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  index = 0;
  for (tmp = new_sources; tmp; tmp = tmp->next)
    {
      source = tmp->data;
      if (!GNOME_IS_XKB_SOURCE (source))
        {
          g_warning ("Ignore sources from other providers");
          continue;
        }
      type = gnome_xkb_source_get_provider (source);
      id = gnome_xkb_source_get_id (source);
      if (index != old_current_index &&
          g_str_equal (type, old_current_type) &&
          g_str_equal (id, old_current_id))
        {
          g_settings_set_uint (priv->settings, KEY_CURRENT_INPUT_SOURCE, index);
        }
      g_variant_builder_add (&builder, "(ss)", type, id);
      index += 1;
    }

  g_settings_set_value (priv->settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));

 exit:
  g_settings_apply (priv->settings);
  g_variant_unref (old_sources);
}

static gchar *
xkb_get_setting (GnomeInputSourceProvider *provider,
                 guint setting_id)
{
  const gchar *key = NULL;
  GSettings *settings;
  gchar *value;
  gchar *shortcut = NULL;
  guint accel_key, *keycode;
  GdkModifierType mods;
  GdkDisplay *display;

  switch (setting_id)
    {
    case GNOME_INPUT_SOURCE_PROVIDER_SETTING_NEXT_SOURCE:
      key = "switch-input-source";
      break;
    case GNOME_INPUT_SOURCE_PROVIDER_SETTING_PREVIOUS_SOURCE:
      key = "switch-input-source-backward";
      break;
    default:
      g_warning ("Unknown setting ID to xkb: %u", setting_id);
      return NULL;
    }
  settings = g_settings_new ("org.gnome.settings-daemon.plugins.media-keys");
  value = g_settings_get_string (settings, key);
  g_object_unref (settings);

  if (!value || !value[0])
    return value;

  gtk_accelerator_parse_with_keycode (value, &accel_key, &keycode, &mods);
  if (accel_key == 0 && keycode == NULL && mods == 0)
    {
      g_warning ("Failed to parse keyboard shortcut: '%s'", value);
      goto exit;
    }

  display = gdk_display_get_default ();
  shortcut = gtk_accelerator_get_label_with_keycode (display,
                                                     accel_key,
                                                     *keycode,
                                                     mods);
  exit:
  g_free (keycode);
  g_free (value);
  return shortcut;
}

static void
xkb_show_settings_dialog (GnomeInputSourceProvider *provider,
                          guint setting_id)
{
  g_spawn_command_line_async ("gnome-control-center keyboard shortcut", NULL);
}

void
gnome_input_source_xkb_add_source_handler (GnomeInputSourceXkb *xkb,
                                           const gchar *provider,
                                           GnomeInputSourceXkbCreateFunc create_func,
                                           gpointer userdata)
{
  g_return_if_fail (GNOME_IS_INPUT_SOURCE_XKB (xkb));
  g_return_if_fail (provider != NULL);
  g_return_if_fail (create_func != NULL);
  GnomeInputSourceXkbPrivate *priv;
  struct SourceHander *handler;
  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (xkb);
  handler = g_new (struct SourceHander, 1);
  handler->func = create_func;
  handler->userdata = userdata;
  g_hash_table_replace (priv->source_handlers, g_strdup (provider), handler);
}

void
gnome_input_source_xkb_remove_source_handler (GnomeInputSourceXkb *xkb,
                                              const gchar *provider)
{
  g_return_if_fail (GNOME_IS_INPUT_SOURCE_XKB (xkb));
  g_return_if_fail (provider != NULL);
  GnomeInputSourceXkbPrivate *priv;
  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (xkb);
  g_hash_table_remove (priv->source_handlers, provider);
}

static GnomeXkbSource *
xkb_source_handler (const gchar *provider,
                    const gchar *id,
                    gpointer userdata)
{
  GnomeInputSourceXkbPrivate *priv;
  const gchar *name;
  const gchar *xkb_layout, *xkb_variant;
  GnomeXkbSource *source;
  priv = GNOME_INPUT_SOURCE_XKB_PRIVATE (userdata);
  gnome_xkb_info_get_layout_info (priv->xkb_info, id, &name, NULL, &xkb_layout, &xkb_variant);
  source = gnome_xkb_source_new (provider, id, name, NULL, xkb_layout, xkb_variant);
  return source;
}

static void
xkb_settings_changed (GSettings *settings,
                      gchar *key,
                      GnomeInputSourceXkb *xkb)
{
  g_signal_emit_by_name (xkb, "sources-changed");
}

static void
gnome_xkb_source_class_init (GnomeXkbSourceClass *klass)
{
  GObjectClass *gobject_class;
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xkb_source_finalize;
  gobject_class->set_property = xkb_source_set_property;
  gobject_class->get_property = xkb_source_get_property;
  g_object_class_install_property (gobject_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "ID",
                                                        "Unique ID of the input source",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class,
                                   PROP_PROVIDER,
                                   g_param_spec_string ("provider",
                                                        "Provier",
                                                        "Name of provider that handles the input source",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class,
                                   PROP_DESKTOP_FILE_NAME,
                                   g_param_spec_string ("desktop-file-name",
                                                        "Desktop file name",
                                                        "Name of the desktop file to the preference app of the input source",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_type_class_add_private (klass, sizeof (GnomeXkbSourcePrivate));
}

static void
gnome_xkb_source_init (GnomeXkbSource *source)
{
}

static void
xkb_source_finalize (GObject *object)
{
  GnomeXkbSourcePrivate *priv;
  priv = GNOME_XKB_SOURCE_PRIVATE (object);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->provider, g_free);
  g_clear_pointer (&priv->desktop_file_name, g_free);
  G_OBJECT_CLASS (gnome_xkb_source_parent_class)->finalize (object);
}

static void
xkb_source_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  GnomeXkbSourcePrivate *priv;
  priv = GNOME_XKB_SOURCE_PRIVATE (object);
  switch (property_id) {
  case PROP_ID:
    g_free (priv->id);
    priv->id = g_value_dup_string (value);
    break;
  case PROP_PROVIDER:
    g_free (priv->provider);
    priv->provider = g_value_dup_string (value);
    break;
  case PROP_DESKTOP_FILE_NAME:
    g_free (priv->desktop_file_name);
    priv->desktop_file_name = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
xkb_source_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  GnomeXkbSourcePrivate *priv;
  priv = GNOME_XKB_SOURCE_PRIVATE (object);
  switch (property_id) {
  case PROP_ID:
    g_value_set_string (value, priv->id);
    break;
  case PROP_PROVIDER:
    g_value_set_string (value, priv->provider);
    break;
  case PROP_DESKTOP_FILE_NAME:
    g_value_set_string (value, priv->desktop_file_name);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

const gchar *
gnome_xkb_source_get_provider (GnomeXkbSource *source)
{
  g_return_val_if_fail (GNOME_IS_XKB_SOURCE (source), NULL);
  GnomeXkbSourcePrivate *priv;
  priv = GNOME_XKB_SOURCE_PRIVATE (source);
  return priv->provider;
}

const gchar *
gnome_xkb_source_get_id (GnomeXkbSource *source)
{
  g_return_val_if_fail (GNOME_IS_XKB_SOURCE (source), NULL);
  GnomeXkbSourcePrivate *priv;
  priv = GNOME_XKB_SOURCE_PRIVATE (source);
  return priv->id;
}

const gchar *
gnome_xkb_source_get_desktop_file_name (GnomeXkbSource *source)
{
  g_return_val_if_fail (GNOME_IS_XKB_SOURCE (source), NULL);
  GnomeXkbSourcePrivate *priv;
  priv = GNOME_XKB_SOURCE_PRIVATE (source);
  return priv->desktop_file_name;
}

GnomeXkbSource *
gnome_xkb_source_new (const gchar *provider,
                      const gchar *id,
                      const gchar *name,
                      const gchar *desktop_file_name,
                      const gchar *layout,
                      const gchar *layout_variant)
{
  g_return_val_if_fail (provider != NULL, NULL);
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  GObject *object;
  object = g_object_new (GNOME_TYPE_XKB_SOURCE,
                         "name", name,
                         "id", id,
                         "provider", provider,
                         "desktop-file-name", desktop_file_name,
                         "layout", layout,
                         "layout-variant", layout_variant,
                         NULL);
  return GNOME_XKB_SOURCE (object);
}
