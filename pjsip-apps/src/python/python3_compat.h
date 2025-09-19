/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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


#ifndef __PY_PYTHON_3_COMPAT__
#define __PY_PYTHON_3_COMPAT__

#if PY_MAJOR_VERSION > 2

#define PY_SSIZE_T_CLEAN

#define PyEval_InitThreads                   	Py_Initialize
#define Py_InitModule3( name, methods, doc ) 	( PyModule_Create( &py_pjsua_module ) )

#if PY_MINOR_VERSION >= 10
#define DL_EXPORT( x )                       	PyObject*
#define init_pjsua( x ) 			PyInit__pjsua( x )
#define INIT_RETURN				( m )
#else
#define DL_EXPORT( x )                       	void
#define INIT_RETURN
#endif

#define PyString_FromStringAndSize( x, y ) PyUnicode_FromStringAndSize( x, y )
#define PyString_Check( x )                PyUnicode_Check( x )
#define PyString_FromString( x )           PyUnicode_FromString( x )
#define PyString_AsString( x )             ( ( char * )PyUnicode_AsUTF8( x ) )
#define PyString_AS_STRING( x )            ( ( char * )PyUnicode_AsUTF8( x ) )
#define PyString_GET_SIZE( x )             PyUnicode_GetLength( x )
#define PyString_Size( x )                 PyUnicode_GetLength( x )

#define PyInt_AsLong( x )                  PyLong_AsLong( x )

#endif // PY_MAJOR_VERSION > 2

#endif // __PY_PYTHON_3_COMPAT__