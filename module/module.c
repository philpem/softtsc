/* SoftTSC - Software MPT1327 Trunking System Controller
* Copyright (C) 2013-2014 Paul Banks (http://paulbanks.org)
* 
* This file is part of SoftTSC
*
* SoftTSC is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* SoftTSC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with SoftTSC.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <Python.h>

#include "channel.h"

typedef struct {
  PyObject_HEAD
  MPT1327Channel* channel;
  PyObject* p_recvfn;
  PyObject* p_txcvfn;
  PyObject* p_userdata;
} MPT1327PyModemObject;

typedef struct {
  PyObject* fcomp;
  PyObject* fcompdata;
} MPT1327PyCompletionContext;

static
void
mpt1327Modem_recv_callback(MPT1327PyModemObject* self, guint64 cw)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject_CallFunction(self->p_recvfn, "OL", self->p_userdata, cw);
  PyGILState_Release(gstate);
}

static
guint64
mpt1327Modem_txcv_callback(MPT1327PyModemObject* self)
{
  guint64 cw = 1;
  PyObject* ret;

  PyGILState_STATE gstate = PyGILState_Ensure();
  ret = PyObject_CallFunction(self->p_txcvfn, "O", self->p_userdata);
  if (ret)
    cw = PyLong_AsLongLong(ret);
  PyGILState_Release(gstate);

  return cw;
}

static
void
mpt1327Modem_compl_callback(MPT1327PyCompletionContext* ctx)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  PyObject_CallFunction(ctx->fcomp, "O", ctx->fcompdata);
  Py_DECREF(ctx->fcomp);
  Py_DECREF(ctx->fcompdata);
  PyGILState_Release(gstate);
  g_free(ctx);
}

static 
PyObject*
mpt1327Modem_start(MPT1327PyModemObject* self, PyObject* args)
{

  int i = mpt1327_channel_start(self->channel);
  return Py_BuildValue("i", i);
}

static 
PyObject*
mpt1327Modem_stop(MPT1327PyModemObject* self, PyObject* args)
{

  int i = mpt1327_channel_stop(self->channel);
  return Py_BuildValue("i", i);
}

static 
PyObject*
mpt1327Modem_tone(MPT1327PyModemObject* self, PyObject* args)
{
  int freq;
  int duration;

  if (!PyArg_ParseTuple(args, "ii", 
                        &freq,
                        &duration)) {
    return NULL;
  }

  return Py_BuildValue("i", 0);
}

static 
PyObject*
mpt1327Modem_morse(MPT1327PyModemObject* self, PyObject* args)
{
  MPT1327PyCompletionContext* compl_ctx;
  const char* morse;
  PyObject* fcomp;
  PyObject* fcompdata;

  if (!PyArg_ParseTuple(args, "sOO",
                        &morse,
                        &fcomp,
                        &fcompdata)) {
    return NULL;
  }

  // Create completion context for callback
  compl_ctx = g_new(MPT1327PyCompletionContext, 1);
  compl_ctx->fcomp = fcomp;
  compl_ctx->fcompdata = fcompdata;
  Py_INCREF(compl_ctx->fcomp);
  Py_INCREF(compl_ctx->fcompdata);

  mpt1327_channel_queue_morse(self->channel, 
                              morse, 
                              (mpt1327_channel_completion_fn)
                                mpt1327Modem_compl_callback, 
                              compl_ctx);
  return Py_BuildValue("i", 0);
}

static 
PyObject*
mpt1327Modem_bridge(MPT1327PyModemObject* self, PyObject* args)
{
  int bridge;

  if (!PyArg_ParseTuple(args, "i", 
                        &bridge)) {
    return NULL;
  }

  mpt1327_channel_bridge(self->channel, bridge);
  return Py_BuildValue("i", 0);
}

static int
mpt1327Modem_traverse(MPT1327PyModemObject *self, visitproc visit, void *arg)
{
  Py_VISIT(self->p_recvfn);
  Py_VISIT(self->p_txcvfn);
  Py_VISIT(self->p_userdata);
  return 0;
}

static int mpt1327Modem_clear(MPT1327PyModemObject* self)
{
  Py_CLEAR(self->p_recvfn);
  Py_CLEAR(self->p_txcvfn);
  Py_CLEAR(self->p_userdata);
  return 0;
}

static void mpt1327Modem_dealloc(MPT1327PyModemObject* self)
{
  mpt1327_channel_stop(self->channel);
  mpt1327_channel_free(&self->channel);


  mpt1327Modem_clear(self);
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static int mpt1327Modem_init(MPT1327PyModemObject* self, 
                               PyObject* args, PyObject* kwds)
{

  char* channelId;

  if (!PyArg_ParseTuple(args, "sOOO", 
                        &channelId,
                        &self->p_recvfn,
                        &self->p_txcvfn,
                        &self->p_userdata))
  {
    return 1;
  }

  Py_INCREF(self->p_recvfn);
  Py_INCREF(self->p_txcvfn);
  Py_INCREF(self->p_userdata);

  return mpt1327_channel_init(&self->channel, channelId,
      (mpt1327_channel_recv_fn)mpt1327Modem_recv_callback,
      (mpt1327_channel_txcv_fn)mpt1327Modem_txcv_callback,
                              self);
}

static PyMethodDef mpt1327ModemMethods[] = {
  {"start", (PyCFunction)mpt1327Modem_start, 
    METH_VARARGS, "Starts the modem"},
  {"stop", (PyCFunction)mpt1327Modem_stop, 
    METH_VARARGS, "Stops the modem"},
  {"tone", (PyCFunction)mpt1327Modem_tone,
    METH_VARARGS, "Sounds a tone"},
  {"morse", (PyCFunction)mpt1327Modem_morse,
    METH_VARARGS, "Morse code broadcast"},
  {"bridge", (PyCFunction)mpt1327Modem_bridge,
    METH_VARARGS, "Bridge rx -> tx"},
  {NULL}
};

static PyTypeObject mpt1327ModemType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "libmpt1327.MPT1327Modem",    /* tp_name */
    sizeof(MPT1327PyModemObject), /* tp_basicsize */
    0,                         /* tp_itemsize */
    0,                         /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,        /* tp_flags */
    "MPT1327 Modem",           /* tp_doc */
};
  
static 
PyObject*
m_fcs(PyObject* self, PyObject* args)
{
  guint64 cw;
  if (!PyArg_ParseTuple(args, "L", &cw))
    return NULL;

  guint32 fcs = mpt1327_channel_fcs(cw);
  return Py_BuildValue("i", fcs);
}

static PyMethodDef MPT1327Methods[] = {
  {"fcs",   m_fcs, METH_VARARGS, "Calculate MPT1327 frame check sequence"},
  {NULL}
};

static PyModuleDef MPT1327Module = {
  PyModuleDef_HEAD_INIT,
  "libmpt1327",
  "MPT1327 Low-Level functionality",
  -1,
  MPT1327Methods
};


PyMODINIT_FUNC
PyInit_libmpt1327modem(void)
{
  PyObject* m;

  PyEval_InitThreads();

  mpt1327ModemType.tp_new = PyType_GenericNew;
  mpt1327ModemType.tp_init = (initproc)mpt1327Modem_init;
  mpt1327ModemType.tp_dealloc = (destructor)mpt1327Modem_dealloc;
  mpt1327ModemType.tp_clear = (inquiry)mpt1327Modem_clear;
  mpt1327ModemType.tp_traverse = (traverseproc)mpt1327Modem_traverse;

  mpt1327ModemType.tp_methods = mpt1327ModemMethods;
  if  (PyType_Ready(&mpt1327ModemType) < 0)
    return NULL;

  m = PyModule_Create(&MPT1327Module);
  if (!m)
    return NULL;

  Py_INCREF(&mpt1327ModemType);
  PyModule_AddObject(m, "MPT1327Modem", (PyObject*)&mpt1327ModemType);
  return m;

}

PyMODINIT_FUNC
initlibmpt1327modem(void)
{
  PyErr_SetString(PyExc_ImportError, 
                  "python < 3 is not yet supported by the native "
                  "MPT1327 module.");
  return NULL;
}

