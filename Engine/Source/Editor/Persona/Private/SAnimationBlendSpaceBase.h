// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IPersonaPreviewScene.h"
#include "Rendering/DrawElements.h"
#include "SAnimEditorBase.h"
#include "Animation/BlendSpaceBase.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/NotifyHook.h"

class SBlendSpaceGridWidget;

class SBlendSpaceEditorBase : public SAnimEditorBase, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceEditorBase)
		: _BlendSpace(NULL)
		{}
			SLATE_ARGUMENT(UBlendSpaceBase*, BlendSpace)
		SLATE_END_ARGS()

	~SBlendSpaceEditorBase();

	void Construct(const FArguments& InArgs, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo);	
	
	// Begin SWidget overrides
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End SWidget overrides

	// Begin FNotifyHook overrides
	virtual void NotifyPreChange(UProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged) override;
	// End FNotifyHook overrides

protected:
	void OnSampleMoved(const int32 SampleIndex, const FVector& NewValue, bool bIsInteractive);
	void OnSampleRemoved(const int32 SampleIndex);
	void OnSampleAdded(UAnimSequence* Animation, const FVector& Value);
	void OnUpdateAnimation(UAnimSequence* Animation, const FVector& Value);
	
	// Begin SAnimEditorBase overrides
	virtual UAnimationAsset* GetEditorObject() const override { return BlendSpace; }
	// End SAnimEditorBase overrides

	virtual void ResampleData() = 0;

	/** Delegate which is called when the Editor has performed an undo operation*/
	void PostUndo();

	/** Updates Persona's preview window */
	void UpdatePreviewParameter() const;
	
	/** Retrieves the preview scene shown by Persona */
	TSharedRef<class IPersonaPreviewScene> GetPreviewScene() const;

	/** Global callback to anticipate on changes to the blend space */
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);
protected:
	/** The blend space being edited */
	UBlendSpaceBase* BlendSpace;

	/** The preview scene we are viewing */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;
	
	/** Pointer to the grid widget which displays the blendspace visualization */
	TSharedPtr<class SBlendSpaceGridWidget> NewBlendSpaceGridWidget;	

	// Property changed delegate
	FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate OnPropertyChangedHandle;	

	/** Handle to the registered OnPropertyChangedHandle delegate */
	FDelegateHandle OnPropertyChangedHandleDelegateHandle;

	/** Flag to check whether or not the preview value should be (re-)set on the next tick */
	bool bShouldSetPreviewValue;
};
