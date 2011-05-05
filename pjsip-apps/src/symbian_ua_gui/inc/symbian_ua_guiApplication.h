/*
========================================================================
 Name        : symbian_ua_guiApplication.h
 Author      : nanang
 Copyright   : (c) PJSIP 2008
 Description : 
========================================================================
*/
#ifndef SYMBIAN_UA_GUIAPPLICATION_H
#define SYMBIAN_UA_GUIAPPLICATION_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknapp.h>
// ]]] end generated region [Generated Includes]

// [[[ begin generated region: do not modify [Generated Constants]
const TUid KUidsymbian_ua_guiApplication = { 0xEBD12EE4 };
// ]]] end generated region [Generated Constants]

/**
 *
 * @class	Csymbian_ua_guiApplication symbian_ua_guiApplication.h
 * @brief	A CAknApplication-derived class is required by the S60 application 
 *          framework. It is subclassed to create the application's document 
 *          object.
 */
class Csymbian_ua_guiApplication : public CAknApplication
	{
private:
	TUid AppDllUid() const;
	CApaDocument* CreateDocumentL();
	
	};
			
#endif // SYMBIAN_UA_GUIAPPLICATION_H		
