// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IPersonaPreviewScene.h"
#include "SAnimCurvePanel.h"
#include "SAnimationScrubPanel.h"
#include "SAnimEditorBase.h"

class SAnimNotifyPanel;
class SAnimTrackCurvePanel;

//////////////////////////////////////////////////////////////////////////
// SSequenceEditor

/** Overall animation sequence editing widget */
class SSequenceEditor : public SAnimEditorBase
{
public:
	SLATE_BEGIN_ARGS( SSequenceEditor )
		: _Sequence(NULL)
		{}

		SLATE_ARGUMENT(UAnimSequenceBase*, Sequence)
		SLATE_EVENT(FOnObjectsSelected, OnObjectsSelected)
		SLATE_EVENT(FSimpleDelegate, OnAnimNotifiesChanged)
		SLATE_EVENT(FOnInvokeTab, OnInvokeTab)

	SLATE_END_ARGS()

private:
	TSharedPtr<class SAnimNotifyPanel>	AnimNotifyPanel;
	TSharedPtr<class SAnimCurvePanel>	AnimCurvePanel;
	TSharedPtr<class SAnimTrackCurvePanel>	AnimTrackCurvePanel;
	TSharedPtr<class SAnimationScrubPanel> AnimScrubPanel;
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;
public:
	void Construct(const FArguments& InArgs, TSharedRef<class IPersonaPreviewScene> InPreviewScene, TSharedRef<class IEditableSkeleton> InEditableSkeleton, FSimpleMulticastDelegate& OnPostUndo);

	virtual UAnimationAsset* GetEditorObject() const override { return SequenceObj; }

private:
	/** Pointer to the animation sequence being edited */
	UAnimSequenceBase* SequenceObj;

	/** Post undo **/
	void PostUndo();
};
