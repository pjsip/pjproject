/*
========================================================================
 Name        : pjsuaDocument.h
 Author      : nanang
 Copyright   : Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 Description : 
========================================================================
*/
#ifndef PJSUADOCUMENT_H
#define PJSUADOCUMENT_H

#include <akndoc.h>
		
class CEikAppUi;

/**
* @class	CpjsuaDocument pjsuaDocument.h
* @brief	A CAknDocument-derived class is required by the S60 application 
*           framework. It is responsible for creating the AppUi object. 
*/
class CpjsuaDocument : public CAknDocument
	{
public: 
	// constructor
	static CpjsuaDocument* NewL( CEikApplication& aApp );

private: 
	// constructors
	CpjsuaDocument( CEikApplication& aApp );
	void ConstructL();
	
public: 
	// from base class CEikDocument
	CEikAppUi* CreateAppUiL();
	};
#endif // PJSUADOCUMENT_H
