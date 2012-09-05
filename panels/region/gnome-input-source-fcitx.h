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

#ifndef _GNOME_INPUT_SOURCE_FCITX_H_
#define _GNOME_INPUT_SOURCE_FCITX_H_

#include "gnome-input-source-provider.h"

G_BEGIN_DECLS

#define GNOME_TYPE_INPUT_SOURCE_FCITX              (gnome_input_source_fcitx_get_type ())
#define GNOME_INPUT_SOURCE_FCITX(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_INPUT_SOURCE_FCITX, GnomeInputSourceFcitx))
#define GNOME_INPUT_SOURCE_FCITX_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_INPUT_SOURCE_FCITX, GnomeInputSourceFcitxClass))
#define GNOME_IS_INPUT_SOURCE_FCITX(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_INPUT_SOURCE_FCITX))
#define GNOME_IS_INPUT_SOURCE_FCITX_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_INPUT_SOURCE_FCITX))
#define GNOME_INPUT_SOURCE_FCITX_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_INPUT_SOURCE_FCITX, GnomeInputSourceFcitxClass))

typedef struct _GnomeInputSourceFcitx      GnomeInputSourceFcitx;
typedef struct _GnomeInputSourceFcitxClass GnomeInputSourceFcitxClass;

struct _GnomeInputSourceFcitx {
  GnomeInputSourceProvider parent_instance;
};

struct _GnomeInputSourceFcitxClass {
  GnomeInputSourceProviderClass parent_class;
};

GType gnome_input_source_fcitx_get_type (void);
GnomeInputSourceFcitx *gnome_input_source_fcitx_new (void);

G_END_DECLS

#endif /* _GNOME_INPUT_SOURCE_FCITX_H_ */
