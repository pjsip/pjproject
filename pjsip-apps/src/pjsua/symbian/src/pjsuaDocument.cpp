/*
========================================================================
 Name        : pjsuaDocument.cpp
 Author      : nanang
 Copyright   : Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 Description : 
========================================================================
*/
// [[[ begin generated region: do not modify [Generated User Includes]
#include "pjsuaDocument.h"
#include "pjsuaAppUi.h"
// ]]] end generated region [Generated User Includes]

/**
 * @brief Constructs the document class for the application.
 * @param anApplication the application instance
 */
CpjsuaDocument::CpjsuaDocument( CEikApplication& anApplication )
	: CAknDocument( anApplication )
	{
	}

/**
 * @brief Completes the second phase of Symbian object construction. 
 * Put initialization code that could leave here.  
 */ 
void CpjsuaDocument::ConstructL()
	{
	}
	
/**
 * Symbian OS two-phase constructor.
 *
 * Creates an instance of CpjsuaDocument, constructs it, and
 * returns it.
 *
 * @param aApp the application instance
 * @return the new CpjsuaDocument
 */
CpjsuaDocument* CpjsuaDocument::NewL( CEikApplication& aApp )
	{
	CpjsuaDocument* self = new ( ELeave ) CpjsuaDocument( aApp );
	CleanupStack::PushL( self );
	self->ConstructL();
	CleanupStack::Pop( self );
	return self;
	}

/**
 * @brief Creates the application UI object for this document.
 * @return the new instance
 */	
CEikAppUi* CpjsuaDocument::CreateAppUiL()
	{
	return new ( ELeave ) CpjsuaAppUi;
	}
				
