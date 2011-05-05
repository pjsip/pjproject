/*
========================================================================
 Name        : Symbian_ua_guiSettingItemListSettings.h
 Author      : nanang
 Copyright   : (c) PJSIP 2008
 Description : 
========================================================================
*/
#ifndef SYMBIAN_UA_GUISETTINGITEMLISTSETTINGS_H
#define SYMBIAN_UA_GUISETTINGITEMLISTSETTINGS_H
			
// [[[ begin generated region: do not modify [Generated Includes]
#include <e32std.h>
// ]]] end generated region [Generated Includes]

// [[[ begin generated region: do not modify [Generated Constants]
const int KEd_registrarMaxLength = 255;
const int KEd_userMaxLength = 255;
const int KEd_passwordMaxLength = 32;
const int KEd_stun_serverMaxLength = 255;
// ]]] end generated region [Generated Constants]

/**
 * @class	TSymbian_ua_guiSettingItemListSettings Symbian_ua_guiSettingItemListSettings.h
 */
class TSymbian_ua_guiSettingItemListSettings
	{
public:
	// construct and destroy
	static TSymbian_ua_guiSettingItemListSettings* NewL();
	void ConstructL();
		
private:
	// constructor
	TSymbian_ua_guiSettingItemListSettings();
	// [[[ begin generated region: do not modify [Generated Accessors]
public:
	TDes& Ed_registrar();
	void SetEd_registrar(const TDesC& aValue);
	TDes& Ed_user();
	void SetEd_user(const TDesC& aValue);
	TDes& Ed_password();
	void SetEd_password(const TDesC& aValue);
	TBool& B_srtp();
	void SetB_srtp(const TBool& aValue);
	TBool& B_ice();
	void SetB_ice(const TBool& aValue);
	TDes& Ed_stun_server();
	void SetEd_stun_server(const TDesC& aValue);
	// ]]] end generated region [Generated Accessors]
	
	// [[[ begin generated region: do not modify [Generated Members]
protected:
	TBuf<KEd_registrarMaxLength> iEd_registrar;
	TBuf<KEd_userMaxLength> iEd_user;
	TBuf<KEd_passwordMaxLength> iEd_password;
	TBool iB_srtp;
	TBool iB_ice;
	TBuf<KEd_stun_serverMaxLength> iEd_stun_server;
	// ]]] end generated region [Generated Members]
	
	};
#endif // SYMBIAN_UA_GUISETTINGITEMLISTSETTINGS_H
