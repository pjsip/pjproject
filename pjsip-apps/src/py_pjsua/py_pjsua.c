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
#define POOL_SIZE    4000
#define SND_DEV_NUM  64

/* LIB BASE */

static PyObject* obj_reconfigure_logging;
static PyObject* obj_logging_init;
static long thread_id;

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
    /* Ignore if this callback is called from alien thread context,
     * or otherwise it will crash Python.
     */
    if (pj_thread_local_get(thread_id) == 0)
	return;

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
static callback_Object * g_obj_callback;


/*
 * cb_on_call_state
 * declares method on_call_state for callback struct
 */
static void cb_on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    if (PyCallable_Check(g_obj_callback->on_call_state))
    {	
        pjsip_event_Object * obj;
		
	obj =
	        (pjsip_event_Object *)PyType_GenericNew(&pjsip_event_Type, 
						    NULL, NULL);
		
	obj->event = e;
		
        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_state,Py_BuildValue("i",call_id),obj,NULL
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
    if (PyCallable_Check(g_obj_callback->on_incoming_call))
    {
	pjsip_rx_data_Object * obj = (pjsip_rx_data_Object *)
				      PyType_GenericNew(&pjsip_rx_data_Type, 
							NULL, NULL);
	obj->rdata = rdata;

        PyObject_CallFunctionObjArgs(
                g_obj_callback->on_incoming_call,
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
    if (PyCallable_Check(g_obj_callback->on_call_media_state))
    {
        PyObject_CallFunction(g_obj_callback->on_call_media_state,"i",call_id);
    }
}


/*
 * Notify application on call being transfered.
 * !modified @061206
 */
static void cb_on_call_transfer_request(pjsua_call_id call_id,
				        const pj_str_t *dst,
				        pjsip_status_code *code)
{
    PyObject * ret;
    int cd;
    if (PyCallable_Check(g_obj_callback->on_call_transfer_request))
    {
        ret = PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_transfer_request,
	    Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(dst->ptr, dst->slen),
            Py_BuildValue("i",*code),
	    NULL
        );
	if (ret != NULL) {
	    if (ret != Py_None) {
		if (PyArg_Parse(ret,"i",&cd)) {
		    *code = cd;
		}
	    }
	}
    }
}


/*
 * Notify application of the status of previously sent call
 * transfer request. Application can monitor the status of the
 * call transfer request, for example to decide whether to 
 * terminate existing call.
 * !modified @061206
 */
static void cb_on_call_transfer_status( pjsua_call_id call_id,
					int status_code,
					const pj_str_t *status_text,
					pj_bool_t final,
					pj_bool_t *p_cont)
{
    PyObject * ret;
    int cnt;
    if (PyCallable_Check(g_obj_callback->on_call_transfer_status))
    {
        ret = PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_transfer_status,
	    Py_BuildValue("i",call_id),
	    Py_BuildValue("i",status_code),
            PyString_FromStringAndSize(status_text->ptr, status_text->slen),
	    Py_BuildValue("i",final),
            Py_BuildValue("i",*p_cont),
	    NULL
        );
	if (ret != NULL) {
	    if (ret != Py_None) {
		if (PyArg_Parse(ret,"i",&cnt)) {
		    *p_cont = cnt;
		}
	    }
	}
    }
}


/*
 * Notify application about incoming INVITE with Replaces header.
 * Application may reject the request by setting non-2xx code.
 * !modified @061206
 */
static void cb_on_call_replace_request( pjsua_call_id call_id,
					pjsip_rx_data *rdata,
					int *st_code,
					pj_str_t *st_text)
{
    PyObject * ret;
    PyObject * txt;
    int cd;
    if (PyCallable_Check(g_obj_callback->on_call_replace_request))
    {
        pjsip_rx_data_Object * obj = (pjsip_rx_data_Object *)
				      PyType_GenericNew(&pjsip_rx_data_Type,
							NULL, NULL);
        obj->rdata = rdata;

        ret = PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_replace_request,
	    Py_BuildValue("i",call_id),
	    obj,
	    Py_BuildValue("i",*st_code),
            PyString_FromStringAndSize(st_text->ptr, st_text->slen),
	    NULL
        );
	if (ret != NULL) {
	    if (ret != Py_None) {
		if (PyArg_ParseTuple(ret,"iO",&cd, &txt)) {
		    *st_code = cd;
		    st_text->ptr = PyString_AsString(txt);
		    st_text->slen = strlen(PyString_AsString(txt));
		}
	    }
	}
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
    if (PyCallable_Check(g_obj_callback->on_call_replaced))
    {
        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_replaced,
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
    if (PyCallable_Check(g_obj_callback->on_reg_state))
    {
        PyObject_CallFunction(g_obj_callback->on_reg_state,"i",acc_id);
    }
}


/*
 * cb_on_buddy_state
 * declares method on_buddy state for callback struct
 */
static void cb_on_buddy_state(pjsua_buddy_id buddy_id)
{
    if (PyCallable_Check(g_obj_callback->on_buddy_state))
    {
        PyObject_CallFunction(g_obj_callback->on_buddy_state,"i",buddy_id);
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
    if (PyCallable_Check(g_obj_callback->on_pager))
    {
        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_pager,Py_BuildValue("i",call_id),
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
    if (PyCallable_Check(g_obj_callback->on_pager))
    {
        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_pager,Py_BuildValue("i",call_id),
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
    if (PyCallable_Check(g_obj_callback->on_typing))
    {
        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_typing,Py_BuildValue("i",call_id),
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
        "on_call_media_state", T_OBJECT_EX,
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
 * !modified @ 061206
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    /*pjsip_hdr hdr_list;*/
    PyObject * hdr_list;
    PyObject * content_type;
    PyObject * msg_body;
} msg_data_Object;


/*
 * msg_data_dealloc
 * deletes a msg_data
 * !modified @ 061206
 */
static void msg_data_dealloc(msg_data_Object* self)
{
    Py_XDECREF(self->hdr_list);
    Py_XDECREF(self->content_type);
    Py_XDECREF(self->msg_body);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * msg_data_new
 * constructor for msg_data object
 * !modified @ 061206
 */
static PyObject * msg_data_new(PyTypeObject *type, PyObject *args,
                                PyObject *kwds)
{
    msg_data_Object *self;

    self = (msg_data_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        Py_INCREF(Py_None);
        self->hdr_list = Py_None;
        if (self->hdr_list == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
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
 * !modified @ 061206
 */
static PyMemberDef msg_data_members[] =
{
    {
        "hdr_list", T_OBJECT_EX, offsetof(msg_data_Object, hdr_list),
	0, "Additional message headers as linked list."
    }, 
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
 * translate_hdr
 * internal function 
 * translate from hdr_list to pjsip_generic_string_hdr
 */
void translate_hdr(pj_pool_t *pool, pjsip_hdr *hdr, PyObject *py_hdr_list)
{
    int i;

    if (PyList_Check(py_hdr_list)) {
        pj_list_init(hdr);

        for (i = 0; i < PyList_Size(py_hdr_list); i++) 
	{ 
            pj_str_t hname, hvalue;
	    pjsip_generic_string_hdr * new_hdr;
            PyObject * tuple = PyList_GetItem(py_hdr_list, i);

            if (PyTuple_Check(tuple)) 
	    {
                hname.ptr = PyString_AsString(PyTuple_GetItem(tuple,0));
                hname.slen = strlen(PyString_AsString
					(PyTuple_GetItem(tuple,0)));
                hvalue.ptr = PyString_AsString(PyTuple_GetItem(tuple,1));
                hvalue.slen = strlen(PyString_AsString
					(PyTuple_GetItem(tuple,1)));
            } else {
		hname.ptr = "";
		hname.slen = 0;
		hvalue.ptr = "";
		hvalue.slen = 0;
            }  
            new_hdr = pjsip_generic_string_hdr_create(pool, &hname, &hvalue);
            pj_list_push_back((pj_list_type *)hdr, (pj_list_type *)new_hdr);
	}     
    }
}

/* 
 * translate_hdr_rev
 * internal function
 * translate from pjsip_generic_string_hdr to hdr_list
 */

void translate_hdr_rev(pjsip_generic_string_hdr *hdr, PyObject *py_hdr_list)
{
    int i;
    int len;
    pjsip_generic_string_hdr * p_hdr;

    len = pj_list_size(hdr);
    
    if (len > 0) 
    {
        p_hdr = hdr;
        Py_XDECREF(py_hdr_list);
        py_hdr_list = PyList_New(len);

        for (i = 0; i < len && p_hdr != NULL; i++) 
	{
            PyObject * tuple;
            PyObject * str;

            tuple = PyTuple_New(2);
	    
            str = PyString_FromStringAndSize(p_hdr->name.ptr, p_hdr->name.slen);
            PyTuple_SetItem(tuple, 0, str);
            str = PyString_FromStringAndSize
		(hdr->hvalue.ptr, p_hdr->hvalue.slen);
            PyTuple_SetItem(tuple, 1, str);
            PyList_SetItem(py_hdr_list, i, tuple);
            p_hdr = p_hdr->next;
	}
    }
    
    
}

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
 * py_pjsua_thread_register
 * !added @ 061206
 */
static PyObject *py_pjsua_thread_register(PyObject *pSelf, PyObject 
*pArgs)
{
	
    pj_status_t status;	
    const char *name;
    PyObject *py_desc;
    pj_thread_t *thread;
    void *thread_desc;
#if 0
    int size;
    int i;
    int *td;
#endif

    if (!PyArg_ParseTuple(pArgs, "sO", &name, &py_desc))
    {
         return NULL;
    }
#if 0
    size = PyList_Size(py_desc);
    td = (int *)malloc(size * sizeof(int));
    for (i = 0; i < size; i++) 
    {
	if (!PyArg_Parse(PyList_GetItem(py_desc,i),"i", td[i])) 
	{
	    return NULL;
	}
    }
    thread_desc = td;
#else
    thread_desc = malloc(sizeof(pj_thread_desc));
#endif
    status = pj_thread_register(name, thread_desc, &thread);

    if (status == PJ_SUCCESS)
	status = pj_thread_local_set(thread_id, (void*)1);
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_logging_config_default
 * !modified @ 051206
 */
static PyObject *py_pjsua_logging_config_default(PyObject *pSelf,
                                                    PyObject *pArgs)
{
    logging_config_Object *obj;	
    pjsua_logging_config cfg;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    
    pjsua_logging_config_default(&cfg);
    obj = (logging_config_Object *) logging_config_new
		(&logging_config_Type,NULL,NULL);
    obj->msg_logging = cfg.msg_logging;
    obj->level = cfg.level;
    obj->console_level = cfg.console_level;
    obj->decor = cfg.decor;
    
    return (PyObject *)obj;
}


/*
 * py_pjsua_config_default
 * !modified @ 051206
 */
static PyObject *py_pjsua_config_default(PyObject *pSelf, PyObject *pArgs)
{
    config_Object *obj;
    pjsua_config cfg;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    pjsua_config_default(&cfg);
    obj = (config_Object *) config_new(&config_Type, NULL, NULL);
    obj->max_calls = cfg.max_calls;
    obj->thread_cnt = cfg.thread_cnt;
    return (PyObject *)obj;
}


/*
 * py_pjsua_media_config_default
 * !modified @ 051206
 */
static PyObject * py_pjsua_media_config_default(PyObject *pSelf,
                                                PyObject *pArgs)
{
    media_config_Object *obj;
    pjsua_media_config cfg;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    pjsua_media_config_default(&cfg);
    obj = (media_config_Object *)PyType_GenericNew
		(&media_config_Type, NULL, NULL);
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
    return (PyObject *)obj;
}


/*
 * py_pjsua_msg_data_init
 * !modified @ 051206
 */
static PyObject *py_pjsua_msg_data_init(PyObject *pSelf, PyObject *pArgs)
{
    msg_data_Object *obj;
    pjsua_msg_data msg;
    
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    pjsua_msg_data_init(&msg);
    obj = (msg_data_Object *)msg_data_new(&msg_data_Type, NULL, NULL);
    Py_XDECREF(obj->content_type);
    obj->content_type = PyString_FromStringAndSize(
        msg.content_type.ptr, msg.content_type.slen
    );
    Py_XDECREF(obj->msg_body);
    obj->msg_body = PyString_FromStringAndSize(
        msg.msg_body.ptr, msg.msg_body.slen
    );

    translate_hdr_rev((pjsip_generic_string_hdr *)&msg.hdr_list,obj->hdr_list);
    
    return (PyObject *)obj;
}


/*
 * py_pjsua_reconfigure_logging
 */
static PyObject *py_pjsua_reconfigure_logging(PyObject *pSelf, PyObject *pArgs)
{
    PyObject * logObj;
    logging_config_Object *log;
    pjsua_logging_config cfg;
    pj_status_t status;

    if (!PyArg_ParseTuple(pArgs, "O", &logObj))
    {
        return NULL;
    }
    if (logObj != Py_None) 
    {
        log = (logging_config_Object *)logObj;
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
    } else {
        status = pjsua_reconfigure_logging(NULL);
    }
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
    
    if (status == PJ_SUCCESS) 
    {
	status = pj_thread_local_alloc(&thread_id);
	if (status == PJ_SUCCESS)
	    status = pj_thread_local_set(thread_id, (void*)1);
    }

    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_init
 */
static PyObject *py_pjsua_init(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject * ua_cfgObj;
    config_Object * ua_cfg;
    PyObject * log_cfgObj;
    logging_config_Object * log_cfg;
    PyObject * media_cfgObj;
    media_config_Object * media_cfg;
    pjsua_config cfg_ua;
    pjsua_config * p_cfg_ua;
    pjsua_logging_config cfg_log;
    pjsua_logging_config * p_cfg_log;
    pjsua_media_config cfg_media;
    pjsua_media_config * p_cfg_media;
    unsigned i;

    if (!PyArg_ParseTuple(pArgs, "OOO", &ua_cfgObj, &log_cfgObj,&media_cfgObj))
    {
        return NULL;
    }

    
    pjsua_config_default(&cfg_ua);
    pjsua_logging_config_default(&cfg_log);
    pjsua_media_config_default(&cfg_media);

    if (ua_cfgObj != Py_None) 
    {
	ua_cfg = (config_Object *)ua_cfgObj;
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

    	g_obj_callback = ua_cfg->cb;
    	Py_INCREF(g_obj_callback);

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

        cfg_ua.outbound_proxy_cnt = ua_cfg->outbound_proxy_cnt;
        cfg_ua.thread_cnt = ua_cfg->thread_cnt;
        cfg_ua.user_agent.ptr = PyString_AsString(ua_cfg->user_agent);
        cfg_ua.user_agent.slen = strlen(cfg_ua.user_agent.ptr);

        p_cfg_ua = &cfg_ua;
    } else {
        p_cfg_ua = NULL;
    }

    if (log_cfgObj != Py_None) 
    {
        log_cfg = (logging_config_Object *)log_cfgObj;
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
        p_cfg_log = &cfg_log;
    } else {
        p_cfg_log = NULL;
    }

    if (media_cfgObj != Py_None) 
    {
        media_cfg = (media_config_Object *)media_cfgObj;
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
	    p_cfg_media = &cfg_media;
    } else {
        p_cfg_media = NULL;
    }

    status = pjsua_init(p_cfg_ua, p_cfg_log, p_cfg_media);
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
    //printf("status %d\n",status);
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
    //printf("status %d\n",status);
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
    //printf("return %d\n",ret);
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
    //printf("status %d\n",status);
    return Py_BuildValue("i",status);
}


/*
 * function doc
 */

static char pjsua_thread_register_doc[] =
    "int py_pjsua.thread_register(string name, int[] desc)";
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
    "py_pjsua.Logging_Config py_pjsua.logging_config_default  ()  "
    "Use this function to initialize logging config.";

static char pjsua_config_default_doc[] =
    "py_pjsua.Config py_pjsua.config_default (). Use this function to "
    "initialize pjsua config. ";

static char pjsua_media_config_default_doc[] =
    "py_pjsua.Media_Config py_pjsua.media_config_default (). "
    "Use this function to initialize media config.";

static char pjsua_msg_data_init_doc[] =
    "py_pjsua.Msg_Data void py_pjsua.msg_data_init () "
    "Initialize message data ";
        

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
 * !modified @ 051206
 */
static PyObject *py_pjsua_stun_config_default(PyObject *pSelf, PyObject *pArgs)
{
    stun_config_Object *obj;
    pjsua_stun_config cfg;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
	
    pjsua_stun_config_default(&cfg);
    obj = (stun_config_Object *)stun_config_new(&stun_config_Type, NULL, NULL);
    obj->stun_port1 = cfg.stun_port1;
    obj->stun_port2 = cfg.stun_port2;
    Py_XDECREF(obj->stun_srv1);
    obj->stun_srv1 = 
        PyString_FromStringAndSize(cfg.stun_srv1.ptr, cfg.stun_srv1.slen);
    Py_XDECREF(obj->stun_srv2);
    obj->stun_srv2 = 
        PyString_FromStringAndSize(cfg.stun_srv2.ptr, cfg.stun_srv2.slen);
    return (PyObject *)obj;
}

/*
 * py_pjsua_transport_config_default
 * !modified @ 051206
 */
static PyObject *py_pjsua_transport_config_default
(PyObject *pSelf, PyObject *pArgs)
{
    transport_config_Object *obj;
    pjsua_transport_config cfg;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    pjsua_transport_config_default(&cfg);
    obj = (transport_config_Object *)transport_config_new
		(&transport_config_Type,NULL,NULL);
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
    return (PyObject *)obj;
}

/*
 * py_pjsua_normalize_stun_config
 */
static PyObject *py_pjsua_normalize_stun_config
(PyObject *pSelf, PyObject *pArgs)
{
    PyObject * tmpObj;
    stun_config_Object *obj;
    pjsua_stun_config *cfg;

    if (!PyArg_ParseTuple(pArgs, "O", &tmpObj))
    {
        return NULL;
    }
    if (tmpObj != Py_None)
    {
        obj = (stun_config_Object *) tmpObj;
        cfg = (pjsua_stun_config *)malloc(sizeof(pjsua_stun_config));
        cfg->stun_port1 = obj->stun_port1;
        cfg->stun_port2 = obj->stun_port2;
        cfg->stun_srv1.ptr = PyString_AsString(obj->stun_srv1);
        cfg->stun_srv1.slen = strlen(PyString_AsString(obj->stun_srv1));
        cfg->stun_srv2.ptr = PyString_AsString(obj->stun_srv2);
        cfg->stun_srv2.slen = strlen(PyString_AsString(obj->stun_srv2));
    } else {
        cfg = NULL;
    }
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
 * py_pjsua_transport_create
 * !modified @ 051206
 */
static PyObject *py_pjsua_transport_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    int type;
    
    PyObject * tmpObj;
    transport_config_Object *obj;
    pjsua_transport_config cfg;
    pjsua_transport_id id;
    if (!PyArg_ParseTuple(pArgs, "iO", &type, &tmpObj))
    {
        return NULL;
    }
    if (tmpObj != Py_None)
    {
        obj = (transport_config_Object *)tmpObj;
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
    } else {
        status = pjsua_transport_create(type, NULL, &id);
    }
    
    
    return Py_BuildValue("ii",status,id);
}

/*
 * py_pjsua_transport_register
 * !modified @ 051206
 */
static PyObject *py_pjsua_transport_register(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;	    
    PyObject * tmpObj;
    pjsip_transport_Object *obj;	
    pjsua_transport_id id;
    if (!PyArg_ParseTuple(pArgs, "O", &tmpObj))
    {
        return NULL;
    }
    if (tmpObj != Py_None)
    {
        obj = (pjsip_transport_Object *)tmpObj;
        status = pjsua_transport_register(obj->tp, &id);
    } else {
        status = pjsua_transport_register(NULL, &id);
    }
    
    return Py_BuildValue("ii",status, id);
}

/*
 * py_pjsua_enum_transports
 * !modified @ 261206
 */
static PyObject *py_pjsua_enum_transports(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_transport_id id[PJSIP_MAX_TRANSPORTS];
    unsigned c, i;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_transports(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {     
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
        {
            return NULL;
        }
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_transport_get_info
 * !modified @ 051206
 */
static PyObject *py_pjsua_transport_get_info(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    int id;
    transport_info_Object *obj;
    pjsua_transport_info info;
    

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    status = pjsua_transport_get_info(id, &info);	
    if (status == PJ_SUCCESS) {
        obj = (transport_info_Object *) transport_info_new
			(&transport_info_Type,NULL,NULL);
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
        return Py_BuildValue("O", obj);
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
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
    "py_pjsua.STUN_Config py_pjsua.stun_config_default () "
    "Call this function to initialize STUN config with default values.";
static char pjsua_transport_config_default_doc[] =
    "py_pjsua.Transport_Config py_pjsua.transport_config_default () "
    "Call this function to initialize UDP config with default values.";
static char pjsua_normalize_stun_config_doc[] =
    "void py_pjsua.normalize_stun_config (py_pjsua.STUN_Config cfg) "
    "Normalize STUN config. ";
static char pjsua_transport_create_doc[] =
    "int, int py_pjsua.transport_create (int type, "
    "py_pjsua.Transport_Config cfg) "
    "Create SIP transport.";
static char pjsua_transport_register_doc[] =
    "int, int py_pjsua.transport_register "
    "(py_pjsua.PJSIP_Transport tp) "
    "Register transport that has been created by application.";
static char pjsua_enum_transports_doc[] =
    "int[] py_pjsua.enum_transports () "
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
    /*pj_str_t proxy[8];*/
    PyListObject * proxy;
    unsigned reg_timeout;
    unsigned cred_count;
    /*pjsip_cred_info cred_info[8];*/
    PyListObject * cred_info;
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
    Py_XDECREF(self->proxy);
    Py_XDECREF(self->cred_info);
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
	self->proxy = (PyListObject *)PyList_New(8);
	if (self->proxy == NULL)
	{
	    Py_DECREF(self);
	    return NULL;
	}
	self->cred_info = (PyListObject *)PyList_New(8);
	if (self->cred_info == NULL)
	{
	    Py_DECREF(self);
	    return NULL;
	}
    }

    return (PyObject *)self;
}



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
        "proxy", T_OBJECT_EX,
        offsetof(acc_config_Object, proxy), 0,
        "Optional URI of the proxies to be visited for all outgoing requests "
	"that are using this account (REGISTER, INVITE, etc). Application need "
	"to specify these proxies if the service provider requires "
	"that requests destined towards its network should go through certain "
	"proxies first (for example, border controllers)."
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
    {
        "cred_info", T_OBJECT_EX,
        offsetof(acc_config_Object, cred_info), 0,
        "Array of credentials. If registration is desired, normally there "
	"should be at least one credential specified, to successfully "
	"authenticate against the service provider. More credentials can "
	"be specified, for example when the requests are expected to be "
	"challenged by the proxies in the route set."
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
    0/*acc_config_methods*/,                              /* tp_methods */
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
 * py_pjsua_acc_config_default
 * !modified @ 051206
 */
static PyObject *py_pjsua_acc_config_default
(PyObject *pSelf, PyObject *pArgs)
{
    acc_config_Object *obj;
    pjsua_acc_config cfg;
    int i;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    pjsua_acc_config_default(&cfg);
    obj = (acc_config_Object *)acc_config_new(&acc_config_Type, NULL, NULL);
    obj->cred_count = cfg.cred_count;
    for (i = 0; i < PJSUA_MAX_ACC; i++) 
    {
        /*obj->cred_info[i] = cfg.cred_info[i];*/
	int ret;
	pjsip_cred_info_Object * ci = 
	    (pjsip_cred_info_Object *)pjsip_cred_info_new
	    (&pjsip_cred_info_Type,NULL,NULL);
	ci->data = PyString_FromStringAndSize(cfg.cred_info[i].data.ptr, 
	    cfg.cred_info[i].data.slen);
	ci->realm = PyString_FromStringAndSize(cfg.cred_info[i].realm.ptr, 
	    cfg.cred_info[i].realm.slen);
	ci->scheme = PyString_FromStringAndSize(cfg.cred_info[i].scheme.ptr, 
	    cfg.cred_info[i].scheme.slen);
	ci->username = PyString_FromStringAndSize(cfg.cred_info[i].username.ptr, 
	    cfg.cred_info[i].username.slen);
	ci->data_type = cfg.cred_info[i].data_type;
	ret = PyList_SetItem((PyObject *)obj->cred_info,i,(PyObject *)ci);
	if (ret == -1) {
	    return NULL;
	}
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
    for (i = 0; i < PJSUA_MAX_ACC; i++) 
    {
	PyObject * str;
	int ret;
        /*obj->proxy[i] = cfg.proxy[i];*/
	str = PyString_FromStringAndSize(cfg.proxy[i].ptr, cfg.proxy[i].slen);
	ret = PyList_SetItem((PyObject *)obj->proxy,i,str);
	if (ret == -1) {
	    return NULL;
	}
    }
    obj->publish_enabled = cfg.publish_enabled;
    obj->reg_timeout = cfg.reg_timeout;
	
    return (PyObject *)obj;
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
 * !modified @ 051206
 */
static PyObject *py_pjsua_acc_add
(PyObject *pSelf, PyObject *pArgs)
{    
    int is_default;
    PyObject * acObj;
    acc_config_Object * ac;
    pjsua_acc_config cfg;
    
    int p_acc_id;
    int status;
    int i;

    if (!PyArg_ParseTuple(pArgs, "Oi", &acObj, &is_default))
    {
        return NULL;
    }

    pjsua_acc_config_default(&cfg);
    if (acObj != Py_None)
    {
        ac = (acc_config_Object *)acObj;
        cfg.cred_count = ac->cred_count;
        for (i = 0; i < PJSUA_MAX_ACC; i++) 
	{
            /*cfg.cred_info[i] = ac->cred_info[i];*/
            pjsip_cred_info_Object * ci = (pjsip_cred_info_Object *)
				PyList_GetItem((PyObject *)ac->cred_info,i);
            cfg.cred_info[i].data.ptr = PyString_AsString(ci->data);
            cfg.cred_info[i].data.slen = strlen(PyString_AsString(ci->data));
            cfg.cred_info[i].realm.ptr = PyString_AsString(ci->realm);
            cfg.cred_info[i].realm.slen = strlen(PyString_AsString(ci->realm));
            cfg.cred_info[i].scheme.ptr = PyString_AsString(ci->scheme);
            cfg.cred_info[i].scheme.slen = strlen
				(PyString_AsString(ci->scheme));
            cfg.cred_info[i].username.ptr = PyString_AsString(ci->username);
            cfg.cred_info[i].username.slen = strlen
				(PyString_AsString(ci->username));
            cfg.cred_info[i].data_type = ci->data_type;
	}
        cfg.force_contact.ptr = PyString_AsString(ac->force_contact);
        cfg.force_contact.slen = strlen(PyString_AsString(ac->force_contact));
        cfg.id.ptr = PyString_AsString(ac->id);
        cfg.id.slen = strlen(PyString_AsString(ac->id));
        cfg.priority = ac->priority;
        for (i = 0; i < PJSUA_MAX_ACC; i++) 
	{
            /*cfg.proxy[i] = ac->proxy[i];*/
            cfg.proxy[i].ptr = PyString_AsString
				(PyList_GetItem((PyObject *)ac->proxy,i));
	}
        cfg.proxy_cnt = ac->proxy_cnt;
        cfg.publish_enabled = ac->publish_enabled;
        cfg.reg_timeout = ac->reg_timeout;
        cfg.reg_uri.ptr = PyString_AsString(ac->reg_uri);
        cfg.reg_uri.slen = strlen(PyString_AsString(ac->reg_uri));
    
        status = pjsua_acc_add(&cfg, is_default, &p_acc_id);
    } else {
        status = pjsua_acc_add(NULL, is_default, &p_acc_id);
    }
    
    return Py_BuildValue("ii", status, p_acc_id);
}

/*
 * py_pjsua_acc_add_local
 * !modified @ 051206
 */
static PyObject *py_pjsua_acc_add_local
(PyObject *pSelf, PyObject *pArgs)
{    
    int is_default;
    int tid;
    
    int p_acc_id;
    int status;
	

    if (!PyArg_ParseTuple(pArgs, "ii", &tid, &is_default))
    {
        return NULL;
    }
	
    
    status = pjsua_acc_add_local(tid, is_default, &p_acc_id);
    
    return Py_BuildValue("ii", status, p_acc_id);
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
    PyObject * acObj;
    acc_config_Object * ac;
    pjsua_acc_config cfg;	
    int acc_id;
    int status;
    int i;

    if (!PyArg_ParseTuple(pArgs, "iO", &acc_id, &acObj))
    {
        return NULL;
    }
    if (acObj != Py_None)
    {
        ac = (acc_config_Object *)acObj;
        cfg.cred_count = ac->cred_count;
        for (i = 0; i < PJSUA_MAX_ACC; i++) 
	{
            /*cfg.cred_info[i] = ac->cred_info[i];*/
            pjsip_cred_info_Object * ci = (pjsip_cred_info_Object *)
				PyList_GetItem((PyObject *)ac->cred_info,i);
            cfg.cred_info[i].data.ptr = PyString_AsString(ci->data);
            cfg.cred_info[i].data.slen = strlen(PyString_AsString(ci->data));
            cfg.cred_info[i].realm.ptr = PyString_AsString(ci->realm);
            cfg.cred_info[i].realm.slen = strlen(PyString_AsString(ci->realm));
            cfg.cred_info[i].scheme.ptr = PyString_AsString(ci->scheme);
            cfg.cred_info[i].scheme.slen = strlen
				(PyString_AsString(ci->scheme));
            cfg.cred_info[i].username.ptr = PyString_AsString(ci->username);
            cfg.cred_info[i].username.slen = strlen
				(PyString_AsString(ci->username));
	}
        cfg.force_contact.ptr = PyString_AsString(ac->force_contact);
        cfg.force_contact.slen = strlen(PyString_AsString(ac->force_contact));
        cfg.id.ptr = PyString_AsString(ac->id);
        cfg.id.slen = strlen(PyString_AsString(ac->id));
        cfg.priority = ac->priority;
        for (i = 0; i < PJSUA_MAX_ACC; i++) 
	{
            /*cfg.proxy[i] = ac->proxy[i];*/
             cfg.proxy[i].ptr = PyString_AsString
				(PyList_GetItem((PyObject *)ac->proxy,i));
	}
        cfg.proxy_cnt = ac->proxy_cnt;
        cfg.publish_enabled = ac->publish_enabled;
        cfg.reg_timeout = ac->reg_timeout;
        cfg.reg_uri.ptr = PyString_AsString(ac->reg_uri);
        cfg.reg_uri.slen = strlen(PyString_AsString(ac->reg_uri));	
        status = pjsua_acc_modify(acc_id, &cfg);
    } else {
        status = pjsua_acc_modify(acc_id, NULL);
    }
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
 * py_pjsua_acc_get_info
 * !modified @ 051206
 */
static PyObject *py_pjsua_acc_get_info
(PyObject *pSelf, PyObject *pArgs)
{    	
    int acc_id;
    acc_info_Object * obj;
    pjsua_acc_info info;
    int status;	
    int i;

    if (!PyArg_ParseTuple(pArgs, "i", &acc_id))
    {
        return NULL;
    }
	
    
    status = pjsua_acc_get_info(acc_id, &info);
    if (status == PJ_SUCCESS) 
    {
	obj = (acc_info_Object *)acc_info_new(&acc_info_Type,NULL, NULL);
        obj->acc_uri =
            PyString_FromStringAndSize(info.acc_uri.ptr, 
            info.acc_uri.slen);
        for (i = 0; i < PJ_ERR_MSG_SIZE; i++) 
	{
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
        return Py_BuildValue("O", obj);
    } else {
	Py_INCREF(Py_None);
	return Py_None;
    }
}

/*
 * py_pjsua_enum_accs
 * !modified @ 241206
 */
static PyObject *py_pjsua_enum_accs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_acc_id id[PJSUA_MAX_ACC];
    unsigned c, i;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    c = PJ_ARRAY_SIZE(id);
    
    status = pjsua_enum_accs(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
	{
            return NULL;
        }
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_acc_enum_info
 * !modified @ 241206
 */
static PyObject *py_pjsua_acc_enum_info(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_acc_info info[PJSUA_MAX_ACC];
    unsigned c, i;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(info);
    status = pjsua_acc_enum_info(info, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret;
        int j;
        acc_info_Object *obj;
        obj = (acc_info_Object *)acc_info_new(&acc_info_Type,NULL,NULL);
        obj->acc_uri = PyString_FromStringAndSize
	    (info[i].acc_uri.ptr, info[i].acc_uri.slen);
        for(j = 0; j < PJ_ERR_MSG_SIZE; j++) 
	{
            obj->buf_[j] = info[i].buf_[j];
	}
        obj->expires = info[i].expires;
        obj->has_registration = info[i].has_registration;
        obj->id = info[i].id;
        obj->is_default = info[i].is_default;
        obj->online_status = info[i].online_status;
        obj->status = info[i].status;
        obj->status_text = PyString_FromStringAndSize(info[i].status_text.ptr,
	    info[i].status_text.slen);
        ret = PyList_SetItem(list, i, (PyObject *)obj);
        if (ret == -1) {
            return NULL;
        }
    }
    
    return Py_BuildValue("O",list);
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
    PyObject * tmpObj;
    pjsip_rx_data_Object * obj;
    pjsip_rx_data * rdata;

    if (!PyArg_ParseTuple(pArgs, "O", &tmpObj))
    {
        return NULL;
    }
    if (tmpObj != Py_None)
    {
        obj = (pjsip_rx_data_Object *)tmpObj;
        rdata = obj->rdata;
        acc_id = pjsua_acc_find_for_incoming(rdata);
    } else {
        acc_id = pjsua_acc_find_for_incoming(NULL);
    }
    return Py_BuildValue("i", acc_id);
}

/*
 * py_pjsua_acc_create_uac_contact
 * !modified @ 061206
 */
static PyObject *py_pjsua_acc_create_uac_contact
(PyObject *pSelf, PyObject *pArgs)
{    	
    int status;
    int acc_id;
    PyObject * pObj;
    pj_pool_Object * p;
    pj_pool_t * pool;
    PyObject * strc;
    pj_str_t contact;
    PyObject * stru;
    pj_str_t uri;

    if (!PyArg_ParseTuple(pArgs, "OiO", &pObj, &acc_id, &stru))
    {
        return NULL;
    }
    if (pObj != Py_None)
    {
        p = (pj_pool_Object *)pObj;
        pool = p->pool;    
        uri.ptr = PyString_AsString(stru);
        uri.slen = strlen(PyString_AsString(stru));
        status = pjsua_acc_create_uac_contact(pool, &contact, acc_id, &uri);
    } else {
        status = pjsua_acc_create_uac_contact(NULL, &contact, acc_id, &uri);
    }
    strc = PyString_FromStringAndSize(contact.ptr, contact.slen);
	
    return Py_BuildValue("O", strc);
}

/*
 * py_pjsua_acc_create_uas_contact
 * !modified @ 061206
 */
static PyObject *py_pjsua_acc_create_uas_contact
(PyObject *pSelf, PyObject *pArgs)
{    	
    int status;
    int acc_id;	
    PyObject * pObj;
    pj_pool_Object * p;
    pj_pool_t * pool;
    PyObject * strc;
    pj_str_t contact;
    PyObject * rObj;
    pjsip_rx_data_Object * objr;
    pjsip_rx_data * rdata;

    if (!PyArg_ParseTuple(pArgs, "OiO", &pObj, &acc_id, &rObj))
    {
        return NULL;
    }
    if (pObj != Py_None)
    {
        p = (pj_pool_Object *)pObj;
        pool = p->pool;
    } else {
		pool = NULL;
    }
    if (rObj != Py_None)
    {
        objr = (pjsip_rx_data_Object *)rObj;
        rdata = objr->rdata;
    } else {
        rdata = NULL;
    }
    status = pjsua_acc_create_uas_contact(pool, &contact, acc_id, rdata);
    strc = PyString_FromStringAndSize(contact.ptr, contact.slen);
	
    return Py_BuildValue("O", strc);
}

static char pjsua_acc_config_default_doc[] =
    "py_pjsua.Acc_Config py_pjsua.acc_config_default () "
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
    "int, int py_pjsua.acc_add (py_pjsua.Acc_Config cfg, "
    "int is_default) "
    "Add a new account to pjsua. PJSUA must have been initialized "
    "(with pjsua_init()) before calling this function.";
static char pjsua_acc_add_local_doc[] =
    "int,int py_pjsua.acc_add_local (int tid, "
    "int is_default) "
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
    "py_pjsua.Acc_Info py_pjsua.acc_get_info (int acc_id) "
    "Get account information.";
static char pjsua_enum_accs_doc[] =
    "int[] py_pjsua.enum_accs () "
    "Enum accounts all account ids.";
static char pjsua_acc_enum_info_doc[] =
    "py_pjsua.Acc_Info[] py_pjsua.acc_enum_info () "
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
    "string py_pjsua.acc_create_uac_contact (pj_pool_Object pool, "
    "int acc_id, string uri) "
    "Create a suitable URI to be put as Contact based on the specified "
    "target URI for the specified account.";
static char pjsua_acc_create_uas_contact_doc[] =
    "string py_pjsua.acc_create_uas_contact (pj_pool_Object pool, "
    "int acc_id, pjsip_rx_data_Object rdata) "
    "Create a suitable URI to be put as Contact based on the information "
    "in the incoming request.";

/* END OF LIB ACCOUNT */

/* LIB BUDDY */



/*
 * buddy_config_Object
 * Buddy Config
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    
    PyObject * uri;
    int subscribe;
} buddy_config_Object;


/*
 * buddy_config_dealloc
 * deletes a buddy_config from memory
 */
static void buddy_config_dealloc(buddy_config_Object* self)
{
    Py_XDECREF(self->uri);     
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * buddy_config_new
 * constructor for buddy_config object
 */
static PyObject * buddy_config_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    buddy_config_Object *self;

    self = (buddy_config_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->uri = PyString_FromString("");
        if (self->uri == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
    }
    return (PyObject *)self;
}

/*
 * buddy_config_members
 */
static PyMemberDef buddy_config_members[] =
{
    
    {
        "uri", T_OBJECT_EX,
        offsetof(buddy_config_Object, uri), 0,
        "TBuddy URL or name address."        
    },
    
    {
        "subscribe", T_INT, 
        offsetof(buddy_config_Object, subscribe), 0,
        "Specify whether presence subscription should start immediately. "
    },
    
    {NULL}  /* Sentinel */
};




/*
 * buddy_config_Type
 */
static PyTypeObject buddy_config_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Buddy_Config",      /*tp_name*/
    sizeof(buddy_config_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)buddy_config_dealloc,/*tp_dealloc*/
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
    "Buddy Config objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    buddy_config_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    buddy_config_new,             /* tp_new */

};

/*
 * buddy_info_Object
 * Buddy Info
 * !modified @ 071206
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    int id;
    PyObject * uri;
    PyObject * contact;
    int status;
    PyObject * status_text;
    int monitor_pres;
    char buf_[256];
} buddy_info_Object;


/*
 * buddy_info_dealloc
 * deletes a buddy_info from memory
 * !modified @ 071206
 */
static void buddy_info_dealloc(buddy_info_Object* self)
{
    Py_XDECREF(self->uri);
    Py_XDECREF(self->contact);
    Py_XDECREF(self->status_text);
    
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * buddy_info_new
 * constructor for buddy_info object
 * !modified @ 071206
 */
static PyObject * buddy_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    buddy_info_Object *self;

    self = (buddy_info_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->uri = PyString_FromString("");
        if (self->uri == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	self->contact = PyString_FromString("");
        if (self->contact == NULL)
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

/*
 * buddy_info_members
 * !modified @ 071206
 */
static PyMemberDef buddy_info_members[] =
{
    {
        "id", T_INT, 
        offsetof(buddy_info_Object, id), 0,
        "The buddy ID."
    },
    {
        "uri", T_OBJECT_EX,
        offsetof(buddy_info_Object, uri), 0,
        "The full URI of the buddy, as specified in the configuration. "        
    },
    {
        "contact", T_OBJECT_EX,
        offsetof(buddy_info_Object, contact), 0,
        "Buddy's Contact, only available when presence subscription "
        "has been established to the buddy."        
    },
    {
        "status", T_INT, 
        offsetof(buddy_info_Object, status), 0,
        "Buddy's online status. "
    },
    {
        "status_text", T_OBJECT_EX,
        offsetof(buddy_info_Object, status_text), 0,
        "Text to describe buddy's online status."        
    },
    {
        "monitor_pres", T_INT, 
        offsetof(buddy_info_Object, monitor_pres), 0,
        "Flag to indicate that we should monitor the presence information "
        "for this buddy (normally yes, unless explicitly disabled). "
    },
    
    
    {NULL}  /* Sentinel */
};




/*
 * buddy_info_Type
 */
static PyTypeObject buddy_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Buddy_Info",      /*tp_name*/
    sizeof(buddy_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)buddy_info_dealloc,/*tp_dealloc*/
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
    "Buddy Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    buddy_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    buddy_info_new,             /* tp_new */

};

/*
 * py_pjsua_buddy_config_default
 */
static PyObject *py_pjsua_buddy_config_default
(PyObject *pSelf, PyObject *pArgs)
{    
    buddy_config_Object *obj;	
    pjsua_buddy_config cfg;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    
    pjsua_buddy_config_default(&cfg);
    obj = (buddy_config_Object *) buddy_config_new
		(&buddy_config_Type,NULL,NULL);
    obj->uri = PyString_FromStringAndSize(
        cfg.uri.ptr, cfg.uri.slen
    );
    obj->subscribe = cfg.subscribe;
    
    return (PyObject *)obj;
}

/*
 * py_pjsua_get_buddy_count
 */
static PyObject *py_pjsua_get_buddy_count
(PyObject *pSelf, PyObject *pArgs)
{    
    int ret;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    ret = pjsua_get_buddy_count();
	
    return Py_BuildValue("i", ret);
}

/*
 * py_pjsua_buddy_is_valid
 */
static PyObject *py_pjsua_buddy_is_valid
(PyObject *pSelf, PyObject *pArgs)
{    
    int id;
    int is_valid;

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }
    is_valid = pjsua_buddy_is_valid(id);
	
    return Py_BuildValue("i", is_valid);
}

/*
 * py_pjsua_enum_buddies
 * !modified @ 241206
 */
static PyObject *py_pjsua_enum_buddies(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_buddy_id id[PJSUA_MAX_BUDDIES];
    unsigned c, i;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_buddies(id, &c);
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
	{
            return NULL;
	}
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_buddy_get_info
 * !modified @ 071206
 */
static PyObject *py_pjsua_buddy_get_info
(PyObject *pSelf, PyObject *pArgs)
{    	
    int buddy_id;
    buddy_info_Object * obj;
    pjsua_buddy_info info;
    int status;	
    int i;

    if (!PyArg_ParseTuple(pArgs, "i", &buddy_id))
    {
        return NULL;
    }
	
    
    status = pjsua_buddy_get_info(buddy_id, &info);
    if (status == PJ_SUCCESS) 
    {
	obj = (buddy_info_Object *)buddy_info_new(&buddy_info_Type,NULL,NULL);
        obj->id = info.id;
        Py_XDECREF(obj->uri);
        obj->uri =
            PyString_FromStringAndSize(info.uri.ptr, 
            info.uri.slen);
        Py_XDECREF(obj->contact);
        obj->contact =
            PyString_FromStringAndSize(info.contact.ptr, 
            info.contact.slen);
        obj->status = info.status;
        Py_XDECREF(obj->status_text);
        obj->status_text =
            PyString_FromStringAndSize(info.status_text.ptr, 
            info.status_text.slen);
        obj->monitor_pres = info.monitor_pres;
        for (i = 0; i < 256; i++) 
	{
	    
            obj->buf_[i] = info.buf_[i];
	}
	
        return Py_BuildValue("O", obj);
    } else {
	Py_INCREF(Py_None);
	return Py_None;
    }
}

/*
 * py_pjsua_buddy_add
 * !modified @ 061206
 */
static PyObject *py_pjsua_buddy_add
(PyObject *pSelf, PyObject *pArgs)
{   
    PyObject * bcObj;
    buddy_config_Object * bc;
	
    pjsua_buddy_config cfg;
    
    int p_buddy_id;
    int status;

    if (!PyArg_ParseTuple(pArgs, "O", &bcObj))
    {
        return NULL;
    }
    if (bcObj != Py_None)
    {
        bc = (buddy_config_Object *)bcObj;

        cfg.subscribe = bc->subscribe;
        cfg.uri.ptr = PyString_AsString(bc->uri);
        cfg.uri.slen = strlen(PyString_AsString(bc->uri));    
    
        status = pjsua_buddy_add(&cfg, &p_buddy_id);
    } else {
        status = pjsua_buddy_add(NULL, &p_buddy_id);
    }
    return Py_BuildValue("ii", status, p_buddy_id);
}

/*
 * py_pjsua_buddy_del
 */
static PyObject *py_pjsua_buddy_del
(PyObject *pSelf, PyObject *pArgs)
{    
    int buddy_id;
    int status;

    if (!PyArg_ParseTuple(pArgs, "i", &buddy_id))
    {
        return NULL;
    }
	
	
    status = pjsua_buddy_del(buddy_id);	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_buddy_subscribe_pres
 */
static PyObject *py_pjsua_buddy_subscribe_pres
(PyObject *pSelf, PyObject *pArgs)
{    
    int buddy_id;
    int status;
    int subscribe;

    if (!PyArg_ParseTuple(pArgs, "ii", &buddy_id, &subscribe))
    {
        return NULL;
    }
	
	
    status = pjsua_buddy_subscribe_pres(buddy_id, subscribe);	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_pres_dump
 */
static PyObject *py_pjsua_pres_dump
(PyObject *pSelf, PyObject *pArgs)
{    
    int verbose;

    if (!PyArg_ParseTuple(pArgs, "i", &verbose))
    {
        return NULL;
    }
	
	
    pjsua_pres_dump(verbose);	
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_im_send
 * !modified @ 071206
 */
static PyObject *py_pjsua_im_send
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int acc_id;
    pj_str_t * mime_type;
    pj_str_t to, content;
    PyObject * st;
    PyObject * smt;
    PyObject * sc;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;
    
    int user_data;
    pj_pool_t *pool;

   
    if (!PyArg_ParseTuple(pArgs, "iOOOOi", &acc_id, 
		&st, &smt, &sc, &omdObj, &user_data))
    {
        return NULL;
    }
    if (smt != NULL)
    {
        mime_type = (pj_str_t *)malloc(sizeof(pj_str_t));
        mime_type->ptr = PyString_AsString(smt);
        mime_type->slen = strlen(PyString_AsString(smt));
    } else {
        mime_type = NULL;
    }
    to.ptr = PyString_AsString(st);
    to.slen = strlen(PyString_AsString(st));
    
    content.ptr = PyString_AsString(sc);
    content.slen = strlen(PyString_AsString(sc));
    if (omdObj != Py_None)
    {
		
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);

        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_im_send(acc_id, &to, mime_type, 
			&content, &msg_data, (void *)user_data);	
        pj_pool_release(pool);
    } else {
		
        status = pjsua_im_send(acc_id, &to, mime_type, 
			&content, NULL, NULL);	
    }
    if (mime_type != NULL)
    {
        free(mime_type);
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_im_typing
 */
static PyObject *py_pjsua_im_typing
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int acc_id;
    pj_str_t to;
    PyObject * st;
    int is_typing;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;
    pj_pool_t * pool;

    if (!PyArg_ParseTuple(pArgs, "iOiO", &acc_id, &st, &is_typing, &omdObj))
    {
        return NULL;
    }
	
    to.ptr = PyString_AsString(st);
    to.slen = strlen(PyString_AsString(st));    
    if (omdObj != Py_None)
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);

        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_im_typing(acc_id, &to, is_typing, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_im_typing(acc_id, &to, is_typing, NULL);
    }
    return Py_BuildValue("i",status);
}

static char pjsua_buddy_config_default_doc[] =
    "py_pjsua.Buddy_Config py_pjsua.buddy_config_default () "
    "Set default values to the buddy config.";
static char pjsua_get_buddy_count_doc[] =
    "int py_pjsua.get_buddy_count () "
    "Get total number of buddies.";
static char pjsua_buddy_is_valid_doc[] =
    "int py_pjsua.buddy_is_valid (int buddy_id) "
    "Check if buddy ID is valid.";
static char pjsua_enum_buddies_doc[] =
    "int[] py_pjsua.enum_buddies () "
    "Enum buddy IDs.";
static char pjsua_buddy_get_info_doc[] =
    "py_pjsua.Buddy_Info py_pjsua.buddy_get_info (int buddy_id) "
    "Get detailed buddy info.";
static char pjsua_buddy_add_doc[] =
    "int,int py_pjsua.buddy_add (py_pjsua.Buddy_Config cfg) "
    "Add new buddy.";
static char pjsua_buddy_del_doc[] =
    "int py_pjsua.buddy_del (int buddy_id) "
    "Delete buddy.";
static char pjsua_buddy_subscribe_pres_doc[] =
    "int py_pjsua.buddy_subscribe_pres (int buddy_id, int subscribe) "
    "Enable/disable buddy's presence monitoring.";
static char pjsua_pres_dump_doc[] =
    "void py_pjsua.pres_dump (int verbose) "
    "Dump presence subscriptions to log file.";
static char pjsua_im_send_doc[] =
    "int py_pjsua.im_send (int acc_id, string to, string mime_type, "
    "string content, py_pjsua.Msg_Data msg_data, int user_data) "
    "Send instant messaging outside dialog, using the specified account "
    "for route set and authentication.";
static char pjsua_im_typing_doc[] =
    "int py_pjsua.im_typing (int acc_id, string to, int is_typing, "
    "py_pjsua.Msg_Data msg_data) "
    "Send typing indication outside dialog.";

/* END OF LIB BUDDY */

/* LIB MEDIA */



/*
 * codec_info_Object
 * Codec Info
 * !modified @ 071206
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    PyObject * codec_id;
    pj_uint8_t priority;    
    char buf_[32];
} codec_info_Object;


/*
 * codec_info_dealloc
 * deletes a codec_info from memory
 * !modified @ 071206
 */
static void codec_info_dealloc(codec_info_Object* self)
{
    Py_XDECREF(self->codec_id);    
    
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * codec_info_new
 * constructor for codec_info object
 * !modified @ 071206
 */
static PyObject * codec_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    codec_info_Object *self;

    self = (codec_info_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->codec_id = PyString_FromString("");
        if (self->codec_id == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	

    }
    return (PyObject *)self;
}

/*
 * codec_info_members
 * !modified @ 071206
 */
static PyMemberDef codec_info_members[] =
{    
    {
        "codec_id", T_OBJECT_EX,
        offsetof(codec_info_Object, codec_id), 0,
        "Codec unique identification."        
    },
    
    {
        "priority", T_INT, 
        offsetof(codec_info_Object, priority), 0,
        "Codec priority (integer 0-255)."
    },
    
    
    
    {NULL}  /* Sentinel */
};




/*
 * codec_info_Type
 */
static PyTypeObject codec_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Codec_Info",      /*tp_name*/
    sizeof(codec_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)codec_info_dealloc,/*tp_dealloc*/
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
    "Codec Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    codec_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    codec_info_new,             /* tp_new */

};

/*
 * conf_port_info_Object
 * Conf Port Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    int  slot_id;
    PyObject *  name;
    unsigned  clock_rate;
    unsigned  channel_count;
    unsigned  samples_per_frame;
    unsigned  bits_per_sample;
    unsigned  listener_cnt;
    PyListObject * listeners;

} conf_port_info_Object;


/*
 * conf_port_info_dealloc
 * deletes a conf_port_info from memory
 */
static void conf_port_info_dealloc(conf_port_info_Object* self)
{
    Py_XDECREF(self->name);    
    Py_XDECREF(self->listeners);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * conf_port_info_new
 * constructor for conf_port_info object
 */
static PyObject * conf_port_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    conf_port_info_Object *self;

    self = (conf_port_info_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->name = PyString_FromString("");
        if (self->name == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	
	self->listeners = (PyListObject *)PyList_New(PJSUA_MAX_CONF_PORTS);
        if (self->listeners == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }
    return (PyObject *)self;
}

/*
 * conf_port_info_members
 */
static PyMemberDef conf_port_info_members[] =
{   
    {
        "slot_id", T_INT, 
        offsetof(conf_port_info_Object, slot_id), 0,
        "Conference port number."
    },
    {
        "name", T_OBJECT_EX,
        offsetof(conf_port_info_Object, name), 0,
        "Port name"        
    },
    {
        "clock_rate", T_INT, 
        offsetof(conf_port_info_Object, clock_rate), 0,
        "Clock rate"
    },
    {
        "channel_count", T_INT, 
        offsetof(conf_port_info_Object, channel_count), 0,
        "Number of channels."
    },
    {
        "samples_per_frame", T_INT, 
        offsetof(conf_port_info_Object, samples_per_frame), 0,
        "Samples per frame "
    },
    {
        "bits_per_sample", T_INT, 
        offsetof(conf_port_info_Object, bits_per_sample), 0,
        "Bits per sample"
    },
    /*{
        "listener_cnt", T_INT, 
        offsetof(conf_port_info_Object, listener_cnt), 0,
        "Number of listeners in the array."
    },*/
    {
        "listeners", T_OBJECT_EX,
        offsetof(conf_port_info_Object, listeners), 0,
        "Array of listeners (in other words, ports where this port "
	"is transmitting to"
    },
    
    {NULL}  /* Sentinel */
};




/*
 * conf_port_info_Type
 */
static PyTypeObject conf_port_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Conf_Port_Info",      /*tp_name*/
    sizeof(conf_port_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)conf_port_info_dealloc,/*tp_dealloc*/
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
    "Conf Port Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    conf_port_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    conf_port_info_new,             /* tp_new */

};

/*
 * pjmedia_port_Object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjmedia_port * port;
} pjmedia_port_Object;


/*
 * pjmedia_port_Type
 */
static PyTypeObject pjmedia_port_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.PJMedia_Port",        /*tp_name*/
    sizeof(pjmedia_port_Object),    /*tp_basicsize*/
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
    "pjmedia_port objects",       /* tp_doc */

};

/*
 * pjmedia_snd_dev_info_Object
 * PJMedia Snd Dev Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    
    unsigned  input_count;
    unsigned  output_count;
    unsigned  default_samples_per_sec;    
    PyListObject * name;

} pjmedia_snd_dev_info_Object;


/*
 * pjmedia_snd_dev_info_dealloc
 * deletes a pjmedia_snd_dev_info from memory
 */
static void pjmedia_snd_dev_info_dealloc(pjmedia_snd_dev_info_Object* self)
{
    Py_XDECREF(self->name);        
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * pjmedia_snd_dev_info_new
 * constructor for pjmedia_snd_dev_info object
 */
static PyObject * pjmedia_snd_dev_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    pjmedia_snd_dev_info_Object *self;

    self = (pjmedia_snd_dev_info_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->name = (PyListObject *)PyList_New(SND_DEV_NUM);
        if (self->name == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	
    }
    return (PyObject *)self;
}

/*
 * pjmedia_snd_dev_info_members
 */
static PyMemberDef pjmedia_snd_dev_info_members[] =
{   
    
    {
        "name", T_OBJECT_EX,
        offsetof(pjmedia_snd_dev_info_Object, name), 0,
        "Device name"        
    },
    {
        "input_count", T_INT, 
        offsetof(pjmedia_snd_dev_info_Object, input_count), 0,
        "Max number of input channels"
    },
    {
        "output_count", T_INT, 
        offsetof(pjmedia_snd_dev_info_Object, output_count), 0,
        "Max number of output channels"
    },
    {
        "default_samples_per_sec", T_INT, 
        offsetof(pjmedia_snd_dev_info_Object, default_samples_per_sec), 0,
        "Default sampling rate."
    },
    
    
    {NULL}  /* Sentinel */
};




/*
 * pjmedia_snd_dev_info_Type
 */
static PyTypeObject pjmedia_snd_dev_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJMedia_Snd_Dev_Info",      /*tp_name*/
    sizeof(pjmedia_snd_dev_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)pjmedia_snd_dev_info_dealloc,/*tp_dealloc*/
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
    "PJMedia Snd Dev Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_snd_dev_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    pjmedia_snd_dev_info_new,             /* tp_new */

};

/*
 * pjmedia_codec_param_info_Object
 * PJMedia Codec Param Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    unsigned  clock_rate;
    unsigned  channel_cnt;
    pj_uint32_t avg_bps;
    pj_uint16_t frm_ptime;
    pj_uint8_t  pcm_bits_per_sample;
    pj_uint8_t  pt;	

} pjmedia_codec_param_info_Object;



/*
 * pjmedia_codec_param_info_members
 */
static PyMemberDef pjmedia_codec_param_info_members[] =
{   
    
    {
        "clock_rate", T_INT, 
        offsetof(pjmedia_codec_param_info_Object, clock_rate), 0,
        "Sampling rate in Hz"
    },
    {
        "channel_cnt", T_INT, 
        offsetof(pjmedia_codec_param_info_Object, channel_cnt), 0,
        "Channel count"
    },
    {
        "avg_bps", T_INT, 
        offsetof(pjmedia_codec_param_info_Object, avg_bps), 0,
        "Average bandwidth in bits/sec"
    },
    {
        "frm_ptime", T_INT, 
        offsetof(pjmedia_codec_param_info_Object, frm_ptime), 0,
        "Base frame ptime in msec."
    },
    {
        "pcm_bits_per_sample", T_INT, 
        offsetof(pjmedia_codec_param_info_Object, pcm_bits_per_sample), 0,
        "Bits/sample in the PCM side"
    },
    {
        "pt", T_INT, 
        offsetof(pjmedia_codec_param_info_Object, pt), 0,
        "Payload type"
    },
    
    {NULL}  /* Sentinel */
};




/*
 * pjmedia_codec_param_info_Type
 */
static PyTypeObject pjmedia_codec_param_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJMedia_Codec_Param_Info",      /*tp_name*/
    sizeof(pjmedia_codec_param_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,/*tp_dealloc*/
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
    "PJMedia Codec Param Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_info_members,         /* tp_members */
    

};

/*
 * pjmedia_codec_param_setting_Object
 * PJMedia Codec Param Setting
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    pj_uint8_t  frm_per_pkt; 
    unsigned    vad;
    unsigned    cng;
    unsigned    penh;
    unsigned    plc;
    unsigned    reserved;
    pj_uint8_t  enc_fmtp_mode;
    pj_uint8_t  dec_fmtp_mode; 

} pjmedia_codec_param_setting_Object;



/*
 * pjmedia_codec_param_setting_members
 */
static PyMemberDef pjmedia_codec_param_setting_members[] =
{   
    
    {
        "frm_per_pkt", T_INT, 
        offsetof(pjmedia_codec_param_setting_Object, frm_per_pkt), 0,
        "Number of frames per packet"
    },
    {
        "vad", T_INT, 
        offsetof(pjmedia_codec_param_setting_Object, vad), 0,
        "Voice Activity Detector"
    },
    {
        "penh", T_INT, 
        offsetof(pjmedia_codec_param_setting_Object, penh), 0,
        "Perceptual Enhancement"
    },
    {
        "plc", T_INT, 
        offsetof(pjmedia_codec_param_setting_Object, plc), 0,
        "Packet loss concealment"
    },
    {
        "reserved", T_INT, 
        offsetof(pjmedia_codec_param_setting_Object, reserved), 0,
        "Reserved, must be zero"
    },
    {
        "cng", T_INT, 
        offsetof(pjmedia_codec_param_setting_Object, cng), 0,
        "Comfort Noise Generator"
    },
    {
        "enc_fmtp_mode", T_INT, 
        offsetof(pjmedia_codec_param_setting_Object, enc_fmtp_mode), 0,
        "Mode param in fmtp (def:0)"
    },
    {
        "dec_fmtp_mode", T_INT, 
        offsetof(pjmedia_codec_param_setting_Object, dec_fmtp_mode), 0,
        "Mode param in fmtp (def:0)"
    },
    
    {NULL}  /* Sentinel */
};




/*
 * pjmedia_codec_param_setting_Type
 */
static PyTypeObject pjmedia_codec_param_setting_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJMedia_Codec_Param_Setting",      /*tp_name*/
    sizeof(pjmedia_codec_param_setting_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,/*tp_dealloc*/
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
    "PJMedia Codec Param Setting objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_setting_members,         /* tp_members */
    

};

/*
 * pjmedia_codec_param_Object
 * PJMedia Codec Param
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    pjmedia_codec_param_info_Object * info;
    pjmedia_codec_param_setting_Object * setting;

} pjmedia_codec_param_Object;


/*
 * pjmedia_codec_param_dealloc
 * deletes a pjmedia_codec_param from memory
 */
static void pjmedia_codec_param_dealloc(pjmedia_codec_param_Object* self)
{
    Py_XDECREF(self->info);        
    Py_XDECREF(self->setting);        
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * pjmedia_codec_param_new
 * constructor for pjmedia_codec_param object
 */
static PyObject * pjmedia_codec_param_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    pjmedia_codec_param_Object *self;

    self = (pjmedia_codec_param_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->info = (pjmedia_codec_param_info_Object *)
	    PyType_GenericNew(&pjmedia_codec_param_info_Type, NULL, NULL);
        if (self->info == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	self->setting = (pjmedia_codec_param_setting_Object *)
	    PyType_GenericNew(&pjmedia_codec_param_setting_Type, NULL, NULL);
        if (self->setting == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
    }
    return (PyObject *)self;
}

/*
 * pjmedia_codec_param_members
 */
static PyMemberDef pjmedia_codec_param_members[] =
{   
    
    {
        "info", T_OBJECT_EX,
        offsetof(pjmedia_codec_param_Object, info), 0,
        "The 'info' part of codec param describes the capability of the codec,"
        " and the value should NOT be changed by application."        
    },
    {
        "setting", T_OBJECT_EX,
        offsetof(pjmedia_codec_param_Object, setting), 0, 
        "The 'setting' part of codec param describes various settings to be "
        "applied to the codec. When the codec param is retrieved from the "
        "codec or codec factory, the values of these will be filled by "
        "the capability of the codec. Any features that are supported by "
        "the codec (e.g. vad or plc) will be turned on, so that application "
        "can query which capabilities are supported by the codec. "
        "Application may change the settings here before instantiating "
        "the codec/stream."        
    },
    
    {NULL}  /* Sentinel */
};




/*
 * pjmedia_codec_param_Type
 */
static PyTypeObject pjmedia_codec_param_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJMedia_Codec_Param",      /*tp_name*/
    sizeof(pjmedia_codec_param_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)pjmedia_codec_param_dealloc,/*tp_dealloc*/
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
    "PJMedia Codec Param objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    pjmedia_codec_param_new,             /* tp_new */

};

/*
 * py_pjsua_conf_get_max_ports
 */
static PyObject *py_pjsua_conf_get_max_ports
(PyObject *pSelf, PyObject *pArgs)
{    
    int ret;
    
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    ret = pjsua_conf_get_max_ports();
	
    return Py_BuildValue("i", ret);
}

/*
 * py_pjsua_conf_get_active_ports
 */
static PyObject *py_pjsua_conf_get_active_ports
(PyObject *pSelf, PyObject *pArgs)
{    
    int ret;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    ret = pjsua_conf_get_active_ports();
	
    return Py_BuildValue("i", ret);
}

/*
 * py_pjsua_enum_conf_ports
 * !modified @ 241206
 */
static PyObject *py_pjsua_enum_conf_ports(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_conf_port_id id[PJSUA_MAX_CONF_PORTS];
    unsigned c, i;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_conf_ports(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
	{
            return NULL;
	}
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_conf_get_port_info
 */
static PyObject *py_pjsua_conf_get_port_info
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    conf_port_info_Object * obj;
    pjsua_conf_port_info info;
    int status;	
    int i;

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }
	
    
    status = pjsua_conf_get_port_info(id, &info);
    obj = (conf_port_info_Object *)conf_port_info_new
	    (&conf_port_info_Type,NULL,NULL);
    obj->bits_per_sample = info.bits_per_sample;
    obj->channel_count = info.bits_per_sample;
    obj->clock_rate = info.clock_rate;
    obj->listener_cnt = info.listener_cnt;
    obj->name = PyString_FromStringAndSize(info.name.ptr, info.name.slen);
    obj->samples_per_frame = info.samples_per_frame;
    obj->slot_id = info.slot_id;
    
    for (i = 0; i < PJSUA_MAX_CONF_PORTS; i++) {
	PyObject * item = Py_BuildValue("i",info.listeners[i]);
	PyList_SetItem((PyObject *)obj->listeners, i, item);
    }
    return Py_BuildValue("O", obj);
}

/*
 * py_pjsua_conf_add_port
 */
static PyObject *py_pjsua_conf_add_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    int p_id;
    PyObject * oportObj;
    pjmedia_port_Object * oport;
    pjmedia_port * port;
    PyObject * opoolObj;
    pj_pool_Object * opool;
    pj_pool_t * pool;
    
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "OO", &opoolObj, &oportObj))
    {
        return NULL;
    }
    if (opoolObj != Py_None)
    {
        opool = (pj_pool_Object *)opoolObj;
		pool = opool->pool;
    } else {
       opool = NULL;
       pool = NULL;
    }
    if (oportObj != Py_None)
    {
        oport = (pjmedia_port_Object *)oportObj;
		port = oport->port;
    } else {
        oport = NULL;
        port = NULL;
    }

    status = pjsua_conf_add_port(pool, port, &p_id);
    
    
    return Py_BuildValue("ii", status, p_id);
}

/*
 * py_pjsua_conf_remove_port
 */
static PyObject *py_pjsua_conf_remove_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    status = pjsua_conf_remove_port(id);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_connect
 */
static PyObject *py_pjsua_conf_connect
(PyObject *pSelf, PyObject *pArgs)
{    	
    int source, sink;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "ii", &source, &sink))
    {
        return NULL;
    }	
    
    status = pjsua_conf_connect(source, sink);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_disconnect
 */
static PyObject *py_pjsua_conf_disconnect
(PyObject *pSelf, PyObject *pArgs)
{    	
    int source, sink;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "ii", &source, &sink))
    {
        return NULL;
    }	
    
    status = pjsua_conf_disconnect(source, sink);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_player_create
 */
static PyObject *py_pjsua_player_create
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int options;
    PyObject * filename;
    pj_str_t str;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "Oi", &filename, &options))
    {
        return NULL;
    }	
    str.ptr = PyString_AsString(filename);
    str.slen = strlen(PyString_AsString(filename));
    status = pjsua_player_create(&str, options, &id);
    
    return Py_BuildValue("ii", status, id);
}

/*
 * py_pjsua_player_get_conf_port
 */
static PyObject *py_pjsua_player_get_conf_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int id, port_id;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    port_id = pjsua_player_get_conf_port(id);
    
    
    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_player_set_pos
 */
static PyObject *py_pjsua_player_set_pos
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    pj_uint32_t samples;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "iI", &id, &samples))
    {
        return NULL;
    }	
    
    status = pjsua_player_set_pos(id, samples);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_player_destroy
 */
static PyObject *py_pjsua_player_destroy
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    status = pjsua_player_destroy(id);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_recorder_create
 * !modified @ 261206
 */
static PyObject *py_pjsua_recorder_create
(PyObject *pSelf, PyObject *pArgs)
{    	
    int p_id;
    int options;
    int max_size;
    PyObject * filename;
    pj_str_t str;
    PyObject * enc_param;
    pj_str_t strparam;
    int enc_type;
    
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "OiOii", &filename, 
		&enc_type, &enc_param, &max_size, &options))
    {
        return NULL;
    }	
    str.ptr = PyString_AsString(filename);
    str.slen = strlen(PyString_AsString(filename));
    strparam.ptr = PyString_AsString(enc_param);
    strparam.slen = strlen(PyString_AsString(enc_param));
    status = pjsua_recorder_create
		(&str, enc_type, NULL, max_size, options, &p_id);
    
    return Py_BuildValue("ii", status, p_id);
}

/*
 * py_pjsua_recorder_get_conf_port
 */
static PyObject *py_pjsua_recorder_get_conf_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int id, port_id;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    port_id = pjsua_recorder_get_conf_port(id);
    
    
    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_recorder_destroy
 */
static PyObject *py_pjsua_recorder_destroy
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    status = pjsua_recorder_destroy(id);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_enum_snd_devs
 */
static PyObject *py_pjsua_enum_snd_devs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjmedia_snd_dev_info info[SND_DEV_NUM];
    unsigned c, i;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(info);
    status = pjsua_enum_snd_devs(info, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret;
        int j;
        pjmedia_snd_dev_info_Object * obj;
        obj = (pjmedia_snd_dev_info_Object *)pjmedia_snd_dev_info_new
	    (&pjmedia_snd_dev_info_Type, NULL, NULL);
        obj->default_samples_per_sec = info[i].default_samples_per_sec;
        obj->input_count = info[i].input_count;
        obj->output_count = info[i].output_count;
        for (j = 0; j < SND_DEV_NUM; j++)
	{
            PyObject * ostr;
            char * str;
            str = (char *)malloc(sizeof(char));
            str[0] = info[i].name[j];
            ostr = PyString_FromStringAndSize(str,1);
            PyList_SetItem((PyObject *)obj->name, j, ostr);
            free(str);
	}
        ret = PyList_SetItem(list, i, (PyObject *)obj);
        if (ret == -1) 
	{
            return NULL;
	}
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_get_snd_dev
 */
static PyObject *py_pjsua_get_snd_dev
(PyObject *pSelf, PyObject *pArgs)
{    	
    int capture_dev, playback_dev;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    status = pjsua_get_snd_dev(&capture_dev, &playback_dev);
    
    
    return Py_BuildValue("ii", capture_dev, playback_dev);
}

/*
 * py_pjsua_set_snd_dev
 */
static PyObject *py_pjsua_set_snd_dev
(PyObject *pSelf, PyObject *pArgs)
{    	
    int capture_dev, playback_dev;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "ii", &capture_dev, &playback_dev))
    {
        return NULL;
    }	
    
    status = pjsua_set_snd_dev(capture_dev, playback_dev);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_set_null_snd_dev
 */
static PyObject *py_pjsua_set_null_snd_dev
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    status = pjsua_set_null_snd_dev();
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_set_no_snd_dev
 */
static PyObject *py_pjsua_set_no_snd_dev
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    pjmedia_port_Object * obj;	
    
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
     
    obj = (pjmedia_port_Object *)PyType_GenericNew
	(&pjmedia_port_Type, NULL, NULL);
    obj->port = pjsua_set_no_snd_dev();
    return Py_BuildValue("O", obj);
}

/*
 * py_pjsua_set_ec
 */
static PyObject *py_pjsua_set_ec
(PyObject *pSelf, PyObject *pArgs)
{    	
    int options;
    int tail_ms;
    int status;	
    

    if (!PyArg_ParseTuple(pArgs, "ii", &tail_ms, &options))
    {
        return NULL;
    }	
    
    status = pjsua_set_ec(tail_ms, options);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_get_ec_tail
 */
static PyObject *py_pjsua_get_ec_tail
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    unsigned p_tail_ms;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    status = pjsua_get_ec_tail(&p_tail_ms);
    
    
    return Py_BuildValue("i", p_tail_ms);
}

/*
 * py_pjsua_enum_codecs
 * !modified @ 261206
 */
static PyObject *py_pjsua_enum_codecs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_codec_info info[PJMEDIA_CODEC_MGR_MAX_CODECS];
    unsigned c, i;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(info);
    status = pjsua_enum_codecs(info, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret;
        int j;
        codec_info_Object * obj;
        obj = (codec_info_Object *)codec_info_new
	    (&codec_info_Type, NULL, NULL);
        obj->codec_id = PyString_FromStringAndSize
	    (info[i].codec_id.ptr, info[i].codec_id.slen);
        obj->priority = info[i].priority;
        for (j = 0; j < 32; j++)
        {	    
             obj->buf_[j] = info[i].buf_[j];
        }	
        ret = PyList_SetItem(list, i, (PyObject *)obj);
        if (ret == -1) {
            return NULL;
        }	
    }
    

    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_codec_set_priority
 */
static PyObject *py_pjsua_codec_set_priority
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    PyObject * id;
    pj_str_t str;
    pj_uint8_t priority;
    
    if (!PyArg_ParseTuple(pArgs, "OB", &id, &priority))
    {
        return NULL;
    }	
    str.ptr = PyString_AsString(id);
    str.slen = strlen(PyString_AsString(id));
    status = pjsua_codec_set_priority(&str, priority);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_codec_get_param
 */
static PyObject *py_pjsua_codec_get_param
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    PyObject * id;
    pj_str_t str;
    pjmedia_codec_param param;
    pjmedia_codec_param_Object *obj;
    
    
    if (!PyArg_ParseTuple(pArgs, "O", &id))
    {
        return NULL;
    }	
    str.ptr = PyString_AsString(id);
    str.slen = strlen(PyString_AsString(id));
    status = pjsua_codec_get_param(&str, &param);
    obj = (pjmedia_codec_param_Object *)pjmedia_codec_param_new
	(&pjmedia_codec_param_Type, NULL, NULL);
    obj->info->avg_bps = param.info.avg_bps;
    obj->info->channel_cnt = param.info.channel_cnt;
    obj->info->clock_rate = param.info.clock_rate;
    obj->info->frm_ptime = param.info.frm_ptime;
    obj->info->pcm_bits_per_sample = param.info.pcm_bits_per_sample;
    obj->info->pt = param.info.pt;
    obj->setting->cng = param.setting.cng;
    obj->setting->dec_fmtp_mode = param.setting.dec_fmtp_mode;
    obj->setting->enc_fmtp_mode = param.setting.enc_fmtp_mode;
    obj->setting->frm_per_pkt = param.setting.frm_per_pkt;
    obj->setting->penh = param.setting.penh;
    obj->setting->plc = param.setting.plc;
    obj->setting->reserved = param.setting.reserved;
    obj->setting->vad = param.setting.vad;

    return Py_BuildValue("O", obj);
}
/*
 * py_pjsua_codec_set_param
 */
static PyObject *py_pjsua_codec_set_param
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    PyObject * id;
    pj_str_t str;
    pjmedia_codec_param param;
    PyObject * tmpObj;
    pjmedia_codec_param_Object *obj;
    
    
    if (!PyArg_ParseTuple(pArgs, "OO", &id, &tmpObj))
    {
        return NULL;
    }	

    str.ptr = PyString_AsString(id);
    str.slen = strlen(PyString_AsString(id));
    if (tmpObj != Py_None)
    {
        obj = (pjmedia_codec_param_Object *)tmpObj;
        param.info.avg_bps = obj->info->avg_bps;
        param.info.channel_cnt = obj->info->channel_cnt;
        param.info.clock_rate = obj->info->clock_rate;
        param.info.frm_ptime = obj->info->frm_ptime;
        param.info.pcm_bits_per_sample = obj->info->pcm_bits_per_sample;
        param.info.pt = obj->info->pt;
        param.setting.cng = obj->setting->cng;
        param.setting.dec_fmtp_mode = obj->setting->dec_fmtp_mode;
        param.setting.enc_fmtp_mode = obj->setting->enc_fmtp_mode;
        param.setting.frm_per_pkt = obj->setting->frm_per_pkt;
        param.setting.penh = obj->setting->penh;
        param.setting.plc = obj->setting->plc;
        param.setting.reserved = obj->setting->reserved;
        param.setting.vad = obj->setting->vad;
        status = pjsua_codec_set_param(&str, &param);
    } else {
        status = pjsua_codec_set_param(&str, NULL);
    }
    return Py_BuildValue("i", status);
}

static char pjsua_conf_get_max_ports_doc[] =
    "int py_pjsua.conf_get_max_ports () "
    "Get maxinum number of conference ports.";
static char pjsua_conf_get_active_ports_doc[] =
    "int py_pjsua.conf_get_active_ports () "
    "Get current number of active ports in the bridge.";
static char pjsua_enum_conf_ports_doc[] =
    "int[] py_pjsua.enum_conf_ports () "
    "Enumerate all conference ports.";
static char pjsua_conf_get_port_info_doc[] =
    "py_pjsua.Conf_Port_Info py_pjsua.conf_get_port_info (int id) "
    "Get information about the specified conference port";
static char pjsua_conf_add_port_doc[] =
    "int, int py_pjsua.conf_add_port "
    "(py_pjsua.PJ_Pool pool, py_pjsua.PJMedia_Port port) "
    "Add arbitrary media port to PJSUA's conference bridge. "
    "Application can use this function to add the media port "
    "that it creates. For media ports that are created by PJSUA-LIB "
    "(such as calls, file player, or file recorder), PJSUA-LIB will "
    "automatically add the port to the bridge.";
static char pjsua_conf_remove_port_doc[] =
    "int py_pjsua.conf_remove_port (int id) "
    "Remove arbitrary slot from the conference bridge. "
    "Application should only call this function "
    "if it registered the port manually.";
static char pjsua_conf_connect_doc[] =
    "int py_pjsua.conf_connect (int source, int sink) "
    "Establish unidirectional media flow from souce to sink. "
    "One source may transmit to multiple destinations/sink. "
    "And if multiple sources are transmitting to the same sink, "
    "the media will be mixed together. Source and sink may refer "
    "to the same ID, effectively looping the media. "
    "If bidirectional media flow is desired, application "
    "needs to call this function twice, with the second "
    "one having the arguments reversed.";
static char pjsua_conf_disconnect_doc[] =
    "int py_pjsua.conf_disconnect (int source, int sink) "
    "Disconnect media flow from the source to destination port.";
static char pjsua_player_create_doc[] =
    "int, int py_pjsua.player_create (string filename, int options) "
    "Create a file player, and automatically connect "
    "this player to the conference bridge.";
static char pjsua_player_get_conf_port_doc[] =
    "int py_pjsua.player_get_conf_port (int) "
    "Get conference port ID associated with player.";
static char pjsua_player_set_pos_doc[] =
    "int py_pjsua.player_set_pos (int id, int samples) "
    "Set playback position.";
static char pjsua_player_destroy_doc[] =
    "int py_pjsua.player_destroy (int id) "
    "Close the file, remove the player from the bridge, "
    "and free resources associated with the file player.";
static char pjsua_recorder_create_doc[] =
    "int, int py_pjsua.recorder_create (string filename, "
    "int enc_type, int enc_param, int max_size, int options) "
    "Create a file recorder, and automatically connect this recorder "
    "to the conference bridge. The recorder currently supports recording "
    "WAV file, and on Windows, MP3 file. The type of the recorder to use "
    "is determined by the extension of the file (e.g. '.wav' or '.mp3').";
static char pjsua_recorder_get_conf_port_doc[] =
    "int py_pjsua.recorder_get_conf_port (int id) "
    "Get conference port associated with recorder.";
static char pjsua_recorder_destroy_doc[] =
    "int py_pjsua.recorder_destroy (int id) "
    "Destroy recorder (this will complete recording).";
static char pjsua_enum_snd_devs_doc[] =
    "py_pjsua.PJMedia_Snd_Dev_Info[] py_pjsua.enum_snd_devs (int count) "
    "Enum sound devices.";
static char pjsua_get_snd_dev_doc[] =
    "int, int py_pjsua.get_snd_dev () "
    "Get currently active sound devices. "
    "If sound devices has not been created "
    "(for example when pjsua_start() is not called), "
    "it is possible that the function returns "
    "PJ_SUCCESS with -1 as device IDs.";
static char pjsua_set_snd_dev_doc[] =
    "int py_pjsua.set_snd_dev (int capture_dev, int playback_dev) "
    "Select or change sound device. Application may call this function "
    "at any time to replace current sound device.";
static char pjsua_set_null_snd_dev_doc[] =
    "int py_pjsua.set_null_snd_dev () "
    "Set pjsua to use null sound device. The null sound device only "
    "provides the timing needed by the conference bridge, and will not "
    "interract with any hardware.";
static char pjsua_set_no_snd_dev_doc[] =
    "py_pjsua.PJMedia_Port py_pjsua.set_no_snd_dev () "
    "Disconnect the main conference bridge from any sound devices, "
    "and let application connect the bridge to it's "
    "own sound device/master port.";
static char pjsua_set_ec_doc[] =
    "int py_pjsua.set_ec (int tail_ms, int options) "
    "Configure the echo canceller tail length of the sound port.";
static char pjsua_get_ec_tail_doc[] =
    "int py_pjsua.get_ec_tail () "
    "Get current echo canceller tail length.";
static char pjsua_enum_codecs_doc[] =
    "py_pjsua.Codec_Info[] py_pjsua.enum_codecs () "
    "Enum all supported codecs in the system.";
static char pjsua_codec_set_priority_doc[] =
    "int py_pjsua.codec_set_priority (string id, int priority) "
    "Change codec priority.";
static char pjsua_codec_get_param_doc[] =
    "py_pjsua.PJMedia_Codec_Param py_pjsua.codec_get_param (string id) "
    "Get codec parameters";
static char pjsua_codec_set_param_doc[] =
    "int py_pjsua.codec_set_param (string id, "
    "py_pjsua.PJMedia_Codec_Param param) "
    "Set codec parameters.";

/* END OF LIB MEDIA */

/* LIB CALL */

/*
 * pj_time_val_Object
 * PJ Time Val
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    long sec;
    long msec;

} pj_time_val_Object;



/*
 * pj_time_val_members
 */
static PyMemberDef pj_time_val_members[] =
{   
    
    {
        "sec", T_INT, 
        offsetof(pj_time_val_Object, sec), 0,
        "The seconds part of the time"
    },
    {
        "msec", T_INT, 
        offsetof(pj_time_val_Object, sec), 0,
        "The milliseconds fraction of the time"
    },
    
    
    {NULL}  /* Sentinel */
};




/*
 * pj_time_val_Type
 */
static PyTypeObject pj_time_val_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJ_Time_Val",      /*tp_name*/
    sizeof(pj_time_val_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,/*tp_dealloc*/
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
    "PJ Time Val objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pj_time_val_members,         /* tp_members */
    

};

/*
 * call_info_Object
 * Call Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    int id;
    int role;
    int acc_id;
    PyObject * local_info;
    PyObject * local_contact;
    PyObject * remote_info;
    PyObject * remote_contact;
    PyObject * call_id;
    int state;
    PyObject * state_text;
    int last_status;
    PyObject * last_status_text;
    int media_status;
    int media_dir;
    int conf_slot;
    pj_time_val_Object * connect_duration;
    pj_time_val_Object * total_duration;
    struct {
	char local_info[128];
	char local_contact[128];
	char remote_info[128];
	char remote_contact[128];
	char call_id[128];
	char last_status_text[128];
    } buf_;

} call_info_Object;


/*
 * call_info_dealloc
 * deletes a call_info from memory
 */
static void call_info_dealloc(call_info_Object* self)
{
    Py_XDECREF(self->local_info);
    Py_XDECREF(self->local_contact);
    Py_XDECREF(self->remote_info);
    Py_XDECREF(self->remote_contact);
    Py_XDECREF(self->call_id);
    Py_XDECREF(self->state_text);
    Py_XDECREF(self->last_status_text);
    Py_XDECREF(self->connect_duration);
    Py_XDECREF(self->total_duration);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * call_info_new
 * constructor for call_info object
 */
static PyObject * call_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    call_info_Object *self;

    self = (call_info_Object *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->local_info = PyString_FromString("");
        if (self->local_info == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }       
	self->local_contact = PyString_FromString("");
        if (self->local_contact == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->remote_info = PyString_FromString("");
        if (self->remote_info == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->remote_contact = PyString_FromString("");
        if (self->remote_contact == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->call_id = PyString_FromString("");
        if (self->call_id == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->state_text = PyString_FromString("");
        if (self->state_text == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->last_status_text = PyString_FromString("");
        if (self->last_status_text == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->connect_duration = (pj_time_val_Object *)PyType_GenericNew
	    (&pj_time_val_Type,NULL,NULL);
        if (self->connect_duration == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->total_duration = (pj_time_val_Object *)PyType_GenericNew
	    (&pj_time_val_Type,NULL,NULL);
        if (self->total_duration == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }
    return (PyObject *)self;
}

/*
 * call_info_members
 */
static PyMemberDef call_info_members[] =
{   
    {
        "id", T_INT, 
        offsetof(call_info_Object, id), 0,
        "Call identification"
    },
    {
        "role", T_INT, 
        offsetof(call_info_Object, role), 0,
        "Initial call role (UAC == caller)"
    },
    {
        "acc_id", T_INT, 
        offsetof(call_info_Object, acc_id), 0,
        "The account ID where this call belongs."
    },
    {
        "local_info", T_OBJECT_EX,
        offsetof(call_info_Object, local_info), 0,
        "Local URI"        
    },
    {
        "local_contact", T_OBJECT_EX,
        offsetof(call_info_Object, local_contact), 0,
        "Local Contact"        
    },
    {
        "remote_info", T_OBJECT_EX,
        offsetof(call_info_Object, remote_info), 0,
        "Remote URI"        
    },
    {
        "remote_contact", T_OBJECT_EX,
        offsetof(call_info_Object, remote_contact), 0,
        "Remote Contact"        
    },
    {
        "call_id", T_OBJECT_EX,
        offsetof(call_info_Object, call_id), 0,
        "Dialog Call-ID string"        
    },
    {
        "state", T_INT, 
        offsetof(call_info_Object, state), 0,
        "Call state"
    },
    {
        "state_text", T_OBJECT_EX,
        offsetof(call_info_Object, state_text), 0,
        "Text describing the state "        
    },
    {
        "last_status", T_INT, 
        offsetof(call_info_Object, last_status), 0,
        "Last status code heard, which can be used as cause code"
    },
    {
        "last_status_text", T_OBJECT_EX,
        offsetof(call_info_Object, last_status_text), 0,
        "The reason phrase describing the status."        
    },
    {
        "media_status", T_INT, 
        offsetof(call_info_Object, media_status), 0,
        "Call media status."
    },
    {
        "media_dir", T_INT, 
        offsetof(call_info_Object, media_dir), 0,
        "Media direction"
    },
    {
        "conf_slot", T_INT, 
        offsetof(call_info_Object, conf_slot), 0,
        "The conference port number for the call"
    },
    {
        "connect_duration", T_OBJECT_EX,
        offsetof(call_info_Object, connect_duration), 0,
        "Up-to-date call connected duration(zero when call is not established)"
    },
    {
        "total_duration", T_OBJECT_EX,
        offsetof(call_info_Object, total_duration), 0,
        "Total call duration, including set-up time"        
    },
    
    {NULL}  /* Sentinel */
};




/*
 * call_info_Type
 */
static PyTypeObject call_info_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Call_Info",      /*tp_name*/
    sizeof(call_info_Object),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)call_info_dealloc,/*tp_dealloc*/
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
    "Call Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    call_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    call_info_new,             /* tp_new */

};

/*
 * py_pjsua_call_get_max_count
 */
static PyObject *py_pjsua_call_get_max_count
(PyObject *pSelf, PyObject *pArgs)
{    	
    int count;

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    count = pjsua_call_get_max_count();
    
    
    return Py_BuildValue("i", count);
}

/*
 * py_pjsua_call_get_count
 */
static PyObject *py_pjsua_call_get_count
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int count;	
    

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    count = pjsua_call_get_count();
    
    
    return Py_BuildValue("i", count);
}

/*
 * py_pjsua_enum_calls
 */
static PyObject *py_pjsua_enum_calls(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_transport_id id[PJSUA_MAX_CALLS];
    unsigned c, i;
    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_calls(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {     
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
        {
            return NULL;
        }
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_call_make_call
 */
static PyObject *py_pjsua_call_make_call
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int acc_id;
    pj_str_t dst_uri;
    PyObject * sd;
    unsigned options;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;
    int user_data;
    int call_id;
    pj_pool_t * pool;

    if (!PyArg_ParseTuple
		(pArgs, "iOIiO", &acc_id, &sd, &options, &user_data, &omdObj))
    {
        return NULL;
    }
	
    dst_uri.ptr = PyString_AsString(sd);
    dst_uri.slen = strlen(PyString_AsString(sd));
    if (omdObj != Py_None) 
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_make_call(acc_id, &dst_uri, 
			options, (void*)user_data, &msg_data, &call_id);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_make_call(acc_id, &dst_uri, 
			options, (void*)user_data, NULL, &call_id);	
    }

    return Py_BuildValue("ii",status, call_id);
}

/*
 * py_pjsua_call_is_active
 */
static PyObject *py_pjsua_call_is_active
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int isActive;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    isActive = pjsua_call_is_active(call_id);
    
    
    return Py_BuildValue("i", isActive);
}

/*
 * py_pjsua_call_has_media
 */
static PyObject *py_pjsua_call_has_media
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int hasMedia;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    hasMedia = pjsua_call_has_media(call_id);
    
    
    return Py_BuildValue("i", hasMedia);
}

/*
 * py_pjsua_call_get_conf_port
 */
static PyObject *py_pjsua_call_get_conf_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int port_id;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    port_id = pjsua_call_get_conf_port(call_id);
    
    
    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_call_get_info
 */
static PyObject *py_pjsua_call_get_info
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int status;
    call_info_Object * oi;
    pjsua_call_info info;
    

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    
    status = pjsua_call_get_info(call_id, &info);
    if (status == PJ_SUCCESS) 
    {
        oi = (call_info_Object *)call_info_new(&call_info_Type, NULL, NULL);
        oi->acc_id = info.acc_id;
        pj_ansi_snprintf(oi->buf_.call_id, sizeof(oi->buf_.call_id),
	    "%.*s", (int)info.call_id.slen, info.call_id.ptr);
        pj_ansi_snprintf(oi->buf_.last_status_text, 
	    sizeof(oi->buf_.last_status_text),
	    "%.*s", (int)info.last_status_text.slen, info.last_status_text.ptr);
        pj_ansi_snprintf(oi->buf_.local_contact, sizeof(oi->buf_.local_contact),
	    "%.*s", (int)info.local_contact.slen, info.local_contact.ptr);
        pj_ansi_snprintf(oi->buf_.local_info, sizeof(oi->buf_.local_info),
	    "%.*s", (int)info.local_info.slen, info.local_info.ptr);
        pj_ansi_snprintf(oi->buf_.remote_contact,
	    sizeof(oi->buf_.remote_contact),
	    "%.*s", (int)info.remote_contact.slen, info.remote_contact.ptr);
        pj_ansi_snprintf(oi->buf_.remote_info, sizeof(oi->buf_.remote_info),
	    "%.*s", (int)info.remote_info.slen, info.remote_info.ptr);

        oi->call_id = PyString_FromStringAndSize(info.call_id.ptr, 
	    info.call_id.slen);
        oi->conf_slot = info.conf_slot;
        oi->connect_duration->sec = info.connect_duration.sec;
        oi->connect_duration->msec = info.connect_duration.msec;
        oi->total_duration->sec = info.total_duration.sec;
        oi->total_duration->msec = info.total_duration.msec;
        oi->id = info.id;
        oi->last_status = info.last_status;
        oi->last_status_text = PyString_FromStringAndSize(
	    info.last_status_text.ptr, info.last_status_text.slen);
        oi->local_contact = PyString_FromStringAndSize(
	    info.local_contact.ptr, info.local_contact.slen);
        oi->local_info = PyString_FromStringAndSize(
   	    info.local_info.ptr, info.local_info.slen);
        oi->remote_contact = PyString_FromStringAndSize(
	    info.remote_contact.ptr, info.remote_contact.slen);
        oi->remote_info = PyString_FromStringAndSize(
	    info.remote_info.ptr, info.remote_info.slen);
        oi->media_dir = info.media_dir;
        oi->media_status = info.media_status;
        oi->role = info.role;
        oi->state = info.state;
        oi->state_text = PyString_FromStringAndSize(
   	    info.state_text.ptr, info.state_text.slen);

	return Py_BuildValue("O", oi);
    } else {
	Py_INCREF(Py_None);
	return Py_None;
    }
}

/*
 * py_pjsua_call_set_user_data
 */
static PyObject *py_pjsua_call_set_user_data
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int user_data;	
    int status;

    if (!PyArg_ParseTuple(pArgs, "ii", &call_id, &user_data))
    {
        return NULL;
    }	
    
    status = pjsua_call_set_user_data(call_id, (void*)user_data);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_get_user_data
 */
static PyObject *py_pjsua_call_get_user_data
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    void * user_data;	
    

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    user_data = pjsua_call_get_user_data(call_id);
    
    
    return Py_BuildValue("i", (int)user_data);
}

/*
 * py_pjsua_call_answer
 */
static PyObject *py_pjsua_call_answer
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t * reason;
    PyObject * sr;
    unsigned code;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;    
    pj_pool_t * pool;

    if (!PyArg_ParseTuple(pArgs, "iIOO", &call_id, &code, &sr, &omdObj))
    {
        return NULL;
    }
    if (sr == Py_None) 
    {
        reason = NULL;
    } else {
	reason = (pj_str_t *)malloc(sizeof(pj_str_t));
        reason->ptr = PyString_AsString(sr);
        reason->slen = strlen(PyString_AsString(sr));
    }
    if (omdObj != Py_None) 
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
	
        status = pjsua_call_answer(call_id, code, reason, &msg_data);	
    
        pj_pool_release(pool);
    } else {
	
        status = pjsua_call_answer(call_id, code, reason, NULL);	
    }
    if (reason != NULL)
    {
        free(reason);
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_hangup
 */
static PyObject *py_pjsua_call_hangup
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t * reason;
    PyObject * sr;
    unsigned code;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;    
    pj_pool_t * pool = NULL;

    if (!PyArg_ParseTuple(pArgs, "iIOO", &call_id, &code, &sr, &omdObj))
    {
        return NULL;
    }
    if (sr != Py_None)
    {
        reason = NULL;
    } else {
        reason = (pj_str_t *)malloc(sizeof(pj_str_t));
        reason->ptr = PyString_AsString(sr);
        reason->slen = strlen(PyString_AsString(sr));
    }
    if (omdObj != Py_None) 
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_hangup(call_id, code, reason, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_hangup(call_id, code, reason, NULL);	
    }
    if (reason != NULL)
    {
        free(reason);
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_set_hold
 */
static PyObject *py_pjsua_call_set_hold
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    pjsua_msg_data msg_data;
	PyObject * omdObj;
    msg_data_Object * omd;    
    pj_pool_t * pool;

    if (!PyArg_ParseTuple(pArgs, "iO", &call_id, &omdObj))
    {
        return NULL;
    }

    if (omdObj != Py_None) 
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_set_hold(call_id, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_set_hold(call_id, NULL);	
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_reinvite
 */
static PyObject *py_pjsua_call_reinvite
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    int unhold;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;    
    pj_pool_t * pool;

    if (!PyArg_ParseTuple(pArgs, "iiO", &call_id, &unhold, &omdObj))
    {
        return NULL;
    }

    if (omdObj != Py_None)
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_reinvite(call_id, unhold, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_reinvite(call_id, unhold, NULL);
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_xfer
 */
static PyObject *py_pjsua_call_xfer
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t dest;
    PyObject * sd;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;    
    pj_pool_t * pool;

    if (!PyArg_ParseTuple(pArgs, "iOO", &call_id, &sd, &omdObj))
    {
        return NULL;
    }
	
    dest.ptr = PyString_AsString(sd);
    dest.slen = strlen(PyString_AsString(sd));
    
    if (omdObj != Py_None)
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_xfer(call_id, &dest, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_xfer(call_id, &dest, NULL);	
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_xfer_replaces
 */
static PyObject *py_pjsua_call_xfer_replaces
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    int dest_call_id;
    unsigned options;    
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;    
    pj_pool_t * pool;

    if (!PyArg_ParseTuple
		(pArgs, "iiIO", &call_id, &dest_call_id, &options, &omdObj))
    {
        return NULL;
    }
	
    if (omdObj != Py_None)
    {
        omd = (msg_data_Object *)omdObj;    
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_xfer_replaces
			(call_id, dest_call_id, options, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_xfer_replaces(call_id, dest_call_id,options, NULL);	
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_dial_dtmf
 */
static PyObject *py_pjsua_call_dial_dtmf
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    PyObject * sd;
    pj_str_t digits;
    int status;

    if (!PyArg_ParseTuple(pArgs, "iO", &call_id, &sd))
    {
        return NULL;
    }	
    digits.ptr = PyString_AsString(sd);
    digits.slen = strlen(PyString_AsString(sd));
    status = pjsua_call_dial_dtmf(call_id, &digits);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_send_im
 */
static PyObject *py_pjsua_call_send_im
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t content;
    pj_str_t * mime_type;
    PyObject * sm;
    PyObject * sc;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;    
    int user_data;
    pj_pool_t * pool;

    if (!PyArg_ParseTuple
		(pArgs, "iOOOi", &call_id, &sm, &sc, &omdObj, &user_data))
    {
        return NULL;
    }
    if (sm == Py_None)
    {
        mime_type = NULL;
    } else {
        mime_type = (pj_str_t *)malloc(sizeof(pj_str_t));
        mime_type->ptr = PyString_AsString(sm);
        mime_type->slen = strlen(PyString_AsString(sm));
    }
    content.ptr = PyString_AsString(sc);
    content.slen = strlen(PyString_AsString(sc));
    
    if (omdObj != Py_None)
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_send_im
		(call_id, mime_type, &content, &msg_data, (void *)user_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_send_im
			(call_id, mime_type, &content, NULL, (void *)user_data);	
    }
    if (mime_type != NULL)
    {
        free(mime_type);
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_send_typing_ind
 */
static PyObject *py_pjsua_call_send_typing_ind
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    int is_typing;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    msg_data_Object * omd;    
    pj_pool_t * pool;

    if (!PyArg_ParseTuple(pArgs, "iiO", &call_id, &is_typing, &omdObj))
    {
        return NULL;
    }
	
    if (omdObj != Py_None)
    {
        omd = (msg_data_Object *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_send_typing_ind(call_id, is_typing, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_send_typing_ind(call_id, is_typing, NULL);	
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_hangup_all
 */
static PyObject *py_pjsua_call_hangup_all
(PyObject *pSelf, PyObject *pArgs)
{    	

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    pjsua_call_hangup_all();
    
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_call_dump
 */
static PyObject *py_pjsua_call_dump
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int with_media;
    PyObject * sb;
    PyObject * si;
    char * buffer;
    char * indent;
    unsigned maxlen;    
    int status;

    if (!PyArg_ParseTuple(pArgs, "iiIO", &call_id, &with_media, &maxlen, &si))
    {
        return NULL;
    }	
    buffer = (char *) malloc (maxlen * sizeof(char));
    indent = PyString_AsString(si);
    
    status = pjsua_call_dump(call_id, with_media, buffer, maxlen, indent);
    sb = PyString_FromStringAndSize(buffer, maxlen);
    free(buffer);
    return Py_BuildValue("O", sb);
}

static char pjsua_call_get_max_count_doc[] =
    "int py_pjsua.call_get_max_count () "
    "Get maximum number of calls configured in pjsua.";
static char pjsua_call_get_count_doc[] =
    "int py_pjsua.call_get_count () "
    "Get number of currently active calls.";
static char pjsua_enum_calls_doc[] =
    "int[] py_pjsua.enum_calls () "
    "Get maximum number of calls configured in pjsua.";
static char pjsua_call_make_call_doc[] =
    "int,int py_pjsua.call_make_call (int acc_id, string dst_uri, int options,"
    "int user_data, py_pjsua.Msg_Data msg_data) "
    "Make outgoing call to the specified URI using the specified account.";
static char pjsua_call_is_active_doc[] =
    "int py_pjsua.call_is_active (int call_id) "
    "Check if the specified call has active INVITE session and the INVITE "
    "session has not been disconnected.";
static char pjsua_call_has_media_doc[] =
    "int py_pjsua.call_has_media (int call_id) "
    "Check if call has an active media session.";
static char pjsua_call_get_conf_port_doc[] =
    "int py_pjsua.call_get_conf_port (int call_id) "
    "Get the conference port identification associated with the call.";
static char pjsua_call_get_info_doc[] =
    "py_pjsua.Call_Info py_pjsua.call_get_info (int call_id) "
    "Obtain detail information about the specified call.";
static char pjsua_call_set_user_data_doc[] =
    "int py_pjsua.call_set_user_data (int call_id, int user_data) "
    "Attach application specific data to the call.";
static char pjsua_call_get_user_data_doc[] =
    "int py_pjsua.call_get_user_data (int call_id) "
    "Get user data attached to the call.";
static char pjsua_call_answer_doc[] =
    "int py_pjsua.call_answer (int call_id, int code, string reason, "
    "py_pjsua.Msg_Data msg_data) "
    "Send response to incoming INVITE request.";
static char pjsua_call_hangup_doc[] =
    "int py_pjsua.call_hangup (int call_id, int code, string reason, "
    "py_pjsua.Msg_Data msg_data) "
    "Hangup call by using method that is appropriate according "
    "to the call state.";
static char pjsua_call_set_hold_doc[] =
    "int py_pjsua.call_set_hold (int call_id, py_pjsua.Msg_Data msg_data) "
    "Put the specified call on hold.";
static char pjsua_call_reinvite_doc[] =
    "int py_pjsua.call_reinvite (int call_id, int unhold, "
    "py_pjsua.Msg_Data msg_data) "
    "Send re-INVITE (to release hold).";
static char pjsua_call_xfer_doc[] =
    "int py_pjsua.call_xfer (int call_id, string dest, "
    "py_pjsua.Msg_Data msg_data) "
    "Initiate call transfer to the specified address. "
    "This function will send REFER request to instruct remote call party "
    "to initiate a new INVITE session to the specified destination/target.";
static char pjsua_call_xfer_replaces_doc[] =
    "int py_pjsua.call_xfer_replaces (int call_id, int dest_call_id, "
    "int options, py_pjsua.Msg_Data msg_data) "
    "Initiate attended call transfer. This function will send REFER request "
    "to instruct remote call party to initiate new INVITE session to the URL "
    "of dest_call_id. The party at dest_call_id then should 'replace' the call"
    "with us with the new call from the REFER recipient.";
static char pjsua_call_dial_dtmf_doc[] =
    "int py_pjsua.call_dial_dtmf (int call_id, string digits) "
    "Send DTMF digits to remote using RFC 2833 payload formats.";
static char pjsua_call_send_im_doc[] =
    "int py_pjsua.call_send_im (int call_id, string mime_type, string content,"
    "py_pjsua.Msg_Data msg_data, int user_data) "
    "Send instant messaging inside INVITE session.";
static char pjsua_call_send_typing_ind_doc[] =
    "int py_pjsua.call_send_typing_ind (int call_id, int is_typing, "
    "py_pjsua.Msg_Data msg_data) "
    "Send IM typing indication inside INVITE session.";
static char pjsua_call_hangup_all_doc[] =
    "void py_pjsua.call_hangup_all () "
    "Terminate all calls.";
static char pjsua_call_dump_doc[] =
    "int py_pjsua.call_dump (int call_id, int with_media, int maxlen, "
    "string indent) "
    "Dump call and media statistics to string.";

/* END OF LIB CALL */

/*
 * Map of function names to functions
 */
static PyMethodDef py_pjsua_methods[] =
{
    {
        "thread_register", py_pjsua_thread_register, METH_VARARGS, 
         pjsua_thread_register_doc
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
    {
        "buddy_config_default", py_pjsua_buddy_config_default, METH_VARARGS,
        pjsua_buddy_config_default_doc
    },
    {
        "get_buddy_count", py_pjsua_get_buddy_count, METH_VARARGS,
        pjsua_get_buddy_count_doc
    },
    {
        "buddy_is_valid", py_pjsua_buddy_is_valid, METH_VARARGS,
        pjsua_buddy_is_valid_doc
    },
    {
        "enum_buddies", py_pjsua_enum_buddies, METH_VARARGS,
        pjsua_enum_buddies_doc
    },    
    {
        "buddy_get_info", py_pjsua_buddy_get_info, METH_VARARGS,
        pjsua_buddy_get_info_doc
    },
    {
        "buddy_add", py_pjsua_buddy_add, METH_VARARGS,
        pjsua_buddy_add_doc
    },
    {
        "buddy_del", py_pjsua_buddy_del, METH_VARARGS,
        pjsua_buddy_del_doc
    },
    {
        "buddy_subscribe_pres", py_pjsua_buddy_subscribe_pres, METH_VARARGS,
        pjsua_buddy_subscribe_pres_doc
    },
    {
        "pres_dump", py_pjsua_pres_dump, METH_VARARGS,
        pjsua_pres_dump_doc
    },
    {
        "im_send", py_pjsua_im_send, METH_VARARGS,
        pjsua_im_send_doc
    },
    {
        "im_typing", py_pjsua_im_typing, METH_VARARGS,
        pjsua_im_typing_doc
    },
        {
        "conf_get_max_ports", py_pjsua_conf_get_max_ports, METH_VARARGS,
        pjsua_conf_get_max_ports_doc
    },
    {
        "conf_get_active_ports", py_pjsua_conf_get_active_ports, METH_VARARGS,
        pjsua_conf_get_active_ports_doc
    },
    {
        "enum_conf_ports", py_pjsua_enum_conf_ports, METH_VARARGS,
        pjsua_enum_conf_ports_doc
    },
    {
        "conf_get_port_info", py_pjsua_conf_get_port_info, METH_VARARGS,
        pjsua_conf_get_port_info_doc
    },
    {
        "conf_add_port", py_pjsua_conf_add_port, METH_VARARGS,
        pjsua_conf_add_port_doc
    },
    {
        "conf_remove_port", py_pjsua_conf_remove_port, METH_VARARGS,
        pjsua_conf_remove_port_doc
    },
    {
        "conf_connect", py_pjsua_conf_connect, METH_VARARGS,
        pjsua_conf_connect_doc
    },
    {
        "conf_disconnect", py_pjsua_conf_disconnect, METH_VARARGS,
        pjsua_conf_disconnect_doc
    },
    {
        "player_create", py_pjsua_player_create, METH_VARARGS,
        pjsua_player_create_doc
    },
    {
        "player_get_conf_port", py_pjsua_player_get_conf_port, METH_VARARGS,
        pjsua_player_get_conf_port_doc
    },
    {
        "player_set_pos", py_pjsua_player_set_pos, METH_VARARGS,
        pjsua_player_set_pos_doc
    },
    {
        "player_destroy", py_pjsua_player_destroy, METH_VARARGS,
        pjsua_player_destroy_doc
    },
    {
        "recorder_create", py_pjsua_recorder_create, METH_VARARGS,
        pjsua_recorder_create_doc
    },
    {
        "recorder_get_conf_port", py_pjsua_recorder_get_conf_port, METH_VARARGS,
        pjsua_recorder_get_conf_port_doc
    },
    {
        "recorder_destroy", py_pjsua_recorder_destroy, METH_VARARGS,
        pjsua_recorder_destroy_doc
    },
    {
        "enum_snd_devs", py_pjsua_enum_snd_devs, METH_VARARGS,
        pjsua_enum_snd_devs_doc
    },
    {
        "get_snd_dev", py_pjsua_get_snd_dev, METH_VARARGS,
        pjsua_get_snd_dev_doc
    },
    {
        "set_snd_dev", py_pjsua_set_snd_dev, METH_VARARGS,
        pjsua_set_snd_dev_doc
    },
    {
        "set_null_snd_dev", py_pjsua_set_null_snd_dev, METH_VARARGS,
        pjsua_set_null_snd_dev_doc
    },
    {
        "set_no_snd_dev", py_pjsua_set_no_snd_dev, METH_VARARGS,
        pjsua_set_no_snd_dev_doc
    },
    {
        "set_ec", py_pjsua_set_ec, METH_VARARGS,
        pjsua_set_ec_doc
    },
    {
        "get_ec_tail", py_pjsua_get_ec_tail, METH_VARARGS,
        pjsua_get_ec_tail_doc
    },
    {
        "enum_codecs", py_pjsua_enum_codecs, METH_VARARGS,
        pjsua_enum_codecs_doc
    },
    {
        "codec_set_priority", py_pjsua_codec_set_priority, METH_VARARGS,
        pjsua_codec_set_priority_doc
    },
    {
        "codec_get_param", py_pjsua_codec_get_param, METH_VARARGS,
        pjsua_codec_get_param_doc
    },
    {
        "codec_set_param", py_pjsua_codec_set_param, METH_VARARGS,
        pjsua_codec_set_param_doc
    },
    {
        "call_get_max_count", py_pjsua_call_get_max_count, METH_VARARGS,
        pjsua_call_get_max_count_doc
    },
    {
        "call_get_count", py_pjsua_call_get_count, METH_VARARGS,
        pjsua_call_get_count_doc
    },
    {
        "enum_calls", py_pjsua_enum_calls, METH_VARARGS,
        pjsua_enum_calls_doc
    },
    {
        "call_make_call", py_pjsua_call_make_call, METH_VARARGS,
        pjsua_call_make_call_doc
    },
    {
        "call_is_active", py_pjsua_call_is_active, METH_VARARGS,
        pjsua_call_is_active_doc
    },
    {
        "call_has_media", py_pjsua_call_has_media, METH_VARARGS,
        pjsua_call_has_media_doc
    },
    {
        "call_get_conf_port", py_pjsua_call_get_conf_port, METH_VARARGS,
        pjsua_call_get_conf_port_doc
    },
    {
        "call_get_info", py_pjsua_call_get_info, METH_VARARGS,
        pjsua_call_get_info_doc
    },
    {
        "call_set_user_data", py_pjsua_call_set_user_data, METH_VARARGS,
        pjsua_call_set_user_data_doc
    },
    {
        "call_get_user_data", py_pjsua_call_get_user_data, METH_VARARGS,
        pjsua_call_get_user_data_doc
    },
    {
        "call_answer", py_pjsua_call_answer, METH_VARARGS,
        pjsua_call_answer_doc
    },
    {
        "call_hangup", py_pjsua_call_hangup, METH_VARARGS,
        pjsua_call_hangup_doc
    },
    {
        "call_set_hold", py_pjsua_call_set_hold, METH_VARARGS,
        pjsua_call_set_hold_doc
    },
    {
        "call_reinvite", py_pjsua_call_reinvite, METH_VARARGS,
        pjsua_call_reinvite_doc
    },
    {
        "call_xfer", py_pjsua_call_xfer, METH_VARARGS,
        pjsua_call_xfer_doc
    },
    {
        "call_xfer_replaces", py_pjsua_call_xfer_replaces, METH_VARARGS,
        pjsua_call_xfer_replaces_doc
    },
    {
        "call_dial_dtmf", py_pjsua_call_dial_dtmf, METH_VARARGS,
        pjsua_call_dial_dtmf_doc
    },
    {
        "call_send_im", py_pjsua_call_send_im, METH_VARARGS,
        pjsua_call_send_im_doc
    },
    {
        "call_send_typing_ind", py_pjsua_call_send_typing_ind, METH_VARARGS,
        pjsua_call_send_typing_ind_doc
    },
    {
        "call_hangup_all", py_pjsua_call_hangup_all, METH_VARARGS,
        pjsua_call_hangup_all_doc
    },
    {
        "call_dump", py_pjsua_call_dump, METH_VARARGS,
        pjsua_call_dump_doc
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
    
    if (PyType_Ready(&transport_info_Type) < 0)
        return;
    
    pjsip_transport_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjsip_transport_Type) < 0)
        return;

    /* END OF LIB TRANSPORT */

    /* LIB ACCOUNT */

    
    if (PyType_Ready(&acc_config_Type) < 0)
        return;
    if (PyType_Ready(&acc_info_Type) < 0)
        return;

    /* END OF LIB ACCOUNT */

    /* LIB BUDDY */

    if (PyType_Ready(&buddy_config_Type) < 0)
        return;
    if (PyType_Ready(&buddy_info_Type) < 0)
        return;

    /* END OF LIB BUDDY */

    /* LIB MEDIA */
  
    if (PyType_Ready(&codec_info_Type) < 0)
        return;

    if (PyType_Ready(&conf_port_info_Type) < 0)
        return;

    pjmedia_port_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjmedia_port_Type) < 0)
        return;

    if (PyType_Ready(&pjmedia_snd_dev_info_Type) < 0)
        return;

    pjmedia_codec_param_info_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjmedia_codec_param_info_Type) < 0)
        return;
    pjmedia_codec_param_setting_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pjmedia_codec_param_setting_Type) < 0)
        return;

    if (PyType_Ready(&pjmedia_codec_param_Type) < 0)
        return;

    /* END OF LIB MEDIA */

    /* LIB CALL */

    pj_time_val_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pj_time_val_Type) < 0)
        return;

    if (PyType_Ready(&call_info_Type) < 0)
        return;

    /* END OF LIB CALL */

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
    
    Py_INCREF(&transport_info_Type);
    PyModule_AddObject(m, "Transport_Info", (PyObject *)&transport_info_Type);
    
    Py_INCREF(&pjsip_transport_Type);
    PyModule_AddObject(m, "PJSIP_Transport", (PyObject *)&pjsip_transport_Type);

    /* END OF LIB TRANSPORT */

    /* LIB ACCOUNT */

    
    Py_INCREF(&acc_config_Type);
    PyModule_AddObject(m, "Acc_Config", (PyObject *)&acc_config_Type);
    Py_INCREF(&acc_info_Type);
    PyModule_AddObject(m, "Acc_Info", (PyObject *)&acc_info_Type);

    /* END OF LIB ACCOUNT */

    /* LIB BUDDY */
    
    Py_INCREF(&buddy_config_Type);
    PyModule_AddObject(m, "Buddy_Config", (PyObject *)&buddy_config_Type);
    Py_INCREF(&buddy_info_Type);
    PyModule_AddObject(m, "Buddy_Info", (PyObject *)&buddy_info_Type);

    /* END OF LIB BUDDY */

    /* LIB MEDIA */

    Py_INCREF(&codec_info_Type);
    PyModule_AddObject(m, "Codec_Info", (PyObject *)&codec_info_Type);
    Py_INCREF(&conf_port_info_Type);
    PyModule_AddObject(m, "Conf_Port_Info", (PyObject *)&conf_port_info_Type);
    Py_INCREF(&pjmedia_port_Type);
    PyModule_AddObject(m, "PJMedia_Port", (PyObject *)&pjmedia_port_Type);
    Py_INCREF(&pjmedia_snd_dev_info_Type);
    PyModule_AddObject(m, "PJMedia_Snd_Dev_Info", 
	(PyObject *)&pjmedia_snd_dev_info_Type);
    Py_INCREF(&pjmedia_codec_param_info_Type);
    PyModule_AddObject(m, "PJMedia_Codec_Param_Info", 
	(PyObject *)&pjmedia_codec_param_info_Type);
    Py_INCREF(&pjmedia_codec_param_setting_Type);
    PyModule_AddObject(m, "PJMedia_Codec_Param_Setting", 
	(PyObject *)&pjmedia_codec_param_setting_Type);
    Py_INCREF(&pjmedia_codec_param_Type);
    PyModule_AddObject(m, "PJMedia_Codec_Param", 
	(PyObject *)&pjmedia_codec_param_Type);

    /* END OF LIB MEDIA */

    /* LIB CALL */

    Py_INCREF(&pj_time_val_Type);
    PyModule_AddObject(m, "PJ_Time_Val", (PyObject *)&pj_time_val_Type);

    Py_INCREF(&call_info_Type);
    PyModule_AddObject(m, "Call_Info", (PyObject *)&call_info_Type);

    /* END OF LIB CALL */

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

#ifdef PJSUA_MAX_BUDDIES
    /*
     * Default account priority.
     */
    PyModule_AddIntConstant(
        m, "PJSUA_MAX_BUDDIES", PJSUA_MAX_BUDDIES
    );
#endif

#ifdef  PJSUA_MAX_CONF_PORTS

    /*
     * Max ports in the conference bridge.
     */
    PyModule_AddIntConstant(
        m, "PJSUA_MAX_CONF_PORTS", PJSUA_MAX_CONF_PORTS
    );

#endif

#ifdef  PJSUA_DEFAULT_CLOCK_RATE  

    PyModule_AddIntConstant(
        m, "PJSUA_DEFAULT_CLOCK_RATE", PJSUA_DEFAULT_CLOCK_RATE
    );

#endif

#ifdef  PJSUA_DEFAULT_CODEC_QUALITY  

    PyModule_AddIntConstant(
        m, "PJSUA_DEFAULT_CODEC_QUALITY", PJSUA_DEFAULT_CODEC_QUALITY
    );

#endif

#ifdef  PJSUA_DEFAULT_ILBC_MODE   

    PyModule_AddIntConstant(
        m, "PJSUA_DEFAULT_ILBC_MODE", PJSUA_DEFAULT_ILBC_MODE
    );

#endif

#ifdef  PJSUA_DEFAULT_EC_TAIL_LEN  

    PyModule_AddIntConstant(
        m, "PJSUA_DEFAULT_EC_TAIL_LEN", PJSUA_DEFAULT_EC_TAIL_LEN
    );

#endif

#ifdef  PJSUA_MAX_CALLS  

    PyModule_AddIntConstant(
        m, "PJSUA_MAX_CALLS", PJSUA_MAX_CALLS
    );

#endif

#ifdef  PJSUA_XFER_NO_REQUIRE_REPLACES

    PyModule_AddIntConstant(
	m, "PJSUA_XFER_NO_REQUIRE_REPLACES", PJSUA_XFER_NO_REQUIRE_REPLACES
    );
#endif

}
