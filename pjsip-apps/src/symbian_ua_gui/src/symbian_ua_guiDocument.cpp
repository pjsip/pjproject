/*
========================================================================
 Name        : symbian_ua_guiDocument.cpp
 Author      : nanang
 Copyright   : (c) 2008-2009 Teluu Inc.
 Description : 
========================================================================
*/
// [[[ begin generated region: do not modify [Generated User Includes]
#include "symbian_ua_guiDocument.h"
#include "symbian_ua_guiAppUi.h"
// ]]] end generated region [Generated User Includes]

/**
 * @brief Constructs the document class for the application.
 * @param anApplication the application instance
 */
Csymbian_ua_guiDocument::Csymbian_ua_guiDocument( CEikApplication& anApplication )
	: CAknDocument( anApplication )
	{
	}

/**
 * @brief Completes the second phase of Symbian object construction. 
 * Put initialization code that could leave here.  
 */ 
void Csymbian_ua_guiDocument::ConstructL()
	{
	}
	
/**
 * Symbian OS two-phase constructor.
 *
 * Creates an instance of Csymbian_ua_guiDocument, constructs it, and
 * returns it.
 *
 * @param aApp the application instance
 * @return the new Csymbian_ua_guiDocument
 */
Csymbian_ua_guiDocument* Csymbian_ua_guiDocument::NewL( CEikApplication& aApp )
	{
	Csymbian_ua_guiDocument* self = new ( ELeave ) Csymbian_ua_guiDocument( aApp );
	CleanupStack::PushL( self );
	self->ConstructL();
	CleanupStack::Pop( self );
	return self;
	}

/**
 * @brief Creates the application UI object for this document.
 * @return the new instance
 */	
CEikAppUi* Csymbian_ua_guiDocument::CreateAppUiL()
	{
	return new ( ELeave ) Csymbian_ua_guiAppUi;
	}
				
