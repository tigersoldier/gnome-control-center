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

#ifndef _GNOME_INPUT_SOURCE_XKB_H_
#define _GNOME_INPUT_SOURCE_XKB_H_

#include "gnome-input-source-provider.h"
#include "gnome-input-source.h"

G_BEGIN_DECLS

/* GnomeXkbSource */
#define GNOME_TYPE_XKB_SOURCE                    (gnome_xkb_source_get_type ())
#define GNOME_XKB_SOURCE(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_XKB_SOURCE, GnomeXkbSource))
#define GNOME_XKB_SOURCE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_XKB_SOURCE, GnomeXkbSourceClass))
#define GNOME_IS_XKB_SOURCE(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_XKB_SOURCE))
#define GNOME_IS_XKB_SOURCE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_XKB_SOURCE))
#define GNOME_XKB_SOURCE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_XKB_SOURCE, GnomeXkbSourceClass))

typedef struct _GnomeXkbSource GnomeXkbSource;
typedef struct _GnomeXkbSourceClass GnomeXkbSourceClass;

struct _GnomeXkbSource {
  GnomeInputSource parent_object;
};

struct _GnomeXkbSourceClass {
  GnomeInputSourceClass parent_class;
};

GType gnome_xkb_source_get_type (void);
GnomeXkbSource *gnome_xkb_source_new (const gchar *provider,
                                      const gchar *id,
                                      const gchar *name,
                                      const gchar *desktop_file_name,
                                      const gchar *layout,
                                      const gchar *layout_variant);
const gchar *gnome_xkb_source_get_provider (GnomeXkbSource *source);
const gchar *gnome_xkb_source_get_id (GnomeXkbSource *source);
const gchar *gnome_xkb_source_get_desktop_file_name (GnomeXkbSource *source);

/* GnomeInputSourceXkb */
#define GNOME_TYPE_INPUT_SOURCE_XKB              (gnome_input_source_xkb_get_type ())
#define GNOME_INPUT_SOURCE_XKB(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_INPUT_SOURCE_XKB, GnomeInputSourceXkb))
#define GNOME_INPUT_SOURCE_XKB_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_INPUT_SOURCE_XKB, GnomeInputSourceXkbClass))
#define GNOME_IS_INPUT_SOURCE_XKB(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_INPUT_SOURCE_XKB))
#define GNOME_IS_INPUT_SOURCE_XKB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_INPUT_SOURCE_XKB))
#define GNOME_INPUT_SOURCE_XKB_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_INPUT_SOURCE_XKB, GnomeInputSourceXkbClass))

typedef struct _GnomeInputSourceXkb      GnomeInputSourceXkb;
typedef struct _GnomeInputSourceXkbClass GnomeInputSourceXkbClass;
typedef GnomeXkbSource *(*GnomeInputSourceXkbCreateFunc) (const gchar *type,
                                                          const gchar *id,
                                                          gpointer userdata);

struct _GnomeInputSourceXkb {
  GnomeInputSourceProvider parent_instance;
};

struct _GnomeInputSourceXkbClass {
  GnomeInputSourceProviderClass parent_class;
};

GType gnome_input_source_xkb_get_type (void);
GnomeInputSourceXkb *gnome_input_source_xkb_new (void);
void gnome_input_source_xkb_add_source_handler (GnomeInputSourceXkb *xkb,
                                                const gchar *provider,
                                                GnomeInputSourceXkbCreateFunc create_func,
                                                gpointer userdata);
void gnome_input_source_xkb_remove_source_handler (GnomeInputSourceXkb *xkb,
                                                   const gchar *provider);
GVariant *gnome_input_source_xkb_get_active_sources (GnomeInputSourceXkb *xkb);

G_END_DECLS

#endif /* _GNOME_INPUT_SOURCE_XKB_H_ */
