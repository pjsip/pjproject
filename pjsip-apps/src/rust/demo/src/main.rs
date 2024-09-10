use pjsua::*;
use std::{ffi::CString, mem::MaybeUninit, os::raw::c_int, ptr};

const CSTRING_NEW_FAILED: &str = "CString::new failed!";

const SIP_DOMAIN: &str = "pjsip.org";
const SIP_USER: &str = "username";
const SIP_PASSWD: &str = "secret";

/* Callback called by the library upon receiving incoming call */
pub unsafe extern "C" fn on_incoming_call(
    _acc_id: pjsua_acc_id,
    call_id: pjsua_call_id,
    _rdata: *mut pjsip_rx_data,
) {
    let mut ci = MaybeUninit::<pjsua_call_info>::uninit();
    pjsua_call_get_info(call_id, ci.as_mut_ptr());

    /* Automatically answer incoming calls with 200/OK */
    pjsua_call_answer(call_id, 200, ptr::null(), ptr::null());
}

fn main() {
    unsafe {
        let mut _status = pjsua_create();

        let mut cfg = MaybeUninit::<pjsua_config>::uninit();
        pjsua_config_default(cfg.as_mut_ptr());
        let cfg_ptr = cfg.assume_init_mut();

        cfg_ptr.cb.on_incoming_call = Some(on_incoming_call);

        let mut log_cfg = MaybeUninit::<pjsua_logging_config>::uninit();
        pjsua_logging_config_default(log_cfg.as_mut_ptr());
        let log_cfg = log_cfg.assume_init();

        _status = pjsua_init(cfg_ptr, &log_cfg, ptr::null());

        let mut t_cfg = MaybeUninit::<pjsua_transport_config>::uninit();
        pjsua_transport_config_default(t_cfg.as_mut_ptr());
        let t_cfg_ptr = t_cfg.assume_init_mut();

        t_cfg_ptr.port = 0;

        let mut transport_id = 0 as c_int;

        _status = pjsua_transport_create(
            pjsip_transport_type_e_PJSIP_TRANSPORT_UDP,
            t_cfg_ptr,
            &mut transport_id,
        );

        _status = pjsua_start();

        let mut acc_cfg = MaybeUninit::<pjsua_acc_config>::uninit();
        pjsua_acc_config_default(acc_cfg.as_mut_ptr());
        let acc_cfg_ptr = acc_cfg.assume_init_mut();

        let id =
            CString::new(&*format!("sip:{}@{}", SIP_USER, SIP_DOMAIN)).expect(CSTRING_NEW_FAILED);
        acc_cfg_ptr.id = pj_str(id.as_ptr() as *mut i8);

        let uri = CString::new(&*format!("sip:{}", SIP_DOMAIN)).expect(CSTRING_NEW_FAILED);
        acc_cfg_ptr.reg_uri = pj_str(uri.as_ptr() as *mut i8);

        acc_cfg_ptr.cred_count = 1;

        let sip_domain = CString::new(SIP_DOMAIN).expect(CSTRING_NEW_FAILED);
        acc_cfg_ptr.cred_info[0].realm = pj_str(sip_domain.as_ptr() as *mut i8);

        let digest = CString::new("digest").expect(CSTRING_NEW_FAILED);
        acc_cfg_ptr.cred_info[0].scheme = pj_str(digest.as_ptr() as *mut i8);

        let username = CString::new(SIP_USER).expect(CSTRING_NEW_FAILED);
        acc_cfg_ptr.cred_info[0].username = pj_str(username.as_ptr() as *mut i8);

        acc_cfg_ptr.cred_info[0].data_type =
            pjsip_cred_data_type_PJSIP_CRED_DATA_PLAIN_PASSWD as i32;

        let password = CString::new(SIP_PASSWD).expect(CSTRING_NEW_FAILED);
        acc_cfg_ptr.cred_info[0].data = pj_str(password.as_ptr() as *mut i8);

        let mut acc_id = MaybeUninit::<pjsua_acc_id>::uninit();

        _status = pjsua_acc_add(
            acc_cfg_ptr,
            pj_constants__PJ_TRUE as i32,
            acc_id.as_mut_ptr(),
        );

        pj_thread_sleep(10000);

        /* Destroy pjsua */
        pjsua_destroy();
    }
}
