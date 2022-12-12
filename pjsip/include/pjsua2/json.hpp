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
#ifndef __PJSUA2_JSON_HPP__
#define __PJSUA2_JSON_HPP__

/**
 * @file pjsua2/persistent.hpp
 * @brief PJSUA2 Persistent Services
 */
#include <pjsua2/persistent.hpp>
#include <pjlib-util/json.h>
#include <pj/pool.h>
#include <string>

/** PJSUA2 API is inside pj namespace */
namespace pj
{

/**
 * @defgroup PJSUA2_JSON JSON Persistent Support
 * @ingroup PJSUA2_PERSISTENT
 * @{
 * Provides object serialization and deserialization to/from JSON document.
 */

using std::string;

/**
 * Persistent document (file) with JSON format.
 */
class JsonDocument : public PersistentDocument
{
public:
    /** Default constructor */
    JsonDocument();

    /** Destructor */
    ~JsonDocument();

    /**
     * Load this document from a file.
     *
     * @param filename          The file name.
     */
    virtual void   loadFile(const string &filename) PJSUA2_THROW(Error);

    /**
     * Load this document from string.
     *
     * @param input             The string.
     */
    virtual void   loadString(const string &input) PJSUA2_THROW(Error);

    /**
     * Write this document to a file.
     *
     * @param filename          The file name.
     */
    virtual void   saveFile(const string &filename) PJSUA2_THROW(Error);

    /**
     * Write this document to string.
     */
    virtual string saveString() PJSUA2_THROW(Error);

    /**
     * Get the root container node for this document
     */
    virtual ContainerNode & getRootContainer() const;

    /**
     * An internal function to create JSON element.
     */
    pj_json_elem*    allocElement() const;

    /**
     * An internal function to get the pool.
     */
    pj_pool_t*       getPool();

private:
    pj_caching_pool       cp;
    mutable ContainerNode rootNode;
    mutable pj_json_elem *root;
    mutable pj_pool_t    *pool;

    void initRoot() const;
};




/**
 * @}  PJSUA2
 */

} // namespace pj


#endif  /* __PJSUA2_JSON_HPP__ */
