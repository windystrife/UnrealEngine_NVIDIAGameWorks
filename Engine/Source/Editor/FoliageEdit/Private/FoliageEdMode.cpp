// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FoliageEdMode.h"
#include "SceneView.h"
#include "EditorViewportClient.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FoliageType.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMeshActor.h"
#include "Components/ModelComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineUtils.h"
#include "EditorModeManager.h"
#include "FileHelpers.h"
#include "ScopedTransaction.h"

#include "FoliageEdModeToolkit.h"
#include "LevelEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "FoliageEditActions.h"

#include "AssetRegistryModule.h"

//Slate dependencies
#include "ILevelViewport.h"
#include "Dialogs/DlgPickAssetPath.h"

// Classes
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/BrushComponent.h"

// VR Editor
#include "EditorWorldExtension.h"
#include "VREditorMode.h"
#include "ViewportWorldInteraction.h"
#include "VREditorInteractor.h"
#include "LevelUtils.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#include "FoliageEditUtility.h"

#define LOCTEXT_NAMESPACE "FoliageEdMode"
#define FOLIAGE_SNAP_TRACE (10000.f)

DEFINE_LOG_CATEGORY_STATIC(LogFoliage, Log, Warning);

DECLARE_CYCLE_STAT(TEXT("Calculate Potential Instance"), STAT_FoliageCalculatePotentialInstance, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Add Instance Imp"), STAT_FoliageAddInstanceImp, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Spawn Instance"), STAT_FoliageSpawnInstance, STATGROUP_Foliage);

namespace VREd
{
	static FAutoConsoleVariable FoliageOpacity(TEXT("VREd.FoliageOpacity"), 0.02f, TEXT("The foliage brush opacity."));
}

//
// FFoliageMeshUIInfo
//
FFoliageMeshUIInfo::FFoliageMeshUIInfo(UFoliageType* InSettings)
	: Settings(InSettings)
	, InstanceCountCurrentLevel(0)
	, InstanceCountTotal(0)
{
}

FText FFoliageMeshUIInfo::GetNameText() const
{
	//@todo: this is redundant with FFoliagePaletteItem::DisplayFName, should probably move sorting implementation over to SFoliagePalette
	FName DisplayFName = Settings->GetDisplayFName();
	return FText::FromName(DisplayFName);
}

//
// FFoliageMeshInfo iterator
//
class FFoliageMeshInfoIterator
{
private:
	const UWorld*				World;
	const UFoliageType*			FoliageType;
	FFoliageMeshInfo*			CurrentMeshInfo;
	AInstancedFoliageActor*		CurrentIFA;
	int32						LevelIdx;

public:
	FFoliageMeshInfoIterator(const UWorld* InWorld, const UFoliageType* InFoliageType)
		: World(InWorld)
		, FoliageType(InFoliageType)
		, CurrentMeshInfo(nullptr)
		, CurrentIFA(nullptr)
	{
		// shortcut for non-assets
		if (!InFoliageType->IsAsset())
		{
			LevelIdx = World->GetNumLevels();
			AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(FoliageType->GetOuter());
			if (IFA->GetLevel()->bIsVisible)
			{
				CurrentIFA = IFA;
				CurrentMeshInfo = CurrentIFA->FindMesh(FoliageType);
			}
		}
		else
		{
			LevelIdx = -1;
			++(*this);
		}
	}

	void operator++()
	{
		const int32 NumLevels = World->GetNumLevels();
		int32 LocalLevelIdx = LevelIdx;

		while (++LocalLevelIdx < NumLevels)
		{
			ULevel* Level = World->GetLevel(LocalLevelIdx);
			if (Level && Level->bIsVisible)
			{
				CurrentIFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
				if (CurrentIFA)
				{
					CurrentMeshInfo = CurrentIFA->FindMesh(FoliageType);
					if (CurrentMeshInfo)
					{
						LevelIdx = LocalLevelIdx;
						return;
					}
				}
			}
		}

		CurrentMeshInfo = nullptr;
		CurrentIFA = nullptr;
	}

	FORCEINLINE FFoliageMeshInfo* operator*()
	{
		check(CurrentMeshInfo);
		return CurrentMeshInfo;
	}

	FORCEINLINE operator bool() const
	{
		return CurrentMeshInfo != nullptr;
	}

	FORCEINLINE AInstancedFoliageActor* GetActor()
	{
		return CurrentIFA;
	}
};

//
// Painting filtering options
//
bool FFoliagePaintingGeometryFilter::operator() (const UPrimitiveComponent* Component) const
{
	if (Component)
	{
		// Whitelist
		bool bAllowed =
			(bAllowLandscape   && Component->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass())) ||
			(bAllowStaticMesh  && Component->IsA(UStaticMeshComponent::StaticClass()) && !Component->IsA(UFoliageInstancedStaticMeshComponent::StaticClass())) ||
			(bAllowBSP         && (Component->IsA(UBrushComponent::StaticClass()) || Component->IsA(UModelComponent::StaticClass()))) ||
			(bAllowFoliage     && Component->IsA(UFoliageInstancedStaticMeshComponent::StaticClass()));

		// Blacklist
		bAllowed &=
			(bAllowTranslucent || !(Component->GetMaterial(0) && IsTranslucentBlendMode(Component->GetMaterial(0)->GetBlendMode())));

		return bAllowed;
	}

	return false;
}

//
// FEdModeFoliage
//

/** Constructor */
FEdModeFoliage::FEdModeFoliage()
	: FEdMode()
	, bToolActive(false)
	, bCanAltDrag(false)
	, bAdjustBrushRadius(false)
	, FoliageMeshListSortMode(EColumnSortMode::Ascending)
	, FoliageInteractor(nullptr)
{
	// Load resources and construct brush component
	UStaticMesh* StaticMesh = nullptr;
	if (!IsRunningCommandlet())
	{
		UMaterial* BrushMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorLandscapeResources/FoliageBrushSphereMaterial.FoliageBrushSphereMaterial"), nullptr, LOAD_None, nullptr);
		BrushMID = UMaterialInstanceDynamic::Create(BrushMaterial, GetTransientPackage());
		check(BrushMID != nullptr);

		StaticMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Sphere.Sphere"), nullptr, LOAD_None, nullptr);
	}

	SphereBrushComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), TEXT("SphereBrushComponent"));
	SphereBrushComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SphereBrushComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SphereBrushComponent->SetStaticMesh(StaticMesh);
	SphereBrushComponent->SetMaterial(0, BrushMID);
	SphereBrushComponent->SetAbsolute(true, true, true);
	SphereBrushComponent->CastShadow = false;

	bBrushTraceValid = false;
	BrushLocation = FVector::ZeroVector;
	
	// Get the default opacity from the material.
	FName OpacityParamName("OpacityAmount");
	BrushMID->GetScalarParameterValue(OpacityParamName, DefaultBrushOpacity);

	FFoliageEditCommands::Register();
	UICommandList = MakeShareable(new FUICommandList);
	BindCommands();
}

void FEdModeFoliage::BindCommands()
{
	const FFoliageEditCommands& Commands = FFoliageEditCommands::Get();

	UICommandList->MapAction(
		Commands.IncreaseBrushSize,
		FExecuteAction::CreateRaw(this, &FEdModeFoliage::AdjustBrushRadius, 50.f),
		FCanExecuteAction::CreateRaw(this, &FEdModeFoliage::CurrentToolUsesBrush));

	UICommandList->MapAction(
		Commands.DecreaseBrushSize,
		FExecuteAction::CreateRaw(this, &FEdModeFoliage::AdjustBrushRadius, -50.f),
		FCanExecuteAction::CreateRaw(this, &FEdModeFoliage::CurrentToolUsesBrush));

	UICommandList->MapAction(
		Commands.SetPaint,
		FExecuteAction::CreateRaw(this, &FEdModeFoliage::OnSetPaint),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]
	{
		return UISettings.GetPaintToolSelected();
	}));

	UICommandList->MapAction(
		Commands.SetReapplySettings,
		FExecuteAction::CreateRaw(this, &FEdModeFoliage::OnSetReapplySettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]
	{
		return UISettings.GetReapplyToolSelected();
	}));

	UICommandList->MapAction(
		Commands.SetSelect,
		FExecuteAction::CreateRaw(this, &FEdModeFoliage::OnSetSelectInstance),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]
	{
		return UISettings.GetSelectToolSelected();
	}));

	UICommandList->MapAction(
		Commands.SetLassoSelect,
		FExecuteAction::CreateRaw(this, &FEdModeFoliage::OnSetLasso),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]
	{
		return UISettings.GetLassoSelectToolSelected();
	}));

	UICommandList->MapAction(
		Commands.SetPaintBucket,
		FExecuteAction::CreateRaw(this, &FEdModeFoliage::OnSetPaintFill),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]
	{
		return UISettings.GetPaintBucketToolSelected();
	}));
}

bool FEdModeFoliage::CurrentToolUsesBrush() const
{
	return UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected();
}

/** Destructor */
FEdModeFoliage::~FEdModeFoliage()
{
	// Save UI settings to config file
	UISettings.Save();
	FEditorDelegates::MapChange.RemoveAll(this);
}


/** FGCObject interface */
void FEdModeFoliage::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Call parent implementation
	FEdMode::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(SphereBrushComponent);
}

/** FEdMode: Called when the mode is entered */
void FEdModeFoliage::Enter()
{
	FEdMode::Enter();

	// register for any objects replaced
	GEditor->OnObjectsReplaced().AddRaw(this, &FEdModeFoliage::OnObjectsReplaced);

	// Clear any selection in case the instanced foliage actor is selected
	GEditor->SelectNone(true, true);

	// Load UI settings from config file
	UISettings.Load();

	// Bind to editor callbacks
	FEditorDelegates::NewCurrentLevel.AddSP(this, &FEdModeFoliage::NotifyNewCurrentLevel);
	FWorldDelegates::LevelAddedToWorld.AddSP(this, &FEdModeFoliage::NotifyLevelAddedToWorld);
	FWorldDelegates::LevelRemovedFromWorld.AddSP(this, &FEdModeFoliage::NotifyLevelRemovedFromWorld);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().AddSP(this, &FEdModeFoliage::NotifyAssetRemoved);

	// Force real-time viewports.  We'll back up the current viewport state so we can restore it when the
	// user exits this mode.
	const bool bWantRealTime = true;
	const bool bRememberCurrentState = true;
	ForceRealTimeViewports(bWantRealTime, bRememberCurrentState);

	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShareable(new FFoliageEdModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
	}

	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		ApplySelectionToComponents(GetWorld(), true);
	}

	TArray<AInstancedFoliageActor*> InstanceFoliageActorList;

	// Subscribe to mesh changed events (for existing and future IFA's)
	UWorld* World = GetWorld();
	OnActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateRaw(this, &FEdModeFoliage::HandleOnActorSpawned));
	const int32 NumLevels = World->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = World->GetLevel(LevelIdx);
		if (Level && Level->bIsVisible)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
			
			if (IFA)
			{
				IFA->OnFoliageTypeMeshChanged().AddSP(this, &FEdModeFoliage::HandleOnFoliageTypeMeshChanged);

				InstanceFoliageActorList.Add(IFA);
			}
		}
	}

	// Update UI
	NotifyNewCurrentLevel();

	// Make sure we're up to date and register static mesh bounds changes
	for (FFoliageMeshUIInfoPtr MeshUIInfo : FoliageMeshList)
	{
		for (AInstancedFoliageActor* Actor : InstanceFoliageActorList)
		{
			FFoliageMeshInfo* MeshInfo = Actor->FindMesh(MeshUIInfo->Settings);

			if (MeshInfo != nullptr && MeshInfo->Component != nullptr && MeshInfo->Component->GetStaticMesh() != nullptr)
			{
				MeshInfo->Component->GetStaticMesh()->GetOnExtendedBoundsChanged().AddRaw(MeshInfo, &FFoliageMeshInfo::HandleComponentMeshBoundsChanged);

				MeshInfo->Component->BuildTreeIfOutdated(true, false);
			}
		}
	}

	// Register for VR input events
	UViewportWorldInteraction* ViewportWorldInteraction = Cast<UViewportWorldInteraction>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( GetWorld() )->FindExtension( UViewportWorldInteraction::StaticClass() ) );
	if (ViewportWorldInteraction != nullptr)
	{
		ViewportWorldInteraction->OnViewportInteractionInputAction().RemoveAll(this);
		ViewportWorldInteraction->OnViewportInteractionInputAction().AddRaw(this, &FEdModeFoliage::OnVRAction);

		ViewportWorldInteraction->OnViewportInteractionHoverUpdate().RemoveAll(this);
		ViewportWorldInteraction->OnViewportInteractionHoverUpdate().AddRaw(this, &FEdModeFoliage::OnVRHoverUpdate);

		// Hide the VR transform gizmo while we're in foliage mode.  It sort of gets in the way of painting.
		ViewportWorldInteraction->SetTransformGizmoVisible( false );

		SetBrushOpacity(VREd::FoliageOpacity->GetFloat());
	}

	// Make sure the brush is visible.
	SphereBrushComponent->SetVisibility(true);
}

/** FEdMode: Called when the mode is exited */
void FEdModeFoliage::Exit()
{
	// Unregister VR mode from event handlers
	{
		UViewportWorldInteraction* ViewportWorldInteraction = Cast<UViewportWorldInteraction>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( GetWorld() )->FindExtension( UViewportWorldInteraction::StaticClass() ) );
		if (ViewportWorldInteraction != nullptr)
		{
			// Restore the transform gizmo visibility
			ViewportWorldInteraction->SetTransformGizmoVisible( true );

			ViewportWorldInteraction->OnViewportInteractionInputAction().RemoveAll(this);
			ViewportWorldInteraction->OnViewportInteractionHoverUpdate().RemoveAll(this);
			FoliageInteractor = nullptr;

			// Reset the brush opacity to default.
			SetBrushOpacity(DefaultBrushOpacity);
		}
	}

	FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	Toolkit.Reset();

	// Remove delegates
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);

	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
	}

	GEditor->OnObjectsReplaced().RemoveAll(this);


	// Remove the brush
	SphereBrushComponent->UnregisterComponent();

	// Restore real-time viewport state if we changed it
	const bool bWantRealTime = false;
	const bool bRememberCurrentState = false;
	ForceRealTimeViewports(bWantRealTime, bRememberCurrentState);

	// Clear the cache (safety, should be empty!)
	LandscapeLayerCaches.Empty();

	// Save UI settings to config file
	UISettings.Save();

	// Clear selection visualization on any foliage items
	ApplySelectionToComponents(GetWorld(), false);

	// Remove all event subscriptions
	TArray<AInstancedFoliageActor*> InstanceFoliageActorList;

	UWorld* World = GetWorld();
	World->RemoveOnActorSpawnedHandler(OnActorSpawnedHandle);
	const int32 NumLevels = World->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = World->GetLevel(LevelIdx);
		if (Level && Level->bIsVisible)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
			if (IFA)
			{
				IFA->OnFoliageTypeMeshChanged().RemoveAll(this);

				InstanceFoliageActorList.Add(IFA);
			}
		}
	}

	// Make sure we're up to date and register static mesh bounds changes
	for (FFoliageMeshUIInfoPtr MeshUIInfo : FoliageMeshList)
	{
		for (AInstancedFoliageActor* Actor : InstanceFoliageActorList)
		{
			FFoliageMeshInfo* MeshInfo = Actor->FindMesh(MeshUIInfo->Settings);

			if (MeshInfo != nullptr && MeshInfo->Component != nullptr && MeshInfo->Component->GetStaticMesh() != nullptr)
			{
				MeshInfo->Component->GetStaticMesh()->GetOnExtendedBoundsChanged().RemoveAll(MeshInfo);
			}
		}
	}

	FoliageMeshList.Empty();

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

void FEdModeFoliage::OnVRHoverUpdate(UViewportInteractor* Interactor, FVector& HoverImpactPoint, bool& bWasHandled)
{
	UVREditorMode* VREditorMode = Cast<UVREditorMode>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( GetWorld() )->FindExtension( UVREditorMode::StaticClass() ) );
	if( VREditorMode != nullptr && VREditorMode->IsFullyInitialized() )
	{
		// Check if we're hovering over UI. If so, stop painting so we don't display the preview brush sphere
		if (FoliageInteractor && (FoliageInteractor->IsHoveringOverPriorityType() || FoliageInteractor->GetDraggingMode() != EViewportInteractionDraggingMode::Nothing))
		{
			EndFoliageBrushTrace();
			FoliageInteractor = nullptr;
		}
		// If there isn't currently a foliage interactor and we are hovering over something valid
		else if (FoliageInteractor == nullptr && !Interactor->IsHoveringOverPriorityType() && Interactor->GetHitResultFromLaserPointer().GetActor() != nullptr)
		{
			FoliageInteractor = Interactor;
		}
		// If we aren't hovering over something valid and the tool isn't active
		else if (Interactor->GetHitResultFromLaserPointer().GetActor() == nullptr && !bToolActive)
		{
			FoliageInteractor = nullptr;
		}

		// Skip other interactors if we are painting with one
		if (FoliageInteractor && Interactor == FoliageInteractor)
		{
			// Go ahead and paint immediately
			FVector LaserPointerStart, LaserPointerEnd;
			if (FoliageInteractor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd))
			{
				const FVector LaserPointerDirection = (LaserPointerEnd - LaserPointerStart).GetSafeNormal();

				FoliageBrushTrace(nullptr, LaserPointerStart, LaserPointerDirection);

				if( bBrushTraceValid )
				{
					HoverImpactPoint = BrushLocation;
					bWasHandled = true;
				}
			}
		}
		const bool bBrushMeshVisible = !(FoliageInteractor == nullptr || Interactor->GetDraggingMode() != EViewportInteractionDraggingMode::Nothing);
		SphereBrushComponent->SetVisibility(bBrushMeshVisible);
	}
}

void FEdModeFoliage::OnVRAction(class FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor, const FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled)
{
	UVREditorMode* VREditorMode = Cast<UVREditorMode>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( GetWorld() )->FindExtension( UVREditorMode::StaticClass() ) );
	if (VREditorMode != nullptr && Interactor != nullptr && Interactor->GetDraggingMode() == EViewportInteractionDraggingMode::Nothing)
	{
		const UVREditorInteractor* VREditorInteractor = Cast<UVREditorInteractor>(Interactor);
		if (Action.ActionType == ViewportWorldActionTypes::SelectAndMove && (VREditorInteractor == nullptr || !VREditorMode->IsShowingRadialMenu(VREditorInteractor)))
		{
			if (Action.Event == IE_Pressed && !Interactor->IsHoveringOverPriorityType())
			{
				bWasHandled = true;
				bOutIsInputCaptured = true;

				// Go ahead and paint immediately
				FVector LaserPointerStart, LaserPointerEnd;
				if (Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd))
				{
					const FVector LaserPointerDirection = (LaserPointerEnd - LaserPointerStart).GetSafeNormal();
					BrushTraceDirection = LaserPointerDirection;

					// Only start painting if we're not dragging a widget handle
					if (ViewportClient.GetCurrentWidgetAxis() == EAxisList::None)
					{
						if (UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected())
						{
							StartFoliageBrushTrace(&ViewportClient, Interactor);
							FoliageBrushTrace(&ViewportClient, LaserPointerStart, LaserPointerDirection);
						}

						// Fill a static mesh with foliage brush
						else if (UISettings.GetPaintBucketToolSelected() || UISettings.GetReapplyPaintBucketToolSelected())
						{
							FHitResult HitResult = Interactor->GetHitResultFromLaserPointer();

							if (HitResult.Actor.Get() != nullptr)
							{
								GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));

								if (IsModifierButtonPressed(&ViewportClient))
								{
									ApplyPaintBucket_Remove(HitResult.Actor.Get());
								}
								else
								{
									ApplyPaintBucket_Add(HitResult.Actor.Get());
								}

								GEditor->EndTransaction();
							}
						}
						// Select an instanced foliage
						else if (UISettings.GetSelectToolSelected())
						{
							FHitResult HitResult = Interactor->GetHitResultFromLaserPointer();

							GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));

							if (HitResult.GetActor() != nullptr)
							{
								// Clear all currently selected instances
								SelectInstances(ViewportClient.GetWorld(), false);
								for (auto& FoliageMeshUI : FoliageMeshList)
								{
									UFoliageType* Settings = FoliageMeshUI->Settings;
									SelectInstanceAtLocation(ViewportClient.GetWorld(), Settings, HitResult.ImpactPoint, !IsModifierButtonPressed(&ViewportClient));
								}
							}

							GEditor->EndTransaction();

							// @todo vreditor: we currently don't have a key mapping scheme to snap selected instances to ground 
							// SnapSelectedInstancesToGround(GetWorld());

						}
					}
				}
			}
			// Stop current tracking if the user is no longer painting
			else if (Action.Event == IE_Released && FoliageInteractor && (FoliageInteractor == Interactor))
			{
				EndFoliageBrushTrace();
				FoliageInteractor = nullptr;

				bWasHandled = true;
				bOutIsInputCaptured = false;
			}
		}
	}
}

void FEdModeFoliage::PostUndo()
{
	FEdMode::PostUndo();

	PopulateFoliageMeshList();
}

/** When the user changes the active streaming level with the level browser */
void FEdModeFoliage::NotifyNewCurrentLevel()
{
	PopulateFoliageMeshList();
}

void FEdModeFoliage::NotifyLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld)
{
	PopulateFoliageMeshList();
}

void FEdModeFoliage::NotifyLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	PopulateFoliageMeshList();
}

void FEdModeFoliage::NotifyAssetRemoved(const FAssetData& AssetInfo)
{
	//TODO: This is not properly removing from the foliage actor. However, when we reload it will skip it.
	//We need to properly fix this, but for now this prevents the crash
	if (UFoliageType* FoliageType = Cast<UFoliageType>(AssetInfo.GetAsset()))
	{
		PopulateFoliageMeshList();
	}
	else if (UBlueprint* Blueprint = Cast<UBlueprint>(AssetInfo.GetAsset()))
	{
		PopulateFoliageMeshList();
	}
}

/** When the user changes the current tool in the UI */
void FEdModeFoliage::HandleToolChanged()
{
	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		ApplySelectionToComponents(GetWorld(), true);
	}
	else
	{
		ApplySelectionToComponents(GetWorld(), false);
	}

	OnToolChanged.Broadcast();
}

void FEdModeFoliage::ClearAllToolSelection()
{
	UISettings.SetLassoSelectToolSelected(false);
	UISettings.SetPaintToolSelected(false);
	UISettings.SetReapplyToolSelected(false);
	UISettings.SetSelectToolSelected(false);
	UISettings.SetPaintBucketToolSelected(false);
}

void FEdModeFoliage::OnSetPaint()
{
	ClearAllToolSelection();
	UISettings.SetPaintToolSelected(true);
	HandleToolChanged();
}

void FEdModeFoliage::OnSetReapplySettings()
{
	ClearAllToolSelection();
	UISettings.SetReapplyToolSelected(true);
	HandleToolChanged();
}

void FEdModeFoliage::OnSetSelectInstance()
{
	ClearAllToolSelection();
	UISettings.SetSelectToolSelected(true);
	HandleToolChanged();
}

void FEdModeFoliage::OnSetLasso()
{
	ClearAllToolSelection();
	UISettings.SetLassoSelectToolSelected(true);
	HandleToolChanged();
}

void FEdModeFoliage::OnSetPaintFill()
{
	ClearAllToolSelection();
	UISettings.SetPaintBucketToolSelected(true);
	HandleToolChanged();
}

bool FEdModeFoliage::DisallowMouseDeltaTracking() const
{
	// We never want to use the mouse delta tracker while painting
	return bToolActive;
}

void FEdModeFoliage::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	bool bAnyFoliageTypeReplaced = false;

	UWorld* World = GetWorld();
	ULevel* CurrentLevel = World->GetCurrentLevel();
	const int32 NumLevels = World->GetNumLevels();

	// See if any IFA needs to update a foliage type reference
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = World->GetLevel(LevelIdx);
		if (Level && Level->bIsVisible)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
			if (IFA)
			{
				for (auto& ReplacementPair : ReplacementMap)
				{
					if (UFoliageType* ReplacedFoliageType = Cast<UFoliageType>(ReplacementPair.Key))
					{
						TUniqueObj<FFoliageMeshInfo> MeshInfo;
						if (IFA->FoliageMeshes.RemoveAndCopyValue(ReplacedFoliageType, MeshInfo))
						{
							// Re-add the unique mesh info associated with the replaced foliage type
							IFA->FoliageMeshes.Add(Cast<UFoliageType>(ReplacementPair.Value), MoveTemp(MeshInfo));
							bAnyFoliageTypeReplaced = true;
						}
					}
				}
			}
		}
	}

	if (bAnyFoliageTypeReplaced)
	{
		PopulateFoliageMeshList();
	}
}

void FEdModeFoliage::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	if (bToolActive)
	{
		ApplyBrush(ViewportClient);
	}

	FEdMode::Tick(ViewportClient, DeltaTime);

	if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		// Update pivot
		UpdateWidgetLocationToInstanceSelection();
	}

	// Update the position and size of the brush component
	if (bBrushTraceValid && (UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected()))
	{
		// Scale adjustment is due to default sphere SM size.
		FTransform BrushTransform = FTransform(FQuat::Identity, BrushLocation, FVector(UISettings.GetRadius() * 0.00625f));
		SphereBrushComponent->SetRelativeTransform(BrushTransform);

		if (!SphereBrushComponent->IsRegistered())
		{
			SphereBrushComponent->RegisterComponentWithWorld(ViewportClient->GetWorld());
		}
	}
	else
	{
		if (SphereBrushComponent->IsRegistered())
		{
			SphereBrushComponent->UnregisterComponent();
		}
	}
}

static TArray<ULevel*> CurrentFoliageTraceBrushAffectedLevels;

void FEdModeFoliage::StartFoliageBrushTrace(FEditorViewportClient* ViewportClient, class UViewportInteractor* Interactor)
{
	if (!bToolActive)
	{
		GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));
		if (Interactor)
		{
			FoliageInteractor = Interactor;
		}
		PreApplyBrush();
		ApplyBrush(ViewportClient);

		if (UISettings.IsInAnySingleInstantiationMode())
		{
			EndFoliageBrushTrace();
		}
		else
		{
			bToolActive = true;
		}
	}
}

void FEdModeFoliage::EndFoliageBrushTrace()
{
	GEditor->EndTransaction();
	InstanceSnapshot.Empty();
	LandscapeLayerCaches.Empty();
	bToolActive = false;
	bBrushTraceValid = false;

	for (auto& FoliageMeshUI : FoliageMeshList)
	{
		UFoliageType* Settings = FoliageMeshUI->Settings;

		if (!Settings->IsSelected)
		{
			continue;
		}

		RebuildFoliageTree(Settings);
	}

	CurrentFoliageTraceBrushAffectedLevels.Empty();
}

/** Trace and update brush position */
void FEdModeFoliage::FoliageBrushTrace(FEditorViewportClient* ViewportClient, const FVector& InRayOrigin, const FVector& InRayDirection)
{
	bBrushTraceValid = false;
	if (ViewportClient == nullptr || ( !ViewportClient->IsMovingCamera() && ViewportClient->IsVisible() ) )
	{
		if (UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected())
		{
			const FVector TraceStart(InRayOrigin);
			const FVector TraceEnd(InRayOrigin + InRayDirection * HALF_WORLD_MAX);

			FHitResult Hit;
			UWorld* World = GetWorld();
			static FName NAME_FoliageBrush = FName(TEXT("FoliageBrush"));
			FFoliagePaintingGeometryFilter FilterFunc = FFoliagePaintingGeometryFilter(UISettings);

			if (AInstancedFoliageActor::FoliageTrace(World, Hit, FDesiredFoliageInstance(TraceStart, TraceEnd), NAME_FoliageBrush, false, FilterFunc))
			{
				UPrimitiveComponent* PrimComp = Hit.Component.Get();
				if (PrimComp != nullptr && CanPaint(PrimComp->GetComponentLevel()))
				{
					if (!bAdjustBrushRadius)
					{
						// Adjust the brush location
						BrushLocation = Hit.Location;
						BrushNormal = Hit.Normal;
					}

					// Still want to draw the brush when resizing
					bBrushTraceValid = true;
				}
			}
		}
	}
}

/**
* Called when the mouse is moved over the viewport
*
* @param	InViewportClient	Level editor viewport client that captured the mouse input
* @param	InViewport			Viewport that captured the mouse input
* @param	InMouseX			New mouse cursor X coordinate
* @param	InMouseY			New mouse cursor Y coordinate
*
* @return	true if input was handled
*/
bool FEdModeFoliage::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 MouseX, int32 MouseY)
{
	// Use mouse capture if there's no other interactor currently tracing brush
	UVREditorMode* VREditorMode = Cast<UVREditorMode>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( GetWorld() )->FindExtension( UVREditorMode::StaticClass() ) );
	if (VREditorMode == nullptr || !VREditorMode->IsActive())
	{
		// Compute a world space ray from the screen space mouse coordinates
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			ViewportClient->Viewport,
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags)
			.SetRealtimeUpdate(ViewportClient->IsRealtime()));

		FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);
		BrushTraceDirection = MouseViewportRay.GetDirection();

		FVector BrushTraceStart = MouseViewportRay.GetOrigin();
		if (ViewportClient->IsOrtho())
		{
			BrushTraceStart += -WORLD_MAX * BrushTraceDirection;
		}

		FoliageBrushTrace(ViewportClient, BrushTraceStart, BrushTraceDirection);
	}
	return false;
}

/**
* Called when the mouse is moved while a window input capture is in effect
*
* @param	InViewportClient	Level editor viewport client that captured the mouse input
* @param	InViewport			Viewport that captured the mouse input
* @param	InMouseX			New mouse cursor X coordinate
* @param	InMouseY			New mouse cursor Y coordinate
*
* @return	true if input was handled
*/
bool FEdModeFoliage::CapturedMouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 MouseX, int32 MouseY)
{
	// Use mouse capture if there's no other interactor currently tracing brush
	UVREditorMode* VREditorMode = Cast<UVREditorMode>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( GetWorld() )->FindExtension( UVREditorMode::StaticClass() ) );
	if (VREditorMode == nullptr || !VREditorMode->IsActive())
	{
		//Compute a world space ray from the screen space mouse coordinates
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			ViewportClient->Viewport,
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags)
			.SetRealtimeUpdate(ViewportClient->IsRealtime()));

		FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);
		BrushTraceDirection = MouseViewportRay.GetDirection();

		FVector BrushTraceStart = MouseViewportRay.GetOrigin();
		if (ViewportClient->IsOrtho())
		{
			BrushTraceStart += -WORLD_MAX * BrushTraceDirection;
		}

		FoliageBrushTrace(ViewportClient, BrushTraceStart, BrushTraceDirection);
	}
	return false;
}

void FEdModeFoliage::GetRandomVectorInBrush(FVector& OutStart, FVector& OutEnd)
{
	// Find Rx and Ry inside the unit circle
	float Ru = (2.f * FMath::FRand() - 1.f);
	float Rv = (2.f * FMath::FRand() - 1.f) * FMath::Sqrt(1.f - FMath::Square(Ru));

	// find random point in circle through brush location on the same plane to brush location hit surface normal
	FVector U, V;
	BrushNormal.FindBestAxisVectors(U, V);
	FVector Point = Ru * U + Rv * V;

	// find distance to surface of sphere brush from this point
	FVector Rw = FMath::Sqrt(1.f - (FMath::Square(Ru) + FMath::Square(Rv))) * BrushNormal;

	OutStart = BrushLocation + UISettings.GetRadius() * (Point + Rw);
	OutEnd = BrushLocation + UISettings.GetRadius() * (Point - Rw);
}

static bool IsWithinSlopeAngle(float NormalZ, float MinAngle, float MaxAngle, float Tolerance = SMALL_NUMBER)
{
	const float MaxNormalAngle = FMath::Cos(FMath::DegreesToRadians(MaxAngle));
	const float MinNormalAngle = FMath::Cos(FMath::DegreesToRadians(MinAngle));
	return !(MaxNormalAngle > (NormalZ + Tolerance) || MinNormalAngle < (NormalZ - Tolerance));
}

/** This does not check for overlaps or density */
static bool CheckLocationForPotentialInstance_ThreadSafe(const UFoliageType* Settings, const FVector& Location, const FVector& Normal)
{
	// Check height range
	if (!Settings->Height.Contains(Location.Z))
	{
		return false;
	}

	// Check slope
	// ImpactNormal sometimes is slightly non-normalized, so compare slope with some little deviation
	return IsWithinSlopeAngle(Normal.Z, Settings->GroundSlopeAngle.Min, Settings->GroundSlopeAngle.Max, SMALL_NUMBER);
}

static bool CheckForOverlappingSphere(AInstancedFoliageActor* IFA, const UFoliageType* Settings, const FSphere& Sphere)
{
	if (IFA)
	{
		FFoliageMeshInfo* MeshInfo = IFA->FindMesh(Settings);
		if (MeshInfo)
		{
			return MeshInfo->CheckForOverlappingSphere(Sphere);
		}
	}

	return false;
}

// Returns whether or not there is are any instances overlapping the sphere specified
static bool CheckForOverlappingSphere(const UWorld* InWorld, const UFoliageType* Settings, const FSphere& Sphere)
{
	for (FFoliageMeshInfoIterator It(InWorld, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		if (MeshInfo->CheckForOverlappingSphere(Sphere))
		{
			return true;
		}
	}

	return false;
}

static bool CheckLocationForPotentialInstance(const UWorld* InWorld, const UFoliageType* Settings, const FVector& Location, const FVector& Normal, TArray<FVector>& PotentialInstanceLocations, FFoliageInstanceHash& PotentialInstanceHash)
{
	if (CheckLocationForPotentialInstance_ThreadSafe(Settings, Location, Normal) == false)
	{
		return false;
	}

	// Check if we're too close to any other instances
	if (Settings->Radius > 0.f)
	{
		// Check existing instances. Use the Density radius rather than the minimum radius
		if (CheckForOverlappingSphere(InWorld, Settings, FSphere(Location, Settings->Radius)))
		{
			return false;
		}

		// Check with other potential instances we're about to add.
		const float RadiusSquared = FMath::Square(Settings->Radius);
		auto TempInstances = PotentialInstanceHash.GetInstancesOverlappingBox(FBox::BuildAABB(Location, FVector(Settings->Radius)));
		for (int32 Idx : TempInstances)
		{
			if ((PotentialInstanceLocations[Idx] - Location).SizeSquared() < RadiusSquared)
			{
				return false;
			}
		}
	}

	int32 PotentialIdx = PotentialInstanceLocations.Add(Location);
	PotentialInstanceHash.InsertInstance(Location, PotentialIdx);

	return true;
}

static bool CheckVertexColor(const UFoliageType* Settings, const FColor& VertexColor)
{
	for (uint8 ChannelIdx = 0; ChannelIdx < (uint8)EVertexColorMaskChannel::MAX_None; ++ChannelIdx)
	{
		const FFoliageVertexColorChannelMask& Mask = Settings->VertexColorMaskByChannel[ChannelIdx];

		if (Mask.UseMask)
		{
			uint8 ColorChannel = 0;
			switch ((EVertexColorMaskChannel)ChannelIdx)
			{
			case EVertexColorMaskChannel::Red:
				ColorChannel = VertexColor.R;
				break;
			case EVertexColorMaskChannel::Green:
				ColorChannel = VertexColor.G;
				break;
			case EVertexColorMaskChannel::Blue:
				ColorChannel = VertexColor.B;
				break;
			case EVertexColorMaskChannel::Alpha:
				ColorChannel = VertexColor.A;
				break;
			default:
				// Invalid channel value
				continue;
			}

			if (Mask.InvertMask)
			{
				if (ColorChannel > FMath::RoundToInt(Mask.MaskThreshold * 255.f))
				{
					return false;
				}
			}
			else
			{
				if (ColorChannel < FMath::RoundToInt(Mask.MaskThreshold * 255.f))
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool LandscapeLayersValid(const UFoliageType* Settings)
{
	bool bValid = false;
	for (FName LayerName : Settings->LandscapeLayers)
	{
		bValid |= LayerName != NAME_None;
	}

	return bValid;
}

bool GetMaxHitWeight(const FVector& Location, UActorComponent* Component, const UFoliageType* Settings, FEdModeFoliage::LandscapeLayerCacheData* LandscapeLayerCaches, float& OutMaxHitWeight)
{
	float MaxHitWeight = 0.f;
	if (ULandscapeHeightfieldCollisionComponent* HitLandscapeCollision = Cast<ULandscapeHeightfieldCollisionComponent>(Component))
	{
		if (ULandscapeComponent* HitLandscape = HitLandscapeCollision->RenderComponent.Get())
		{
			for (const FName LandscapeLayerName : Settings->LandscapeLayers)
			{
				// Cache store mapping between component and weight data
				TMap<ULandscapeComponent*, TArray<uint8> >* LandscapeLayerCache = &LandscapeLayerCaches->FindOrAdd(LandscapeLayerName);;
				TArray<uint8>* LayerCache = &LandscapeLayerCache->FindOrAdd(HitLandscape);
				// TODO: FName to LayerInfo?
				const float HitWeight = HitLandscape->GetLayerWeightAtLocation(Location, HitLandscape->GetLandscapeInfo()->GetLayerInfoByName(LandscapeLayerName), LayerCache);
				MaxHitWeight = FMath::Max(MaxHitWeight, HitWeight);
			}

			OutMaxHitWeight = MaxHitWeight;
			return true;
		}
	}

	return false;
}

bool FilterByWeight(float Weight, const UFoliageType* Settings)
{
	const float WeightNeeded = FMath::Max(Settings->MinimumLayerWeight, FMath::FRand());
	return Weight < FMath::Max(SMALL_NUMBER, WeightNeeded);
}

bool FEdModeFoliage::IsUsingVertexColorMask(const UFoliageType* Settings)
{
	for (uint8 ChannelIdx = 0; ChannelIdx < (uint8)EVertexColorMaskChannel::MAX_None; ++ChannelIdx)
	{
		const FFoliageVertexColorChannelMask& Mask = Settings->VertexColorMaskByChannel[ChannelIdx];
		if (Mask.UseMask)
		{
			return true;
		}
	}

	return false;
}

bool FEdModeFoliage::VertexMaskCheck(const FHitResult& Hit, const UFoliageType* Settings)
{
	if (Hit.FaceIndex != INDEX_NONE && IsUsingVertexColorMask(Settings))
	{
		if (UStaticMeshComponent* HitStaticMeshComponent = Cast<UStaticMeshComponent>(Hit.Component.Get()))
		{
			FColor VertexColor;
			if (FEdModeFoliage::GetStaticMeshVertexColorForHit(HitStaticMeshComponent, Hit.FaceIndex, Hit.ImpactPoint, VertexColor))
			{
				if (!CheckVertexColor(Settings, VertexColor))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FEdModeFoliage::SetBrushOpacity(const float InOpacity)
{
	static FName OpacityParamName("OpacityAmount");
	BrushMID->SetScalarParameterValue(OpacityParamName, InOpacity);
}

bool LandscapeLayerCheck(const FHitResult& Hit, const UFoliageType* Settings, FEdModeFoliage::LandscapeLayerCacheData& LandscapeLayersCache, float& OutHitWeight)
{
	OutHitWeight = 1.f;
	if (LandscapeLayersValid(Settings) && GetMaxHitWeight(Hit.ImpactPoint, Hit.Component.Get(), Settings, &LandscapeLayersCache, OutHitWeight))
	{
		// Reject instance randomly in proportion to weight
		if (FilterByWeight(OutHitWeight, Settings))
		{
			return false;
		}
	}

	return true;
}

void FEdModeFoliage::CalculatePotentialInstances_ThreadSafe(const UWorld* InWorld, const UFoliageType* Settings, const TArray<FDesiredFoliageInstance>* DesiredInstances, TArray<FPotentialInstance> OutPotentialInstances[NUM_INSTANCE_BUCKETS], const FFoliageUISettings* UISettings, const int32 StartIdx, const int32 LastIdx, const FFoliagePaintingGeometryFilter* OverrideGeometryFilter)
{
	LandscapeLayerCacheData LocalCache;

	// Reserve space in buckets for a potential instances 
	for (int32 BucketIdx = 0; BucketIdx < NUM_INSTANCE_BUCKETS; ++BucketIdx)
	{
		auto& Bucket = OutPotentialInstances[BucketIdx];
		Bucket.Reserve(DesiredInstances->Num());
	}

	for (int32 InstanceIdx = StartIdx; InstanceIdx <= LastIdx; ++InstanceIdx)
	{
		const FDesiredFoliageInstance& DesiredInst = (*DesiredInstances)[InstanceIdx];
		FHitResult Hit;
		static FName NAME_AddFoliageInstances = FName(TEXT("AddFoliageInstances"));

		FFoliageTraceFilterFunc TraceFilterFunc;
		if (DesiredInst.PlacementMode == EFoliagePlacementMode::Manual && UISettings != nullptr)
		{
			// Enable geometry filters when painting foliage manually
			TraceFilterFunc = FFoliagePaintingGeometryFilter(*UISettings);
		}

		if (OverrideGeometryFilter)
		{
			TraceFilterFunc = *OverrideGeometryFilter;
		}

		if (AInstancedFoliageActor::FoliageTrace(InWorld, Hit, DesiredInst, NAME_AddFoliageInstances, true, TraceFilterFunc))
		{
			float HitWeight = 1.f;
			const bool bValidInstance = CheckLocationForPotentialInstance_ThreadSafe(Settings, Hit.ImpactPoint, Hit.ImpactNormal)
				&& VertexMaskCheck(Hit, Settings)
				&& LandscapeLayerCheck(Hit, Settings, LocalCache, HitWeight);

			if (bValidInstance)
			{
				const int32 BucketIndex = FMath::RoundToInt(HitWeight * (float)(NUM_INSTANCE_BUCKETS - 1));
				OutPotentialInstances[BucketIndex].Add(FPotentialInstance(Hit.ImpactPoint, Hit.ImpactNormal, Hit.Component.Get(), HitWeight, DesiredInst));
			}
		}
	}
}

void FEdModeFoliage::CalculatePotentialInstances(const UWorld* InWorld, const UFoliageType* Settings, const TArray<FDesiredFoliageInstance>& DesiredInstances, TArray<FPotentialInstance> OutPotentialInstances[NUM_INSTANCE_BUCKETS], LandscapeLayerCacheData* LandscapeLayerCachesPtr, const FFoliageUISettings* UISettings, const FFoliagePaintingGeometryFilter* OverrideGeometryFilter)
{
	SCOPE_CYCLE_COUNTER(STAT_FoliageCalculatePotentialInstance);
	
	LandscapeLayerCacheData LocalCache;
	LandscapeLayerCachesPtr = LandscapeLayerCachesPtr ? LandscapeLayerCachesPtr : &LocalCache;

	// Quick lookup of potential instance locations, used for overlapping check.
	TArray<FVector> PotentialInstanceLocations;
	FFoliageInstanceHash PotentialInstanceHash(7);	// use 128x128 cell size, things like brush radius are typically small
	PotentialInstanceLocations.Empty(DesiredInstances.Num());

	// Reserve space in buckets for a potential instances 
	for (int32 BucketIdx = 0; BucketIdx < NUM_INSTANCE_BUCKETS; ++BucketIdx)
	{
		auto& Bucket = OutPotentialInstances[BucketIdx];
		Bucket.Reserve(DesiredInstances.Num());
	}

	for (const FDesiredFoliageInstance& DesiredInst : DesiredInstances)
	{
		FFoliageTraceFilterFunc TraceFilterFunc;
		if (DesiredInst.PlacementMode == EFoliagePlacementMode::Manual && UISettings != nullptr)
		{
			// Enable geometry filters when painting foliage manually
			TraceFilterFunc = FFoliagePaintingGeometryFilter(*UISettings);
		}

		if (OverrideGeometryFilter)
		{
			TraceFilterFunc = *OverrideGeometryFilter;
		}

		FHitResult Hit;
		static FName NAME_AddFoliageInstances = FName(TEXT("AddFoliageInstances"));
		if (AInstancedFoliageActor::FoliageTrace(InWorld, Hit, DesiredInst, NAME_AddFoliageInstances, true, TraceFilterFunc))
		{
			float HitWeight = 1.f;

			UPrimitiveComponent* InstanceBase = Hit.GetComponent();
			check(InstanceBase);

			ULevel* TargetLevel = InstanceBase->GetComponentLevel();
			// We can paint into new level only if FoliageType is shared
			if (!CanPaint(Settings, TargetLevel))
			{
				continue;
			}

			const bool bValidInstance = CheckLocationForPotentialInstance(InWorld, Settings, Hit.ImpactPoint, Hit.ImpactNormal, PotentialInstanceLocations, PotentialInstanceHash)
				&& VertexMaskCheck(Hit, Settings)
				&& LandscapeLayerCheck(Hit, Settings, LocalCache, HitWeight);
			if (bValidInstance)
			{
				const int32 BucketIndex = FMath::RoundToInt(HitWeight * (float)(NUM_INSTANCE_BUCKETS - 1));
				OutPotentialInstances[BucketIndex].Add(FPotentialInstance(Hit.ImpactPoint, Hit.ImpactNormal, InstanceBase, HitWeight, DesiredInst));
			}
		}
	}
}

void FEdModeFoliage::AddInstances(UWorld* InWorld, const TArray<FDesiredFoliageInstance>& DesiredInstances, const FFoliagePaintingGeometryFilter& OverrideGeometryFilter)
{
	TMap<const UFoliageType*, TArray<FDesiredFoliageInstance>> SettingsInstancesMap;
	for (const FDesiredFoliageInstance& DesiredInst : DesiredInstances)
	{
		TArray<FDesiredFoliageInstance>& Instances = SettingsInstancesMap.FindOrAdd(DesiredInst.FoliageType);
		Instances.Add(DesiredInst);
	}

	for (auto It = SettingsInstancesMap.CreateConstIterator(); It; ++It)
	{
		const UFoliageType* FoliageType = It.Key();

		const TArray<FDesiredFoliageInstance>& Instances = It.Value();
		AddInstancesImp(InWorld, FoliageType, Instances, TArray<int32>(), 1.f, nullptr, nullptr, &OverrideGeometryFilter);
	}
}

static void SpawnFoliageInstance(UWorld* InWorld, const UFoliageType* Settings, const FFoliageUISettings* UISettings, const FFoliageInstance& Instance, UActorComponent* BaseComponent)
{
	SCOPE_CYCLE_COUNTER(STAT_FoliageSpawnInstance);
	
	// We always spawn instances in base component level
	ULevel* TargetLevel = (UISettings != nullptr && UISettings->GetIsInSpawnInCurrentLevelMode()) ? InWorld->GetCurrentLevel() : BaseComponent->GetComponentLevel();
	CurrentFoliageTraceBrushAffectedLevels.AddUnique(TargetLevel);

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(TargetLevel, true);

	FFoliageMeshInfo* MeshInfo;
	UFoliageType* FoliageSettings = IFA->AddFoliageType(Settings, &MeshInfo);

	MeshInfo->AddInstance(IFA, FoliageSettings, Instance, BaseComponent, false);
}

void FEdModeFoliage::RebuildFoliageTree(const UFoliageType* Settings)
{
	for (ULevel* AffectedLevel : CurrentFoliageTraceBrushAffectedLevels)
	{
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(AffectedLevel, false);

		if (IFA != nullptr)
		{
			FFoliageMeshInfo* MeshInfo = IFA->FindMesh(Settings);

			if (MeshInfo != nullptr && MeshInfo->Component != nullptr)
			{
				MeshInfo->Component->BuildTreeIfOutdated(true, false);
			}
		}
	}
}

void FEdModeFoliage::AddInstancesImp(UWorld* InWorld, const UFoliageType* Settings, const TArray<FDesiredFoliageInstance>& DesiredInstances, const TArray<int32>& ExistingInstanceBuckets, const float Pressure, LandscapeLayerCacheData* LandscapeLayerCachesPtr, const FFoliageUISettings* UISettings, const FFoliagePaintingGeometryFilter* OverrideGeometryFilter)
{
	SCOPE_CYCLE_COUNTER(STAT_FoliageAddInstanceImp);
	
	if (DesiredInstances.Num() == 0)
	{
		return;
	}

	TArray<FPotentialInstance> PotentialInstanceBuckets[NUM_INSTANCE_BUCKETS];
	if (DesiredInstances[0].PlacementMode == EFoliagePlacementMode::Manual)
	{
		CalculatePotentialInstances(InWorld, Settings, DesiredInstances, PotentialInstanceBuckets, LandscapeLayerCachesPtr, UISettings, OverrideGeometryFilter);
	}
	else
	{
		//@TODO: actual threaded part coming, need parts of this refactor sooner for content team
		CalculatePotentialInstances_ThreadSafe(InWorld, Settings, &DesiredInstances, PotentialInstanceBuckets, nullptr, 0, DesiredInstances.Num() - 1, OverrideGeometryFilter);

		// Existing foliage types in the palette  we want to override any existing mesh settings with the procedural settings.
		TMap<AInstancedFoliageActor*, TArray<const UFoliageType*>> UpdatedTypesByIFA;
		for (TArray<FPotentialInstance>& Bucket : PotentialInstanceBuckets)
		{
			for (auto& PotentialInst : Bucket)
			{
				// Get the IFA for the base component level that contains the component the instance will be placed upon
				AInstancedFoliageActor* TargetIFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(PotentialInst.HitComponent->GetComponentLevel(), true);

				// Update the type in the IFA if needed
				TArray<const UFoliageType*>& UpdatedTypes = UpdatedTypesByIFA.FindOrAdd(TargetIFA);
				if (!UpdatedTypes.Contains(PotentialInst.DesiredInstance.FoliageType))
				{
					UpdatedTypes.Add(PotentialInst.DesiredInstance.FoliageType);
					TargetIFA->AddFoliageType(PotentialInst.DesiredInstance.FoliageType);
				}
			}
		}
	}

	for (int32 BucketIdx = 0; BucketIdx < NUM_INSTANCE_BUCKETS; BucketIdx++)
	{
		TArray<FPotentialInstance>& PotentialInstances = PotentialInstanceBuckets[BucketIdx];
		float BucketFraction = (float)(BucketIdx + 1) / (float)NUM_INSTANCE_BUCKETS;

		// We use the number that actually succeeded in placement (due to parameters) as the target
		// for the number that should be in the brush region.
		const int32 BucketOffset = (ExistingInstanceBuckets.Num() ? ExistingInstanceBuckets[BucketIdx] : 0);
		int32 AdditionalInstances = FMath::Clamp<int32>(FMath::RoundToInt(BucketFraction * (float)(PotentialInstances.Num() - BucketOffset) * Pressure), 0, PotentialInstances.Num());
		for (int32 Idx = 0; Idx < AdditionalInstances; Idx++)
		{
			FPotentialInstance& PotentialInstance = PotentialInstances[Idx];
			FFoliageInstance Inst;
			if (PotentialInstance.PlaceInstance(InWorld, Settings, Inst))
			{
				Inst.ProceduralGuid = PotentialInstance.DesiredInstance.ProceduralGuid;

				SpawnFoliageInstance(InWorld, Settings, UISettings, Inst, PotentialInstance.HitComponent);
			}
		}
	}
}

void FEdModeFoliage::AddSingleInstanceForBrush(UWorld* InWorld, const UFoliageType* Settings, float Pressure)
{
	TArray<FDesiredFoliageInstance> DesiredInstances;
	DesiredInstances.Reserve(1);

	// Simply generate a start/end around the brush location so the line check will hit the brush location
	FVector Start = BrushLocation + BrushNormal;
	FVector End = BrushLocation - BrushNormal;

	FDesiredFoliageInstance* DesiredInstance = new (DesiredInstances)FDesiredFoliageInstance(Start, End);

	// We do not apply the density limitation based on the brush size
	TArray<int32> ExistingInstanceBuckets;
	ExistingInstanceBuckets.AddZeroed(NUM_INSTANCE_BUCKETS);

	AddInstancesImp(InWorld, Settings, DesiredInstances, ExistingInstanceBuckets, Pressure, &LandscapeLayerCaches, &UISettings);
}

/** Add instances inside the brush to match DesiredInstanceCount */
void FEdModeFoliage::AddInstancesForBrush(UWorld* InWorld, const UFoliageType* Settings, const FSphere& BrushSphere, int32 DesiredInstanceCount, float Pressure)
{
	UWorld* World = GetWorld();
	const bool bHasValidLandscapeLayers = LandscapeLayersValid(Settings);

	TArray<int32> ExistingInstanceBuckets;
	ExistingInstanceBuckets.AddZeroed(NUM_INSTANCE_BUCKETS);
	int32 NumExistingInstances = 0;

	for (FFoliageMeshInfoIterator It(World, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		TArray<int32> ExistingInstances;
		MeshInfo->GetInstancesInsideSphere(BrushSphere, ExistingInstances);
		NumExistingInstances += ExistingInstances.Num();

		if (bHasValidLandscapeLayers)
		{
			// Find the landscape weights of existing ExistingInstances
			for (int32 Idx : ExistingInstances)
			{
				FFoliageInstance& Instance = MeshInfo->Instances[Idx];
				auto InstanceBasePtr = It.GetActor()->InstanceBaseCache.GetInstanceBasePtr(Instance.BaseId);
				float HitWeight;
				if (GetMaxHitWeight(Instance.Location, InstanceBasePtr.Get(), Settings, &LandscapeLayerCaches, HitWeight))
				{
					// Add count to bucket.
					ExistingInstanceBuckets[FMath::RoundToInt(HitWeight * (float)(NUM_INSTANCE_BUCKETS - 1))]++;
				}
			}
		}
		else
		{
			// When not tied to a layer, put all the ExistingInstances in the last bucket.
			ExistingInstanceBuckets[NUM_INSTANCE_BUCKETS - 1] = NumExistingInstances;
		}
	}

	if (DesiredInstanceCount > NumExistingInstances)
	{
		TArray<FDesiredFoliageInstance> DesiredInstances;	//we compute instances for the brush
		DesiredInstances.Reserve(DesiredInstanceCount);

		for (int32 DesiredIdx = 0; DesiredIdx < DesiredInstanceCount; DesiredIdx++)
		{
			FVector Start, End;
			GetRandomVectorInBrush(Start, End);
			FDesiredFoliageInstance* DesiredInstance = new (DesiredInstances)FDesiredFoliageInstance(Start, End);
		}

		AddInstancesImp(InWorld, Settings, DesiredInstances, ExistingInstanceBuckets, Pressure, &LandscapeLayerCaches, &UISettings);
	}
}

/** Remove instances inside the brush to match DesiredInstanceCount */
void FEdModeFoliage::RemoveInstancesForBrush(UWorld* InWorld, const UFoliageType* Settings, const FSphere& BrushSphere, int32 DesiredInstanceCount, float Pressure)
{
	for (FFoliageMeshInfoIterator It(InWorld, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		AInstancedFoliageActor* IFA = It.GetActor();

		TArray<int32> PotentialInstancesToRemove;
		MeshInfo->GetInstancesInsideSphere(BrushSphere, PotentialInstancesToRemove);
		if (PotentialInstancesToRemove.Num() == 0)
		{
			continue;
		}

		int32 InstancesToRemove = FMath::RoundToInt((float)(PotentialInstancesToRemove.Num() - DesiredInstanceCount) * Pressure);
		if (InstancesToRemove <= 0)
		{
			continue;
		}

		int32 InstancesToKeep = PotentialInstancesToRemove.Num() - InstancesToRemove;
		if (InstancesToKeep > 0)
		{
			// Remove InstancesToKeep random PotentialInstancesToRemove from the array to leave those PotentialInstancesToRemove behind, and delete all the rest
			for (int32 i = 0; i < InstancesToKeep; i++)
			{
				PotentialInstancesToRemove.RemoveAtSwap(FMath::Rand() % PotentialInstancesToRemove.Num());
			}
		}

		FFoliagePaintingGeometryFilter GeometryFilterFunc(UISettings);

		// Filter PotentialInstancesToRemove
		for (int32 Idx = 0; Idx < PotentialInstancesToRemove.Num(); Idx++)
		{
			auto BaseId = MeshInfo->Instances[PotentialInstancesToRemove[Idx]].BaseId;
			auto BasePtr = IFA->InstanceBaseCache.GetInstanceBasePtr(BaseId);
			UPrimitiveComponent* Base = Cast<UPrimitiveComponent>(BasePtr.Get());

			// Check if instance is candidate for removal based on filter settings
			if (Base && !GeometryFilterFunc(Base))
			{
				// Instance should not be removed, so remove it from the removal list.
				PotentialInstancesToRemove.RemoveAtSwap(Idx);
				Idx--;
			}
		}

		// Remove PotentialInstancesToRemove to reduce it to desired count
		if (PotentialInstancesToRemove.Num() > 0)
		{
			CurrentFoliageTraceBrushAffectedLevels.AddUnique(IFA->GetLevel());

			MeshInfo->RemoveInstances(IFA, PotentialInstancesToRemove, false);
		}
	}
}


void FEdModeFoliage::SelectInstanceAtLocation(UWorld* InWorld, const UFoliageType* Settings, const FVector& Location, bool bSelect)
{
	for (FFoliageMeshInfoIterator It(InWorld, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		AInstancedFoliageActor* IFA = It.GetActor();

		int32 Instance;
		bool bResult;
		MeshInfo->GetInstanceAtLocation(Location, Instance, bResult);
		if (bResult)
		{
		        TArray<int32> Instances;
			Instances.Add(Instance);
			MeshInfo->SelectInstances(IFA, bSelect, Instances);
		}
	}
}

void FEdModeFoliage::SelectInstancesForBrush(UWorld* InWorld, const UFoliageType* Settings, const FSphere& BrushSphere, bool bSelect)
{
	for (FFoliageMeshInfoIterator It(InWorld, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		AInstancedFoliageActor* IFA = It.GetActor();

		TArray<int32> Instances;
		MeshInfo->GetInstancesInsideSphere(BrushSphere, Instances);
		if (Instances.Num() == 0)
		{
			continue;
		}

		MeshInfo->SelectInstances(IFA, bSelect, Instances);
	}
}

void FEdModeFoliage::SelectInstances(const UFoliageType* Settings, bool bSelect)
{
	SelectInstances(GetWorld(), Settings, bSelect);
}

void FEdModeFoliage::SelectInstances(UWorld* InWorld, bool bSelect)
{
	for (auto& FoliageMeshUI : FoliageMeshList)
	{
		UFoliageType* Settings = FoliageMeshUI->Settings;

		if (bSelect && !Settings->IsSelected)
		{
			continue;
		}

		SelectInstances(InWorld, Settings, bSelect);
	}
}

void FEdModeFoliage::SelectInstances(UWorld* InWorld, const UFoliageType* Settings, bool bSelect)
{
	for (FFoliageMeshInfoIterator It(InWorld, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		AInstancedFoliageActor* IFA = It.GetActor();

		MeshInfo->SelectInstances(IFA, bSelect);
	}
}

void FEdModeFoliage::ApplySelectionToComponents(UWorld* InWorld, bool bApply)
{
	const int32 NumLevels = InWorld->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = InWorld->GetLevel(LevelIdx);
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
		if (IFA)
		{
			IFA->ApplySelectionToComponents(bApply);
		}
	}
}

void FEdModeFoliage::TransformSelectedInstances(UWorld* InWorld, const FVector& InDrag, const FRotator& InRot, const FVector& InScale, bool bDuplicate)
{
	const int32 NumLevels = InWorld->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = InWorld->GetLevel(LevelIdx);
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
		if (IFA)
		{
			bool bFoundSelection = false;

			for (auto& MeshPair : IFA->FoliageMeshes)
			{
				FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
				TArray<int32> SelectedIndices = MeshInfo.SelectedIndices.Array();

				if (SelectedIndices.Num() > 0)
				{
					// Mark actor once we found selection
					if (!bFoundSelection)
					{
						IFA->Modify();
						bFoundSelection = true;
					}

					if (bDuplicate)
					{
						MeshInfo.DuplicateInstances(IFA, MeshPair.Key, SelectedIndices);
					}

					MeshInfo.PreMoveInstances(IFA, SelectedIndices);

					for (int32 SelectedInstanceIdx : SelectedIndices)
					{
						FFoliageInstance& Instance = MeshInfo.Instances[SelectedInstanceIdx];
						Instance.Location += InDrag;
						Instance.ZOffset = 0.f;
						Instance.Rotation += InRot;
						Instance.DrawScale3D += InScale;
					}

					MeshInfo.PostMoveInstances(IFA, SelectedIndices);
				}
			}

			if (bFoundSelection)
			{
				IFA->MarkComponentsRenderStateDirty();
			}
		}
	}
}

AInstancedFoliageActor* FEdModeFoliage::GetSelectionLocation(UWorld* InWorld, FVector& OutLocation) const
{
	// Prefer current level
	{
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(InWorld);
		if (IFA && IFA->GetSelectionLocation(OutLocation))
		{
			return IFA;
		}
	}

	// Go through all sub-levels
	const int32 NumLevels = InWorld->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = InWorld->GetLevel(LevelIdx);
		if (Level != InWorld->GetCurrentLevel())
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
			if (IFA && IFA->GetSelectionLocation(OutLocation))
			{
				return IFA;
			}
		}
	}

	return nullptr;
}

void FEdModeFoliage::UpdateWidgetLocationToInstanceSelection()
{
	FVector SelectionLocation = FVector::ZeroVector;
	AInstancedFoliageActor* IFA = GetSelectionLocation(GetWorld(), SelectionLocation);
	Owner->PivotLocation = Owner->SnappedLocation = SelectionLocation;
	//if (IFA)
	//{
	//	IFA->MarkComponentsRenderStateDirty();
	//}
}

void FEdModeFoliage::RemoveSelectedInstances(UWorld* InWorld)
{
	GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));

	const int32 NumLevels = InWorld->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = InWorld->GetLevel(LevelIdx);
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
		if (IFA)
		{
			bool bHasSelection = false;
			for (auto& MeshPair : IFA->FoliageMeshes)
			{
				if (MeshPair.Value->SelectedIndices.Num())
				{
					bHasSelection = true;
					break;
				}
			}

			if (bHasSelection)
			{
				IFA->Modify();
				for (auto& MeshPair : IFA->FoliageMeshes)
				{
					FFoliageMeshInfo& Mesh = *MeshPair.Value;
					if (Mesh.SelectedIndices.Num() > 0)
					{
						TArray<int32> InstancesToDelete = Mesh.SelectedIndices.Array();
						Mesh.RemoveInstances(IFA, InstancesToDelete, false);
						Mesh.Component->BuildTreeIfOutdated(true, true);

						OnInstanceCountUpdated(MeshPair.Key);
					}
				}
			}
		}
	}

	GEditor->EndTransaction();
}

TAutoConsoleVariable<float> CVarOffGroundTreshold(
	TEXT("foliage.OffGroundThreshold"),
	5.0f,
	TEXT("Maximum distance from base component (in local space) at which instance is still considered as valid"));

void FEdModeFoliage::SelectInvalidInstances(const UFoliageType* Settings)
{
	UWorld* InWorld = GetWorld();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FoliageGroundCheck), true);
	QueryParams.bReturnFaceIndex = false;
	FCollisionShape SphereShape;
	SphereShape.SetSphere(0.f);
	float InstanceOffGroundLocalThreshold = CVarOffGroundTreshold.GetValueOnGameThread();

	for (FFoliageMeshInfoIterator It(InWorld, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		AInstancedFoliageActor* IFA = It.GetActor();
		int32 NumInstances = MeshInfo->Instances.Num();
		TArray<FHitResult> Hits; Hits.Reserve(16);

		TArray<int32> InvalidInstances;

		for (int32 InstanceIdx = 0; InstanceIdx < NumInstances; ++InstanceIdx)
		{
			FFoliageInstance& Instance = MeshInfo->Instances[InstanceIdx];
			UActorComponent* CurrentInstanceBase = IFA->InstanceBaseCache.GetInstanceBasePtr(Instance.BaseId).Get();
			bool bInvalidInstance = true;

			if (CurrentInstanceBase != nullptr)
			{
				FVector InstanceTraceRange = Instance.GetInstanceWorldTransform().TransformVector(FVector(0.f, 0.f, 1000.f));
				FVector Start = Instance.Location + InstanceTraceRange;
				FVector End = Instance.Location - InstanceTraceRange;

				InWorld->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), SphereShape, QueryParams);

				for (const FHitResult& Hit : Hits)
				{
					UPrimitiveComponent* HitComponent = Hit.GetComponent();
					check(HitComponent);

					if (HitComponent->IsCreatedByConstructionScript())
					{
						continue;
					}

					UModelComponent* ModelComponent = Cast<UModelComponent>(HitComponent);
					if (ModelComponent)
					{
						ABrush* BrushActor = ModelComponent->GetModel()->FindBrush(Hit.Location);
						if (BrushActor)
						{
							HitComponent = BrushActor->GetBrushComponent();
						}
					}

					if (HitComponent == CurrentInstanceBase)
					{
						FVector InstanceWorldZOffset = Instance.GetInstanceWorldTransform().TransformVector(FVector(0.f, 0.f, Instance.ZOffset));
						float DistanceToGround = FVector::Dist(Instance.Location, Hit.Location + InstanceWorldZOffset);
						float InstanceWorldTreshold = Instance.GetInstanceWorldTransform().TransformVector(FVector(0.f, 0.f, InstanceOffGroundLocalThreshold)).Size();

						if ((DistanceToGround - InstanceWorldTreshold) <= KINDA_SMALL_NUMBER)
						{
							bInvalidInstance = false;
						}
					}
				}
			}

			if (bInvalidInstance)
			{
				InvalidInstances.Add(InstanceIdx);
			}
		}

		if (InvalidInstances.Num() > 0)
		{
			MeshInfo->SelectInstances(IFA, true, InvalidInstances);
		}
	}
}

void FEdModeFoliage::AdjustBrushRadius(float Adjustment)
{
	if (UISettings.IsInAnySingleInstantiationMode())
	{
		return;
	}

	const float CurrentBrushRadius = UISettings.GetRadius();

	if (Adjustment > 0.f)
	{
		UISettings.SetRadius(FMath::Min(CurrentBrushRadius + Adjustment, 8192.f));
	}
	else if (Adjustment < 0.f)
	{
		UISettings.SetRadius(FMath::Max(CurrentBrushRadius + Adjustment, 0.f));
	}
}

void FEdModeFoliage::ReapplyInstancesForBrush(UWorld* InWorld, const UFoliageType* Settings, const FSphere& BrushSphere, float Pressure)
{
	// Adjust instance density first
	ReapplyInstancesDensityForBrush(InWorld, Settings, BrushSphere, Pressure);

	for (FFoliageMeshInfoIterator It(InWorld, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		AInstancedFoliageActor* IFA = It.GetActor();

		ReapplyInstancesForBrush(InWorld, IFA, Settings, MeshInfo, BrushSphere, Pressure);
	}
}

/** Reapply instance settings to exiting instances */
void FEdModeFoliage::ReapplyInstancesForBrush(UWorld* InWorld, AInstancedFoliageActor* IFA, const UFoliageType* Settings, FFoliageMeshInfo* MeshInfo, const FSphere& BrushSphere, float Pressure)
{
	TArray<int32> ExistingInstances;
	MeshInfo->GetInstancesInsideSphere(BrushSphere, ExistingInstances);

	bool bUpdated = false;
	TArray<int32> UpdatedInstances;
	TSet<int32> InstancesToDelete;

	IFA->Modify();

	for (int32 Idx = 0; Idx < ExistingInstances.Num(); Idx++)
	{
		int32 InstanceIndex = ExistingInstances[Idx];
		FFoliageInstance& Instance = MeshInfo->Instances[InstanceIndex];

		if ((Instance.Flags & FOLIAGE_Readjusted) == 0)
		{
			// record that we've made changes to this instance already, so we don't touch it again.
			Instance.Flags |= FOLIAGE_Readjusted;

			// See if we need to update the location in the instance hash
			bool bReapplyLocation = false;
			FVector OldInstanceLocation = Instance.Location;

			// remove any Z offset first, so the offset is reapplied to any new 
			if (FMath::Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER)
			{
				Instance.Location = Instance.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, -Instance.ZOffset));
				bReapplyLocation = true;
			}


			// Defer normal reapplication 
			bool bReapplyNormal = false;

			// Reapply normal alignment
			if (Settings->ReapplyAlignToNormal)
			{
				if (Settings->AlignToNormal)
				{
					if ((Instance.Flags & FOLIAGE_AlignToNormal) == 0)
					{
						bReapplyNormal = true;
						bUpdated = true;
					}
				}
				else
				{
					if (Instance.Flags & FOLIAGE_AlignToNormal)
					{
						Instance.Rotation = Instance.PreAlignRotation;
						Instance.Flags &= (~FOLIAGE_AlignToNormal);
						bUpdated = true;
					}
				}
			}

			// Reapply random yaw
			if (Settings->ReapplyRandomYaw)
			{
				if (Settings->RandomYaw)
				{
					if (Instance.Flags & FOLIAGE_NoRandomYaw)
					{
						// See if we need to remove any normal alignment first
						if (!bReapplyNormal && (Instance.Flags & FOLIAGE_AlignToNormal))
						{
							Instance.Rotation = Instance.PreAlignRotation;
							bReapplyNormal = true;
						}
						Instance.Rotation.Yaw = FMath::FRand() * 360.f;
						Instance.Flags &= (~FOLIAGE_NoRandomYaw);
						bUpdated = true;
					}
				}
				else
				{
					if ((Instance.Flags & FOLIAGE_NoRandomYaw) == 0)
					{
						// See if we need to remove any normal alignment first
						if (!bReapplyNormal && (Instance.Flags & FOLIAGE_AlignToNormal))
						{
							Instance.Rotation = Instance.PreAlignRotation;
							bReapplyNormal = true;
						}
						Instance.Rotation.Yaw = 0.f;
						Instance.Flags |= FOLIAGE_NoRandomYaw;
						bUpdated = true;
					}
				}
			}

			// Reapply random pitch angle
			if (Settings->ReapplyRandomPitchAngle)
			{
				// See if we need to remove any normal alignment first
				if (!bReapplyNormal && (Instance.Flags & FOLIAGE_AlignToNormal))
				{
					Instance.Rotation = Instance.PreAlignRotation;
					bReapplyNormal = true;
				}

				Instance.Rotation.Pitch = FMath::FRand() * Settings->RandomPitchAngle;
				Instance.Flags |= FOLIAGE_NoRandomYaw;

				bUpdated = true;
			}

			// Reapply scale
			if (Settings->ReapplyScaling)
			{
				FVector NewScale = Settings->GetRandomScale();

				if (Settings->ReapplyScaleX)
				{
					if (Settings->Scaling == EFoliageScaling::Uniform)
					{
						Instance.DrawScale3D = NewScale;
					}
					else
					{
						Instance.DrawScale3D.X = NewScale.X;
					}
					bUpdated = true;
				}

				if (Settings->ReapplyScaleY)
				{
					Instance.DrawScale3D.Y = NewScale.Y;
					bUpdated = true;
				}

				if (Settings->ReapplyScaleZ)
				{
					Instance.DrawScale3D.Z = NewScale.Z;
					bUpdated = true;
				}
			}

			// Reapply ZOffset
			if (Settings->ReapplyZOffset)
			{
				Instance.ZOffset = Settings->ZOffset.Interpolate(FMath::FRand());
				bUpdated = true;
			}

			// Find a ground normal for either normal or ground slope check.
			if (bReapplyNormal || Settings->ReapplyGroundSlope || Settings->ReapplyVertexColorMask || (Settings->ReapplyLandscapeLayers && LandscapeLayersValid(Settings)))
			{
				FHitResult Hit;
				static const FName NAME_ReapplyInstancesForBrush = TEXT("ReapplyInstancesForBrush");

				// trace along the mesh's Z axis.
				FVector ZAxis = Instance.Rotation.Quaternion().GetAxisZ();
				FVector Start = Instance.Location + 16.f * ZAxis;
				FVector End = Instance.Location - 16.f * ZAxis;
				if (AInstancedFoliageActor::FoliageTrace(InWorld, Hit, FDesiredFoliageInstance(Start, End), NAME_ReapplyInstancesForBrush, true))
				{
					// Reapply the normal
					if (bReapplyNormal)
					{
						Instance.PreAlignRotation = Instance.Rotation;
						Instance.AlignToNormal(Hit.Normal, Settings->AlignMaxAngle);
					}

					// Cull instances that don't meet the ground slope check.
					if (Settings->ReapplyGroundSlope)
					{
						if (!IsWithinSlopeAngle(Hit.Normal.Z, Settings->GroundSlopeAngle.Min, Settings->GroundSlopeAngle.Max))
						{
							InstancesToDelete.Add(InstanceIndex);
							if (bReapplyLocation)
							{
								// restore the location so the hash removal will succeed
								Instance.Location = OldInstanceLocation;
							}
							continue;
						}
					}

					// Cull instances for the landscape layer
					if (Settings->ReapplyLandscapeLayers && LandscapeLayersValid(Settings))
					{
						float HitWeight = 1.f;
						if (GetMaxHitWeight(Hit.Location, Hit.GetComponent(), Settings, &LandscapeLayerCaches, HitWeight))
						{
							if (FilterByWeight(HitWeight, Settings))
							{
								InstancesToDelete.Add(InstanceIndex);
								if (bReapplyLocation)
								{
									// restore the location so the hash removal will succeed
									Instance.Location = OldInstanceLocation;
								}
								continue;
							}
						}
					}

					// Reapply vertex color mask
					if (Settings->ReapplyVertexColorMask)
					{
						if (Hit.FaceIndex != INDEX_NONE && IsUsingVertexColorMask(Settings))
						{
							UStaticMeshComponent* HitStaticMeshComponent = Cast<UStaticMeshComponent>(Hit.Component.Get());
							if (HitStaticMeshComponent)
							{
								FColor VertexColor;
								if (GetStaticMeshVertexColorForHit(HitStaticMeshComponent, Hit.FaceIndex, Hit.Location, VertexColor))
								{
									if (!CheckVertexColor(Settings, VertexColor))
									{
										InstancesToDelete.Add(InstanceIndex);
										if (bReapplyLocation)
										{
											// restore the location so the hash removal will succeed
											Instance.Location = OldInstanceLocation;
										}
										continue;
									}
								}
							}
						}
					}
				}
			}

			// Cull instances that don't meet the height range
			if (Settings->ReapplyHeight)
			{
				if (!Settings->Height.Contains(Instance.Location.Z))
				{
					InstancesToDelete.Add(InstanceIndex);
					if (bReapplyLocation)
					{
						// restore the location so the hash removal will succeed
						Instance.Location = OldInstanceLocation;
					}
					continue;
				}
			}

			if (bUpdated && FMath::Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER)
			{
				// Reapply the Z offset in new local space
				Instance.Location = Instance.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, Instance.ZOffset));
				bReapplyLocation = true;
			}

			// Update the hash
			if (bReapplyLocation)
			{
				MeshInfo->InstanceHash->RemoveInstance(OldInstanceLocation, InstanceIndex);
				MeshInfo->InstanceHash->InsertInstance(Instance.Location, InstanceIndex);
			}

			// Cull overlapping based on radius
			if (Settings->ReapplyRadius && Settings->Radius > 0.f)
			{
				if (MeshInfo->CheckForOverlappingInstanceExcluding(InstanceIndex, Settings->Radius, InstancesToDelete))
				{
					InstancesToDelete.Add(InstanceIndex);
					continue;
				}
			}

			// Remove mesh collide with world
			if (Settings->ReapplyCollisionWithWorld)
			{
				FHitResult Hit;
				static const FName NAME_ReapplyInstancesForBrush = TEXT("ReapplyCollisionWithWorld");
				FVector Start = Instance.Location + FVector(0.f, 0.f, 16.f);
				FVector End = Instance.Location - FVector(0.f, 0.f, 16.f);
				if (AInstancedFoliageActor::FoliageTrace(InWorld, Hit, FDesiredFoliageInstance(Start, End), NAME_ReapplyInstancesForBrush))
				{
					if (!AInstancedFoliageActor::CheckCollisionWithWorld(InWorld, Settings, Instance, Hit.Normal, Hit.Location, Hit.Component.Get()))
					{
						InstancesToDelete.Add(InstanceIndex);
						continue;
					}
				}
				else
				{
					InstancesToDelete.Add(InstanceIndex);
				}
			}

			if (bUpdated)
			{
				UpdatedInstances.Add(InstanceIndex);
			}
		}
	}

	if (UpdatedInstances.Num() > 0)
	{
		MeshInfo->PostUpdateInstances(IFA, UpdatedInstances);
		IFA->RegisterAllComponents();
	}

	if (InstancesToDelete.Num())
	{
		MeshInfo->RemoveInstances(IFA, InstancesToDelete.Array(), true);
	}
}

void FEdModeFoliage::ReapplyInstancesDensityForBrush(UWorld* InWorld, const UFoliageType* Settings, const FSphere& BrushSphere, float Pressure)
{
	if (Settings->ReapplyDensity && !FMath::IsNearlyEqual(Settings->DensityAdjustmentFactor, 1.f))
	{
		// Determine number of instances at the start of the brush stroke
		int32 SnapshotInstanceCount = 0;
		TArray<const FMeshInfoSnapshot*> SnapshotList;
		InstanceSnapshot.MultiFindPointer(const_cast<UFoliageType*>(Settings), SnapshotList);
		for (const auto* Snapshot : SnapshotList)
		{
			SnapshotInstanceCount += Snapshot->CountInstancesInsideSphere(BrushSphere);
		}

		// Determine desired number of instances
		int32 DesiredInstanceCount = FMath::RoundToInt((float)SnapshotInstanceCount * Settings->DensityAdjustmentFactor);

		if (Settings->DensityAdjustmentFactor > 1.f)
		{
			AddInstancesForBrush(InWorld, Settings, BrushSphere, DesiredInstanceCount, Pressure);
		}
		else if (Settings->DensityAdjustmentFactor < 1.f)
		{
			RemoveInstancesForBrush(InWorld, Settings, BrushSphere, DesiredInstanceCount, Pressure);
		}
	}
}

void FEdModeFoliage::PreApplyBrush()
{
	InstanceSnapshot.Empty();

	UWorld* World = GetWorld();
	// Special setup beginning a stroke with the Reapply tool
	// Necessary so we don't keep reapplying settings over and over for the same instances.
	if (UISettings.GetReapplyToolSelected())
	{
		for (auto& FoliageMeshUI : FoliageMeshList)
		{
			UFoliageType* Settings = FoliageMeshUI->Settings;

			if (!Settings->IsSelected)
			{
				continue;
			}

			for (FFoliageMeshInfoIterator It(World, Settings); It; ++It)
			{
				FFoliageMeshInfo* MeshInfo = (*It);

				// Take a snapshot of all the locations
				InstanceSnapshot.Add(Settings, FMeshInfoSnapshot(MeshInfo));

				// Clear the "FOLIAGE_Readjusted" flag
				for (auto& Instance : MeshInfo->Instances)
				{
					Instance.Flags &= (~FOLIAGE_Readjusted);
				}
			}
		}
	}
}

void FEdModeFoliage::ApplyBrush(FEditorViewportClient* ViewportClient)
{
	if (!bBrushTraceValid || ViewportClient != GCurrentLevelEditingViewportClient)
	{
		return;
	}

	float BrushArea = PI * FMath::Square(UISettings.GetRadius());

	// Tablet pressure or motion controller pressure
	const UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>(FoliageInteractor);
	const float Pressure = VRInteractor ? VRInteractor->GetSelectAndMoveTriggerValue() : ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

	// Cache a copy of the world pointer
	UWorld* World = ViewportClient->GetWorld();

	for (auto& FoliageMeshUI : FoliageMeshList)
	{
		UFoliageType* Settings = FoliageMeshUI->Settings;

		if (!Settings->IsSelected)
		{
			continue;
		}

		FSphere BrushSphere(BrushLocation, UISettings.GetRadius());

		if (UISettings.GetLassoSelectToolSelected())
		{
			SelectInstancesForBrush(World, Settings, BrushSphere, !IsModifierButtonPressed(ViewportClient));
		}
		else if (UISettings.GetReapplyToolSelected())
		{
			// Reapply any settings checked by the user
			ReapplyInstancesForBrush(World, Settings, BrushSphere, Pressure);
		}
		else if (UISettings.GetPaintToolSelected())
		{
			if (IsModifierButtonPressed(ViewportClient))
			{
				int32 DesiredInstanceCount = FMath::RoundToInt(BrushArea * Settings->Density * UISettings.GetUnpaintDensity() / (1000.f*1000.f));

				RemoveInstancesForBrush(World, Settings, BrushSphere, DesiredInstanceCount, Pressure);
			}
			else
			{
				if (UISettings.IsInAnySingleInstantiationMode())
				{
					AddSingleInstanceForBrush(World, Settings, Pressure);
				}
				else
				{
					// This is the total set of instances disregarding parameters like slope, height or layer.
					float DesiredInstanceCountFloat = BrushArea * Settings->Density * UISettings.GetPaintDensity() / (1000.f*1000.f);
					// Allow a single instance with a random chance, if the brush is smaller than the density
					int32 DesiredInstanceCount = DesiredInstanceCountFloat > 1.f ? FMath::RoundToInt(DesiredInstanceCountFloat) : FMath::FRand() < DesiredInstanceCountFloat ? 1 : 0;

					AddInstancesForBrush(World, Settings, BrushSphere, DesiredInstanceCount, Pressure);
				}
			}
		}

		OnInstanceCountUpdated(Settings);
	}

	if (UISettings.GetLassoSelectToolSelected())
	{
		UpdateWidgetLocationToInstanceSelection();
	}
}

struct FFoliagePaintBucketTriangle
{
	FFoliagePaintBucketTriangle(const FTransform& InLocalToWorld, const FVector& InVertex0, const FVector& InVertex1, const FVector& InVertex2, const FColor InColor0, const FColor InColor1, const FColor InColor2)
	{
		Vertex = InLocalToWorld.TransformPosition(InVertex0);
		Vector1 = InLocalToWorld.TransformPosition(InVertex1) - Vertex;
		Vector2 = InLocalToWorld.TransformPosition(InVertex2) - Vertex;
		VertexColor[0] = InColor0;
		VertexColor[1] = InColor1;
		VertexColor[2] = InColor2;

		WorldNormal = InLocalToWorld.GetDeterminant() >= 0.f ? Vector2 ^ Vector1 : Vector1 ^ Vector2;
		float WorldNormalSize = WorldNormal.Size();
		Area = WorldNormalSize * 0.5f;
		if (WorldNormalSize > SMALL_NUMBER)
		{
			WorldNormal /= WorldNormalSize;
		}
	}

	void GetRandomPoint(FVector& OutPoint, FColor& OutBaryVertexColor)
	{
		// Sample parallelogram
		float x = FMath::FRand();
		float y = FMath::FRand();

		// Flip if we're outside the triangle
		if (x + y > 1.f)
		{
			x = 1.f - x;
			y = 1.f - y;
		}

		OutBaryVertexColor = ((1.f - x - y) * VertexColor[0] + x * VertexColor[1] + y * VertexColor[2]).ToFColor(true);
		OutPoint = Vertex + x * Vector1 + y * Vector2;
	}

	FVector	Vertex;
	FVector Vector1;
	FVector Vector2;
	FVector WorldNormal;
	float Area;
	FColor VertexColor[3];
};

/** Apply paint bucket to actor */
void FEdModeFoliage::ApplyPaintBucket_Remove(AActor* Actor)
{
	UWorld* World = Actor->GetWorld();

	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	// Remove all instances of the selected meshes
	for (const auto& MeshUIInfo : FoliageMeshList)
	{
		UFoliageType* FoliageType = MeshUIInfo->Settings;
		if (!FoliageType->IsSelected)
		{
			continue;
		}

		// Go through all FoliageActors in the world and delete 
		for (FFoliageMeshInfoIterator It(World, FoliageType); It; ++It)
		{
			AInstancedFoliageActor* IFA = It.GetActor();

			for (auto Component : Components)
			{
				IFA->DeleteInstancesForComponent(Component, FoliageType);
			}
		}

		OnInstanceCountUpdated(FoliageType);
	}
}

/** Apply paint bucket to actor */
void FEdModeFoliage::ApplyPaintBucket_Add(AActor* Actor)
{
	UWorld* World = Actor->GetWorld();
	TMap<UPrimitiveComponent*, TArray<FFoliagePaintBucketTriangle> > ComponentPotentialTriangles;

	// Check all the components of the hit actor
	TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents;
	Actor->GetComponents(StaticMeshComponents);

	for (auto StaticMeshComponent : StaticMeshComponents)
	{
		UMaterialInterface* Material = StaticMeshComponent->GetMaterial(0);

		if (UISettings.bFilterStaticMesh && StaticMeshComponent->GetStaticMesh() && StaticMeshComponent->GetStaticMesh()->RenderData &&
			(UISettings.bFilterTranslucent || !Material || !IsTranslucentBlendMode(Material->GetBlendMode())))
		{
			UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
			FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];
			TArray<FFoliagePaintBucketTriangle>& PotentialTriangles = ComponentPotentialTriangles.Add(StaticMeshComponent, TArray<FFoliagePaintBucketTriangle>());

			bool bHasInstancedColorData = false;
			const FStaticMeshComponentLODInfo* InstanceMeshLODInfo = nullptr;
			if (StaticMeshComponent->LODData.Num() > 0)
			{
				InstanceMeshLODInfo = StaticMeshComponent->LODData.GetData();
				bHasInstancedColorData = InstanceMeshLODInfo->PaintedVertices.Num() == LODModel.VertexBuffer.GetNumVertices();
			}

			const bool bHasColorData = bHasInstancedColorData || LODModel.ColorVertexBuffer.GetNumVertices();

			// Get the raw triangle data for this static mesh
			FTransform LocalToWorld = StaticMeshComponent->GetComponentTransform();
			FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
			const FPositionVertexBuffer& PositionVertexBuffer = LODModel.PositionVertexBuffer;
			const FColorVertexBuffer& ColorVertexBuffer = LODModel.ColorVertexBuffer;

			if (USplineMeshComponent* SplineMesh = Cast<USplineMeshComponent>(StaticMeshComponent))
			{
				// Transform spline mesh verts correctly
				FVector Mask = FVector(1, 1, 1);
				USplineMeshComponent::GetAxisValue(Mask, SplineMesh->ForwardAxis) = 0;

				for (int32 Idx = 0; Idx < Indices.Num(); Idx += 3)
				{
					const int32 Index0 = Indices[Idx];
					const int32 Index1 = Indices[Idx + 1];
					const int32 Index2 = Indices[Idx + 2];

					const FVector Vert0 = SplineMesh->CalcSliceTransform(USplineMeshComponent::GetAxisValue(PositionVertexBuffer.VertexPosition(Index0), SplineMesh->ForwardAxis)).TransformPosition(PositionVertexBuffer.VertexPosition(Index0) * Mask);
					const FVector Vert1 = SplineMesh->CalcSliceTransform(USplineMeshComponent::GetAxisValue(PositionVertexBuffer.VertexPosition(Index1), SplineMesh->ForwardAxis)).TransformPosition(PositionVertexBuffer.VertexPosition(Index1) * Mask);
					const FVector Vert2 = SplineMesh->CalcSliceTransform(USplineMeshComponent::GetAxisValue(PositionVertexBuffer.VertexPosition(Index2), SplineMesh->ForwardAxis)).TransformPosition(PositionVertexBuffer.VertexPosition(Index2) * Mask);

					new(PotentialTriangles)FFoliagePaintBucketTriangle(LocalToWorld
						, Vert0
						, Vert1
						, Vert2
						, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index0].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index0) : FColor::White)
						, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index1].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index1) : FColor::White)
						, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index2].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index2) : FColor::White)
						);
				}
			}
			else
			{
				// Build a mapping of vertex positions to vertex colors.  Using a TMap will allow for fast lookups so we can match new static mesh vertices with existing colors 
				for (int32 Idx = 0; Idx < Indices.Num(); Idx += 3)
				{
					const int32 Index0 = Indices[Idx];
					const int32 Index1 = Indices[Idx + 1];
					const int32 Index2 = Indices[Idx + 2];

					new(PotentialTriangles)FFoliagePaintBucketTriangle(LocalToWorld
						, PositionVertexBuffer.VertexPosition(Index0)
						, PositionVertexBuffer.VertexPosition(Index1)
						, PositionVertexBuffer.VertexPosition(Index2)
						, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index0].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index0) : FColor::White)
						, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index1].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index1) : FColor::White)
						, bHasInstancedColorData ? InstanceMeshLODInfo->PaintedVertices[Index2].Color : (bHasColorData ? ColorVertexBuffer.VertexColor(Index2) : FColor::White)
						);
				}
			}
		}
	}

	for (const auto& MeshUIInfo : FoliageMeshList)
	{
		UFoliageType* Settings = MeshUIInfo->Settings;
		if (!Settings->IsSelected)
		{
			continue;
		}

		// Quick lookup of potential instance locations, used for overlapping check.
		TArray<FVector> PotentialInstanceLocations;
		FFoliageInstanceHash PotentialInstanceHash(7);	// use 128x128 cell size, as the brush radius is typically small.
		TArray<FPotentialInstance> InstancesToPlace;

		for (TMap<UPrimitiveComponent*, TArray<FFoliagePaintBucketTriangle> >::TIterator ComponentIt(ComponentPotentialTriangles); ComponentIt; ++ComponentIt)
		{
			UPrimitiveComponent* Component = ComponentIt.Key();
			TArray<FFoliagePaintBucketTriangle>& PotentialTriangles = ComponentIt.Value();

			for (int32 TriIdx = 0; TriIdx<PotentialTriangles.Num(); TriIdx++)
			{
				FFoliagePaintBucketTriangle& Triangle = PotentialTriangles[TriIdx];

				// Check if we can reject this triangle based on normal.
				if (!IsWithinSlopeAngle(Triangle.WorldNormal.Z, Settings->GroundSlopeAngle.Min, Settings->GroundSlopeAngle.Max))
				{
					continue;
				}

				// This is the total set of instances disregarding parameters like slope, height or layer.
				float DesiredInstanceCountFloat = Triangle.Area * Settings->Density * UISettings.GetPaintDensity() / (1000.f*1000.f);

				// Allow a single instance with a random chance, if the brush is smaller than the density
				int32 DesiredInstanceCount = DesiredInstanceCountFloat > 1.f ? FMath::RoundToInt(DesiredInstanceCountFloat) : FMath::FRand() < DesiredInstanceCountFloat ? 1 : 0;

				for (int32 Idx = 0; Idx < DesiredInstanceCount; Idx++)
				{
					FVector InstLocation;
					FColor VertexColor;
					Triangle.GetRandomPoint(InstLocation, VertexColor);

					// Check color mask and filters at this location
					if (!CheckVertexColor(Settings, VertexColor) ||
						!CheckLocationForPotentialInstance(World, Settings, InstLocation, Triangle.WorldNormal, PotentialInstanceLocations, PotentialInstanceHash))
					{
						continue;
					}

					new(InstancesToPlace)FPotentialInstance(InstLocation, Triangle.WorldNormal, Component, 1.f);
				}
			}
		}

		// Place instances
		for (FPotentialInstance& PotentialInstance : InstancesToPlace)
		{
			FFoliageInstance Inst;
			if (PotentialInstance.PlaceInstance(World, Settings, Inst))
			{
				SpawnFoliageInstance(World, Settings, &UISettings, Inst, PotentialInstance.HitComponent);
			}
		}

		RebuildFoliageTree(Settings);

		//
		OnInstanceCountUpdated(Settings);
	}
}

bool FEdModeFoliage::GetStaticMeshVertexColorForHit(const UStaticMeshComponent* InStaticMeshComponent, int32 InTriangleIndex, const FVector& InHitLocation, FColor& OutVertexColor)
{
	const UStaticMesh* StaticMesh = InStaticMeshComponent->GetStaticMesh();

	if (StaticMesh == nullptr || StaticMesh->RenderData == nullptr)
	{
		return false;
	}

	const FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];
	bool bHasInstancedColorData = false;
	const FStaticMeshComponentLODInfo* InstanceMeshLODInfo = nullptr;
	if (InStaticMeshComponent->LODData.Num() > 0)
	{
		InstanceMeshLODInfo = InStaticMeshComponent->LODData.GetData();
		bHasInstancedColorData = InstanceMeshLODInfo->PaintedVertices.Num() == LODModel.VertexBuffer.GetNumVertices();
	}

	const FColorVertexBuffer& ColorVertexBuffer = LODModel.ColorVertexBuffer;

	// no vertex color data
	if (!bHasInstancedColorData && ColorVertexBuffer.GetNumVertices() == 0)
	{
		return false;
	}

	// Get the raw triangle data for this static mesh
	FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
	const FPositionVertexBuffer& PositionVertexBuffer = LODModel.PositionVertexBuffer;

	int32 SectionFirstTriIndex = 0;
	for (TArray<FStaticMeshSection>::TConstIterator SectionIt(LODModel.Sections); SectionIt; ++SectionIt)
	{
		const FStaticMeshSection& Section = *SectionIt;

		if (Section.bEnableCollision)
		{
			int32 NextSectionTriIndex = SectionFirstTriIndex + Section.NumTriangles;
			if (InTriangleIndex >= SectionFirstTriIndex && InTriangleIndex < NextSectionTriIndex)
			{

				int32 IndexBufferIdx = (InTriangleIndex - SectionFirstTriIndex) * 3 + Section.FirstIndex;

				// Look up the triangle vertex indices
				int32 Index0 = Indices[IndexBufferIdx];
				int32 Index1 = Indices[IndexBufferIdx + 1];
				int32 Index2 = Indices[IndexBufferIdx + 2];

				// Lookup the triangle world positions and colors.
				FVector WorldVert0 = InStaticMeshComponent->GetComponentTransform().TransformPosition(PositionVertexBuffer.VertexPosition(Index0));
				FVector WorldVert1 = InStaticMeshComponent->GetComponentTransform().TransformPosition(PositionVertexBuffer.VertexPosition(Index1));
				FVector WorldVert2 = InStaticMeshComponent->GetComponentTransform().TransformPosition(PositionVertexBuffer.VertexPosition(Index2));

				FLinearColor Color0;
				FLinearColor Color1;
				FLinearColor Color2;

				if (bHasInstancedColorData)
				{
					Color0 = InstanceMeshLODInfo->PaintedVertices[Index0].Color.ReinterpretAsLinear();
					Color1 = InstanceMeshLODInfo->PaintedVertices[Index1].Color.ReinterpretAsLinear();
					Color2 = InstanceMeshLODInfo->PaintedVertices[Index2].Color.ReinterpretAsLinear();
				}
				else
				{
					Color0 = ColorVertexBuffer.VertexColor(Index0).ReinterpretAsLinear();
					Color1 = ColorVertexBuffer.VertexColor(Index1).ReinterpretAsLinear();
					Color2 = ColorVertexBuffer.VertexColor(Index2).ReinterpretAsLinear();
				}

				// find the barycentric coordiantes of the hit location, so we can interpolate the vertex colors
				FVector BaryCoords = FMath::GetBaryCentric2D(InHitLocation, WorldVert0, WorldVert1, WorldVert2);

				FLinearColor InterpColor = BaryCoords.X * Color0 + BaryCoords.Y * Color1 + BaryCoords.Z * Color2;

				// convert back to FColor.
				OutVertexColor = InterpColor.ToFColor(false);

				return true;
			}

			SectionFirstTriIndex = NextSectionTriIndex;
		}
	}

	return false;
}

void FEdModeFoliage::SnapSelectedInstancesToGround(UWorld* InWorld)
{
	GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_Transaction_SnapToGround", "Snap Foliage To Ground"));
	{
		bool bMovedInstance = false;

		const int32 NumLevels = InWorld->GetNumLevels();
		for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
		{
			ULevel* Level = InWorld->GetLevel(LevelIdx);
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
			if (IFA)
			{
				bool bFoundSelection = false;

				for (auto& MeshPair : IFA->FoliageMeshes)
				{
					FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
					TArray<int32> SelectedIndices = MeshInfo.SelectedIndices.Array();

					if (SelectedIndices.Num() > 0)
					{
						// Mark actor once we found selection
						if (!bFoundSelection)
						{
							IFA->Modify();
							bFoundSelection = true;
						}

						MeshInfo.PreMoveInstances(IFA, SelectedIndices);

						for (int32 InstanceIndex : SelectedIndices)
						{
							bMovedInstance |= SnapInstanceToGround(IFA, MeshPair.Key->AlignMaxAngle, MeshInfo, InstanceIndex);
						}

						MeshInfo.PostMoveInstances(IFA, SelectedIndices);
					}
				}
			}
		}

		if (bMovedInstance)
		{
			UpdateWidgetLocationToInstanceSelection();
		}
	}
	GEditor->EndTransaction();
}

void FEdModeFoliage::HandleOnActorSpawned(AActor* Actor)
{
	if (AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(Actor))
	{
		// If an IFA was created, we want to be notified if any meshes assigned to its foliage types change
		IFA->OnFoliageTypeMeshChanged().AddSP(this, &FEdModeFoliage::HandleOnFoliageTypeMeshChanged);
	}
}

void FEdModeFoliage::HandleOnFoliageTypeMeshChanged(UFoliageType* FoliageType)
{
	if (FoliageType->IsNotAssetOrBlueprint() && FoliageType->GetStaticMesh() == nullptr)
	{
		RemoveFoliageType(&FoliageType, 1);
	}
	else
	{
		StaticCastSharedPtr<FFoliageEdModeToolkit>(Toolkit)->NotifyFoliageTypeMeshChanged(FoliageType);
	}
}

bool FEdModeFoliage::SnapInstanceToGround(AInstancedFoliageActor* InIFA, float AlignMaxAngle, FFoliageMeshInfo& Mesh, int32 InstanceIdx)
{
	FFoliageInstance& Instance = Mesh.Instances[InstanceIdx];
	FVector Start = Instance.Location;
	FVector End = Instance.Location - FVector(0.f, 0.f, FOLIAGE_SNAP_TRACE);

	FHitResult Hit;
	static FName NAME_FoliageSnap = FName("FoliageSnap");
	if (AInstancedFoliageActor::FoliageTrace(InIFA->GetWorld(), Hit, FDesiredFoliageInstance(Start, End), NAME_FoliageSnap))
	{
		UPrimitiveComponent* HitComponent = Hit.Component.Get();

		if (HitComponent->GetComponentLevel() != InIFA->GetLevel())
		{
			// We should not create cross-level references automatically
			return false;
		}

		// We cannot be based on an a blueprint component as these will disappear when the construction script is re-run
		if (HitComponent->IsCreatedByConstructionScript())
		{
			return false;
		}

		// Find BSP brush 
		UModelComponent* ModelComponent = Cast<UModelComponent>(HitComponent);
		if (ModelComponent)
		{
			ABrush* BrushActor = ModelComponent->GetModel()->FindBrush(Hit.Location);
			if (BrushActor)
			{
				HitComponent = BrushActor->GetBrushComponent();
			}
		}

		// Set new base
		auto NewBaseId = InIFA->InstanceBaseCache.AddInstanceBaseId(HitComponent);
		Mesh.RemoveFromBaseHash(InstanceIdx);
		Instance.BaseId = NewBaseId;
		Mesh.AddToBaseHash(InstanceIdx);
		Instance.Location = Hit.Location;
		Instance.ZOffset = 0.f;

		if (Instance.Flags & FOLIAGE_AlignToNormal)
		{
			// Remove previous alignment and align to new normal.
			Instance.Rotation = Instance.PreAlignRotation;
			Instance.AlignToNormal(Hit.Normal, AlignMaxAngle);
		}

		return true;
	}

	return false;
}

TArray<FFoliageMeshUIInfoPtr>& FEdModeFoliage::GetFoliageMeshList()
{
	return FoliageMeshList;
}

void FEdModeFoliage::PopulateFoliageMeshList()
{
	FoliageMeshList.Empty();

	// Collect set of all available foliage types
	UWorld* World = GetWorld();
	ULevel* CurrentLevel = World->GetCurrentLevel();
	const int32 NumLevels = World->GetNumLevels();

	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = World->GetLevel(LevelIdx);
		if (Level && Level->bIsVisible)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
			if (IFA)
			{
				for (auto& MeshPair : IFA->FoliageMeshes)
				{
					if (!CanPaint(MeshPair.Key, CurrentLevel))
					{
						continue;
					}

					int32 ElementIdx = FoliageMeshList.IndexOfByPredicate([&](const FFoliageMeshUIInfoPtr& Item)
					{
						return Item->Settings == MeshPair.Key;
					});

					if (ElementIdx == INDEX_NONE)
					{
						ElementIdx = FoliageMeshList.Add(MakeShareable(new FFoliageMeshUIInfo(MeshPair.Key)));
					}

					FoliageMeshList[ElementIdx]->InstanceCountTotal += MeshPair.Value->GetInstanceCount();
					if (Level == World->GetCurrentLevel())
					{
						FoliageMeshList[ElementIdx]->InstanceCountCurrentLevel += MeshPair.Value->GetInstanceCount();
					}
				}
			}
		}
	}

	if (FoliageMeshListSortMode != EColumnSortMode::None)
	{
		EColumnSortMode::Type SortMode = FoliageMeshListSortMode;
		auto CompareFoliageType = [SortMode](const FFoliageMeshUIInfoPtr& A, const FFoliageMeshUIInfoPtr& B)
		{
			bool CompareResult = (A->GetNameText().CompareToCaseIgnored(B->GetNameText()) <= 0);
			return SortMode == EColumnSortMode::Ascending ? CompareResult : !CompareResult;
		};

		FoliageMeshList.Sort(CompareFoliageType);
	}

	StaticCastSharedPtr<FFoliageEdModeToolkit>(Toolkit)->RefreshFullList();
}

void FEdModeFoliage::OnFoliageMeshListSortModeChanged(EColumnSortMode::Type InSortMode)
{
	FoliageMeshListSortMode = InSortMode;
	PopulateFoliageMeshList();
}

EColumnSortMode::Type FEdModeFoliage::GetFoliageMeshListSortMode() const
{
	return FoliageMeshListSortMode;
}

void FEdModeFoliage::OnInstanceCountUpdated(const UFoliageType* FoliageType)
{
	int32 EntryIndex = FoliageMeshList.IndexOfByPredicate([&](const FFoliageMeshUIInfoPtr& UIInfoPtr)
	{
		return UIInfoPtr->Settings == FoliageType;
	});

	if (EntryIndex == INDEX_NONE)
	{
		return;
	}

	int32 InstanceCountTotal = 0;
	int32 InstanceCountCurrentLevel = 0;
	UWorld* World = GetWorld();
	ULevel* CurrentLevel = World->GetCurrentLevel();

	for (FFoliageMeshInfoIterator It(World, FoliageType); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		InstanceCountTotal += MeshInfo->Instances.Num();
		if (It.GetActor()->GetLevel() == CurrentLevel)
		{
			InstanceCountCurrentLevel = MeshInfo->Instances.Num();
		}
	}

	//
	FoliageMeshList[EntryIndex]->InstanceCountTotal = InstanceCountTotal;
	FoliageMeshList[EntryIndex]->InstanceCountCurrentLevel = InstanceCountCurrentLevel;
}

void FEdModeFoliage::CalcTotalInstanceCount(int32& OutInstanceCountTotal, int32& OutInstanceCountCurrentLevel)
{
	OutInstanceCountTotal = 0;
	OutInstanceCountCurrentLevel = 0;
	UWorld* InWorld = GetWorld();
	ULevel* CurrentLevel = InWorld->GetCurrentLevel();

	const int32 NumLevels = InWorld->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = InWorld->GetLevel(LevelIdx);
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
		if (IFA)
		{
			int32 IFAInstanceCount = 0;
			for (const auto& MeshPair : IFA->FoliageMeshes)
			{
				const FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
				IFAInstanceCount += MeshInfo.Instances.Num();
			}

			OutInstanceCountTotal += IFAInstanceCount;
			if (CurrentLevel == Level)
			{
				OutInstanceCountCurrentLevel += IFAInstanceCount;
			}
		}
	}
}

bool FEdModeFoliage::CanPaint(const ULevel* InLevel)
{
	for (const auto& MeshUIPtr : FoliageMeshList)
	{
		if (MeshUIPtr->Settings->IsSelected && CanPaint(MeshUIPtr->Settings, InLevel))
		{
			return true;
		}
	}

	return false;
}

bool FEdModeFoliage::CanPaint(const UFoliageType* FoliageType, const ULevel* InLevel)
{
	if (FoliageType == nullptr)	//if asset has already been deleted we can't paint
	{
		return false;
	}

	// Non-shared objects can be painted only into their own level
	// Assets can be painted everywhere
	if (FoliageType->IsAsset() || FoliageType->GetOutermost() == InLevel->GetOutermost())
	{
		return true;
	}

	return false;
}

bool FEdModeFoliage::IsModifierButtonPressed(const FEditorViewportClient* ViewportClient) const
{
	const UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>(FoliageInteractor);
	bool bIsModifierPressed = false;
	if (VRInteractor != nullptr)
	{
		bIsModifierPressed = VRInteractor->IsModifierPressed();
	}

	return IsShiftDown(ViewportClient->Viewport) || bIsModifierPressed;
}

bool FEdModeFoliage::CanMoveSelectedFoliageToLevel(ULevel* InTargetLevel) const
{
	UWorld* World = InTargetLevel->OwningWorld;
	const int32 NumLevels = World->GetNumLevels();

	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = World->GetLevel(LevelIdx);
		if (Level != InTargetLevel)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level, /*bCreateIfNone*/ false);

			if (IFA && IFA->HasSelectedInstances())
			{
				return true;
			}
		}
	}

	return false;
}

void FEdModeFoliage::MoveSelectedFoliageToLevel(ULevel* InTargetLevel)
{
	// Can't move into a locked level
	if (FLevelUtils::IsLevelLocked(InTargetLevel))
	{
		FNotificationInfo NotificatioInfo(NSLOCTEXT("UnrealEd", "CannotMoveFoliageIntoLockedLevel", "Cannot move the selected foliage into a locked level"));
		NotificatioInfo.bUseThrobber = false;
		FSlateNotificationManager::Get().AddNotification(NotificatioInfo)->SetCompletionState(SNotificationItem::CS_Fail);
		return;
	}

	// Get a world context
	UWorld* World = InTargetLevel->OwningWorld;
	bool PromptToMoveFoliageTypeToAsset = World->StreamingLevels.Num() > 0;
	bool ShouldPopulateMeshList = false;

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MoveSelectedFoliageToSelectedLevel", "Move Selected Foliage to Level"));
	
	// Iterate over all foliage actors in the world and move selected instances to a foliage actor in the target level
	const int32 NumLevels = World->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = World->GetLevel(LevelIdx);
		if (Level != InTargetLevel)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level, /*bCreateIfNone*/ false);

			if (IFA && IFA->HasSelectedInstances())
			{
				bool CanMoveInstanceType = true;

				// Make sure all our foliage type used by our selected instances are asset otherwise promote them to assets
				TMap<UFoliageType*, FFoliageMeshInfo*> SelectedInstanceFoliageTypes = IFA->GetSelectedInstancesFoliageType();

				for (auto& MeshPair : SelectedInstanceFoliageTypes)
				{
					if (!MeshPair.Key->IsAsset())
					{
						// Keep previous selection
						TSet<int32> PreviousSelectionSet = MeshPair.Value->SelectedIndices;
						TArray<int32> PreviousSelectionArray;
						PreviousSelectionArray.Reserve(PreviousSelectionSet.Num());

						for (int32& Value : PreviousSelectionSet)
						{
							PreviousSelectionArray.Add(Value);
						}

						UFoliageType* NewFoliageType = SaveFoliageTypeObject(MeshPair.Key);
						CanMoveInstanceType = NewFoliageType != nullptr;

						if (NewFoliageType != nullptr)
						{
							// Restore previous selection for move operation
							FFoliageMeshInfo* MeshInfo = IFA->FindMesh(NewFoliageType);
							MeshInfo->SelectInstances(IFA, true, PreviousSelectionArray);
						}
					}
				}
			
				// Update our actor if we saved some foliage type as asset
				if (CanMoveInstanceType)
				{
					IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level, /*bCreateIfNone*/ false);
					ensure(IFA != nullptr && IFA->HasSelectedInstances());

					IFA->MoveSelectedInstancesToLevel(InTargetLevel);
					ShouldPopulateMeshList = true;
				}
			}
		}
	}

	// Update foliage usages
	if (ShouldPopulateMeshList)
	{
		PopulateFoliageMeshList();
	}
}

UFoliageType* FEdModeFoliage::AddFoliageAsset(UObject* InAsset)
{
	UFoliageType* FoliageType = nullptr;

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(InAsset);
	if (StaticMesh)
	{
		UWorld* World = GetWorld();

		{
			const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "FoliageMode_AddTypeTransaction", "Add Foliage Type"));

			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
			FoliageType = IFA->GetLocalFoliageTypeForMesh(StaticMesh);
			if (!FoliageType)
			{
				IFA->AddMesh(StaticMesh, &FoliageType);
			}
		}

		// If there is multiple levels for this world, save the foliage directly as an asset, so user will be able to paint over all levels by default
		if (World->StreamingLevels.Num() > 0)
		{
			UFoliageType* TypeSaved = SaveFoliageTypeObject(FoliageType);

			if (TypeSaved != nullptr)
			{
				FoliageType = TypeSaved;
			}
		}
	}
	else
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "FoliageMode_AddTypeTransaction", "Add Foliage Type"));

		FoliageType = Cast<UFoliageType>(InAsset);
		if (FoliageType)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld(), true);
			FoliageType = IFA->AddFoliageType(FoliageType);
		}
	}

	if (FoliageType)
	{
		PopulateFoliageMeshList();
	}

	return FoliageType;
}

/** Remove a mesh */
bool FEdModeFoliage::RemoveFoliageType(UFoliageType** FoliageTypeList, int32 Num)
{
	TArray<AInstancedFoliageActor*> IFAList;
	// Find all foliage actors that have any of these types
	UWorld* World = GetWorld();
	for (int32 FoliageTypeIdx = 0; FoliageTypeIdx < Num; ++FoliageTypeIdx)
	{
		UFoliageType* FoliageType = FoliageTypeList[FoliageTypeIdx];
		for (FFoliageMeshInfoIterator It(World, FoliageType); It; ++It)
		{
			IFAList.Add(It.GetActor());
		}
	}

	if (IFAList.Num())
	{
		GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_RemoveMeshTransaction", "Foliage Editing: Remove Mesh"));
		for (AInstancedFoliageActor* IFA : IFAList)
		{
			IFA->RemoveFoliageType(FoliageTypeList, Num);
		}
		GEditor->EndTransaction();

		PopulateFoliageMeshList();
		return true;
	}

	return false;
}

/** Bake instances to StaticMeshActors */
void FEdModeFoliage::BakeFoliage(UFoliageType* Settings, bool bSelectedOnly)
{
	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	if (IFA == nullptr)
	{
		return;
	}

	FFoliageMeshInfo* MeshInfo = IFA->FindMesh(Settings);
	if (MeshInfo != nullptr)
	{
		TArray<int32> InstancesToConvert;
		if (bSelectedOnly)
		{
			InstancesToConvert = MeshInfo->SelectedIndices.Array();
		}
		else
		{
			for (int32 InstanceIdx = 0; InstanceIdx < MeshInfo->Instances.Num(); InstanceIdx++)
			{
				InstancesToConvert.Add(InstanceIdx);
			}
		}

		// Convert
		for (int32 Idx = 0; Idx < InstancesToConvert.Num(); Idx++)
		{
			FFoliageInstance& Instance = MeshInfo->Instances[InstancesToConvert[Idx]];
			// We need a world in which to spawn the actor. Use the one from the original instance.
			UWorld* World = IFA->GetWorld();
			check(World != nullptr);
			AStaticMeshActor* SMA = World->SpawnActor<AStaticMeshActor>(Instance.Location, Instance.Rotation);
			SMA->GetStaticMeshComponent()->SetStaticMesh(Settings->GetStaticMesh());
			SMA->GetRootComponent()->SetRelativeScale3D(Instance.DrawScale3D);
			SMA->MarkComponentsRenderStateDirty();
		}

		// Remove
		MeshInfo->RemoveInstances(IFA, InstancesToConvert, true);
	}
}

/** Copy the settings object for this static mesh */
UFoliageType* FEdModeFoliage::CopySettingsObject(UFoliageType* Settings)
{
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "FoliageMode_DuplicateSettingsObject", "Foliage Editing: Copy Settings Object"));

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(GetWorld());
	IFA->Modify();

	TUniqueObj<FFoliageMeshInfo> MeshInfo;
	if (IFA->FoliageMeshes.RemoveAndCopyValue(Settings, MeshInfo))
	{
		Settings = (UFoliageType*)StaticDuplicateObject(Settings, IFA, NAME_None, RF_AllFlags & ~(RF_Standalone | RF_Public));
		IFA->FoliageMeshes.Add(Settings, MoveTemp(MeshInfo));
		return Settings;
	}
	else
	{
		Transaction.Cancel();
	}

	return nullptr;
}

/** Replace the settings object for this static mesh with the one specified */
void FEdModeFoliage::ReplaceSettingsObject(UFoliageType* OldSettings, UFoliageType* NewSettings)
{
	FFoliageEditUtility::ReplaceFoliageTypeObject(GetWorld(), OldSettings, NewSettings);

	PopulateFoliageMeshList();
}

UFoliageType* FEdModeFoliage::SaveFoliageTypeObject(UFoliageType* InFoliageType)
{
	UFoliageType* TypeToSave = FFoliageEditUtility::SaveFoliageTypeObject(InFoliageType);

	if (TypeToSave != nullptr && TypeToSave != InFoliageType)
	{
		ReplaceSettingsObject(InFoliageType, TypeToSave);
	}

	return TypeToSave;
}

/** Reapply cluster settings to all the instances */
void FEdModeFoliage::ReallocateClusters(UFoliageType* Settings)
{
	UWorld* World = GetWorld();
	for (FFoliageMeshInfoIterator It(World, Settings); It; ++It)
	{
		FFoliageMeshInfo* MeshInfo = (*It);
		MeshInfo->ReallocateClusters(It.GetActor(), Settings);
	}
}

/** FEdMode: Called when a key is pressed */
bool FEdModeFoliage::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Event != IE_Released)
	{
		if (UICommandList->ProcessCommandBindings(Key, FSlateApplication::Get().GetModifierKeys(), false/*Event == IE_Repeat*/))
		{
			return true;
		}
	}

	bool bHandled = false;
	if ((UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected()) && FoliageInteractor == nullptr)
	{
		// Require Ctrl or not as per user preference
		ELandscapeFoliageEditorControlType FoliageEditorControlType = GetDefault<ULevelEditorViewportSettings>()->FoliageEditorControlType;

		if (Key == EKeys::LeftMouseButton && Event == IE_Pressed)
		{
			// Only activate tool if we're not already moving the camera and we're not trying to drag a transform widget
			// Not using "if (!ViewportClient->IsMovingCamera())" because it's wrong in ortho viewports :D
			bool bMovingCamera = Viewport->KeyState(EKeys::MiddleMouseButton) || Viewport->KeyState(EKeys::RightMouseButton) || IsAltDown(Viewport);

			if ((Viewport->IsPenActive() && Viewport->GetTabletPressure() > 0.f) ||
				(!bMovingCamera && ViewportClient->GetCurrentWidgetAxis() == EAxisList::None &&
					(FoliageEditorControlType == ELandscapeFoliageEditorControlType::IgnoreCtrl ||
						(FoliageEditorControlType == ELandscapeFoliageEditorControlType::RequireCtrl   && IsCtrlDown(Viewport)) ||
						(FoliageEditorControlType == ELandscapeFoliageEditorControlType::RequireNoCtrl && !IsCtrlDown(Viewport)))))
			{
				if (!bToolActive)
				{
					StartFoliageBrushTrace(ViewportClient);

					bHandled = true;
				}
			}
		}
		else if (bToolActive && Event == IE_Released &&
			(Key == EKeys::LeftMouseButton || (FoliageEditorControlType == ELandscapeFoliageEditorControlType::RequireCtrl && (Key == EKeys::LeftControl || Key == EKeys::RightControl))))
		{
			//Set the cursor position to that of the slate cursor so it wont snap back
			Viewport->SetPreCaptureMousePosFromSlateCursor();
			EndFoliageBrushTrace();

			bHandled = true;
		}
		else if (IsCtrlDown(Viewport))
		{
			// Control + scroll adjusts the brush radius
			static const float RadiusAdjustmentAmount = 25.f;
			if (Key == EKeys::MouseScrollUp)
			{
				AdjustBrushRadius(RadiusAdjustmentAmount);

				bHandled = true;
			}
			else if (Key == EKeys::MouseScrollDown)
			{
				AdjustBrushRadius(-RadiusAdjustmentAmount);

				bHandled = true;
			}
		}
		else if (Key == EKeys::I && Event == IE_Released)
		{
			UISettings.SetIsInQuickSingleInstantiationMode(false);
		}
		else if (Key == EKeys::I && Event == IE_Pressed)
		{
			UISettings.SetIsInQuickSingleInstantiationMode(true);
		}
	}

	if (!bHandled && (UISettings.GetLassoSelectToolSelected() || UISettings.GetSelectToolSelected()) && FoliageInteractor == nullptr)
	{
		if (Event == IE_Pressed)
		{
			if (Key == EKeys::Platform_Delete)
			{
				RemoveSelectedInstances(GetWorld());

				bHandled = true;
			}
			else if (Key == EKeys::End)
			{
				SnapSelectedInstancesToGround(GetWorld());

				bHandled = true;
			}
		}
	}

	return bHandled;
}

/** FEdMode: Render the foliage edit mode */
void FEdModeFoliage::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	/** Call parent implementation */
	FEdMode::Render(View, Viewport, PDI);
}


/** FEdMode: Render HUD elements for this tool */
void FEdModeFoliage::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
}

/** FEdMode: Check to see if an actor can be selected in this mode - no side effects */
bool FEdModeFoliage::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	return false;
}

/** FEdMode: Handling SelectActor */
bool FEdModeFoliage::Select(AActor* InActor, bool bInSelected)
{
	// return true if you filter that selection
	// however - return false if we are trying to deselect so that it will infact do the deselection
	if (bInSelected == false)
	{
		return false;
	}
	return true;
}

/** FEdMode: Called when the currently selected actor has changed */
void FEdModeFoliage::ActorSelectionChangeNotify()
{
}


/** Forces real-time perspective viewports */
void FEdModeFoliage::ForceRealTimeViewports(const bool bEnable, const bool bStoreCurrentState)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr< ILevelViewport > ViewportWindow = LevelEditorModule.GetFirstActiveViewport();
	if (ViewportWindow.IsValid())
	{
		FEditorViewportClient &Viewport = ViewportWindow->GetLevelViewportClient();
		if (Viewport.IsPerspective())
		{
			if (bEnable)
			{
				Viewport.SetRealtime(bEnable, bStoreCurrentState);
			}
			else
			{
				const bool bAllowDisable = true;
				Viewport.RestoreRealtime(bAllowDisable);
			}

		}
	}
}

bool FEdModeFoliage::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	if (UISettings.GetSelectToolSelected())
	{
		if (HitProxy && HitProxy->IsA(HInstancedStaticMeshInstance::StaticGetType()))
		{
			HInstancedStaticMeshInstance* SMIProxy = ((HInstancedStaticMeshInstance*)HitProxy);
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(SMIProxy->Component->GetComponentLevel());
			if (IFA)
			{
				IFA->SelectInstance(SMIProxy->Component, SMIProxy->InstanceIndex, Click.IsControlDown());
				// Update pivot
				UpdateWidgetLocationToInstanceSelection();
			}
		}
		else
		{
			if (!Click.IsControlDown())
			{
				// Select none if not trying to toggle
				SelectInstances(GetWorld(), false);
			}
		}

		return true;
	}
	else if (UISettings.GetPaintBucketToolSelected() || UISettings.GetReapplyPaintBucketToolSelected())
	{
		if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
		{
			GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));
			
			if (IsModifierButtonPressed(InViewportClient))
			{
				ApplyPaintBucket_Remove(((HActor*)HitProxy)->Actor);
			}
			else
			{
				ApplyPaintBucket_Add(((HActor*)HitProxy)->Actor);
			}

			GEditor->EndTransaction();
		}

		return true;
	}


	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}

FVector FEdModeFoliage::GetWidgetLocation() const
{
	return FEdMode::GetWidgetLocation();
}

/** FEdMode: Called when a mouse button is pressed */
bool FEdModeFoliage::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (IsCtrlDown(InViewport) && InViewport->KeyState(EKeys::MiddleMouseButton) && (UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected()))
	{
		bAdjustBrushRadius = true;
		return true;
	}
	else if (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected())
	{
		// Update pivot
		UpdateWidgetLocationToInstanceSelection();

		GEditor->BeginTransaction(NSLOCTEXT("UnrealEd", "FoliageMode_EditTransaction", "Foliage Editing"));

		bCanAltDrag = true;

		return true;
	}
	return FEdMode::StartTracking(InViewportClient, InViewport);
}

/** FEdMode: Called when the a mouse button is released */
bool FEdModeFoliage::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (!bAdjustBrushRadius && (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected()))
	{
		GEditor->EndTransaction();
		return true;
	}
	else
	{
		bAdjustBrushRadius = false;
		return true;
	}

	return FEdMode::EndTracking(InViewportClient, InViewport);
}

/** FEdMode: Called when mouse drag input it applied */
bool FEdModeFoliage::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (bAdjustBrushRadius)
	{
		if (UISettings.GetPaintToolSelected() || UISettings.GetReapplyToolSelected() || UISettings.GetLassoSelectToolSelected())
		{
			static const float RadiusAdjustmentFactor = 10.f;
			AdjustBrushRadius(RadiusAdjustmentFactor * InDrag.Y);
		}
	}
	else
	{
		if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None && (UISettings.GetSelectToolSelected() || UISettings.GetLassoSelectToolSelected()))
		{
			const bool bDuplicateInstances = (bCanAltDrag && IsAltDown(InViewport) && (InViewportClient->GetCurrentWidgetAxis() & EAxisList::XYZ));

			TransformSelectedInstances(GetWorld(), InDrag, InRot, InScale, bDuplicateInstances);

			// Only allow alt-drag on first InputDelta
			bCanAltDrag = false;
		}
	}

	return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
}

bool FEdModeFoliage::AllowWidgetMove()
{
	return ShouldDrawWidget();
}

bool FEdModeFoliage::UsesTransformWidget() const
{
	return ShouldDrawWidget();
}

bool FEdModeFoliage::ShouldDrawWidget() const
{
	if (UISettings.GetSelectToolSelected() || (UISettings.GetLassoSelectToolSelected() && !bToolActive))
	{
		FVector Location;
		return GetSelectionLocation(GetWorld(), Location) != nullptr;
	}
	return false;
}

EAxisList::Type FEdModeFoliage::GetWidgetAxisToDraw(FWidget::EWidgetMode InWidgetMode) const
{
	switch (InWidgetMode)
	{
	case FWidget::WM_Translate:
	case FWidget::WM_Rotate:
	case FWidget::WM_Scale:
		return EAxisList::XYZ;
	default:
		return EAxisList::None;
	}
}

/** Load UI settings from ini file */
void FFoliageUISettings::Load()
{
	FString WindowPositionString;
	if (GConfig->GetString(TEXT("FoliageEdit"), TEXT("WindowPosition"), WindowPositionString, GEditorPerProjectIni))
	{
		TArray<FString> PositionValues;
		if (WindowPositionString.ParseIntoArray(PositionValues, TEXT(","), true) == 4)
		{
			WindowX = FCString::Atoi(*PositionValues[0]);
			WindowY = FCString::Atoi(*PositionValues[1]);
			WindowWidth = FCString::Atoi(*PositionValues[2]);
			WindowHeight = FCString::Atoi(*PositionValues[3]);
		}
	}

	GConfig->GetFloat(TEXT("FoliageEdit"), TEXT("Radius"), Radius, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("FoliageEdit"), TEXT("PaintDensity"), PaintDensity, GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("FoliageEdit"), TEXT("UnpaintDensity"), UnpaintDensity, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterLandscape"), bFilterLandscape, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterStaticMesh"), bFilterStaticMesh, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterBSP"), bFilterBSP, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterFoliage"), bFilterFoliage, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bFilterTranslucent"), bFilterTranslucent, GEditorPerProjectIni);

	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bShowPaletteItemDetails"), bShowPaletteItemDetails, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("FoliageEdit"), TEXT("bShowPaletteItemTooltips"), bShowPaletteItemTooltips, GEditorPerProjectIni);

	int32 ActivePaletteViewModeAsInt = 0;
	GConfig->GetInt(TEXT("FoliageEdit"), TEXT("ActivePaletteViewMode"), ActivePaletteViewModeAsInt, GEditorPerProjectIni);
	ActivePaletteViewMode = EFoliagePaletteViewMode::Type(ActivePaletteViewModeAsInt);

	GConfig->GetFloat(TEXT("FoliageEdit"), TEXT("PaletteThumbnailScale"), PaletteThumbnailScale, GEditorPerProjectIni);
}

/** Save UI settings to ini file */
void FFoliageUISettings::Save()
{
	FString WindowPositionString = FString::Printf(TEXT("%d,%d,%d,%d"), WindowX, WindowY, WindowWidth, WindowHeight);
	GConfig->SetString(TEXT("FoliageEdit"), TEXT("WindowPosition"), *WindowPositionString, GEditorPerProjectIni);

	GConfig->SetFloat(TEXT("FoliageEdit"), TEXT("Radius"), Radius, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("FoliageEdit"), TEXT("PaintDensity"), PaintDensity, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("FoliageEdit"), TEXT("UnpaintDensity"), UnpaintDensity, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterLandscape"), bFilterLandscape, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterStaticMesh"), bFilterStaticMesh, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterBSP"), bFilterBSP, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterFoliage"), bFilterFoliage, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bFilterTranslucent"), bFilterTranslucent, GEditorPerProjectIni);

	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bShowPaletteItemDetails"), bShowPaletteItemDetails, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("FoliageEdit"), TEXT("bShowPaletteItemTooltips"), bShowPaletteItemTooltips, GEditorPerProjectIni);
	GConfig->SetInt(TEXT("FoliageEdit"), TEXT("ActivePaletteViewMode"), ActivePaletteViewMode, GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("FoliageEdit"), TEXT("PaletteThumbnailScale"), PaletteThumbnailScale, GEditorPerProjectIni);
}

#undef LOCTEXT_NAMESPACE