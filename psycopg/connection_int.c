/* connection_int.c - code used by the connection object
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

#include <Python.h>
#include <string.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/psycopg.h"
#include "psycopg/connection.h"
#include "psycopg/cursor.h"
#include "psycopg/pqpath.h"

/* conn_notice_callback - process notices */

void
conn_notice_callback(void *args, const char *message)
{
    connectionObject *self = (connectionObject *)args;

    Dprintf("conn_notice_callback: %s", message);

    /* unfortunately the old protocl return COPY FROM errors only as notices,
       so we need to filter them looking for such errors */
    if (strncmp(message, "ERROR", 5) == 0)
        pq_set_critical(self, message);
    else
        PyList_Append(self->notice_list, PyString_FromString(message));
}

/* conn_connect - execute a connection to the dataabase */

int
conn_connect(connectionObject *self)
{
    PGconn *pgconn;
    PGresult *pgres;

    /* we need the initial date style to be ISO, for typecasters; if the user
       later change it, she must know what she's doing... */
    const char *datestyle = "SET DATESTYLE TO 'ISO'";
    const char *encoding  = "SHOW client_encoding";
    
    Py_BEGIN_ALLOW_THREADS;
    pgconn = PQconnectdb(self->dsn);
    Py_END_ALLOW_THREADS;
    
    Dprintf("conn_connect: new postgresql connection at %p", pgconn);

    if (pgconn == NULL)
    {
        Dprintf("conn_connect: PQconnectdb(%s) FAILED", self->dsn);
        PyErr_SetString(OperationalError, "PQconnectdb() failed");
        return -1;
    }
    else if (PQstatus(pgconn) == CONNECTION_BAD)
    {
        Dprintf("conn_connect: PQconnectdb(%s) returned BAD", self->dsn);
        PyErr_SetString(OperationalError, PQerrorMessage(pgconn));
        PQfinish(pgconn);
        return -1;
    }

    PQsetNoticeProcessor(pgconn, conn_notice_callback, (void*)self);

    Py_BEGIN_ALLOW_THREADS;
    pgres = PQexec(pgconn, datestyle);
    Py_END_ALLOW_THREADS;

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_COMMAND_OK ) {
        Dprintf("conn_connect: setting datestyle to iso FAILED");
        PyErr_SetString(OperationalError, "can't set datestyle to ISO");
        PQfinish(pgconn);
        IFCLEARPGRES(pgres);
        return -1;
    }
    CLEARPGRES(pgres);

    Py_BEGIN_ALLOW_THREADS;
    pgres = PQexec(pgconn, encoding);
    Py_END_ALLOW_THREADS;

    if (pgres == NULL || PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        Dprintf("conn_connect: fetching current client_encoding FAILED");
        PyErr_SetString(OperationalError, "can't fetch client_encoding");
        PQfinish(pgconn);
        IFCLEARPGRES(pgres);
        return -1;
    }
    self->encoding = strdup(PQgetvalue(pgres, 0, 0));
    CLEARPGRES(pgres);

    if (PQsetnonblocking(pgconn, 1) != 0) {
        Dprintf("conn_connect: PQsetnonblocking() FAILED");
        PyErr_SetString(OperationalError, "PQsetnonblocking() failed");
        PQfinish(pgconn);
        return -1;
    }

#ifdef HAVE_PQPROTOCOL3
    self->protocol = PQprotocolVersion(pgconn);
#else
    self->protocol = 2;
#endif
    Dprintf("conn_connect: using protocol %d", self->protocol);
    
    self->pgconn = pgconn;
    return 0;
}

/* conn_close - do anything needed to shut down the connection */

void
conn_close(connectionObject *self)
{
    int len, i;
    PyObject *t = NULL;

    /* sets this connection as closed even for other threads; also note that
       we need to check the value of pgconn, because we get called even when
       the connection fails! */
    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    self->closed = 1;

    /* execute a forced rollback on the connection (but don't check the
       result, we're going to close the pq connection anyway */
    if (self->pgconn) pq_abort(self);
    
    /* orphans all the children cursors but do NOT destroy them (note that we
       need to lock the connection before orphaning a cursor: we don't want to
       remove a connection from a cursor executing a DB operation */
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    pthread_mutex_lock(&self->lock);
    len = PyList_Size(self->cursors);
    Dprintf("conn_close: ophaning %d cursors", len);
    for (i = len-1; i >= 0; i--) {
        t = PySequence_GetItem(self->cursors, i);
        Dprintf("conn close:     cursor at %p: refcnt = %d", t, t->ob_refcnt);
        PySequence_DelItem(self->cursors, i);
        ((cursorObject *)t)->conn = NULL; /* orphaned */
        Dprintf("conn_close:     -> new refcnt = %d", t->ob_refcnt);
    }
    pthread_mutex_unlock(&self->lock);
    
    /* now that all cursors have been orphaned (they can't operate on the
       database anymore) we can shut down the connection */
    if (self->pgconn) {
        PQfinish(self->pgconn);
        Dprintf("conn_close: PQfinish called");
        self->pgconn = NULL;
    }
}

/* conn_commit - commit on a connection */

int
conn_commit(connectionObject *self)
{
    int res;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    res = pq_commit(self);

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    return res;
}

/* conn_rollback - rollback a connection */

int
conn_rollback(connectionObject *self)
{
    int res;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);

    res = pq_abort(self);

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    return res;
}

/* conn_switch_isolation_level - switch isolation level on the connection */

int
conn_switch_isolation_level(connectionObject *self, int level)
{
    int res = 0;

    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);
    
    /* if the current isolation level is > 0 we need to abort the current
       transaction before changing; that all folks! */
    if (self->isolation_level != level && self->isolation_level > 0) {
        res = pq_abort(self);
    }
    self->isolation_level = level;

    Dprintf("conn_switch_isolation_level: switched to level %d", level);
    
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    return res;   
}

/* conn_set_client_encoding - switch client encoding on connection */

int
conn_set_client_encoding(connectionObject *self, char *enc)
{
    PGresult *pgres;
    char query[48];
    int res = 0;
    
    /* TODO: check for async query here and raise error if necessary */
    
    Py_BEGIN_ALLOW_THREADS;
    pthread_mutex_lock(&self->lock);
    
    /* set encoding, no encoding string is longer than 24 bytes */
    snprintf(query, 47, "SET client_encoding = '%s'", enc);

    /* abort the current transaction, to set the encoding ouside of
       transactions */
    res = pq_abort(self);

    if (res == 0) {
        pgres = PQexec(self->pgconn, query);

        if (pgres == NULL || PQresultStatus(pgres) != PGRES_COMMAND_OK ) {
            PyErr_Format(OperationalError, "can't set encoding to '%s'", enc);
            res = -1;
        }
        IFCLEARPGRES(pgres);

        if (self->encoding) free(self->encoding);
        self->encoding = strdup(enc);
    }
    
    Dprintf("conn_set_client_encoding: set encoding to %s", self->encoding);
    
    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS;

    return res;   
}
