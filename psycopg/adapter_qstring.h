/* adapter_qstring.h - definition for the QuotedString type
 *
 * Copyright (C) 2003 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of psycopg.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PSYCOPG_QSTRING_H
#define PSYCOPG_QSTRING_H 1

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

extern PyTypeObject qstringType;

typedef struct {
    PyObject HEAD;

    PyObject *wrapped;
    PyObject *buffer;
    char     *encoding;
} qstringObject;
    
/* functions exported to psycopgmodule.c */
    
extern PyObject *psyco_QuotedString(PyObject *module, PyObject *args);
#define psyco_QuotedString_doc \
    "psycopg.QuotedString(str, enc) -> new quoted string"

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_QSTRING_H) */
