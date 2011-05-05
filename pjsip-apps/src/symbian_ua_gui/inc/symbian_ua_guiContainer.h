/*
========================================================================
 Name        : symbian_ua_guiContainer.h
 Author      : nanang
 Copyright   : (c) PJSIP 2008
 Description : 
========================================================================
*/
#ifndef SYMBIAN_UA_GUICONTAINER_H
#define SYMBIAN_UA_GUICONTAINER_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <coecntrl.h>		
// ]]] end generated region [Generated Includes]


// [[[ begin [Event Handler Includes]
// ]]] end [Event Handler Includes]

// [[[ begin generated region: do not modify [Generated Forward Declarations]
class MEikCommandObserver;		
class CEikLabel;
class CEikEdwin;
// ]]] end generated region [Generated Forward Declarations]

/**
 * Container class for symbian_ua_guiContainer
 * 
 * @class	CSymbian_ua_guiContainer symbian_ua_guiContainer.h
 */
class CSymbian_ua_guiContainer : public CCoeControl
	{
public:
	// constructors and destructor
	CSymbian_ua_guiContainer();
	static CSymbian_ua_guiContainer* NewL( 
		const TRect& aRect, 
		const CCoeControl* aParent, 
		MEikCommandObserver* aCommandObserver );
	static CSymbian_ua_guiContainer* NewLC( 
		const TRect& aRect, 
		const CCoeControl* aParent, 
		MEikCommandObserver* aCommandObserver );
	void ConstructL( 
		const TRect& aRect, 
		const CCoeControl* aParent, 
		MEikCommandObserver* aCommandObserver );
	virtual ~CSymbian_ua_guiContainer();

public:
	// from base class CCoeControl
	TInt CountComponentControls() const;
	CCoeControl* ComponentControl( TInt aIndex ) const;
	TKeyResponse OfferKeyEventL( 
			const TKeyEvent& aKeyEvent, 
			TEventCode aType );
	void HandleResourceChange( TInt aType );
	
protected:
	// from base class CCoeControl
	void SizeChanged();

private:
	// from base class CCoeControl
	void Draw( const TRect& aRect ) const;

private:
	void InitializeControlsL();
	void LayoutControls();
	CCoeControl* iFocusControl;
	MEikCommandObserver* iCommandObserver;
	// [[[ begin generated region: do not modify [Generated Methods]
public: 
	// ]]] end generated region [Generated Methods]
	
	// [[[ begin generated region: do not modify [Generated Type Declarations]
public: 
	// ]]] end generated region [Generated Type Declarations]
	
	// [[[ begin generated region: do not modify [Generated Instance Variables]
private: 
	CEikLabel* iLabel1;
	CEikEdwin* iEd_url;
	CEikEdwin* iEd_info;
	// ]]] end generated region [Generated Instance Variables]
	
	
	// [[[ begin [Overridden Methods]
protected: 
	// ]]] end [Overridden Methods]
	
	
	// [[[ begin [User Handlers]
protected: 
	// ]]] end [User Handlers]
	
public: 
	enum TControls
		{
		// [[[ begin generated region: do not modify [Generated Contents]
		ELabel1,
		EEd_url,
		EEd_info,
		
		// ]]] end generated region [Generated Contents]
		
		// add any user-defined entries here...
		
		ELastControl
		};
	};
				
#endif // SYMBIAN_UA_GUICONTAINER_H
