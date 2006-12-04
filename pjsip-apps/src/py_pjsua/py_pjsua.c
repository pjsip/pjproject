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

/* LIB BASE */

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
	PyObject * realm;		
    PyObject * scheme;		
    PyObject * username;	
    int	data_type;	
    PyObject * data;	
    
} pjsip_cred_info_Object;

/*
 * cred_info_dealloc
 * deletes a cred info from memory
 */
static void pjsip_cred_info_dealloc(pjsip_cred_info_Object* self)
{
    Py_XDECREF(self->realm);
    Py_XDECREF(self->scheme);
	Py_XDECREF(self->username);
	Py_XDECREF(self->data);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * cred_info_new
 * constructor for cred_info object
 */
static PyObject * pjsip_cred_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    pjsip_cred_info_Object *self;

    self = (pjsip_cred_info_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->realm = PyString_FromString("");
        if (self->realm == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
         self->scheme = PyString_FromString("");
        if (self->scheme == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
		 self->username = PyString_FromString("");
        if (self->username == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
		 self->data = PyString_FromString("");
        if (self->data == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * pjsip_cred_info_members
 */
static PyMemberDef pjsip_cred_info_members[] =
{
	{
    	"realm", T_OBJECT_EX,
    	offsetof(pjsip_cred_info_Object, realm), 0,
    	"Realm"
    },
	{
    	"scheme", T_OBJECT_EX,
    	offsetof(pjsip_cred_info_Object, scheme), 0,
    	"Scheme"
    },
	{
    	"username", T_OBJECT_EX,
    	offsetof(pjsip_cred_info_Object, username), 0,
    	"User name"
    },
	{
    	"data", T_OBJECT_EX,
    	offsetof(pjsip_cred_info_Object, data), 0,
    	"The data, which can be a plaintext password or a hashed digest. "
    },
    {
    	"data_type", T_INT, offsetof(pjsip_cred_info_Object, data_type), 0,
    	"Type of data"
    },
    
    {NULL}  /* Sentinel */
};

/*
 * pjsip_cred_info_Type
 */
static PyTypeObject pjsip_cred_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJSIP_Cred_Info",      /*tp_name*/
    sizeof(pjsip_cred_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)pjsip_cred_info_dealloc,/*tp_dealloc*/
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
    "PJSIP Cred Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjsip_cred_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    pjsip_cred_info_new,             /* tp_new */

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
	pjsip_cred_info s;
	pjsip_cred_info d;
    if (!PyArg_ParseTuple(pArgs, "OOO", &pool, &dest, &src))
    {
        return NULL;
    }
	s.data.ptr = PyString_AsString(src->data);
	s.data.slen = strlen(PyString_AsString(src->data));
	s.realm.ptr = PyString_AsString(src->realm);
	s.realm.slen = strlen(PyString_AsString(src->realm));
	s.scheme.ptr = PyString_AsString(src->scheme);
	s.scheme.slen = strlen(PyString_AsString(src->scheme));
	s.username.ptr = PyString_AsString(src->username);
	s.username.slen = strlen(PyString_AsString(src->username));
	s.data_type = src->data_type;
	d.data.ptr = PyString_AsString(dest->data);
	d.data.slen = strlen(PyString_AsString(dest->data));
	d.realm.ptr = PyString_AsString(dest->realm);
	d.realm.slen = strlen(PyString_AsString(dest->realm));
	d.scheme.ptr = PyString_AsString(dest->scheme);
	d.scheme.slen = strlen(PyString_AsString(dest->scheme));
	d.username.ptr = PyString_AsString(dest->username);
	d.username.slen = strlen(PyString_AsString(dest->username));
	d.data_type = dest->data_type;
    pjsip_cred_dup(pool->pool, &d, &s);
	Py_XDECREF(src->data);
	src->data = PyString_FromStringAndSize(s.data.ptr, s.data.slen);
	Py_XDECREF(src->realm);
	src->realm = PyString_FromStringAndSize(s.realm.ptr, s.realm.slen);
	Py_XDECREF(src->scheme);
	src->scheme = PyString_FromStringAndSize(s.scheme.ptr, s.scheme.slen);
	Py_XDECREF(src->username);
	src->username = 
		PyString_FromStringAndSize(s.username.ptr, s.username.slen);
    Py_INCREF(Py_None);
	src->data_type = s.data_type;
	Py_XDECREF(dest->data);
	dest->data = PyString_FromStringAndSize(d.data.ptr, d.data.slen);
	Py_XDECREF(dest->realm);
	dest->realm = PyString_FromStringAndSize(d.realm.ptr, d.realm.slen);
	Py_XDECREF(dest->scheme);
	dest->scheme = PyString_FromStringAndSize(d.scheme.ptr, d.scheme.slen);
	Py_XDECREF(dest->username);
	dest->username = 
		PyString_FromStringAndSize(d.username.ptr, d.username.slen);
    Py_INCREF(Py_None);
	src->data_type = s.data_type;
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

/* END OF LIB BASE */

/* LIB TRANSPORT */

/*
 * stun_config_Object
 * STUN configuration
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
	PyObject * stun_srv1;    
    unsigned stun_port1;
    PyObject * stun_srv2;
    unsigned stun_port2;    
} stun_config_Object;


/*
 * stun_config_dealloc
 * deletes a stun config from memory
 */
static void stun_config_dealloc(stun_config_Object* self)
{
    Py_XDECREF(self->stun_srv1);
    Py_XDECREF(self->stun_srv2);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * stun_config_new
 * constructor for stun_config object
 */
static PyObject * stun_config_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    stun_config_Object *self;	
    self = (stun_config_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->stun_srv1 = PyString_FromString("");
        if (self->stun_srv1 == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->stun_srv2 = PyString_FromString("");
        if (self->stun_srv2 == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * stun_config_members
 */
static PyMemberDef stun_config_members[] =
{
    {
    	"stun_port1", T_INT, offsetof(stun_config_Object, stun_port1), 0,
    	"The first STUN server IP address or hostname."
    },
    {
    	"stun_port2", T_INT, offsetof(stun_config_Object, stun_port2), 0,
    	"Port number of the second STUN server. "
		"If zero, default STUN port will be used."
    },    
    {
    	"stun_srv1", T_OBJECT_EX,
    	offsetof(stun_config_Object, stun_srv1), 0,
    	"The first STUN server IP address or hostname"
    },
    {
    	"stun_srv2", T_OBJECT_EX,
    	offsetof(stun_config_Object, stun_srv2), 0,
    	"Optional second STUN server IP address or hostname, for which the "
		"result of the mapping request will be compared to. If the value "
		"is empty, only one STUN server will be used"
    },
    {NULL}  /* Sentinel */
};




/*
 * stun_config_Type
 */
static PyTypeObject stun_config_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.STUN_Config",      /*tp_name*/
    sizeof(stun_config_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)stun_config_dealloc,/*tp_dealloc*/
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
    "STUN Config objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    stun_config_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    stun_config_new,             /* tp_new */

};

/*
 * transport_config_Object
 * Transport configuration for creating UDP transports for both SIP
 * and media.
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
	unsigned port;
	PyObject * public_addr;
	PyObject * bound_addr;
	int use_stun;
	stun_config_Object * stun_config;
} transport_config_Object;


/*
 * transport_config_dealloc
 * deletes a transport config from memory
 */
static void transport_config_dealloc(transport_config_Object* self)
{
	Py_XDECREF(self->public_addr);    
	Py_XDECREF(self->bound_addr);    
    Py_XDECREF(self->stun_config);    
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * transport_config_new
 * constructor for transport_config object
 */
static PyObject * transport_config_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    transport_config_Object *self;

    self = (transport_config_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
		self->public_addr = PyString_FromString("");
        if (self->public_addr == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
		self->bound_addr = PyString_FromString("");
        if (self->bound_addr == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->stun_config = 
			(stun_config_Object *)stun_config_new(&stun_config_Type,NULL,NULL);
        if (self->stun_config == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        
    }

    return (PyObject *)self;
}


/*
 * transport_config_members
 */
static PyMemberDef transport_config_members[] =
{
    {
    	"port", T_INT, offsetof(transport_config_Object, port), 0,
    	"UDP port number to bind locally. This setting MUST be specified "
		"even when default port is desired. If the value is zero, the "
		"transport will be bound to any available port, and application "
		"can query the port by querying the transport info."
    },
    {
    	"public_addr", T_OBJECT_EX, 
		offsetof(transport_config_Object, public_addr), 0,
    	"Optional address to advertise as the address of this transport. "
		"Application can specify any address or hostname for this field, "
		"for example it can point to one of the interface address in the "
		"system, or it can point to the public address of a NAT router "
		"where port mappings have been configured for the application."		
    },    
	{
    	"bound_addr", T_OBJECT_EX, 
		offsetof(transport_config_Object, bound_addr), 0,
    	"Optional address where the socket should be bound to. This option "
		"SHOULD only be used to selectively bind the socket to particular "
		"interface (instead of 0.0.0.0), and SHOULD NOT be used to set the "
		"published address of a transport (the public_addr field should be "
		"used for that purpose)."		
    },    
    {
    	"use_stun", T_INT,
    	offsetof(transport_config_Object, use_stun), 0,
    	"Flag to indicate whether STUN should be used."
    },
    {
    	"stun_config", T_OBJECT_EX,
    	offsetof(transport_config_Object, stun_config), 0,
    	"STUN configuration, must be specified when STUN is used."
    },
    {NULL}  /* Sentinel */
};




/*
 * transport_config_Type
 */
static PyTypeObject transport_config_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Transport_Config",      /*tp_name*/
    sizeof(transport_config_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)transport_config_dealloc,/*tp_dealloc*/
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
    "Transport Config objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    transport_config_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    transport_config_new,             /* tp_new */

};

/*
 * sockaddr_Object
 * C/Python wrapper for sockaddr object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
    pj_uint8_t  sa_zero_len;
    pj_uint8_t  sa_family;
#else
    pj_uint16_t	sa_family;	/**< Common data: address family.   */
#endif
    PyObject * sa_data;	/**< Address data.		    */
} sockaddr_Object;

/*
 * sockaddr_dealloc
 * deletes a sockaddr from memory
 */
static void sockaddr_dealloc(sockaddr_Object* self)
{
    Py_XDECREF(self->sa_data);    
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * sockaddr_new
 * constructor for sockaddr object
 */
static PyObject * sockaddr_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    sockaddr_Object *self;

    self = (sockaddr_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->sa_data = PyString_FromString("");
        if (self->sa_data == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        
    }

    return (PyObject *)self;
}


/*
 * sockaddr_members
 * declares attributes accessible from both C and Python for sockaddr object
 */
static PyMemberDef sockaddr_members[] =
{
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
    {
        "sa_zero_len", T_INT, offsetof(sockaddr_Object, sa_zero_len), 0,
        ""
    },
    {
        "sa_family", T_INT,
        offsetof(sockaddr_Object, sa_family), 0,
        "Common data: address family."
    },
#else
    {
        "sa_family", T_INT,
        offsetof(sockaddr_Object, sa_family), 0,
        "Common data: address family."
    },
#endif
    {
    	"sa_data", T_OBJECT_EX,
    	offsetof(sockaddr_Object, sa_data), 0,
    	"Address data"
    },
    {NULL}  /* Sentinel */
};


/*
 * sockaddr_Type
 */
static PyTypeObject sockaddr_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Sockaddr",        /*tp_name*/
    sizeof(sockaddr_Object),    /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)sockaddr_dealloc,/*tp_dealloc*/
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
    "Sockaddr objects",         /*tp_doc*/
    0,                              /*tp_traverse*/
    0,                              /*tp_clear*/
    0,                              /*tp_richcompare*/
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    sockaddr_members,           /* tp_members */
	0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    sockaddr_new,             /* tp_new */
};

/*
 * host_port_Object
 * C/Python wrapper for host_port object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    PyObject * host;
	int port;
} host_port_Object;

/*
 * host_port_dealloc
 * deletes a host_port from memory
 */
static void host_port_dealloc(host_port_Object* self)
{
    Py_XDECREF(self->host);    
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * host_port_new
 * constructor for host_port object
 */
static PyObject * host_port_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    host_port_Object *self;

    self = (host_port_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->host = PyString_FromString("");
        if (self->host == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        
    }

    return (PyObject *)self;
}


/*
 * host_port_members
 * declares attributes accessible from both C and Python for host_port object
 */
static PyMemberDef host_port_members[] =
{    
    {
        "port", T_INT,
        offsetof(host_port_Object, port), 0,
        "Port number."
    },
    {
    	"host", T_OBJECT_EX,
    	offsetof(host_port_Object, host), 0,
    	"Host part or IP address."
    },
    {NULL}  /* Sentinel */
};


/*
 * host_port_Type
 */
static PyTypeObject host_port_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Host_Port",        /*tp_name*/
    sizeof(host_port_Object),    /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)host_port_dealloc,/*tp_dealloc*/
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
    "Host_port objects",         /*tp_doc*/
    0,                              /*tp_traverse*/
    0,                              /*tp_clear*/
    0,                              /*tp_richcompare*/
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    host_port_members,           /* tp_members */
	0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    host_port_new,             /* tp_new */
};

/*
 * transport_id_Object
 * C/Python wrapper for transport_id object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    int transport_id;
} transport_id_Object;


/*
 * transport_id_members
 * declares attributes accessible from 
 * both C and Python for transport_id object
 */
static PyMemberDef transport_id_members[] =
{
    {
        "transport_id", T_INT, offsetof(transport_id_Object, transport_id), 0,
        "SIP transport identification"
    },
    {NULL}  /* Sentinel */
};


/*
 * transport_id_Type
 */
static PyTypeObject transport_id_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Transport_ID",        /*tp_name*/
    sizeof(transport_id_Object),    /*tp_basicsize*/
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
    "Transport ID objects",         /*tp_doc*/
    0,                              /*tp_traverse*/
    0,                              /*tp_clear*/
    0,                              /*tp_richcompare*/
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    transport_id_members,           /* tp_members */

};

/*
 * integer_Object
 * C/Python wrapper for integer object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    int integer;
} integer_Object;


/*
 * integer_members
 * declares attributes accessible from both C and Python for integer object
 */
static PyMemberDef integer_members[] =
{
    {
        "integer", T_INT, offsetof(integer_Object, integer), 0,
        "integer value"
    },
    {NULL}  /* Sentinel */
};


/*
 * integer_Type
 */
static PyTypeObject integer_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Integer",        /*tp_name*/
    sizeof(integer_Object),    /*tp_basicsize*/
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
    "Integer objects",         /*tp_doc*/
    0,                              /*tp_traverse*/
    0,                              /*tp_clear*/
    0,                              /*tp_richcompare*/
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    integer_members,           /* tp_members */

};


/*
 * transport_info_Object
 * Transport info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
	int id;
	int type;
	PyObject * type_name;
	PyObject * info;
	unsigned flag;
	unsigned addr_len;
	sockaddr_Object * local_addr;
	host_port_Object * local_name;
	unsigned usage_count;
} transport_info_Object;


/*
 * transport_info_dealloc
 * deletes a transport info from memory
 */
static void transport_info_dealloc(transport_info_Object* self)
{
    Py_XDECREF(self->type_name); 
	Py_XDECREF(self->info);
	Py_XDECREF(self->local_addr);
	Py_XDECREF(self->local_name);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * transport_info_new
 * constructor for transport_info object
 */
static PyObject * transport_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    transport_info_Object *self;

    self = (transport_info_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
		self->type_name = PyString_FromString("");
        if (self->type_name == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
		self->info = PyString_FromString("");
        if (self->info == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->local_addr = 
			(sockaddr_Object *)sockaddr_new(&sockaddr_Type,NULL,NULL);
        if (self->local_addr == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->local_name = 
			(host_port_Object *)host_port_new(&host_port_Type,NULL,NULL);
        if (self->local_name == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * transport_info_members
 */
static PyMemberDef transport_info_members[] =
{
	{
    	"id", T_INT, offsetof(transport_info_Object, id), 0,
    	"PJSUA transport identification."
    },
	{
    	"type", T_INT, offsetof(transport_info_Object, id), 0,
    	"Transport type."
    },
	{
    	"type_name", T_OBJECT_EX,
    	offsetof(transport_info_Object, type_name), 0,
    	"Transport type name."
    },
	{
    	"info", T_OBJECT_EX,
    	offsetof(transport_info_Object, info), 0,
    	"Transport string info/description."
    },
	{
    	"flag", T_INT, offsetof(transport_info_Object, flag), 0,
    	"Transport flag (see ##pjsip_transport_flags_e)."
    },
	{
    	"addr_len", T_INT, offsetof(transport_info_Object, addr_len), 0,
    	"Local address length."
    },
	{
    	"local_addr", T_OBJECT_EX,
    	offsetof(transport_info_Object, local_addr), 0,
    	"Local/bound address."
    },
	{
    	"local_name", T_OBJECT_EX,
    	offsetof(transport_info_Object, local_name), 0,
    	"Published address (or transport address name)."
    },
	{
    	"usage_count", T_INT, offsetof(transport_info_Object, usage_count), 0,
    	"Current number of objects currently referencing this transport."
    },    
    {NULL}  /* Sentinel */
};




/*
 * transport_info_Type
 */
static PyTypeObject transport_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Transport_Info",      /*tp_name*/
    sizeof(transport_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)transport_info_dealloc,/*tp_dealloc*/
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
    "Transport Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    transport_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    transport_info_new,             /* tp_new */

};

/*
 * pjsip_transport_Object
 * C/python typewrapper for pjsip_transport
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_transport *tp;
} pjsip_transport_Object;


/*
 * pjsip_transport_Type
 */
static PyTypeObject pjsip_transport_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJSIP_Transport",       /*tp_name*/
    sizeof(pjsip_transport_Object),   /*tp_basicsize*/
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
    "pjsip_transport objects",        /*tp_doc*/
};


/*
 * py_pjsua_stun_config_default
 */
static PyObject *py_pjsua_stun_config_default(PyObject *pSelf, PyObject *pArgs)
{
    stun_config_Object *obj;
    pjsua_stun_config cfg;

    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
    pjsua_stun_config_default(&cfg);
    obj->stun_port1 = cfg.stun_port1;
	obj->stun_port2 = cfg.stun_port2;
	Py_XDECREF(obj->stun_srv1);
    obj->stun_srv1 = 
		PyString_FromStringAndSize(cfg.stun_srv1.ptr, cfg.stun_srv1.slen);
	Py_XDECREF(obj->stun_srv2);
	obj->stun_srv2 = 
		PyString_FromStringAndSize(cfg.stun_srv2.ptr, cfg.stun_srv2.slen);
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_transport_config_default
 */
static PyObject *py_pjsua_transport_config_default
(PyObject *pSelf, PyObject *pArgs)
{
    transport_config_Object *obj;
    pjsua_transport_config cfg;

    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
    pjsua_transport_config_default(&cfg);
    obj->public_addr = 
		PyString_FromStringAndSize(cfg.public_addr.ptr, cfg.public_addr.slen);
	obj->bound_addr = 
		PyString_FromStringAndSize(cfg.bound_addr.ptr, cfg.bound_addr.slen);
	obj->port = cfg.port;
	obj->use_stun = cfg.use_stun;
	Py_XDECREF(obj->stun_config);
	obj->stun_config = 
		(stun_config_Object *)stun_config_new(&stun_config_Type, NULL, NULL);
    obj->stun_config->stun_port1 = cfg.stun_config.stun_port1;
	obj->stun_config->stun_port2 = cfg.stun_config.stun_port2;
	Py_XDECREF(obj->stun_config->stun_srv1);
    obj->stun_config->stun_srv1 = 
		PyString_FromStringAndSize(cfg.stun_config.stun_srv1.ptr, 
		cfg.stun_config.stun_srv1.slen);
	Py_XDECREF(obj->stun_config->stun_srv2);
	obj->stun_config->stun_srv2 = 
		PyString_FromStringAndSize(cfg.stun_config.stun_srv2.ptr, 
		cfg.stun_config.stun_srv2.slen);
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_normalize_stun_config
 */
static PyObject *py_pjsua_normalize_stun_config
(PyObject *pSelf, PyObject *pArgs)
{
    stun_config_Object *obj;
    pjsua_stun_config *cfg;

    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
	cfg = (pjsua_stun_config *)malloc(sizeof(pjsua_stun_config));
	cfg->stun_port1 = obj->stun_port1;
	cfg->stun_port2 = obj->stun_port2;
	cfg->stun_srv1.ptr = PyString_AsString(obj->stun_srv1);
	cfg->stun_srv1.slen = strlen(PyString_AsString(obj->stun_srv1));
	cfg->stun_srv2.ptr = PyString_AsString(obj->stun_srv2);
	cfg->stun_srv2.slen = strlen(PyString_AsString(obj->stun_srv2));
    pjsua_normalize_stun_config(cfg);
    obj->stun_port1 = cfg->stun_port1;
	obj->stun_port2 = cfg->stun_port2;
	Py_XDECREF(obj->stun_srv1);
    obj->stun_srv1 = 
		PyString_FromStringAndSize(cfg->stun_srv1.ptr, cfg->stun_srv1.slen);
	Py_XDECREF(obj->stun_srv2);
	obj->stun_srv2 = 
		PyString_FromStringAndSize(cfg->stun_srv2.ptr, cfg->stun_srv2.slen);
	free(cfg);
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_transport_config_dup
 */
static PyObject *py_pjsua_transport_config_dup
(PyObject *pSelf, PyObject *pArgs)
{
    pj_pool_Object *pool;
    transport_config_Object *src;
    transport_config_Object *dest;
	pj_pool_t *p;
	pjsua_transport_config s;
	pjsua_transport_config d;	

    if (!PyArg_ParseTuple(pArgs, "OOO", &pool, &dest, &src))
    {
        return NULL;
    }
    p = pool->pool;
	s.public_addr.ptr = PyString_AsString(src->public_addr);
	s.public_addr.slen = strlen(PyString_AsString(src->public_addr));
	s.bound_addr.ptr = PyString_AsString(src->bound_addr);
	s.bound_addr.slen = strlen(PyString_AsString(src->bound_addr));
	s.port = src->port;
	s.use_stun = src->use_stun;
	s.stun_config.stun_port1 = src->stun_config->stun_port1;
	s.stun_config.stun_port2 = src->stun_config->stun_port2;
	s.stun_config.stun_srv1.ptr = 
		PyString_AsString(src->stun_config->stun_srv1);
	s.stun_config.stun_srv1.slen = 
		strlen(PyString_AsString(src->stun_config->stun_srv1));
	s.stun_config.stun_srv2.ptr = 
		PyString_AsString(src->stun_config->stun_srv2);
	s.stun_config.stun_srv2.slen = 
		strlen(PyString_AsString(src->stun_config->stun_srv2));
	d.public_addr.ptr = PyString_AsString(dest->public_addr);
	d.public_addr.slen = strlen(PyString_AsString(dest->public_addr));
	d.bound_addr.ptr = PyString_AsString(dest->bound_addr);
	d.bound_addr.slen = strlen(PyString_AsString(dest->bound_addr));
	d.port = dest->port;
	d.use_stun = dest->use_stun;
	d.stun_config.stun_port1 = dest->stun_config->stun_port1;
	d.stun_config.stun_port2 = dest->stun_config->stun_port2;
	d.stun_config.stun_srv1.ptr = 
		PyString_AsString(dest->stun_config->stun_srv1);
	d.stun_config.stun_srv1.slen = 
		strlen(PyString_AsString(dest->stun_config->stun_srv1));
	d.stun_config.stun_srv2.ptr = 
		PyString_AsString(dest->stun_config->stun_srv2);
	d.stun_config.stun_srv2.slen = 
		strlen(PyString_AsString(dest->stun_config->stun_srv2));
	pjsua_transport_config_dup(p, &d, &s);
	src->public_addr = 
		PyString_FromStringAndSize(s.public_addr.ptr, s.public_addr.slen);
	src->bound_addr = 
		PyString_FromStringAndSize(s.bound_addr.ptr, s.bound_addr.slen);
	src->port = s.port;
	src->use_stun = s.use_stun;
	src->stun_config->stun_port1 = s.stun_config.stun_port1;
	src->stun_config->stun_port2 = s.stun_config.stun_port2;
	Py_XDECREF(src->stun_config->stun_srv1);
	src->stun_config->stun_srv1 = 
		PyString_FromStringAndSize(s.stun_config.stun_srv1.ptr, 
		s.stun_config.stun_srv1.slen);
	Py_XDECREF(src->stun_config->stun_srv2);
	src->stun_config->stun_srv2 = 
		PyString_FromStringAndSize(s.stun_config.stun_srv2.ptr, 
		s.stun_config.stun_srv2.slen);	
	dest->public_addr = 
		PyString_FromStringAndSize(d.public_addr.ptr, d.public_addr.slen);
	dest->bound_addr = 
		PyString_FromStringAndSize(d.bound_addr.ptr, d.bound_addr.slen);
	dest->port = d.port;
	dest->use_stun = d.use_stun;
	dest->stun_config->stun_port1 = d.stun_config.stun_port1;
	dest->stun_config->stun_port2 = d.stun_config.stun_port2;
	Py_XDECREF(dest->stun_config->stun_srv1);
	dest->stun_config->stun_srv1 = 
		PyString_FromStringAndSize(d.stun_config.stun_srv1.ptr, 
		d.stun_config.stun_srv1.slen);
	Py_XDECREF(dest->stun_config->stun_srv2);
	dest->stun_config->stun_srv2 = 
		PyString_FromStringAndSize(d.stun_config.stun_srv2.ptr, 
		d.stun_config.stun_srv2.slen);
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_transport_create
 */
static PyObject *py_pjsua_transport_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
	int type;
	transport_id_Object *p_id;
	transport_config_Object *obj;
	pjsua_transport_config cfg;
	pjsua_transport_id id;
    if (!PyArg_ParseTuple(pArgs, "iOO", &type, &obj, &p_id))
    {
        return NULL;
    }
	cfg.public_addr.ptr = PyString_AsString(obj->public_addr);
	cfg.public_addr.slen = strlen(PyString_AsString(obj->public_addr));
	cfg.bound_addr.ptr = PyString_AsString(obj->bound_addr);
	cfg.bound_addr.slen = strlen(PyString_AsString(obj->bound_addr));
	cfg.port = obj->port;
	cfg.use_stun = obj->use_stun;
	cfg.stun_config.stun_port1 = obj->stun_config->stun_port1;
	cfg.stun_config.stun_port2 = obj->stun_config->stun_port2;
	cfg.stun_config.stun_srv1.ptr = 
		PyString_AsString(obj->stun_config->stun_srv1);
	cfg.stun_config.stun_srv1.slen = 
		strlen(PyString_AsString(obj->stun_config->stun_srv1));
	cfg.stun_config.stun_srv2.ptr = 
		PyString_AsString(obj->stun_config->stun_srv2);
	cfg.stun_config.stun_srv2.slen = 
		strlen(PyString_AsString(obj->stun_config->stun_srv2));
    status = pjsua_transport_create(type, &cfg, &id);
	p_id->transport_id = id;
    printf("status %d\n",status);
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_transport_register
 */
static PyObject *py_pjsua_transport_register(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;	
	transport_id_Object *p_id;
	pjsip_transport_Object *obj;	
	pjsua_transport_id id;
    if (!PyArg_ParseTuple(pArgs, "OO", &obj, &p_id))
    {
        return NULL;
    }
	
	id = p_id->transport_id;
    status = pjsua_transport_register(obj->tp, &id);
	p_id->transport_id = id;    
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_enum_transports
 */
static PyObject *py_pjsua_enum_transports(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
	PyObject *list;
	integer_Object *count;
	pjsua_transport_id *id;
	int c, i;
    if (!PyArg_ParseTuple(pArgs, "OO", &list, &count))
    {
        return NULL;
    }	
	c = count->integer;
	id = (pjsua_transport_id *)malloc(c * sizeof(pjsua_transport_id));
    status = pjsua_enum_transports(id, &c);
	Py_XDECREF(list);
	list = PyList_New(c);
	for (i = 0; i < c; i++) {
		int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
		if (ret == -1) {
			return NULL;
		}
	}
	count->integer = c;
	free(id);
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_transport_get_info
 */
static PyObject *py_pjsua_transport_get_info(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
	int id;
	transport_info_Object *obj;
	pjsua_transport_info info;
	char * str;
	int len;
	int i;

    if (!PyArg_ParseTuple(pArgs, "iO", &id, &obj))
    {
        return NULL;
    }	
	info.addr_len = obj->addr_len;
	info.flag = obj->flag;
	info.id = obj->id;
	info.info.ptr = PyString_AsString(obj->info);
	info.info.slen = strlen(PyString_AsString(obj->info));
	str = PyString_AsString(obj->local_addr->sa_data);
	len = strlen(str);
	if (len > 14) {
		len = 14;
	}
	for (i = 0; i < len; i++) {
		info.local_addr.sa_data[i] = str[i];
	}
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
	info.local_addr.sa_zero_len = obj->local_addr->sa_zero_len;
	info.local_addr.sa_family = obj->local_addr->sa_family;
#else
	info.local_addr.sa_family = obj->local_addr->sa_family;
#endif
    status = pjsua_transport_get_info(id, &info);	
    obj->addr_len = info.addr_len;
	obj->flag = info.flag;
	obj->id = info.id;
	obj->info = PyString_FromStringAndSize(info.info.ptr, info.info.slen);
	obj->local_addr->sa_data = 
		PyString_FromStringAndSize(info.local_addr.sa_data, 14);
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
	obj->local_addr->sa_zero_len = info.local_addr.sa_zero_len;
	obj->local_addr->sa_family = info.local_addr.sa_family;
#else
	obj->local_addr->sa_family = info.local_addr.sa_family;
#endif
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_transport_set_enable
 */
static PyObject *py_pjsua_transport_set_enable
(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
	int id;
	int enabled;
    if (!PyArg_ParseTuple(pArgs, "ii", &id, &enabled))
    {
        return NULL;
    }	
    status = pjsua_transport_set_enable(id, enabled);	
    //printf("status %d\n",status);
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_transport_close
 */
static PyObject *py_pjsua_transport_close(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
	int id;
	int force;
    if (!PyArg_ParseTuple(pArgs, "ii", &id, &force))
    {
        return NULL;
    }	
    status = pjsua_transport_close(id, force);	
    //printf("status %d\n",status);
    return Py_BuildValue("i",status);
}

static char pjsua_stun_config_default_doc[] =
    "void py_pjsua.stun_config_default (py_pjsua.STUN_Config cfg) "
    "Call this function to initialize STUN config with default values.";
static char pjsua_transport_config_default_doc[] =
	"void py_pjsua.transport_config_default (py_pjsua.Transport_Config cfg) "
	"Call this function to initialize UDP config with default values.";
static char pjsua_normalize_stun_config_doc[] =
	"void py_pjsua.normalize_stun_config (py_pjsua.STUN_Config cfg) "
	"Normalize STUN config. ";
static char pjsua_transport_config_dup_doc[] =
	"void py_pjsua.transport_config_dup (py_pjsua.Pool pool, "
	"py_pjsua.Transport_Config dest, py_pjsua.Transport_Config dest) "
	"Duplicate transport config. ";
static char pjsua_transport_create_doc[] =
	"void py_pjsua.transport_create (int type, "
	"py_pjsua.Transport_Config cfg, py_pjsua.Transport_ID p_id) "
	"Create SIP transport.";
static char pjsua_transport_register_doc[] =
	"void py_pjsua.transport_register "
	"(py_pjsua.PJSIP_Transport tp, py_pjsua.Transport_ID p_id) "
	"Register transport that has been created by application.";
static char pjsua_enum_transports_doc[] =
	"void py_pjsua.enum_transports "
	"(py_pjsua.Transport_ID id[], py_pjsua.Integer count) "
	"Enumerate all transports currently created in the system.";
static char pjsua_transport_get_info_doc[] =
	"void py_pjsua.transport_get_info "
	"(py_pjsua.Transport_ID id, py_pjsua.Transport_Info info) "
	"Get information about transports.";
static char pjsua_transport_set_enable_doc[] =
	"void py_pjsua.transport_set_enable "
	"(py_pjsua.Transport_ID id, int enabled) "
	"Disable a transport or re-enable it. "
	"By default transport is always enabled after it is created. "
	"Disabling a transport does not necessarily close the socket, "
	"it will only discard incoming messages and prevent the transport "
	"from being used to send outgoing messages.";
static char pjsua_transport_close_doc[] =
	"void py_pjsua.transport_close (py_pjsua.Transport_ID id, int force) "
	"Close the transport. If transport is forcefully closed, "
	"it will be immediately closed, and any pending transactions "
	"that are using the transport may not terminate properly. "
	"Otherwise, the system will wait until all transactions are closed "
	"while preventing new users from using the transport, and will close "
	"the transport when it is safe to do so.";

/* END OF LIB TRANSPORT */

/* LIB ACCOUNT */

/*
 * acc_config_Object
 * Acc Config
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
	int priority;	
	PyObject * id;
	PyObject * reg_uri;
	int publish_enabled;
	PyObject * force_contact;
	unsigned proxy_cnt;
	pj_str_t proxy[8];
	unsigned reg_timeout;
	unsigned cred_count;
	pjsip_cred_info cred_info[8];
} acc_config_Object;


/*
 * acc_config_dealloc
 * deletes a acc_config from memory
 */
static void acc_config_dealloc(acc_config_Object* self)
{
    Py_XDECREF(self->id); 
	Py_XDECREF(self->reg_uri);
	Py_XDECREF(self->force_contact);	
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * acc_config_new
 * constructor for acc_config object
 */
static PyObject * acc_config_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    acc_config_Object *self;

    self = (acc_config_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
		self->id = PyString_FromString("");
        if (self->id == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
		self->reg_uri = PyString_FromString("");
        if (self->reg_uri == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->force_contact = PyString_FromString("");
        if (self->force_contact == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}

static PyObject * acc_config_get_proxy
(acc_config_Object *self, PyObject * args)
{
	int idx;
	pj_str_t elmt;
	if (!PyArg_ParseTuple(args,"i",&idx)) 
	{
		return NULL;
	}
	if ((idx >= 0) && (idx < 8)) 
	{
		elmt = self->proxy[idx];
	} 
	else
	{
		return NULL;
	}
	return PyString_FromStringAndSize(elmt.ptr, elmt.slen);
}

static PyObject * acc_config_set_proxy
(acc_config_Object *self, PyObject * args)
{
	int idx;
	PyObject * str;	
	if (!PyArg_ParseTuple(args,"iO",&idx, &str)) 
	{
		return NULL;
	}
	if ((idx >= 0) && (idx < 8)) 
	{
		self->proxy[idx].ptr = PyString_AsString(str);
		self->proxy[idx].slen = strlen(PyString_AsString(str));
	} 
	else
	{
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject * acc_config_get_cred_info
(acc_config_Object *self, PyObject * args)
{
	int idx;
	pjsip_cred_info elmt;
	pjsip_cred_info_Object *obj;
	if (!PyArg_ParseTuple(args,"i",&idx)) 
	{
		return NULL;
	}
	if ((idx >= 0) && (idx < 8)) 
	{
		elmt = self->cred_info[idx];
	} 
	else
	{
		return NULL;
	}
	obj = (pjsip_cred_info_Object *)
		PyType_GenericNew(&pjsip_cred_info_Type, NULL, NULL);
	obj->data = PyString_FromStringAndSize(elmt.data.ptr, elmt.data.slen);
	obj->realm = PyString_FromStringAndSize(elmt.realm.ptr, elmt.realm.slen);
	obj->scheme = 
		PyString_FromStringAndSize(elmt.scheme.ptr, elmt.scheme.slen);
	obj->username = 
		PyString_FromStringAndSize(elmt.username.ptr, elmt.username.slen);
	obj->data_type = elmt.data_type;
	return (PyObject *)obj;
}

static PyObject * acc_config_set_cred_info
(acc_config_Object *self, PyObject * args)
{
	int idx;
	pjsip_cred_info_Object * obj;	
	if (!PyArg_ParseTuple(args,"iO",&idx, &obj)) 
	{
		return NULL;
	}
	if ((idx >= 0) && (idx < 8)) 
	{
		self->cred_info[idx].data.ptr = PyString_AsString(obj->data);
		self->cred_info[idx].data.slen = strlen(PyString_AsString(obj->data));
		self->cred_info[idx].realm.ptr = PyString_AsString(obj->realm);
		self->cred_info[idx].realm.slen = strlen(PyString_AsString(obj->realm));
		self->cred_info[idx].scheme.ptr = PyString_AsString(obj->scheme);
		self->cred_info[idx].scheme.slen = 
			strlen(PyString_AsString(obj->scheme));
		self->cred_info[idx].username.ptr = PyString_AsString(obj->username);
		self->cred_info[idx].username.slen = 
			strlen(PyString_AsString(obj->username));
		self->cred_info[idx].data_type = obj->data_type;
	} 
	else
	{
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef acc_config_methods[] = {
    {
		"get_proxy", (PyCFunction)acc_config_get_proxy, METH_VARARGS,
		"Return proxy at specified index"
    },
	{
		"set_proxy", (PyCFunction)acc_config_set_proxy, METH_VARARGS,
		"Set proxy at specified index"
    },
	{
		"get_cred_info", (PyCFunction)acc_config_get_cred_info, METH_VARARGS,
		"Return cred_info at specified index"
    },
	{
		"set_cred_info", (PyCFunction)acc_config_set_cred_info, METH_VARARGS,
		"Set cred_info at specified index"
    },
    {NULL}  /* Sentinel */
};



/*
 * acc_config_members
 */
static PyMemberDef acc_config_members[] =
{
	{
    	"priority", T_INT, offsetof(acc_config_Object, priority), 0,
    	"Account priority, which is used to control the order of matching "
		"incoming/outgoing requests. The higher the number means the higher "
		"the priority is, and the account will be matched first. "
    },
	{
    	"id", T_OBJECT_EX,
    	offsetof(acc_config_Object, id), 0,
    	"The full SIP URL for the account. "
		"The value can take name address or URL format, "
		"and will look something like 'sip:account@serviceprovider'. "
		"This field is mandatory."
    },
	{
    	"reg_uri", T_OBJECT_EX,
    	offsetof(acc_config_Object, reg_uri), 0,
    	"This is the URL to be put in the request URI for the registration, "
		"and will look something like 'sip:serviceprovider'. "
		"This field should be specified if registration is desired. "
		"If the value is empty, no account registration will be performed. "
    },
	{
    	"publish_enabled", T_INT, 
			offsetof(acc_config_Object, publish_enabled), 0,
    	"Publish presence? "
    },
	{
    	"force_contact", T_OBJECT_EX,
    	offsetof(acc_config_Object, force_contact), 0,
    	"Optional URI to be put as Contact for this account. "
		"It is recommended that this field is left empty, "
		"so that the value will be calculated automatically "
		"based on the transport address. "
    },
	{
    	"proxy_cnt", T_INT, offsetof(acc_config_Object, proxy_cnt), 0,
    	"Number of proxies in the proxy array below. "
    },
	{
    	"reg_timeout", T_INT, offsetof(acc_config_Object, reg_timeout), 0,
    	"Optional interval for registration, in seconds. "
		"If the value is zero, default interval will be used "
		"(PJSUA_REG_INTERVAL, 55 seconds). "
    },
	{
    	"cred_count", T_INT, offsetof(acc_config_Object, cred_count), 0,
    	"Number of credentials in the credential array. "
    },
    {NULL}  /* Sentinel */
};




/*
 * acc_config_Type
 */
static PyTypeObject acc_config_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Acc_Config",      /*tp_name*/
    sizeof(acc_config_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)acc_config_dealloc,/*tp_dealloc*/
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
    "Acc Config objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    acc_config_methods,                              /* tp_methods */
    acc_config_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    acc_config_new,             /* tp_new */

};

/*
 * acc_info_Object
 * Acc Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
	int id;	
	int is_default;
	PyObject * acc_uri;
	int has_registration;
	int expires;
	int status;
	PyObject * status_text;
	int online_status;	
	char buf_[PJ_ERR_MSG_SIZE];
} acc_info_Object;


/*
 * acc_info_dealloc
 * deletes a acc_info from memory
 */
static void acc_info_dealloc(acc_info_Object* self)
{
    Py_XDECREF(self->acc_uri); 
	Py_XDECREF(self->status_text);	
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * acc_info_new
 * constructor for acc_info object
 */
static PyObject * acc_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    acc_info_Object *self;

    self = (acc_info_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
		self->acc_uri = PyString_FromString("");
        if (self->acc_uri == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
		self->status_text = PyString_FromString("");
        if (self->status_text == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        
    }

    return (PyObject *)self;
}

static PyObject * acc_info_get_buf
(acc_info_Object *self, PyObject * args)
{
	int idx;
	char elmt;
	if (!PyArg_ParseTuple(args,"i",&idx)) 
	{
		return NULL;
	}
	if ((idx >= 0) && (idx < PJ_ERR_MSG_SIZE)) 
	{
		elmt = self->buf_[idx];
	} 
	else
	{
		return NULL;
	}
	return PyString_FromStringAndSize(&elmt, 1);
}

static PyObject * acc_info_set_buf
(acc_info_Object *self, PyObject * args)
{
	int idx;
	PyObject * str;	
	char * s;
	if (!PyArg_ParseTuple(args,"iO",&idx, &str)) 
	{
		return NULL;
	}
	if ((idx >= 0) && (idx < PJ_ERR_MSG_SIZE)) 
	{
		s = PyString_AsString(str);
		if (s[0]) 
		{
			self->buf_[idx] = s[0];
		} 
		else 
		{
			return NULL;
		}
	} 
	else
	{
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef acc_info_methods[] = {
    {
		"get_buf", (PyCFunction)acc_info_get_buf, METH_VARARGS,
		"Return buf char at specified index"
    },
	{
		"set_buf", (PyCFunction)acc_info_set_buf, METH_VARARGS,
		"Set buf at specified index"
    },
	
    {NULL}  /* Sentinel */
};



/*
 * acc_info_members
 */
static PyMemberDef acc_info_members[] =
{
	{
    	"id", T_INT, offsetof(acc_info_Object, id), 0,
    	"The account ID."
    },
	{
    	"is_default", T_INT, offsetof(acc_info_Object, is_default), 0,
    	"Flag to indicate whether this is the default account. "
    },
	{
    	"acc_uri", T_OBJECT_EX,
    	offsetof(acc_info_Object, acc_uri), 0,
    	"Account URI"
    },
	{
    	"has_registration", T_INT, offsetof(acc_info_Object, has_registration),
		0,
    	"Flag to tell whether this account has registration setting "
		"(reg_uri is not empty)."
    },
	{
    	"expires", T_INT, offsetof(acc_info_Object, expires), 0,
    	"An up to date expiration interval for account registration session."
    },
	{
    	"status", T_INT, offsetof(acc_info_Object, status), 0,
    	"Last registration status code. If status code is zero, "
		"the account is currently not registered. Any other value indicates "
		"the SIP status code of the registration. "
    },
	{
    	"status_text", T_OBJECT_EX,
    	offsetof(acc_info_Object, status_text), 0,
    	"String describing the registration status."
    },
	{
    	"online_status", T_INT, offsetof(acc_info_Object, online_status), 0,
    	"Presence online status for this account. "
    },
    {NULL}  /* Sentinel */
};




/*
 * acc_info_Type
 */
static PyTypeObject acc_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Acc_Info",      /*tp_name*/
    sizeof(acc_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)acc_info_dealloc,/*tp_dealloc*/
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
    "Acc Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    acc_info_methods,                              /* tp_methods */
    acc_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    acc_info_new,             /* tp_new */

};

/*
 * acc_id_Object
 * C/Python wrapper for acc_id object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    int acc_id;
} acc_id_Object;


/*
 * acc_id_members
 * declares attributes accessible from 
 * both C and Python for acc_id object
 */
static PyMemberDef acc_id_members[] =
{
    {
        "acc_id", T_INT, offsetof(acc_id_Object, acc_id), 0,
        "Account identification"
    },
    {NULL}  /* Sentinel */
};


/*
 * acc_id_Type
 */
static PyTypeObject acc_id_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Acc_ID",        /*tp_name*/
    sizeof(acc_id_Object),    /*tp_basicsize*/
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
    "Acc ID objects",         /*tp_doc*/
    0,                              /*tp_traverse*/
    0,                              /*tp_clear*/
    0,                              /*tp_richcompare*/
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    acc_id_members,           /* tp_members */

};

/*
 * py_pjsua_acc_config_default
 */
static PyObject *py_pjsua_acc_config_default
(PyObject *pSelf, PyObject *pArgs)
{
    acc_config_Object *obj;
    pjsua_acc_config cfg;
	int i;

    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
    pjsua_acc_config_default(&cfg);
	obj->cred_count = cfg.cred_count;
	for (i = 0; i < 8; i++) 
	{
		obj->cred_info[i] = cfg.cred_info[i];
	}
	Py_XDECREF(obj->force_contact);
	obj->force_contact = 
		PyString_FromStringAndSize(cfg.force_contact.ptr, 
		cfg.force_contact.slen);
	obj->priority = cfg.priority;
	Py_XDECREF(obj->id);
	obj->id = 
		PyString_FromStringAndSize(cfg.id.ptr, cfg.id.slen);
	Py_XDECREF(obj->reg_uri);
	obj->reg_uri = 
		PyString_FromStringAndSize(cfg.reg_uri.ptr, cfg.reg_uri.slen);
	obj->proxy_cnt = cfg.proxy_cnt;
	for (i = 0; i < 8; i++) 
	{
		obj->proxy[i] = cfg.proxy[i];
	}
	obj->publish_enabled = cfg.publish_enabled;
	obj->reg_timeout = cfg.reg_timeout;
	
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_acc_get_count
 */
static PyObject *py_pjsua_acc_get_count
(PyObject *pSelf, PyObject *pArgs)
{
	int count;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    count = pjsua_acc_get_count();
    return Py_BuildValue("i",count);
}

/*
 * py_pjsua_acc_is_valid
 */
static PyObject *py_pjsua_acc_is_valid
(PyObject *pSelf, PyObject *pArgs)
{    
	int id;
	int is_valid;

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }
    is_valid = pjsua_acc_is_valid(id);
	
    return Py_BuildValue("i", is_valid);
}

/*
 * py_pjsua_acc_set_default
 */
static PyObject *py_pjsua_acc_set_default
(PyObject *pSelf, PyObject *pArgs)
{    
	int id;
	int status;

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }
    status = pjsua_acc_set_default(id);
	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_get_default
 */
static PyObject *py_pjsua_acc_get_default
(PyObject *pSelf, PyObject *pArgs)
{    
	int id;
	
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    id = pjsua_acc_get_default();
	
    return Py_BuildValue("i", id);
}

/*
 * py_pjsua_acc_add
 */
static PyObject *py_pjsua_acc_add
(PyObject *pSelf, PyObject *pArgs)
{    
	int is_default;
	acc_config_Object * ac;
	pjsua_acc_config cfg;
	acc_id_Object * id;
	int p_acc_id;
	int status;
	int i;

    if (!PyArg_ParseTuple(pArgs, "OiO", &ac, &is_default, &id))
    {
        return NULL;
    }
	cfg.cred_count = ac->cred_count;
	for (i = 0; i < 8; i++) 
	{
		cfg.cred_info[i] = ac->cred_info[i];
	}
	cfg.force_contact.ptr = PyString_AsString(ac->force_contact);
	cfg.force_contact.slen = strlen(PyString_AsString(ac->force_contact));
	cfg.id.ptr = PyString_AsString(ac->id);
	cfg.id.slen = strlen(PyString_AsString(ac->id));
	cfg.priority = ac->priority;
	for (i = 0; i < 8; i++) {
		cfg.proxy[i] = ac->proxy[i];
	}
	cfg.proxy_cnt = ac->proxy_cnt;
	cfg.publish_enabled = ac->publish_enabled;
	cfg.reg_timeout = ac->reg_timeout;
	cfg.reg_uri.ptr = PyString_AsString(ac->reg_uri);
	cfg.reg_uri.slen = strlen(PyString_AsString(ac->reg_uri));
	p_acc_id = id->acc_id;
    status = pjsua_acc_add(&cfg, is_default, &p_acc_id);
	id->acc_id = p_acc_id;
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_add_local
 */
static PyObject *py_pjsua_acc_add_local
(PyObject *pSelf, PyObject *pArgs)
{    
	int is_default;
	int tid;
	acc_id_Object * id;
	int p_acc_id;
	int status;
	

    if (!PyArg_ParseTuple(pArgs, "iiO", &tid, &is_default, &id))
    {
        return NULL;
    }
	
	p_acc_id = id->acc_id;
    status = pjsua_acc_add_local(tid, is_default, &p_acc_id);
	id->acc_id = p_acc_id;
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_del
 */
static PyObject *py_pjsua_acc_del
(PyObject *pSelf, PyObject *pArgs)
{    
	int acc_id;
	int status;
	
    if (!PyArg_ParseTuple(pArgs, "i", &acc_id))
    {
        return NULL;
    }
	
	
    status = pjsua_acc_del(acc_id);	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_modify
 */
static PyObject *py_pjsua_acc_modify
(PyObject *pSelf, PyObject *pArgs)
{    	
	acc_config_Object * ac;
	pjsua_acc_config cfg;	
	int acc_id;
	int status;
	int i;

    if (!PyArg_ParseTuple(pArgs, "iO", &acc_id, &ac))
    {
        return NULL;
    }
	cfg.cred_count = ac->cred_count;
	for (i = 0; i < 8; i++) 
	{
		cfg.cred_info[i] = ac->cred_info[i];
	}
	cfg.force_contact.ptr = PyString_AsString(ac->force_contact);
	cfg.force_contact.slen = strlen(PyString_AsString(ac->force_contact));
	cfg.id.ptr = PyString_AsString(ac->id);
	cfg.id.slen = strlen(PyString_AsString(ac->id));
	cfg.priority = ac->priority;
	for (i = 0; i < 8; i++) {
		cfg.proxy[i] = ac->proxy[i];
	}
	cfg.proxy_cnt = ac->proxy_cnt;
	cfg.publish_enabled = ac->publish_enabled;
	cfg.reg_timeout = ac->reg_timeout;
	cfg.reg_uri.ptr = PyString_AsString(ac->reg_uri);
	cfg.reg_uri.slen = strlen(PyString_AsString(ac->reg_uri));	
    status = pjsua_acc_modify(acc_id, &cfg);	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_set_online_status
 */
static PyObject *py_pjsua_acc_set_online_status
(PyObject *pSelf, PyObject *pArgs)
{    
	int is_online;	
	int acc_id;
	int status;	

    if (!PyArg_ParseTuple(pArgs, "ii", &acc_id, &is_online))
    {
        return NULL;
    }
	
    status = pjsua_acc_set_online_status(acc_id, is_online);
	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_set_registration
 */
static PyObject *py_pjsua_acc_set_registration
(PyObject *pSelf, PyObject *pArgs)
{    
	int renew;	
	int acc_id;
	int status;	

    if (!PyArg_ParseTuple(pArgs, "ii", &acc_id, &renew))
    {
        return NULL;
    }
	
    status = pjsua_acc_set_registration(acc_id, renew);
	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_set_get_info
 */
static PyObject *py_pjsua_acc_get_info
(PyObject *pSelf, PyObject *pArgs)
{    	
	int acc_id;
	acc_info_Object * obj;
	pjsua_acc_info info;
	int status;	
	int i;

    if (!PyArg_ParseTuple(pArgs, "iO", &acc_id, &obj))
    {
        return NULL;
    }
	
	info.acc_uri.ptr = PyString_AsString(obj->acc_uri);
	info.acc_uri.slen = strlen(PyString_AsString(obj->acc_uri));
	for (i = 0; i < PJ_ERR_MSG_SIZE; i++) {
		info.buf_[i] = obj->buf_[i];
	}
	info.expires = obj->expires;
	info.has_registration = obj->has_registration;
	info.id = obj->id;
	info.is_default = obj->is_default;
	info.online_status = obj->online_status;
	info.status = obj->status;
	info.status_text.ptr = PyString_AsString(obj->status_text);
	info.status_text.slen = strlen(PyString_AsString(obj->status_text));
    status = pjsua_acc_get_info(acc_id, &info);
	obj->acc_uri =
		PyString_FromStringAndSize(info.acc_uri.ptr, 
		info.acc_uri.slen);
	for (i = 0; i < PJ_ERR_MSG_SIZE; i++) {
		obj->buf_[i] = info.buf_[i];
	}
	obj->expires = info.expires;
	obj->has_registration = info.has_registration;
	obj->id = info.id;
	obj->is_default = info.is_default;
	obj->online_status = info.online_status;
	obj->status = info.status;
	obj->status_text =
		PyString_FromStringAndSize(info.status_text.ptr, 
		info.status_text.slen);
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_enum_accs
 */
static PyObject *py_pjsua_enum_accs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
	PyObject *list;
	integer_Object *count;
	pjsua_acc_id *id;
	int c, i;
    if (!PyArg_ParseTuple(pArgs, "OO", &list, &count))
    {
        return NULL;
    }	
	c = count->integer;
	id = (pjsua_acc_id *)malloc(c * sizeof(pjsua_acc_id));
    status = pjsua_enum_accs(id, &c);
	Py_XDECREF(list);
	list = PyList_New(c);
	for (i = 0; i < c; i++) {
		int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
		if (ret == -1) {
			return NULL;
		}
	}
	count->integer = c;
	free(id);
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_acc_enum_info
 */
static PyObject *py_pjsua_acc_enum_info(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
	PyObject *list;
	integer_Object *count;
	pjsua_acc_info *info;
	int c, i;
    if (!PyArg_ParseTuple(pArgs, "OO", &list, &count))
    {
        return NULL;
    }	
	c = count->integer;
	info = (pjsua_acc_info *)malloc(c * sizeof(pjsua_acc_info));
    status = pjsua_acc_enum_info(info, &c);
	Py_XDECREF(list);
	list = PyList_New(c);
	for (i = 0; i < c; i++) {
		int ret = PyList_SetItem(list, i, Py_BuildValue("i", info[i]));
		if (ret == -1) {
			return NULL;
		}
	}
	count->integer = c;
	free(info);
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_acc_find_for_outgoing
 */
static PyObject *py_pjsua_acc_find_for_outgoing
(PyObject *pSelf, PyObject *pArgs)
{    
	
	int acc_id;	
	PyObject * url;
	pj_str_t str;

    if (!PyArg_ParseTuple(pArgs, "O", &url))
    {
        return NULL;
    }
	str.ptr = PyString_AsString(url);
	str.slen = strlen(PyString_AsString(url));
	
    acc_id = pjsua_acc_find_for_outgoing(&str);
	
    return Py_BuildValue("i", acc_id);
}

/*
 * py_pjsua_acc_find_for_incoming
 */
static PyObject *py_pjsua_acc_find_for_incoming
(PyObject *pSelf, PyObject *pArgs)
{    	
	int acc_id;	
	pjsip_rx_data_Object * obj;
	pjsip_rx_data * rdata;

    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }
	
	rdata = obj->rdata;
    acc_id = pjsua_acc_find_for_incoming(rdata);
	
    return Py_BuildValue("i", acc_id);
}

/*
 * py_pjsua_acc_create_uac_contact
 */
static PyObject *py_pjsua_acc_create_uac_contact
(PyObject *pSelf, PyObject *pArgs)
{    	
	int status;
	int acc_id;	
	pj_pool_Object * p;
	pj_pool_t * pool;
	PyObject * strc;
	pj_str_t contact;
	PyObject * stru;
	pj_str_t uri;

    if (!PyArg_ParseTuple(pArgs, "OOiO", &p, &strc, &acc_id, &stru))
    {
        return NULL;
    }
	
	pool = p->pool;
	contact.ptr = PyString_AsString(strc);
	contact.slen = strlen(PyString_AsString(strc));
	uri.ptr = PyString_AsString(stru);
	uri.slen = strlen(PyString_AsString(stru));
    status = pjsua_acc_create_uac_contact(pool, &contact, acc_id, &uri);
	strc = PyString_FromStringAndSize(contact.ptr, contact.slen);
	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_create_uas_contact
 */
static PyObject *py_pjsua_acc_create_uas_contact
(PyObject *pSelf, PyObject *pArgs)
{    	
	int status;
	int acc_id;	
	pj_pool_Object * p;
	pj_pool_t * pool;
	PyObject * strc;
	pj_str_t contact;
	pjsip_rx_data_Object * objr;
	pjsip_rx_data * rdata;

    if (!PyArg_ParseTuple(pArgs, "OOiO", &p, &strc, &acc_id, &objr))
    {
        return NULL;
    }
	
	pool = p->pool;
	contact.ptr = PyString_AsString(strc);
	contact.slen = strlen(PyString_AsString(strc));
	rdata = objr->rdata;
    status = pjsua_acc_create_uas_contact(pool, &contact, acc_id, rdata);
	strc = PyString_FromStringAndSize(contact.ptr, contact.slen);
	
    return Py_BuildValue("i", status);
}

static char pjsua_acc_config_default_doc[] =
    "void py_pjsua.acc_config_default (py_pjsua.Acc_Config cfg) "
    "Call this function to initialize account config with default values.";
static char pjsua_acc_get_count_doc[] =
    "int py_pjsua.acc_get_count () "
    "Get number of current accounts.";
static char pjsua_acc_is_valid_doc[] =
    "int py_pjsua.acc_is_valid (int acc_id)  "
    "Check if the specified account ID is valid.";
static char pjsua_acc_set_default_doc[] =
    "int py_pjsua.acc_set_default (int acc_id) "
    "Set default account to be used when incoming "
	"and outgoing requests doesn't match any accounts.";
static char pjsua_acc_get_default_doc[] =
    "int py_pjsua.acc_get_default () "
    "Get default account.";
static char pjsua_acc_add_doc[] =
    "int py_pjsua.acc_add (py_pjsua.Acc_Config cfg, "
	"int is_default, py_pjsua.Acc_ID p_acc_id) "
    "Add a new account to pjsua. PJSUA must have been initialized "
	"(with pjsua_init()) before calling this function.";
static char pjsua_acc_add_local_doc[] =
    "int py_pjsua.acc_add_local (int tid, "
	"int is_default, py_pjsua.Acc_ID p_acc_id) "
    "Add a local account. A local account is used to identify "
	"local endpoint instead of a specific user, and for this reason, "
	"a transport ID is needed to obtain the local address information.";
static char pjsua_acc_del_doc[] =
    "int py_pjsua.acc_del (int acc_id) "
    "Delete account.";
static char pjsua_acc_modify_doc[] =
    "int py_pjsua.acc_modify (int acc_id, py_pjsua.Acc_Config cfg) "
    "Modify account information.";
static char pjsua_acc_set_online_status_doc[] =
    "int py_pjsua.acc_set_online_status (int acc_id, int is_online) "
    "Modify account's presence status to be advertised "
	"to remote/presence subscribers.";
static char pjsua_acc_set_registration_doc[] =
    "int py_pjsua.acc_set_registration (int acc_id, int renew) "
    "Update registration or perform unregistration.";
static char pjsua_acc_get_info_doc[] =
    "int py_pjsua.acc_get_info (int acc_id, py_pjsua.Acc_Info info) "
    "Get account information.";
static char pjsua_enum_accs_doc[] =
    "int py_pjsua.enum_accs (py_pjsua.Acc_ID ids[], py_pjsua.Integer count) "
    "Enum accounts all account ids.";
static char pjsua_acc_enum_info_doc[] =
    "int py_pjsua.acc_enum_info (py_pjsua.Acc_Info info[], "
	"py_pjsua.Integer count) "
    "Enum accounts info.";
static char pjsua_acc_find_for_outgoing_doc[] =
    "int py_pjsua.acc_find_for_outgoing (string url) "
    "This is an internal function to find the most appropriate account "
	"to used to reach to the specified URL.";
static char pjsua_acc_find_for_incoming_doc[] =
    "int py_pjsua.acc_find_for_incoming (pjsip_rx_data_Object rdata) "
    "This is an internal function to find the most appropriate account "
	"to be used to handle incoming calls.";
static char pjsua_acc_create_uac_contact_doc[] =
    "int py_pjsua.acc_create_uac_contact (pj_pool_Object pool, "
	"string contact, int acc_id, string uri) "
    "Create a suitable URI to be put as Contact based on the specified "
	"target URI for the specified account.";
static char pjsua_acc_create_uas_contact_doc[] =
    "int py_pjsua.acc_create_uas_contact (pj_pool_Object pool, "
	"string contact, int acc_id, pjsip_rx_data_Object rdata) "
    "Create a suitable URI to be put as Contact based on the information "
	"in the incoming request.";

/* END OF LIB ACCOUNT */

/* XXX test */
static PyObject *py_my_parse_by_reference
(PyObject *pSelf, PyObject *pArgs)
{    
    PyObject *obj;

    if (!PyArg_ParseTuple(pArgs, "O", &obj))
    {
        return NULL;
    }


    Py_INCREF(Py_None);
    return Py_None;
}

/* XXX end-test */


/*
 * Map of function names to functions
 */
static PyMethodDef py_pjsua_methods[] =
{
    {
	"my_parse_by_reference", py_my_parse_by_reference, METH_VARARGS, ""
    },
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
	{
    	"stun_config_default", py_pjsua_stun_config_default, METH_VARARGS,
    	pjsua_stun_config_default_doc
    },
	{
    	"transport_config_default", py_pjsua_transport_config_default, 
			METH_VARARGS,pjsua_transport_config_default_doc
    },
	{
    	"normalize_stun_config", py_pjsua_normalize_stun_config, METH_VARARGS,
    	pjsua_normalize_stun_config_doc
    },
	{
    	"transport_config_dup", py_pjsua_transport_config_dup, METH_VARARGS,
    	pjsua_transport_config_dup_doc
    },
	{
    	"transport_create", py_pjsua_transport_create, METH_VARARGS,
    	pjsua_transport_create_doc
    },
	{
    	"transport_register", py_pjsua_transport_register, METH_VARARGS,
    	pjsua_transport_register_doc
    },
	{
    	"transport_enum_transports", py_pjsua_enum_transports, METH_VARARGS,
    	pjsua_enum_transports_doc
    },
	{
    	"transport_get_info", py_pjsua_transport_get_info, METH_VARARGS,
    	pjsua_transport_get_info_doc
    },
	{
    	"transport_set_enable", py_pjsua_transport_set_enable, METH_VARARGS,
    	pjsua_transport_set_enable_doc
    },
	{
    	"transport_close", py_pjsua_transport_close, METH_VARARGS,
    	pjsua_transport_close_doc
    },
	{
    	"acc_config_default", py_pjsua_acc_config_default, METH_VARARGS,
    	pjsua_acc_config_default_doc
    },
	{
    	"acc_get_count", py_pjsua_acc_get_count, METH_VARARGS,
    	pjsua_acc_get_count_doc
    },
	{
    	"acc_is_valid", py_pjsua_acc_is_valid, METH_VARARGS,
    	pjsua_acc_is_valid_doc
    },
	{
    	"acc_set_default", py_pjsua_acc_set_default, METH_VARARGS,
    	pjsua_acc_set_default_doc
    },
	{
    	"acc_get_default", py_pjsua_acc_get_default, METH_VARARGS,
    	pjsua_acc_get_default_doc
    },
	{
    	"acc_add", py_pjsua_acc_add, METH_VARARGS,
    	pjsua_acc_add_doc
    },
	{
    	"acc_add_local", py_pjsua_acc_add_local, METH_VARARGS,
    	pjsua_acc_add_local_doc
    },
	{
    	"acc_del", py_pjsua_acc_del, METH_VARARGS,
    	pjsua_acc_del_doc
    },
	{
    	"acc_modify", py_pjsua_acc_modify, METH_VARARGS,
    	pjsua_acc_modify_doc
    },
	{
    	"acc_set_online_status", py_pjsua_acc_set_online_status, METH_VARARGS,
    	pjsua_acc_set_online_status_doc
    },
	{
    	"acc_set_registration", py_pjsua_acc_set_registration, METH_VARARGS,
    	pjsua_acc_set_registration_doc
    },
	{
    	"acc_get_info", py_pjsua_acc_get_info, METH_VARARGS,
    	pjsua_acc_get_info_doc
    },
	{
    	"enum_accs", py_pjsua_enum_accs, METH_VARARGS,
    	pjsua_enum_accs_doc
    },
	{
    	"acc_enum_info", py_pjsua_acc_enum_info, METH_VARARGS,
    	pjsua_acc_enum_info_doc
    },
	{
    	"acc_find_for_outgoing", py_pjsua_acc_find_for_outgoing, METH_VARARGS,
    	pjsua_acc_find_for_outgoing_doc
    },
	{
    	"acc_find_for_incoming", py_pjsua_acc_find_for_incoming, METH_VARARGS,
    	pjsua_acc_find_for_incoming_doc
    },
	{
    	"acc_create_uac_contact", py_pjsua_acc_create_uac_contact, METH_VARARGS,
    	pjsua_acc_create_uac_contact_doc
    },
	{
    	"acc_create_uas_contact", py_pjsua_acc_create_uas_contact, METH_VARARGS,
    	pjsua_acc_create_uas_contact_doc
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

	/* LIB TRANSPORT */

	if (PyType_Ready(&stun_config_Type) < 0)
        return;
	if (PyType_Ready(&transport_config_Type) < 0)
        return;
	if (PyType_Ready(&sockaddr_Type) < 0)
        return;
	if (PyType_Ready(&host_port_Type) < 0)
        return;
	transport_id_Type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&transport_id_Type) < 0)
        return;
	if (PyType_Ready(&transport_info_Type) < 0)
        return;
	integer_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&integer_Type) < 0)
        return;
	pjsip_transport_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjsip_transport_Type) < 0)
        return;

	/* END OF LIB TRANSPORT */

	/* LIB ACCOUNT */

	acc_id_Type.tp_new = PyType_GenericNew;
	if (PyType_Ready(&acc_id_Type) < 0)
        return;
	if (PyType_Ready(&acc_config_Type) < 0)
        return;
	if (PyType_Ready(&acc_info_Type) < 0)
        return;

	/* END OF LIB ACCOUNT */

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

	/* LIB TRANSPORT */

	Py_INCREF(&stun_config_Type);
    PyModule_AddObject(m, "STUN_Config", (PyObject *)&stun_config_Type);
	Py_INCREF(&transport_config_Type);
    PyModule_AddObject
		(m, "Transport_Config", (PyObject *)&transport_config_Type);
	Py_INCREF(&sockaddr_Type);
    PyModule_AddObject(m, "Sockaddr", (PyObject *)&sockaddr_Type);
	Py_INCREF(&host_port_Type);
    PyModule_AddObject(m, "Host_Port", (PyObject *)&host_port_Type);
	Py_INCREF(&transport_id_Type);
    PyModule_AddObject(m, "Transport_ID", (PyObject *)&transport_id_Type);
	Py_INCREF(&transport_info_Type);
    PyModule_AddObject(m, "Transport_Info", (PyObject *)&transport_info_Type);
	Py_INCREF(&integer_Type);
    PyModule_AddObject(m, "Integer", (PyObject *)&integer_Type);
	Py_INCREF(&pjsip_transport_Type);
    PyModule_AddObject(m, "PJSIP_Transport", (PyObject *)&pjsip_transport_Type);

	/* END OF LIB TRANSPORT */

	/* LIB ACCOUNT */

	Py_INCREF(&acc_id_Type);
    PyModule_AddObject(m, "Acc_ID", (PyObject *)&acc_id_Type);
	Py_INCREF(&acc_config_Type);
    PyModule_AddObject(m, "Acc_Config", (PyObject *)&acc_config_Type);
	Py_INCREF(&acc_info_Type);
    PyModule_AddObject(m, "Acc_Info", (PyObject *)&acc_info_Type);

	/* END OF LIB ACCOUNT */

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

#ifdef PJSUA_MAX_ACC
    /*
     * Maximum account.
     */
    PyModule_AddIntConstant(
        m, "PJSUA_MAX_ACC", PJSUA_MAX_ACC
    );
#endif

#ifdef PJSUA_REG_INTERVAL
    /*
     * Default registration interval..
     */
    PyModule_AddIntConstant(
        m, "PJSUA_REG_INTERVAL", PJSUA_REG_INTERVAL
    );
#endif

#ifdef PJSUA_PUBLISH_EXPIRATION
    /*
     * Default PUBLISH expiration
     */
    PyModule_AddIntConstant(
        m, "PJSUA_PUBLISH_EXPIRATION", PJSUA_PUBLISH_EXPIRATION
    );
#endif
	
#ifdef PJSUA_DEFAULT_ACC_PRIORITY
    /*
     * Default account priority.
     */
    PyModule_AddIntConstant(
        m, "PJSUA_DEFAULT_ACC_PRIORITY", PJSUA_DEFAULT_ACC_PRIORITY
    );
#endif

}
