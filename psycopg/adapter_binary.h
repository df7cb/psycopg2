/* adapter_binary.h - definition for the Binary type
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

#ifndef PSYCOPG_BINARY_H
#define PSYCOPG_BINARY_H 1

#include <Python.h>
#include <libpq-fe.h>

#ifdef __cplusplus
extern "C" {
#endif

extern PyTypeObject binaryType;

typedef struct {
    PyObject HEAD;

    PyObject *wrapped;
    PyObject *buffer;
    char     *encoding;
} binaryObject;

/* functions exported to psycopgmodule.c */
    
extern PyObject *psyco_Binary(PyObject *module, PyObject *args);
#define psyco_Binary_doc \
    "psycopg.Binary(buffer) -> new binary object"

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_BINARY_H) */
