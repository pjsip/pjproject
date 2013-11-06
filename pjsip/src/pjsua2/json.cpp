/* $Id$ */
/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
#include <pjsua2/json.hpp>
#include <pjlib-util/errno.h>
#include <pj/file_io.h>
#include "util.hpp"

#define THIS_FILE	"json.cpp"

using namespace pj;
using namespace std;

/* Json node operations */
static bool		jsonNode_hasUnread(const ContainerNode*);
static string		jsonNode_unreadName(const ContainerNode*n)
				            throw(Error);
static float		jsonNode_readNumber(const ContainerNode*,
            		                    const string&)
					      throw(Error);
static bool		jsonNode_readBool(const ContainerNode*,
           		                  const string&)
					  throw(Error);
static string		jsonNode_readString(const ContainerNode*,
             		                    const string&)
					    throw(Error);
static StringVector	jsonNode_readStringVector(const ContainerNode*,
                   	                          const string&)
						  throw(Error);
static ContainerNode	jsonNode_readContainer(const ContainerNode*,
                    	                       const string &)
					       throw(Error);
static ContainerNode	jsonNode_readArray(const ContainerNode*,
                    	                   const string &)
					   throw(Error);
static void		jsonNode_writeNumber(ContainerNode*,
           		                     const string &name,
           		                     float num)
           		                     throw(Error);
static void		jsonNode_writeBool(ContainerNode*,
           		                   const string &name,
           		                   bool value)
					   throw(Error);
static void		jsonNode_writeString(ContainerNode*,
           		                     const string &name,
           		                     const string &value)
					     throw(Error);
static void		jsonNode_writeStringVector(ContainerNode*,
           		                           const string &name,
           		                           const StringVector &value)
					           throw(Error);
static ContainerNode 	jsonNode_writeNewContainer(ContainerNode*,
                     	                           const string &name)
					           throw(Error);
static ContainerNode 	jsonNode_writeNewArray(ContainerNode*,
                     	                       const string &name)
					       throw(Error);

static container_node_op json_op =
{
    &jsonNode_hasUnread,
    &jsonNode_unreadName,
    &jsonNode_readNumber,
    &jsonNode_readBool,
    &jsonNode_readString,
    &jsonNode_readStringVector,
    &jsonNode_readContainer,
    &jsonNode_readArray,
    &jsonNode_writeNumber,
    &jsonNode_writeBool,
    &jsonNode_writeString,
    &jsonNode_writeStringVector,
    &jsonNode_writeNewContainer,
    &jsonNode_writeNewArray
};

///////////////////////////////////////////////////////////////////////////////
JsonDocument::JsonDocument()
: root(NULL)
{
    pj_caching_pool_init(&cp, NULL, 0);
    pool = pj_pool_create(&cp.factory, "jsondoc", 512, 512, NULL);
    if (!pool)
	PJSUA2_RAISE_ERROR(PJ_ENOMEM);
}

JsonDocument::~JsonDocument()
{
    if (pool)
	pj_pool_release(pool);
    pj_caching_pool_destroy(&cp);
}

void JsonDocument::initRoot() const
{
    rootNode.op = &json_op;
    rootNode.data.doc = (void*)this;
    rootNode.data.data1 = (void*)root;
    rootNode.data.data2 = root->value.children.next;
}

void JsonDocument::loadFile(const string &filename) throw(Error)
{
    if (root)
	PJSUA2_RAISE_ERROR3(PJ_EINVALIDOP, "JsonDocument.loadString()",
	                    "Document already initialized");

    if (!pj_file_exists(filename.c_str()))
	PJSUA2_RAISE_ERROR(PJ_ENOTFOUND);

    pj_ssize_t size = pj_file_size(filename.c_str());
    pj_status_t status;

    char *buffer = (char*)pj_pool_alloc(pool, size+1);
    pj_oshandle_t fd = 0;
    unsigned parse_size;
    char err_msg[120];
    pj_json_err_info err_info;

    err_msg[0] = '\0';

    status = pj_file_open(pool, filename.c_str(), PJ_O_RDONLY, &fd);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_file_read(fd, buffer, &size);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_file_close(fd);
    fd = NULL;

    if (size <= 0) {
	status = PJ_EEOF;
	goto on_error;
    }

    parse_size = (unsigned)size;
    root = pj_json_parse(pool, buffer, &parse_size, &err_info);
    if (root == NULL) {
	pj_ansi_snprintf(err_msg, sizeof(err_msg),
	                 "JSON parsing failed: syntax error in file '%s' at "
	                 "line %d column %d",
	                 filename.c_str(), err_info.line, err_info.col);
	PJ_LOG(1,(THIS_FILE, err_msg));
	status = PJLIB_UTIL_EINJSON;
	goto on_error;
    }

    initRoot();
    return;

on_error:
    if (fd)
	pj_file_close(fd);
    if (err_msg[0])
	PJSUA2_RAISE_ERROR3(status, "loadFile()", err_msg);
    else
	PJSUA2_RAISE_ERROR(status);
}

void JsonDocument::loadString(const string &input) throw(Error)
{
    if (root)
	PJSUA2_RAISE_ERROR3(PJ_EINVALIDOP, "JsonDocument.loadString()",
	                    "Document already initialized");

    unsigned size = input.size();
    char *buffer = (char*)pj_pool_alloc(pool, size+1);
    unsigned parse_size = (unsigned)size;
    pj_json_err_info err_info;

    pj_memcpy(buffer, input.c_str(), size);

    root = pj_json_parse(pool, buffer, &parse_size, &err_info);
    if (root == NULL) {
	char err_msg[80];

	pj_ansi_snprintf(err_msg, sizeof(err_msg),
	                 "JSON parsing failed at line %d column %d",
	                 err_info.line, err_info.col);
	PJ_LOG(1,(THIS_FILE, err_msg));
	PJSUA2_RAISE_ERROR3(PJLIB_UTIL_EINJSON, "loadString()", err_msg);
    }
    initRoot();
}

struct save_file_data
{
    pj_oshandle_t fd;
};

static pj_status_t json_file_writer(const char *s,
				    unsigned size,
				    void *user_data)
{
    save_file_data *sd = (save_file_data*)user_data;
    pj_ssize_t ssize = (pj_ssize_t)size;
    return pj_file_write(sd->fd, s, &ssize);
}

void JsonDocument::saveFile(const string &filename) throw(Error)
{
    struct save_file_data sd;
    pj_status_t status;

    /* Make sure root container has been created */
    getRootContainer();

    status = pj_file_open(pool, filename.c_str(), PJ_O_WRONLY, &sd.fd);
    if (status != PJ_SUCCESS)
	PJSUA2_RAISE_ERROR(status);

    status = pj_json_writef(root, &json_file_writer, &sd.fd);
    pj_file_close(sd.fd);

    if (status != PJ_SUCCESS)
	PJSUA2_RAISE_ERROR(status);
}

struct save_string_data
{
    string	output;
};

static pj_status_t json_string_writer(const char *s,
                                      unsigned size,
                                      void *user_data)
{
    save_string_data *sd = (save_string_data*)user_data;
    sd->output.append(s, size);
    return PJ_SUCCESS;
}

string JsonDocument::saveString() throw(Error)
{
    struct save_string_data sd;
    pj_status_t status;

    /* Make sure root container has been created */
    getRootContainer();

    status = pj_json_writef(root, &json_string_writer, &sd);
    if (status != PJ_SUCCESS)
	PJSUA2_RAISE_ERROR(status);

    return sd.output;
}

ContainerNode & JsonDocument::getRootContainer() const
{
    if (!root) {
	root = allocElement();
	pj_json_elem_obj(root, NULL);
	initRoot();
    }

    return rootNode;
}

pj_json_elem* JsonDocument::allocElement() const
{
    return (pj_json_elem*)pj_pool_alloc(pool, sizeof(pj_json_elem));
}

pj_pool_t *JsonDocument::getPool()
{
    return pool;
}

///////////////////////////////////////////////////////////////////////////////
struct json_node_data
{
    JsonDocument	*doc;
    pj_json_elem	*jnode;
    pj_json_elem	*childPtr;
};

static bool jsonNode_hasUnread(const ContainerNode *node)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    return jdat->childPtr != (pj_json_elem*)&jdat->jnode->value.children;
}

static void json_verify(struct json_node_data *jdat,
                        const char *op,
                        const string &name,
                        pj_json_val_type type)
{
    if (jdat->childPtr == (pj_json_elem*)&jdat->jnode->value.children)
	PJSUA2_RAISE_ERROR3(PJ_EEOF, op, "No unread element");

    /* If name is specified, then check if the names match, except
     * when the node name itself is empty and the parent node is
     * an array, then ignore the checking (JSON doesn't allow array
     * elements to have name).
     */
    if (name.size() && name.compare(0, name.size(),
                                    jdat->childPtr->name.ptr,
                                    jdat->childPtr->name.slen) &&
        jdat->childPtr->name.slen &&
        jdat->jnode->type != PJ_JSON_VAL_ARRAY)
    {
	char err_msg[80];
	pj_ansi_snprintf(err_msg, sizeof(err_msg),
	                 "Name mismatch: expecting '%s' got '%.*s'",
	                 name.c_str(), (int)jdat->childPtr->name.slen,
	                 jdat->childPtr->name.ptr);
	PJSUA2_RAISE_ERROR3(PJLIB_UTIL_EINJSON, op, err_msg);
    }

    if (type != PJ_JSON_VAL_NULL && jdat->childPtr->type != type) {
	char err_msg[80];
	pj_ansi_snprintf(err_msg, sizeof(err_msg),
	                 "Type mismatch: expecting %d got %d",
	                 type, jdat->childPtr->type);
	PJSUA2_RAISE_ERROR3(PJLIB_UTIL_EINJSON, op, err_msg);
    }
}

static string jsonNode_unreadName(const ContainerNode *node)
				  throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    json_verify(jdat, "unreadName()", "", PJ_JSON_VAL_NULL);
    return pj2Str(jdat->childPtr->name);
}

static float jsonNode_readNumber(const ContainerNode *node,
            		         const string &name)
				 throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    json_verify(jdat, "readNumber()", name, PJ_JSON_VAL_NUMBER);
    jdat->childPtr = jdat->childPtr->next;
    return jdat->childPtr->prev->value.num;
}

static bool jsonNode_readBool(const ContainerNode *node,
			      const string &name)
			      throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    json_verify(jdat, "readBool()", name, PJ_JSON_VAL_BOOL);
    jdat->childPtr = jdat->childPtr->next;
    return jdat->childPtr->prev->value.is_true;
}

static string jsonNode_readString(const ContainerNode *node,
				  const string &name)
				  throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    json_verify(jdat, "readString()", name, PJ_JSON_VAL_STRING);
    jdat->childPtr = jdat->childPtr->next;
    return pj2Str(jdat->childPtr->prev->value.str);
}

static StringVector jsonNode_readStringVector(const ContainerNode *node,
                   	                      const string &name)
					      throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    json_verify(jdat, "readStringVector()", name, PJ_JSON_VAL_ARRAY);

    StringVector result;
    pj_json_elem *child = jdat->childPtr->value.children.next;
    while (child != (pj_json_elem*)&jdat->childPtr->value.children) {
	if (child->type != PJ_JSON_VAL_STRING) {
	    char err_msg[80];
	    pj_ansi_snprintf(err_msg, sizeof(err_msg),
			     "Elements not string but type %d",
			     child->type);
	    PJSUA2_RAISE_ERROR3(PJLIB_UTIL_EINJSON, "readStringVector()",
	                        err_msg);
	}
	result.push_back(pj2Str(child->value.str));
	child = child->next;
    }

    jdat->childPtr = jdat->childPtr->next;
    return result;
}

static ContainerNode jsonNode_readContainer(const ContainerNode *node,
                    	                    const string &name)
					    throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    json_verify(jdat, "readContainer()", name, PJ_JSON_VAL_OBJ);

    ContainerNode json_node;

    json_node.op = &json_op;
    json_node.data.doc = (void*)jdat->doc;
    json_node.data.data1 = (void*)jdat->childPtr;
    json_node.data.data2 = (void*)jdat->childPtr->value.children.next;

    jdat->childPtr = jdat->childPtr->next;
    return json_node;
}

static ContainerNode jsonNode_readArray(const ContainerNode *node,
                    	                const string &name)
					throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    json_verify(jdat, "readArray()", name, PJ_JSON_VAL_ARRAY);

    ContainerNode json_node;

    json_node.op = &json_op;
    json_node.data.doc = (void*)jdat->doc;
    json_node.data.data1 = (void*)jdat->childPtr;
    json_node.data.data2 = (void*)jdat->childPtr->value.children.next;

    jdat->childPtr = jdat->childPtr->next;
    return json_node;
}

static pj_str_t alloc_name(JsonDocument *doc, const string &name)
{
    pj_str_t new_name;
    pj_strdup2(doc->getPool(), &new_name, name.c_str());
    return new_name;
}

static void jsonNode_writeNumber(ContainerNode *node,
           		         const string &name,
           		         float num)
           		         throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    pj_json_elem *el = jdat->doc->allocElement();
    pj_str_t nm = alloc_name(jdat->doc, name);
    pj_json_elem_number(el, &nm, num);
    pj_json_elem_add(jdat->jnode, el);
}

static void jsonNode_writeBool(ContainerNode *node,
           		       const string &name,
           		       bool value)
			       throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    pj_json_elem *el = jdat->doc->allocElement();
    pj_str_t nm = alloc_name(jdat->doc, name);
    pj_json_elem_bool(el, &nm, value);
    pj_json_elem_add(jdat->jnode, el);
}

static void jsonNode_writeString(ContainerNode *node,
           		         const string &name,
           		         const string &value)
				 throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    pj_json_elem *el = jdat->doc->allocElement();
    pj_str_t nm = alloc_name(jdat->doc, name);
    pj_str_t new_val;
    pj_strdup2(jdat->doc->getPool(), &new_val, value.c_str());
    pj_json_elem_string(el, &nm, &new_val);

    pj_json_elem_add(jdat->jnode, el);
}

static void jsonNode_writeStringVector(ContainerNode *node,
           		               const string &name,
           		               const StringVector &value)
				       throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    pj_json_elem *el = jdat->doc->allocElement();
    pj_str_t nm = alloc_name(jdat->doc, name);

    pj_json_elem_array(el, &nm);
    for (unsigned i=0; i<value.size(); ++i) {
	pj_str_t new_val;

	pj_strdup2(jdat->doc->getPool(), &new_val, value[i].c_str());
	pj_json_elem *child = jdat->doc->allocElement();
	pj_json_elem_string(child, NULL, &new_val);
	pj_json_elem_add(el, child);
    }

    pj_json_elem_add(jdat->jnode, el);
}

static ContainerNode jsonNode_writeNewContainer(ContainerNode *node,
                     	                        const string &name)
					        throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    pj_json_elem *el = jdat->doc->allocElement();
    pj_str_t nm = alloc_name(jdat->doc, name);

    pj_json_elem_obj(el, &nm);
    pj_json_elem_add(jdat->jnode, el);

    ContainerNode json_node;

    json_node.op = &json_op;
    json_node.data.doc = (void*)jdat->doc;
    json_node.data.data1 = (void*)el;
    json_node.data.data2 = (void*)el->value.children.next;

    return json_node;
}

static ContainerNode jsonNode_writeNewArray(ContainerNode *node,
                     	                    const string &name)
					    throw(Error)
{
    json_node_data *jdat = (json_node_data*)&node->data;
    pj_json_elem *el = jdat->doc->allocElement();
    pj_str_t nm = alloc_name(jdat->doc, name);

    pj_json_elem_array(el, &nm);
    pj_json_elem_add(jdat->jnode, el);

    ContainerNode json_node;

    json_node.op = &json_op;
    json_node.data.doc = (void*)jdat->doc;
    json_node.data.data1 = (void*)el;
    json_node.data.data2 = (void*)el->value.children.next;

    return json_node;
}
