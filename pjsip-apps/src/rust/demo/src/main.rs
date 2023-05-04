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
    let ci = MaybeUninit::<pjsua_call_info>::uninit().as_mut_ptr();
    pjsua_call_get_info(call_id, ci);

    /* Automatically answer incoming calls with 200/OK */
    pjsua_call_answer(call_id, 200, ptr::null(), ptr::null());
}

fn main() {
    unsafe {
        let mut _status = pjsua_create();

        let mut cfg = MaybeUninit::<pjsua_config>::uninit().as_mut_ptr();
        pjsua_config_default(cfg);

        (*cfg).cb.on_incoming_call = Some(on_incoming_call);

        let log_cfg = MaybeUninit::<pjsua_logging_config>::uninit().as_mut_ptr();
        pjsua_logging_config_default(log_cfg);

        _status = pjsua_init(cfg, log_cfg, ptr::null());

        let mut t_cfg = MaybeUninit::<pjsua_transport_config>::uninit().as_mut_ptr();
        pjsua_transport_config_default(t_cfg);
        (*t_cfg).port = 0;

        let mut transport_id = 0 as c_int;
        _status = pjsua_transport_create(
            pjsip_transport_type_e_PJSIP_TRANSPORT_UDP,
            t_cfg,
            &mut transport_id,
        );

        _status = pjsua_start();

        let mut acc_cfg = MaybeUninit::<pjsua_acc_config>::uninit().as_mut_ptr();
        pjsua_acc_config_default(acc_cfg);

        let id =
            CString::new(&*format!("sip:{}@{}", SIP_USER, SIP_DOMAIN)).expect(CSTRING_NEW_FAILED);
        (*acc_cfg).id = pj_str(id.as_ptr() as *mut i8);

        let uri = CString::new(&*format!("sip:{}", SIP_DOMAIN)).expect(CSTRING_NEW_FAILED);
        (*acc_cfg).reg_uri = pj_str(uri.as_ptr() as *mut i8);

        (*acc_cfg).cred_count = 1;

        let sip_domain = CString::new(SIP_DOMAIN).expect(CSTRING_NEW_FAILED);
        (*acc_cfg).cred_info[0].realm = pj_str(sip_domain.as_ptr() as *mut i8);

        let digest = CString::new("digest").expect(CSTRING_NEW_FAILED);
        (*acc_cfg).cred_info[0].scheme = pj_str(digest.as_ptr() as *mut i8);

        let username = CString::new(SIP_USER).expect(CSTRING_NEW_FAILED);
        (*acc_cfg).cred_info[0].username = pj_str(username.as_ptr() as *mut i8);

        (*acc_cfg).cred_info[0].data_type =
            pjsip_cred_data_type_PJSIP_CRED_DATA_PLAIN_PASSWD as i32;

        let password = CString::new(SIP_PASSWD).expect(CSTRING_NEW_FAILED);
        (*acc_cfg).cred_info[0].data = pj_str(password.as_ptr() as *mut i8);

        let acc_id = MaybeUninit::<pjsua_acc_id>::uninit().as_mut_ptr();

        _status = pjsua_acc_add(acc_cfg, pj_constants__PJ_TRUE as i32, acc_id);

        pj_thread_sleep(10000);

        /* Destroy pjsua */
        pjsua_destroy();
    }
}
