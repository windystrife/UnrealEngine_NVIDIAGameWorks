// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Animation/AnimSequence.h"
#include "Editor.h"
#include "PersonaDelegates.h"
#include "Factories/FbxImportUI.h"

class FBlueprintEditor;
class IDetailsView;
class IEditableSkeleton;
class IPersonaPreviewScene;
class IPersonaToolkit;
class UAnimBlueprint;
class USkeletalMeshComponent;
class UPhysicsAsset;

extern const FName PersonaAppName;

// Editor mode constants
struct PERSONA_API FPersonaEditModes
{
	/** Selection/manipulation of bones & sockets */
	const static FEditorModeID SkeletonSelection;
};

DECLARE_DELEGATE_TwoParams(FIsRecordingActive, USkeletalMeshComponent* /*Component*/, bool& /* bIsRecording */);
DECLARE_DELEGATE_OneParam(FRecord, USkeletalMeshComponent* /*Component*/);
DECLARE_DELEGATE_OneParam(FStopRecording, USkeletalMeshComponent* /*Component*/);
DECLARE_DELEGATE_TwoParams(FGetCurrentRecording, USkeletalMeshComponent* /*Component*/, class UAnimSequence*& /* OutRecording */);
DECLARE_DELEGATE_TwoParams(FGetCurrentRecordingTime, USkeletalMeshComponent* /*Component*/, float& /* OutTime */);
DECLARE_DELEGATE_TwoParams(FTickRecording, USkeletalMeshComponent* /*Component*/, float /* DeltaSeconds */);

/** Called back when a viewport is created */
DECLARE_DELEGATE_OneParam(FOnViewportCreated, const TSharedRef<class IPersonaViewport>&);

/** Called back when a details panel is created */
DECLARE_DELEGATE_OneParam(FOnDetailsCreated, const TSharedRef<class IDetailsView>&);

/** Called back when an anim sequence browser is created */
DECLARE_DELEGATE_OneParam(FOnAnimationSequenceBrowserCreated, const TSharedRef<class IAnimationSequenceBrowser>&);

/** Called back when a Persona preview scene is created */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreviewSceneCreated, const TSharedRef<class IPersonaPreviewScene>&);

/** Initialization parameters for persona toolkits */
struct FPersonaToolkitArgs
{
	/** 
	 * Delegate called when the preview scene is created, used to setup the scene 
	 * If this is not set, then a default scene will be set up.
	 */
	FOnPreviewSceneCreated::FDelegate OnPreviewSceneCreated;

	/** Whether to create a preview scene */
	bool bCreatePreviewScene;

	FPersonaToolkitArgs()
		: bCreatePreviewScene(true)
	{
	}
};

struct FAnimDocumentArgs
{
	FAnimDocumentArgs(const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, const TSharedRef<class IPersonaToolkit>& InPersonaToolkit, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnPostUndo, FSimpleMulticastDelegate& InOnAnimNotifiesChanged, FSimpleMulticastDelegate& InOnSectionsChanged)
		: PreviewScene(InPreviewScene)
		, PersonaToolkit(InPersonaToolkit)
		, EditableSkeleton(InEditableSkeleton)
		, OnPostUndo(InOnPostUndo)
		, OnAnimNotifiesChanged(InOnAnimNotifiesChanged)
		, OnSectionsChanged(InOnSectionsChanged)
	{}

	/** Required args */
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
	TWeakPtr<class IPersonaToolkit> PersonaToolkit;
	TWeakPtr<class IEditableSkeleton> EditableSkeleton;
	FSimpleMulticastDelegate& OnPostUndo;
	FSimpleMulticastDelegate& OnAnimNotifiesChanged;
	FSimpleMulticastDelegate& OnSectionsChanged;

	/** Optional args */
	FOnObjectsSelected OnDespatchObjectsSelected;
	FOnInvokeTab OnDespatchInvokeTab;
	FSimpleDelegate OnDespatchSectionsChanged;
	FSimpleDelegate OnDespatchAnimNotifiesChanged;
};

/** Arguments used to create a persona viewport tab */
struct FPersonaViewportArgs
{
	FPersonaViewportArgs(const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo)
		: SkeletonTree(InSkeletonTree)
		, PreviewScene(InPreviewScene)
		, OnPostUndo(InOnPostUndo)
		, bShowShowMenu(true)
		, bShowLODMenu(true)
		, bShowPlaySpeedMenu(true)
		, bShowTimeline(true)
		, bShowStats(true)
		, bAlwaysShowTransformToolbar(false)
		, bShowFloorOptions(true)
		, bShowTurnTable(true)
		, bShowPhysicsMenu(false)
	{}

	/** Required args */
	TSharedRef<class ISkeletonTree> SkeletonTree;
	TSharedRef<class IPersonaPreviewScene> PreviewScene;
	FSimpleMulticastDelegate& OnPostUndo;

	/** Optional blueprint editor that we can be embedded in */
	TSharedPtr<class FBlueprintEditor> BlueprintEditor;

	/** Delegate fired when the viewport is created */
	FOnViewportCreated OnViewportCreated;
	
	/** Menu extenders */
	TArray<TSharedPtr<FExtender>> Extenders;

	/** Whether to show the 'Show' menu */
	bool bShowShowMenu;

	/** Whether to show the 'LOD' menu */
	bool bShowLODMenu;

	/** Whether to show the 'Play Speed' menu */
	bool bShowPlaySpeedMenu;

	/** Whether to show the animation timeline */
	bool bShowTimeline;

	/** Whether to show in-viewport stats */
	bool bShowStats;

	/** Whether we should always show the transform toolbar for this viewport */
	bool bAlwaysShowTransformToolbar;

	/** Whether to show options relating to floor height */
	bool bShowFloorOptions;

	/** Whether to show options relating to turntable */
	bool bShowTurnTable;

	/** Whether to show options relating to physics */
	bool bShowPhysicsMenu;
};

/**
 * Persona module manages the lifetime of all instances of Persona editors.
 */
class FPersonaModule : public IModuleInterface,
	public IHasMenuExtensibility
{
public:
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule();

	/** Create a re-usable toolkit that multiple asset editors that are concerned with USkeleton-related data can use */
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(USkeleton* InSkeleton, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(UAnimationAsset* InAnimationAsset, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(USkeletalMesh* InSkeletalMesh, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(UAnimBlueprint* InAnimBlueprint, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(UPhysicsAsset* InPhysicsAsset, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;

	/** Create an asset family for the supplied persona asset */
	virtual TSharedRef<class IAssetFamily> CreatePersonaAssetFamily(const UObject* InAsset) const;

	/** Create a shortcut widget for an asset family */
	virtual TSharedRef<SWidget> CreateAssetFamilyShortcutWidget(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily) const;

	/** Create a details panel tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreateDetailsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, FOnDetailsCreated InOnDetailsCreated) const;

	/** Create a persona viewport tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreatePersonaViewportTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const FPersonaViewportArgs& InArgs) const;

	/** Create an anim notifies tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAnimNotifiesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnChangeAnimNotifies, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected) const;

	/** Create a skeleton cuve viewer tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreateCurveViewerTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected) const;

	/** Create a retarget manager tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreateRetargetManagerTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo) const;

	/** Create a tab factory used to configure preview scene settings */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAdvancedPreviewSceneTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene) const;

	/** Create a tab factory for the animation asset browser */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAnimationAssetBrowserTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaToolkit>& InPersonaToolkit, FOnOpenNewAsset InOnOpenNewAsset, FOnAnimationSequenceBrowserCreated InOnAnimationSequenceBrowserCreated, bool bInShowHistory) const;

	/** Create a tab factory for editing a single object (like an animation asset) */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAssetDetailsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, FOnGetAsset InOnGetAsset, FOnDetailsCreated InOnDetailsCreated) const;

	/** Create a tab factory for for previewing morph targets */
	virtual TSharedRef<class FWorkflowTabFactory> CreateMorphTargetTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo) const;

	/** Create a tab factory for editing anim blueprint preview & defaults */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAnimBlueprintPreviewTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor, const TSharedRef<IPersonaPreviewScene>& InPreviewScene) const;

	/** Create a tab factory for editing anim blueprint parent overrides */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAnimBlueprintAssetOverridesTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor, UAnimBlueprint* InAnimBlueprint, FSimpleMulticastDelegate& InOnPostUndo) const;

	/** Create a tab factory for editing slot names and groups */
	virtual TSharedRef<class FWorkflowTabFactory> CreateSkeletonSlotNamesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectSelected InOnObjectSelected) const;

	/** Create a widget that acts as a document for an animation asset */
	virtual TSharedRef<SWidget> CreateEditorWidgetForAnimDocument(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, UObject* InAnimAsset, const FAnimDocumentArgs& InArgs, FString& OutDocumentLink);

	/** Customize a skeletal mesh details panel */
	virtual void CustomizeMeshDetails(const TSharedRef<IDetailsView>& InDetailsView, const TSharedRef<IPersonaToolkit>& InPersonaToolkit);

	/** Gets the extensibility managers for outside entities to extend persona editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

	/** Import a new asset using the supplied skeleton */
	virtual void ImportNewAsset(USkeleton* InSkeleton, EFBXImportType DefaultImportType);

	/** Check all animations & skeletal meshes for curve usage */
	virtual void TestSkeletonCurveNamesForUse(const TSharedRef<IEditableSkeleton>& InEditableSkeleton) const;

	/** Apply Compression to list of animations */
	virtual void ApplyCompression(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences);

	/** Export to FBX files of the list of animations */
	virtual void ExportToFBX(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences, USkeletalMesh* SkeletalMesh);

	/** Add looping interpolation to the list of animations */
	virtual void AddLoopingInterpolation(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences);

	/** Customize the details of a slot node for the specified details view */
	virtual void CustomizeSlotNodeDetails(const TSharedRef<class IDetailsView>& InDetailsView, FOnInvokeTab InOnInvokeTab);

	/** Create a Persona editor mode manager. Should be destroyed using plain ol' delete. Note: Only FPersonaEditMode-derived modes should be used with this manager! */
	virtual class IPersonaEditorModeManager* CreatePersonaEditorModeManager();

	/** Delegate used to query whether recording is active */
	FIsRecordingActive& OnIsRecordingActive() { return IsRecordingActiveDelegate; }

	/** Delegate used to start recording animation */
	FRecord& OnRecord() { return RecordDelegate; }

	/** Delegate used to stop recording animation */
	FStopRecording& OnStopRecording() { return StopRecordingDelegate; }

	/** Delegate used to get the currently recording animation */
	FGetCurrentRecording& OnGetCurrentRecording() { return GetCurrentRecordingDelegate; }

	/** Delegate used to get the currently recording animation time */
	FGetCurrentRecordingTime& OnGetCurrentRecordingTime() { return GetCurrentRecordingTimeDelegate; }

	/** Delegate broadcast when a preview scene is created */
	virtual FOnPreviewSceneCreated& OnPreviewSceneCreated() { return OnPreviewSceneCreatedDelegate; }

	/** Add common toobar extensions */
	virtual void AddCommonToolbarExtensions(FToolBarBuilder& InToolbarBuilder, TSharedRef<IPersonaToolkit> PersonaToolkit);

private:
	/** When a new anim notify blueprint is created, this will handle post creation work such as adding non-event default nodes */
	void HandleNewAnimNotifyBlueprintCreated(UBlueprint* InBlueprint);

	/** When a new anim notify state blueprint is created, this will handle post creation work such as adding non-event default nodes */
	void HandleNewAnimNotifyStateBlueprintCreated(UBlueprint* InBlueprint);

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/** Delegate used to query whether recording is active */
	FIsRecordingActive IsRecordingActiveDelegate;

	/** Delegate used to start recording animation */
	FRecord RecordDelegate;

	/** Delegate used to stop recording animation */
	FStopRecording StopRecordingDelegate;

	/** Delegate used to get the currently recording animation */
	FGetCurrentRecording GetCurrentRecordingDelegate;

	/** Delegate used to get the currently recording animation time */
	FGetCurrentRecordingTime GetCurrentRecordingTimeDelegate;

	/** Delegate used to tick the skelmesh component recording */
	FTickRecording TickRecordingDelegate;

	/** Delegate broadcast when a preview scene is created */
	FOnPreviewSceneCreated OnPreviewSceneCreatedDelegate;
};

