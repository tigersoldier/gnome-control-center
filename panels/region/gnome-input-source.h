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

#ifndef _GNOME_INPUT_SOURCE_H_
#define _GNOME_INPUT_SOURCE_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_INPUT_SOURCE              (gnome_input_source_get_type ())
#define GNOME_INPUT_SOURCE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_INPUT_SOURCE, GnomeInputSource))
#define GNOME_INPUT_SOURCE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_INPUT_SOURCE, GnomeInputSourceClass))
#define GNOME_IS_INPUT_SOURCE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_INPUT_SOURCE))
#define GNOME_IS_INPUT_SOURCE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_INPUT_SOURCE))
#define GNOME_INPUT_SOURCE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_INPUT_SOURCE, GnomeInputSourceClass))

typedef struct _GnomeInputSource      GnomeInputSource;
typedef struct _GnomeInputSourceClass GnomeInputSourceClass;

struct _GnomeInputSource
{
  GObject parent_instance;
};

struct _GnomeInputSourceClass
{
  GObjectClass parent_class;
  gboolean (*has_preference) (GnomeInputSource *source);
  void (*show_preference) (GnomeInputSource *source);
};

GType gnome_input_source_get_type (void);
GnomeInputSource *gnome_input_source_new (const gchar *id,
                                          const gchar *name,
                                          const gchar *layout,
                                          const gchar *layout_variant);
const gchar *gnome_input_source_get_id (GnomeInputSource *source);
const gchar *gnome_input_source_get_name (GnomeInputSource *source);
const gchar *gnome_input_source_get_layout (GnomeInputSource *source);
const gchar *gnome_input_source_get_layout_variant (GnomeInputSource *source);
gboolean gnome_input_source_has_preference (GnomeInputSource *source);
void gnome_input_source_show_preference (GnomeInputSource *source);

G_END_DECLS

#endif /* _GNOME_INPUT_SOURCE_H_ */
