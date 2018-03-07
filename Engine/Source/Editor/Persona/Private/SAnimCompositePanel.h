// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SAnimTrackPanel.h"
#include "STrack.h"

class SAnimCompositeEditor;
class SBorder;

//////////////////////////////////////////////////////////////////////////
// SAnimCompositePanel
//	This is the main montage editing widget that is responsible for setting up
//	a set of generic widgets (STrack and STrackNodes) for editing an anim composite.
//
//	SAnimCompositePanel will usually not edit the montage directly but just setup the 
//  callbacks to SAnimCompositeEditor.
//
//////////////////////////////////////////////////////////////////////////

class SAnimCompositePanel : public SAnimTrackPanel
{
public:
	SLATE_BEGIN_ARGS( SAnimCompositePanel )
		: _Composite()
		, _CompositeEditor()
		, _ViewInputMin()
		, _ViewInputMax()
		, _InputMin()
		, _InputMax()
		, _OnSetInputViewRange()
	{}

	SLATE_ARGUMENT( class UAnimComposite*, Composite)
	SLATE_ARGUMENT( TWeakPtr<class SAnimCompositeEditor>, CompositeEditor)
	SLATE_ARGUMENT( float, WidgetWidth )
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( float, InputMin )
	SLATE_ATTRIBUTE( float, InputMax )
	SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Recreates the editor panel to reflect changes to the composite */
	void Update();

	/** Clears the selected segment from the details panel */
	void ClearSelected();

	/** Handlers for when the user clicks on a anim sequence node*/
	void ShowSegmentInDetailsView(int32 SegmentIndex);

private:
	/** Reference to our editor object */
	TWeakPtr<SAnimCompositeEditor>	CompositeEditor;

	/** The composite we are currently editing */
	class UAnimComposite*		Composite;

	/** Is populated by the Update method with the panels UI */
	TSharedPtr<SBorder> PanelArea;

	/** Passed to the anim segments panel to handle selection */
	STrackNodeSelectionSet SelectionSet;
};
