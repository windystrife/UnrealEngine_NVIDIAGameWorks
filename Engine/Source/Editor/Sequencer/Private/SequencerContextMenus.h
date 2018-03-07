// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Sequencer.h"
#include "SequencerClipboardReconciler.h"
#include "ScopedTransaction.h"

struct FEasingAreaHandle;
class FMenuBuilder;
class UMovieSceneSection;

/**
 * Class responsible for generating a menu for the currently selected sections.
 * This is a shared class that's entirely owned by the context menu handlers.
 * Once the menu is closed, all references to this class are removed, and the 
 * instance is cleaned up. To ensure no weak references are held, AsShared is hidden
 */
struct FSectionContextMenu : TSharedFromThis<FSectionContextMenu>
{
	static void BuildMenu(FMenuBuilder& MenuBuilder, FSequencer& Sequencer, float InMouseDownTime);

private:

	/** Hidden AsShared() methods to discourage CreateSP delegate use. */
	using TSharedFromThis::AsShared;

	FSectionContextMenu(FSequencer& InSeqeuncer, float InMouseDownTime)
		: Sequencer(StaticCastSharedRef<FSequencer>(InSeqeuncer.AsShared()))
		, MouseDownTime(InMouseDownTime)
	{}

	void PopulateMenu(FMenuBuilder& MenuBuilder);

	/** Add edit menu for trim and split */
	void AddEditMenu(FMenuBuilder& MenuBuilder);

	/** Add extrapolation menu for pre and post infinity */
	void AddExtrapolationMenu(FMenuBuilder& MenuBuilder, bool bPreInfinity);

	/** Add the Properties sub-menu. */
	void AddPropertiesMenu(FMenuBuilder& MenuBuilder);

	/** Add the Order sub-menu. */
	void AddOrderMenu(FMenuBuilder& MenuBuilder);

	void AddBlendTypeMenu(FMenuBuilder& MenuBuilder);

	void SelectAllKeys();

	void CopyAllKeys();

	bool CanSelectAllKeys() const;

	void TogglePrimeForRecording() const;

	bool IsPrimedForRecording() const;

	bool CanPrimeForRecording() const;

	bool CanSetExtrapolationMode() const;

	void TrimSection(bool bTrimLeft);

	void SplitSection();

	void ReduceKeys();

	bool IsTrimmable() const;

	bool CanReduceKeys() const;

	void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity);

	bool IsExtrapolationModeSelected(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) const;

	void ToggleSectionActive();

	bool IsSectionActive() const;

	void ToggleSectionLocked();

	bool IsSectionLocked() const;

	void DeleteSection();

	void BringToFront();

	void SendToBack();

	void BringForward();

	void SendBackward();

	FMovieSceneBlendTypeField GetSupportedBlendTypes() const;

	/** The sequencer */
	TSharedRef<FSequencer> Sequencer;

	/** The time that we clicked on to summon this menu */
	float MouseDownTime;
};

/** Arguments required for a paste operation */
struct FPasteContextMenuArgs
{
	/** Paste the clipboard into the specified array of sequencer nodes, at the given time */
	static FPasteContextMenuArgs PasteInto(TArray<TSharedRef<FSequencerDisplayNode>> InNodes, float InTime, TSharedPtr<FMovieSceneClipboard> InClipboard = nullptr)
	{
		FPasteContextMenuArgs Args;
		Args.Clipboard = InClipboard;
		Args.DestinationNodes = MoveTemp(InNodes);
		Args.PasteAtTime = InTime;
		return Args;
	}

	/** Paste the clipboard at the given time, using the sequencer selection states to determine paste destinations */
	static FPasteContextMenuArgs PasteAt(float InTime, TSharedPtr<FMovieSceneClipboard> InClipboard = nullptr)
	{
		FPasteContextMenuArgs Args;
		Args.Clipboard = InClipboard;
		Args.PasteAtTime = InTime;
		return Args;
	}

	/** The clipboard to paste */
	TSharedPtr<FMovieSceneClipboard> Clipboard;

	/** The Time to paste at */
	float PasteAtTime;

	/** Optional user-supplied nodes to paste into */
	TArray<TSharedRef<FSequencerDisplayNode>> DestinationNodes;
};

struct FPasteContextMenu : TSharedFromThis<FPasteContextMenu>
{
	static bool BuildMenu(FMenuBuilder& MenuBuilder, FSequencer& Sequencer, const FPasteContextMenuArgs& Args);

	static TSharedRef<FPasteContextMenu> CreateMenu(FSequencer& Sequencer, const FPasteContextMenuArgs& Args);

	void PopulateMenu(FMenuBuilder& MenuBuilder);

	bool IsValidPaste() const;

	bool AutoPaste();

private:

	using TSharedFromThis::AsShared;

	FPasteContextMenu(FSequencer& InSequencer, const FPasteContextMenuArgs& InArgs)
		: Sequencer(StaticCastSharedRef<FSequencer>(InSequencer.AsShared()))
		, Args(InArgs)
	{}

	void Setup();

	void AddPasteMenuForTrackType(FMenuBuilder& MenuBuilder, int32 DestinationIndex);

	void PasteInto(int32 DestinationIndex, FName KeyAreaName);

	void GatherPasteDestinationsForNode(FSequencerDisplayNode& InNode, UMovieSceneSection* InSection, const FName& CurrentScope, TMap<FName, FSequencerClipboardReconciler>& Map);

	/** The sequencer */
	TSharedRef<FSequencer> Sequencer;

	/** Paste destinations are organized by track type primarily, then by key area name  */
	struct FPasteDestination
	{
		FText Name;
		TMap<FName, FSequencerClipboardReconciler> Reconcilers;
	};
	TArray<FPasteDestination> PasteDestinations;

	/** Paste arguments */
	FPasteContextMenuArgs Args;
};

struct FPasteFromHistoryContextMenu : TSharedFromThis<FPasteFromHistoryContextMenu>
{
	static bool BuildMenu(FMenuBuilder& MenuBuilder, FSequencer& Sequencer, const FPasteContextMenuArgs& Args);

	static TSharedPtr<FPasteFromHistoryContextMenu> CreateMenu(FSequencer& Sequencer, const FPasteContextMenuArgs& Args);

	void PopulateMenu(FMenuBuilder& MenuBuilder);

private:

	using TSharedFromThis::AsShared;

	FPasteFromHistoryContextMenu(FSequencer& InSequencer, const FPasteContextMenuArgs& InArgs)
		: Sequencer(StaticCastSharedRef<FSequencer>(InSequencer.AsShared()))
		, Args(InArgs)
	{}

	/** The sequencer */
	TSharedRef<FSequencer> Sequencer;

	/** Paste arguments */
	FPasteContextMenuArgs Args;
};

/**
 * Class responsible for generating a menu for the currently selected keys.
 * This is a shared class that's entirely owned by the context menu handlers.
 * Once the menu is closed, all references to this class are removed, and the 
 * instance is cleaned up. To ensure no weak references are held, AsShared is hidden
 */
struct FKeyContextMenu : TSharedFromThis<FKeyContextMenu>
{
	static void BuildMenu(FMenuBuilder& MenuBuilder, FSequencer& Sequencer);

private:

	FKeyContextMenu(FSequencer& InSeqeuncer)
		: Sequencer(StaticCastSharedRef<FSequencer>(InSeqeuncer.AsShared()))
	{}

	/** Hidden AsShared() methods to discourage CreateSP delegate use. */
	using TSharedFromThis::AsShared;

	/** Add the Properties sub-menu. */
	void AddPropertiesMenu(FMenuBuilder& MenuBuilder);

	/** Check if we can add the key properties menu */
	bool CanAddPropertiesMenu() const;

	void PopulateMenu(FMenuBuilder& MenuBuilder);

	/** The sequencer */
	TSharedRef<FSequencer> Sequencer;
};


/**
 * Class responsible for generating a menu for a set of easing curves.
 * This is a shared class that's entirely owned by the context menu handlers.
 * Once the menu is closed, all references to this class are removed, and the 
 * instance is cleaned up. To ensure no weak references are held, AsShared is hidden
 */
struct FEasingContextMenu : TSharedFromThis<FEasingContextMenu>
{
	static void BuildMenu(FMenuBuilder& MenuBuilder, const TArray<FEasingAreaHandle>& InEasings, FSequencer& Sequencer, float InMouseDownTime);

private:

	FEasingContextMenu(const TArray<FEasingAreaHandle>& InEasings)
		: Easings(InEasings)
	{}

	/** Hidden AsShared() methods to discourage CreateSP delegate use. */
	using TSharedFromThis::AsShared;

	void PopulateMenu(FMenuBuilder& MenuBuilder);

	FText GetEasingTypeText() const;

	void EasingTypeMenu(FMenuBuilder& MenuBuilder);

	void EasingOptionsMenu(FMenuBuilder& MenuBuilder);

	void OnEasingTypeChanged(UClass* NewClass);

	void OnUpdateLength(float NewLength);

	TOptional<float> GetCurrentLength() const;

	ECheckBoxState GetAutoEasingCheckState() const;

	void SetAutoEasing(bool bAutoEasing);

	/** The sequencer */
	TArray<FEasingAreaHandle> Easings;

	/** A scoped transaction for a current operation */
	TUniquePtr<FScopedTransaction> ScopedTransaction;
};
