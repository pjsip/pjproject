#include <Python.h>
#include <pjsua-lib/pjsua.h>


static PyObject *py_pjsua_perror(PyObject *pSelf, PyObject *pArgs) {
	const char *sender;
	const char *title;
	pj_status_t status;
	if (!PyArg_ParseTuple(pArgs, "ssi", &sender, &title, &status)) {
		return NULL;
	}
	pjsua_perror(sender, title, status);
	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject *py_pjsua_create(PyObject *pSelf, PyObject *pArgs) {	
	pj_status_t status;
	if (!PyArg_ParseTuple(pArgs, "")) {
		return NULL;
	}
	status = pjsua_create();
	printf("status %d\n",status);
	return Py_BuildValue("i",status);
}
static PyObject *py_pjsua_start(PyObject *pSelf, PyObject *pArgs) {	
	pj_status_t status;
	if (!PyArg_ParseTuple(pArgs, "")) {
		return NULL;
	}
	status = pjsua_start();
	printf("status %d\n",status);
	return Py_BuildValue("i",status);
}

static PyObject *py_pjsua_destroy(PyObject *pSelf, PyObject *pArgs) {	
	pj_status_t status;
	if (!PyArg_ParseTuple(pArgs, "")) {
		return NULL;
	}
	status = pjsua_destroy();	
	printf("status %d\n",status);
	return Py_BuildValue("i",status);
}
static PyObject *py_pjsua_handle_events(PyObject *pSelf, PyObject *pArgs) {	
	int ret;
	unsigned msec;
	if (!PyArg_ParseTuple(pArgs, "i", &msec)) {
		return NULL;
	}
	ret = pjsua_handle_events(msec);
	printf("return %d\n",ret);
	return Py_BuildValue("i",ret);
}
static PyObject *py_pjsua_verify_sip_url(PyObject *pSelf, PyObject *pArgs) {	
	pj_status_t status;
	const char *url;
	if (!PyArg_ParseTuple(pArgs, "s", &url)) {
		return NULL;
	}
	status = pjsua_verify_sip_url(url);
	printf("status %d\n",status);
	return Py_BuildValue("i",status);
}
/* doc string */
static char pjsua_perror_doc[] = "Display error message for the specified error code";
static char pjsua_create_doc[] = "Instantiate pjsua application";
static char pjsua_start_doc[] = "Application is recommended to call this function after all initialization is done, so that the library can do additional checking set up additional";
static char pjsua_destroy_doc[] = "Destroy pjsua";
static char pjsua_handle_events_doc[] = "Poll pjsua for events, and if necessary block the caller thread for the specified maximum interval (in miliseconds)";
static char pjsua_verify_sip_url_doc[] = "Verify that valid SIP url is given";

/* Map of function names to functions */
static PyMethodDef py_pjsua_methods[] = {
	{"perror", py_pjsua_perror, METH_VARARGS, pjsua_perror_doc},
	{"create", py_pjsua_create, METH_VARARGS, pjsua_create_doc},
	{"start", py_pjsua_start, METH_VARARGS, pjsua_start_doc},
	{"destroy", py_pjsua_destroy, METH_VARARGS, pjsua_destroy_doc},	
	{"handle_events", py_pjsua_handle_events, METH_VARARGS, pjsua_handle_events_doc},
	{"verify_sip_url", py_pjsua_verify_sip_url, METH_VARARGS, pjsua_verify_sip_url_doc},
	{NULL, NULL} /* End of functions */
};


PyMODINIT_FUNC
initpy_pjsua(void)
{
	Py_InitModule("py_pjsua", py_pjsua_methods);
}
