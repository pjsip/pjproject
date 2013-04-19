/*
========================================================================
 Name        : pjsuaContainer.cpp
 Author      : nanang
 Copyright   : Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 Description : 
========================================================================
*/
// [[[ begin generated region: do not modify [Generated System Includes]
#include <barsread.h>
#include <eikimage.h>
#include <eikenv.h>
#include <stringloader.h>
#include <eiklabel.h>
#include <aknviewappui.h>
#include <eikappui.h>
#include <akniconutils.h>
#include <pjsua.mbg>
#include <pjsua.rsg>
// ]]] end generated region [Generated System Includes]

// [[[ begin generated region: do not modify [Generated User Includes]
#include "pjsuaContainer.h"
#include "pjsuaContainerView.h"
#include "pjsua.hrh"
// ]]] end generated region [Generated User Includes]

#include <eikmenub.h>

// [[[ begin generated region: do not modify [Generated Constants]
_LIT( KpjsuaFile, "\\resource\\apps\\pjsua.mbm" );
// ]]] end generated region [Generated Constants]

/**
 * First phase of Symbian two-phase construction. Should not 
 * contain any code that could leave.
 */
CPjsuaContainer::CPjsuaContainer()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	iImage1 = NULL;
	iLabel1 = NULL;
	// ]]] end generated region [Generated Contents]
	
	}
/** 
 * Destroy child controls.
 */
CPjsuaContainer::~CPjsuaContainer()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	delete iImage1;
	iImage1 = NULL;
	delete iLabel1;
	iLabel1 = NULL;
	// ]]] end generated region [Generated Contents]
	
	}
				
/**
 * Construct the control (first phase).
 *  Creates an instance and initializes it.
 *  Instance is not left on cleanup stack.
 * @param aRect bounding rectangle
 * @param aParent owning parent, or NULL
 * @param aCommandObserver command observer
 * @return initialized instance of CPjsuaContainer
 */
CPjsuaContainer* CPjsuaContainer::NewL( 
		const TRect& aRect, 
		const CCoeControl* aParent, 
		MEikCommandObserver* aCommandObserver )
	{
	CPjsuaContainer* self = CPjsuaContainer::NewLC( 
			aRect, 
			aParent, 
			aCommandObserver );
	CleanupStack::Pop( self );
	return self;
	}

/**
 * Construct the control (first phase).
 *  Creates an instance and initializes it.
 *  Instance is left on cleanup stack.
 * @param aRect The rectangle for this window
 * @param aParent owning parent, or NULL
 * @param aCommandObserver command observer
 * @return new instance of CPjsuaContainer
 */
CPjsuaContainer* CPjsuaContainer::NewLC( 
		const TRect& aRect, 
		const CCoeControl* aParent, 
		MEikCommandObserver* aCommandObserver )
	{
	CPjsuaContainer* self = new ( ELeave ) CPjsuaContainer();
	CleanupStack::PushL( self );
	self->ConstructL( aRect, aParent, aCommandObserver );
	return self;
	}
			
/**
 * Construct the control (second phase).
 *  Creates a window to contain the controls and activates it.
 * @param aRect bounding rectangle
 * @param aCommandObserver command observer
 * @param aParent owning parent, or NULL
 */ 
void CPjsuaContainer::ConstructL( 
		const TRect& aRect, 
		const CCoeControl* aParent, 
		MEikCommandObserver* aCommandObserver )
	{
	if ( aParent == NULL )
	    {
		CreateWindowL();
	    }
	else
	    {
	    SetContainerWindowL( *aParent );
	    }
	iFocusControl = NULL;
	iCommandObserver = aCommandObserver;
	InitializeControlsL();
	SetRect( aRect );

	// Full screen
	SetExtentToWholeScreen();
	
	// Set label color
	//iLabel1->OverrideColorL( EColorLabelText, KRgbWhite );
	//iLabel1->OverrideColorL(EColorControlBackground, KRgbBlack )
	iLabel1->SetEmphasis( CEikLabel::EFullEmphasis);
	iLabel1->OverrideColorL( EColorLabelHighlightFullEmphasis, KRgbBlack );
	iLabel1->OverrideColorL( EColorLabelTextEmphasis, KRgbWhite );

	// Set label font
	 CFont* fontUsed;
	_LIT(f,"Arial");
	TFontSpec* fontSpec = new TFontSpec(f, 105);
	TFontStyle* fontStyle = new TFontStyle();
	fontStyle->SetPosture(EPostureUpright);
	fontStyle->SetStrokeWeight(EStrokeWeightNormal);
	fontSpec->iFontStyle = *fontStyle;
	fontUsed = iCoeEnv->CreateScreenFontL(*fontSpec);
	iLabel1->SetFont(fontUsed);
	iLabel1->SetAlignment( EHCenterVCenter );
	
	ActivateL();
	// [[[ begin generated region: do not modify [Post-ActivateL initializations]
	// ]]] end generated region [Post-ActivateL initializations]
	
	}
			
/**
* Return the number of controls in the container (override)
* @return count
*/
TInt CPjsuaContainer::CountComponentControls() const
	{
	return ( int ) ELastControl;
	}
				
/**
* Get the control with the given index (override)
* @param aIndex Control index [0...n) (limited by #CountComponentControls)
* @return Pointer to control
*/
CCoeControl* CPjsuaContainer::ComponentControl( TInt aIndex ) const
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	switch ( aIndex )
		{
		case EImage1:
			return iImage1;
		case ELabel1:
			return iLabel1;
		}
	// ]]] end generated region [Generated Contents]
	
	// handle any user controls here...
	
	return NULL;
	}
				
/**
 *	Handle resizing of the container. This implementation will lay out
 *  full-sized controls like list boxes for any screen size, and will layout
 *  labels, editors, etc. to the size they were given in the UI designer.
 *  This code will need to be modified to adjust arbitrary controls to
 *  any screen size.
 */				
void CPjsuaContainer::SizeChanged()
	{
	CCoeControl::SizeChanged();
	LayoutControls();

	// Align the image
	int x = (Size().iWidth - iImage1->Size().iWidth) / 2;
	int y = (Size().iHeight - iImage1->Size().iHeight) / 2;
	iImage1->SetPosition(TPoint(x, y));
	
	// Align the label
	iLabel1->SetExtent(TPoint(0, Size().iHeight - iLabel1->Size().iHeight),
			   TSize(Size().iWidth, iLabel1->Size().iHeight));
	
	// [[[ begin generated region: do not modify [Generated Contents]
			
	// ]]] end generated region [Generated Contents]
	
	}
				
// [[[ begin generated function: do not modify
/**
 * Layout components as specified in the UI Designer
 */
void CPjsuaContainer::LayoutControls()
	{
	iImage1->SetExtent( TPoint( 0, 0 ), TSize( 99, 111 ) );
	iLabel1->SetExtent( TPoint( 0, 196 ), TSize( 241, 27 ) );
	}
// ]]] end generated function

/**
 *	Handle key events.
 */				
TKeyResponse CPjsuaContainer::OfferKeyEventL( 
		const TKeyEvent& aKeyEvent, 
		TEventCode aType )
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	
	// ]]] end generated region [Generated Contents]
	
	if ( iFocusControl != NULL
		&& iFocusControl->OfferKeyEventL( aKeyEvent, aType ) == EKeyWasConsumed )
		{
		return EKeyWasConsumed;
		}
	return CCoeControl::OfferKeyEventL( aKeyEvent, aType );
	}
				
// [[[ begin generated function: do not modify
/**
 *	Initialize each control upon creation.
 */				
void CPjsuaContainer::InitializeControlsL()
	{
	iImage1 = new ( ELeave ) CEikImage;
		{
		CFbsBitmap *bitmap, *mask;
		AknIconUtils::CreateIconL( bitmap, mask,
				KpjsuaFile, EMbmPjsuaPjsua, -1 );
		AknIconUtils::SetSize( bitmap, TSize( 99, 111 ), EAspectRatioPreserved );
		iImage1->SetPicture( bitmap );
		}
	iImage1->SetAlignment( EHCenterVTop );
	iLabel1 = new ( ELeave ) CEikLabel;
	iLabel1->SetContainerWindowL( *this );
		{
		TResourceReader reader;
		iEikonEnv->CreateResourceReaderLC( reader, R_PJSUA_CONTAINER_LABEL1 );
		iLabel1->ConstructFromResourceL( reader );
		CleanupStack::PopAndDestroy(); // reader internal state
		}
	
	}
// ]]] end generated function

/** 
 * Handle global resource changes, such as scalable UI or skin events (override)
 */
void CPjsuaContainer::HandleResourceChange( TInt aType )
	{
	CCoeControl::HandleResourceChange( aType );
	SetRect( iAvkonViewAppUi->View( TUid::Uid( EPjsuaContainerViewId ) )->ClientRect() );
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}
				
/**
 *	Draw container contents.
 */				
void CPjsuaContainer::Draw( const TRect& aRect ) const
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	CWindowGc& gc = SystemGc();
	gc.SetPenStyle( CGraphicsContext::ENullPen );
	TRgb backColor( 0,0,0 );
	gc.SetBrushColor( backColor );
	gc.SetBrushStyle( CGraphicsContext::ESolidBrush );
	gc.DrawRect( aRect );
	
	// ]]] end generated region [Generated Contents]
	
	}

void CPjsuaContainer::PutMessageL( const char * msg )
{
    if (!iLabel1)
	return;
    
    TPtrC8 ptr(reinterpret_cast<const TUint8*>(msg));
    HBufC* buffer = HBufC::NewLC(ptr.Length());
    buffer->Des().Copy(ptr);

    iLabel1->SetTextL(*buffer);
    iLabel1->DrawNow();

    CleanupStack::PopAndDestroy(buffer);
}
