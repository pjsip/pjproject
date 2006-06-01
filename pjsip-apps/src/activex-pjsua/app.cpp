// App.cpp : Implementation of CApp
#include "stdafx.h"
#include <pj/types.h>
#include <pjsua-lib/pjsua.h>
#include "activex-pjsua.h"
#include "app.h"


/////////////////////////////////////////////////////////////////////////////
// CApp

// {9CE3052A-7A32-4229-B31C-5E02E0667A77}
static const GUID IID_Pjsip_Cred_Info = 
{ 0x9ce3052a, 0x7a32, 0x4229, { 0xb3, 0x1c, 0x5e, 0x2, 0xe0, 0x66, 0x7a, 0x77 } };

// {3B12B04F-6E48-46a7-B9E0-6C4BF1594A96}
static const GUID IID_Pjsua_Acc_Config = 
{ 0x3b12b04f, 0x6e48, 0x46a7, { 0xb9, 0xe0, 0x6c, 0x4b, 0xf1, 0x59, 0x4a, 0x96 } };

// {E4B6573D-CF5E-484d-863F-ADAD5947FBE4}
static const GUID IID_Pjsua_Config = 
{ 0xe4b6573d, 0xcf5e, 0x484d, { 0x86, 0x3f, 0xad, 0xad, 0x59, 0x47, 0xfb, 0xe4 } };

// {5043AC9E-A417-4f03-927E-D7AE52DDD064}
static const GUID IID_Pjsua_Call_Info = 
{ 0x5043ac9e, 0xa417, 0x4f03, { 0x92, 0x7e, 0xd7, 0xae, 0x52, 0xdd, 0xd0, 0x64 } };

// {2729F0BC-8A5E-4f3f-BC29-C1740A86393A}
static const GUID IID_Pjsua_Buddy_Info = 
{ 0x2729f0bc, 0x8a5e, 0x4f3f, { 0xbc, 0x29, 0xc1, 0x74, 0xa, 0x86, 0x39, 0x3a } };

// {8D345956-10B7-4450-8A06-A80D2F319EFD}
static const GUID IID_Pjsua_Acc_Info = 
{ 0x8d345956, 0x10b7, 0x4450, { 0x8a, 0x6, 0xa8, 0xd, 0x2f, 0x31, 0x9e, 0xfd } };

#define SA_SIZE(lbound,ubound)	(ubound-lbound)


class Temp_Pool
{
public:
    Temp_Pool()
    {
	pool_ = pjsip_endpt_create_pool( pjsua_get_pjsip_endpt(), "ActivePJSUA",
					 4000, 4000);
    }
    ~Temp_Pool()
    {
	pj_pool_release(pool_);
    }

    pj_pool_t *get_pool()
    {
	return pool_;
    }

private:
    pj_pool_t *pool_;
};

static pj_str_t Pj_str(pj_pool_t *pool, Pj_String s)
{
    pj_str_t ret;
    unsigned len;
    
    len = wcslen(s);
    if (len) {
	ret.ptr = (char*)pj_pool_alloc(pool, len+1);
	ret.slen = len;
	pj_unicode_to_ansi(s, len, ret.ptr, len+1);
	ret.ptr[ret.slen] = '\0';
    } else {
	ret.ptr = NULL;
	ret.slen = 0;
    }

    return ret;
}

BSTR str2bstr(const char *str, unsigned len)
{
    if (len == 0) {
	return SysAllocString(L"");
    } else {
	OLECHAR *tmp;
	BSTR result;
	tmp = (OLECHAR*) malloc((len+1) * sizeof(OLECHAR));
	pj_ansi_to_unicode(str, len, tmp, len+1);
	result = SysAllocString(tmp);
	free(tmp);
	return result;
    }
}

#define Cp(d,s)	Cp2(&d,s)
static void Cp2(BSTR *dst, const pj_str_t *src)
{
    *dst = str2bstr(src->ptr, src->slen);
}



static void SafeStringArray2pjstrarray(pj_pool_t *pool,
				       SAFEARRAY *sa, unsigned *count,
				       pj_str_t a[])
{
    if (!sa)
	*count = 0;
    else {
	HRESULT hr;
	long lbound;
	unsigned i;

	hr = SafeArrayGetLBound(sa, 1, &lbound);
	if (FAILED(hr))
	    *count = 0;
	else {
	    *count = 0;
	    for (i=0; i<sa->cbElements; ++i) {
		BSTR str;
		long rg = lbound + i;
		hr = SafeArrayGetElement(sa, &rg, &str);
		if (FAILED(hr))
		    break;
		a[*count] = Pj_str(pool, str);
		*count = *count + 1;
	    }
	}
    }
}

static void pjstrarray2SafeStringArray(unsigned count, const pj_str_t a[],
				       SAFEARRAY **psa)
{
    unsigned i;
    SAFEARRAY *sa;

    sa = SafeArrayCreateVector( VT_BSTR, 0, count);

    for (i=0; i<count; ++i) {
	BSTR value;
	Cp(value, &a[i]);
	long rg = i;
	SafeArrayPutElement(sa, &rg, value);
    }

    *psa = sa;
}

static void AccConfig2accconfig(pj_pool_t *pool,
				Pjsua_Acc_Config *c1,
				pjsua_acc_config *c2)
{
    pj_memset(c2, 0, sizeof(pjsua_acc_config));

    c2->id = Pj_str(pool, c1->acc_uri);
    c2->reg_uri = Pj_str(pool, c1->reg_uri);
    c2->contact = Pj_str(pool, c1->contact_uri);
    c2->proxy = Pj_str(pool, c1->proxy_uri);
    c2->reg_timeout = c1->reg_timeout;

    if (c1->cred_info == NULL) {
	c2->cred_count = 0;
    } else {
	unsigned i;
	long lbound;
	HRESULT hr;

	hr = SafeArrayGetLBound(c1->cred_info, 1, &lbound);
	if (FAILED(hr)) {
	    c2->cred_count = 0;
	} else {
	    c2->cred_count = 0;
	    for (i=0; i<c1->cred_info->cbElements; ++i) {
		Pjsip_Cred_Info cred_info;
		long rg = lbound + i;
		hr = SafeArrayGetElement(c1->cred_info, &rg, &cred_info);
		if (FAILED(hr))
		    break;
		c2->cred_info[i].realm = Pj_str(pool, cred_info.realm);
		c2->cred_info[i].scheme = Pj_str(pool, cred_info.scheme);
		c2->cred_info[i].username = Pj_str(pool, cred_info.username);
		c2->cred_info[i].data_type = cred_info.hashed;
		c2->cred_info[i].data = Pj_str(pool, cred_info.data);
	    }
	    c2->cred_count = i;
	}
    }
}

static HRESULT accconfig2AccConfig(pjsua_acc_config *c1,
				  Pjsua_Acc_Config *c2)
{
    unsigned i;

    //pj_memset(c2, 0, sizeof(Pjsua_Acc_Config));

    Cp(c2->acc_uri, &c1->id);
    Cp(c2->reg_uri, &c1->reg_uri);
    Cp(c2->contact_uri, &c1->contact);
    Cp(c2->proxy_uri, &c1->proxy);
    c2->reg_timeout = c1->reg_timeout;


    IRecordInfo *pUdtRecordInfo = NULL;
    HRESULT hr = GetRecordInfoFromGuids( LIBID_ACTIVEPJSUALib,
                                         1, 0, 
                                         0,
                                         IID_Pjsip_Cred_Info,
                                         &pUdtRecordInfo );
    if( FAILED( hr ) ) {
        return( hr ); //Return original HRESULT hr2 is for debug only
    }

    SAFEARRAYBOUND rgsabound[1];
    rgsabound[0].lLbound = 0;
    rgsabound[0].cElements = c1->cred_count;

    c2->cred_info = ::SafeArrayCreateEx( VT_RECORD, 1, rgsabound, pUdtRecordInfo );

    pUdtRecordInfo->Release(); //do not forget to release the interface

    for (i=0; i<c1->cred_count; ++i) {
	Pjsip_Cred_Info cred_info;

	Cp(cred_info.realm, &c1->cred_info[i].realm);
	Cp(cred_info.scheme, &c1->cred_info[i].scheme);
	Cp(cred_info.username, &c1->cred_info[i].username);
	cred_info.hashed = (c1->cred_info[i].data_type != 0);
	Cp(cred_info.data, &c1->cred_info[i].data);

	long rg = i;
	SafeArrayPutElement(c2->cred_info, &rg, &cred_info);
    }

    return S_OK;
}

static HRESULT Config2config(pj_pool_t *pool, Pjsua_Config *c1, pjsua_config *c2)
{
    pj_memset(c2, 0, sizeof(pjsua_config));

    c2->udp_port = c1->udp_port;
    c2->sip_host = Pj_str(pool, c1->sip_host);
    c2->sip_port = c1->sip_port;
    c2->start_rtp_port = c1->rtp_port;
    c2->max_calls = c1->max_calls;
    c2->conf_ports = c1->conf_ports;
    c2->thread_cnt = c1->thread_cnt;
    c2->stun_srv1 = Pj_str(pool, c1->stun_srv1);
    c2->stun_port1 = c1->stun_port1;
    c2->stun_srv2 = Pj_str(pool, c1->stun_srv2);
    c2->stun_port2 = c1->stun_port2;
    c2->snd_player_id = c1->snd_player_id;
    c2->snd_capture_id = c1->snd_capture_id;
    c2->clock_rate = c1->clock_rate;
    c2->null_audio = c1->null_audio;
    c2->quality = c1->quality;
    c2->complexity = c1->complexity;

    SafeStringArray2pjstrarray(pool, c1->codec_arg, &c2->codec_cnt, c2->codec_arg);

    c2->auto_answer = c1->auto_answer;
    c2->uas_refresh = c1->uas_refresh;
    c2->outbound_proxy = Pj_str(pool, c1->outbound_proxy);

    if (!c1->acc_config)
	c2->acc_cnt = 0;
    else {
	HRESULT hr;
	long lbound;
	unsigned i;

	hr = SafeArrayGetLBound(c1->acc_config, 1, &lbound);
	if (FAILED(hr))
	    c2->acc_cnt = 0;
	else {
	    c2->acc_cnt = 0;

	    for (i=0; i<c1->acc_config->cbElements; ++i) {
		Pjsua_Acc_Config acc_config;
		long rg = lbound + i;
		hr = SafeArrayGetElement(c1->acc_config, &rg, &acc_config);
		if (FAILED(hr))
		    break;
		AccConfig2accconfig(pool, &acc_config, &c2->acc_config[i]);
	    }

	    c2->acc_cnt = i;
	}
    }

    c2->log_level = c1->log_level;
    c2->app_log_level = c1->app_log_level;
    c2->log_decor = c1->log_decor;
    c2->log_filename = Pj_str(pool, c1->log_filename);

    SafeStringArray2pjstrarray(pool, c1->buddy_uri, &c2->buddy_cnt, c2->buddy_uri);

    return S_OK;
}

static HRESULT config2Config(pjsua_config *c1, Pjsua_Config *c2)
{
    unsigned i;
    HRESULT hr;

    //pj_memset(c2, 0, sizeof(Pjsua_Config));

    c2->udp_port = c1->udp_port;
    Cp(c2->sip_host, &c1->sip_host);
    c2->sip_port = c1->sip_port;
    c2->rtp_port = c1->start_rtp_port;
    c2->max_calls = c1->max_calls;
    c2->conf_ports = c1->conf_ports;
    c2->thread_cnt = c1->thread_cnt;
    Cp(c2->stun_srv1, &c1->stun_srv1);
    c2->stun_port1 = c1->stun_port1;
    Cp(c2->stun_srv2, &c1->stun_srv2);
    c2->stun_port2 = c1->stun_port2;
    c2->snd_player_id = c1->snd_player_id;
    c2->snd_capture_id = c1->snd_capture_id;
    c2->clock_rate = c1->clock_rate;
    c2->null_audio = c1->null_audio;
    c2->quality = c1->quality;
    c2->complexity = c1->complexity;

    pjstrarray2SafeStringArray(c1->codec_cnt, c1->codec_arg, &c2->codec_arg);

    c2->auto_answer = c1->auto_answer;
    c2->uas_refresh = c1->uas_refresh;

    Cp(c2->outbound_proxy, &c1->outbound_proxy);

    IRecordInfo *pUdtRecordInfo = NULL;
    hr = GetRecordInfoFromGuids( LIBID_ACTIVEPJSUALib,
                                 1, 0, 
                                 0,
                                 IID_Pjsua_Acc_Config,
                                 &pUdtRecordInfo );
    if( FAILED( hr ) ) {
        return( hr ); //Return original HRESULT hr2 is for debug only
    }

    SAFEARRAYBOUND rgsabound[1];
    rgsabound[0].lLbound = 0;
    rgsabound[0].cElements = c1->acc_cnt;

    c2->acc_config = ::SafeArrayCreateEx( VT_RECORD, 1, rgsabound, pUdtRecordInfo );

    pUdtRecordInfo->Release(); //do not forget to release the interface

    for (i=0; i<c1->acc_cnt; ++i) {
	Pjsua_Acc_Config acc_cfg;

	hr = accconfig2AccConfig(&c1->acc_config[i], &acc_cfg);
	if (FAILED(hr))
	    return hr;

	long rg = i;
	SafeArrayPutElement(c2->acc_config, &rg, &acc_cfg);
    }


    c2->log_level = c1->log_level;
    c2->app_log_level = c1->app_log_level;
    c2->log_decor = c1->log_decor;

    Cp(c2->log_filename, &c1->log_filename);

    pjstrarray2SafeStringArray(c1->buddy_cnt, c1->buddy_uri, &c2->buddy_uri);

    return S_OK;
}

static void callinfo2CallInfo(pjsua_call_info *c1, Pjsua_Call_Info *c2)
{
    pj_memset(c2, 0, sizeof(Pjsua_Call_Info));

    c2->index = c1->index;
    c2->active = c1->active;
    c2->is_uac = (c1->role == PJSIP_ROLE_UAC);
    Cp(c2->local_info, &c1->local_info);
    Cp(c2->remote_info, &c1->remote_info);
    c2->state = (Pjsua_Call_State)c1->state;
    Cp(c2->state_text, &c1->state_text);
    c2->connect_duration = c1->connect_duration.sec;
    c2->total_duration = c1->total_duration.sec;
    c2->cause = c1->cause;
    Cp(c2->cause_text, &c1->cause_text);
    c2->has_media = c1->has_media;
    c2->conf_slot = c1->conf_slot;
}

static void accinfo2AccInfo(pjsua_acc_info *info1, Pjsua_Acc_Info *info2)
{
    pj_memset(info2, 0, sizeof(Pjsua_Acc_Info));

    info2->index = info1->index;
    Cp(info2->acc_id, &info1->acc_id);
    info2->has_registration = info1->has_registration;
    info2->expires = info1->expires;
    info2->status_code = info1->status;
    Cp(info2->status_text, &info1->status_text);
    info2->online_status = info1->online_status;
}

static void buddyinfo2BuddyInfo(pjsua_buddy_info *info1, Pjsua_Buddy_Info *info2)
{
    pj_memset(info2, 0, sizeof(Pjsua_Buddy_Info));

    info2->index = info1->index;
    info2->is_valid = info1->is_valid;
    Cp(info2->name, &info1->name);
    Cp(info2->display, &info1->display_name);
    Cp(info2->host, &info1->host);
    info2->port = info1->port;
    Cp(info2->uri, &info1->uri);
    info2->status = (Pjsua_Buddy_State)info1->status;
    Cp(info2->status_text, &info1->status_text);
    info2->monitor = info1->monitor;
    info2->acc_index = info1->acc_index;
}

static CApp *CApp_Instance;

CApp::CApp()
{
    CApp_Instance = this;
}

STDMETHODIMP CApp::app_create(Pj_Status *ret)
{
    *ret = pjsua_create();
    return S_OK;
}

STDMETHODIMP CApp::app_default_config(Pjsua_Config *pConfig)
{
    pjsua_config cfg;
    pjsua_default_config(&cfg);
    return config2Config(&cfg, pConfig);
}

STDMETHODIMP CApp::app_test_config(Pjsua_Config *pConfig, BSTR *retmsg)
{
    pjsua_config cfg;
    HRESULT hr;
    Temp_Pool tp;
    char errmsg[PJ_ERR_MSG_SIZE];

    hr = Config2config(tp.get_pool(), pConfig, &cfg);
    if (FAILED(hr))
	return hr;

    pjsua_test_config(&cfg, errmsg, sizeof(errmsg));
    *retmsg = str2bstr(errmsg, strlen(errmsg));
    return S_OK;
}

static void on_call_state(int call_index, pjsip_event *e)
{
    pjsua_call_info call_info;
    Pjsua_Call_Info *Call_Info = new Pjsua_Call_Info;

    pjsua_get_call_info(call_index, &call_info);
    callinfo2CallInfo(&call_info, Call_Info);

    CApp_Instance->Fire_OnCallState(call_index, Call_Info);
}

static void on_reg_state(int acc_index)
{
    CApp_Instance->Fire_OnRegState(acc_index);
}

static void on_buddy_state(int buddy_index)
{
    CApp_Instance->Fire_OnBuddyState(buddy_index);
}

static void on_pager(int call_index, const pj_str_t *from,
		     const pj_str_t *to, const pj_str_t *txt)
{
    BSTR fromURI, toURI, imText;

    Cp2(&fromURI, from);
    Cp2(&toURI, to);
    Cp2(&imText, txt);

    CApp_Instance->Fire_OnIncomingPager(call_index, fromURI, toURI, imText);
}

static void on_typing(int call_index, const pj_str_t *from,
		      const pj_str_t *to, pj_bool_t is_typing)
{
    BSTR fromURI, toURI;

    Cp2(&fromURI, from);
    Cp2(&toURI, to);

    CApp_Instance->Fire_OnTypingIndication(call_index, fromURI, toURI, is_typing);
}


STDMETHODIMP CApp::app_init(Pjsua_Config *pConfig, Pj_Status *pStatus)
{
    pjsua_config cfg;
    pjsua_callback cb;
    Temp_Pool tp;
    HRESULT hr;

    pj_memset(&cb, 0, sizeof(cb));
    cb.on_call_state = &on_call_state;
    cb.on_reg_state = &on_reg_state;
    cb.on_buddy_state = &on_buddy_state;
    cb.on_pager = &on_pager;
    cb.on_typing = &on_typing;

    hr = Config2config(tp.get_pool(), pConfig, &cfg);
    if (FAILED(hr))
	return hr;
    
    *pStatus = pjsua_init(&cfg, &cb);
    return S_OK;
}

STDMETHODIMP CApp::app_start(Pj_Status *retStatus)
{
    *retStatus = pjsua_start();
    return S_OK;
}

STDMETHODIMP CApp::app_destroy(Pj_Status *retStatus)
{
    *retStatus = pjsua_destroy();
    return S_OK;
}

STDMETHODIMP CApp::call_get_max_count(int *retCount)
{
    *retCount = pjsua_get_max_calls();
    return S_OK;
}

STDMETHODIMP CApp::call_get_count(int *retCount)
{
    *retCount = pjsua_get_call_count();
    return S_OK;
}

STDMETHODIMP CApp::call_is_active(int call_index, Pj_Bool *retVal)
{
    *retVal = pjsua_call_is_active(call_index);
    return S_OK;
}

STDMETHODIMP CApp::call_has_media(int call_index, Pj_Bool *pRet)
{
    *pRet = pjsua_call_has_media(call_index);
    return S_OK;
}

STDMETHODIMP CApp::call_get_info(int call_index, Pjsua_Call_Info *pInfo, Pj_Status *pRet)
{
    pjsua_call_info info;
    *pRet = pjsua_get_call_info(call_index, &info);
    callinfo2CallInfo(&info, pInfo);
    return S_OK;
}

STDMETHODIMP CApp::call_make_call(int acc_index, Pj_String dst_uri, int *call_index, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), dst_uri);

    *pRet = pjsua_make_call(acc_index, &tmp, call_index);
    return S_OK;
}

STDMETHODIMP CApp::call_answer(int call_index, int status_code, Pj_Status *pRet)
{
    pjsua_call_answer(call_index, status_code);
    *pRet = PJ_SUCCESS;
    return S_OK;
}

STDMETHODIMP CApp::call_hangup(int call_index, Pj_Status *pRet)
{
    pjsua_call_hangup(call_index);
    *pRet = PJ_SUCCESS;
    return S_OK;
}

STDMETHODIMP CApp::call_set_hold(int call_index, Pj_Status *pRet)
{
    pjsua_call_set_hold(call_index);
    *pRet = PJ_SUCCESS;
    return S_OK;
}

STDMETHODIMP CApp::call_release_hold(int call_index, Pj_Status *pRet)
{
    pjsua_call_reinvite(call_index);
    *pRet = PJ_SUCCESS;
    return S_OK;
}

STDMETHODIMP CApp::call_xfer(int call_index, Pj_String dst_uri, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), dst_uri);
    pjsua_call_xfer(call_index, &tmp);
    *pRet = PJ_SUCCESS;
    return S_OK;
}

STDMETHODIMP CApp::call_dial_dtmf(int call_index, Pj_String digits, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), digits);
    *pRet = pjsua_call_dial_dtmf(call_index, &tmp);
    return S_OK;
}

STDMETHODIMP CApp::call_send_im(int call_index, Pj_String text, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), text);
    pjsua_call_send_im(call_index, &tmp);
    *pRet = PJ_SUCCESS;
    return S_OK;
}

STDMETHODIMP CApp::call_typing(int call_index, int is_typing, Pj_Status *pRet)
{
    pjsua_call_typing(call_index, is_typing);
    *pRet = PJ_SUCCESS;
    return S_OK;
}

STDMETHODIMP CApp::call_hangup_all()
{
    pjsua_call_hangup_all();
    return S_OK;
}

STDMETHODIMP CApp::call_get_textstat(int call_index, BSTR *textstat)
{
    char buf[1024];
    pjsua_dump_call(call_index, 1, buf, sizeof(buf), "");

    OLECHAR wbuf[1024];
    pj_ansi_to_unicode(buf, strlen(buf), wbuf, PJ_ARRAY_SIZE(wbuf));
    *textstat = SysAllocString(wbuf);
    return S_OK;
}


STDMETHODIMP CApp::acc_get_count(int *pCount)
{
    *pCount = pjsua_get_acc_count();
    return S_OK;
}

STDMETHODIMP CApp::acc_get_info(int acc_index, Pjsua_Acc_Info *pInfo, Pj_Status *pRet)
{
    pjsua_acc_info info;
    *pRet = pjsua_acc_get_info(acc_index, &info);
    accinfo2AccInfo(&info, pInfo);
    return S_OK;
}

STDMETHODIMP CApp::acc_add(Pjsua_Acc_Config *pConfig, int *pAcc_Index, Pj_Status *pRet)
{
    Temp_Pool tp;
    pjsua_acc_config config;
    AccConfig2accconfig(tp.get_pool(), pConfig,  &config);
    *pRet = pjsua_acc_add(&config, pAcc_Index);
    return S_OK;
}

STDMETHODIMP CApp::acc_set_online_status(int acc_index, int is_online, Pj_Status *pRet)
{
    *pRet = pjsua_acc_set_online_status(acc_index, is_online);
    return S_OK;
}

STDMETHODIMP CApp::acc_set_registration(int acc_index, int reg_active, Pj_Status *pRet)
{
    *pRet = pjsua_acc_set_registration(acc_index, reg_active);
    return S_OK;
}

STDMETHODIMP CApp::buddy_get_count(int *pCount)
{
    *pCount = pjsua_get_buddy_count();
    return S_OK;
}

STDMETHODIMP CApp::buddy_get_info(int buddy_index, Pjsua_Buddy_Info *pInfo, Pj_Status *pRet)
{
    pjsua_buddy_info info;
    *pRet = pjsua_buddy_get_info(buddy_index, &info);
    buddyinfo2BuddyInfo(&info, pInfo);
    return S_OK;
}

STDMETHODIMP CApp::buddy_add(Pj_String uri, int *pBuddy_Index, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), uri);
    *pRet = pjsua_buddy_add(&tmp, pBuddy_Index);
    return S_OK;
}

STDMETHODIMP CApp::buddy_subscribe_pres(int buddy_index, int subscribe, Pj_Status *pRet)
{
    *pRet = pjsua_buddy_subscribe_pres(buddy_index, subscribe);
    pjsua_pres_refresh();
    return S_OK;
}

STDMETHODIMP CApp::im_send_text(int acc_index, Pj_String dst_uri, Pj_String text, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp_uri = Pj_str(tp.get_pool(), dst_uri);
    pj_str_t tmp_text = Pj_str(tp.get_pool(), text);
    *pRet = pjsua_im_send(acc_index, &tmp_uri, &tmp_text);
    return S_OK;
}

STDMETHODIMP CApp::im_typing(int acc_index, Pj_URI dst_uri, int is_typing, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp_uri = Pj_str(tp.get_pool(), dst_uri);
    *pRet = pjsua_im_typing(acc_index, &tmp_uri, is_typing);
    return S_OK;
}

STDMETHODIMP CApp::conf_connect(int src_port, int sink_port, Pj_Status *pRet)
{
    *pRet = pjsua_conf_connect(src_port, sink_port);
    return S_OK;
}

STDMETHODIMP CApp::conf_disconnect(int src_port, int sink_port, Pj_Status *pRet)
{
    *pRet = pjsua_conf_disconnect(src_port, sink_port);
    return S_OK;
}

STDMETHODIMP CApp::player_create(Pj_String filename, int *pPlayer_Id, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), filename);
    *pRet = pjsua_player_create(&tmp, pPlayer_Id);
    return S_OK;
}

STDMETHODIMP CApp::player_get_conf_port(int player_id, int *pPort)
{
    *pPort = pjsua_player_get_conf_port(player_id);
    return S_OK;
}

STDMETHODIMP CApp::player_set_pos(int player_id, int pos, Pj_Status *pRet)
{
    *pRet = pjsua_player_set_pos(player_id, pos);
    return S_OK;
}

STDMETHODIMP CApp::player_destroy(int player_id, Pj_Status *pRet)
{
    *pRet = pjsua_player_destroy(player_id);
    return S_OK;
}

STDMETHODIMP CApp::recorder_create(Pj_String filename, int *pRecorder_Id, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), filename);
    *pRet = pjsua_recorder_create(&tmp, pRecorder_Id);
    return S_OK;
}

STDMETHODIMP CApp::recorder_get_conf_port(int recorder_id, int *pPort)
{
    *pPort = pjsua_recorder_get_conf_port(recorder_id);
    return S_OK;
}

STDMETHODIMP CApp::recorder_destroy(int recorder_id, Pj_Status *pRet)
{
    *pRet = pjsua_recorder_destroy(recorder_id);
    return S_OK;
}

STDMETHODIMP CApp::app_load_config(Pj_String filename, Pjsua_Config *pConfig, Pj_Status *pRet)
{
    pjsua_config config;
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), filename);
    pjsua_default_config(&config);
    *pRet = pjsua_load_settings(tmp.ptr, &config);
    if (*pRet == PJ_SUCCESS)
	*pRet = config2Config(&config, pConfig);
    return S_OK;
}

STDMETHODIMP CApp::app_save_config(Pj_String filename, Pjsua_Config *pConfig, Pj_Status *pRet)
{
    Temp_Pool tp;
    pjsua_config config;
    pj_str_t tmp = Pj_str(tp.get_pool(), filename);
    HRESULT hr;

    hr = Config2config(tp.get_pool(), pConfig, &config);
    if (FAILED(hr))
	return hr;

    *pRet = pjsua_save_settings(tmp.ptr, &config);
    return S_OK;
}

STDMETHODIMP CApp::app_get_current_config(Pjsua_Config *pConfig)
{
    pjsua_config *config;
    config = (pjsua_config*) pjsua_get_config();
    return config2Config(config, pConfig);
}

STDMETHODIMP CApp::app_get_error_msg(Pj_Status status, BSTR * pRet)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    OLECHAR werrmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    pj_ansi_to_unicode(errmsg, strlen(errmsg), werrmsg, PJ_ARRAY_SIZE(werrmsg));
    *pRet = SysAllocString(werrmsg);
    return S_OK;
}

STDMETHODIMP CApp::app_verify_sip_url(Pj_String uri, Pj_Status *pRet)
{
    Temp_Pool tp;
    pj_str_t tmp = Pj_str(tp.get_pool(), uri);
    *pRet = pjsua_verify_sip_url(tmp.ptr);
    return S_OK;
}

STDMETHODIMP CApp::app_handle_events(int msec_timeout, int *pEvCount)
{
    if (msec_timeout < 0)
	msec_timeout = 0;

    *pEvCount = pjsua_handle_events(msec_timeout);
    return S_OK;
}
