#include <Python.h>

static PyObject *xdelta3_test(PyObject *self)
{
  Py_RETURN_NONE;
}
  
static PyMethodDef xdelta3_methods[] = {
  { "test", xdelta3_test, METH_NOARGS, "Test" },
  { NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC initxdelta3(void)
{
  PyObject *m;

  m = Py_InitModule("xdelta3", xdelta3_methods);
  if (m == NULL)
    return;
}
