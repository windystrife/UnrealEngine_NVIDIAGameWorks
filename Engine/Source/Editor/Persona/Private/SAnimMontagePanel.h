// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SAnimTrackPanel.h"
#include "SMontageEditor.h"
#include "Animation/EditorAnimBaseObj.h"
#include "STrack.h"

class FMenuBuilder;
class SBorder;
class SImage;
class STextBlock;

DECLARE_DELEGATE( FOnMontageLengthChange )
DECLARE_DELEGATE( FOnMontagePropertyChanged )
DECLARE_DELEGATE_OneParam( FOnMontageSetPreviewSlot, int32)

//////////////////////////////////////////////////////////////////////////
// SAnimMontagePanel
//	This is the main montage editing widget that is responsible for setting up
//	a set of generic widgets (STrack and STrackNodes) for editing an anim montage.
//
//	SAnimMontagePanel will usually not edit the montage directly but just setup the 
//  callbacks to SMontageEditor.
//
//////////////////////////////////////////////////////////////////////////

class SAnimMontagePanel : public SAnimTrackPanel
{
public:
	SLATE_BEGIN_ARGS( SAnimMontagePanel )
		: _Montage()
		, _MontageEditor()
		, _CurrentPosition()
		, _ViewInputMin()
		, _ViewInputMax()
		, _InputMin()
		, _InputMax()
		, _OnSetInputViewRange()
		, _OnGetScrubValue()
		, _OnMontageChange()
		, _OnInvokeTab()
		, _bChildAnimMontage(false)
	{}

	SLATE_ARGUMENT( class UAnimMontage*, Montage)
	SLATE_ARGUMENT( TWeakPtr<class SMontageEditor>, MontageEditor)
	SLATE_ARGUMENT( float, WidgetWidth )
	SLATE_ATTRIBUTE( float, CurrentPosition )
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( float, InputMin )
	SLATE_ATTRIBUTE( float, InputMax )
	SLATE_ATTRIBUTE(EVisibility, SectionTimingNodeVisibility)
	SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
	SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )
	SLATE_EVENT( FOnAnimObjectChange, OnMontageChange )
	SLATE_EVENT( FOnInvokeTab, OnInvokeTab )
	SLATE_EVENT(FOnMontageSetPreviewSlot, OnSetMontagePreviewSlot)
	SLATE_ARGUMENT(bool, bChildAnimMontage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FSimpleMulticastDelegate& OnAnimNotifiesChanged, FSimpleMulticastDelegate& OnSectionsChanged);
	void SetMontage(class UAnimMontage * InMontage);

	// SWidget interface
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	void Update();

	// Functions used as delegates that build the context menus for the montage tracks
	void SummonTrackContextMenu( FMenuBuilder& MenuBuilder, float DataPosX, int32 SectionIndex, int32 AnimSlotIndex );
	void SummonSectionContextMenu( FMenuBuilder& MenuBuilder, int32 SectionIndex );
	void FillElementSubMenuForTimes( FMenuBuilder& MenuBuilder );
	void FillSlotSubMenu( FMenuBuilder& Menubuilder );

	void OnNewSlotClicked();
	void OnNewSectionClicked(float DataPosX);
	bool CanAddNewSection();

	void ShowSegmentInDetailsView(int32 AnimSegmentIndex, int32 AnimSlotIndex);
	void ShowSectionInDetailsView(int32 SectionIndex);

	void ClearSelected();

	// helper method to check whether the slot name is empty or not. If empty, shows an error message to provide a valid name
	void CheckSlotName(const FText& SlotName, int32 SlotNodeIndex, bool bShouldCheckCollapsed = false) const;

	// check the slot name whether valid or not while the user is typing
	void OnSlotNameChanged(const FText& NewText, int32 SlotNodeIndex);

	// get slot name from a montage editor and check the slot name whether valid or not
	FText GetMontageSlotName(int32 SlotIndex) const;
	
	// Handlers for preview slot checkbox UI
	ECheckBoxState IsSlotPreviewed(int32 SlotIndex) const;
	void OnSlotPreviewedChanged(ECheckBoxState NewState, int32 SlotIndex);

	// Context menu callback to set all elements in the montage to a given link method
	void OnSetElementsToLinkMode(EAnimLinkMethod::Type NewLinkMethod);

	// Fills the given array with all linkable elements in the montage
	void CollectLinkableElements(TArray<FAnimLinkableElement*>& Elements);

	// Context menu callback to set all elements in the montage to a given slot
	void OnSetElementsToSlot(int32 SlotIndex);

	// Clears the name track of timing nodes and rebuilds them
	void RefreshTimingNodes();

private:
	TWeakPtr<SMontageEditor>	MontageEditor;
	TSharedPtr<SBorder> PanelArea;
	class UAnimMontage* Montage;
	TAttribute<float> CurrentPosition;

	TArray<TSharedPtr<class SMontageEdTrack>>	TrackList;

	FString LastContextHeading;

	STrackNodeSelectionSet SelectionSet;

	TArray< TSharedPtr< class STextComboBox> >	SlotNameComboBoxes;
	TArray< FName > SlotNameComboSelectedNames;

	TArray< TSharedPtr< FString > > SlotNameComboListItems;
	TArray< FName > SlotNameList;

	TArray< TSharedPtr<SImage> > SlotWarningImages;

	TSharedPtr<STrack> SectionNameTrack;
	TAttribute<EVisibility> SectionTimingNodeVisibility;

	/** Member data to allow use to set preview slot from editor */
	int32					 CurrentPreviewSlot;
	FOnMontageSetPreviewSlot OnSetMontagePreviewSlot;

  	/* 
	 * Child Anim Montage: Child Anim Montage only can replace name of animations, and no other meaningful edits 
	 * as it will derive every data from Parent. There might be some other data that will allow to be replaced, but for now, it is
	 * not. 
	 */
	bool bChildAnimMontage;

	/************************************************************************/
	/* Status Bar                                                           */
	/************************************************************************/
	TSharedPtr<STextBlock> StatusBarTextBlock;
	TSharedPtr<SImage> StatusBarWarningImage;

	void OnSlotNameChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo, int32 AnimSlotIndex);
	void OnSlotListOpening(int32 AnimSlotIndex);
	FReply OnOpenAnimSlotManager();
	void RefreshComboLists(bool bOnlyRefreshIfDifferent = false);
	void UpdateSlotGroupWarningVisibility();
	
	void CreateNewSlot(const FText& NewSlotName, ETextCommit::Type CommitInfo);
	void CreateNewSection(const FText& NewSectionName, ETextCommit::Type CommitInfo, float StartTime);

	/** Called when a segment is removed from a track, so we can adjust the indices in linkable elements
	 *	@param SegmentIndex - Index of the removed segment
	 */
	void OnAnimSegmentRemoved(int32 SegmentIndex, int32 SlotIndex);

	/** Gets the length of the montage we are currently editing
	 */
	virtual float GetSequenceLength() const override;

	/** Delegate used to invoke a tab in the containing editor */
	FOnInvokeTab OnInvokeTab;
};
