/*
 * Copyright (C) 2019-2019 Teluu Inc. (http://www.teluu.com)
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

#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

/* Certificate helpers shared by the two Apple TLS backends,
 * ssl_sock_darwin.c (Secure Transport) and ssl_sock_apple.m
 * (Network.framework). Both are built on Security.framework and
 * CoreFoundation and had byte-identical copies of everything here.
 *
 * This file is #included by each backend rather than compiled on its own,
 * the same way ssl_sock_imp_common.c is, so THIS_FILE in the log calls
 * below names the including backend.
 */

/* Certificates and PKCS#12 bundles are small. This bound is far above any
 * legitimate one and exists so that a cert_file or CA_file which is not a
 * regular file -- a FIFO with no writer, or /dev/zero -- cannot block the
 * calling thread indefinitely or grow the buffer until the process dies.
 */
#define MAX_CERT_FILE_SIZE      (1024 * 1024)

static pj_status_t create_data_from_file(CFDataRef *data,
                                         pj_str_t *fname, pj_str_t *path)
{
    CFURLRef file;
    CFReadStreamRef read_stream;
    CFMutableDataRef buf;
    UInt8 data_buf[8192];
    CFIndex nbytes = 0;
    pj_bool_t too_big = PJ_FALSE;

    if (path) {
        CFURLRef filepath;
        CFStringRef path_str;

        path_str = CFStringCreateWithBytes(NULL, (const UInt8 *)path->ptr,
                                           path->slen,
                                           kCFStringEncodingUTF8, false);
        if (!path_str) return PJ_ENOMEM;

        filepath = CFURLCreateWithFileSystemPath(NULL, path_str,
                                                 kCFURLPOSIXPathStyle, true);
        CFRelease(path_str);
        if (!filepath) return PJ_ENOMEM;

        path_str = CFStringCreateWithBytes(NULL, (const UInt8 *)fname->ptr,
                                           fname->slen,
                                           kCFStringEncodingUTF8, false);
        if (!path_str) {
            CFRelease(filepath);
            return PJ_ENOMEM;
        }

        file = CFURLCreateCopyAppendingPathComponent(NULL, filepath,
                                                     path_str, false);
        CFRelease(path_str);
        CFRelease(filepath);
    } else {
        file = CFURLCreateFromFileSystemRepresentation(NULL,
               (const UInt8 *)fname->ptr, fname->slen, false);
    }

    if (!file)
        return PJ_ENOMEM;

    read_stream = CFReadStreamCreateWithFile(NULL, file);
    CFRelease(file);

    if (!read_stream)
        return PJ_ENOTFOUND;

    if (!CFReadStreamOpen(read_stream)) {
        PJ_LOG(2, (THIS_FILE, "Failed opening file"));
        CFRelease(read_stream);
        return PJ_EINVAL;
    }

    *data = NULL;

    buf = CFDataCreateMutable(NULL, 0);
    if (!buf) {
        CFReadStreamClose(read_stream);
        CFRelease(read_stream);
        return PJ_ENOMEM;
    }

    /* Read until end of stream: one CFReadStreamRead() returns at most
     * sizeof(data_buf) bytes and is not obliged to fill the buffer even
     * when more is available, so a single call would silently truncate.
     * Bounded so that a stream which never ends cannot run us out of memory;
     * every iteration that continues has appended at least one byte, so the
     * size limit bounds the iteration count too.
     */
    while ((nbytes = CFReadStreamRead(read_stream, data_buf,
                                      sizeof(data_buf))) > 0)
    {
        if (CFDataGetLength(buf) + nbytes > MAX_CERT_FILE_SIZE) {
            PJ_LOG(2, (THIS_FILE, "Certificate file exceeds %d bytes",
                       MAX_CERT_FILE_SIZE));
            too_big = PJ_TRUE;
            break;
        }
        CFDataAppendBytes(buf, data_buf, nbytes);
    }

    CFReadStreamClose(read_stream);
    CFRelease(read_stream);

    if (too_big) {
        CFRelease(buf);
        return PJ_ETOOBIG;
    }

    if (nbytes < 0 || CFDataGetLength(buf) == 0) {
        CFRelease(buf);
        return PJ_EINVAL;
    }

    *data = buf;

    return PJ_SUCCESS;
}

/* CFStringGetCString() leaves the buffer undefined when the string does
 * not fit, so a failed conversion yields an empty string instead. */
static pj_bool_t cfstr_to_cstr(CFStringRef str, char *buf, size_t size)
{
    if (CFStringGetCString(str, buf, (CFIndex)size, kCFStringEncodingUTF8))
        return PJ_TRUE;
    buf[0] = '\0';
    return PJ_FALSE;
}

#if !TARGET_OS_IPHONE
static void get_info_and_cn(CFArrayRef array, CFMutableStringRef info,
                            CFStringRef *cn)
{
    const void *keys[] = {kSecOIDOrganizationalUnitName, kSecOIDCountryName,
                          kSecOIDStateProvinceName, kSecOIDLocalityName,
                          kSecOIDOrganizationName, kSecOIDCommonName};
    const char *labels[] = { "OU=", "C=", "ST=", "L=", "O=", "CN="};
    pj_bool_t add_separator = PJ_FALSE;
    int i, n;

    *cn = NULL;
    if (!array || CFGetTypeID(array) != CFArrayGetTypeID())
        return;
    for(i = 0; i < (int)PJ_ARRAY_SIZE(keys);  i++) {
        for (n = 0 ; n < CFArrayGetCount(array); n++) {
            CFDictionaryRef dict;
            CFTypeRef dictkey;
            CFStringRef str;

            dict = CFArrayGetValueAtIndex(array, n);
            if (CFGetTypeID(dict) != CFDictionaryGetTypeID())
                continue;
            dictkey = CFDictionaryGetValue(dict, kSecPropertyKeyLabel);
            if (!dictkey || !CFEqual(dictkey, keys[i]))
                continue;
            str = (CFStringRef) CFDictionaryGetValue(dict,
                                                     kSecPropertyKeyValue);
            if (!str || CFGetTypeID(str) != CFStringGetTypeID())
                continue;

            if (CFStringGetLength(str) > 0) {
                if (add_separator) {
                    CFStringAppendCString(info, "/", kCFStringEncodingUTF8);
                }
                CFStringAppendCString(info, labels[i], kCFStringEncodingUTF8);
                CFStringAppend(info, str);
                add_separator = PJ_TRUE;

                if (CFEqual(keys[i], kSecOIDCommonName) && !*cn)
                    *cn = str;
            }
        }
    }
}

static CFDictionaryRef get_cert_oid(SecCertificateRef cert, CFStringRef oid,
                                    CFTypeRef *value)
{
    void *key[1];
    CFArrayRef key_arr;
    CFDictionaryRef vals, dict;

    key[0] = (void *)oid;
    key_arr = CFArrayCreate(NULL, (const void **)key, 1,
                            &kCFTypeArrayCallBacks);

    vals = SecCertificateCopyValues(cert, key_arr, NULL);
    dict = vals? CFDictionaryGetValue(vals, key[0]): NULL;
    if (dict && CFGetTypeID(dict) != CFDictionaryGetTypeID())
        dict = NULL;
    if (!dict) {
        CFRelease(key_arr);
        if (vals)
            CFRelease(vals);
        return NULL;
    }

    *value = CFDictionaryGetValue(dict, kSecPropertyKeyValue);

    CFRelease(key_arr);

    return vals;
}

#endif

#if !TARGET_OS_IPHONE
/* Convert an "IP Address" SAN value to text. Modern Security.framework
 * delivers the address as a dotted string; older systems carried the raw
 * octets inside the CFString. Try the text form first (both families),
 * else extract the bytes verbatim (ISOLatin1 maps char to octet 1:1). */
static pj_bool_t ip_san_to_text(CFStringRef value, char *buf,
                                size_t bufsize)
{
    pj_uint8_t ip_buf[16];
    pj_str_t s;
    CFIndex rawlen, used, i;

    if (cfstr_to_cstr(value, buf, bufsize)) {
        pj_strset(&s, buf, pj_ansi_strlen(buf));
        if (pj_inet_pton(pj_AF_INET(), &s, ip_buf) == PJ_SUCCESS &&
            pj_inet_ntop2(pj_AF_INET(), ip_buf, buf, bufsize))
        {
            return PJ_TRUE;
        }
        if (pj_inet_pton(pj_AF_INET6(), &s, ip_buf) == PJ_SUCCESS &&
            pj_inet_ntop2(pj_AF_INET6(), ip_buf, buf, bufsize))
        {
            return PJ_TRUE;
        }
    }

    rawlen = CFStringGetLength(value);
    if (rawlen != (CFIndex)sizeof(pj_in_addr) &&
        rawlen != (CFIndex)sizeof(pj_in6_addr))
    {
        return PJ_FALSE;
    }
    /* usedBufLen is an out-param, so the expected length must be kept
     * separately: require all chars converted and exactly one byte each. */
    if (CFStringGetBytes(value, CFRangeMake(0, rawlen),
                         kCFStringEncodingISOLatin1, 0, false,
                         ip_buf, sizeof(ip_buf), &used) != rawlen ||
        used != rawlen)
    {
        return PJ_FALSE;
    }
    /* A fully printable value was meant as text that failed to parse —
     * not raw octets; interpreting it would fabricate an address. */
    for (i = 0; i < rawlen; i++) {
        if (ip_buf[i] >= 0x20 && ip_buf[i] <= 0x7E)
            continue;
        break;
    }
    if (i == rawlen)
        return PJ_FALSE;
    return pj_inet_ntop2(rawlen == (CFIndex)sizeof(pj_in_addr)
                         ? pj_AF_INET() : pj_AF_INET6(),
                         ip_buf, buf, bufsize) != NULL;
}
#endif

/* Get certificate info; in case the certificate info is already populated,
 * this function will check if the contents need updating by inspecting the
 * issuer, the subject common name, and the serial number. */
static void get_cert_info(pj_pool_t *pool, pj_ssl_cert_info *ci,
                          SecCertificateRef cert, pj_bool_t reclaim)
{
    pj_bool_t update_needed;
    char buf[512], buf2[512];
    size_t bufsize = sizeof(buf);
    pj_uint8_t serial_no[sizeof(ci->serial_no)] = {0};
    pj_bool_t serial_truncated = PJ_FALSE;
    CFMutableStringRef issuer_info;
    CFStringRef str = NULL;
#if !TARGET_OS_IPHONE
    CFStringRef issuer_cn = NULL;
    CFDictionaryRef dict;
#endif

    pj_assert(pool && ci && cert);

    /* Get issuer */
    issuer_info = CFStringCreateMutable(NULL, 0);
    if (!issuer_info)
        return;
#if !TARGET_OS_IPHONE
{
    /* Unfortunately, unlike on Mac, on iOS we don't have these APIs
     * to query the certificate info such as the issuer, version,
     * validity, and alt names.
     */
    CFArrayRef issuer_vals;

    dict = get_cert_oid(cert, kSecOIDX509V1IssuerName,
                        (CFTypeRef *)&issuer_vals);
    if (dict) {
        get_info_and_cn(issuer_vals, issuer_info, &issuer_cn);
        if (issuer_cn)
            issuer_cn = CFStringCreateCopy(NULL, issuer_cn);
        CFRelease(dict);
    }
}
#endif
    cfstr_to_cstr(issuer_info, buf, bufsize);

    /* Get serial no, zero-padded to the fixed width of ci->serial_no so
     * the comparison below covers the whole field: a shorter serial that
     * is a prefix of the stored one must not compare equal. */
    if (__builtin_available(macOS 10.13, iOS 11.0, *)) {
        CFDataRef serial = SecCertificateCopySerialNumberData(cert, NULL);
        if (serial) {
            CFIndex len = CFDataGetLength(serial);
            if (len > (CFIndex)sizeof(serial_no)) {
                len = sizeof(serial_no);
                serial_truncated = PJ_TRUE;
            }
            pj_memcpy(serial_no, CFDataGetBytePtr(serial), len);
            CFRelease(serial);
        }
    }

    /* Subject CN is part of the update check, so read it now.
     * SecCertificateCopySubjectSummary() must not be used here: it returns
     * a display summary, which falls back to another attribute such as the
     * email address when the subject carries no Common Name. */
    buf2[0] = '\0';
    if (SecCertificateCopyCommonName(cert, &str) == errSecSuccess && str) {
        cfstr_to_cstr(str, buf2, sizeof(buf2));
        CFRelease(str);
        str = NULL;
    }

    /* Check if the contents need to be updated. A serial longer than the
     * stored field can never be proven equal to it — refresh rather than
     * collide on the shared prefix. */
    update_needed = pj_strcmp2(&ci->issuer.info, buf) ||
                    pj_strcmp2(&ci->subject.cn, buf2) ||
                    pj_memcmp(ci->serial_no, serial_no, sizeof(serial_no)) ||
                    serial_truncated;
    if (!update_needed) {
#if !TARGET_OS_IPHONE
        if (issuer_cn)
            CFRelease(issuer_cn);
#endif
        CFRelease(issuer_info);
        return;
    }

    /* Update cert info. When the caller passed the dedicated info pool,
     * reset it first so a refresh reclaims the previous strings instead
     * of accumulating them across renegotiations. */
    if (reclaim)
        pj_pool_reset(pool);

    pj_bzero(ci, sizeof(pj_ssl_cert_info));

    /* Version */
#if !TARGET_OS_IPHONE
{
    CFTypeRef version;

    dict = get_cert_oid(cert, kSecOIDX509V1Version, &version);
    if (dict) {
        /* The value may arrive as a CFNumber or a CFString; take either. */
        if (version && CFGetTypeID(version) == CFNumberGetTypeID()) {
            int v;
            if (CFNumberGetValue(version, kCFNumberIntType, &v))
                ci->version = (unsigned)v;
        } else if (version && CFGetTypeID(version) == CFStringGetTypeID()) {
            ci->version = CFStringGetIntValue((CFStringRef)version);
        }
        CFRelease(dict);
    }
}
#endif

    /* Issuer */
    pj_strdup2(pool, &ci->issuer.info, buf);
#if !TARGET_OS_IPHONE
    if (issuer_cn) {
        cfstr_to_cstr(issuer_cn, buf, bufsize);
        pj_strdup2(pool, &ci->issuer.cn, buf);
        CFRelease(issuer_cn);
    }
#endif
    CFRelease(issuer_info);

    /* Serial number */
    pj_memcpy(ci->serial_no, serial_no, sizeof(ci->serial_no));

    /* Subject Common Name (already read for the update check). */
    pj_strdup2(pool, &ci->subject.cn, buf2);
#if !TARGET_OS_IPHONE
{
    CFArrayRef subject;
    CFMutableStringRef subject_info;

    dict = get_cert_oid(cert, kSecOIDX509V1SubjectName,
                        (CFTypeRef *)&subject);
    if (dict) {
        subject_info = CFStringCreateMutable(NULL, 0);
        if (subject_info) {
            get_info_and_cn(subject, subject_info, &str);

            cfstr_to_cstr(subject_info, buf, bufsize);
            pj_strdup2(pool, &ci->subject.info, buf);

            CFRelease(subject_info);
        }
        CFRelease(dict);
    }
}
#endif

    /* Validity */
#if !TARGET_OS_IPHONE
{
    CFNumberRef validity;
    double interval;

    dict = get_cert_oid(cert, kSecOIDX509V1ValidityNotBefore,
                        (CFTypeRef *)&validity);
    if (dict) {
        if (validity && CFGetTypeID(validity) == CFNumberGetTypeID() &&
            CFNumberGetValue(validity, CFNumberGetType(validity),
                             &interval))
        {
            /* Darwin's absolute reference date is 1 Jan 2001 00:00:00 GMT */
            ci->validity.start.sec = (unsigned long)interval + 978278400L;
        }
        CFRelease(dict);
    }

    dict = get_cert_oid(cert, kSecOIDX509V1ValidityNotAfter,
                        (CFTypeRef *)&validity);
    if (dict) {
        if (validity && CFGetTypeID(validity) == CFNumberGetTypeID() &&
            CFNumberGetValue(validity, CFNumberGetType(validity),
                             &interval))
        {
            ci->validity.end.sec = (unsigned long)interval + 978278400L;
        }
        CFRelease(dict);
    }
}
#endif

    /* Subject Alternative Name extension */
#if !TARGET_OS_IPHONE
{
    CFArrayRef altname;
    CFIndex i;

    dict = get_cert_oid(cert, kSecOIDSubjectAltName, (CFTypeRef *)&altname);
    if (!dict)
        return;
    if (!altname || CFGetTypeID(altname) != CFArrayGetTypeID() ||
        !CFArrayGetCount(altname))
    {
        CFRelease(dict);
        return;
    }

    ci->subj_alt_name.entry = pj_pool_calloc(pool, CFArrayGetCount(altname),
                                             sizeof(*ci->subj_alt_name.entry));

    for (i = 0; i < CFArrayGetCount(altname); ++i) {
        CFDictionaryRef item;
        CFStringRef label, value;
        pj_ssl_cert_name_type type = PJ_SSL_CERT_NAME_UNKNOWN;

        item = CFArrayGetValueAtIndex(altname, i);
        if (CFGetTypeID(item) != CFDictionaryGetTypeID())
            continue;

        label = (CFStringRef)CFDictionaryGetValue(item, kSecPropertyKeyLabel);
        value = (CFStringRef)CFDictionaryGetValue(item, kSecPropertyKeyValue);
        if (!label || !value || CFGetTypeID(label) != CFStringGetTypeID())
            continue;

        if (!CFStringCompare(label, CFSTR("DNS Name"),
                             kCFCompareCaseInsensitive))
        {
            if (CFGetTypeID(value) != CFStringGetTypeID() ||
                !cfstr_to_cstr(value, buf, bufsize))
                continue;
            type = PJ_SSL_CERT_NAME_DNS;
        } else if (!CFStringCompare(label, CFSTR("IP Address"),
                                    kCFCompareCaseInsensitive))
        {
            if (CFGetTypeID(value) != CFStringGetTypeID() ||
                !ip_san_to_text(value, buf, bufsize))
                continue;
            type = PJ_SSL_CERT_NAME_IP;
        } else if (!CFStringCompare(label, CFSTR("Email Address"),
                                    kCFCompareCaseInsensitive))
        {
            if (CFGetTypeID(value) != CFStringGetTypeID() ||
                !cfstr_to_cstr(value, buf, bufsize))
                continue;
            type = PJ_SSL_CERT_NAME_RFC822;
        } else if (!CFStringCompare(label, CFSTR("URI"),
                                    kCFCompareCaseInsensitive))
        {
            CFStringRef uri;

            if (CFGetTypeID(value) != CFURLGetTypeID())
                continue;
            uri = CFURLGetString((CFURLRef)value);
            if (!uri || !cfstr_to_cstr(uri, buf, bufsize))
                continue;
            type = PJ_SSL_CERT_NAME_URI;
        }

        if (type != PJ_SSL_CERT_NAME_UNKNOWN) {
            ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
            pj_strdup2(pool,
                &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name, buf);
            ci->subj_alt_name.cnt++;
        }
    }

    CFRelease(dict);
}
#endif
}
