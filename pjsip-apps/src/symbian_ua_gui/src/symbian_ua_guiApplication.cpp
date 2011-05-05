/*
========================================================================
 Name        : symbian_ua_guiApplication.cpp
 Author      : nanang
 Copyright   : (c) 2008-2009 Teluu Inc.
 Description : 
========================================================================
*/
// [[[ begin generated region: do not modify [Generated System Includes]
// ]]] end generated region [Generated System Includes]

// [[[ begin generated region: do not modify [Generated Includes]
#include "symbian_ua_guiApplication.h"
#include "symbian_ua_guiDocument.h"
#ifdef EKA2
#include <eikstart.h>
#endif
// ]]] end generated region [Generated Includes]


// Needed by APS
TPtrC APP_UID = _L("EBD12EE4");

/**
 * @brief Returns the application's UID (override from CApaApplication::AppDllUid())
 * @return UID for this application (KUidsymbian_ua_guiApplication)
 */
TUid Csymbian_ua_guiApplication::AppDllUid() const
	{
	return KUidsymbian_ua_guiApplication;
	}

/**
 * @brief Creates the application's document (override from CApaApplication::CreateDocumentL())
 * @return Pointer to the created document object (Csymbian_ua_guiDocument)
 */
CApaDocument* Csymbian_ua_guiApplication::CreateDocumentL()
	{
	return Csymbian_ua_guiDocument::NewL( *this );
	}

#ifdef EKA2

/**
 *	@brief Called by the application framework to construct the application object
 *  @return The application (Csymbian_ua_guiApplication)
 */	
LOCAL_C CApaApplication* NewApplication()
	{
	return new Csymbian_ua_guiApplication;
	}

/**
* @brief This standard export is the entry point for all Series 60 applications
* @return error code
 */	
GLDEF_C TInt E32Main()
	{
	TInt err;
	
	err = EikStart::RunApplication( NewApplication );

	return err;
	}
	
#else 	// Series 60 2.x main DLL program code

/**
* @brief This standard export constructs the application object.
* @return The application (Csymbian_ua_guiApplication)
*/
EXPORT_C CApaApplication* NewApplication()
	{
	return new Csymbian_ua_guiApplication;
	}

/**
* @brief This standard export is the entry point for all Series 60 applications
* @return error code
*/
GLDEF_C TInt E32Dll(TDllReason /*reason*/)
	{
	return KErrNone;
	}

#endif // EKA2
