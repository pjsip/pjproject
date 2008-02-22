// SettingsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "PocketPJ.h"
#include "SettingsDlg.h"
#include <atlbase.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define REG_PATH	_T("pjsip.org\\PocketPC")
#define REG_DOMAIN	_T("Domain")
#define REG_USER	_T("User")
#define REG_PASSWD	_T("Data")
#define REG_USE_STUN	_T("UseSTUN")
#define REG_STUN_SRV	_T("STUNSrv")
#define REG_DNS		_T("DNS")
#define REG_USE_ICE	_T("UseICE")
#define REG_USE_SRTP	_T("UseSRTP")
#define REG_USE_PUBLISH	_T("UsePUBLISH")
#define REG_BUDDY_CNT	_T("BuddyCnt")
#define REG_BUDDY_X	_T("Buddy%u")


/////////////////////////////////////////////////////////////////////////////
// Settings

// Load from registry
void CPocketPJSettings::LoadRegistry()
{
    CRegKey key;
    wchar_t textVal[256];
    DWORD dwordVal;
    DWORD cbData;


    if (key.Open(HKEY_CURRENT_USER, REG_PATH) != ERROR_SUCCESS)
	return;

    cbData = sizeof(textVal);
    if (key.QueryValue(textVal, REG_DOMAIN, &cbData) == ERROR_SUCCESS) {
	m_Domain = textVal;
    }

    cbData = sizeof(textVal);
    if (key.QueryValue(textVal, REG_USER, &cbData) == ERROR_SUCCESS) {
	m_User = textVal;
    }

    cbData = sizeof(textVal);
    if (key.QueryValue(textVal, REG_PASSWD, &cbData) == ERROR_SUCCESS) {
	m_Password = textVal;
    }

    cbData = sizeof(textVal);
    if (key.QueryValue(textVal, REG_STUN_SRV, &cbData) == ERROR_SUCCESS) {
	m_StunSrv = textVal;
    }

    cbData = sizeof(textVal);
    if (key.QueryValue(textVal, REG_DNS, &cbData) == ERROR_SUCCESS) {
	m_DNS = textVal;
    }

    dwordVal = 0;
    if (key.QueryValue(dwordVal, REG_USE_STUN) == ERROR_SUCCESS) {
	m_UseStun = dwordVal != 0;
    }

    if (key.QueryValue(dwordVal, REG_USE_ICE) == ERROR_SUCCESS) {
	m_UseIce = dwordVal != 0;
    }


    if (key.QueryValue(dwordVal, REG_USE_SRTP) == ERROR_SUCCESS) {
	m_UseSrtp = dwordVal != 0;
    }


    cbData = sizeof(dwordVal);
    if (key.QueryValue(dwordVal, REG_USE_PUBLISH) == ERROR_SUCCESS) {
	m_UsePublish = dwordVal != 0;
    }

    m_BuddyList.RemoveAll();

    DWORD buddyCount = 0;
    cbData = sizeof(dwordVal);
    if (key.QueryValue(dwordVal, REG_BUDDY_CNT) == ERROR_SUCCESS) {
	buddyCount = dwordVal;
    }

    unsigned i;
    for (i=0; i<buddyCount; ++i) {
	CString entry;
	entry.Format(REG_BUDDY_X, i);

	cbData = sizeof(textVal);
	if (key.QueryValue(textVal, entry, &cbData) == ERROR_SUCCESS) {
	    m_BuddyList.Add(textVal);
	}
    }

    key.Close();
}

// Save to registry
void CPocketPJSettings::SaveRegistry()
{
    CRegKey key;

    if (key.Create(HKEY_CURRENT_USER, REG_PATH) != ERROR_SUCCESS)
	return;

    key.SetValue(m_Domain, REG_DOMAIN);
    key.SetValue(m_User, REG_USER);
    key.SetValue(m_Password, REG_PASSWD);
    key.SetValue(m_StunSrv, REG_STUN_SRV);
    key.SetValue(m_DNS, REG_DNS);
    
    key.SetValue(m_UseStun, REG_USE_STUN);
    key.SetValue(m_UseIce, REG_USE_ICE);
    key.SetValue(m_UseSrtp, REG_USE_SRTP);
    key.SetValue(m_UsePublish, REG_USE_PUBLISH);

    key.SetValue(m_BuddyList.GetSize(), REG_BUDDY_CNT);

    unsigned i;
    for (i=0; i<m_BuddyList.GetSize(); ++i) {
	CString entry;
	entry.Format(REG_BUDDY_X, i);
	key.SetValue(m_BuddyList.GetAt(i), entry);
    }

    key.Close();
}


/////////////////////////////////////////////////////////////////////////////
// CSettingsDlg dialog


CSettingsDlg::CSettingsDlg(CPocketPJSettings &cfg, CWnd* pParent)
	: CDialog(CSettingsDlg::IDD, pParent), m_Cfg(cfg)
{
	//{{AFX_DATA_INIT(CSettingsDlg)
	m_Domain = _T("");
	m_ICE = FALSE;
	m_Passwd = _T("");
	m_PUBLISH = FALSE;
	m_SRTP = FALSE;
	m_STUN = FALSE;
	m_StunSrv = _T("");
	m_User = _T("");
	m_Dns = _T("");
	//}}AFX_DATA_INIT

	m_Domain    = m_Cfg.m_Domain;
	m_ICE	    = m_Cfg.m_UseIce;
	m_Passwd    = m_Cfg.m_Password;
	m_PUBLISH   = m_Cfg.m_UsePublish;
	m_SRTP	    = m_Cfg.m_UseSrtp;
	m_STUN	    = m_Cfg.m_UseStun;
	m_StunSrv   = m_Cfg.m_StunSrv;
	m_User	    = m_Cfg.m_User;
	m_Dns	    = m_Cfg.m_DNS;
}


void CSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSettingsDlg)
	DDX_Text(pDX, IDC_DOMAIN, m_Domain);
	DDX_Check(pDX, IDC_ICE, m_ICE);
	DDX_Text(pDX, IDC_PASSWD, m_Passwd);
	DDX_Check(pDX, IDC_PUBLISH, m_PUBLISH);
	DDX_Check(pDX, IDC_SRTP, m_SRTP);
	DDX_Check(pDX, IDC_STUN, m_STUN);
	DDX_Text(pDX, IDC_STUN_SRV, m_StunSrv);
	DDX_Text(pDX, IDC_USER, m_User);
	DDX_Text(pDX, IDC_DNS, m_Dns);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSettingsDlg, CDialog)
	//{{AFX_MSG_MAP(CSettingsDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSettingsDlg message handlers

int CSettingsDlg::DoModal() 
{
    int rc = CDialog::DoModal();	

    if (rc == IDOK) {
	m_Cfg.m_Domain	    = m_Domain;
	m_Cfg.m_UseIce	    = m_ICE;
	m_Cfg.m_Password    = m_Passwd;
	m_Cfg.m_UsePublish  = m_PUBLISH;
	m_Cfg.m_UseSrtp	    = m_SRTP;
	m_Cfg.m_UseStun	    = m_STUN;
	m_Cfg.m_StunSrv	    = m_StunSrv;
	m_Cfg.m_User	    = m_User;
	m_Cfg.m_DNS	    = m_Dns;
    }

    return rc;
}
