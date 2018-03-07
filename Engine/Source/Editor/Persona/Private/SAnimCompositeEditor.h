// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SAnimCurvePanel.h"
#include "SAnimEditorBase.h"
#include "Animation/AnimComposite.h"
#include "SAnimCompositePanel.h"

class SAnimNotifyPanel;

//////////////////////////////////////////////////////////////////////////
// SAnimCompositeEditor

/** Overall animation composite editing widget. This mostly contains functions for editing the UAnimComposite.

	SAnimCompositeEditor will create the SAnimCompositePanel which is mostly responsible for setting up the UI 
	portion of the composite tool and registering callbacks to the SAnimCompositeEditor to do the actual editing.
	
*/
class SAnimCompositeEditor : public SAnimEditorBase
{
public:
	SLATE_BEGIN_ARGS( SAnimCompositeEditor )
		: _Composite(NULL)
		{}

		SLATE_ARGUMENT( UAnimComposite*, Composite)
		SLATE_EVENT(FOnObjectsSelected, OnObjectsSelected)
		SLATE_EVENT(FSimpleDelegate, OnAnimNotifiesChanged)
		SLATE_EVENT(FOnInvokeTab, OnInvokeTab)

	SLATE_END_ARGS()

private:
	/** Slate editor panels */
	TSharedPtr<class SAnimCompositePanel> AnimCompositePanel;
	TSharedPtr<class SAnimNotifyPanel> AnimNotifyPanel;
	TSharedPtr<class SAnimCurvePanel>	AnimCurvePanel;

	/** do I have to update the panel **/
	bool bRebuildPanel;
	void RebuildPanel();

	/** Handler for when composite is edited in details view */
	void OnCompositeChange(class UObject *EditorAnimBaseObj, bool bRebuild);

	/** This will remove empty spaces in the composite's anim segments but not resort. e.g. - all cached indexes remains valid. UI IS NOT REBUILT after this */
	void CollapseComposite();

protected:
	virtual void InitDetailsViewEditorObject(class UEditorAnimBaseObj* EdObj) override;

public:
	void Construct(const FArguments& InArgs, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& OnPostUndo);
	
	/** Return the animation composite being edited */
	UAnimComposite* GetCompositeObj() const { return CompositeObj; }
	virtual UAnimationAsset* GetEditorObject() const override { return GetCompositeObj(); }

private:
	/** Pointer to the animation composite being edited */
	UAnimComposite* CompositeObj;
	
	void PostUndo();
	
	virtual float CalculateSequenceLengthOfEditorObject() const override;

	/** This will sort all components of the montage and update (recreate) the UI */
	void SortAndUpdateComposite();

	/** One-off active timer to trigger a panel rebuild */
	EActiveTimerReturnType TriggerRebuildPanel(double InCurrentTime, float InDeltaTime);

	/** Whether the active timer to trigger a panel rebuild is currently registered */
	bool bIsActiveTimerRegistered;

public:

	/** delegate handlers for when the composite is being editor */
	void			PreAnimUpdate();
	void			PostAnimUpdate();

	//~ Begin SAnimEditorBase Interface
	virtual TSharedRef<SWidget> CreateDocumentAnchor() override;
	//~ End SAnimEditorBase Interface
};
