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

#include "gnome-input-source-ibus.h"
#include <ibus.h>
#include "gdm-languages.h"
#include "gnome-input-source-xkb.h"

#define INPUT_SOURCE_TYPE_IBUS "ibus"
#define IMMODULE_SIMPLE        "gtk-im-context-simple"
#define IMMODULE_IBUS          "ibus"

#define GNOME_INPUT_SOURCE_IBUS_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_INPUT_SOURCE_IBUS, GnomeInputSourceIBusPrivate))

G_DEFINE_TYPE (GnomeInputSourceIBus, gnome_input_source_ibus, GNOME_TYPE_INPUT_SOURCE_PROVIDER)

static const gchar *supported_ibus_engines[] = {
  "bopomofo",
  "pinyin",
  "chewing",
  "anthy",
  "hangul",
  NULL
};

typedef struct _GnomeInputSourceIBusPrivate GnomeInputSourceIBusPrivate;
struct _GnomeInputSourceIBusPrivate {
  GnomeInputSourceProvider *xkb;
  IBusBus *ibus;
  gboolean connected;
  GHashTable *ibus_engines;
  guint shell_name_watch_id;
};

static gboolean ibus_get_activated (GnomeInputSourceProvider *provider);
static GList *ibus_get_active_sources (GnomeInputSourceProvider *provider);
static GList *ibus_get_inactive_sources (GnomeInputSourceProvider *provider);
static void ibus_set_active_sources (GnomeInputSourceProvider *provider,
                                    GList *sources);
static gchar *ibus_get_setting (GnomeInputSourceProvider *provider,
                                guint setting_id);
static void ibus_show_settings_dialog (GnomeInputSourceProvider *provider,
                                       guint setting_id);
static void ibus_finalize (GObject *object);
static void ibus_sources_changed (GnomeInputSourceIBus *ibus,
                                  gpointer userdata);
static GnomeXkbSource *ibus_source_create (const gchar *provider,
                                           const gchar *id,
                                           gpointer userdata);
static void ibus_connected_cb (GnomeInputSourceIBus *ibus, gpointer userdata);
static void ibus_disconnected_cb (GnomeInputSourceIBus *ibus, gpointer userdata);
static void on_shell_appeared (GDBusConnection *connection,
                               const gchar     *name,
                               const gchar     *name_owner,
                               gpointer         data);
static void fetch_ibus_engines (GnomeInputSourceIBus *ibus);
static gchar *engine_get_display_name (IBusEngineDesc *engine_desc);

static void
gnome_input_source_ibus_class_init (GnomeInputSourceIBusClass *klass)
{
  GObjectClass *object_class;
  GnomeInputSourceProviderClass *provider_class;
  object_class = G_OBJECT_CLASS (klass);
  provider_class = GNOME_INPUT_SOURCE_PROVIDER_CLASS (klass);
  object_class->finalize = ibus_finalize;
  provider_class->get_activated = ibus_get_activated;
  provider_class->get_active_sources = ibus_get_active_sources;
  provider_class->get_inactive_sources = ibus_get_inactive_sources;
  provider_class->set_active_sources = ibus_set_active_sources;
  provider_class->get_setting = ibus_get_setting;
  provider_class->show_settings_dialog = ibus_show_settings_dialog;
  g_type_class_add_private (klass, sizeof (GnomeInputSourceIBusPrivate));
}

static void
gnome_input_source_ibus_init (GnomeInputSourceIBus *ibus)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (ibus);
  ibus_init ();
  priv->shell_name_watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                "org.gnome.Shell",
                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                on_shell_appeared,
                                                NULL,
                                                ibus,
                                                NULL);
  priv->xkb = GNOME_INPUT_SOURCE_PROVIDER (gnome_input_source_xkb_new ());
  gnome_input_source_xkb_add_source_handler (GNOME_INPUT_SOURCE_XKB (priv->xkb),
                                             INPUT_SOURCE_TYPE_IBUS,
                                             ibus_source_create,
                                             ibus);
  g_signal_connect_swapped (priv->xkb,
                            "sources-changed",
                            G_CALLBACK (ibus_sources_changed),
                            ibus);
}

static void
ibus_finalize (GObject *object)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (object);
  g_signal_handlers_disconnect_by_func (priv->xkb,
                                        G_CALLBACK (ibus_sources_changed),
                                        object);
  gnome_input_source_xkb_remove_source_handler (GNOME_INPUT_SOURCE_XKB (priv->xkb),
                                                INPUT_SOURCE_TYPE_IBUS);
  g_clear_object (&priv->xkb);
  if (priv->shell_name_watch_id > 0)
    {
      g_bus_unwatch_name (priv->shell_name_watch_id);
      priv->shell_name_watch_id = 0;
    }
  g_clear_pointer (&priv->ibus_engines, g_hash_table_destroy);
  g_signal_handlers_disconnect_by_func (priv->ibus, ibus_connected_cb, object);
  g_signal_handlers_disconnect_by_func (priv->ibus, ibus_disconnected_cb, object);
  g_clear_object (&priv->ibus);
  G_OBJECT_CLASS (gnome_input_source_ibus_parent_class)->finalize (object);
}

static void
ibus_connected_cb (GnomeInputSourceIBus *ibus, gpointer userdata)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (ibus);
  priv->connected = TRUE;
  gnome_input_source_provider_set_immodule_name (GNOME_INPUT_SOURCE_PROVIDER (ibus),
                                                 IMMODULE_IBUS);
  g_signal_emit_by_name (ibus, "activated");
  fetch_ibus_engines (ibus);
  g_signal_emit_by_name (ibus, "sources-changed");
}

static void
ibus_disconnected_cb (GnomeInputSourceIBus *ibus, gpointer userdata)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (ibus);
  priv->connected = FALSE;
  if (priv->ibus_engines)
    g_clear_pointer (&priv->ibus_engines, g_hash_table_destroy);
  g_signal_emit_by_name (ibus, "deactivated");
  gnome_input_source_provider_set_immodule_name (GNOME_INPUT_SOURCE_PROVIDER (ibus),
                                                 IMMODULE_SIMPLE);
  g_signal_emit_by_name (ibus, "sources-changed");
}

static gboolean
ibus_get_activated (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (provider);
  return priv->connected;
}

static GList *
ibus_get_active_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (provider);
  return gnome_input_source_provider_get_active_sources (priv->xkb);
}

static GList *
ibus_get_inactive_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceIBusPrivate *priv;
  GnomeXkbSource *source;
  GList *xkb_sources, *sources, *ibus_sources = NULL, *tmp;
  GHashTable *active_sources_table;
  GVariant *active_sources;
  GVariantIter iter;
  const gchar *provider_name;
  const gchar *id;

  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (provider);
  xkb_sources = gnome_input_source_provider_get_inactive_sources (priv->xkb);
  active_sources_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  active_sources = gnome_input_source_xkb_get_active_sources (GNOME_INPUT_SOURCE_XKB (priv->xkb));
  g_variant_iter_init (&iter, active_sources);
  while (g_variant_iter_next (&iter, "(&s&s)", &provider_name, &id))
    {
      if (g_str_equal (provider_name, INPUT_SOURCE_TYPE_IBUS))
        {
          g_hash_table_replace (active_sources_table, g_strdup (id), NULL);
        }
    }
  g_variant_unref (active_sources);

  if (priv->ibus_engines)
    {
      sources = g_hash_table_get_keys (priv->ibus_engines);
      for (tmp = sources; tmp; tmp = tmp->next)
        {
          if (g_hash_table_contains (active_sources_table, tmp->data))
            continue;
          source = ibus_source_create (INPUT_SOURCE_TYPE_IBUS,
                                       tmp->data,
                                       provider);
          if (source)
            ibus_sources = g_list_prepend (ibus_sources, source);
        }
      g_list_free (sources);
    }
  return g_list_concat (g_list_reverse (ibus_sources), xkb_sources);
}

static void
ibus_set_active_sources (GnomeInputSourceProvider *provider,
                         GList *sources)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (provider);
  gnome_input_source_provider_set_active_sources (priv->xkb,
                                                  sources);
}

static gchar *
ibus_get_setting (GnomeInputSourceProvider *provider,
                  guint setting_id)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (provider);
  return gnome_input_source_provider_get_setting (priv->xkb,
                                                  setting_id);
}

static void
ibus_show_settings_dialog (GnomeInputSourceProvider *provider,
                           guint setting_id)
{
  GnomeInputSourceIBusPrivate *priv;
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (provider);
  gnome_input_source_provider_show_settings_dialog (priv->xkb, setting_id);
}

static void
ibus_sources_changed (GnomeInputSourceIBus *ibus,
                      gpointer userdata)
{
  g_signal_emit_by_name (ibus, "sources-changed");
}

static GnomeXkbSource *
ibus_source_create (const gchar *provider,
                    const gchar *id,
                    gpointer userdata)
{
  GnomeInputSourceIBusPrivate *priv;
  gchar *display_name, *desktop_file_name;
  GnomeXkbSource *source;
  IBusEngineDesc *engine_desc;
  const gchar *layout;
  
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (userdata);
  if (!priv->ibus_engines)
    return NULL;
  engine_desc = g_hash_table_lookup (priv->ibus_engines, id);
  display_name = engine_get_display_name (engine_desc);
  desktop_file_name = g_strdup_printf ("ibus-setup-%s.desktop", id);
  layout = ibus_engine_desc_get_layout (engine_desc);
  source = gnome_xkb_source_new (provider, id, display_name, desktop_file_name, layout, NULL);
  g_free (display_name);
  g_free (desktop_file_name);
  return source;
}

static void
on_shell_appeared (GDBusConnection *connection,
                   const gchar     *name,
                   const gchar     *name_owner,
                   gpointer         data)
{
  GnomeInputSourceIBus *ibus;
  GnomeInputSourceIBusPrivate *priv;
  ibus = GNOME_INPUT_SOURCE_IBUS (data);
  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (data);

  if (!priv->ibus)
    {
      priv->ibus = ibus_bus_new_async ();
      if (ibus_bus_is_connected (priv->ibus))
        ibus_connected_cb (ibus, NULL);
      g_signal_connect_swapped (priv->ibus, "connected",
                                G_CALLBACK (ibus_connected_cb), ibus);
      g_signal_connect_swapped (priv->ibus, "disconnected",
                                G_CALLBACK (ibus_disconnected_cb), ibus);
    }
}

static void
fetch_ibus_engines (GnomeInputSourceIBus *ibus)
{
  GnomeInputSourceIBusPrivate *priv;
  IBusEngineDesc **engines, **iter;
  const gchar *name;

  priv = GNOME_INPUT_SOURCE_IBUS_PRIVATE (ibus);
  if (priv->ibus_engines)
    g_hash_table_destroy (priv->ibus_engines);
  priv->ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  engines = ibus_bus_get_engines_by_names (priv->ibus, supported_ibus_engines);
  for (iter = engines; *iter; ++iter)
    {
      name = ibus_engine_desc_get_name (*iter);
      g_hash_table_replace (priv->ibus_engines, (gpointer)name, *iter);
    }

  g_free (engines);

  g_signal_emit_by_name (ibus, "sources-changed");
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

GnomeInputSourceIBus *
gnome_input_source_ibus_new (void)
{
  const gchar *supported_immodules[] = {
    IMMODULE_IBUS,
    IMMODULE_SIMPLE,
    NULL,
  };
  return GNOME_INPUT_SOURCE_IBUS (g_object_new (GNOME_TYPE_INPUT_SOURCE_IBUS,
                                                "immodule-name", IMMODULE_SIMPLE,
                                                "supported-immodules", supported_immodules,
                                                NULL));
}
