// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "EditorUndoClient.h"


class AActor;
class FSCSEditorTreeNode;
class FTabManager;
class FUICommandList;
class IDetailsView;
class SBox;
class SSCSEditor;
class SSplitter;
class UBlueprint;


/**
 * Wraps a details panel customized for viewing actors
 */
class SActorDetails : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SActorDetails) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FName TabIdentifier, TSharedPtr<FUICommandList> InCommandList, TSharedPtr<FTabManager> InTabManager);
	~SActorDetails();

	/**
	 * Sets the objects to be viewed by the details panel
	 *
	 * @param InObjects	The objects to set
	 */
	void SetObjects(const TArray<UObject*>& InObjects, bool bForceRefresh = false);

	/** FEditorUndoClient Interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	AActor* GetSelectedActorInEditor() const;
	AActor* GetActorContext() const;
	bool GetAllowComponentTreeEditing() const;

	void OnComponentsEditedInWorld();
	void OnEditorSelectionChanged(UObject* Object);
	void OnSCSEditorRootSelected(AActor* Actor);
	void OnSCSEditorTreeViewSelectionChanged(const TArray<TSharedPtr<class FSCSEditorTreeNode> >& SelectedNodes);
	void OnSCSEditorTreeViewItemDoubleClicked(const TSharedPtr<class FSCSEditorTreeNode> ClickedNode);
	void UpdateComponentTreeFromEditorSelection();

	bool IsPropertyReadOnly(const struct FPropertyAndParent& PropertyAndParent) const;
	bool IsPropertyEditingEnabled() const;
	EVisibility GetUCSComponentWarningVisibility() const;
	EVisibility GetInheritedBlueprintComponentWarningVisibility() const;
	EVisibility GetNativeComponentWarningVisibility() const;
	void OnBlueprintedComponentWarningHyperlinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata);
	void OnNativeComponentWarningHyperlinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata);

	void AddBPComponentCompileEventDelegate(UBlueprint* ComponentBlueprint);
	void RemoveBPComponentCompileEventDelegate();
	void OnBlueprintComponentCompiled(UBlueprint* ComponentBlueprint);

private:
	TSharedPtr<SSplitter> DetailsSplitter;
	TSharedPtr<class IDetailsView> DetailsView;
	TSharedPtr<SBox> ComponentsBox;
	TSharedPtr<class SSCSEditor> SCSEditor;

	// The actor selected when the details panel was locked
	TWeakObjectPtr<AActor> LockedActorSelection;

	// The current component blueprint selection
	TWeakObjectPtr<UBlueprint> SelectedBPComponentBlueprint;
	bool bSelectedComponentRecompiled;

	// Used to prevent reentrant changes
	bool bSelectionGuard;

	// True if the actor "root" node in the SCS editor is currently shown as selected
	bool bShowingRootActorNodeSelected;
};
