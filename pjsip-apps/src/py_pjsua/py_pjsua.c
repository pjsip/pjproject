/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <Python.h>
#include "structmember.h"
#include <pjsua-lib/pjsua.h>

#define THIS_FILE    "main.c"

static PyObject* obj_reconfigure_logging;
static PyObject* obj_logging_init;

/*
 * cb_reconfigure_logging
 * declares method for reconfiguring logging process for callback struct
 */
static void cb_reconfigure_logging(int level, const char *data, pj_size_t len)
{
    if (PyCallable_Check(obj_reconfigure_logging))
    {
        PyObject_CallFunctionObjArgs(
            obj_reconfigure_logging, Py_BuildValue("i",level),
            PyString_FromString(data), Py_BuildValue("i",len), NULL
        );
    }
}


/*
 * cb_logging_init
 * declares method logging_init for callback struct
 */
static void cb_logging_init(int level, const char *data, pj_size_t len)
{
    if (PyCallable_Check(obj_logging_init))
    {
        //PyObject_CallFunction(obj_logging_init,"iSi",level,data,len);
        //printf("level : %d data : %s len : %d\n",level, data, len);
        PyObject_CallFunctionObjArgs(
            obj_logging_init, Py_BuildValue("i",level),
            PyString_FromString(data), Py_BuildValue("i",len), NULL
        );
    }
}


/*
 * pjsip_event_Object
 * C/python typewrapper for event struct
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_event * event;
} pjsip_event_Object;


/*
 * pjsip_event_Type
 * event struct signatures
 */
static PyTypeObject pjsip_event_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                          /*ob_size*/
    "py_pjsua.PJSIP_Event",     /*tp_name*/
    sizeof(pjsip_event_Object), /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    0,                          /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,         /*tp_flags*/
    "pjsip_event objects",      /*tp_doc */
};


/*
 * pjsip_rx_data_Object
 * C/python typewrapper for RX data struct
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_rx_data * rdata;
} pjsip_rx_data_Object;


/*
 * pjsip_rx_data_Type
 */
static PyTypeObject pjsip_rx_data_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJSIP_RX_Data",       /*tp_name*/
    sizeof(pjsip_rx_data_Object),   /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,                              /*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "pjsip_rx_data objects",        /*tp_doc*/
};


/*
 * callback_Object
 * C/python typewrapper for callback struct
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    PyObject * on_call_state;
    PyObject * on_incoming_call;
    PyObject * on_call_media_state;
    PyObject * on_call_transfer_request;
    PyObject * on_call_transfer_status;
    PyObject * on_call_replace_request;
    PyObject * on_call_replaced;
    PyObject * on_reg_state;
    PyObject * on_buddy_state;
    PyObject * on_pager;
    PyObject * on_pager_status;
    PyObject * on_typing;

} callback_Object;


/*
 * The global callback object.
 */
static callback_Object * obj_callback;


/*
 * cb_on_call_state
 * declares method on_call_state for callback struct
 */
static void cb_on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    if (PyCallable_Check(obj_callback->on_call_state))
    {
	pjsip_event_Object * obj =
	    (pjsip_event_Object *)PyType_GenericNew(&pjsip_event_Type, 
						    NULL, NULL);
	obj->event = e;

        PyObject_CallFunctionObjArgs(
            obj_callback->on_call_state,Py_BuildValue("i",call_id),obj,NULL
        );
    }
}


/*
 * cb_on_incoming_call
 * declares method on_incoming_call for callback struct
 */
static void cb_on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                                pjsip_rx_data *rdata)
{
    if (PyCallable_Check(obj_callback->on_incoming_call))
    {
	pjsip_rx_data_Object * obj = (pjsip_rx_data_Object *)
				      PyType_GenericNew(&pjsip_rx_data_Type, 
							NULL, NULL);
	obj->rdata = rdata;

        PyObject_CallFunctionObjArgs(
                obj_callback->on_incoming_call,
		Py_BuildValue("i",acc_id),
                Py_BuildValue("i",call_id),
		obj,
		NULL
        );
    }
}


/*
 * cb_on_call_media_state
 * declares method on_call_media_state for callback struct
 */
static void cb_on_call_media_state(pjsua_call_id call_id)
{
    if (PyCallable_Check(obj_callback->on_call_media_state))
    {
        PyObject_CallFunction(obj_callback->on_call_media_state,"i",call_id);
    }
}


/*
 * Notify application on call being transfered.
 */
static void cb_on_call_transfer_request(pjsua_call_id call_id,
				        const pj_str_t *dst,
				        pjsip_status_code *code)
{
    if (PyCallable_Check(obj_callback->on_call_transfer_request))
    {
        PyObject_CallFunctionObjArgs(
            obj_callback->on_call_transfer_request,
	    Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(dst->ptr, dst->slen),
            Py_BuildValue("i",*code),
	    NULL
        );
    }
}


/*
 * Notify application of the status of previously sent call
 * transfer request. Application can monitor the status of the
 * call transfer request, for example to decide whether to 
 * terminate existing call.
 */
static void cb_on_call_transfer_status( pjsua_call_id call_id,
					int status_code,
					const pj_str_t *status_text,
					pj_bool_t final,
					pj_bool_t *p_cont)
{
    if (PyCallable_Check(obj_callback->on_call_transfer_status))
    {
        PyObject_CallFunctionObjArgs(
            obj_callback->on_call_transfer_status,
	    Py_BuildValue("i",call_id),
	    Py_BuildValue("i",status_code),
            PyString_FromStringAndSize(status_text->ptr, status_text->slen),
	    Py_BuildValue("i",final),
            Py_BuildValue("i",*p_cont),
	    NULL
        );
    }
}


/*
 * Notify application about incoming INVITE with Replaces header.
 * Application may reject the request by setting non-2xx code.
 */
static void cb_on_call_replace_request( pjsua_call_id call_id,
					pjsip_rx_data *rdata,
					int *st_code,
					pj_str_t *st_text)
{
    if (PyCallable_Check(obj_callback->on_call_replace_request))
    {
	pjsip_rx_data_Object * obj = (pjsip_rx_data_Object *)
				      PyType_GenericNew(&pjsip_rx_data_Type,
							NULL, NULL);
	obj->rdata = rdata;

        PyObject_CallFunctionObjArgs(
            obj_callback->on_call_replace_request,
	    Py_BuildValue("i",call_id),
	    obj,
	    Py_BuildValue("i",*st_code),
            PyString_FromStringAndSize(st_text->ptr, st_text->slen),
	    NULL
        );
    }
}


/*
 * Notify application that an existing call has been replaced with
 * a new call. This happens when PJSUA-API receives incoming INVITE
 * request with Replaces header.
 */
static void cb_on_call_replaced(pjsua_call_id old_call_id,
				pjsua_call_id new_call_id)
{
    if (PyCallable_Check(obj_callback->on_call_replaced))
    {
        PyObject_CallFunctionObjArgs(
            obj_callback->on_call_replaced,
	    Py_BuildValue("i",old_call_id),
	    Py_BuildValue("i",old_call_id),
	    NULL
        );
    }
}


/*
 * cb_on_reg_state
 * declares method on_reg_state for callback struct
 */
static void cb_on_reg_state(pjsua_acc_id acc_id)
{
    if (PyCallable_Check(obj_callback->on_reg_state))
    {
        PyObject_CallFunction(obj_callback->on_reg_state,"i",acc_id);
    }
}


/*
 * cb_on_buddy_state
 * declares method on_buddy state for callback struct
 */
static void cb_on_buddy_state(pjsua_buddy_id buddy_id)
{
    if (PyCallable_Check(obj_callback->on_buddy_state))
    {
        PyObject_CallFunction(obj_callback->on_buddy_state,"i",buddy_id);
    }
}

/*
 * cb_on_pager
 * * declares method on_pager for callback struct
 */
static void cb_on_pager(pjsua_call_id call_id, const pj_str_t *from,
                        const pj_str_t *to, const pj_str_t *contact,
                        const pj_str_t *mime_type, const pj_str_t *body)
{
    if (PyCallable_Check(obj_callback->on_pager))
    {
        PyObject_CallFunctionObjArgs(
            obj_callback->on_pager,Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(from->ptr, from->slen),
            PyString_FromStringAndSize(to->ptr, to->slen),
            PyString_FromStringAndSize(contact->ptr, contact->slen),
            PyString_FromStringAndSize(mime_type->ptr, mime_type->slen),
            PyString_FromStringAndSize(body->ptr, body->slen), NULL
        );
    }
}


/*
 * cb_on_pager_status
 * declares method on_pager_status for callback struct
 */
static void cb_on_pager_status(pjsua_call_id call_id, const pj_str_t *to,
                                const pj_str_t *body, void *user_data,
                                pjsip_status_code status,
                                const pj_str_t *reason)
{
    PyObject * obj = PyType_GenericNew(user_data, NULL, NULL);
    if (PyCallable_Check(obj_callback->on_pager))
    {
        PyObject_CallFunctionObjArgs(
            obj_callback->on_pager,Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(to->ptr, to->slen),
            PyString_FromStringAndSize(body->ptr, body->slen),obj,
            Py_BuildValue("i",status),PyString_FromStringAndSize(reason->ptr,
            reason->slen),NULL
        );
    }
}


/*
 * cb_on_typing
 * declares method on_typing for callback struct
 */
static void cb_on_typing(pjsua_call_id call_id, const pj_str_t *from,
                            const pj_str_t *to, const pj_str_t *contact,
                            pj_bool_t is_typing)
{
    if (PyCallable_Check(obj_callback->on_typing))
    {
        PyObject_CallFunctionObjArgs(
            obj_callback->on_typing,Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(from->ptr, from->slen),
            PyString_FromStringAndSize(to->ptr, to->slen),
            PyString_FromStringAndSize(contact->ptr, contact->slen),
            Py_BuildValue("i",is_typing),NULL
        );
    }
}


/*
 * callback_dealloc
 * destructor function for callback struct
 */
static void callback_dealloc(callback_Object* self)
{
    Py_XDECREF(self->on_call_state);
    Py_XDECREF(self->on_incoming_call);
    Py_XDECREF(self->on_call_media_state);
    Py_XDECREF(self->on_call_transfer_request);
    Py_XDECREF(self->on_call_transfer_status);
    Py_XDECREF(self->on_call_replace_request);
    Py_XDECREF(self->on_call_replaced);
    Py_XDECREF(self->on_reg_state);
    Py_XDECREF(self->on_buddy_state);
    Py_XDECREF(self->on_pager);
    Py_XDECREF(self->on_pager_status);
    Py_XDECREF(self->on_typing);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * callback_new
 * * declares constructor for callback struct
 */
static PyObject * callback_new(PyTypeObject *type, PyObject *args,
                               PyObject *kwds)
{
    callback_Object *self;

    self = (callback_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        Py_INCREF(Py_None);
        self->on_call_state = Py_None;
        if (self->on_call_state == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_incoming_call = Py_None;
        if (self->on_incoming_call == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_media_state = Py_None;
        if (self->on_call_media_state == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_transfer_request = Py_None;
        if (self->on_call_transfer_request == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_transfer_status = Py_None;
        if (self->on_call_transfer_status == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_replace_request = Py_None;
        if (self->on_call_replace_request == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_replaced = Py_None;
        if (self->on_call_replaced == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_reg_state = Py_None;
        if (self->on_reg_state == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_buddy_state = Py_None;
        if (self->on_buddy_state == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_pager = Py_None;
        if (self->on_pager == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_pager_status = Py_None;
        if (self->on_pager_status == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_typing = Py_None;
        if (self->on_typing == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * callback_members
 * declares available functions for callback object
 */
static PyMemberDef callback_members[] =
{
    {
        "on_call_state", T_OBJECT_EX, offsetof(callback_Object, on_call_state),
        0, "Notify application when invite state has changed. Application may "
        "then query the call info to get the detail call states."
    },
    {
        "on_incoming_call", T_OBJECT_EX,
        offsetof(callback_Object, on_incoming_call), 0,
        "Notify application on incoming call."
    },
    {
        "on_call_media__state", T_OBJECT_EX,
        offsetof(callback_Object, on_call_media_state), 0,
        "Notify application when media state in the call has changed. Normal "
        "application would need to implement this callback, e.g. to connect "
        "the call's media to sound device."
    },
    {
        "on_call_transfer_request", T_OBJECT_EX,
        offsetof(callback_Object, on_call_transfer_request), 0,
        "Notify application on call being transfered. "
	"Application can decide to accept/reject transfer request "
	"by setting the code (default is 200). When this callback "
	"is not defined, the default behavior is to accept the "
	"transfer."
    },
    {
        "on_call_transfer_status", T_OBJECT_EX,
        offsetof(callback_Object, on_call_transfer_status), 0,
        "Notify application of the status of previously sent call "
        "transfer request. Application can monitor the status of the "
        "call transfer request, for example to decide whether to "
        "terminate existing call."
    },
    {
        "on_call_replace_request", T_OBJECT_EX,
        offsetof(callback_Object, on_call_replace_request), 0,
        "Notify application about incoming INVITE with Replaces header. "
        "Application may reject the request by setting non-2xx code."
    },
    {
        "on_call_replaced", T_OBJECT_EX,
        offsetof(callback_Object, on_call_replaced), 0,
	"Notify application that an existing call has been replaced with "
	"a new call. This happens when PJSUA-API receives incoming INVITE "
	"request with Replaces header."
	" "
	"After this callback is called, normally PJSUA-API will disconnect "
	"old_call_id and establish new_call_id."
    },
    {
        "on_reg_state", T_OBJECT_EX,
        offsetof(callback_Object, on_reg_state), 0,
        "Notify application when registration status has changed. Application "
        "may then query the account info to get the registration details."
    },
    {
        "on_buddy_state", T_OBJECT_EX,
        offsetof(callback_Object, on_buddy_state), 0,
        "Notify application when the buddy state has changed. Application may "
        "then query the buddy into to get the details."
    },
    {
        "on_pager", T_OBJECT_EX, offsetof(callback_Object, on_pager), 0,
        "Notify application on incoming pager (i.e. MESSAGE request). "
        "Argument call_id will be -1 if MESSAGE request is not related to an "
        "existing call."
    },
    {
        "on_pager_status", T_OBJECT_EX,
        offsetof(callback_Object, on_pager_status), 0,
        "Notify application about the delivery status of outgoing pager "
        "request."
    },
    {
        "on_typing", T_OBJECT_EX, offsetof(callback_Object, on_typing), 0,
        "Notify application about typing indication."
    },
    {NULL}  /* Sentinel */
};


/*
 * callback_Type
 * callback class definition
 */
static PyTypeObject callback_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Callback",            /*tp_name*/
    sizeof(callback_Object),        /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)callback_dealloc,   /*tp_dealloc*/
    0,                             	/*tp_print*/
    0,                             	/*tp_getattr*/
    0,                             	/*tp_setattr*/
    0,                             	/*tp_compare*/
    0,                             	/*tp_repr*/
    0,                             	/*tp_as_number*/
    0,                             	/*tp_as_sequence*/
    0,                             	/*tp_as_mapping*/
    0,                             	/*tp_hash */
    0,                             	/*tp_call*/
    0,                             	/*tp_str*/
    0,                             	/*tp_getattro*/
    0,                             	/*tp_setattro*/
    0,                             	/*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,            	/*tp_flags*/
    "Callback objects",             /* tp_doc */
    0,                           	/* tp_traverse */
    0,                           	/* tp_clear */
    0,                           	/* tp_richcompare */
    0,                           	/* tp_weaklistoffset */
    0,                           	/* tp_iter */
    0,                           	/* tp_iternext */
    0,                 				/* tp_methods */
    callback_members,               /* tp_members */
    0,                             	/* tp_getset */
    0,                             	/* tp_base */
    0,                             	/* tp_dict */
    0,                             	/* tp_descr_get */
    0,                             	/* tp_descr_set */
    0,                             	/* tp_dictoffset */
    0,          					/* tp_init */
    0,                             	/* tp_alloc */
    callback_new,                   /* tp_new */

};


/*
 * media_config_Object
 * C/Python wrapper for media_config object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    unsigned clock_rate;
    unsigned max_media_ports;
    int has_ioqueue;
    unsigned thread_cnt;
    unsigned quality;
    unsigned ptime;
    int no_vad;
    unsigned ilbc_mode;
    unsigned tx_drop_pct;
    unsigned rx_drop_pct;
    unsigned ec_options;
    unsigned ec_tail_len;
} media_config_Object;


/*
 * media_config_members
 * declares attributes accessible from both C and Python for media_config file
 */
static PyMemberDef media_config_members[] =
{
    {
        "clock_rate", T_INT, offsetof(media_config_Object, clock_rate), 0,
        "Clock rate to be applied to the conference bridge. If value is zero, "
        "default clock rate will be used (16KHz)."
    },
    {
        "max_media_ports", T_INT,
        offsetof(media_config_Object, max_media_ports), 0,
        "Specify maximum number of media ports to be created in the "
        "conference bridge. Since all media terminate in the bridge (calls, "
        "file player, file recorder, etc), the value must be large enough to "
        "support all of them. However, the larger the value, the more "
        "computations are performed."
    },
    {
        "has_ioqueue", T_INT, offsetof(media_config_Object, has_ioqueue), 0,
        "Specify whether the media manager should manage its own ioqueue for "
        "the RTP/RTCP sockets. If yes, ioqueue will be created and at least "
        "one worker thread will be created too. If no, the RTP/RTCP sockets "
        "will share the same ioqueue as SIP sockets, and no worker thread is "
        "needed."
    },
    {
        "thread_cnt", T_INT, offsetof(media_config_Object, thread_cnt), 0,
        "Specify the number of worker threads to handle incoming RTP packets. "
        "A value of one is recommended for most applications."
    },
    {
        "quality", T_INT, offsetof(media_config_Object, quality), 0,
        "The media quality also sets speex codec quality/complexity to the "
        "number."
    },
    {
        "ptime", T_INT, offsetof(media_config_Object, ptime), 0,
        "Specify default ptime."
    },
    {
        "no_vad", T_INT, offsetof(media_config_Object, no_vad), 0,
        "Disable VAD?"
    },
    {
        "ilbc_mode", T_INT, offsetof(media_config_Object, ilbc_mode), 0,
        "iLBC mode (20 or 30)."
    },
    {
        "tx_drop_pct", T_INT, offsetof(media_config_Object, tx_drop_pct), 0,
        "Percentage of RTP packet to drop in TX direction (to simulate packet "
        "lost)."
    },
    {
        "rx_drop_pct", T_INT, offsetof(media_config_Object, rx_drop_pct), 0,
        "Percentage of RTP packet to drop in RX direction (to simulate packet "
        "lost)."},
    {
        "ec_options", T_INT, offsetof(media_config_Object, ec_options), 0,
        "Echo canceller options (see #pjmedia_echo_create())"
    },
    {
        "ec_tail_len", T_INT, offsetof(media_config_Object, ec_tail_len), 0,
        "Echo canceller tail length, in miliseconds."
    },
    {NULL}  /* Sentinel */
};


/*
 * media_config_Type
 */
static PyTypeObject media_config_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Media_Config",        /*tp_name*/
    sizeof(media_config_Object),    /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,                              /*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Media Config objects",         /*tp_doc*/
    0,                              /*tp_traverse*/
    0,                              /*tp_clear*/
    0,                              /*tp_richcompare*/
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    media_config_members,           /* tp_members */

};


/*
 * config_Object
 * attribute list for config object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    unsigned max_calls;
    unsigned thread_cnt;
    unsigned outbound_proxy_cnt;
    pj_str_t outbound_proxy[4];
    unsigned cred_count;
    pjsip_cred_info cred_info[PJSUA_ACC_MAX_PROXIES];
    callback_Object * cb;
    PyObject * user_agent;
} config_Object;


/*
 * config_dealloc
 * deallocates a config object
 */
static void config_dealloc(config_Object* self)
{
    Py_XDECREF(self->cb);
    Py_XDECREF(self->user_agent);
    self->ob_type->tp_free((PyObject*)self);
}

/*
 * config_new 
 * config object constructor
 */
static PyObject *config_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    config_Object *self;

    self = (config_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->user_agent = PyString_FromString("");
        if (self->user_agent == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->cb = (callback_Object *)PyType_GenericNew(
            &callback_Type, NULL, NULL
        );
        if (self->cb == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
    }
    return (PyObject *)self;
}


/*
 * config_members
 * attribute list accessible from Python/C
 */
static PyMemberDef config_members[] =
{
    {
    	"max_calls", T_INT, offsetof(config_Object, max_calls), 0,
    	"Maximum calls to support (default: 4) "
    },
    {
    	"thread_cnt", T_INT, offsetof(config_Object, thread_cnt), 0,
    	"Number of worker threads. Normally application will want to have at "
    	"least one worker thread, unless when it wants to poll the library "
    	"periodically, which in this case the worker thread can be set to "
    	"zero."
    },
    {
    	"outbound_proxy_cnt", T_INT,
    	offsetof(config_Object, outbound_proxy_cnt), 0,
    	"Number of outbound proxies in the array."
    },
    {
    	"cred_count", T_INT, offsetof(config_Object, cred_count), 0,
    	"Number of credentials in the credential array."
    },
    {
    	"user_agent", T_OBJECT_EX, offsetof(config_Object, user_agent), 0,
    	"User agent string (default empty)"
    },
    {
    	"cb", T_OBJECT_EX, offsetof(config_Object, cb), 0,
    	"Application callback."
    },
    {NULL}  /* Sentinel */
};


/*
 * config_Type
 * type wrapper for config class
 */
static PyTypeObject config_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.Config",         /*tp_name*/
    sizeof(config_Object),     /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)config_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "Config objects",          /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    config_members,            /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    config_new,                 /* tp_new */

};


/*
 * logging_config_Object
 * configuration class for logging_config object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    int msg_logging;
    unsigned level;
    unsigned console_level;
    unsigned decor;
    PyObject * log_filename;
    PyObject * cb;
} logging_config_Object;


/*
 * logging_config_dealloc
 * deletes a logging config from memory
 */
static void logging_config_dealloc(logging_config_Object* self)
{
    Py_XDECREF(self->log_filename);
    Py_XDECREF(self->cb);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * logging_config_new
 * constructor for logging_config object
 */
static PyObject * logging_config_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    logging_config_Object *self;

    self = (logging_config_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->log_filename = PyString_FromString("");
        if (self->log_filename == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->cb = Py_None;
        if (self->cb == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * logging_config_members
 */
static PyMemberDef logging_config_members[] =
{
    {
    	"msg_logging", T_INT, offsetof(logging_config_Object, msg_logging), 0,
    	"Log incoming and outgoing SIP message? Yes!"
    },
    {
    	"level", T_INT, offsetof(logging_config_Object, level), 0,
    	"Input verbosity level. Value 5 is reasonable."
    },
    {
    	"console_level", T_INT, offsetof(logging_config_Object, console_level),
    	0, "Verbosity level for console. Value 4 is reasonable."
    },
    {
    	"decor", T_INT, offsetof(logging_config_Object, decor), 0,
    	"Log decoration"
    },
    {
    	"log_filename", T_OBJECT_EX,
    	offsetof(logging_config_Object, log_filename), 0,
    	"Optional log filename"
    },
    {
    	"cb", T_OBJECT_EX, offsetof(logging_config_Object, cb), 0,
    	"Optional callback function to be called to write log to application "
    	"specific device. This function will be called forlog messages on "
    	"input verbosity level."
    },
    {NULL}  /* Sentinel */
};




/*
 * logging_config_Type
 */
static PyTypeObject logging_config_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Logging_Config",      /*tp_name*/
    sizeof(logging_config_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)logging_config_dealloc,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Logging Config objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    logging_config_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    logging_config_new,             /* tp_new */

};


/*
 * msg_data_Object
 * typewrapper for MessageData class
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_hdr hdr_list;
    PyObject * content_type;
    PyObject * msg_body;
} msg_data_Object;


/*
 * msg_data_dealloc
 * deletes a msg_data
 */
static void msg_data_dealloc(msg_data_Object* self)
{
    Py_XDECREF(self->content_type);
    Py_XDECREF(self->msg_body);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * msg_data_new
 * constructor for msg_data object
 */
static PyObject * msg_data_new(PyTypeObject *type, PyObject *args,
                                PyObject *kwds)
{
    msg_data_Object *self;

    self = (msg_data_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->content_type = PyString_FromString("");
        if (self->content_type == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->msg_body = PyString_FromString("");
        if (self->msg_body == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * msg_data_members
 */
static PyMemberDef msg_data_members[] =
{
    {
    	"content_type", T_OBJECT_EX, offsetof(msg_data_Object, content_type),
    	0, "MIME type of optional message body."
    },
    {
    	"msg_body", T_OBJECT_EX, offsetof(msg_data_Object, msg_body), 0,
    	"Optional message body."
    },
    {NULL}  /* Sentinel */
};


/*
 * msg_data_Type
 */
static PyTypeObject msg_data_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.Msg_Data",       /*tp_name*/
    sizeof(msg_data_Object),   /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)msg_data_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "msg_data objects",        /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    msg_data_members,          /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    msg_data_new,                 /* tp_new */

};


/*
 * pj_pool_Object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pj_pool_t * pool;
} pj_pool_Object;


/*
 * pj_pool_Type
 */
static PyTypeObject pj_pool_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.PJ_Pool",        /*tp_name*/
    sizeof(pj_pool_Object),    /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pj_pool_t objects",       /* tp_doc */

};


/*
 * pjsip_endpoint_Object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_endpoint * endpt;
} pjsip_endpoint_Object;


/*
 * pjsip_endpoint_Type
 */
static PyTypeObject pjsip_endpoint_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.PJSIP_Endpoint", /*tp_name*/
    sizeof(pjsip_endpoint_Object),/*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pjsip_endpoint objects",  /* tp_doc */
};


/*
 * pjmedia_endpt_Object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjmedia_endpt * endpt;
} pjmedia_endpt_Object;


/*
 * pjmedia_endpt_Type
 */
static PyTypeObject pjmedia_endpt_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.PJMedia_Endpt",  /*tp_name*/
    sizeof(pjmedia_endpt_Object), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pjmedia_endpt objects",   /* tp_doc */

};


/*
 * pj_pool_factory_Object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pj_pool_factory * pool_fact;
} pj_pool_factory_Object;



/*
 * pj_pool_factory_Type
 */
static PyTypeObject pj_pool_factory_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.PJ_Pool_Factory",/*tp_name*/
    sizeof(pj_pool_factory_Object), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pj_pool_factory objects", /* tp_doc */

};


/*
 * pjsip_cred_info_Object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_cred_info * cred;
} pjsip_cred_info_Object;


/*
 * pjsip_cred_info_Type
 */
static PyTypeObject pjsip_cred_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.PJSIP_Cred_Info",/*tp_name*/
    sizeof(pjsip_cred_info_Object), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pjsip_cred_info objects", /* tp_doc */

};


/*
 * py_pjsua_logging_config_default
 */
static PyObject *py_pjsua_logging_config_default(PyObject *pSelf,
                                                    PyObject *pArgs)
{
    logging_config_Object *obj;
    pjsua_logging_config cfg;

    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
    /*pj_bzero(cfg, sizeof(*cfg));

    cfg->msg_logging = obj->msg_logging;
    cfg->level = obj->level;
    cfg->console_level = obj->console_level;
    cfg->decor = obj->decor;*/
    pjsua_logging_config_default(&cfg);
    obj->msg_logging = cfg.msg_logging;
    obj->level = cfg.level;
    obj->console_level = cfg.console_level;
    obj->decor = cfg.decor;
    /*printf("msg logging : %d\n",obj->msg_logging);
    printf("level : %d\n",obj->level);
    printf("console level : %d\n",obj->console_level);
    printf("decor : %d\n",obj->decor);
    printf("str sebelum ");
    printf(PyString_AsString(obj->log_filename));

    Py_XDECREF(obj->log_filename);
    obj->log_filename = PyString_FromString("oke");
    printf("\nstr sesudah ");
    printf(PyString_AsString(obj->log_filename));*/
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsua_config_default
 */
static PyObject *py_pjsua_config_default(PyObject *pSelf, PyObject *pArgs)
{
    config_Object *obj;
    pjsua_config cfg;

    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
    pjsua_config_default(&cfg);
    obj->max_calls = cfg.max_calls;
    obj->thread_cnt = cfg.thread_cnt;
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsua_media_config_default
 */
static PyObject * py_pjsua_media_config_default(PyObject *pSelf,
                                                PyObject *pArgs)
{
    media_config_Object *obj;
    pjsua_media_config cfg;
    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
    pjsua_media_config_default(&cfg);
    obj->clock_rate = cfg.clock_rate;
    obj->ec_options = cfg.ec_options;
    obj->ec_tail_len = cfg.ec_tail_len;
    obj->has_ioqueue = cfg.has_ioqueue;
    obj->ilbc_mode = cfg.ilbc_mode;
    obj->max_media_ports = cfg.max_media_ports;
    obj->no_vad = cfg.no_vad;
    obj->ptime = cfg.ptime;
    obj->quality = cfg.quality;
    obj->rx_drop_pct = cfg.rx_drop_pct;
    obj->thread_cnt = cfg.thread_cnt;
    obj->tx_drop_pct = cfg.tx_drop_pct;
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsua_msg_data_init
 */
static PyObject *py_pjsua_msg_data_init(PyObject *pSelf, PyObject *pArgs)
{
    msg_data_Object *obj;
    pjsua_msg_data msg;
    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
    pjsua_msg_data_init(&msg);
    Py_XDECREF(obj->content_type);
    obj->content_type = PyString_FromStringAndSize(
        msg.content_type.ptr, msg.content_type.slen
    );
    Py_XDECREF(obj->msg_body);
    obj->msg_body = PyString_FromStringAndSize(
        msg.msg_body.ptr, msg.msg_body.slen
    );
    obj->hdr_list = msg.hdr_list;
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsua_logging_config_dup
 */
static PyObject *py_pjsua_logging_config_dup(PyObject *pSelf, PyObject *pArgs)
{
    pj_pool_Object *pool;
    logging_config_Object *src;
    logging_config_Object *dest;
    pj_str_t strdest;
    pj_str_t strsrc;
    int len;

    if (!PyArg_ParseTuple(pArgs, "OOO", &pool, &dest, &src))
    {
        return NULL;
    }
    pj_memcpy(dest, src, sizeof(*src));
    len = strlen(PyString_AsString(src->log_filename));
    strsrc.ptr = PyString_AsString(src->log_filename);
    strsrc.slen = len;
    pj_strdup_with_null(pool->pool, &strdest, &strsrc);
    Py_XDECREF(dest->log_filename);
    dest->log_filename = PyString_FromStringAndSize(strdest.ptr, strdest.slen);
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsua_config_dup
 */
static PyObject *py_pjsua_config_dup(PyObject *pSelf, PyObject *pArgs)
{
    pj_pool_Object *pool;
    config_Object *src;
    config_Object *dest;
    pj_str_t strdest;
    pj_str_t strsrc;
    int len;
    unsigned i;

    if (!PyArg_ParseTuple(pArgs, "OOO", &pool, &dest, &src))
    {
        return NULL;
    }
    pj_memcpy(dest, src, sizeof(*src));

    for (i=0; i<src->outbound_proxy_cnt; ++i)
    {
        pj_strdup_with_null(
            pool->pool, &dest->outbound_proxy[i], &src->outbound_proxy[i]
        );
    }

    for (i=0; i<src->cred_count; ++i)
    {
        pjsip_cred_dup(pool->pool, &dest->cred_info[i], &src->cred_info[i]);
    }
    len = strlen(PyString_AsString(src->user_agent));
    strsrc.ptr = PyString_AsString(src->user_agent);
    strsrc.slen = len;
    pj_strdup_with_null(pool->pool, &strdest, &strsrc);
    Py_XDECREF(dest->user_agent);
    dest->user_agent = PyString_FromStringAndSize(strdest.ptr, strdest.slen);
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsip_cred_dup
 */
static PyObject *py_pjsip_cred_dup(PyObject *pSelf, PyObject *pArgs)
{
    pj_pool_Object *pool;
    pjsip_cred_info_Object *src;
    pjsip_cred_info_Object *dest;
    if (!PyArg_ParseTuple(pArgs, "OOO", &pool, &dest, &src))
    {
        return NULL;
    }
    pjsip_cred_dup(pool->pool, dest->cred, src->cred);
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsua_reconfigure_logging
 */
static PyObject *py_pjsua_reconfigure_logging(PyObject *pSelf, PyObject *pArgs)
{
    logging_config_Object *log;
    pjsua_logging_config cfg;
    pj_status_t status;

    if (!PyArg_ParseTuple(pArgs, "O", &log))
    {
        return NULL;
    }
    cfg.msg_logging = log->msg_logging;
    cfg.level = log->level;
    cfg.console_level = log->console_level;
    cfg.decor = log->decor;
    cfg.log_filename.ptr = PyString_AsString(log->log_filename);
    cfg.log_filename.slen = strlen(cfg.log_filename.ptr);
    Py_XDECREF(obj_reconfigure_logging);
    obj_reconfigure_logging = log->cb;
    Py_INCREF(obj_reconfigure_logging);
    cfg.cb = &cb_reconfigure_logging;
    status = pjsua_reconfigure_logging(&cfg);
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_pool_create
 */
static PyObject *py_pjsua_pool_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_size_t init_size;
    pj_size_t increment;
    const char * name;
    pj_pool_t *p;
    pj_pool_Object *pool;

    if (!PyArg_ParseTuple(pArgs, "sII", &name, &init_size, &increment))
    {
        return NULL;
    }
    /*printf("name : %s\n",name);
    printf("init : %d\n", init_size);
    printf("increment : %d\n", increment);*/
    p = pjsua_pool_create(name, init_size, increment);
    pool = (pj_pool_Object *)PyType_GenericNew(&pj_pool_Type, NULL, NULL);
    pool->pool = p;
    return (PyObject *)pool;

}


/*
 * py_pjsua_get_pjsip_endpt
 */
static PyObject *py_pjsua_get_pjsip_endpt(PyObject *pSelf, PyObject *pArgs)
{
    pjsip_endpoint_Object *endpt;
    pjsip_endpoint *e;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    e = pjsua_get_pjsip_endpt();
    endpt = (pjsip_endpoint_Object *)PyType_GenericNew(
        &pjsip_endpoint_Type, NULL, NULL
    );
    endpt->endpt = e;
    return (PyObject *)endpt;
}


/*
 * py_pjsua_get_pjmedia_endpt
 */
static PyObject *py_pjsua_get_pjmedia_endpt(PyObject *pSelf, PyObject *pArgs)
{
    pjmedia_endpt_Object *endpt;
    pjmedia_endpt *e;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    e = pjsua_get_pjmedia_endpt();
    endpt = (pjmedia_endpt_Object *)PyType_GenericNew(
        &pjmedia_endpt_Type, NULL, NULL
    );
    endpt->endpt = e;
    return (PyObject *)endpt;
}


/*
 * py_pjsua_get_pool_factory
 */
static PyObject *py_pjsua_get_pool_factory(PyObject *pSelf, PyObject *pArgs)
{
    pj_pool_factory_Object *pool;
    pj_pool_factory *p;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    p = pjsua_get_pool_factory();
    pool = (pj_pool_factory_Object *)PyType_GenericNew(
        &pj_pool_factory_Type, NULL, NULL
    );
    pool->pool_fact = p;
    return (PyObject *)pool;
}


/*
 * py_pjsua_perror
 */
static PyObject *py_pjsua_perror(PyObject *pSelf, PyObject *pArgs)
{
    const char *sender;
    const char *title;
    pj_status_t status;
    if (!PyArg_ParseTuple(pArgs, "ssi", &sender, &title, &status))
    {
        return NULL;
    }
    pjsua_perror(sender, title, status);
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsua_create
 */
static PyObject *py_pjsua_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    status = pjsua_create();
    printf("status %d\n",status);
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_init
 */
static PyObject *py_pjsua_init(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    config_Object * ua_cfg;
    logging_config_Object * log_cfg;
    media_config_Object * media_cfg;
    pjsua_config cfg_ua;
    pjsua_logging_config cfg_log;
    pjsua_media_config cfg_media;
    unsigned i;

    if (!PyArg_ParseTuple(pArgs, "OOO", &ua_cfg, &log_cfg, &media_cfg))
    {
        return NULL;
    }

    pjsua_config_default(&cfg_ua);
    pjsua_logging_config_default(&cfg_log);
    pjsua_media_config_default(&cfg_media);
    cfg_ua.cred_count = ua_cfg->cred_count;
    for (i = 0; i < 4; i++)
    {
        cfg_ua.cred_info[i] = ua_cfg->cred_info[i];
    }
    cfg_ua.max_calls = ua_cfg->max_calls;
    for (i = 0; i < PJSUA_ACC_MAX_PROXIES; i++)
    {
        cfg_ua.outbound_proxy[i] = ua_cfg->outbound_proxy[i];
    }

    cfg_ua.outbound_proxy_cnt = ua_cfg->outbound_proxy_cnt;
    cfg_ua.thread_cnt = ua_cfg->thread_cnt;
    cfg_ua.user_agent.ptr = PyString_AsString(ua_cfg->user_agent);
    cfg_ua.user_agent.slen = strlen(cfg_ua.user_agent.ptr);

    obj_callback = ua_cfg->cb;
    cfg_ua.cb.on_call_state = &cb_on_call_state;
    cfg_ua.cb.on_incoming_call = &cb_on_incoming_call;
    cfg_ua.cb.on_call_media_state = &cb_on_call_media_state;
    cfg_ua.cb.on_call_transfer_request = &cb_on_call_transfer_request;
    cfg_ua.cb.on_call_transfer_status = &cb_on_call_transfer_status;
    cfg_ua.cb.on_call_replace_request = &cb_on_call_replace_request;
    cfg_ua.cb.on_call_replaced = &cb_on_call_replaced;
    cfg_ua.cb.on_reg_state = &cb_on_reg_state;
    cfg_ua.cb.on_buddy_state = &cb_on_buddy_state;
    cfg_ua.cb.on_pager = &cb_on_pager;
    cfg_ua.cb.on_pager_status = &cb_on_pager_status;
    cfg_ua.cb.on_typing = &cb_on_typing;

    cfg_log.msg_logging = log_cfg->msg_logging;
    cfg_log.level = log_cfg->level;
    cfg_log.console_level = log_cfg->console_level;
    cfg_log.decor = log_cfg->decor;
    cfg_log.log_filename.ptr = PyString_AsString(log_cfg->log_filename);
    cfg_log.log_filename.slen = strlen(cfg_log.log_filename.ptr);
    Py_XDECREF(obj_logging_init);
    obj_logging_init = log_cfg->cb;
    Py_INCREF(obj_logging_init);
    cfg_log.cb = &cb_logging_init;


    cfg_media.clock_rate = media_cfg->clock_rate;
    cfg_media.ec_options = media_cfg->ec_options;
    cfg_media.ec_tail_len = media_cfg->ec_tail_len;
    cfg_media.has_ioqueue = media_cfg->has_ioqueue;
    cfg_media.ilbc_mode = media_cfg->ilbc_mode;
    cfg_media.max_media_ports = media_cfg->max_media_ports;
    cfg_media.no_vad = media_cfg->no_vad;
    cfg_media.ptime = media_cfg->ptime;
    cfg_media.quality = media_cfg->quality;
    cfg_media.rx_drop_pct = media_cfg->rx_drop_pct;
    cfg_media.thread_cnt = media_cfg->thread_cnt;
    cfg_media.tx_drop_pct = media_cfg->tx_drop_pct;

    status = pjsua_init(&cfg_ua, &cfg_log, &cfg_media);
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_start
 */
static PyObject *py_pjsua_start(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    status = pjsua_start();
    printf("status %d\n",status);
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_destroy
 */
static PyObject *py_pjsua_destroy(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    status = pjsua_destroy();
    printf("status %d\n",status);
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_handle_events
 */
static PyObject *py_pjsua_handle_events(PyObject *pSelf, PyObject *pArgs)
{
    int ret;
    unsigned msec;
    if (!PyArg_ParseTuple(pArgs, "i", &msec))
    {
        return NULL;
    }
    ret = pjsua_handle_events(msec);
    printf("return %d\n",ret);
    return Py_BuildValue("i",ret);
}


/*
 * py_pjsua_verify_sip_url
 */
static PyObject *py_pjsua_verify_sip_url(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    const char *url;
    if (!PyArg_ParseTuple(pArgs, "s", &url))
    {
        return NULL;
    }
    status = pjsua_verify_sip_url(url);
    printf("status %d\n",status);
    return Py_BuildValue("i",status);
}






/*
 * error messages
 */

static char pjsua_perror_doc[] =
    "void py_pjsua.perror (string sender, string title, int status) "
    "Display error message for the specified error code. Parameters: "
        "sender: The log sender field;  "
        "title: Message title for the error; "
        "status: Status code.";

static char pjsua_create_doc[] =
    "int py_pjsua.create (void) "
    "Instantiate pjsua application. Application "
    "must call this function before calling any other functions, to make sure "
    "that the underlying libraries are properly initialized. Once this "
    "function has returned success, application must call pjsua_destroy() "
    "before quitting.";

static char pjsua_init_doc[] =
    "int py_pjsua.init (py_pjsua.Config ua_cfg, "
        "py_pjsua.Logging_Config log_cfg, py_pjsua.Media_Config media_cfg) "
    "Initialize pjsua with the specified settings. All the settings are "
    "optional, and the default values will be used when the config is not "
    "specified. Parameters: "
        "ua_cfg : User agent configuration;  "
        "log_cfg : Optional logging configuration; "
        "media_cfg : Optional media configuration.";

static char pjsua_start_doc[] =
    "int py_pjsua.start (void) "
    "Application is recommended to call this function after all "
    "initialization is done, so that the library can do additional checking "
    "set up additional";

static char pjsua_destroy_doc[] =
    "int py_pjsua.destroy (void) "
    "Destroy pjsua This function must be called once PJSUA is created. To "
    "make it easier for application, application may call this function "
    "several times with no danger.";

static char pjsua_handle_events_doc[] =
    "int py_pjsua.handle_events (int msec_timeout) "
    "Poll pjsua for events, and if necessary block the caller thread for the "
    "specified maximum interval (in miliseconds) Parameters: "
        "msec_timeout: Maximum time to wait, in miliseconds. "
    "Returns: The number of events that have been handled during the poll. "
    "Negative value indicates error, and application can retrieve the error "
    "as (err = -return_value).";

static char pjsua_verify_sip_url_doc[] =
    "int py_pjsua.verify_sip_url (string c_url) "
    "Verify that valid SIP url is given Parameters: "
        "c_url: The URL, as NULL terminated string.";

static char pjsua_pool_create_doc[] =
    "py_pjsua.PJ_Pool py_pjsua.pool_create (string name, int init_size, "
                                            "int increment) "
    "Create memory pool Parameters: "
        "name: Optional pool name; "
        "init_size: Initial size of the pool;  "
        "increment: Increment size.";

static char pjsua_get_pjsip_endpt_doc[] =
    "py_pjsua.PJSIP_Endpoint py_pjsua.get_pjsip_endpt (void) "
    "Internal function to get SIP endpoint instance of pjsua, which is needed "
    "for example to register module, create transports, etc. Probably is only "
    "valid after pjsua_init() is called.";

static char pjsua_get_pjmedia_endpt_doc[] =
    "py_pjsua.PJMedia_Endpt py_pjsua.get_pjmedia_endpt (void) "
    "Internal function to get media endpoint instance. Only valid after "
    "pjsua_init() is called.";

static char pjsua_get_pool_factory_doc[] =
    "py_pjsua.PJ_Pool_Factory py_pjsua.get_pool_factory (void) "
    "Internal function to get PJSUA pool factory. Only valid after "
    "pjsua_init() is called.";

static char pjsua_reconfigure_logging_doc[] =
    "int py_pjsua.reconfigure_logging (py_pjsua.Logging_Config c) "
    "Application can call this function at any time (after pjsua_create(), of "
    "course) to change logging settings. Parameters: "
        "c: Logging configuration.";

static char pjsua_logging_config_default_doc[] =
    "void py_pjsua.logging_config_default  (py_pjsua.Logging_Config cfg)  "
    "Use this function to initialize logging config.";

static char pjsua_config_default_doc[] =
    "void py_pjsua.config_default (py_pjsua.Config cfg). Use this function to "
    "initialize pjsua config. Parameters: "
        "cfg: pjsua config to be initialized.";

static char pjsua_media_config_default_doc[] =
    "Use this function to initialize media config.";

static char pjsua_logging_config_dup_doc[] =
    "void py_pjsua.logging_config_dup  (py_pjsua.PJ_Pool pool, "
        "py_pjsua.Logging_Config dst, py_pjsua.Logging_Config src) "
    "Use this function to duplicate logging config. Parameters: "
        "pool: Pool to use;  "
        "dst: Destination config;  "
        "src: Source config.";

static char pjsua_config_dup_doc[] =
    "void py_pjsua.config_dup (py_pjsua.PJ_Pool pool, py_pjsua.Config dst, "
                                "py_pjsua.Config src) "
    "Duplicate pjsua_config. ";

static char pjsip_cred_dup_doc[] =
    "void py_pjsua.pjsip_cred_dup (py_pjsua.PJ_Pool pool, "
                                "py_pjsua.PJSIP_Cred_Info dst, "
                                "py_pjsua.PJSIP_Cred_Info src) "
    "Duplicate credential.";

static char pjsua_msg_data_init_doc[] =
    "void py_pjsua.msg_data_init (py_pjsua.Msg_Data msg_data) "
    "Initialize message data Parameters: "
        "msg_data: Message data to be initialized.";


/*
 * Map of function names to functions
 */
static PyMethodDef py_pjsua_methods[] =
{
    {
    	"perror", py_pjsua_perror, METH_VARARGS, pjsua_perror_doc
    },
    {
    	"create", py_pjsua_create, METH_VARARGS, pjsua_create_doc
    },
    {
    	"init", py_pjsua_init, METH_VARARGS, pjsua_init_doc
    },
    {
    	"start", py_pjsua_start, METH_VARARGS, pjsua_start_doc
    },
    {
    	"destroy", py_pjsua_destroy, METH_VARARGS, pjsua_destroy_doc
    },
    {
    	"handle_events", py_pjsua_handle_events, METH_VARARGS,
    	pjsua_handle_events_doc
    },
    {
    	"verify_sip_url", py_pjsua_verify_sip_url, METH_VARARGS,
    	pjsua_verify_sip_url_doc
    },
    {
    	"pool_create", py_pjsua_pool_create, METH_VARARGS,
    	pjsua_pool_create_doc
    },
    {
    	"get_pjsip_endpt", py_pjsua_get_pjsip_endpt, METH_VARARGS,
    	pjsua_get_pjsip_endpt_doc
    },
    {
    	"get_pjmedia_endpt", py_pjsua_get_pjmedia_endpt, METH_VARARGS,
    	pjsua_get_pjmedia_endpt_doc
    },
    {
    	"get_pool_factory", py_pjsua_get_pool_factory, METH_VARARGS,
    	pjsua_get_pool_factory_doc
    },
    {
    	"reconfigure_logging", py_pjsua_reconfigure_logging, METH_VARARGS,
    	pjsua_reconfigure_logging_doc
    },
    {
    	"logging_config_default", py_pjsua_logging_config_default,
    	METH_VARARGS, pjsua_logging_config_default_doc
    },
    {
    	"config_default", py_pjsua_config_default, METH_VARARGS,
    	pjsua_config_default_doc
    },
    {
    	"media_config_default", py_pjsua_media_config_default, METH_VARARGS,
    	pjsua_media_config_default_doc
    },
    {
    	"logging_config_dup", py_pjsua_logging_config_dup, METH_VARARGS,
    	pjsua_logging_config_dup_doc
    },
    {
    	"config_dup", py_pjsua_config_dup, METH_VARARGS, pjsua_config_dup_doc
    },
    {
    	"pjsip_cred_dup", py_pjsip_cred_dup, METH_VARARGS, pjsip_cred_dup_doc
    },
    {
    	"msg_data_init", py_pjsua_msg_data_init, METH_VARARGS,
    	pjsua_msg_data_init_doc
    },
    {NULL, NULL} /* end of function list */
};



/*
 * Mapping C structs from and to Python objects & initializing object
 */
DL_EXPORT(void)
initpy_pjsua(void)
{
    PyObject* m = NULL;

    if (PyType_Ready(&callback_Type) < 0)
        return;
    if (PyType_Ready(&config_Type) < 0)
        return;
    if (PyType_Ready(&logging_config_Type) < 0)
        return;
    if (PyType_Ready(&msg_data_Type) < 0)
        return;
    media_config_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&media_config_Type) < 0)
        return;
    pjsip_event_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjsip_event_Type) < 0)
        return;
    pjsip_rx_data_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjsip_rx_data_Type) < 0)
        return;
    pj_pool_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pj_pool_Type) < 0)
        return;
    pjsip_endpoint_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjsip_endpoint_Type) < 0)
        return;
    pjmedia_endpt_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjmedia_endpt_Type) < 0)
        return;
    pj_pool_factory_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pj_pool_factory_Type) < 0)
        return;
    pjsip_cred_info_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjsip_cred_info_Type) < 0)
        return;

    m = Py_InitModule3(
        "py_pjsua", py_pjsua_methods,"PJSUA-lib module for python"
    );

    Py_INCREF(&callback_Type);
    PyModule_AddObject(m, "Callback", (PyObject *)&callback_Type);

    Py_INCREF(&config_Type);
    PyModule_AddObject(m, "Config", (PyObject *)&config_Type);

    Py_INCREF(&media_config_Type);
    PyModule_AddObject(m, "Media_Config", (PyObject *)&media_config_Type);

    Py_INCREF(&logging_config_Type);
    PyModule_AddObject(m, "Logging_Config", (PyObject *)&logging_config_Type);

    Py_INCREF(&msg_data_Type);
    PyModule_AddObject(m, "Msg_Data", (PyObject *)&msg_data_Type);

    Py_INCREF(&pjsip_event_Type);
    PyModule_AddObject(m, "PJSIP_Event", (PyObject *)&pjsip_event_Type);

    Py_INCREF(&pjsip_rx_data_Type);
    PyModule_AddObject(m, "PJSIP_RX_Data", (PyObject *)&pjsip_rx_data_Type);

    Py_INCREF(&pj_pool_Type);
    PyModule_AddObject(m, "PJ_Pool", (PyObject *)&pj_pool_Type);

    Py_INCREF(&pjsip_endpoint_Type);
    PyModule_AddObject(m, "PJSIP_Endpoint", (PyObject *)&pjsip_endpoint_Type);

    Py_INCREF(&pjmedia_endpt_Type);
    PyModule_AddObject(m, "PJMedia_Endpt", (PyObject *)&pjmedia_endpt_Type);

    Py_INCREF(&pj_pool_factory_Type);
    PyModule_AddObject(
        m, "PJ_Pool_Factory", (PyObject *)&pj_pool_factory_Type
    );

    Py_INCREF(&pjsip_cred_info_Type);
    PyModule_AddObject(m, "PJSIP_Cred_Info",
        (PyObject *)&pjsip_cred_info_Type
    );


#ifdef PJSUA_INVALID_ID
    /*
     * Constant to identify invalid ID for all sorts of IDs.
     */
    PyModule_AddIntConstant(m, "PJSUA_INVALID_ID", PJSUA_INVALID_ID);
#endif

#ifdef PJSUA_ACC_MAX_PROXIES
    /*
     * Maximum proxies in account.
     */
    PyModule_AddIntConstant(
        m, "PJSUA_ACC_MAX_PROXIES ", PJSUA_ACC_MAX_PROXIES
    );
#endif
}
