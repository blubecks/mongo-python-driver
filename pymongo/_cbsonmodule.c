/*
 * Copyright 2009 10gen, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file contains C implementations of some of the functions needed by the
 * bson module. If possible, these implementations should be used to speed up
 * BSON encoding and decoding.
 */

#include <Python.h>
#include <string.h>

static PyObject* CBSONError;

static char* shuffle_oid(char* oid) {
  char* shuffled = (char*) malloc(12);
  int i;

  if (!shuffled) {
    PyErr_NoMemory();
    return NULL;
  }

  for (i = 0; i < 8; i++) {
    shuffled[i] = oid[7 - i];
  }
  for (i = 0; i < 4; i++) {
    shuffled[i + 8] = oid[11 - i];
  }

  return shuffled;
}

static PyObject* _cbson_shuffle_oid(PyObject* self, PyObject* args) {
  PyObject* result;
  char* data;
  char* shuffled;
  int length;

  if (!PyArg_ParseTuple(args, "s#", &data, &length)) {
    return NULL;
  }

  if (length != 12) {
    PyErr_SetString(PyExc_ValueError, "oid must be of length 12");
  }

  shuffled = shuffle_oid(data);
  if (!shuffled) {
    return NULL;
  }

  result = Py_BuildValue("s#", shuffled, 12);
  free(shuffled);
  return result;
}

/* TODO our platform better be little-endian w/ 4-byte ints! */
static PyObject* _cbson_element_to_bson(PyObject* self, PyObject* args) {
  const char* name;
  const char* type;
  int name_length;
  PyObject* value;

  if (!PyArg_ParseTuple(args, "s#O", &name, &name_length, &value)) {
    return NULL;
  }

  /* TODO this isn't quite the same as the Python version:
   * here we check for type equivalence, not isinstance. */
  type = value->ob_type->tp_name;

  if (PyUnicode_Check(value)) {
    PyObject* encoded = PyUnicode_AsUTF8String(value);
    if (!encoded) {
      return NULL;
    }
    const char* encoded_bytes = PyString_AsString(encoded);
    if (!encoded_bytes) {
      return NULL;
    }
    int bytes_length = strlen(encoded_bytes) + 1;
    int return_value_length = 5 + name_length + bytes_length;
    char* to_return = (char*)malloc(return_value_length);
    if (!to_return) {
      PyErr_NoMemory();
      return NULL;
    }

    to_return[0] = 0x02;
    memcpy(to_return + 1, name, name_length);
    memcpy(to_return + 1 + name_length, &bytes_length, 4);
    memcpy(to_return + 5 + name_length, encoded_bytes, bytes_length);

    PyObject* result = Py_BuildValue("s#", to_return, return_value_length);
    free(to_return);
    return result;
  }
  PyErr_SetString(CBSONError, "no c encoder for this type yet");
  return NULL;
}

static PyMethodDef _CBSONMethods[] = {
  {"_shuffle_oid", _cbson_shuffle_oid, METH_VARARGS,
   "shuffle an ObjectId into proper byte order."},
  {"_element_to_bson", _cbson_element_to_bson, METH_VARARGS,
   "convert a key and value to its bson representation."},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC init_cbson(void) {
  PyObject *m;
  m = Py_InitModule("_cbson", _CBSONMethods);
  if (m == NULL) {
    return;
  }

  CBSONError = PyErr_NewException("_cbson.error", NULL, NULL);
  Py_INCREF(CBSONError);
  PyModule_AddObject(m, "error", CBSONError);
}