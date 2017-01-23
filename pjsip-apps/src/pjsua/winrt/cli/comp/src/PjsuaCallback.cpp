/* $Id$ */
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

#include "PjsuaCallback.h"

using namespace PjsuaCLI::BackEnd;
using namespace Platform;
using namespace Windows::Foundation;

#define THIS_FILE	"PjsuaCallback.cpp"

void PjsuaCallback::SetCallback(IPjsuaCallback^ cb)
{
    this->callback = cb;
}

void PjsuaCallback::OnStarted(Platform::String ^outStr)
{
    if (callback) {
	callback->OnPjsuaStarted(outStr);
    }
}

void PjsuaCallback::OnStopped(int restart)
{
    if (callback) {
	callback->OnPjsuaStopped(restart);
    }
}

PjsuaCallback::PjsuaCallback() : callback(nullptr) {}

PjsuaCallback::~PjsuaCallback() {}

