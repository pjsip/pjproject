/*
========================================================================
 Name        : symbian_ua_guiAppUi.h
 Author      : nanang
 Copyright   : (c) PJSIP 2008
 Description : 
========================================================================
*/
#ifndef SYMBIAN_UA_GUIAPPUI_H
#define SYMBIAN_UA_GUIAPPUI_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknviewappui.h>
#include <aknwaitdialog.h>
// ]]] end generated region [Generated Includes]

// [[[ begin generated region: do not modify [Generated Forward Declarations]
class Csymbian_ua_guiContainerView;
class Csymbian_ua_guiSettingItemListView;
// ]]] end generated region [Generated Forward Declarations]

/**
 * @class	Csymbian_ua_guiAppUi symbian_ua_guiAppUi.h
 * @brief The AppUi class handles application-wide aspects of the user interface, including
 *        view management and the default menu, control pane, and status pane.
 */
class Csymbian_ua_guiAppUi : public CAknViewAppUi, public CTimer
	{
public: 
	// constructor and destructor
	Csymbian_ua_guiAppUi();
	virtual ~Csymbian_ua_guiAppUi();
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
	void ExecuteDlg_wait_initLD( const TDesC* aOverrideText = NULL );
	void RemoveDlg_wait_initL();
	// ]]] end generated region [Generated Methods]
	
	// [[[ begin generated region: do not modify [Generated Instance Variables]
private: 
	CAknWaitDialog* iDlg_wait_init;
	class CProgressDialogCallback;
	CProgressDialogCallback* iDlg_wait_initCallback;
	Csymbian_ua_guiContainerView* iSymbian_ua_guiContainerView;
	Csymbian_ua_guiSettingItemListView* iSymbian_ua_guiSettingItemListView;
	// ]]] end generated region [Generated Instance Variables]
	
	
	// [[[ begin [User Handlers]
protected: 
	void HandleSymbian_ua_guiAppUiApplicationSpecificEventL( 
			TInt aType, 
			const TWsEvent& anEvent );
	void HandleDlg_wait_initCanceledL( CAknProgressDialog* aDialog );
	// ]]] end [User Handlers]
	
	
	// [[[ begin [Overridden Methods]
protected: 
	void HandleApplicationSpecificEventL( 
			TInt aType, 
			const TWsEvent& anEvent );
	// ]]] end [Overridden Methods]
	
	virtual void RunL();
	
	// [[[ begin [MProgressDialogCallback support]
private: 
	typedef void ( Csymbian_ua_guiAppUi::*ProgressDialogEventHandler )( 
			CAknProgressDialog* aProgressDialog );
	
	/**
	 * This is a helper class for progress/wait dialog callbacks. It routes the dialog's
	 * cancel notification to the handler function for the cancel event.
	 */
	class CProgressDialogCallback : public CBase, public MProgressDialogCallback
		{ 
		public:
			CProgressDialogCallback( 
					Csymbian_ua_guiAppUi* aHandlerObj, 
					CAknProgressDialog* aDialog, 
					ProgressDialogEventHandler aHandler ) :
				handlerObj( aHandlerObj ), dialog( aDialog ), handler( aHandler )
				{}
				
			void DialogDismissedL( TInt aButtonId ) 
				{
				( handlerObj->*handler )( dialog );
				}
		private:
			Csymbian_ua_guiAppUi* handlerObj;
			CAknProgressDialog* dialog;
			ProgressDialogEventHandler handler;
		};
		
	// ]]] end [MProgressDialogCallback support]
	
	};

#endif // SYMBIAN_UA_GUIAPPUI_H			
