#include <xdelta3.h>
#include <Python.h>
#include <structmember.h>

#define DEFAULT_BLOCK_SIZE 32768

#define Py_REASSIGN(a, b) \
  do { \
    PyObject *tmp = a; \
    Py_INCREF(b); \
    a = b; \
    Py_XDECREF(tmp); \
  } while (0)
  
/*******************************************************************************
 * xdelta3.Error
 ******************************************************************************/
 
static PyObject *Xdelta3Error;

/*******************************************************************************
 * xdelta3.Stream
 ******************************************************************************/

int _config_xd3_stream(xd3_stream *stream, xoff_t winsize)
{
  xd3_config config;

  memset(&config, 0, sizeof(config));
  xd3_init_config(&config, 0);
  config.winsize = winsize;
  
  if (xd3_config_stream(stream, &config))
  {
    PyErr_SetString(Xdelta3Error, "xd3_config_stream error");
    return 0;
  }
  
  return 1;
}

typedef struct
{
  PyObject_HEAD
  xd3_stream _stream;
} Stream;

static PyMemberDef Stream_members[] = {
  { NULL }
};

static PyMethodDef Stream_methods[] = {
  { NULL }
};

static void
Stream_dealloc(Stream *self)
{
  xd3_free_stream(&self->_stream);
  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
Stream_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  Stream *self;

  self = (Stream *) type->tp_alloc(type, 0);
  
  if (self == NULL)
    return NULL;
  
  if (!_config_xd3_stream(&self->_stream, DEFAULT_BLOCK_SIZE))
  {
    Py_DECREF(self);
    return NULL;
  }
  
  return (PyObject *) self;
}

static int
Stream_init(Stream *self, PyObject *args, PyObject *kwds)
{
  static char *keywords[] = { "winsize", NULL };
  Py_ssize_t block_size = DEFAULT_BLOCK_SIZE;
  
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|n:__init__", keywords,
      &block_size))
    return -1;
  
  xd3_free_stream(&self->_stream);
  if (!_config_xd3_stream(&self->_stream, block_size))
    return -1;
  
  return 0;
}

static PyTypeObject StreamType = {
  PyObject_HEAD_INIT(NULL)
  0,                                        /* ob_size*/
  "xdelta3.Stream",                         /* tp_name*/
  sizeof(Stream),                           /* tp_basicsize*/
  0,                                        /* tp_itemsize*/
  (destructor) Stream_dealloc,              /* tp_dealloc*/
  0,                                        /* tp_print*/
  0,                                        /* tp_getattr*/
  0,                                        /* tp_setattr*/
  0,                                        /* tp_compare*/
  0,                                        /* tp_repr*/
  0,                                        /* tp_as_number*/
  0,                                        /* tp_as_sequence*/
  0,                                        /* tp_as_mapping*/
  0,                                        /* tp_hash */
  0,                                        /* tp_call*/
  0,                                        /* tp_str*/
  0,                                        /* tp_getattro*/
  0,                                        /* tp_setattro*/
  0,                                        /* tp_as_buffer*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
  "Stream objects",                         /* tp_doc */
  0,                                        /* tp_traverse */
  0,                                        /* tp_clear */
  0,                                        /* tp_richcompare */
  0,                                        /* tp_weaklistoffset */
  0,                                        /* tp_iter */
  0,                                        /* tp_iternext */
  Stream_methods,                           /* tp_methods */
  Stream_members,                           /* tp_members */
  0,                                        /* tp_getset */
  0,                                        /* tp_base */
  0,                                        /* tp_dict */
  0,                                        /* tp_descr_get */
  0,                                        /* tp_descr_set */
  0,                                        /* tp_dictoffset */
  (initproc) Stream_init,                   /* tp_init */
  0,                                        /* tp_alloc */
  Stream_new,                               /* tp_new */
};

/*******************************************************************************
 * xdelta3.Xdelta3
 ******************************************************************************/
 
typedef struct
{
  PyObject_HEAD
  
  PyObject *source_reader;
  PyObject *output_writer;
  
  PyObject *source_data;

  xd3_stream stream;
  xd3_source source;
  
  Py_ssize_t block_size;
} Xdelta3;

static PyMemberDef Xdelta3_members[] = {
  { NULL }
};

static PyObject *Xdelta3_input(Xdelta3 *self, PyObject *args)
{
  char *data;
  Py_ssize_t data_len;
  
  if (!PyArg_ParseTuple(args, "s#:input", &data, &data_len))
    return NULL;
  
  xd3_avail_input(&self->stream, (unsigned char *) data, data_len);
  
  while (1)
  {
    int ret = xd3_decode_input(&self->stream);
    
    if (ret == XD3_INPUT)
      Py_RETURN_NONE;
    
    if (ret == XD3_OUTPUT)
    {
      PyObject *result = PyObject_CallFunction(self->output_writer, "s#",
          self->stream.next_out, self->stream.avail_out);
      if (!result)
        return NULL;
      
      Py_DECREF(result);
      xd3_consume_output(&self->stream);
    }
    else if (ret == XD3_GETSRCBLK)
    {
      char *src;
      Py_ssize_t src_len;
      
      PyObject *result = PyObject_CallFunction(self->source_reader, "kn",
          self->source.getblkno, self->block_size);
      if (!result)
        return NULL;
      
      if (PyString_AsStringAndSize(result, &src, &src_len) == -1)
      {
        Py_DECREF(result);
        return NULL;
      }
      
      Py_REASSIGN(self->source_data, result);
      
      self->source.curblkno = self->source.getblkno;
      self->source.onblk = src_len;
      self->source.curblk = (unsigned char *) src;
    }
    else if (ret != XD3_GOTHEADER && ret != XD3_WINSTART && ret != XD3_WINFINISH)
    {
      PyErr_SetString(Xdelta3Error, "xd3_decode_input error");
      return NULL;
    }
  }
}
  
static PyMethodDef Xdelta3_methods[] = {
  { "input", (PyCFunction) Xdelta3_input, METH_VARARGS, NULL },
  { NULL }
};

static void
Xdelta3_dealloc(Xdelta3 *self)
{
  xd3_free_stream(&self->stream);
  
  Py_XDECREF(self->source_reader);
  Py_XDECREF(self->output_writer);
  Py_XDECREF(self->source_data);
  
  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
Xdelta3_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  Xdelta3 *self;

  self = (Xdelta3 *) type->tp_alloc(type, 0);
  
  if (self == NULL)
    return NULL;
  
  xd3_config config;
  
  self->block_size = 32768;
  
  self->source_reader = Py_None;
  Py_INCREF(Py_None);
  self->output_writer = Py_None;
  Py_INCREF(Py_None);
  self->source_data = Py_None;
  Py_INCREF(Py_None);
  
  memset(&config, 0, sizeof(config));
  xd3_init_config(&config, 0);
  config.winsize = self->block_size;
  if (xd3_config_stream(&self->stream, &config))
  {
    Py_DECREF(self);
    PyErr_SetString(Xdelta3Error, "xd3_config_stream error");
    return NULL;
  }
  
  self->source.blksize = self->block_size;
  self->source.curblkno = (xoff_t) -1;
  self->source.curblk = NULL;

  if (xd3_set_source(&self->stream, &self->source))
  {
    Py_DECREF(self);
    PyErr_SetString(Xdelta3Error, "xd3_set_source error");
    return NULL;
  }

  return (PyObject *) self;
}

static int
Xdelta3_init(Xdelta3 *self, PyObject *args, PyObject *kwds)
{
  PyObject *reader, *writer;
  
  if (!PyArg_ParseTuple(args, "OO", &reader, &writer))
    return -1;
  
  Py_REASSIGN(self->source_reader, reader);
  Py_REASSIGN(self->output_writer, writer);
  
  return 0;
}

static PyTypeObject Xdelta3Type = {
  PyObject_HEAD_INIT(NULL)
  0,                                        /* ob_size*/
  "xdelta3.Xdelta3",                        /* tp_name*/
  sizeof(Xdelta3),                          /* tp_basicsize*/
  0,                                        /* tp_itemsize*/
  (destructor) Xdelta3_dealloc,             /* tp_dealloc*/
  0,                                        /* tp_print*/
  0,                                        /* tp_getattr*/
  0,                                        /* tp_setattr*/
  0,                                        /* tp_compare*/
  0,                                        /* tp_repr*/
  0,                                        /* tp_as_number*/
  0,                                        /* tp_as_sequence*/
  0,                                        /* tp_as_mapping*/
  0,                                        /* tp_hash */
  0,                                        /* tp_call*/
  0,                                        /* tp_str*/
  0,                                        /* tp_getattro*/
  0,                                        /* tp_setattro*/
  0,                                        /* tp_as_buffer*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
  "Xdelta3 objects",                        /* tp_doc */
  0,                                        /* tp_traverse */
  0,                                        /* tp_clear */
  0,                                        /* tp_richcompare */
  0,                                        /* tp_weaklistoffset */
  0,                                        /* tp_iter */
  0,                                        /* tp_iternext */
  Xdelta3_methods,                          /* tp_methods */
  Xdelta3_members,                          /* tp_members */
  0,                                        /* tp_getset */
  0,                                        /* tp_base */
  0,                                        /* tp_dict */
  0,                                        /* tp_descr_get */
  0,                                        /* tp_descr_set */
  0,                                        /* tp_dictoffset */
  (initproc) Xdelta3_init,                  /* tp_init */
  0,                                        /* tp_alloc */
  Xdelta3_new,                              /* tp_new */
};

/*******************************************************************************
 * xdelta3 module definitions
 ******************************************************************************/
 
static PyMethodDef xdelta3_methods[] = {
  { NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC init_xdelta3(void)
{
  PyObject *m;

  if (PyType_Ready(&StreamType) < 0)
    return;
  if (PyType_Ready(&Xdelta3Type) < 0)
    return;
  
  m = Py_InitModule("_xdelta3", xdelta3_methods);
  if (m == NULL)
    return;
  
  Py_INCREF(&StreamType);
  PyModule_AddObject(m, "Stream", (PyObject *) &StreamType);
  
  Py_INCREF(&Xdelta3Type);
  PyModule_AddObject(m, "Xdelta3", (PyObject *) &Xdelta3Type);
  
  Xdelta3Error = PyErr_NewException("_xdelta3.Error", NULL, NULL);
  Py_INCREF(Xdelta3Error);
  PyModule_AddObject(m, "Error", Xdelta3Error);
}
