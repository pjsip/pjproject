use pjsua::*;
use std::{ffi::CString, mem::MaybeUninit, os::raw::c_int, ptr};

const SIP_DOMAIN: &str = "pjsip.org";
const SIP_USER: &str = "username";
const SIP_PASSWD: &str = "secret";

/* Callback called by the library upon receiving incoming call */
pub unsafe extern "C" fn on_incoming_call(
    _acc_id: pjsua_acc_id,
    call_id: pjsua_call_id,
    _rdata: *mut pjsip_rx_data,
) {
    let mut ci = MaybeUninit::<pjsua_call_info>::uninit().assume_init();
    pjsua_call_get_info(call_id, &mut ci);
    /* Automatically answer incoming calls with 200/OK */
    pjsua_call_answer(call_id, 200, ptr::null(), ptr::null());
}

fn main() {
    unsafe {
        let mut _status = pjsua_create();

        let mut cfg = MaybeUninit::<pjsua_config>::uninit().assume_init();
        pjsua_config_default(&mut cfg);

        cfg.cb.on_incoming_call = Some(on_incoming_call);

        let mut log_cfg = MaybeUninit::<pjsua_logging_config>::uninit().assume_init();
        pjsua_logging_config_default(&mut log_cfg);

        _status = pjsua_init(&mut cfg, &mut log_cfg, ptr::null());

        let mut t_cfg = MaybeUninit::<pjsua_transport_config>::uninit().assume_init();
        pjsua_transport_config_default(&mut t_cfg);
        t_cfg.port = 0;

        let mut transport_id = 0 as c_int;
        _status = pjsua_transport_create(
            pjsip_transport_type_e_PJSIP_TRANSPORT_UDP,
            &mut t_cfg,
            &mut transport_id,
        );

        _status = pjsua_start();

        let mut acc_cfg = MaybeUninit::<pjsua_acc_config>::uninit().assume_init();
        pjsua_acc_config_default(&mut acc_cfg);

        let id = CString::new(&*format!("sip:{}@{}", SIP_USER, SIP_DOMAIN)).unwrap();
        acc_cfg.id = pj_str(id.as_ptr() as *mut i8);

        let uri = CString::new(&*format!("sip:{}", SIP_DOMAIN)).unwrap();
        acc_cfg.reg_uri = pj_str(uri.as_ptr() as *mut i8);

        acc_cfg.cred_count = 1;

        let sip_domain = CString::new(SIP_DOMAIN).unwrap();
        acc_cfg.cred_info[0].realm = pj_str(sip_domain.as_ptr() as *mut i8);

        let digest = CString::new("digest").unwrap();
        acc_cfg.cred_info[0].scheme = pj_str(digest.as_ptr() as *mut i8);

        let username = CString::new(SIP_USER).unwrap();
        acc_cfg.cred_info[0].username = pj_str(username.as_ptr() as *mut i8);

        acc_cfg.cred_info[0].data_type = pjsip_cred_data_type_PJSIP_CRED_DATA_PLAIN_PASSWD as i32;

        let password = CString::new(SIP_PASSWD).unwrap();
        acc_cfg.cred_info[0].data = pj_str(password.as_ptr() as *mut i8);

        let mut acc_id = MaybeUninit::<pjsua_acc_id>::uninit().assume_init();
        _status = pjsua_acc_add(&mut acc_cfg, pj_constants__PJ_TRUE as i32, &mut acc_id);

        pj_thread_sleep(10000);

        /* Destroy pjsua */
        pjsua_destroy();
    }
}
