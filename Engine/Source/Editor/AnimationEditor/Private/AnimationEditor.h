// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Widgets/SWidget.h"
#include "UObject/GCObject.h"
#include "Toolkits/IToolkitHost.h"
#include "IAnimationEditor.h"
#include "TickableEditorObject.h"
#include "EditorUndoClient.h"
#include "ArrayView.h"

struct FAssetData;
class FMenuBuilder;
class IAnimationSequenceBrowser;
class IDetailsView;
class IPersonaToolkit;
class IPersonaViewport;
class ISkeletonTree;
class UAnimationAsset;
class USkeletalMeshComponent;
class UAnimSequence;
class ISkeletonTreeItem;

namespace AnimationEditorModes
{
	// Mode identifiers
	extern const FName AnimationEditorMode;
}

namespace AnimationEditorTabs
{
	// Tab identifiers
	extern const FName DetailsTab;
	extern const FName SkeletonTreeTab;
	extern const FName ViewportTab;
	extern const FName AdvancedPreviewTab;
	extern const FName DocumentTab;
	extern const FName AssetBrowserTab;
	extern const FName AssetDetailsTab;
	extern const FName CurveNamesTab;
	extern const FName SlotNamesTab;
}

class FAnimationEditor : public IAnimationEditor, public FGCObject, public FEditorUndoClient, public FTickableEditorObject
{
public:
	FAnimationEditor();

	virtual ~FAnimationEditor();

	/** Edits the specified Skeleton object */
	void InitAnimationEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, class UAnimationAsset* InAnimationAsset);

	/** IAnimationEditor interface */
	virtual void SetAnimationAsset(UAnimationAsset* AnimAsset) override;

	/** IHasPersonaToolkit interface */
	virtual TSharedRef<class IPersonaToolkit> GetPersonaToolkit() const override { return PersonaToolkit.ToSharedRef(); }

	/** IToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** FTickableEditorObject Interface */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	
	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Animation/AnimationEditor"));
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** Get the skeleton tree widget */
	TSharedRef<class ISkeletonTree> GetSkeletonTree() const { return SkeletonTree.ToSharedRef(); }

	void HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView);

	UObject* HandleGetAsset();

	void HandleOpenNewAsset(UObject* InNewAsset);

	void HandleAnimationSequenceBrowserCreated(const TSharedRef<class IAnimationSequenceBrowser>& InSequenceBrowser);

	void HandleSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo);

	void HandleObjectSelected(UObject* InObject);

	void HandleObjectsSelected(const TArray<UObject*>& InObjects);

private:

	enum class EPoseSourceOption : uint8
	{
		ReferencePose,
		CurrentPose,
		CurrentAnimation_AnimData,
		CurrentAnimation_PreviewMesh,
		Max
	};

	void HandleAnimNotifiesChanged();

	void HandleSectionsChanged();

	bool HasValidAnimationSequence() const;

	bool CanSetKey() const;

	void OnSetKey();

	bool CanApplyRawAnimChanges() const;

	void OnApplyRawAnimChanges();

	void OnReimportAnimation();

	void OnApplyCompression();

	void OnExportToFBX(const EPoseSourceOption Option);
	void ExportToFBX(const TArray<UObject*> NewAssets, bool bRecordAnimation);

	void OnAddLoopingInterpolation();

	TSharedRef< SWidget > GenerateCreateAssetMenu() const;
	TSharedRef< SWidget > GenerateExportAssetMenu() const;

	void FillCreateAnimationMenu(FMenuBuilder& MenuBuilder) const;

	void FillCreateAnimationFromCurrentAnimationMenu(FMenuBuilder& MenuBuilder) const;

	void FillCreatePoseAssetMenu(FMenuBuilder& MenuBuilder) const;

	void FillInsertPoseMenu(FMenuBuilder& MenuBuilder) const;

	void InsertCurrentPoseToAsset(const FAssetData& NewPoseAssetData);

	void FillCopyToSoundWaveMenu(FMenuBuilder& MenuBuilder) const;

	void FillExportAssetMenu(FMenuBuilder& MenuBuilder) const;

	void CopyCurveToSoundWave(const FAssetData& SoundWaveAssetData) const;

	void CreateAnimation(const TArray<UObject*> NewAssets, const EPoseSourceOption Option);

	void CreatePoseAsset(const TArray<UObject*> NewAssets, const EPoseSourceOption Option);

	void HandleAssetCreated(const TArray<UObject*> NewAssets);

	void ConditionalRefreshEditor(UObject* InObject);

	void HandlePostReimport(UObject* InObject, bool bSuccess);

	void HandlePostImport(class UFactory* InFactory, UObject* InObject);

private:
	void ExtendMenu();

	void ExtendToolbar();

	void BindCommands();

	TSharedPtr<SDockTab> OpenNewAnimationDocumentTab(UAnimationAsset* InAnimAsset);

	bool RecordMeshToAnimation(USkeletalMeshComponent* PreviewComponent, UAnimSequence* NewAsset) const;
public:
	/** Multicast delegate fired on anim notifies changing */
	FSimpleMulticastDelegate OnChangeAnimNotifies;

	/** Multicast delegate fired on global undo/redo */
	FSimpleMulticastDelegate OnPostUndo;

	/** Multicast delegate fired on global undo/redo */
	FSimpleMulticastDelegate OnLODChanged;

	/** Multicast delegate fired on sections changing */
	FSimpleMulticastDelegate OnSectionsChanged;

private:
	/** The animation asset we are editing */
	UAnimationAsset* AnimationAsset;

	/** Toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Menu extender */
	TSharedPtr<FExtender> MenuExtender;

	/** Persona toolkit */
	TSharedPtr<class IPersonaToolkit> PersonaToolkit;

	/** Skeleton tree */
	TSharedPtr<class ISkeletonTree> SkeletonTree;

	/** Viewport */
	TSharedPtr<class IPersonaViewport> Viewport;

	/** Details panel */
	TSharedPtr<class IDetailsView> DetailsView;

	/** The animation document currently being edited */
	TWeakPtr<SDockTab> SharedAnimDocumentTab;

	/** Sequence Browser **/
	TWeakPtr<class IAnimationSequenceBrowser> SequenceBrowser;
};
