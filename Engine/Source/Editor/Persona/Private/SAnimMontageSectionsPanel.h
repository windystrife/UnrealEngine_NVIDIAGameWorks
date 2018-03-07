// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "SNodePanel.h"
#include "STrack.h"

class SBorder;
class SMontageEditor;
class UAnimMontage;

// Forward declatations
class UAnimMontage;
class SMontageEditor;

class SAnimMontageSectionsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimMontageSectionsPanel )
		: _Montage()
		, _MontageEditor()
		, _bChildAnimMontage(false)
	{}

		SLATE_ARGUMENT( UAnimMontage*, Montage)						// The montage asset being edited
		SLATE_ARGUMENT( TWeakPtr<SMontageEditor>, MontageEditor )	// The montage editor that owns this widget
		SLATE_ARGUMENT( bool, bChildAnimMontage)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Rebuild panel widgets
	void Update();


	void SetSectionPos(float NewPos, int32 SectionIdx, int32 RowIdx);
	// Callback when mouse-drag ends for a section
	void OnSectionDrop();

	// Callback when a section in the upper display is clicked
	// @param SectionIndex - Section that was clicked
	void TopSectionClicked(int32 SectionIndex);

	// Callback when a section in the lower display is clicked
	// @param SectionIndex - Section that was clicked
	void SectionClicked(int32 SectionIndex);
	// Unlinks the requested section
	// @param SectionIndex - Section to unlink
	void RemoveLink(int32 SectionIndex);


	// Set section to play sequentially (default behavior)
	FReply MakeDefaultSequence();
	// Completely remove section sequence data
	FReply ClearSequence();

	// Starts playing from the first section
	FReply PreviewAllSectionsClicked();
	// Plays the clicked section
	// @param SectionIndex - Section to play
	FReply PreviewSectionClicked(int32 SectionIndex);

private:

	// Returns whether or not the provided section is part of a loop
	// @param SectionIdx - Section to check
	bool IsLoop(int32 SectionIdx);

	// Main panel area widget
	TSharedPtr<SBorder>			PanelArea;

	// The montage we are currently observing
	UAnimMontage*				Montage;
	// The montage editor panel we are a child of
	TWeakPtr<SMontageEditor>	MontageEditor;
	// Section to row idx mapping
	TArray<TArray<int32>>		SectionMap;

	// Selection sets 
	STrackNodeSelectionSet		TopSelectionSet;
	STrackNodeSelectionSet		SelectionSet;

	// Currently selected section index
	int32						SelectedCompositeSection;

	/*
	* Child Anim Montage: Child Anim Montage only can replace name of animations, and no other meaningful edits
	* as it will derive every data from Parent. There might be some other data that will allow to be replaced, but for now, it is
	* not.
	*/
	bool						bChildAnimMontage;
};
