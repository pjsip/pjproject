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
#include <pjsua2/persistent.hpp>

using namespace pj;
using namespace std;


bool PersistentDocument::hasUnread() const
{
    return getRootContainer().hasUnread();
}

string PersistentDocument::unreadName() const PJSUA2_THROW(Error)
{
    return getRootContainer().unreadName();
}

int PersistentDocument::readInt(const string &name) const PJSUA2_THROW(Error)
{
    return (int)getRootContainer().readNumber(name);
}

float PersistentDocument::readNumber(const string &name) const
				     PJSUA2_THROW(Error)
{
    return getRootContainer().readNumber(name);
}

bool PersistentDocument::readBool(const string &name) const
				  PJSUA2_THROW(Error)
{
    return getRootContainer().readBool(name);
}

string PersistentDocument::readString(const string &name) const
				      PJSUA2_THROW(Error)
{
    return getRootContainer().readString(name);
}

StringVector PersistentDocument::readStringVector(const string &name) const
						  PJSUA2_THROW(Error)
{
    return getRootContainer().readStringVector(name);
}

void PersistentDocument::readObject(PersistentObject &obj) const
				    PJSUA2_THROW(Error)
{
    getRootContainer().readObject(obj);
}

ContainerNode PersistentDocument::readContainer(const string &name) const
					        PJSUA2_THROW(Error)
{
    return getRootContainer().readContainer(name);
}

ContainerNode PersistentDocument::readArray(const string &name) const
					    PJSUA2_THROW(Error)
{
    return getRootContainer().readArray(name);
}

void PersistentDocument::writeNumber(const string &name,
				     float num) PJSUA2_THROW(Error)
{
    getRootContainer().writeNumber(name, num);
}

void PersistentDocument::writeInt(const string &name,
				  int num) PJSUA2_THROW(Error)
{
    getRootContainer().writeNumber(name, (float)num);
}

void PersistentDocument::writeBool(const string &name,
				   bool value) PJSUA2_THROW(Error)
{
    getRootContainer().writeBool(name, value);
}

void PersistentDocument::writeString(const string &name,
                                     const string &value) PJSUA2_THROW(Error)
{
    getRootContainer().writeString(name, value);
}

void PersistentDocument::writeStringVector(const string &name,
                                           const StringVector &value)
					   PJSUA2_THROW(Error)
{
    getRootContainer().writeStringVector(name, value);
}

void PersistentDocument::writeObject(const PersistentObject &obj)
				     PJSUA2_THROW(Error)
{
    getRootContainer().writeObject(obj);
}

ContainerNode PersistentDocument::writeNewContainer(const string &name)
						    PJSUA2_THROW(Error)
{
    return getRootContainer().writeNewContainer(name);
}

ContainerNode PersistentDocument::writeNewArray(const string &name)
						    PJSUA2_THROW(Error)
{
    return getRootContainer().writeNewArray(name);
}

///////////////////////////////////////////////////////////////////////////////

bool ContainerNode::hasUnread() const
{
    return op->hasUnread(this);
}

string ContainerNode::unreadName() const PJSUA2_THROW(Error)
{
    return op->unreadName(this);
}

int ContainerNode::readInt(const string &name) const PJSUA2_THROW(Error)
{
    return (int)op->readNumber(this, name);
}

float ContainerNode::readNumber(const string &name) const PJSUA2_THROW(Error)
{
    return op->readNumber(this, name);
}

bool ContainerNode::readBool(const string &name) const PJSUA2_THROW(Error)
{
    return op->readBool(this, name);
}

string ContainerNode::readString(const string &name) const PJSUA2_THROW(Error)
{
    return op->readString(this, name);
}

StringVector ContainerNode::readStringVector(const string &name) const
					     PJSUA2_THROW(Error)
{
    return op->readStringVector(this, name);
}

void ContainerNode::readObject(PersistentObject &obj) const
			       PJSUA2_THROW(Error)
{
    obj.readObject(*this);
}

ContainerNode ContainerNode::readContainer(const string &name) const
					   PJSUA2_THROW(Error)
{
    return op->readContainer(this, name);
}

ContainerNode ContainerNode::readArray(const string &name) const
					   PJSUA2_THROW(Error)
{
    return op->readArray(this, name);
}

void ContainerNode::writeNumber(const string &name,
				float num) PJSUA2_THROW(Error)
{
    return op->writeNumber(this, name, num);
}

void ContainerNode::writeInt(const string &name,
			     int num) PJSUA2_THROW(Error)
{
    return op->writeNumber(this, name, (float)num);
}

void ContainerNode::writeBool(const string &name,
			      bool value) PJSUA2_THROW(Error)
{
    return op->writeBool(this, name, value);
}

void ContainerNode::writeString(const string &name,
				const string &value) PJSUA2_THROW(Error)
{
    return op->writeString(this, name, value);
}

void ContainerNode::writeStringVector(const string &name,
				      const StringVector &value)
				      PJSUA2_THROW(Error)
{
    return op->writeStringVector(this, name, value);
}

void ContainerNode::writeObject(const PersistentObject &obj)
				PJSUA2_THROW(Error)
{
    obj.writeObject(*this);
}

ContainerNode ContainerNode::writeNewContainer(const string &name)
					       PJSUA2_THROW(Error)
{
    return op->writeNewContainer(this, name);
}

ContainerNode ContainerNode::writeNewArray(const string &name)
					   PJSUA2_THROW(Error)
{
    return op->writeNewArray(this, name);
}
