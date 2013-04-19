/*
========================================================================
 Name        : pjsuaApplication.h
 Author      : nanang
 Copyright   : Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 Description : 
========================================================================
*/
#ifndef PJSUAAPPLICATION_H
#define PJSUAAPPLICATION_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknapp.h>
// ]]] end generated region [Generated Includes]

// [[[ begin generated region: do not modify [Generated Constants]
const TUid KUidpjsuaApplication = { 0xE44C2D02 };
// ]]] end generated region [Generated Constants]

/**
 *
 * @class	CpjsuaApplication pjsuaApplication.h
 * @brief	A CAknApplication-derived class is required by the S60 application 
 *          framework. It is subclassed to create the application's document 
 *          object.
 */
class CpjsuaApplication : public CAknApplication
	{
private:
	TUid AppDllUid() const;
	CApaDocument* CreateDocumentL();
	
	};
			
#endif // PJSUAAPPLICATION_H		
