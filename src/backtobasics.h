/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * backtobasics.h
 * Copyright (C) 2013 Hans Oesterholt <debian@oesterholt.net>
 * 
 * BackToBasics is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * BackToBasics is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BACKTOBASICS_
#define _BACKTOBASICS_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BACKTOBASICS_TYPE_APPLICATION             (backtobasics_get_type ())
#define BACKTOBASICS_APPLICATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BACKTOBASICS_TYPE_APPLICATION, Backtobasics))
#define BACKTOBASICS_APPLICATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BACKTOBASICS_TYPE_APPLICATION, BacktobasicsClass))
#define BACKTOBASICS_IS_APPLICATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BACKTOBASICS_TYPE_APPLICATION))
#define BACKTOBASICS_IS_APPLICATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BACKTOBASICS_TYPE_APPLICATION))
#define BACKTOBASICS_APPLICATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BACKTOBASICS_TYPE_APPLICATION, BacktobasicsClass))

typedef struct _BacktobasicsClass BacktobasicsClass;
typedef struct _Backtobasics Backtobasics;
typedef struct _BacktobasicsPrivate BacktobasicsPrivate;

struct _BacktobasicsClass
{
	GtkApplicationClass parent_class;
};

struct _Backtobasics
{
	GtkApplication parent_instance;

	BacktobasicsPrivate *priv;

};

GType backtobasics_get_type (void) G_GNUC_CONST;
Backtobasics *backtobasics_new (void);

/* Callbacks */

G_END_DECLS

#endif /* _APPLICATION_H_ */