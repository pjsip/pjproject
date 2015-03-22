/* $Id: pjsua_app_callback.h $ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSUA_APP_CALLBACK_H__
#define __PJSUA_APP_CALLBACK_H__

#include <jni.h>

class PjsuaAppCallback {
public:
    virtual ~PjsuaAppCallback() {}
    virtual void onStarted(const char *msg) {}
    virtual void onStopped(int restart) {}
};

extern "C" {
int pjsuaStart();
void pjsuaDestroy();
int pjsuaRestart();
void setCallbackObject(PjsuaAppCallback* callback);
void setIncomingVideoRenderer(jobject surface);
}

#endif /* __PJSUA_APP_CALLBACK_H__ */
