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

#include "gnome-input-source.h"

#define GNOME_INPUT_SOURCE_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_INPUT_SOURCE, GnomeInputSourcePrivate))

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_LAYOUT,
  PROP_LAYOUT_VARIANT,
};

typedef struct _GnomeInputSourcePrivate GnomeInputSourcePrivate;

struct _GnomeInputSourcePrivate {
  gchar *id;
  gchar *name;
  gchar *layout;
  gchar *layout_variant;
};

static void gnome_input_source_finalize (GObject *object);
static void gnome_input_source_set_property (GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void gnome_input_source_get_property (GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec);

G_DEFINE_TYPE (GnomeInputSource, gnome_input_source, G_TYPE_OBJECT)

static void
gnome_input_source_class_init (GnomeInputSourceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = gnome_input_source_finalize;
  gobject_class->set_property = gnome_input_source_set_property;
  gobject_class->get_property = gnome_input_source_get_property;
  g_object_class_install_property (gobject_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "ID",
                                                        "ID",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT,
                                   g_param_spec_string ("layout",
                                                        "Layout",
                                                        "Keyboard layout of the input source",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT_VARIANT,
                                   g_param_spec_string ("layout-variant",
                                                        "Layout Variant",
                                                        "Keyboard variant of the input source layout",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_type_class_add_private (klass, sizeof (GnomeInputSourcePrivate));
}

static void
gnome_input_source_init (GnomeInputSource *source)
{
}

GnomeInputSource *
gnome_input_source_new (const gchar *id,
                        const gchar *name,
                        const gchar *layout,
                        const gchar *layout_variant)
{
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  return g_object_new (GNOME_TYPE_INPUT_SOURCE,
                       "id", id,
                       "name", name,
                       "layout", layout,
                       "layout-variant", layout_variant,
                       NULL);
}

static void
gnome_input_source_finalize (GObject *object)
{
  GnomeInputSourcePrivate *priv = GNOME_INPUT_SOURCE_PRIVATE (object);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->layout, g_free);
  g_clear_pointer (&priv->layout_variant, g_free);
  G_OBJECT_CLASS (gnome_input_source_parent_class)->finalize (object);
}

static void
gnome_input_source_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  GnomeInputSourcePrivate *priv = GNOME_INPUT_SOURCE_PRIVATE (object);
  switch (property_id)
    {
    case PROP_ID:
      g_free (priv->id);
      priv->id = g_value_dup_string (value);
      break;
    case PROP_NAME:
      g_free (priv->name);
      priv->name = g_value_dup_string (value);
      break;
    case PROP_LAYOUT:
      g_free (priv->layout);
      priv->layout = g_value_dup_string (value);
      break;
    case PROP_LAYOUT_VARIANT:
      g_free (priv->layout_variant);
      priv->layout_variant = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id,
                                         pspec);
    }
}

static void
gnome_input_source_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)

{
  GnomeInputSourcePrivate *priv = GNOME_INPUT_SOURCE_PRIVATE (object);
  switch (property_id)
    {
    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_LAYOUT:
      g_value_set_string (value, priv->layout);
      break;
    case PROP_LAYOUT_VARIANT:
      g_value_set_string (value, priv->layout_variant);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id,
                                         pspec);
    }
}

const gchar *
gnome_input_source_get_id (GnomeInputSource *source)
{
  if (!GNOME_IS_INPUT_SOURCE (source))
    return NULL;
  return GNOME_INPUT_SOURCE_PRIVATE (source)->id;
}

const gchar *
gnome_input_source_get_name (GnomeInputSource *source)
{
  if (!GNOME_IS_INPUT_SOURCE (source))
    return NULL;
  return GNOME_INPUT_SOURCE_PRIVATE (source)->name;
}

const gchar *
gnome_input_source_get_layout (GnomeInputSource *source)
{
  if (!GNOME_IS_INPUT_SOURCE (source))
    return NULL;
  return GNOME_INPUT_SOURCE_PRIVATE (source)->layout;
}

const gchar *
gnome_input_source_get_layout_variant (GnomeInputSource *source)
{
  if (!GNOME_IS_INPUT_SOURCE (source))
    return NULL;
  return GNOME_INPUT_SOURCE_PRIVATE (source)->layout_variant;
}

gboolean
gnome_input_source_has_preference (GnomeInputSource *source)
{
  GnomeInputSourceClass *klass = GNOME_INPUT_SOURCE_GET_CLASS (source);
  if (klass && klass->has_preference)
    return klass->has_preference (source);
  return FALSE;
}

void
gnome_input_source_show_preference (GnomeInputSource *source)
{
  GnomeInputSourceClass *klass = GNOME_INPUT_SOURCE_GET_CLASS (source);
  if (klass && klass->show_preference)
    return klass->show_preference (source);
}
