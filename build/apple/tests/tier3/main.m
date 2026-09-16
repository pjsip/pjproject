/*
 * Tier 3: a real iOS application, launched on a booted simulator.
 *
 * This is the only tier that exercises the framework in an app process: the
 * CoreAudio and AVFoundation device backends, the Apple TLS backend actually
 * opening a listener, and the usage-description requirements that only apply
 * to a bundled app. It prints a single VERIFY-OK line on success, which the
 * driver script greps for, because simctl does not surface an exit code.
 *
 * Set SIP_DOMAIN, SIP_USER and SIP_PASS (via SIMCTL_CHILD_*) to also register
 * against a real server; without them the registration check is skipped.
 */
#import <UIKit/UIKit.h>
#include <stdlib.h>
#include <pjsua.h>

#define REG_TIMEOUT_SEC 20

static void fail(const char *what, pj_status_t status)
{
    char buf[PJ_ERR_MSG_SIZE];

    pj_strerror(status, buf, sizeof(buf));
    fprintf(stderr, "VERIFY-FAIL %s: %s\n", what, buf);
    fflush(stderr);
    exit(1);
}

/* Returns -1 when no credentials were supplied, 0 on a successful register. */
static int try_register(void)
{
    const char *domain = getenv("SIP_DOMAIN");
    const char *user = getenv("SIP_USER");
    const char *pass = getenv("SIP_PASS");
    pjsua_acc_config acc_cfg;
    pjsua_acc_id acc_id;
    pjsua_acc_info info;
    char id_uri[256], reg_uri[256];
    pj_status_t status;
    int waited;

    if (!domain || !user || !pass)
        return -1;

    pjsua_acc_config_default(&acc_cfg);
    pj_ansi_snprintf(id_uri, sizeof(id_uri), "sip:%s@%s", user, domain);
    pj_ansi_snprintf(reg_uri, sizeof(reg_uri), "sip:%s", domain);
    acc_cfg.id = pj_str(id_uri);
    acc_cfg.reg_uri = pj_str(reg_uri);
    acc_cfg.cred_count = 1;
    acc_cfg.cred_info[0].realm = pj_str("*");
    acc_cfg.cred_info[0].scheme = pj_str("digest");
    acc_cfg.cred_info[0].username = pj_str((char *)user);
    acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acc_cfg.cred_info[0].data = pj_str((char *)pass);

    status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &acc_id);
    if (status != PJ_SUCCESS)
        fail("pjsua_acc_add", status);

    for (waited = 0; waited < REG_TIMEOUT_SEC; ++waited) {
        pj_thread_sleep(1000);
        if (pjsua_acc_get_info(acc_id, &info) != PJ_SUCCESS)
            continue;
        if (info.status == 200) {
            printf("register: %s ok\n", id_uri);
            return 0;
        }
        if (info.status >= 300) {
            fprintf(stderr, "VERIFY-FAIL register %s: SIP %d\n",
                    id_uri, info.status);
            fflush(stderr);
            exit(1);
        }
    }

    fprintf(stderr, "VERIFY-FAIL register %s: timed out\n", id_uri);
    fflush(stderr);
    exit(1);
}

static void run_checks(void)
{
    pjsua_config cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config media_cfg;
    pjsua_transport_config tcfg;
    pjsua_transport_id tid;
    pj_status_t status;
    unsigned aud_devs, vid_devs;
    int registered;

    /* pj_init() registers the calling thread, so every pjsua call below has
     * to stay on this one. */
    status = pjsua_create();
    if (status != PJ_SUCCESS)
        fail("pjsua_create", status);

    pjsua_config_default(&cfg);
    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = 3;
    pjsua_media_config_default(&media_cfg);

    status = pjsua_init(&cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS)
        fail("pjsua_init", status);

    pjsua_transport_config_default(&tcfg);
    tcfg.port = 0;

    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tcfg, &tid);
    if (status != PJ_SUCCESS)
        fail("udp transport", status);

    /* The one check that proves the Apple Network framework backend is not
     * merely linked but able to open a listener. */
    status = pjsua_transport_create(PJSIP_TRANSPORT_TLS, &tcfg, &tid);
    if (status != PJ_SUCCESS)
        fail("tls transport", status);

    status = pjsua_start();
    if (status != PJ_SUCCESS)
        fail("pjsua_start", status);

    aud_devs = pjmedia_aud_dev_count();
    vid_devs = pjsua_vid_dev_count();
    if (aud_devs == 0)
        fail("no audio devices", PJ_ENOTFOUND);
    if (vid_devs == 0)
        fail("no video devices", PJ_ENOTFOUND);

    registered = try_register();

    /* Tear down before claiming success: simctl does not surface an exit
     * code, so the driver greps for the marker. Printing it first would let a
     * teardown crash pass as a clean run. */
    status = pjsua_destroy();
    if (status != PJ_SUCCESS)
        fail("pjsua_destroy", status);

    printf("VERIFY-OK ios pjsip=%s audio_devs=%u video_devs=%u register=%s\n",
           pj_get_version(), aud_devs, vid_devs,
           registered == 0 ? "ok" : "skipped");
    fflush(stdout);
    exit(0);
}

@interface VerifyDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation VerifyDelegate
- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)options
{
    (void)application;
    (void)options;
    /* Off the main thread so a slow registration cannot trip the launch
     * watchdog; the whole sequence then stays on this one thread. */
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        run_checks();
    });
    return YES;
}
@end

int main(int argc, char *argv[])
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil,
                                 NSStringFromClass([VerifyDelegate class]));
    }
}
