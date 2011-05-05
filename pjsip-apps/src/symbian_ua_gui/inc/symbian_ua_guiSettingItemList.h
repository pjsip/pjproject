/*
========================================================================
 Name        : symbian_ua_guiSettingItemList.h
 Author      : nanang
 Copyright   : (c) PJSIP 2008
 Description : 
========================================================================
*/
#ifndef SYMBIAN_UA_GUISETTINGITEMLIST_H
#define SYMBIAN_UA_GUISETTINGITEMLIST_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknsettingitemlist.h>
// ]]] end generated region [Generated Includes]


// [[[ begin [Event Handler Includes]
// ]]] end [Event Handler Includes]

// [[[ begin generated region: do not modify [Generated Forward Declarations]
class MEikCommandObserver;
class TSymbian_ua_guiSettingItemListSettings;
// ]]] end generated region [Generated Forward Declarations]

/**
 * @class	CSymbian_ua_guiSettingItemList symbian_ua_guiSettingItemList.h
 */
class CSymbian_ua_guiSettingItemList : public CAknSettingItemList
	{
public: // constructors and destructor

	CSymbian_ua_guiSettingItemList( 
			TSymbian_ua_guiSettingItemListSettings& settings, 
			MEikCommandObserver* aCommandObserver );
	virtual ~CSymbian_ua_guiSettingItemList();

public:

	// from CCoeControl
	void HandleResourceChange( TInt aType );

	// overrides of CAknSettingItemList
	CAknSettingItem* CreateSettingItemL( TInt id );
	void EditItemL( TInt aIndex, TBool aCalledFromMenu );
	TKeyResponse OfferKeyEventL( 
			const TKeyEvent& aKeyEvent, 
			TEventCode aType );

public:
	// utility function for menu
	void ChangeSelectedItemL();

	void LoadSettingValuesL();
	void SaveSettingValuesL();
		
private:
	// override of CAknSettingItemList
	void SizeChanged();

private:
	// current settings values
	TSymbian_ua_guiSettingItemListSettings& iSettings;
	MEikCommandObserver* iCommandObserver;
	// [[[ begin generated region: do not modify [Generated Methods]
public: 
	// ]]] end generated region [Generated Methods]
	
	// [[[ begin generated region: do not modify [Generated Type Declarations]
public: 
	// ]]] end generated region [Generated Type Declarations]
	
	// [[[ begin generated region: do not modify [Generated Instance Variables]
private: 
	// ]]] end generated region [Generated Instance Variables]
	
	
	// [[[ begin [Overridden Methods]
protected: 
	// ]]] end [Overridden Methods]
	
	
	// [[[ begin [User Handlers]
protected: 
	// ]]] end [User Handlers]
	
	};
#endif // SYMBIAN_UA_GUISETTINGITEMLIST_H
