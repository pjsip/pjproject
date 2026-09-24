/*
 * Tests for the certificate-info reader shared by the two Apple TLS
 * backends. The functions under test are static inside
 * pj/ssl_sock_apple_common.c; the backends reach them by textual
 * inclusion, and so does this file. The fixtures are DER certificates
 * embedded below as hex (openssl-generated, CWD-independent), parsed by
 * Security.framework, so the peer-reachable paths are exercised with
 * real parser output rather than mocks.
 */
#include "test.h"

#define THIS_FILE   "ssl_sock_apple_test.c"

#if defined(PJ_DARWINOS) && PJ_DARWINOS != 0

#include <pjlib.h>
#include <pj/ssl_sock.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include "../pj/ssl_sock_apple_common.c"

static const char CERT_A_DER[] =
    "3082036a30820252a00302010202140102030405060708090a0b0c0d0e0f1011121314300d06"
    "092a864886f70d01010b0500302a3117301506035504030c0e6465722d746573742e6c6f6361"
    "6c310f300d060355040a0c06504a54455354301e170d3236303932343138333631355a170d33"
    "36303932313138333631355a302a3117301506035504030c0e6465722d746573742e6c6f6361"
    "6c310f300d060355040a0c06504a5445535430820122300d06092a864886f70d010101050003"
    "82010f003082010a0282010100ae1d8f776385243be017b993abeb813a9469e9860139d74eda"
    "a6dba674aa842933f1145ee7b111cb566de337f3dc0e43cdba60f8c4ce7024ec56f26f2cb2ff"
    "710a1c5f283c1504f0082c910086b24d3d156a743f1d8181a192a58a9e88d25a03ec7606527f"
    "047e582bab7d7a380379b554d84ec696fe9849392320a78b604f9adc758bb4db7a66eac5b9d6"
    "eb58d7880c7df474a6343796e31a531a3ce3d8ac139e6f4d3c37fd868931aba7280611c38442"
    "59e32c4e0aa8d3dd452079e38728ca95b1903be33170adbce8798c2e5f3870b4962a67faec7e"
    "c91e0827e9c78a7002240948e55b97fb9caa084d3f7c39a814d488639cd0f17e9b194ed703f4"
    "2e7aa90203010001a38187308184301d0603551d0e0416041485b4082b88c9054de0c9f5d56e"
    "f7b8f7e90439e3301f0603551d2304183016801485b4082b88c9054de0c9f5d56ef7b8f7e904"
    "39e3300f0603551d130101ff040530030101ff30310603551d11042a3028820e6465722d7465"
    "73742e6c6f63616c87040a000001871020010db8000000000000000000000001300d06092a86"
    "4886f70d01010b050003820101009bc042c8bf358a0cfc95275633d908a389d812d35c76b544"
    "4ab2daaeb016cbf9d7538ff857667e9fc6dac9fbbfaae91e21be66f4615869a23d7662c127c9"
    "04cae7d14d23b2394ecf19d0044a1b11507722ac470dd56afdce3f7f9a08ff8e90ee2558b4a7"
    "77bd190ffa1f6a1ee1e88d3c8c7b50f445f1a082aa5d4293f81baf504cfa728e1508251c49a7"
    "70891e9f6f730afbb545bddb97e6fbeebc2f6a8b226737dd746338750e8c3dfb2afe12dc74dc"
    "02f368fba86bfe47efb6cd864dd1f6acd04c553dcc339344281f6c5c4efa1f08b1189b6de363"
    "53151cabda2809d86b9a82407b542601013430acee59fe878c837db2acb849d6de163ef7604b"
    "2eec87a3";

static const char CERT_NOCN_DER[] =
    "30820307308201efa00302010202142607443145e654fcefc55031e47d4f3a74b8bd82300d06"
    "092a864886f70d01010b050030133111300f060355040a0c084e6f434e204f7267301e170d32"
    "36303932343137343833375a170d3236313032343137343833375a30133111300f060355040a"
    "0c084e6f434e204f726730820122300d06092a864886f70d01010105000382010f003082010a"
    "0282010100daa66d52e87fc6d2a22cd804be3e9fa70d8416321d0573e87cca7252e37c15e0bf"
    "534b5b61135fe2ceff66a62c46f0cab4ba374d3d7f7ecd6232a4772fb99060a2d13fdc31ceed"
    "ca0f20f82252e5f521f5dd28fd18ddcaa17f56181e0b8a0a77e51822f01e8169f5fec16266d2"
    "524f613eeea91361b166ebe673b74ff11bae455bd92de94f02d1725fd5a0702909ea188cc97e"
    "5b30d300e4ee5a92054832e9273c96f2bee619d92ea3d7157c82a2a5c2c3a5a4d87c923ca81f"
    "29c6bf9b7de91204969d31c2e4d2a3f15a563849deae6e8605122a0b8a6213c901f2bb6d2b7c"
    "94775c6272f4823b5781928d4bd991a6733ace59e02cb47d79d035ad524c20282f0203010001"
    "a3533051301d0603551d0e04160414364a026d701452afdaf6fb8c9b510f8aa6da57ac301f06"
    "03551d23041830168014364a026d701452afdaf6fb8c9b510f8aa6da57ac300f0603551d1301"
    "01ff040530030101ff300d06092a864886f70d01010b05000382010100c6d4ce755993f3f3d9"
    "82ee928fa78d3856070a7eee3aeac59d0c740cf76f731447127321426c933efd7df74c0767db"
    "96420cc99a3c963c09fc553536d424dacd926b7e9c59680e362ed05bff73da88bf5882cb8296"
    "57ffb2c0f400247812a6080012659b2c93068de0d8f67e093aa2f7d52e69f5db61d4bc8c7642"
    "55386ddad2fec66a0beb531e753cd2a99057c4dbdb2f0768eebcc84ca5afb3c5394606b97c43"
    "f287ede5f1e5b61c52a708aede2a150e5a8a8c4954bd1b7ce4829623a82aa955fb8863f099af"
    "21bb70040a2378cb9683e39d1e30762be24547c2fe5e9cc4a469bf537e6800152bbf314c75b2"
    "8b43e0999d7bdb806d8e6d40501754a6ccd5bc";

static const char CERT_LONGSER_DER[] =
    "3082031c30820204a00302010202170102030405060708090a0b0c0d0e0f1011121314151617"
    "300d06092a864886f70d01010b0500301c311a301806035504030c116c6f6e672d7365726961"
    "6c2e6c6f63616c301e170d3236303932343137343833375a170d323631303234313734383337"
    "5a301c311a301806035504030c116c6f6e672d73657269616c2e6c6f63616c30820122300d06"
    "092a864886f70d01010105000382010f003082010a0282010100dec1a4bb0667f853d537adc4"
    "4963f8044f7ad19f674be39fa37c2ecae71afeb9bba74afb5d17840d7cb820d219c0ee202315"
    "a7cd34f807b3ee601cb67c917c1c49ccb7e34d874c4ed6572d10d85cf0fb8022eb5ec9266b10"
    "9cf7f2728fe1b662b8197b2b6d8810f488002ca1c1bc60cb01ae76fd09b8519c56ed5b36640f"
    "76dcdb9cc6323f1333ce88987e808588301ab600965667cdf6917a45d5beede205c9c6c9adea"
    "c19367df153142f0684370f3f4ff101969f07f22f80becbfcedeaa90b965654772441b059041"
    "bcf69597ed4beb194a875e1d3afa2d04140960175b21a73fb3e921fad04f303d4090626e7787"
    "028d9a4031927454d86b2ed71890af430203010001a3533051301d0603551d0e041604140368"
    "24707643bbf95fe275025c5059b67cf215cd301f0603551d23041830168014036824707643bb"
    "f95fe275025c5059b67cf215cd300f0603551d130101ff040530030101ff300d06092a864886"
    "f70d01010b0500038201010007d6c58ee84031bb6f49b7568c7a27efd7f861e067c4ba77e646"
    "a2f12ec0980b4e31090da082656b6efaf64fdeb6cc84224e9100252abb4d0e61a694e4eca926"
    "9cccb26eb8607185c57ffdce45e921dbeeb20ea1e3b27288a576b7f5f9ffe83348ff72b95fb9"
    "a3cf3a6a85206929800dd4fd4d22038f88c741a5947092b3ba62e9dd5cc4ead2b4fc981672b3"
    "a771fa89c7840e71809495df819c63fa93b7417b5f042139d4966caf89366956be026fb1d144"
    "5df25ac0796e0a79d85ed2bf5853302157bec8635653ff471e481dfdc32d3d89acae16cb8974"
    "042e769b30045c54e377fed3dc77fbabf4356b771dd7c361dae982468d0f2a5dbf5397dbe965"
    "7368";

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static SecCertificateRef cert_from_hex(const char *hex)
{
    SecCertificateRef cert;
    CFDataRef data;
    pj_uint8_t der[1024];
    pj_size_t len = pj_ansi_strlen(hex) / 2, i;

    if (len > sizeof(der))
        return NULL;
    for (i = 0; i < len; i++)
        der[i] = (pj_uint8_t)((hexval(hex[2*i]) << 4) | hexval(hex[2*i+1]));
    data = CFDataCreate(NULL, der, (CFIndex)len);
    if (!data)
        return NULL;
    cert = SecCertificateCreateWithData(NULL, data);
    CFRelease(data);
    return cert;
}

static pj_bool_t find_san(const pj_ssl_cert_info *ci,
                          pj_ssl_cert_name_type type, const char *name)
{
    unsigned i;
    for (i = 0; i < ci->subj_alt_name.cnt; i++) {
        const pj_ssl_cert_name_type t = ci->subj_alt_name.entry[i].type;
        if (t == type && pj_strcmp2(&ci->subj_alt_name.entry[i].name,
                                    name) == 0)
            return PJ_TRUE;
    }
    return PJ_FALSE;
}

#if !TARGET_OS_IPHONE
/* IP SAN conversion coverage with synthetic values. The DER fixtures only
 * show whatever delivery format the local Security.framework uses, so the
 * raw-octets-in-CFString variant is built here with ISOLatin1 bytes. */
static int san_ip_tests(void)
{
    static const pj_uint8_t raw4[4] = { 10, 0, 0, 2 };
    static const pj_uint8_t raw16[16] = {
        0x20,0x01,0x0d,0xb8, 0,0,0,0, 0,0,0,0, 0,0,0,1
    };
    char buf[512];
    pj_str_t out;
    CFStringRef s;
    int rc = 0;

#define SAN_OK(cfstr, expect)                                              \
    (ip_san_to_text(cfstr, buf, sizeof(buf)) &&                            \
     (pj_strset(&out, buf, pj_ansi_strlen(buf)),                           \
      pj_strcmp2(&out, expect) == 0))

    /* Textual IPv4. */
    PJ_TEST_TRUE(SAN_OK(CFSTR("192.0.2.1"), "192.0.2.1"),
                 "textual IPv4", rc = -40);

    /* Textual IPv6, canonicalised. */
    PJ_TEST_TRUE(SAN_OK(CFSTR("2001:DB8::1"), "2001:db8::1"),
                 "textual IPv6", rc = -42);

    /* Raw octets in a CFString (older delivery format). */
    s = CFStringCreateWithBytes(NULL, raw4, 4,
                                kCFStringEncodingISOLatin1, false);
    PJ_TEST_TRUE(s != NULL, "raw4 CFString", rc = -44);
    PJ_TEST_TRUE(rc || SAN_OK(s, "10.0.0.2"),
                 "raw IPv4 octets", rc = -45);
    CFRelease(s);

    s = CFStringCreateWithBytes(NULL, raw16, 16,
                                kCFStringEncodingISOLatin1, false);
    PJ_TEST_TRUE(s != NULL, "raw16 CFString", rc = -46);
    PJ_TEST_TRUE(rc || SAN_OK(s, "2001:db8::1"),
                 "raw IPv6 octets", rc = -47);
    CFRelease(s);

    /* Malformed values must be skipped, not stored. */
    PJ_TEST_TRUE(!ip_san_to_text(CFSTR("not.an.ip"), buf, sizeof(buf)),
                 "garbage text rejected", rc = -48);
    s = CFStringCreateWithBytes(NULL, raw4, 3,
                                kCFStringEncodingISOLatin1, false);
    PJ_TEST_TRUE(s == NULL || !ip_san_to_text(s, buf, sizeof(buf)),
                 "3-octet value rejected", rc = -49);
    if (s) CFRelease(s);

    /* Printable strings that fail textual parsing are malformed text,
     * not raw octets — they must not be fabricated into addresses. */
    PJ_TEST_TRUE(!ip_san_to_text(CFSTR("abcd"), buf, sizeof(buf)),
                 "printable 4-char rejected", rc = -50);
    PJ_TEST_TRUE(!ip_san_to_text(CFSTR("0123456789abcdef"),
                                 buf, sizeof(buf)),
                 "printable 16-char rejected", rc = -51);

    /* A partially convertible string must not fabricate from
     * uninitialised tail bytes either. */
    PJ_TEST_TRUE(!ip_san_to_text(CFSTR("\u00e9\u00e9\u00e9\u2603"),
                                 buf, sizeof(buf)),
                 "partial conversion rejected", rc = -52);

#undef SAN_OK
    return rc;
}
#endif /* !TARGET_OS_IPHONE */

/* Guard-path coverage for get_info_and_cn: it consumes peer-controlled
 * OID entry dicts, so feed it synthetic arrays with missing keys, wrong
 * value types, and non-dict elements. */
static int guard_tests(void)
{
    CFMutableStringRef info;
    CFStringRef cn;
    CFTypeRef num;
    CFDictionaryRef e_novalue, e_numvalue, e_cn;
    CFArrayRef arr;
    int iv = 42;
    int rc = 0;

    info = CFStringCreateMutable(NULL, 0);
    num = CFNumberCreate(NULL, kCFNumberIntType, &iv);

    /* NULL and wrongly typed arrays leave cn NULL and info empty. */
    get_info_and_cn(NULL, info, &cn);
    PJ_TEST_TRUE(cn == NULL && CFStringGetLength(info) == 0,
                 "NULL array", rc = -60);
    get_info_and_cn((CFArrayRef)info, info, &cn);
    PJ_TEST_TRUE(cn == NULL && CFStringGetLength(info) == 0,
                 "non-array argument", rc = -61);

    e_novalue = CFDictionaryCreate(NULL,
        (const void **)&kSecPropertyKeyLabel,
        (const void **)&kSecOIDCommonName, 1,
        &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    e_numvalue = CFDictionaryCreate(NULL,
        (const void *[]){kSecPropertyKeyLabel, kSecPropertyKeyValue},
        (const void *[]){kSecOIDCommonName, num}, 2,
        &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    arr = CFArrayCreate(NULL,
        (const void *[]){info /* not a dict */, e_novalue, e_numvalue},
        3, &kCFTypeArrayCallBacks);
    get_info_and_cn(arr, info, &cn);
    PJ_TEST_TRUE(cn == NULL && CFStringGetLength(info) == 0,
                 "malformed entries skipped", rc = -62);
    CFRelease(arr);

    /* A valid CN entry among malformed ones is still picked up. */
    e_cn = CFDictionaryCreate(NULL,
        (const void *[]){kSecPropertyKeyLabel, kSecPropertyKeyValue},
        (const void *[]){kSecOIDCommonName, CFSTR("ok.local")}, 2,
        &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    arr = CFArrayCreate(NULL,
        (const void *[]){e_numvalue, e_cn}, 2,
        &kCFTypeArrayCallBacks);
    get_info_and_cn(arr, info, &cn);
    PJ_TEST_TRUE(cn != NULL && CFStringCompare(cn, CFSTR("ok.local"),
                                               0) == kCFCompareEqualTo,
                 "valid CN extracted", rc = -63);
    CFRelease(arr);
    CFRelease(e_cn);
    CFRelease(e_novalue);
    CFRelease(e_numvalue);
    CFRelease(num);
    CFRelease(info);

    return rc;
}

/* Update-detection coverage, observed through pool usage: the no-update
 * path allocates nothing, and the refresh path resets the dedicated
 * pool, so neither may grow it across repeated reads. */
static int refresh_tests(void)
{
    static const pj_uint8_t longser_first20[20] = {
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,
        0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14
    };
    pj_pool_t *info_pool;
    pj_ssl_cert_info ci;
    SecCertificateRef cert;
    pj_size_t used0;
    int i;

    /* Same cert twenty times: the no-update path must not allocate. */
    info_pool = pj_pool_create(mem, "ci1", 512, 512, NULL);
    pj_bzero(&ci, sizeof(ci));
    cert = cert_from_hex(CERT_A_DER);
    if (!cert) return -10;
    get_cert_info(info_pool, &ci, cert, PJ_TRUE);
    used0 = pj_pool_get_used_size(info_pool);
    for (i = 0; i < 20; i++)
        get_cert_info(info_pool, &ci, cert, PJ_TRUE);
    CFRelease(cert);
    PJ_TEST_EQ(pj_pool_get_used_size(info_pool), used0,
               "unchanged cert must not refresh",
               pj_pool_release(info_pool); return -11);
    pj_pool_release(info_pool);

    /* Overlong serial: first 20 octets stored, and each read refreshes —
     * the pool reset must keep usage flat across refreshes. */
    info_pool = pj_pool_create(mem, "ci2", 512, 512, NULL);
    pj_bzero(&ci, sizeof(ci));
    cert = cert_from_hex(CERT_LONGSER_DER);
    if (!cert) return -20;
    get_cert_info(info_pool, &ci, cert, PJ_TRUE);
    PJ_TEST_TRUE(pj_memcmp(ci.serial_no, longser_first20,
                           sizeof(longser_first20)) == 0,
                 "serial_no must hold the first 20 octets",
                 pj_pool_release(info_pool); CFRelease(cert); return -21);
    used0 = pj_pool_get_used_size(info_pool);
    for (i = 0; i < 20; i++)
        get_cert_info(info_pool, &ci, cert, PJ_TRUE);
    CFRelease(cert);
    PJ_TEST_EQ(pj_pool_get_used_size(info_pool), used0,
               "refreshes must not grow the pool",
               pj_pool_release(info_pool); return -22);
    pj_pool_release(info_pool);

    return 0;
}

int ssl_sock_apple_test(void)
{
    pj_pool_t *pool, *info_pool;
    pj_ssl_cert_info ci;
    SecCertificateRef cert, cert_b;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "..apple cert-info reader test"));

    pool = pj_pool_create(mem, "certinfo", 512, 512, NULL);
    info_pool = pj_pool_create(mem, "certinfo_i", 512, 512, NULL);
    pj_bzero(&ci, sizeof(ci));

    /* Ordinary cert: subject CN, DNS SAN, IP SAN. */
    cert = cert_from_hex(CERT_A_DER);
    if (!cert) return -5;
    get_cert_info(info_pool, &ci, cert, PJ_TRUE);
    PJ_TEST_TRUE(pj_strcmp2(&ci.subject.cn, "der-test.local") == 0,
                 "subject CN", rc = -6; goto out_a);
#if !TARGET_OS_IPHONE
    PJ_TEST_TRUE(find_san(&ci, PJ_SSL_CERT_NAME_DNS, "der-test.local"),
                 "DNS SAN entry", rc = -7; goto out_a);
    /* The IP value is raw octets; a UTF-8 read would garble or drop it. */
    PJ_TEST_TRUE(find_san(&ci, PJ_SSL_CERT_NAME_IP, "10.0.0.1"),
                 "IP SAN entry must be byte-exact", rc = -8; goto out_a);
    PJ_TEST_TRUE(find_san(&ci, PJ_SSL_CERT_NAME_IP, "2001:db8::1"),
                 "IPv6 SAN entry", rc = -10; goto out_a);
    PJ_TEST_TRUE(ci.version != 0, "version", rc = -9; goto out_a);
#endif
out_a:

    /* Certificate without a subject CN must not crash or misreport. */
    cert_b = cert_from_hex(CERT_NOCN_DER);
    if (!cert_b) { rc = -29; goto done; }
    get_cert_info(info_pool, &ci, cert_b, PJ_TRUE);
    PJ_TEST_EQ(ci.subject.cn.slen, 0,
               "no-CN cert yields empty subject.cn", rc = -30);

    /* The non-reclaiming variant (used for the local cert) must fill
     * the same fields without touching the pool. */
    if (rc == 0) {
        pj_ssl_cert_info ci_local;
        pj_bzero(&ci_local, sizeof(ci_local));
        get_cert_info(pool, &ci_local, cert, PJ_FALSE);
        PJ_TEST_TRUE(pj_strcmp2(&ci_local.subject.cn,
                                "der-test.local") == 0,
                     "non-reclaim path fills fields", rc = -32);
    }

    /* Cert change must refresh: after cert_b populated ci, reading
     * cert_a must restore its subject CN. */
    if (cert_b && rc == 0) {
        get_cert_info(info_pool, &ci, cert, PJ_TRUE);
        PJ_TEST_TRUE(pj_strcmp2(&ci.subject.cn, "der-test.local") == 0,
                     "changed cert must refresh", rc = -31);
    }
    if (rc == 0)
        rc = refresh_tests();
    if (rc == 0)
        rc = guard_tests();
#if !TARGET_OS_IPHONE
    if (rc == 0)
        rc = san_ip_tests();
#endif

done:
    if (cert) CFRelease(cert);
    if (cert_b) CFRelease(cert_b);
    pj_pool_release(info_pool);
    pj_pool_release(pool);
    return rc;
}

#else

int ssl_sock_apple_test(void)
{
    return 0;
}

#endif /* PJ_DARWINOS */
