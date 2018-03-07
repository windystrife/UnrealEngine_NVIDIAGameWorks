// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Animation/Skeleton.h"
#include "SAnimTrackPanel.h"
#include "SAnimCurvePanel.h"
#include "Animation/AnimSequence.h"

class IPersonaPreviewScene;
class SSplitter;
class STransformCurveEdTrack;

//////////////////////////////////////////////////////////////////////////
// SAnimTrackCurvePanel

class SAnimTrackCurvePanel: public SAnimTrackPanel
{
public:
	SLATE_BEGIN_ARGS( SAnimTrackCurvePanel )
		: _Sequence()
		, _ViewInputMin()
		, _ViewInputMax()
		, _InputMin()
		, _InputMax()
		, _OnSetInputViewRange()
		, _OnGetScrubValue()
	{}

	/**
	 * AnimSequenceBase to be used for this panel
	 */
	SLATE_ARGUMENT( class UAnimSequence*, Sequence)
	/**
	 * Right side of widget width (outside of curve)
	 */
	SLATE_ARGUMENT( float, WidgetWidth )
	/**
	 * Viewable Range control variables
	 */
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( float, InputMin )
	SLATE_ATTRIBUTE( float, InputMax )
	SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
	/**
	 * Get current value
	 */
	SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene);

	~SAnimTrackCurvePanel();
	/**
	 * Delete Track
	 *
	 */
	void DeleteTrack(USkeleton::AnimCurveUID Uid);

	/**
	 * Sets the specified flag value to State for the provided curve
	 */
	void SetCurveFlag(FAnimCurveBase* CurveInterface, bool State, EAnimAssetCurveFlags FlagToSet);

	/**
	* Update Panel
	* Used internally and by sequence editor to refresh the panel contents
	* @todo this has to be more efficient. Right now it refreshes the entire panel
	*/
	void UpdatePanel();

	/** Get Context Menu Per Track **/
	TSharedRef<SWidget> CreateCurveContextMenu(USkeleton::AnimCurveUID CurveUid) const;

	/** Get Length of Sequence */
	float GetLength() { return (Sequence)? Sequence->SequenceLength : 0.f; }

private:
	TWeakPtr<IPersonaPreviewScene> PreviewScenePtr;
	TSharedPtr<SSplitter> PanelSlot;
	class UAnimSequence* Sequence;
	TAttribute<float> CurrentPosition;
	FOnGetScrubValue OnGetScrubValue;
	TArray<TWeakPtr<class STransformCurveEdTrack>> Tracks;

	/**
	 * This is to control visibility of the curves, so you can edit or not
	 * Get Widget that shows all curve list and edit
	 */
	TSharedRef<SWidget>		GenerateCurveList();
	/**
	 * Returns true if this curve is editable
	 */
	ECheckBoxState	IsCurveEditable(USkeleton::AnimCurveUID Uid) const;
	/**
	 * Toggle curve visibility
	 */
	void		ToggleEditability(ECheckBoxState NewType, USkeleton::AnimCurveUID Uid);
	/**
	 * Refresh Panel
	 */
	FReply		RefreshPanel();
	/**
	 *	Show All Curves
	 */
	FReply		ShowAll(bool bShow);

	/**
	 * Convert the requested flag bool value into a checkbox state
	 */
	ECheckBoxState GetCurveFlagAsCheckboxState(USkeleton::AnimCurveUID CurveUid, EAnimAssetCurveFlags InFlag) const;

	/**
	 * Convert a given checkbox state into a flag value in the provided curve
	 */
	void SetCurveFlagFromCheckboxState(ECheckBoxState CheckState, USkeleton::AnimCurveUID CurveUid, EAnimAssetCurveFlags InFlag);

	/** Handle key complete delegate */
	void HandleKeyComplete();
};