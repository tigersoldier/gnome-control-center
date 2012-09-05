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

#ifndef _GNOME_INPUT_SOURCE_PROVIDER_H_
#define _GNOME_INPUT_SOURCE_PROVIDER_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_INPUT_SOURCE_PROVIDER              (gnome_input_source_provider_get_type ())
#define GNOME_INPUT_SOURCE_PROVIDER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_INPUT_SOURCE_PROVIDER, GnomeInputSourceProvider))
#define GNOME_INPUT_SOURCE_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_INPUT_SOURCE_PROVIDER, GnomeInputSourceProviderClass))
#define GNOME_IS_INPUT_SOURCE_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_INPUT_SOURCE_PROVIDER))
#define GNOME_IS_INPUT_SOURCE_PROVIDER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_INPUT_SOURCE_PROVIDER))
#define GNOME_INPUT_SOURCE_PROVIDER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_INPUT_SOURCE_PROVIDER, GnomeInputSourceProviderClass))

enum GnomeInputSourceProviderSetting {
  GNOME_INPUT_SOURCE_PROVIDER_SETTING_NEXT_SOURCE,
  GNOME_INPUT_SOURCE_PROVIDER_SETTING_PREVIOUS_SOURCE,
};

typedef struct _GnomeInputSourceProvider      GnomeInputSourceProvider;
typedef struct _GnomeInputSourceProviderClass GnomeInputSourceProviderClass;

struct _GnomeInputSourceProvider {
  GObject parent_instance;
};

struct _GnomeInputSourceProviderClass {
  /*< private >*/
  GObjectClass parent_class;
  /*< public >*/
  /* Signals */
  void (*activated) (GnomeInputSourceProvider *provider);
  void (*deactivated) (GnomeInputSourceProvider *provider);
  void (*sources_changed) (GnomeInputSourceProvider *provider);
  void (*settings_changed) (GnomeInputSourceProvider *provider);
  /* Virtual functions */
  gboolean (*get_activated) (GnomeInputSourceProvider *provider);
  GList *(*get_active_sources) (GnomeInputSourceProvider *provider);
  GList *(*get_inactive_sources) (GnomeInputSourceProvider *provider);
  void (*set_active_sources) (GnomeInputSourceProvider *provider,
                              GList *sources);
  gchar *(*get_setting) (GnomeInputSourceProvider *provider,
                         guint setting_id);
  void (*show_settings_dialog) (GnomeInputSourceProvider *provider,
                                guint setting_id);
  /* Padding for future extension */
  void (*_reservied1) (void);
  void (*_reservied2) (void);
  void (*_reservied3) (void);
  void (*_reservied4) (void);
  void (*_reservied5) (void);
  void (*_reservied6) (void);
};

GType gnome_input_source_provider_get_type (void) G_GNUC_CONST;
const gchar *gnome_input_source_provider_get_immodule_name (GnomeInputSourceProvider *provider);
void gnome_input_source_provider_set_immodule_name (GnomeInputSourceProvider *provider,
                                                    const gchar *immodule_name);
gboolean gnome_input_source_provider_get_activated (GnomeInputSourceProvider *provider);
GList *gnome_input_source_provider_get_active_sources (GnomeInputSourceProvider *provider);
GList *gnome_input_source_provider_get_inactive_sources (GnomeInputSourceProvider *provider);
void gnome_input_source_provider_set_active_sources (GnomeInputSourceProvider *provider, GList *sources);
const gchar * const *gnome_input_source_get_supported_immodules (GnomeInputSourceProvider *provider);
void gnome_input_source_set_supported_immodules (GnomeInputSourceProvider *provider,
                                                 const gchar * const *supported_immodules);
gchar *gnome_input_source_provider_get_setting (GnomeInputSourceProvider *provider,
                                                guint setting_id);
gchar *gnome_input_source_provider_get_next_source_key (GnomeInputSourceProvider *provider);
gchar *gnome_input_source_provider_get_previous_source_key (GnomeInputSourceProvider *provider);
void gnome_input_source_provider_show_settings_dialog (GnomeInputSourceProvider *provider,
                                                         guint setting_id);
G_END_DECLS

#endif /* _GNOME_INPUT_SOURCE_PROVIDER_H_ */
