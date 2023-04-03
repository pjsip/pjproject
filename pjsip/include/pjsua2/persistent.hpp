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
#ifndef __PJSUA2_PERSISTENT_HPP__
#define __PJSUA2_PERSISTENT_HPP__

/**
 * @file pjsua2/persistent.hpp
 * @brief PJSUA2 Persistent Services
 */
#include <pjsua2/types.hpp>

#include <string>
#include <vector>

/** PJSUA2 API is inside pj namespace */
namespace pj
{

/**
 * @defgroup PJSUA2_PERSISTENT Persistent API
 * @ingroup PJSUA2_Ref
 * @{
 * The persistent API provides functionality to read/write data from/to
 * a document (string or file). The data can be simple data types such
 * as boolean, number, string, and string arrays, or a user defined object.
 * Currently the implementation supports reading and writing from/to JSON
 * document, but the framework allows application to extend the API to
 * support other document formats.
 */

using std::string;
using std::vector;

/* Forward declaration for ContainerNode */
class ContainerNode;

/**
 * This is the abstract base class of objects that can be serialized to/from
 * persistent document.
 */
class PersistentObject
{
public:
    /**
     * Virtual destructor
     */
    virtual ~PersistentObject()
    {}

    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error) = 0;

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error) = 0;
};


/**
 * This a the abstract base class for a persistent document. A document
 * is created either by loading from a string or a file, or by constructing
 * it manually when writing data to it. The document then can be saved
 * to either string or to a file. A document contains one root ContainerNode
 * where all data are stored under.
 *
 * Document is read and written serially, hence the order of reading must be
 * the same as the order of writing. The PersistentDocument class provides
 * API to read and write to the root node, but for more flexible operations
 * application can use the ContainerNode methods instead. Indeed the read
 * and write API in PersistentDocument is just a shorthand which calls the
 * relevant methods in the ContainerNode. As a tip, normally application only
 * uses the readObject() and writeObject() methods declared here to read/write
 * top level objects, and use the macros that are explained in ContainerNode
 * documentation to read/write more detailed data.
 */
class PersistentDocument
{
public:
    /**
     * Virtual destructor
     */
    virtual ~PersistentDocument()
    {}

    /**
     * Load this document from a file.
     *
     * @param filename  The file name.
     */
    virtual void        loadFile(const string &filename)
                                 PJSUA2_THROW(Error) = 0;

    /**
     * Load this document from string.
     *
     * @param input     The string.
     */
    virtual void        loadString(const string &input)
                                   PJSUA2_THROW(Error) = 0;

    /**
     * Write this document to a file.
     *
     * @param filename  The file name.
     */
    virtual void        saveFile(const string &filename)
                                 PJSUA2_THROW(Error) = 0;

    /**
     * Write this document to string.
     *
     * @return          The string document.
     */
    virtual string      saveString() PJSUA2_THROW(Error) = 0;

    /**
     * Get the root container node for this document
     *
     * @return          The root node.
     */
    virtual ContainerNode & getRootContainer() const = 0;


    /*
     * Shorthand functions for reading and writing from/to the root container
     */


    /**
     * Determine if there is unread element. If yes, then app can use one of
     * the readXxx() functions to read it.
     *
     * @return          True if there is.
     */
    bool                hasUnread() const;

    /**
     * Get the name of the next unread element. It will throw Error if there
     * is no more element to read.
     *
     * @return          The name of the next element .
     */
    string              unreadName() const PJSUA2_THROW(Error);

    /**
     * Read an integer value from the document and return the value.
     * This will throw Error if the current element is not a number.
     * The read position will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    int                 readInt(const string &name="") const
                                PJSUA2_THROW(Error);

    /**
     * Read a float value from the document and return the value.
     * This will throw Error if the current element is not a number.
     * The read position will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    float               readNumber(const string &name="") const
                                   PJSUA2_THROW(Error);

    /**
     * Read a boolean value from the container and return the value.
     * This will throw Error if the current element is not a boolean.
     * The read position will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    bool                readBool(const string &name="") const
                                 PJSUA2_THROW(Error);

    /**
     * Read a string value from the container and return the value.
     * This will throw Error if the current element is not a string.
     * The read position will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    string              readString(const string &name="") const
                                   PJSUA2_THROW(Error);

    /**
     * Read a string array from the container. This will throw Error
     * if the current element is not a string array. The read position
     * will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    StringVector        readStringVector(const string &name="") const
                                         PJSUA2_THROW(Error);

    /**
     * Read the specified object from the container. This is equal to
     * calling PersistentObject.readObject(ContainerNode);
     *
     * @param obj       The object to read.
     */
    void                readObject(PersistentObject &obj) const
                                   PJSUA2_THROW(Error);

    /**
     * Read a container from the container. This will throw Error if the
     * current element is not an object. The read position will be advanced
     * to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          Container object.
     */
    ContainerNode       readContainer(const string &name="") const
                                      PJSUA2_THROW(Error);

    /**
     * Read array container from the container. This will throw Error if the
     * current element is not an array. The read position will be advanced
     * to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          Container object.
     */
    ContainerNode       readArray(const string &name="") const
                                  PJSUA2_THROW(Error);

    /**
     * Write a number value to the container.
     *
     * @param name      The name for the value in the container.
     * @param num       The value to be written.
     */
    void                writeNumber(const string &name,
                                    float num) PJSUA2_THROW(Error);

    /**
     * Write a number value to the container.
     *
     * @param name      The name for the value in the container.
     * @param num       The value to be written.
     */
    void                writeInt(const string &name,
                                 int num) PJSUA2_THROW(Error);

    /**
     * Write a boolean value to the container.
     *
     * @param name      The name for the value in the container.
     * @param value     The value to be written.
     */
    void                writeBool(const string &name,
                                  bool value) PJSUA2_THROW(Error);

    /**
     * Write a string value to the container.
     *
     * @param name      The name for the value in the container.
     * @param value     The value to be written.
     */
    void                writeString(const string &name,
                                    const string &value) PJSUA2_THROW(Error);

    /**
     * Write string vector to the container.
     *
     * @param name      The name for the value in the container.
     * @param arr       The vector to be written.
     */
    void                writeStringVector(const string &name,
                                          const StringVector &arr)
                                          PJSUA2_THROW(Error);

    /**
     * Write an object to the container. This is equal to calling
     * PersistentObject.writeObject(ContainerNode);
     *
     * @param obj       The object to be written
     */
    void                writeObject(const PersistentObject &obj)
                                    PJSUA2_THROW(Error);

    /**
     * Create and write an empty Object node that can be used as parent
     * for subsequent write operations.
     *
     * @param name      The name for the new container in the container.
     *
     * @return          A sub-container.
     */
    ContainerNode       writeNewContainer(const string &name)
                                          PJSUA2_THROW(Error);

    /**
     * Create and write an empty array node that can be used as parent
     * for subsequent write operations.
     *
     * @param name      The name for the array.
     *
     * @return          A sub-container.
     */
    ContainerNode       writeNewArray(const string &name)
                                      PJSUA2_THROW(Error);
};


/**
 * Forward declaration of container_node_op.
 */
struct container_node_op;


/**
 * Internal data for ContainerNode. See ContainerNode implementation notes
 * for more info.
 */
struct container_node_internal_data
{
    void        *doc;           /**< The document.      */
    void        *data1;         /**< Internal data 1    */
    void        *data2;         /**< Internal data 2    */
};

/**
 * A container node is a placeholder for storing other data elements, which
 * could be boolean, number, string, array of strings, or another container.
 * Each data in the container is basically a name/value pair, with a type
 * internally associated with it so that written data can be read in the
 * correct type. Data is read and written serially, hence the order of
 * reading must be the same as the order of writing.
 *
 * Application can read data from it by using the various read methods, and
 * write data to it using the various write methods. Alternatively, it
 * may be more convenient to use the provided macros below to read and write
 * the data, because these macros set the name automatically:
 *      - NODE_READ_BOOL(node,item)
 *      - NODE_READ_UNSIGNED(node,item)
 *      - NODE_READ_INT(node,item)
 *      - NODE_READ_FLOAT(node,item)
 *      - NODE_READ_NUM_T(node,type,item)
 *      - NODE_READ_STRING(node,item)
 *      - NODE_READ_STRINGV(node,item)
 *      - NODE_READ_OBJ(node,item)
 *      - NODE_WRITE_BOOL(node,item)
 *      - NODE_WRITE_UNSIGNED(node,item)
 *      - NODE_WRITE_INT(node,item)
 *      - NODE_WRITE_FLOAT(node,item)
 *      - NODE_WRITE_NUM_T(node,type,item)
 *      - NODE_WRITE_STRING(node,item)
 *      - NODE_WRITE_STRINGV(node,item)
 *      - NODE_WRITE_OBJ(node,item)
 *
 * Implementation notes:
 *
 * The ContainerNode class is subclass-able, but not in the usual C++ way.
 * With the usual C++ inheritance, some methods will be made pure virtual
 * and must be implemented by the actual class. However, doing so will
 * require dynamic instantiation of the ContainerNode class, which means
 * we will need to pass around the class as pointer, for example as the
 * return value of readContainer() and writeNewContainer() methods. Then
 * we will need to establish who needs or how to delete these objects, or
 * use shared pointer mechanism, each of which is considered too inconvenient
 * or complicated for the purpose.
 *
 * So hence we use C style "inheritance", where the methods are declared in
 * container_node_op and the data in container_node_internal_data structures.
 * An implementation of ContainerNode class will need to set up these members
 * with values that makes sense to itself. The methods in container_node_op
 * contains the pointer to the actual implementation of the operation, which
 * would be specific according to the format of the document. The methods in
 * this ContainerNode class are just thin wrappers which call the
 * implementation in the container_node_op structure.
 *
 */
class ContainerNode
{
public:
    /**
     * Determine if there is unread element. If yes, then app can use one of
     * the readXxx() functions to read it.
     */
    bool                hasUnread() const;

    /**
     * Get the name of the next unread element.
     */
    string              unreadName() const PJSUA2_THROW(Error);

    /**
     * Read an integer value from the document and return the value.
     * This will throw Error if the current element is not a number.
     * The read position will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    int                 readInt(const string &name="") const
                                PJSUA2_THROW(Error);

    /**
     * Read a number value from the document and return the value.
     * This will throw Error if the current element is not a number.
     * The read position will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    float               readNumber(const string &name="") const
                                   PJSUA2_THROW(Error);

    /**
     * Read a boolean value from the container and return the value.
     * This will throw Error if the current element is not a boolean.
     * The read position will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    bool                readBool(const string &name="") const
                                 PJSUA2_THROW(Error);

    /**
     * Read a string value from the container and return the value.
     * This will throw Error if the current element is not a string.
     * The read position will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    string              readString(const string &name="") const
                                   PJSUA2_THROW(Error);

    /**
     * Read a string array from the container. This will throw Error
     * if the current element is not a string array. The read position
     * will be advanced to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          The value.
     */
    StringVector        readStringVector(const string &name="") const
                                         PJSUA2_THROW(Error);

    /**
     * Read the specified object from the container. This is equal to
     * calling PersistentObject.readObject(ContainerNode);
     *
     * @param obj       The object to read.
     */
    void                readObject(PersistentObject &obj) const
                                   PJSUA2_THROW(Error);

    /**
     * Read a container from the container. This will throw Error if the
     * current element is not a container. The read position will be advanced
     * to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          Container object.
     */
    ContainerNode       readContainer(const string &name="") const
                                      PJSUA2_THROW(Error);

    /**
     * Read array container from the container. This will throw Error if the
     * current element is not an array. The read position will be advanced
     * to the next element.
     *
     * @param name      If specified, then the function will check if the
     *                  name of the next element matches the specified
     *                  name and throw Error if it doesn't match.
     *
     * @return          Container object.
     */
    ContainerNode       readArray(const string &name="") const
                                  PJSUA2_THROW(Error);

    /**
     * Write a number value to the container.
     *
     * @param name      The name for the value in the container.
     * @param num       The value to be written.
     */
    void                writeNumber(const string &name,
                                    float num) PJSUA2_THROW(Error);

    /**
     * Write a number value to the container.
     *
     * @param name      The name for the value in the container.
     * @param num       The value to be written.
     */
    void                writeInt(const string &name,
                                 int num) PJSUA2_THROW(Error);

    /**
     * Write a boolean value to the container.
     *
     * @param name      The name for the value in the container.
     * @param value     The value to be written.
     */
    void                writeBool(const string &name,
                                  bool value) PJSUA2_THROW(Error);

    /**
     * Write a string value to the container.
     *
     * @param name      The name for the value in the container.
     * @param value     The value to be written.
     */
    void                writeString(const string &name,
                                    const string &value) PJSUA2_THROW(Error);

    /**
     * Write string vector to the container.
     *
     * @param name      The name for the value in the container.
     * @param arr       The vector to be written.
     */
    void                writeStringVector(const string &name,
                                          const StringVector &arr)
                                          PJSUA2_THROW(Error);

    /**
     * Write an object to the container. This is equal to calling
     * PersistentObject.writeObject(ContainerNode);
     *
     * @param obj       The object to be written
     */
    void                writeObject(const PersistentObject &obj)
                                    PJSUA2_THROW(Error);

    /**
     * Create and write an empty Object node that can be used as parent
     * for subsequent write operations.
     *
     * @param name      The name for the new container in the container.
     *
     * @return          A sub-container.
     */
    ContainerNode       writeNewContainer(const string &name)
                                          PJSUA2_THROW(Error);

    /**
     * Create and write an empty array node that can be used as parent
     * for subsequent write operations.
     *
     * @param name      The name for the array.
     *
     * @return          A sub-container.
     */
    ContainerNode       writeNewArray(const string &name)
                                      PJSUA2_THROW(Error);

public:
    /* internal data */
    container_node_op *op;              /**< Method table.      */
    container_node_internal_data data;  /**< Internal data      */

    ContainerNode()
    : op(NULL)
    {
        pj_bzero(&data, sizeof(data));
    }
};


/**
 * Pointer to actual ContainerNode implementation. See ContainerNode
 * implementation notes for more info.
 */
//! @cond Doxygen_Suppress
struct container_node_op
{
    bool                (*hasUnread)(const ContainerNode*);
    string              (*unreadName)(const ContainerNode*)
                                      PJSUA2_THROW(Error);
    float               (*readNumber)(const ContainerNode*,
                                      const string&)
                                      PJSUA2_THROW(Error);
    bool                (*readBool)(const ContainerNode*,
                                    const string&)
                                    PJSUA2_THROW(Error);
    string              (*readString)(const ContainerNode*,
                                      const string&)
                                      PJSUA2_THROW(Error);
    StringVector        (*readStringVector)(const ContainerNode*,
                                            const string&)
                                            PJSUA2_THROW(Error);
    ContainerNode       (*readContainer)(const ContainerNode*,
                                         const string &)
                                         PJSUA2_THROW(Error);
    ContainerNode       (*readArray)(const ContainerNode*,
                                     const string &)
                                     PJSUA2_THROW(Error);
    void                (*writeNumber)(ContainerNode*,
                                       const string &name,
                                       float num)
                                       PJSUA2_THROW(Error);
    void                (*writeBool)(ContainerNode*,
                                     const string &name,
                                     bool value)
                                     PJSUA2_THROW(Error);
    void                (*writeString)(ContainerNode*,
                                       const string &name,
                                       const string &value)
                                       PJSUA2_THROW(Error);
    void                (*writeStringVector)(ContainerNode*,
                                             const string &name,
                                             const StringVector &value)
                                             PJSUA2_THROW(Error);
    ContainerNode       (*writeNewContainer)(ContainerNode*,
                                             const string &name)
                                             PJSUA2_THROW(Error);
    ContainerNode       (*writeNewArray)(ContainerNode*,
                                         const string &name)
                                         PJSUA2_THROW(Error);
};

/*
 * Convenient macros.
 */
#define NODE_READ_BOOL(node,item)       item = node.readBool(#item)
#define NODE_READ_UNSIGNED(node,item)   item = (unsigned)node.readNumber(#item)
#define NODE_READ_INT(node,item)        item = (int) node.readNumber(#item)
#define NODE_READ_FLOAT(node,item)      item = node.readNumber(#item)
#define NODE_READ_NUM_T(node,T,item)    item = (T)(int)node.readNumber(#item)
#define NODE_READ_STRING(node,item)     item = node.readString(#item)
#define NODE_READ_STRINGV(node,item)    item = node.readStringVector(#item)
#define NODE_READ_OBJ(node,item)        node.readObject(item)

#define NODE_WRITE_BOOL(node,item)      node.writeBool(#item, item)
#define NODE_WRITE_UNSIGNED(node,item)  node.writeNumber(#item, (float)item)
#define NODE_WRITE_INT(node,item)       node.writeNumber(#item, (float)item)
#define NODE_WRITE_NUM_T(node,T,item)   node.writeNumber(#item, (float)item)
#define NODE_WRITE_FLOAT(node,item)     node.writeNumber(#item, item)
#define NODE_WRITE_STRING(node,item)    node.writeString(#item, item)
#define NODE_WRITE_STRINGV(node,item)   node.writeStringVector(#item, item)
#define NODE_WRITE_OBJ(node,item)       node.writeObject(item)

//! @endcond

/**
 * @}  PJSUA2
 */

} // namespace pj



#endif  /* __PJSUA2_PERSISTENT_HPP__ */
