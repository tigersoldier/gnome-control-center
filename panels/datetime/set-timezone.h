/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __SET_SYSTEM_TIMEZONE_H__

#include <config.h>

#include <glib.h>
#include <time.h>

typedef void (*GetTimezoneFunc) (gpointer     data,
                                 const gchar *timezone,
                                 GError      *error);
void     get_system_timezone_async   (GetTimezoneFunc callback,
                                      gpointer        data,
                                      GDestroyNotify  notify);

gint     can_set_system_timezone (void);

gint     can_set_system_time     (void);

void     set_system_time_async   (gint64         time,
                                  GFunc          callback,
                                  gpointer       data,
                                  GDestroyNotify notify);

void     set_system_timezone_async   (const gchar    *filename,
                                      GFunc           callback,
                                      gpointer        data,
                                      GDestroyNotify  notify);



#ifdef HAVE_SOLARIS
#define SYSTEM_ZONEINFODIR "/usr/share/lib/zoneinfo/tab"
#else
#define SYSTEM_ZONEINFODIR "/usr/share/zoneinfo"
#endif

#endif