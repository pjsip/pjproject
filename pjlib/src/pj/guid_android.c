/* 
 * Copyright (C) 2015-2016 Teluu Inc. (http://www.teluu.com)
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
/* This original code was kindly contributed by Johan Lantz.
 */
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>

#include <jni.h>

PJ_DEF_DATA(const unsigned) PJ_GUID_STRING_LENGTH=36;

PJ_DEF(unsigned) pj_GUID_STRING_LENGTH()
{
    return PJ_GUID_STRING_LENGTH;
}

PJ_DEF(pj_str_t*) pj_generate_unique_string(pj_str_t *str)
{
    jclass uuid_class;
    jmethodID get_uuid_method;
    jmethodID to_string_method;
    JNIEnv *jni_env = 0;
    jobject javaUuid;
    jstring uuid_string;
    const char *native_string;
    pj_str_t native_str;

    pj_bool_t attached = pj_jni_attach_jvm((void **)&jni_env);
    if (!jni_env)
        goto on_error;

    uuid_class = (*jni_env)->FindClass(jni_env, "java/util/UUID");

    if (uuid_class == 0)
        goto on_error;

    get_uuid_method = (*jni_env)->GetStaticMethodID(jni_env, uuid_class,
                      "randomUUID",
                      "()Ljava/util/UUID;");
    if (get_uuid_method == 0)
        goto on_error;

    javaUuid = (*jni_env)->CallStaticObjectMethod(jni_env, uuid_class, 
                                                  get_uuid_method);
    if (javaUuid == 0)
        goto on_error;

    to_string_method = (*jni_env)->GetMethodID(jni_env, uuid_class,
                                                "toString",
                                                "()Ljava/lang/String;");
    if (to_string_method == 0)
        goto on_error;

    uuid_string = (*jni_env)->CallObjectMethod(jni_env, javaUuid,
                                               to_string_method);
    if (uuid_string == 0)
        goto on_error;

    native_string = (*jni_env)->GetStringUTFChars(jni_env, uuid_string,
                                                  JNI_FALSE);
    if (native_string == 0)
        goto on_error;

    native_str.ptr = (char *)native_string;
    native_str.slen = pj_ansi_strlen(native_string);
    pj_strncpy(str, &native_str, PJ_GUID_STRING_LENGTH);

    (*jni_env)->ReleaseStringUTFChars(jni_env, uuid_string, native_string);
    (*jni_env)->DeleteLocalRef(jni_env, javaUuid);
    (*jni_env)->DeleteLocalRef(jni_env, uuid_class);
    (*jni_env)->DeleteLocalRef(jni_env, uuid_string);
    pj_jni_detach_jvm(attached);

    return str;

on_error:
    PJ_LOG(2, ("guid_android.c", ("Error generating UUID")));
    pj_jni_detach_jvm(attached);
    return NULL;
}
