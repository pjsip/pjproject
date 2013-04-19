/*
========================================================================
 Name        : pjsuaAppUi.h
 Author      : nanang
 Copyright   : Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 Description : 
========================================================================
*/
#ifndef PJSUAAPPUI_H
#define PJSUAAPPUI_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknviewappui.h>
// ]]] end generated region [Generated Includes]


// [[[ begin generated region: do not modify [Generated Forward Declarations]
class CpjsuaContainerView;
// ]]] end generated region [Generated Forward Declarations]

/**
 * @class	CpjsuaAppUi pjsuaAppUi.h
 * @brief The AppUi class handles application-wide aspects of the user interface, including
 *        view management and the default menu, control pane, and status pane.
 */
class CpjsuaAppUi : public CAknViewAppUi
	{
public: 
	// constructor and destructor
	CpjsuaAppUi();
	virtual ~CpjsuaAppUi();
	void ConstructL();

public:
	// from CCoeAppUi
	TKeyResponse HandleKeyEventL(
				const TKeyEvent& aKeyEvent,
				TEventCode aType );

	// from CEikAppUi
	void HandleCommandL( TInt aCommand );
	void HandleResourceChangeL( TInt aType );

	// from CAknAppUi
	void HandleViewDeactivation( 
			const TVwsViewId& aViewIdToBeDeactivated, 
			const TVwsViewId& aNewlyActivatedViewId );

private:
	void InitializeContainersL();
	// [[[ begin generated region: do not modify [Generated Methods]
public: 
	// ]]] end generated region [Generated Methods]
	
	void PutMsg(const char *msg);
	
	// [[[ begin generated region: do not modify [Generated Instance Variables]
private: 
	CpjsuaContainerView* iPjsuaContainerView;
	// ]]] end generated region [Generated Instance Variables]
	
	
	// [[[ begin [User Handlers]
protected: 
	// ]]] end [User Handlers]
	void PrepareToExit();

	};

#endif // PJSUAAPPUI_H			
