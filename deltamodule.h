/* 
 * File:   deltamodule.h
 * Author: Michael Winter <mail@michael-winter.me.uk>
 *
 * Created on 23 December 2014, 01:54
 */

#ifndef DELTAMODULE_H
#define	DELTAMODULE_H

#include "config.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <Python.h>

#include "xdelta3.h"
#include "buffer.h"
#include "lru_cache.h"


typedef struct {
    PyObject_HEAD
    PyObject *target;
    
    buffer_t *buffer;
    lru_cache_t *cache;
    
    xd3_stream  stream;
    xd3_source *source;
} xd3py_stream;


PyMODINIT_FUNC init_xdelta(void);

#endif	/* DELTAMODULE_H */
