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

#include "gnome-input-source-multiprovider.h"
#include <gtk/gtk.h>
#include <config.h>
#include "gnome-input-source-xkb.h"

#ifdef HAVE_IBUS
#include "gnome-input-source-ibus.h"
#endif  /* HAVE_IBUS */

#ifdef HAVE_FCITX
#include "gnome-input-source-fcitx.h"
#endif  /* HAVE_FCITX */

#define GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_INPUT_SOURCE_MULTIPROVIDER, GnomeInputSourceMultiproviderPrivate))

typedef GnomeInputSourceProvider *(*ProviderNewFunc) (void);
typedef struct _GnomeInputSourceMultiproviderPrivate GnomeInputSourceMultiproviderPrivate;
struct _GnomeInputSourceMultiproviderPrivate {
  gboolean init_done;
  GnomeInputSourceProvider *slave;
  GHashTable *activated_provider_table;
  GList *providers;
  GnomeInputSourceProvider *xkb;
  GtkIMMulticontext *multicontext;
  GtkSettings *settings;
};

enum {
  SIGNAL_PROVIDER_CHANGED = 0,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GnomeInputSourceMultiprovider, gnome_input_source_multiprovider, GNOME_TYPE_INPUT_SOURCE_PROVIDER)

static gboolean multiprovider_get_activated (GnomeInputSourceProvider *provider);
static GList *multiprovider_get_active_sources (GnomeInputSourceProvider *provider);
static GList *multiprovider_get_inactive_sources (GnomeInputSourceProvider *provider);
static void multiprovider_set_active_sources (GnomeInputSourceProvider *provider,
                                    GList *sources);
static gchar *multiprovider_get_setting (GnomeInputSourceProvider *provider,
                                         guint setting_id);
static void multiprovider_show_settings_dialog (GnomeInputSourceProvider *provider,
                                                guint setting_id);
static void multiprovider_finalize (GObject *object);

static void multiprovider_add_provider (GnomeInputSourceMultiprovider *multiprovider,
                                        ProviderNewFunc new_func);
static void provider_activated_cb (GnomeInputSourceProvider *provider,
                                   GnomeInputSourceMultiprovider *multiprovider);
static void provider_deactivated_cb (GnomeInputSourceProvider *provider,
                                     GnomeInputSourceMultiprovider *multiprovider);
static void multiprovider_set_slave (GnomeInputSourceMultiprovider *multiprovider,
                                     GnomeInputSourceProvider *slave);
static void multiprovider_choose_slave (GnomeInputSourceMultiprovider *multiprovider);
static void im_module_setting_changed (GtkSettings *settings,
                                       GParamSpec *pspec,
                                       GnomeInputSourceMultiprovider *multiprovider);

static void slave_sources_changed_cb (GnomeInputSourceMultiprovider *multiprovider,
                                      GnomeInputSourceProvider *slave);
static void slave_immodule_name_changed_cb (GnomeInputSourceMultiprovider *multiprovider,
                                            GParamSpec *pspec,
                                            GnomeInputSourceProvider *slave);
static void slave_settings_changed_cb (GnomeInputSourceMultiprovider *multiprovider,
                                       GnomeInputSourceProvider *slave);

static void
gnome_input_source_multiprovider_class_init (GnomeInputSourceMultiproviderClass *klass)
{
  GObjectClass *object_class;
  GnomeInputSourceProviderClass *provider_class;
  object_class = G_OBJECT_CLASS (klass);
  provider_class = GNOME_INPUT_SOURCE_PROVIDER_CLASS (klass);
  object_class->finalize = multiprovider_finalize;
  provider_class->get_activated = multiprovider_get_activated;
  provider_class->get_active_sources = multiprovider_get_active_sources;
  provider_class->get_inactive_sources = multiprovider_get_inactive_sources;
  provider_class->set_active_sources = multiprovider_set_active_sources;
  provider_class->get_setting = multiprovider_get_setting;
  provider_class->show_settings_dialog = multiprovider_show_settings_dialog;

  signals[SIGNAL_PROVIDER_CHANGED] =
    g_signal_new ("provider-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_type_class_add_private (klass,
                            sizeof (GnomeInputSourceMultiproviderPrivate));
}

static void
gnome_input_source_multiprovider_init (GnomeInputSourceMultiprovider *multiprovider)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (multiprovider);
  priv->init_done = FALSE;
  priv->xkb = GNOME_INPUT_SOURCE_PROVIDER (gnome_input_source_xkb_new ());
  priv->activated_provider_table = g_hash_table_new (g_direct_hash, g_direct_equal);
#ifdef HAVE_IBUS
  multiprovider_add_provider (multiprovider, (ProviderNewFunc) gnome_input_source_ibus_new);
#endif  /* HAVE_IBUS */
#ifdef HAVE_FCITX
  multiprovider_add_provider (multiprovider, (ProviderNewFunc) gnome_input_source_fcitx_new);
#endif  /* HAVE_FCITX */
  multiprovider_choose_slave (multiprovider);
  priv->multicontext = GTK_IM_MULTICONTEXT (gtk_im_multicontext_new ());
  priv->settings = gtk_settings_get_default ();
  g_signal_connect (priv->settings,
                    "notify::gtk-im-module",
                    G_CALLBACK (im_module_setting_changed),
                    multiprovider);
  priv->init_done = TRUE;
}

static void
multiprovider_finalize (GObject *object)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (object);
  multiprovider_set_slave (GNOME_INPUT_SOURCE_MULTIPROVIDER (object),
                           NULL);
  g_clear_object (&priv->xkb);
  g_clear_object (&priv->slave);
  g_clear_pointer (&priv->activated_provider_table, g_hash_table_destroy);
  g_list_free_full (priv->providers, g_object_unref);
  priv->providers = NULL;
  g_clear_object (&priv->multicontext);
  g_signal_handlers_disconnect_by_func (priv->settings,
                                        G_CALLBACK (im_module_setting_changed),
                                        object);
  priv->settings = NULL;
  G_OBJECT_CLASS (gnome_input_source_multiprovider_parent_class)->finalize (object);
}

static void
multiprovider_add_provider (GnomeInputSourceMultiprovider *multiprovider,
                            ProviderNewFunc new_func)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (multiprovider);
  GnomeInputSourceProvider *provider = new_func();
  g_signal_connect (provider,
                    "activated",
                    G_CALLBACK (provider_activated_cb),
                    multiprovider);
  priv->providers = g_list_prepend (priv->providers, provider);
  if (gnome_input_source_provider_get_activated (provider))
      provider_activated_cb (provider, multiprovider);
}

static void
provider_activated_cb (GnomeInputSourceProvider *provider,
                       GnomeInputSourceMultiprovider *multiprovider)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (multiprovider);
  if (g_hash_table_lookup (priv->activated_provider_table, provider))
    {
      g_warning ("Provider was activated twice.");
      return;
    }
  g_hash_table_add (priv->activated_provider_table, provider);
  g_signal_connect (provider,
                    "deactivated",
                    G_CALLBACK (provider_deactivated_cb),
                    multiprovider);
  if (priv->init_done)
    multiprovider_choose_slave (multiprovider);
}

static void
provider_deactivated_cb (GnomeInputSourceProvider *provider,
                         GnomeInputSourceMultiprovider *multiprovider)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (multiprovider);
  if (!g_hash_table_lookup (priv->activated_provider_table, provider))
    {
      g_warning ("Provider was not activated.");
      return;
    }
  g_hash_table_remove (priv->activated_provider_table, provider);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (provider_deactivated_cb),
                                        multiprovider);
  if (priv->slave == provider)
    {
      multiprovider_choose_slave (multiprovider);
    }
}

static void
multiprovider_choose_slave (GnomeInputSourceMultiprovider *multiprovider)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  GnomeInputSourceProvider *slave = NULL, *provider;
  GList *activated_providers, *tmp;
  const gchar *global_immodule;
  const gchar * const * supported_immodules;

  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (multiprovider);
  activated_providers = g_hash_table_get_keys (priv->activated_provider_table);
  if (activated_providers)
    {
      if (activated_providers->next == NULL)
        {
          /* Choose the only activated provider as slave */
          slave = activated_providers->data;
        }
      else
        {
          gtk_im_context_set_client_window (GTK_IM_CONTEXT (priv->multicontext),
                                            gdk_get_default_root_window ());
          global_immodule = gtk_im_multicontext_get_context_id (priv->multicontext);
          for (tmp = activated_providers; tmp; tmp = tmp->next)
            {
              provider = tmp->data;
              supported_immodules = gnome_input_source_get_supported_immodules (provider);
              for (; supported_immodules && *supported_immodules; supported_immodules++)
                {
                  if (g_strcmp0 (*supported_immodules,
                                 global_immodule) == 0)
                    {
                      /* Choose the activated provider that provides the immodule
                         user is using */
                      slave = provider;
                      break;
                    }
                }
            }
        }
      g_list_free (activated_providers);
    }
  if (!slave)
    /* No activated provider chosen, fallback to xkb */
    slave = priv->xkb;
  if (slave != priv->slave)
    multiprovider_set_slave (multiprovider, slave);
}

static gboolean
im_module_setting_changed_timeout (GnomeInputSourceMultiprovider *multiprovider)
{
  multiprovider_choose_slave (multiprovider);
  return FALSE;
}

static void
im_module_setting_changed (GtkSettings *settings,
                           GParamSpec *pspec,
                           GnomeInputSourceMultiprovider *multiprovider)
{
  /* GtkIMMulticontext may not have receive this signal yet. Wait for a while. */
  g_timeout_add_seconds (0,
                         (GSourceFunc) im_module_setting_changed_timeout,
                         multiprovider);
}

static gboolean
multiprovider_get_activated (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (provider);
  return gnome_input_source_provider_get_activated (priv->slave);
}

static GList *
multiprovider_get_active_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (provider);
  return gnome_input_source_provider_get_active_sources (priv->slave);
}

static GList
*multiprovider_get_inactive_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (provider);
  return gnome_input_source_provider_get_inactive_sources (priv->slave);
}

static void
multiprovider_set_active_sources (GnomeInputSourceProvider *provider,
                                  GList *sources)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (provider);
  gnome_input_source_provider_set_active_sources (priv->slave, sources);
}

static gchar *
multiprovider_get_setting (GnomeInputSourceProvider *provider,
                  guint setting_id)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (provider);
  return gnome_input_source_provider_get_setting (priv->slave, setting_id);
}

static void
multiprovider_show_settings_dialog (GnomeInputSourceProvider *provider,
                           guint setting_id)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (provider);
  gnome_input_source_provider_show_settings_dialog (priv->slave, setting_id);
}

static void
multiprovider_set_slave (GnomeInputSourceMultiprovider *multiprovider,
                         GnomeInputSourceProvider *slave)
{
  GnomeInputSourceMultiproviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_MULTIPROVIDER_PRIVATE (multiprovider);
  if (priv->slave)
    {
      g_signal_handlers_disconnect_by_func (priv->slave,
                                            slave_sources_changed_cb,
                                            multiprovider);
      g_signal_handlers_disconnect_by_func (priv->slave,
                                            slave_immodule_name_changed_cb,
                                            multiprovider);
      g_signal_handlers_disconnect_by_func (priv->slave,
                                            slave_settings_changed_cb,
                                            multiprovider);
    }
  priv->slave = slave;
  if (slave)
    {
      g_signal_connect_swapped (slave,
                                "notify::immodule-name",
                                G_CALLBACK (slave_immodule_name_changed_cb),
                                multiprovider);
      g_signal_connect_swapped (slave,
                                "sources-changed",
                                G_CALLBACK (slave_sources_changed_cb),
                                multiprovider);
      g_signal_connect_swapped (slave,
                                "settings_changed",
                                G_CALLBACK (slave_settings_changed_cb),
                                multiprovider);
      slave_immodule_name_changed_cb (multiprovider,
                                      NULL,
                                      slave);

      g_signal_emit (multiprovider, signals[SIGNAL_PROVIDER_CHANGED], 0);
      slave_immodule_name_changed_cb (multiprovider, NULL, slave);
      slave_sources_changed_cb (multiprovider, slave);
      slave_settings_changed_cb (multiprovider, slave);
    }
}

static void
slave_sources_changed_cb (GnomeInputSourceMultiprovider *multiprovider,
                          GnomeInputSourceProvider *provider)
{
  g_signal_emit_by_name (multiprovider, "sources-changed");
}

static void
slave_immodule_name_changed_cb (GnomeInputSourceMultiprovider *multiprovider,
                                GParamSpec *pspec,
                                GnomeInputSourceProvider *slave)
{
  gnome_input_source_provider_set_immodule_name (GNOME_INPUT_SOURCE_PROVIDER (multiprovider),
                                                 gnome_input_source_provider_get_immodule_name (slave));
}

static void
slave_settings_changed_cb (GnomeInputSourceMultiprovider *multiprovider,
                           GnomeInputSourceProvider *slave)
{
  g_signal_emit_by_name (multiprovider, "settings-changed");
}

GnomeInputSourceMultiprovider *
gnome_input_source_multiprovider_new (void)
{
  return g_object_new (GNOME_TYPE_INPUT_SOURCE_MULTIPROVIDER, NULL);
}
