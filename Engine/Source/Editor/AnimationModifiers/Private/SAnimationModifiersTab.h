// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetEditorToolkit.h"
#include "SModifierListview.h"
#include "EditorUndoClient.h"

class IDetailsView;
class UAnimSequence;
class USkeleton;
class UAnimationModifiersAssetUserData;
class UAnimationModifier;
class UBlueprint;
class SMenuAnchor;

class ANIMATIONMODIFIERS_API SAnimationModifiersTab : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SAnimationModifiersTab)
	{}	
	SLATE_ARGUMENT(TWeakPtr<class FAssetEditorToolkit>, InHostingApp)
	SLATE_END_ARGS()

	SAnimationModifiersTab();
	~SAnimationModifiersTab();

	/** SWidget functions */
	void Construct(const FArguments& InArgs);

	/** Begin SCompoundWidget */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/** End SCompoundWidget */

	/** Begin FEditorUndoClient */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	/** End FEditorUndoClient */
protected:
	/** Creates the class picker for the available modifiers */
	TSharedRef<SWidget> GetModifierPicker();
	/** Callback for when user has picked a modifier to add */
	void OnModifierPicked(UClass* PickedClass);

	void CreateInstanceDetailsView();	

	/** UI apply all modifiers button callback */
	FReply OnApplyAllModifiersClicked();

	/** Callbacks for available modifier actions */
	void OnApplyModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances);	
	void OnRevertModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances);
	void OnRemoveModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances);
	void OnOpenModifier(const TWeakObjectPtr<UAnimationModifier>& Instance);

	void OnMoveModifierUp(const TWeakObjectPtr<UAnimationModifier>& Instance);
	void OnMoveModifierDown(const TWeakObjectPtr<UAnimationModifier>& Instance);

	/** Flags UI dirty and will refresh during the next Tick*/
	void Refresh();

	/** Callback for compiled blueprints, this ensures to refresh the UI */
	void OnBlueprintCompiled(UBlueprint* Blueprint);
	/** Callback to keep track of when an asset is opened, this is necessary for when an editor document tab is reused and this tab isn't recreated */
	void OnAssetOpened(UObject* Object, IAssetEditorInstance* Instance);

	/** Applying and reverting of modifiers */
	void ApplyModifiers(const TArray<UAnimationModifier*>& Modifiers);
	void RevertModifiers(const TArray<UAnimationModifier*>& Modifiers);

	/** Retrieves the currently opened animation asset type and modifier user data */
	void RetrieveAnimationAsset();
	/** Creates a new Modifier instance to store with the current asset */
	UAnimationModifier* CreateModifierInstance(UObject* Outer, UClass* InClass);
	/** Retrieves all animation sequences which are dependent on the current opened skeleton */
	void FindAnimSequencesForSkeleton(TArray<UAnimSequence *> &ReferencedAnimSequences);
protected:
	TWeakPtr<class FAssetEditorToolkit> HostingApp;

	/** Retrieved currently open animation asset type */
	USkeleton* Skeleton;
	UAnimSequence* AnimationSequence;
	/** Asset user data retrieved from AnimSequence or Skeleton */
	UAnimationModifiersAssetUserData* AssetUserData;
	/** List of blueprints for which a delegate was registered for OnCompiled */
	TArray<UBlueprint*> DelegateRegisteredBlueprints;
	/** Flag whether or not the UI should be refreshed  */
	bool bDirty;
protected:
	/** UI elements and data */
	TSharedPtr<IDetailsView> ModifierInstanceDetailsView;
	TArray<ModifierListviewItem> ModifierItems;
	TSharedPtr<SModifierListView> ModifierListView;
	TSharedPtr<SMenuAnchor> AddModifierCombobox;	
private:
	void RetrieveModifierData();
	void ResetModifierData();
};