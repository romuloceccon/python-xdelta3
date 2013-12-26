#include <xdelta3.h>
#include <Python.h>
#include <structmember.h>

#if SIZEOF_XOFF_T == 8
#  define PyLong_FromXoff_t PyLong_FromUnsignedLongLong
#  define PyLong_AsXoff_t PyLong_AsUnsignedLongLong
#  define FMT_Xoff_t "K"
#elif SIZEOF_XOFF_T == 4
#  define PyLong_FromXoff_t PyLong_FromUnsignedLong
#  define PyLong_AsXoff_t PyLong_AsUnsignedLong
#  define FMT_Xoff_t "k"
#else
#  error "Invalid SIZEOF_XOFF_T definition"
#endif

#define Py_REASSIGN(a, b) \
  do { \
    PyObject *tmp = (void *) a; \
    Py_INCREF(b); \
    a = (void *) b; \
    Py_DECREF(tmp); \
  } while (0)
  
/*******************************************************************************
 * xdelta3.Error
 ******************************************************************************/
 
static PyObject *Error;

/*******************************************************************************
 * xdelta3.Source
 ******************************************************************************/

typedef struct
{
  PyObject_HEAD
  xd3_source source;
  PyObject *block_data;
} Source;

static PyMemberDef Source_members[] = {
  { NULL }
};

static PyObject *
Source_set_curblk(Source *self, PyObject *args)
{
  xoff_t block_no;
  PyObject *data;
  
  if (!PyArg_ParseTuple(args, FMT_Xoff_t "O!:set_curblk", &block_no,
      &PyBytes_Type, &data))
    return NULL;
    
  self->source.curblkno = block_no;
  self->source.onblk = PyBytes_GET_SIZE(data);
  self->source.curblk = (unsigned char *) PyBytes_AS_STRING(data);
  
  Py_REASSIGN(self->block_data, data);
  
  Py_RETURN_NONE;
}

static PyMethodDef Source_methods[] = {
  { "set_curblk", (PyCFunction) Source_set_curblk, METH_VARARGS, NULL },
  { NULL }
};

static PyObject *
Source_getblkno(Source *self, void *closure)
{
  return PyLong_FromXoff_t(self->source.getblkno);
}

static PyGetSetDef Source_getset[] = {
  { "getblkno", (getter) Source_getblkno, NULL, NULL, NULL },
  { NULL }
};

static int
Source_traverse(Source *self, visitproc visit, void *arg)
{
  Py_VISIT(self->block_data);
  return 0;
}

static int
Source_clear(Source *self)
{
  Py_CLEAR(self->block_data);
  return 0;
}

static void
Source_dealloc(Source *self)
{
  Source_clear(self);
  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
Source_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  Source *self;

  self = (Source *) type->tp_alloc(type, 0);
  
  if (self == NULL)
    return NULL;
  
  self->source.blksize = XD3_DEFAULT_SRCWINSZ;
  self->source.max_winsize = XD3_DEFAULT_SRCWINSZ;
  self->source.curblkno = (xoff_t) -1;
  
  self->block_data = Py_None;
  Py_INCREF(Py_None);
  
  return (PyObject *) self;
}

static int
Source_init(Source *self, PyObject *args, PyObject *kwds)
{
  static char *keywords[] = { "winsize", "max_winsize", NULL };
  xoff_t winsize = XD3_DEFAULT_SRCWINSZ, max_winsize = XD3_DEFAULT_SRCWINSZ;
  
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|" FMT_Xoff_t FMT_Xoff_t \
      ":__init__", keywords, &winsize, &max_winsize))
    return -1;
  
  self->source.blksize = winsize;
  self->source.max_winsize = max_winsize;
  
  return 0;
}

static PyTypeObject SourceType = {
  PyObject_HEAD_INIT(NULL)
  0,                                        /* ob_size*/
  "_xdelta3.Source",                        /* tp_name*/
  sizeof(Source),                           /* tp_basicsize*/
  0,                                        /* tp_itemsize*/
  (destructor) Source_dealloc,              /* tp_dealloc*/
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
      Py_TPFLAGS_HAVE_GC,                   /* tp_flags*/
  "Source objects",                         /* tp_doc */
  (traverseproc) Source_traverse,           /* tp_traverse */
  (inquiry) Source_clear,                   /* tp_clear */
  0,                                        /* tp_richcompare */
  0,                                        /* tp_weaklistoffset */
  0,                                        /* tp_iter */
  0,                                        /* tp_iternext */
  Source_methods,                           /* tp_methods */
  Source_members,                           /* tp_members */
  Source_getset,                            /* tp_getset */
  0,                                        /* tp_base */
  0,                                        /* tp_dict */
  0,                                        /* tp_descr_get */
  0,                                        /* tp_descr_set */
  0,                                        /* tp_dictoffset */
  (initproc) Source_init,                   /* tp_init */
  0,                                        /* tp_alloc */
  Source_new,                               /* tp_new */
};

/*******************************************************************************
 * xdelta3.Stream
 ******************************************************************************/

typedef struct
{
  PyObject_HEAD
  xd3_stream stream;
  PyObject *source;
} Stream;

static int _config_stream(xd3_stream *stream, usize_t winsize, int flags)
{
  xd3_config config;

  memset(&config, 0, sizeof(config));
  xd3_init_config(&config, 0);
  config.winsize = winsize;
  config.flags = flags;
  
  if (xd3_config_stream(stream, &config))
  {
    PyErr_SetString(Error, "xd3_config_stream error");
    return 0;
  }
  
  return 1;
}

static PyObject *
_xxcode_input(Stream *stream, int (*func)(xd3_stream *stream),
    char const* func_name)
{
  int r = func(&stream->stream);
  
  switch (r)
  {
  case XD3_INPUT:
  case XD3_OUTPUT:
  case XD3_GETSRCBLK:
  case XD3_GOTHEADER:
  case XD3_WINSTART:
  case XD3_WINFINISH:
    return PyLong_FromLong(r);
  default:
    PyErr_Format(Error, "%s failed (%d)", func_name, r);
    return NULL;
  }
}

static PyMemberDef Stream_members[] = {
  { NULL }
};

static PyObject *
Stream_set_source(Stream *self, PyObject *args)
{
  Source *source;
  
  if (!PyArg_ParseTuple(args, "O!:set_source", &SourceType, &source))
    return NULL;
  
  if (xd3_set_source(&self->stream, &source->source))
  {
    PyErr_SetString(Error, "xd3_set_source error");
    return NULL;
  }
  
  Py_REASSIGN(self->source, source);
  
  Py_RETURN_NONE;
}

static PyObject *
Stream_avail_input(Stream *self, PyObject *args)
{
  char *data;
  Py_ssize_t len;
  
  if (!PyArg_ParseTuple(args, "s#:avail_input", &data, &len))
    return NULL;
  
  xd3_avail_input(&self->stream, (unsigned char *) data, len);
  
  Py_RETURN_NONE;
}

static PyObject *
Stream_encode_input(Stream *self)
{
  return _xxcode_input(self, xd3_encode_input, "xd3_encode_input");
}

static PyObject *
Stream_decode_input(Stream *self)
{
  return _xxcode_input(self, xd3_decode_input, "xd3_decode_input");
}

static PyObject *
Stream_consume_output(Stream *self)
{
  xd3_consume_output(&self->stream);
  
  Py_RETURN_NONE;
}

static PyMethodDef Stream_methods[] = {
  { "set_source", (PyCFunction) Stream_set_source, METH_VARARGS, NULL },
  { "avail_input", (PyCFunction) Stream_avail_input, METH_VARARGS, NULL },
  { "encode_input", (PyCFunction) Stream_encode_input, METH_NOARGS, NULL },
  { "decode_input", (PyCFunction) Stream_decode_input, METH_NOARGS, NULL },
  { "consume_output", (PyCFunction) Stream_consume_output, METH_NOARGS, NULL },
  { NULL }
};

static PyObject *
Stream_flags(Stream *self, void *closure)
{
  return PyInt_FromLong(self->stream.flags);
}

static int
Stream_set_flags(Stream *self, PyObject *value, void *closure)
{
  int val = PyInt_AsLong(value);
  if (val == -1 && PyErr_Occurred())
    return -1;
  self->stream.flags = val;
  return 0;
}

static PyObject *
Stream_src(Stream *self, void *closure)
{
  Py_INCREF(self->source);
  return self->source;
}

static PyObject *
Stream_next_out(Stream *self, void *closure)
{
  return PyBytes_FromStringAndSize((char *) self->stream.next_out,
      self->stream.avail_out);
}

static PyGetSetDef Stream_getset[] = {
  { "flags", (getter) Stream_flags, (setter) Stream_set_flags, NULL, NULL },
  { "src", (getter) Stream_src, NULL, NULL, NULL },
  { "next_out", (getter) Stream_next_out, NULL, NULL, NULL },
  { NULL }
};

static int
Stream_traverse(Stream *self, visitproc visit, void *arg)
{
  Py_VISIT(self->source);
  return 0;
}

static int
Stream_clear(Stream *self)
{
  Py_CLEAR(self->source);
  return 0;
}

static void
Stream_dealloc(Stream *self)
{
  Stream_clear(self);
  xd3_free_stream(&self->stream);
  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
Stream_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  Stream *self;

  self = (Stream *) type->tp_alloc(type, 0);
  
  if (self == NULL)
    return NULL;
  
  if (!_config_stream(&self->stream, XD3_DEFAULT_WINSIZE, 0))
  {
    Py_DECREF(self);
    return NULL;
  }
  
  self->source = Py_None;
  Py_INCREF(Py_None);
  
  return (PyObject *) self;
}

static int
Stream_init(Stream *self, PyObject *args, PyObject *kwds)
{
  static char *keywords[] = { "winsize", "flags", NULL };
  usize_t winsize = XD3_DEFAULT_WINSIZE;
  int flags = 0;
  
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ki:__init__", keywords,
      &winsize, &flags))
    return -1;
  
  xd3_free_stream(&self->stream);
  if (!_config_stream(&self->stream, winsize, flags))
    return -1;
  
  return 0;
}

static PyTypeObject StreamType = {
  PyObject_HEAD_INIT(NULL)
  0,                                        /* ob_size*/
  "_xdelta3.Stream",                        /* tp_name*/
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
      Py_TPFLAGS_HAVE_GC,                   /* tp_flags*/
  "Stream objects",                         /* tp_doc */
  (traverseproc) Stream_traverse,           /* tp_traverse */
  (inquiry) Stream_clear,                   /* tp_clear */
  0,                                        /* tp_richcompare */
  0,                                        /* tp_weaklistoffset */
  0,                                        /* tp_iter */
  0,                                        /* tp_iternext */
  Stream_methods,                           /* tp_methods */
  Stream_members,                           /* tp_members */
  Stream_getset,                            /* tp_getset */
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
 * xdelta3 module definitions
 ******************************************************************************/
 
static PyMethodDef _xdelta3_methods[] = {
  { NULL, NULL, 0, NULL }
};

#define INIT_LONG_CONSTANT(x) \
    _xdelta3_ ## x = PyLong_FromLong(XD3_ ## x); \
    Py_INCREF(_xdelta3_ ## x); \
    PyModule_AddObject(m, #x, _xdelta3_ ## x);

static PyObject *_xdelta3_INPUT;
static PyObject *_xdelta3_OUTPUT;
static PyObject *_xdelta3_GETSRCBLK;
static PyObject *_xdelta3_GOTHEADER;
static PyObject *_xdelta3_WINSTART;
static PyObject *_xdelta3_WINFINISH;

static PyObject *_xdelta3_JUST_HDR;
static PyObject *_xdelta3_SKIP_WINDOW;
static PyObject *_xdelta3_SKIP_EMIT;
static PyObject *_xdelta3_FLUSH;
static PyObject *_xdelta3_SEC_DJW;
static PyObject *_xdelta3_SEC_FGK;
static PyObject *_xdelta3_SEC_LZMA;
static PyObject *_xdelta3_SEC_TYPE;
static PyObject *_xdelta3_SEC_NODATA;
static PyObject *_xdelta3_SEC_NOINST;
static PyObject *_xdelta3_SEC_NOADDR;
static PyObject *_xdelta3_SEC_NOALL;
static PyObject *_xdelta3_ADLER32;
static PyObject *_xdelta3_ADLER32_NOVER;
static PyObject *_xdelta3_ALT_CODE_TABLE;
static PyObject *_xdelta3_NOCOMPRESS;
static PyObject *_xdelta3_BEGREEDY;
static PyObject *_xdelta3_ADLER32_RECODE;
static PyObject *_xdelta3_COMPLEVEL_1;
static PyObject *_xdelta3_COMPLEVEL_2;
static PyObject *_xdelta3_COMPLEVEL_3;
static PyObject *_xdelta3_COMPLEVEL_6;
static PyObject *_xdelta3_COMPLEVEL_9;

PyMODINIT_FUNC init_xdelta3(void)
{
  PyObject *m;

  if (PyType_Ready(&SourceType) < 0)
    return;
  if (PyType_Ready(&StreamType) < 0)
    return;
  
  m = Py_InitModule("_xdelta3", _xdelta3_methods);
  if (m == NULL)
    return;
  
  Py_INCREF(&SourceType);
  PyModule_AddObject(m, "Source", (PyObject *) &SourceType);
  
  Py_INCREF(&StreamType);
  PyModule_AddObject(m, "Stream", (PyObject *) &StreamType);
  
  Error = PyErr_NewException("_xdelta3.Error", NULL, NULL);
  Py_INCREF(Error);
  PyModule_AddObject(m, "Error", Error);
  
  INIT_LONG_CONSTANT(INPUT);
  INIT_LONG_CONSTANT(OUTPUT);
  INIT_LONG_CONSTANT(GETSRCBLK);
  INIT_LONG_CONSTANT(GOTHEADER);
  INIT_LONG_CONSTANT(WINSTART);
  INIT_LONG_CONSTANT(WINFINISH);
  
  INIT_LONG_CONSTANT(JUST_HDR);
  INIT_LONG_CONSTANT(SKIP_WINDOW);
  INIT_LONG_CONSTANT(SKIP_EMIT);
  INIT_LONG_CONSTANT(FLUSH);
  INIT_LONG_CONSTANT(SEC_DJW);
  INIT_LONG_CONSTANT(SEC_FGK);
  INIT_LONG_CONSTANT(SEC_LZMA);
  INIT_LONG_CONSTANT(SEC_TYPE);
  INIT_LONG_CONSTANT(SEC_NODATA);
  INIT_LONG_CONSTANT(SEC_NOINST);
  INIT_LONG_CONSTANT(SEC_NOADDR);
  INIT_LONG_CONSTANT(SEC_NOALL);
  INIT_LONG_CONSTANT(ADLER32);
  INIT_LONG_CONSTANT(ADLER32_NOVER);
  INIT_LONG_CONSTANT(ALT_CODE_TABLE);
  INIT_LONG_CONSTANT(NOCOMPRESS);
  INIT_LONG_CONSTANT(BEGREEDY);
  INIT_LONG_CONSTANT(ADLER32_RECODE);
  INIT_LONG_CONSTANT(COMPLEVEL_1);
  INIT_LONG_CONSTANT(COMPLEVEL_2);
  INIT_LONG_CONSTANT(COMPLEVEL_3);
  INIT_LONG_CONSTANT(COMPLEVEL_6);
  INIT_LONG_CONSTANT(COMPLEVEL_9);
}
