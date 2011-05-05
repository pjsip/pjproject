/*
========================================================================
 Name        : symbian_ua_guiDocument.h
 Author      : nanang
 Copyright   : (c) PJSIP 2008
 Description : 
========================================================================
*/
#ifndef SYMBIAN_UA_GUIDOCUMENT_H
#define SYMBIAN_UA_GUIDOCUMENT_H

#include <akndoc.h>
		
class CEikAppUi;

/**
* @class	Csymbian_ua_guiDocument symbian_ua_guiDocument.h
* @brief	A CAknDocument-derived class is required by the S60 application 
*           framework. It is responsible for creating the AppUi object. 
*/
class Csymbian_ua_guiDocument : public CAknDocument
	{
public: 
	// constructor
	static Csymbian_ua_guiDocument* NewL( CEikApplication& aApp );

private: 
	// constructors
	Csymbian_ua_guiDocument( CEikApplication& aApp );
	void ConstructL();
	
public: 
	// from base class CEikDocument
	CEikAppUi* CreateAppUiL();
	};
#endif // SYMBIAN_UA_GUIDOCUMENT_H
