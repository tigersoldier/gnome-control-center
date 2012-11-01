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

#include "gnome-input-source-fcitx.h"
#include "gnome-input-source.h"
#include <stdio.h>
#include <fcitx-gclient/fcitxinputmethod.h>
#include <fcitx-gclient/fcitxkbd.h>
#include <fcitx-config/fcitx-config.h>
#include <fcitx-utils/utils.h>

#define GNOME_INPUT_SOURCE_FCITX_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_INPUT_SOURCE_FCITX, GnomeInputSourceFcitxPrivate))

G_DEFINE_TYPE (GnomeInputSourceFcitx, gnome_input_source_fcitx, GNOME_TYPE_INPUT_SOURCE_PROVIDER)

typedef struct _GnomeInputSourceFcitxPrivate GnomeInputSourceFcitxPrivate;
struct _GnomeInputSourceFcitxPrivate {
  FcitxInputMethod *im;
  FcitxKbd *kbd;
  GPtrArray *imlist;
  FcitxConfigFileDesc *config_desc;
  FcitxConfigFile *config_file;
};

static gboolean fcitx_get_activated (GnomeInputSourceProvider *provider);
static GList *fcitx_get_active_sources (GnomeInputSourceProvider *provider);
static GList *fcitx_get_inactive_sources (GnomeInputSourceProvider *provider);
static void fcitx_set_active_sources (GnomeInputSourceProvider *provider,
                                      GList *sources);
static gchar *fcitx_get_setting (GnomeInputSourceProvider *provider,
                                 guint setting_id);
static void fcitx_show_settings_dialog (GnomeInputSourceProvider *provider,
                                        guint setting_id);
static void fcitx_finalize (GObject *object);
static void fcitx_imlist_changed_cb (FcitxInputMethod *fcitx_im,
                                     GnomeInputSourceFcitx *fcitx);
static void fcitx_name_owner_changed_cb (GObject *gobject,
                                         GParamSpec *pspec,
                                         GnomeInputSourceFcitx *fcitx);
static GnomeInputSource *fcitx_source_new (GnomeInputSourceFcitx *fcitx,
                                           FcitxIMItem *imitem);
static FcitxConfigFileDesc *fcitx_get_config_desc (void);
static FcitxConfigFile *fcitx_get_config_file (FcitxConfigFileDesc *cfdesc);

static void
gnome_input_source_fcitx_class_init (GnomeInputSourceFcitxClass *klass)
{
  GObjectClass *object_class;
  GnomeInputSourceProviderClass *provider_class;
  object_class = G_OBJECT_CLASS (klass);
  provider_class = GNOME_INPUT_SOURCE_PROVIDER_CLASS (klass);
  object_class->finalize = fcitx_finalize;
  provider_class->get_activated = fcitx_get_activated;
  provider_class->get_active_sources = fcitx_get_active_sources;
  provider_class->get_inactive_sources = fcitx_get_inactive_sources;
  provider_class->set_active_sources = fcitx_set_active_sources;
  provider_class->get_setting = fcitx_get_setting;
  provider_class->show_settings_dialog = fcitx_show_settings_dialog;
  g_type_class_add_private (klass, sizeof (GnomeInputSourceFcitxPrivate));
}

static void
gnome_input_source_fcitx_init (GnomeInputSourceFcitx *fcitx)
{
  GnomeInputSourceFcitxPrivate *priv;
  GError *error = NULL;
  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (fcitx);
  priv->im = fcitx_input_method_new (G_BUS_TYPE_SESSION,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     fcitx_utils_get_display_number (),
                                     NULL,
                                     &error);
  if (!priv->im)
    {
      g_warning ("Failed to create FcitxInputMethod instance: %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      g_signal_connect (G_OBJECT (priv->im),
                        "imlist-changed",
                        G_CALLBACK (fcitx_imlist_changed_cb),
                        fcitx);
      g_signal_connect (G_OBJECT (priv->im),
                        "notify::g-name-owner",
                        G_CALLBACK (fcitx_name_owner_changed_cb),
                        fcitx);
      priv->imlist = fcitx_input_method_get_imlist (priv->im);
    }
  priv->kbd = fcitx_kbd_new (G_BUS_TYPE_SESSION,
                             G_DBUS_PROXY_FLAGS_NONE,
                             fcitx_utils_get_display_number(),
                             NULL, NULL);
  priv->config_desc = fcitx_get_config_desc ();
  priv->config_file = fcitx_get_config_file (priv->config_desc);
}

static void
fcitx_finalize (GObject *object)
{
  GnomeInputSourceFcitxPrivate *priv;
  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (object);
  g_clear_pointer (&priv->config_file, FcitxConfigFreeConfigFile);
  g_clear_pointer (&priv->config_desc, FcitxConfigFreeConfigFileDesc);
  if (priv->im)
    {
      g_signal_handlers_disconnect_by_func (priv->im,
                                            G_CALLBACK (fcitx_imlist_changed_cb),
                                            object);
      g_signal_handlers_disconnect_by_func (priv->im,
                                            G_CALLBACK (fcitx_name_owner_changed_cb),
                                            object);
      g_clear_object (&priv->im);
    }
  g_clear_object (&priv->kbd);
  if (priv->imlist)
    {
      g_ptr_array_set_free_func (priv->imlist,
                                 (GDestroyNotify) fcitx_im_item_free);
      g_ptr_array_free (priv->imlist, TRUE);
      priv->imlist = NULL;
    }
  G_OBJECT_CLASS (gnome_input_source_fcitx_parent_class)->finalize (object);
}

static void
fcitx_name_owner_changed_cb (GObject *gobject,
                             GParamSpec *pspec,
                             GnomeInputSourceFcitx *fcitx)
{
  if (fcitx_get_activated (GNOME_INPUT_SOURCE_PROVIDER (fcitx)))
    g_signal_emit_by_name (fcitx, "activated");
  else
    g_signal_emit_by_name (fcitx, "deactivated");
}

static gboolean
fcitx_get_activated (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceFcitxPrivate *priv;
  gchar *name_owner;
  gboolean connected;

  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (provider);
  if (!FCITX_IS_INPUT_METHOD (priv->im))
    return FALSE;
  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (priv->im));
  connected = (name_owner != NULL);
  g_free (name_owner);
  return connected;
}

static GnomeInputSource *
fcitx_source_new (GnomeInputSourceFcitx *fcitx,
                  FcitxIMItem *imitem)
{
  GnomeInputSourceFcitxPrivate *priv;
  gchar *layout = NULL, *variant = NULL;
  GnomeInputSource *source;
  gchar **keyboard_parts;

  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (fcitx);
  if (g_str_has_prefix (imitem->unique_name, "fcitx-keyboard"))
    {
      keyboard_parts = g_strsplit (imitem->unique_name, "-", 4);
      layout = g_strdup (keyboard_parts [2]);
      if (keyboard_parts[3])
        variant = g_strdup (keyboard_parts[3]);
      g_strfreev (keyboard_parts);
    }
  else
    {
      fcitx_kbd_get_layout_for_im(priv->kbd, imitem->unique_name, &layout, &variant);
      if (!layout || !layout[0])
        {
          /* FIXME: Read the system default layout */
        }
    }
  source = gnome_input_source_new (imitem->unique_name,
                                   imitem->name,
                                   layout,
                                   variant);
  g_free (layout);
  g_free (variant);
  return source;
}

static GList *
fcitx_get_active_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceFcitxPrivate *priv;
  GList *active_sources = NULL;
  gint i;
  FcitxIMItem *imitem;
  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (provider);
  if (!priv->imlist)
    return NULL;
  for (i = 0; i < priv->imlist->len; i++)
    {
      imitem = g_ptr_array_index (priv->imlist, i);
      if (!imitem->enable)
        continue;
      active_sources = g_list_prepend (active_sources,
                                       fcitx_source_new (GNOME_INPUT_SOURCE_FCITX (provider),
                                                         imitem));
    }
  return g_list_reverse (active_sources);
}

static GList *
fcitx_get_inactive_sources (GnomeInputSourceProvider *provider)
{
  GnomeInputSourceFcitxPrivate *priv;
  GList *inactive_sources = NULL;
  gint i;
  FcitxIMItem *imitem;
  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (provider);
  if (!priv->imlist)
    return NULL;
  for (i = 0; i < priv->imlist->len; i++)
    {
      imitem = g_ptr_array_index (priv->imlist, i);
      if (imitem->enable)
        continue;
      inactive_sources = g_list_prepend (inactive_sources,
                                         fcitx_source_new (GNOME_INPUT_SOURCE_FCITX (provider),
                                                           imitem));
    }
  return g_list_reverse (inactive_sources);
}

static void
fcitx_set_active_sources (GnomeInputSourceProvider *provider,
                          GList *sources)
{
  GnomeInputSourceFcitxPrivate *priv;
  GHashTable *imitem_table, *active_source_table;
  GList *tmp;
  FcitxIMItem *imitem;
  GnomeInputSource *source;
  guint i;
  GPtrArray *new_imlist;
  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (provider);
  if (!priv->im || !priv->imlist)
    return;
  imitem_table = g_hash_table_new (g_str_hash, g_str_equal);
  active_source_table = g_hash_table_new (g_str_hash, g_str_equal);
  new_imlist = g_ptr_array_new_with_free_func ((GDestroyNotify) fcitx_im_item_free);
  for (i = 0; i < priv->imlist->len; i++)
    {
      imitem = g_ptr_array_index (priv->imlist, i);
      g_hash_table_insert (imitem_table, imitem->unique_name, imitem);
    }
  for (tmp = sources; tmp; tmp = tmp->next)
    {
      source = tmp->data;
      imitem = g_hash_table_lookup (imitem_table, gnome_input_source_get_id (source));
      if (imitem)
        {
          g_hash_table_insert (active_source_table, imitem->unique_name, imitem);
          imitem->enable = TRUE;
          g_ptr_array_add (new_imlist, imitem);
        }
    }
  for (i = 0; i < priv->imlist->len; i++)
    {
      imitem = g_ptr_array_index (priv->imlist, i);
      if (NULL == g_hash_table_lookup (active_source_table, imitem->unique_name))
        {
          imitem->enable = FALSE;
          g_ptr_array_add (new_imlist, imitem);
        }
    }
  g_hash_table_destroy (imitem_table);
  g_hash_table_destroy (active_source_table);
  g_ptr_array_set_free_func (priv->imlist, NULL);
  g_ptr_array_free (priv->imlist, TRUE);
  priv->imlist = new_imlist;
  fcitx_input_method_set_imlist (priv->im, new_imlist);
}

static gchar *
fcitx_get_setting (GnomeInputSourceProvider *provider,
                   guint setting_id)
{
  GnomeInputSourceFcitxPrivate *priv;
  FcitxConfigOption *option = NULL;
  gchar *setting = NULL;
  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (provider);
  switch (setting_id)
    {
    case GNOME_INPUT_SOURCE_PROVIDER_SETTING_NEXT_SOURCE:
      option = FcitxConfigFileGetOption (priv->config_file,
                                         "Hotkey", "IMSwitchHotkey");
      setting = g_strdup (option->rawValue);
      break;
    case GNOME_INPUT_SOURCE_PROVIDER_SETTING_PREVIOUS_SOURCE:
      /* Fcitx dosn't support switch input source backwards */
      break;
    default:
      g_warning ("Unknown setting ID: %u", setting_id);
      break;
    }
  return setting;
}

static void
fcitx_show_settings_dialog (GnomeInputSourceProvider *provider,
                            guint setting_id)
{
  g_spawn_command_line_async ("fcitx-configtool", NULL);
}

static void
fcitx_imlist_changed_cb (FcitxInputMethod *fcitx_im,
                         GnomeInputSourceFcitx *fcitx)
{
  GnomeInputSourceFcitxPrivate *priv;

  priv = GNOME_INPUT_SOURCE_FCITX_PRIVATE (fcitx);
  if (priv->imlist)
    {
      g_ptr_array_set_free_func (priv->imlist,
                                 (GDestroyNotify) fcitx_im_item_free);
      g_ptr_array_free (priv->imlist, TRUE);
    }
  priv->imlist = fcitx_input_method_get_imlist (fcitx_im);
  g_signal_emit_by_name (fcitx, "sources-changed");
}

GnomeInputSourceFcitx *
gnome_input_source_fcitx_new (void)
{
  const gchar *supported_immodules[] = { "fcitx", NULL };
  return g_object_new (GNOME_TYPE_INPUT_SOURCE_FCITX,
                       "immodule-name", "fcitx",
                       "supported-immodules", supported_immodules,
                       NULL);
}


static FcitxConfigFileDesc *
fcitx_get_config_desc(void)
{
  FcitxConfigFileDesc *desc = NULL;
  FILE * tmpfp;
  tmpfp = FcitxXDGGetFileWithPrefix("configdesc", "config.desc", "r", NULL);
  if (tmpfp)
    {
      desc = FcitxConfigParseConfigFileDescFp(tmpfp);
      fclose(tmpfp);
    }
  else
    {
      g_warning ("Cannot parse fcitx config file desc");
    }
  return desc;
}

static FcitxConfigFile *
fcitx_get_config_file (FcitxConfigFileDesc *cfdesc)
{
  FcitxConfigFile *config_file = NULL;
  FILE * tmpfp;
  if (!cfdesc)
    return NULL;
  tmpfp = FcitxXDGGetFileWithPrefix("", "config", "r", NULL);
  if (tmpfp)
    {
      config_file = FcitxConfigParseConfigFileFp (tmpfp, cfdesc);
      fclose (tmpfp);
    }
  else
    {
      g_warning ("Cannot parse fcitx config file");
    }
  return config_file;
}
