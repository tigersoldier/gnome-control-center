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

#include "gnome-input-source-provider.h"

enum {
  SIGNAL_ACTIVATED,
  SIGNAL_DEACTIVATED,
  SIGNAL_SOURCES_CHANGED,
  SIGNAL_SETTINGS_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_IMMODULE_NAME,
  PROP_SUPPORTED_IMMODULES,
};

static guint provider_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE (GnomeInputSourceProvider, gnome_input_source_provider, G_TYPE_OBJECT)

#define GNOME_INPUT_SOURCE_PROVIDER_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_INPUT_SOURCE_PROVIDER, GnomeInputSourceProviderPrivate))

typedef struct _GnomeInputSourceProviderPrivate GnomeInputSourceProviderPrivate;
struct _GnomeInputSourceProviderPrivate {
  gchar *immodule_name;
  gchar **supported_immodules;
};

static void gnome_input_source_provider_set_property (GObject *object,
                                                      guint property_id,
                                                      const GValue *value,
                                                      GParamSpec *pspec);
static void gnome_input_source_provider_get_property (GObject *object,
                                                      guint property_id,
                                                      GValue *value,
                                                      GParamSpec *pspec);
static void gnome_input_source_provider_finalize (GObject *object);

static void
gnome_input_source_provider_class_init (GnomeInputSourceProviderClass *klass)
{
  GObjectClass *gobject_class;
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = gnome_input_source_provider_set_property;
  gobject_class->get_property = gnome_input_source_provider_get_property;
  gobject_class->finalize = gnome_input_source_provider_finalize;

  provider_signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GnomeInputSourceProviderClass, activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  provider_signals[SIGNAL_DEACTIVATED] =
    g_signal_new ("deactivated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GnomeInputSourceProviderClass, deactivated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  provider_signals[SIGNAL_SOURCES_CHANGED] =
    g_signal_new ("sources-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GnomeInputSourceProviderClass, sources_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  provider_signals[SIGNAL_SETTINGS_CHANGED] =
    g_signal_new ("settings-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GnomeInputSourceProviderClass, settings_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_object_class_install_property (gobject_class,
                                   PROP_IMMODULE_NAME,
                                   g_param_spec_string ("immodule-name",
                                                        "IM Module Name",
                                                        "Name of the IM Module",
                                                        "",
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (gobject_class,
                                   PROP_SUPPORTED_IMMODULES,
                                   g_param_spec_boxed ("supported-immodules",
                                                       "Supported IM Modules",
                                                       "A NUL-terminated string list of name of supported IM Modules",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT));
  g_type_class_add_private (klass, sizeof (GnomeInputSourceProviderPrivate));
}

static void
gnome_input_source_provider_init (GnomeInputSourceProvider *provider)
{
}

static void
gnome_input_source_provider_finalize (GObject *object)
{
  GnomeInputSourceProviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_PROVIDER_PRIVATE (object);
  g_clear_pointer (&priv->immodule_name, g_free);
  g_clear_pointer (&priv->supported_immodules, g_strfreev);
  G_OBJECT_CLASS (gnome_input_source_provider_parent_class)->finalize (object);
}

static void
gnome_input_source_provider_set_property (GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
  GnomeInputSourceProviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_PROVIDER_PRIVATE (object);
  switch (property_id)
    {
    case PROP_IMMODULE_NAME:
      g_free (priv->immodule_name);
      priv->immodule_name = g_value_dup_string (value);
      break;
    case PROP_SUPPORTED_IMMODULES:
      if (priv->supported_immodules)
        g_strfreev (priv->supported_immodules);
      priv->supported_immodules = g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gnome_input_source_provider_get_property (GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
  GnomeInputSourceProviderPrivate *priv;
  priv = GNOME_INPUT_SOURCE_PROVIDER_PRIVATE (object);
  switch (property_id)
    {
    case PROP_IMMODULE_NAME:
      g_value_set_string (value, priv->immodule_name);
      break;
    case PROP_SUPPORTED_IMMODULES:
      g_value_set_boxed (value, priv->supported_immodules);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


gboolean
gnome_input_source_provider_get_activated (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceProviderClass *klass = GNOME_INPUT_SOURCE_PROVIDER_GET_CLASS (provider);
  if (klass && klass->get_activated)
    return klass->get_activated (provider);
  return FALSE;
}

GList *
gnome_input_source_provider_get_active_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceProviderClass *klass = GNOME_INPUT_SOURCE_PROVIDER_GET_CLASS (provider);
  if (klass && klass->get_active_sources)
    return klass->get_active_sources (provider);
  return NULL;
}

GList *
gnome_input_source_provider_get_inactive_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceProviderClass *klass = GNOME_INPUT_SOURCE_PROVIDER_GET_CLASS (provider);
  if (klass && klass->get_inactive_sources)
    return klass->get_inactive_sources (provider);
  return NULL;
}

void
gnome_input_source_provider_set_active_sources (GnomeInputSourceProvider *provider, GList *sources)
{
  GnomeInputSourceProviderClass *klass = GNOME_INPUT_SOURCE_PROVIDER_GET_CLASS (provider);
  if (klass && klass->set_active_sources)
    klass->set_active_sources (provider, sources);
}

const gchar *
gnome_input_source_provider_get_immodule_name (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceProviderPrivate *priv;
  g_return_val_if_fail (GNOME_IS_INPUT_SOURCE_PROVIDER (provider), NULL);

  priv = GNOME_INPUT_SOURCE_PROVIDER_PRIVATE (provider);
  return priv->immodule_name;
}

void
gnome_input_source_provider_set_immodule_name (GnomeInputSourceProvider *provider,
                                               const gchar *immodule_name)
{
  GnomeInputSourceProviderPrivate *priv;
  g_return_if_fail (GNOME_IS_INPUT_SOURCE_PROVIDER (provider));
  g_return_if_fail (immodule_name != NULL);

  priv = GNOME_INPUT_SOURCE_PROVIDER_PRIVATE (provider);
  g_free (priv->immodule_name);
  priv->immodule_name = g_strdup (immodule_name);
  g_object_notify (G_OBJECT (provider), "immodule-name");
}

const gchar * const *
gnome_input_source_get_supported_immodules (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceProviderPrivate *priv;
  g_return_val_if_fail (GNOME_IS_INPUT_SOURCE_PROVIDER (provider), NULL);
  priv = GNOME_INPUT_SOURCE_PROVIDER_PRIVATE (provider);
  return (const gchar * const *) priv->supported_immodules;
}

void
gnome_input_source_set_supported_immodules (GnomeInputSourceProvider *provider,
                                            const gchar * const *supported_immodules)
{
  GnomeInputSourceProviderPrivate *priv;
  g_return_if_fail (GNOME_IS_INPUT_SOURCE_PROVIDER (provider));
  priv = GNOME_INPUT_SOURCE_PROVIDER_PRIVATE (provider);
  if (priv->supported_immodules)
    g_strfreev (priv->supported_immodules);
  priv->supported_immodules = g_boxed_copy (G_TYPE_STRV, supported_immodules);
  g_object_notify (G_OBJECT (provider), "supported-immodules");
}

gchar *
gnome_input_source_provider_get_setting (GnomeInputSourceProvider *provider,
                                         guint setting_id)
{
  GnomeInputSourceProviderClass *klass;
  g_return_val_if_fail (GNOME_IS_INPUT_SOURCE_PROVIDER (provider), NULL);
  klass = GNOME_INPUT_SOURCE_PROVIDER_GET_CLASS (provider);
  if (klass && klass->get_setting)
    return klass->get_setting (provider, setting_id);
  else
    return NULL;
}
gchar *
gnome_input_source_provider_get_next_source_key (GnomeInputSourceProvider *provider)
{
  return gnome_input_source_provider_get_setting (provider,
                                                  GNOME_INPUT_SOURCE_PROVIDER_SETTING_NEXT_SOURCE);
}

gchar *
gnome_input_source_provider_get_previous_source_key (GnomeInputSourceProvider *provider)
{
  return gnome_input_source_provider_get_setting (provider,
                                                  GNOME_INPUT_SOURCE_PROVIDER_SETTING_PREVIOUS_SOURCE);
}

void
gnome_input_source_provider_show_settings_dialog (GnomeInputSourceProvider *provider,
                                         guint setting_id)
{
  GnomeInputSourceProviderClass *klass;
  g_return_if_fail (GNOME_IS_INPUT_SOURCE_PROVIDER (provider));
  klass = GNOME_INPUT_SOURCE_PROVIDER_GET_CLASS (provider);
  if (klass && klass->show_settings_dialog)
    klass->show_settings_dialog (provider, setting_id);
}
