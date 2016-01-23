/* 
 * File:   deltamodule.c
 * Author: Michael Winter <mail@michael-winter.me.uk>
 *
 * Created on 23 December 2014, 01:50
 */

#include "deltamodule.h"

#define MODULE_NAME "_xdelta"
#define STREAM_NAME "Stream"
#define QUALIFIED_NAME(NAME) MODULE_NAME "." NAME
#define MAX_SOURCE_BLOCKS 32
#define SOURCE_BLOCK_SIZE (XD3_DEFAULT_SRCWINSZ / MAX_SOURCE_BLOCKS)


typedef int       (*processing_func) (xd3_stream *);
typedef PyObject *(*input_func) (PyObject *const, const size_t, const size_t);
typedef int       (*output_func) (PyObject **dest, const char *const src, const size_t len);


static PyObject *stream_new(PyTypeObject *, PyObject *, PyObject *);
static int       stream_init(xd3py_stream *, PyObject *, PyObject *);
static void      stream_dealloc(xd3py_stream *);
static PyObject *stream_read(xd3py_stream *, PyObject *, PyObject *);
static PyObject *stream_write(xd3py_stream *, PyObject *, PyObject *);
static PyObject *stream_flush(xd3py_stream *const);

static PyObject *stream_get_source(xd3py_stream *, void *);
static int       stream_set_source(xd3py_stream *, PyObject *, void *);

static PyObject *read_from_file(PyObject *const, const size_t, const size_t);
static PyObject *read_from_string(PyObject *const, const size_t, const size_t);
static int       write_to_file(PyObject **, const char *const, const size_t);
static int       write_to_string(PyObject **, const char *const, const size_t);

static int get_source_block(xd3_stream *, xd3_source *, xoff_t);
static int do_processing(xd3_stream *const, PyObject *const, PyObject **, const Py_ssize_t,
                         input_func, processing_func, output_func, buffer_t **);

static int file_object_check(PyObject *);
static int in_progress(const xd3_stream *const);
static int is_callable(PyObject *, const char *);


static PyGetSetDef stream_accessors[] = {
    {"source", (getter) stream_get_source, (setter) stream_set_source,
            "Specify the source file used for comparison during encoding and decoding.", NULL},
    {NULL}  /* sentinel */
};


static PyMethodDef stream_methods[] = {
    {"read", (PyCFunction) stream_read, METH_VARARGS | METH_KEYWORDS,
            "Read and decode a number of bytes from the stream."},
    {"write", (PyCFunction) stream_write, METH_VARARGS | METH_KEYWORDS,
            "Write and encode the specified data to the stream."},
    {"flush", (PyCFunction) stream_flush, METH_NOARGS,
            "Write any buffered data to the stream."},
    {NULL}  /* sentinel */
};


static PyTypeObject stream_type = {
    PyObject_HEAD_INIT(NULL)
    0,                           /*ob_size*/
    QUALIFIED_NAME(STREAM_NAME), /*tp_name*/
    sizeof(xd3py_stream),        /*tp_basicsize*/
    0,                           /*tp_itemsize*/
    (destructor) stream_dealloc, /*tp_dealloc*/
    0,                           /*tp_print*/
    0,                           /*tp_getattr*/
    0,                           /*tp_setattr*/
    0,                           /*tp_compare*/
    0,                           /*tp_repr*/
    0,                           /*tp_as_number*/
    0,                           /*tp_as_sequence*/
    0,                           /*tp_as_mapping*/
    0,                           /*tp_hash*/
    0,                           /*tp_call*/
    0,                           /*tp_str*/
    0,                           /*tp_getattro*/
    0,                           /*tp_setattro*/
    0,                           /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,          /*tp_flags*/
    "XDelta3 Stream objects",    /*tp_doc*/
    0,		                 /*tp_traverse*/
    0,		                 /*tp_clear*/
    0,		                 /*tp_richcompare*/
    0,		                 /*tp_weaklistoffset*/
    0,		                 /*tp_iter*/
    0,		                 /*tp_iternext*/
    stream_methods,              /*tp_methods*/
    0,                           /*tp_members*/
    stream_accessors,            /*tp_getset*/
    0,                           /*tp_base*/
    0,                           /*tp_dict*/
    0,                           /*tp_descr_get*/
    0,                           /*tp_descr_set*/
    0,                           /*tp_dictoffset*/
    (initproc) stream_init,      /*tp_init*/
    0,                           /*tp_alloc*/
    stream_new,                  /*tp_new*/
};


static PyMethodDef module_functions[] = {
    {NULL}  /* sentinel */
};


/**
 * Create an instance of a stream object.
 * 
 * Takes ownership of an instance of None.
 * 
 * @param type a pointer to the type of object being instantiated; stream_type.
 * @param args a pointer to a tuple containing position arguments passed during
 *             invocation.
 * @param kwds a pointer to a dictionary of named arguments passed during
 *             invocation.
 * @return a pointer to the new instance on success; NULL otherwise.
 */
static PyObject *stream_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    xd3py_stream *self = (xd3py_stream *) type->tp_alloc(type, 0);
    if (self != NULL) {
        Py_INCREF(Py_None);
        self->target = Py_None;
    }
    return (PyObject *) self;
}


/**
 * Initialise a new instance of a stream object.
 * 
 * Takes ownership of objects passed as the target and source arguments (or
 * None if they were omitted).
 * 
 * @param self a pointer to an allocated stream instance.
 * @param args a pointer to a tuple containing the position arguments "target"
 *             and "source" passed during invocation. Both are file-like
 *             objects (they have, at a minimum, read and write methods) and
 *             are optional.
 * @param kwds a pointer to a dictionary optionally containing the named
 *             arguments "target" and "source" passed during invocation.
 * @return 0 on success; -1 otherwise.
 */
static int stream_init(xd3py_stream *self, PyObject *args, PyObject *kwds) {
    PyObject *target = NULL;
    PyObject *source = NULL;
    static char *kwlist[] = {"target", "source", NULL};
    xd3_config config;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OO", kwlist, &target, &source))
        return -1;

    xd3_init_config(&config, XD3_SEC_DJW | XD3_ADLER32 | XD3_COMPLEVEL_9);
    config.sec_data.ngroups = 0;
    config.sec_inst.ngroups = 1;
    config.sec_addr.ngroups = 1;
    config.smatch_cfg = XD3_SMATCH_SLOW;
    config.iopt_size = XD3_DEFAULT_IOPT_SIZE;
    config.getblk = get_source_block;
    config.opaque = self;

    if (target) {
        PyObject *temp;
        if ((target != Py_None) && (file_object_check(target) == -1))
            return -1;
        temp = self->target;
        Py_INCREF(target);
        self->target = target;
        Py_XDECREF(temp);
    }

    if (xd3_config_stream(&self->stream, &config) != 0)
        return -1;

    self->cache = lru_cache_init(MAX_SOURCE_BLOCKS, SOURCE_BLOCK_SIZE);
    if (source && (stream_set_source(self, source, NULL) == -1))
        return -1;

    return 0;
}


/**
 * Release resources consumed by given stream instance.
 * 
 * Frees references to the "target" and "source" objects (or their None
 * substitutes).
 * 
 * @param self a pointer to the instance to release.
 */
static void stream_dealloc(xd3py_stream *self) {
    Py_XDECREF(self->target);
    
    if (self->source != NULL) {
        Py_XDECREF(self->source->ioh);
        free(self->source);
        self->source = NULL;
    }
    lru_cache_free(self->cache);
    buffer_release(&self->buffer);
    xd3_free_stream(&self->stream);

    self->ob_type->tp_free((PyObject *) self);
}


/**
 * Read and decode compressed data from the target file. If a source file has
 * been provided, it will be used as a reference.
 * 
 * Ownership of the returned string is passed to the caller.
 * 
 * @param self a pointer to the stream instance from which data is read.
 * @param args a pointer to a tuple that may contain the positional argument
 *             num_bytes, which is an upper limit on the number of bytes
 *             returned by the call. If omitted or -1, all available data is
 *             returned.
 * @param kwds a pointer to a dictionary that may contain the keyword argument
 *             num_bytes.
 * @return a pointer to a PyObject containing the read data on success; NULL
 *         otherwise.
 */
static PyObject *stream_read(xd3py_stream *self, PyObject *args, PyObject *kwds) {
    static char *kwlist[] = {"num_bytes", NULL};
    Py_ssize_t allocated = buffer_remaining(self->buffer);
    PyObject *result = NULL;
    Py_ssize_t wanted = -1;
    ulong consumed;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|n", kwlist, &wanted))
        return NULL;
    if ((wanted != -1) && (wanted < allocated))
        allocated = wanted;
    if (!(result = PyString_FromStringAndSize(NULL, allocated)))
        return NULL;
    consumed = buffer_take(self->buffer, PyString_AS_STRING(result), (ulong) allocated);
    
    if (!do_processing(&self->stream, self->target, &result, wanted - consumed, read_from_file,
            xd3_decode_input, write_to_string, &self->buffer))
        Py_CLEAR(result);
    
    return result;
}

/**
 * Encode and write compressed data to the target file. If a source file has
 * been provided, it will be used as a reference.
 * 
 * The object passed as content is borrowed. Ownership of the returned None
 * object is passed to the caller.
 * 
 * @param self a pointer to the stream instance to which data is written.
 * @param args a pointer to a tuple that may contain the positional argument
 *             content, which is a string of data to encode. If the object is
 *             not a string, it is coerced; None is equivalent to the empty
 *             string.
 * @param kwds a pointer to a dictionary that may contain the keyword argument
 *             content.
 * @return a pointer to None on success; NULL otherwise.
 */
static PyObject *stream_write(xd3py_stream *self, PyObject *args, PyObject *kwds) {
    static char *kwlist[] = {"content", NULL};
    PyObject *object = NULL;
    PyObject *ret = NULL;
    PyObject *string = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &object))
        return NULL;
    string = (object != Py_None) ? PyObject_Str(object) : PyString_FromString("");
    if (string == NULL)
        goto exit;

    if (do_processing(&self->stream, string, &self->target, -1, read_from_string, xd3_encode_input,
            write_to_file, NULL)) {
        Py_INCREF(Py_None);
        ret = Py_None;
    }
    
exit:
    Py_XDECREF(string);
    return ret;
}


static PyObject *stream_flush(xd3py_stream *const self) {
    if (self->stream.enc_state != ENC_INIT) {
        PyObject *args = Py_BuildValue("(O)", Py_None);
        PyObject *ret;
        if (args == NULL)
            return NULL;
        
        self->stream.flags |= XD3_FLUSH;
        ret = stream_write(self, args, NULL);
        self->stream.flags ^= XD3_FLUSH;
        Py_DECREF(Py_None);
        return ret;
    }
    Py_RETURN_NONE;
}


static PyObject *stream_get_source(xd3py_stream *self, void *closure) {
    (void) closure;
    
    if (self->source != NULL) {
        Py_INCREF(self->source->ioh);
        return (PyObject *) self->source->ioh;
    }
    Py_RETURN_NONE;
}


static int stream_set_source(xd3py_stream *self, PyObject *value, void *closure) {
    PyObject *temp;
    (void) closure;

    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the source attribute");
        return -1;
    }
    if (in_progress(&self->stream)) {
        PyErr_SetString(PyExc_AttributeError,
                "Cannot alter the source during encoding or decoding");
        return -1;
    }
    if ((value != Py_None) && (file_object_check(value) == -1))
        return -1;

    if (self->source == NULL) {
        self->source = calloc(1, sizeof(xd3_source));
        if (self->source == NULL)
            return -1;
        
        self->source->max_winsize = SOURCE_BLOCK_SIZE * MAX_SOURCE_BLOCKS;
        self->source->blksize = SOURCE_BLOCK_SIZE;
    }

    temp = self->source->ioh;
    Py_INCREF(value);
    self->source->ioh = value;
    Py_XDECREF(temp);
    
    if (value != Py_None)
        xd3_set_source(&self->stream, self->source);
    else
        self->stream.src = NULL;
    
    return 0;
}


static int get_source_block(xd3_stream *stream, xd3_source *source, xoff_t block) {
    lru_cache_t *const cache = ((xd3py_stream *) stream->opaque)->cache;
    const lru_cache_entry_t *entry = lru_cache_get(cache, (ulong) block);
	
    if (entry == NULL) {
        const lru_cache_entry_t *recent = lru_cache_last(cache);
        ulong id = (recent != NULL) ? recent->id + 1 : 0;
        PyObject *data = NULL;
        
        for (; id <= block; id++) {
            data = read_from_file(source->ioh, -1, SOURCE_BLOCK_SIZE);
            if (data != NULL) {
                char *bytes;
                Py_ssize_t length;
                if (PyString_AsStringAndSize(data, &bytes, &length) == -1)
                    break;
                entry = lru_cache_put(cache, id, bytes, (ulong) length);
                Py_CLEAR(data);
            } else
                break;
        }
        
        Py_XDECREF(data);
        if ((entry != NULL) && (entry->id != block))
            entry = NULL;
    }
    if (entry != NULL) {
        source->curblkno = entry->id;
        source->onblk = entry->size;
        source->curblk = (uint8_t *) entry->data;
        return 0;
    }
    return XD3_TOOFARBACK;
}


static PyObject *read_from_file(PyObject *const file, const size_t offset,
        const size_t max_length) {
    PyObject *const block = PyObject_CallMethod(file, "read", "n", max_length);
    if ((block == NULL) || !PyString_Check(block)) {
        Py_XDECREF(block);
        return NULL;
    }
    return block;
}


static PyObject *read_from_string(PyObject *const str, const size_t offset,
        const size_t max_length) {
    PyObject *result = PySequence_GetSlice(str, offset, offset + max_length);
    return result;
}


static int write_to_file(PyObject **dest, const char *const src, const size_t len) {
    PyObject *const result = PyObject_CallMethod(*dest, "write", "s#", src, len);
    if (result == NULL)
        return 0;
    Py_DECREF(result);
    return 1;
}


static int write_to_string(PyObject **dest, const char *const src, const size_t len) {
    const Py_ssize_t pos = PyString_GET_SIZE(*dest);
    // Cannot resize an empty string as it is shared.
    if (pos == 0) {
        Py_DECREF(*dest);
        if ((*dest = PyString_FromStringAndSize(NULL, len)) == NULL)
            return 0;
    } else if (_PyString_Resize(dest, pos + len) == -1)
        return 0;
    memcpy(PyString_AS_STRING(*dest) + pos, src, len);
    return 1;
}


static int do_processing(xd3_stream *const stream, PyObject *const src, PyObject **dest,
        const Py_ssize_t wanted, input_func input, processing_func process, output_func output,
        buffer_t **buffer) {
    PyObject *data = NULL;
    Py_ssize_t read = 0;
    size_t remaining = (wanted >= 0) ? wanted : PY_SSIZE_T_MAX;
    int ret = 0;
    size_t total_read = 0;
    const usize_t window_len = stream->winsize;
    
    do {
        Py_ssize_t available;
        char *bytes;
        if (((data = input(src, total_read, window_len)) == NULL)
                || (PyString_AsStringAndSize(data, &bytes, &read) == -1))
            goto exit;
        total_read += read;
        xd3_avail_input(stream, (uint8_t *) bytes, (usize_t) read);
        
repeat_processing:
        switch(process(stream)) {
            case XD3_INPUT:
                Py_CLEAR(data);
                continue;
            case XD3_OUTPUT:
                available = min(stream->avail_out, remaining);
                if (!output(dest, (char *) stream->next_out, available))
                    goto exit;
                remaining -= available;
                if (available != stream->avail_out)
                    buffer_append(buffer, (char *) stream->next_out + available,
                            (ulong) (stream->avail_out - available));
                xd3_consume_output(stream);
                /* Fall through */
            case XD3_WINSTART:
                /* Fall through */
            case XD3_WINFINISH:
                /* Fall through */
            case XD3_GOTHEADER:
                goto repeat_processing;
            case ENOMEM:
                PyErr_NoMemory();
                goto exit;
            default:
                PyErr_SetString(PyExc_IOError, stream->msg);
                goto exit;
        }
    } while ((read == window_len) && (remaining != 0));
    ret = 1;
    
exit:
    Py_XDECREF(data);
    return ret;
}


static int file_object_check(PyObject *object) {
    if (!is_callable(object, "read") || !is_callable(object, "write")) {
        PyErr_SetString(PyExc_TypeError, "object must be file-like");
        return -1;
    }
    return 0;
}


static int in_progress(const xd3_stream *const stream) {
    return (stream->dec_state >= DEC_APPLEN) || (stream->enc_state != ENC_INIT);
}


static int is_callable(PyObject *object, const char *attr) {
    int callable = 0;

    if (PyObject_HasAttrString(object, attr)) {
        PyObject *method = PyObject_GetAttrString(object, attr);

        if (method != NULL) {
            callable = PyCallable_Check(method);
            Py_DECREF(method);
        }
    }
    return callable;
}


PyMODINIT_FUNC init_xdelta(void) {
    PyObject *module;
    if (PyType_Ready(&stream_type) < 0)
        return;

    module = Py_InitModule3(MODULE_NAME, module_functions,
            "Example module that creates an extension type.");
    if (module == NULL)
        return;

    Py_INCREF(&stream_type);
    PyModule_AddObject(module, STREAM_NAME, (PyObject *) &stream_type);
}
