#include <xdelta3.h>
#include <Python.h>
#include <structmember.h>

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
 * xdelta3.Xdelta3
 ******************************************************************************/
 
typedef struct
{
  PyObject_HEAD
  
  PyObject *source_reader;
  PyObject *output_writer;
  
  xd3_stream stream;
  xd3_source source;
  
  int block_size;
  char *source_buffer;
} Xdelta3;

static PyMemberDef Xdelta3_members[] = {
  { NULL }
};

static PyObject *Xdelta3_test(Xdelta3 *self)
{
  Py_RETURN_NONE;
}
  
static PyMethodDef Xdelta3_methods[] = {
  { "test", (PyCFunction) Xdelta3_test, METH_NOARGS, NULL },
  { NULL }
};

static void
Xdelta3_dealloc(Xdelta3 *self)
{
  xd3_free_stream(&self->stream);
  
  Py_XDECREF(self->source_reader);
  Py_XDECREF(self->output_writer);
  
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
  
  self->source_buffer = PyMem_Malloc(self->block_size);
  if (self->source_buffer == NULL)
  {
    Py_DECREF(self);
    PyErr_NoMemory();
    return NULL;
  }
  
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

PyMODINIT_FUNC initxdelta3(void)
{
  PyObject *m;

  if (PyType_Ready(&Xdelta3Type) < 0)
    return;
  
  m = Py_InitModule("xdelta3", xdelta3_methods);
  if (m == NULL)
    return;
  
  Py_INCREF(&Xdelta3Type);
  PyModule_AddObject(m, "Xdelta3", (PyObject *) &Xdelta3Type);
  
  Xdelta3Error = PyErr_NewException("xdelta3.Error", NULL, NULL);
  Py_INCREF(Xdelta3Error);
  PyModule_AddObject(m, "Error", Xdelta3Error);
}
