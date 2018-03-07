// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaintModePainter.h"

#include "Engine/Selection.h"

#include "Components/StaticMeshComponent.h"
#include "ComponentReregisterContext.h"
#include "StaticMeshResources.h"
#include "Components/SkeletalMeshComponent.h"


#include "MeshPaintModule.h"
#include "MeshPaintEdMode.h"

#include "MeshPaintHelpers.h"
#include "ScopedTransaction.h"

#include "Misc/FeedbackContext.h"
#include "MeshPaintSettings.h"
#include "Dialogs/Dialogs.h"

#include "Engine/TextureRenderTarget2D.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"
#include "MeshPaintTypes.h"
#include "TexturePaintHelpers.h"

#include "VREditorMode.h"
#include "IVREditorModule.h"
#include "ViewportWorldInteraction.h"
#include "ViewportInteractableInterface.h"
#include "VREditorInteractor.h"
#include "EditorWorldExtension.h"

#include "Commands/UICommandList.h"
#include "PaintModeCommands.h"
#include "PaintModeSettings.h"
#include "MeshPaintAdapterFactory.h"
#include "IMeshPaintGeometryAdapter.h"

#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "PaintModePainter"

FPaintModePainter::FPaintModePainter()
	: BrushRenderTargetTexture(nullptr),
	BrushMaskRenderTargetTexture(nullptr),
	SeamMaskRenderTargetTexture(nullptr),
	TexturePaintingCurrentMeshComponent(nullptr),
	PaintingTexture2D(nullptr),
	bDoRestoreRenTargets(false),
	bRefreshCachedData(true),
	bSelectionContainsPerLODColors(false)
{}

FPaintModePainter::~FPaintModePainter()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	ComponentToAdapterMap.Empty();
	ComponentToTexturePaintSettingsMap.Empty();
}

FPaintModePainter* FPaintModePainter::Get()
{
	static FPaintModePainter* Painter = nullptr;
	static bool bInit = false;
	if (!bInit)
	{
		bInit = true;
		Painter = new FPaintModePainter();
		Painter->Init();
	}
	return Painter;
}

void FPaintModePainter::Init()
{
	/** Setup necessary data */
	BrushSettings = DuplicateObject<UPaintBrushSettings>(GetMutableDefault<UPaintBrushSettings>(), GetTransientPackage());
	BrushSettings->AddToRoot();
	PaintSettings = UPaintModeSettings::Get();
	FPaintModeCommands::Register();
	UICommandList = TSharedPtr<FUICommandList>(new FUICommandList());
	RegisterVertexPaintCommands();
	RegisterTexturePaintCommands();
	Widget = SNew(SPaintModeWidget, this);
	CachedLODIndex = PaintSettings->VertexPaintSettings.LODIndex;
	bCachedForceLOD = PaintSettings->VertexPaintSettings.bPaintOnSpecificLOD;
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FPaintModePainter::UpdatePaintTargets);
}

void FPaintModePainter::RegisterTexturePaintCommands()
{
	UICommandList->MapAction(FPaintModeCommands::Get().PropagateTexturePaint, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::CommitAllPaintedTextures), FCanExecuteAction::CreateLambda([this]() -> bool {
		return GetNumberOfPendingPaintChanges() > 0; })));

	UICommandList->MapAction(FPaintModeCommands::Get().SaveTexturePaint, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::SaveModifiedTextures), FCanExecuteAction::CreateRaw(this, &FPaintModePainter::CanSaveModifiedTextures)));
}

void FPaintModePainter::RegisterVertexPaintCommands()
{
	UICommandList->MapAction(FPaintModeCommands::Get().Fill, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::FillWithVertexColor),
		FCanExecuteAction::CreateRaw(this, &FPaintModePainter::SelectionContainsValidAdapters)));

	UICommandList->MapAction(FPaintModeCommands::Get().Propagate, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::PropagateVertexColorsToAsset), FCanExecuteAction::CreateRaw(this, &FPaintModePainter::CanPropagateVertexColors)));

	auto IsAValidMeshComponentSelected = [this]() -> bool { return (GetSelectedComponents<UMeshComponent>().Num() == 1) && SelectionContainsValidAdapters(); };
	UICommandList->MapAction(FPaintModeCommands::Get().Import, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::ImportVertexColors), FCanExecuteAction::CreateLambda(IsAValidMeshComponentSelected)));

	UICommandList->MapAction(FPaintModeCommands::Get().Save, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::SavePaintedAssets), FCanExecuteAction::CreateRaw(this, &FPaintModePainter::CanSaveMeshPackages)));

	UICommandList->MapAction(FPaintModeCommands::Get().Copy, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::CopyVertexColors), FCanExecuteAction::CreateRaw(this, &FPaintModePainter::CanCopyInstanceVertexColors)));

	UICommandList->MapAction(FPaintModeCommands::Get().Paste, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::PasteVertexColors), FCanExecuteAction::CreateRaw(this, &FPaintModePainter::CanPasteInstanceVertexColors)));

	UICommandList->MapAction(FPaintModeCommands::Get().Remove, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::RemoveVertexColors), FCanExecuteAction::CreateRaw(this, &FPaintModePainter::CanRemoveInstanceColors)));

	UICommandList->MapAction(FPaintModeCommands::Get().Fix, FUIAction(FExecuteAction::CreateRaw(this, &FPaintModePainter::FixVertexColors), FCanExecuteAction::CreateRaw(this, &FPaintModePainter::DoesRequireVertexColorsFixup)));
}

void FPaintModePainter::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	/** Render viewport interactors */
	RenderInteractors(View, Viewport, PDI, PaintSettings->PaintMode == EPaintMode::Vertices);
}

UPaintBrushSettings* FPaintModePainter::GetBrushSettings()
{
	return BrushSettings;
}

UMeshPaintSettings* FPaintModePainter::GetPainterSettings()
{
	return PaintSettings;
}

TSharedPtr<class SWidget> FPaintModePainter::GetWidget()
{	
	return Widget;
}

TSharedPtr<FUICommandList> FPaintModePainter::GetUICommandList()
{
	return UICommandList;
}

bool FPaintModePainter::DoesRequireVertexColorsFixup() const
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	bool bAnyMeshNeedsFixing = false;
	/** Check if there are any static mesh components which require fixing */
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		bAnyMeshNeedsFixing |= Component->RequiresOverrideVertexColorsFixup();
	}

	return bAnyMeshNeedsFixing;
}

bool FPaintModePainter::CanRemoveInstanceColors() const
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	const int32 PaintingMeshLODIndex = PaintSettings->VertexPaintSettings.bPaintOnSpecificLOD ? PaintSettings->VertexPaintSettings.LODIndex : 0;
	int32 NumValidMeshes = 0;
	// Retrieve per instance vertex color information (only valid if the component contains actual instance vertex colors)
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		if (Component != nullptr && Component->GetStaticMesh() != nullptr && Component->GetStaticMesh()->GetNumLODs() > (int32)PaintingMeshLODIndex)
		{
			uint32 BufferSize = MeshPaintHelpers::GetVertexColorBufferSize(Component, PaintingMeshLODIndex, true);

			if (BufferSize > 0)
			{
				++NumValidMeshes;
			}
		}
	}

	return (NumValidMeshes != 0);
}

bool FPaintModePainter::CanPasteInstanceVertexColors() const
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	bool bValidForPasting = false;
	/** Make sure we have copied vertex color data which matches at least mesh component in the current selection */
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		checkf(Component != nullptr, TEXT("Invalid Static Mesh Component"));
		UStaticMesh* Mesh = Component->GetStaticMesh();
		if (Mesh && Mesh->GetNumLODs() > 0)
		{
			// See if there is a valid instance of copied vertex colors for this mesh
			const int32 BlueprintCreatedComponentIndex = Component->GetBlueprintCreatedComponentIndex();
			const FPerComponentVertexColorData* PasteColors = CopiedColorsByComponent.FindByPredicate([=](const FPerComponentVertexColorData& ComponentData)
			{
				return (ComponentData.OriginalMesh.Get() == Mesh && ComponentData.ComponentIndex == BlueprintCreatedComponentIndex);
			});

			if (PasteColors)
			{
				bValidForPasting = true;
				break;
			}
		}
	}

	return bValidForPasting;
}

bool FPaintModePainter::CanCopyInstanceVertexColors() const
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	const int32 PaintingMeshLODIndex = PaintSettings->VertexPaintSettings.bPaintOnSpecificLOD ? PaintSettings->VertexPaintSettings.LODIndex : 0;

	// Ensure that the selection does not contain two components which point to identical meshes
	TArray<const UStaticMesh*> ContainedMeshes;

	bool bValidSelection = true;
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		checkf(Component != nullptr, TEXT("Invalid Static Mesh Component"));
		if (Component->GetStaticMesh() != nullptr)
		{
			const UStaticMesh* StaticMesh = Component->GetStaticMesh();
			if (!ContainedMeshes.Contains(StaticMesh))
			{
				ContainedMeshes.Add(StaticMesh);
			}
			else
			{
				bValidSelection = false;
				break;
			}
		}
	}

	int32 NumValidMeshes = 0;
	// Retrieve per instance vertex color information (only valid if the component contains actual instance vertex colors)
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		checkf(Component != nullptr, TEXT("Invalid Static Mesh Component"));
		if (Component->GetStaticMesh() != nullptr && Component->GetStaticMesh()->GetNumLODs() > (int32)PaintingMeshLODIndex)
		{
			uint32 BufferSize = MeshPaintHelpers::GetVertexColorBufferSize(Component, PaintingMeshLODIndex, true);

			if (BufferSize > 0)
			{
				++NumValidMeshes;
			}
		}
	}

	return bValidSelection && (NumValidMeshes != 0);
}

void FPaintModePainter::CopyVertexColors()
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		/** Make sure we have valid data to copy from */
		checkf(Component != nullptr, TEXT("Invalid Static Mesh Component"));
		const UStaticMesh* StaticMesh = Component->GetStaticMesh();
		ensure(StaticMesh != nullptr);
		if (StaticMesh)
		{
			// Create copy structure instance for this mesh 
			FPerComponentVertexColorData ComponentData(StaticMesh, Component->GetBlueprintCreatedComponentIndex());
			const int32 NumLODs = StaticMesh->GetNumLODs();
			ComponentData.PerLODVertexColorData.AddDefaulted(NumLODs);

			// Retrieve and store vertex colors for each LOD in the mesh 
			for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
			{
				FPerLODVertexColorData& LODData = ComponentData.PerLODVertexColorData[LODIndex];

				TArray<FColor> ColorData;
				TArray<FVector> VertexData;

				if (Component->LODData.IsValidIndex(LODIndex) && (Component->LODData[LODIndex].OverrideVertexColors != nullptr))
				{
					ColorData = MeshPaintHelpers::GetInstanceColorDataForLOD(Component, LODIndex);
				}
				else
				{
					ColorData = MeshPaintHelpers::GetColorDataForLOD(StaticMesh, LODIndex);
				}
				VertexData = MeshPaintHelpers::GetVerticesForLOD(StaticMesh, LODIndex);

				const bool bValidColorData = VertexData.Num() == ColorData.Num();
				for (int32 VertexIndex = 0; VertexIndex < VertexData.Num(); ++VertexIndex)
				{
					const FColor& Color = bValidColorData ? ColorData[VertexIndex] : FColor::White;
					LODData.ColorsByIndex.Add(Color);
					LODData.ColorsByPosition.Add(VertexData[VertexIndex], Color);
				}
			}

			CopiedColorsByComponent.Add(ComponentData);
		}
	}
}

void FPaintModePainter::PasteVertexColors()
{
	FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionPasteInstColors", "Pasting Per-Instance Vertex Colors"));
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		TUniquePtr< FComponentReregisterContext > ComponentReregisterContext;
		checkf(Component != nullptr, TEXT("Invalid Static Mesh Component"));
		UStaticMesh* Mesh = Component->GetStaticMesh();
		if (Mesh && Mesh->GetNumLODs() > 0)
		{
			// See if there is a valid instance of copied vertex colors for this mesh
			const int32 BlueprintCreatedComponentIndex = Component->GetBlueprintCreatedComponentIndex();
			FPerComponentVertexColorData* PasteColors = CopiedColorsByComponent.FindByPredicate([=](const FPerComponentVertexColorData& ComponentData)
			{
				return (ComponentData.OriginalMesh.Get() == Mesh && ComponentData.ComponentIndex == BlueprintCreatedComponentIndex);
			});

			if (PasteColors)
			{
				ComponentReregisterContext = MakeUnique<FComponentReregisterContext>(Component);

				const int32 NumLods = Mesh->GetNumLODs();
				Component->SetFlags(RF_Transactional);
				Component->Modify();
				Component->SetLODDataCount(NumLods, NumLods);
				/** Remove all vertex colors before we paste in new ones */
				MeshPaintHelpers::RemoveComponentInstanceVertexColors(Component);

				/** Try and apply copied vertex colors for each LOD in the mesh */
				for (int32 LODIndex = 0; LODIndex < NumLods; ++LODIndex)
				{
					FStaticMeshLODResources& LodRenderData = Mesh->RenderData->LODResources[LODIndex];
					FStaticMeshComponentLODInfo& ComponentLodInfo = Component->LODData[LODIndex];

					const int32 NumLodsInCopyBuffer = PasteColors->PerLODVertexColorData.Num();
					if (LODIndex >= NumLodsInCopyBuffer)
					{
						// no corresponding LOD in color paste buffer CopiedColorsByLOD
						// create array of all white verts
						MeshPaintHelpers::SetInstanceColorDataForLOD(Component, LODIndex, FColor::White);
					}
					else
					{
						FPerLODVertexColorData& LODData = PasteColors->PerLODVertexColorData[LODIndex];
						const int32 NumLODVertices = LodRenderData.GetNumVertices();

						if (NumLODVertices == LODData.ColorsByIndex.Num())
						{
							MeshPaintHelpers::SetInstanceColorDataForLOD(Component, LODIndex, LODData.ColorsByIndex);
						}
						else
						{
							// verts counts mismatch - build translation/fixup list of colors in ReOrderedColors
							TArray<FColor> PositionMatchedColors;
							PositionMatchedColors.Empty(NumLODVertices);

							for (int32 VertexIndex = 0; VertexIndex < NumLODVertices; ++VertexIndex)
							{
								// Search for color matching this vertex position otherwise fill it with white
								const FVector& Vertex = LodRenderData.PositionVertexBuffer.VertexPosition(VertexIndex);
								const FColor* FoundColor = LODData.ColorsByPosition.Find(Vertex);
								PositionMatchedColors.Add(FoundColor ? *FoundColor : FColor::White);
							}

							MeshPaintHelpers::SetInstanceColorDataForLOD(Component, LODIndex, PositionMatchedColors);
						}
					}
				}

				/** Update cached paint data on static mesh component and update DDC key */
				Component->CachePaintedDataIfNecessary();
				Component->StaticMeshDerivedDataKey = Mesh->RenderData->DerivedDataKey;
			}
		}
	}
}

void FPaintModePainter::FixVertexColors()
{
	FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionFixInstColors", "Fixing Per-Instance Vertex Colors"));
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		Component->FixupOverrideColorsIfNecessary();
	}
}

void FPaintModePainter::RemoveVertexColors()
{
	FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionRemoveInstColors", "Removing Per-Instance Vertex Colors"));
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		MeshPaintHelpers::RemoveComponentInstanceVertexColors(Component);
	}
}

bool FPaintModePainter::CanSaveMeshPackages() const
{
	// Check whether or not any of our selected mesh components contain mesh objects which require saving
	TArray<UMeshComponent*> Components = GetSelectedComponents<UMeshComponent>();

	bool bValid = false;

	for (UMeshComponent* Component : Components)
	{
		UObject* Object = nullptr;
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			Object = StaticMeshComponent->GetStaticMesh();
		}
		else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component))
		{
			Object = SkeletalMeshComponent->SkeletalMesh;
		}

		if (Object != nullptr && Object->GetOutermost()->IsDirty())
		{
			bValid = true;
			break;
		}
	}

	return bValid;
}

bool FPaintModePainter::SelectionContainsValidAdapters() const
{
	for (auto& MeshAdapterPair : ComponentToAdapterMap)
	{
		if (MeshAdapterPair.Value->IsValid())
		{
			return true;
		}
	}

	return false;
}

bool FPaintModePainter::CanPropagateVertexColors() const
{
	// Check whether or not our selected Static Mesh Components contain instance based vertex colors (only these can be propagated to the base mesh)
	int32 NumInstanceVertexColorBytes = 0;

	TArray<UStaticMesh*> StaticMeshes;
	TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	bool bValid = StaticMeshComponents.Num() > 0;
	for (const UStaticMeshComponent* Component : StaticMeshComponents)
	{
		UStaticMesh* StaticMesh = Component->GetStaticMesh();
		// Check for components painting to the same static mesh
		const bool bDuplicateSelection = StaticMesh != nullptr && StaticMeshes.Contains(StaticMesh);

		if (bDuplicateSelection)
		{
			bValid = false;
			break;
		}

		if (StaticMesh != nullptr)
		{
			StaticMeshes.AddUnique(StaticMesh);
		}

		MeshPaintHelpers::GetInstanceColorDataInfo(Component, CachedLODIndex, NumInstanceVertexColorBytes);
	}

	return bValid && (NumInstanceVertexColorBytes > 0);
}

bool FPaintModePainter::ShouldFilterTextureAsset(const FAssetData& AssetData) const
{	
	return !(PaintableTextures.ContainsByPredicate([=](const FPaintableTexture& Texture) { return Texture.Texture->GetFullName() == AssetData.GetFullName(); }));
}

void FPaintModePainter::PaintTextureChanged(const FAssetData& AssetData)
{
	UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset());
	if (Texture)
	{
		// Loop through our list of textures and see which one the user wants to select
		for (int32 targetIndex = 0; targetIndex < TexturePaintTargetList.Num(); targetIndex++)
		{
			FTextureTargetListInfo& TextureTarget = TexturePaintTargetList[targetIndex];
			if (TextureTarget.TextureData == Texture)
			{
				TextureTarget.bIsSelected = true;
				PaintSettings->TexturePaintSettings.UVChannel = TextureTarget.UVChannelIndex;
			}
			else
			{
				TextureTarget.bIsSelected = false;
			}
		}
	}
}

void FPaintModePainter::RegisterCommands(TSharedRef<FUICommandList> CommandList)
{
	IMeshPainter::RegisterCommands(CommandList);

	const FPaintModeCommands& Commands = FPaintModeCommands::Get();	
	
	/** Lambda used to cycle through available textures to paint on */
	auto TextureCycleLambda = [this](int32 Direction)
	{
		UTexture2D*& SelectedTexture = PaintSettings->TexturePaintSettings.PaintTexture;

		const int32 TextureIndex = (SelectedTexture != nullptr) ? PaintableTextures.IndexOfByKey(SelectedTexture) : 0;
		if (TextureIndex != INDEX_NONE)
		{
			int32 NewTextureIndex = TextureIndex + Direction;
			if (NewTextureIndex < 0)
			{
				NewTextureIndex += PaintableTextures.Num();
			}
			NewTextureIndex %= PaintableTextures.Num();

			if (PaintableTextures.IsValidIndex(NewTextureIndex))
			{
				SelectedTexture = (UTexture2D*)PaintableTextures[NewTextureIndex].Texture;
			}
		}
	};

	/** Map next and previous texture commands to lambda */
	auto TexturePaintModeLambda = [this]() -> bool { return PaintSettings->PaintMode == EPaintMode::Textures;  };
	CommandList->MapAction(Commands.NextTexture, FExecuteAction::CreateLambda(TextureCycleLambda, 1), FCanExecuteAction::CreateLambda(TexturePaintModeLambda));
	CommandList->MapAction(Commands.PreviousTexture, FExecuteAction::CreateLambda(TextureCycleLambda, -1), FCanExecuteAction::CreateLambda(TexturePaintModeLambda));
	
	/** Map commit texture painting to commiting all the outstanding paint changes */
	auto CanCommitLambda = [this]() -> bool { return GetNumberOfPendingPaintChanges() > 0; };
	CommandList->MapAction(Commands.CommitTexturePainting, FExecuteAction::CreateLambda([this]() { CommitAllPaintedTextures(); }), FCanExecuteAction::CreateLambda([this]() -> bool { return GetNumberOfPendingPaintChanges() > 0; }));
}

void FPaintModePainter::UnregisterCommands(TSharedRef<FUICommandList> CommandList)
{
	/** Unregister previously added commands */
	IMeshPainter::UnregisterCommands(CommandList);
	const FPaintModeCommands& Commands = FPaintModeCommands::Get();
	for (const TSharedPtr<const FUICommandInfo> Action : Commands.Commands)
	{
		CommandList->UnmapAction(Action);
	}
}

const FHitResult FPaintModePainter::GetHitResult(const FVector& Origin, const FVector& Direction)
{
	TArray<UMeshComponent*> HoveredComponents;
	HoveredComponents.Empty(PaintableComponents.Num());

	// Fire out a ray to see if there is a *selected* component under the mouse cursor that can be painted.
	// NOTE: We can't use a GWorld line check for this as that would ignore components that have collision disabled
	FHitResult BestTraceResult;
	{
		const FVector TraceStart(Origin);
		const FVector TraceEnd(Origin + Direction * HALF_WORLD_MAX);

		for (UMeshComponent* MeshComponent : PaintableComponents)
		{
			TSharedPtr<IMeshPaintGeometryAdapter> MeshAdapter = ComponentToAdapterMap.FindChecked(MeshComponent);

			// Ray trace
			FHitResult TraceHitResult(1.0f);

			if (MeshAdapter->LineTraceComponent(TraceHitResult, TraceStart, TraceEnd, FCollisionQueryParams(SCENE_QUERY_STAT(Paint), true)))
			{
				// Find the closest impact
				if ((BestTraceResult.GetComponent() == nullptr) || (TraceHitResult.Time < BestTraceResult.Time))
				{
					BestTraceResult = TraceHitResult;
				}
			}
		}
	}

	return BestTraceResult;
}

void FPaintModePainter::ActorSelected(AActor* Actor)
{
	/** If actor selection changed and we're in texture paint mode update the paint settings for this specific instance */
	if (PaintSettings->PaintMode == EPaintMode::Textures)
	{
		TInlineComponentArray<UMeshComponent*> MeshComponents;
		Actor->GetComponents<UMeshComponent>(MeshComponents);
	
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			FInstanceTexturePaintSettings& Settings = AddOrRetrieveInstanceTexturePaintSettings(MeshComponent);
			PaintSettings->TexturePaintSettings.PaintTexture = Settings.SelectedTexture;
			PaintSettings->TexturePaintSettings.UVChannel = Settings.SelectedUVChannel;
		}
	}
	else if (PaintSettings->PaintMode == EPaintMode::Vertices)
	{
		if (bCachedForceLOD)
		{
			TInlineComponentArray<UMeshComponent*> MeshComponents;
			Actor->GetComponents<UMeshComponent>(MeshComponents);

			for (UMeshComponent* MeshComponent : MeshComponents)
			{
				MeshPaintHelpers::ForceRenderMeshLOD(MeshComponent, CachedLODIndex);
			}
		}
	}

	Refresh();
}

void FPaintModePainter::ActorDeselected(AActor* Actor)
{
	TInlineComponentArray<UMeshComponent*> MeshComponents;
	Actor->GetComponents<UMeshComponent>(MeshComponents);
		
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		/** Try and find an adapter for this component */
		if (IMeshPaintGeometryAdapter* Adapter = ComponentToAdapterMap.FindRef(MeshComponent).Get())
		{	
			/** If in texture paint mode save out the specific instance settings, and reset texture overrides */
			if (PaintSettings->PaintMode == EPaintMode::Textures)
			{
				MeshPaintHelpers::ClearMeshTextureOverrides(*Adapter, MeshComponent);
				FInstanceTexturePaintSettings& Settings = AddOrRetrieveInstanceTexturePaintSettings(MeshComponent);
				Settings.SelectedTexture = PaintSettings->TexturePaintSettings.PaintTexture;
				Settings.SelectedUVChannel = PaintSettings->TexturePaintSettings.UVChannel;
			}
			/** If in vertex painting mode ensure we reset forced LOD rendering and propagate vertex colors to other LODs if necessary */
			else if (PaintSettings->PaintMode == EPaintMode::Vertices)
			{
				if (!bCachedForceLOD)
				{
					MeshPaintHelpers::ApplyVertexColorsToAllLODs(*Adapter, MeshComponent);
				}
				MeshPaintHelpers::ForceRenderMeshLOD(MeshComponent, -1);
				FComponentReregisterContext ReregisterContext(MeshComponent);
			}
		}
	}

	Refresh();
}

void FPaintModePainter::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TexturePaintingCurrentMeshComponent);
	Collector.AddReferencedObject(PaintingTexture2D);
	Collector.AddReferencedObject(BrushRenderTargetTexture);
	Collector.AddReferencedObject(BrushMaskRenderTargetTexture);
	Collector.AddReferencedObject(SeamMaskRenderTargetTexture);
	for (TMap< UTexture2D*, FPaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		Collector.AddReferencedObject(It.Key());
		It.Value().AddReferencedObjects(Collector);
	}

	for (TMap< UMeshComponent*, TSharedPtr<IMeshPaintGeometryAdapter>>::TIterator It(ComponentToAdapterMap); It; ++It)
	{
		Collector.AddReferencedObject(It.Key());
		It.Value()->AddReferencedObjects(Collector);
	}
}

void FPaintModePainter::FinishPainting()
{
	/** Reset state and apply outstanding paint data*/
	IMeshPainter::FinishPainting();	
	FinishPaintingTexture();	
}

bool FPaintModePainter::PaintInternal(const FVector& InCameraOrigin, const FVector& InRayOrigin, const FVector& InRayDirection, EMeshPaintAction PaintAction, float PaintStrength)
{
	const float BrushRadius = BrushSettings->GetBrushRadius();
	// Fire out a ray to see if there is a *selected* component under the mouse cursor that can be painted.
	// NOTE: We can't use a GWorld line check for this as that would ignore components that have collision disabled

	TArray<UMeshComponent*> HoveredComponents;
	FHitResult BestTraceResult;
	{
		const FVector TraceStart(InRayOrigin);
		const FVector TraceEnd(InRayOrigin + InRayDirection * HALF_WORLD_MAX);

		for (UMeshComponent* MeshComponent : PaintableComponents)
		{
			TSharedPtr<IMeshPaintGeometryAdapter>* MeshAdapterPtr = ComponentToAdapterMap.Find(MeshComponent);
			if (!MeshAdapterPtr)
			{
				continue;
			}
			TSharedPtr<IMeshPaintGeometryAdapter> MeshAdapter = *MeshAdapterPtr;

			// Ray trace
			FHitResult TraceHitResult(1.0f);

			if (MeshAdapter->LineTraceComponent(TraceHitResult, TraceStart, TraceEnd, FCollisionQueryParams(SCENE_QUERY_STAT(Paint), true)))
			{
				// Find the closest impact
				if ((BestTraceResult.GetComponent() == nullptr) || (TraceHitResult.Time < BestTraceResult.Time))
				{
					BestTraceResult = TraceHitResult;
				}
			}
		}

		if (BestTraceResult.GetComponent() != nullptr)
		{
			// If we're using texture paint, just use the best trace result we found as we currently only
			// support painting a single mesh at a time in that mode.
			if (PaintSettings->PaintMode == EPaintMode::Textures)
			{
				UMeshComponent* ComponentToPaint = CastChecked<UMeshComponent>(BestTraceResult.GetComponent());
				HoveredComponents.AddUnique(ComponentToPaint);
			}
			else
			{
				FBox BrushBounds = FBox::BuildAABB(BestTraceResult.Location, FVector(BrushRadius * 1.25f, BrushRadius * 1.25f, BrushRadius * 1.25f));

				// Vertex paint mode, so we want all valid components overlapping the brush hit location
				for (auto TestComponent : PaintableComponents)
				{
					const FBox ComponentBounds = TestComponent->Bounds.GetBox();

					if (ComponentToAdapterMap.Contains(TestComponent) && ComponentBounds.Intersect(BrushBounds))
					{
						// OK, this mesh potentially overlaps the brush!
						HoveredComponents.AddUnique(TestComponent);
					}
				}
			}
		}
	}

	const bool bIsPainting = (PaintAction == EMeshPaintAction::Paint);
	const float InStrengthScale = PaintStrength;;

	bool bPaintApplied = false;

	if (HoveredComponents.Num() > 0)
	{
		if (bArePainting == false)
		{
			BeginTransaction(LOCTEXT("MeshPaintMode_VertexPaint_TransactionPaintStroke", "Vertex Paint"));
			bArePainting = true;
			TimeSinceStartedPainting = 0.0f;
		}

		FVector BrushXAxis, BrushYAxis;
		BestTraceResult.Normal.FindBestAxisVectors(BrushXAxis, BrushYAxis);
		// Display settings
		const float VisualBiasDistance = 0.15f;
		const FVector BrushVisualPosition = BestTraceResult.Location + BestTraceResult.Normal * VisualBiasDistance;

		const FLinearColor PaintColor = (PaintSettings->PaintMode == EPaintMode::Vertices) ? (PaintSettings->VertexPaintSettings.PaintColor) : PaintSettings->TexturePaintSettings.PaintColor;
		const FLinearColor EraseColor = (PaintSettings->PaintMode == EPaintMode::Vertices) ? (PaintSettings->VertexPaintSettings.EraseColor) : PaintSettings->TexturePaintSettings.EraseColor;

		// NOTE: We square the brush strength to maximize slider precision in the low range
		const float BrushStrength =
			BrushSettings->BrushStrength *  BrushSettings->BrushStrength *
			InStrengthScale;

		const float BrushDepth = BrushRadius;

		// Mesh paint settings
		FMeshPaintParameters Params;
		{
			Params.PaintMode = PaintSettings->VertexPaintSettings.MeshPaintMode;
			Params.PaintAction = PaintAction;
			Params.BrushPosition = BestTraceResult.Location;
			Params.BrushNormal = BestTraceResult.Normal;
			Params.BrushColor = bIsPainting ? PaintColor : EraseColor;
			Params.SquaredBrushRadius = BrushRadius * BrushRadius;
			Params.BrushRadialFalloffRange = BrushSettings->BrushFalloffAmount * BrushRadius;
			Params.InnerBrushRadius = BrushRadius - Params.BrushRadialFalloffRange;
			Params.BrushDepth = BrushDepth;
			Params.BrushDepthFalloffRange = BrushSettings->BrushFalloffAmount * BrushDepth;
			Params.InnerBrushDepth = BrushDepth - Params.BrushDepthFalloffRange;
			Params.BrushStrength = BrushStrength;
			Params.BrushToWorldMatrix = FMatrix(BrushXAxis, BrushYAxis, Params.BrushNormal, Params.BrushPosition);
			Params.InverseBrushToWorldMatrix = Params.BrushToWorldMatrix.InverseFast();
			Params.bWriteRed = PaintSettings->VertexPaintSettings.bWriteRed;
			Params.bWriteGreen = PaintSettings->VertexPaintSettings.bWriteGreen;
			Params.bWriteBlue = PaintSettings->VertexPaintSettings.bWriteBlue;
			Params.bWriteAlpha = PaintSettings->VertexPaintSettings.bWriteAlpha;
			Params.TotalWeightCount = (int32)PaintSettings->VertexPaintSettings.TextureWeightType;

			// Select texture weight index based on whether or not we're painting or erasing
			{
				const int32 PaintWeightIndex = bIsPainting ? (int32)PaintSettings->VertexPaintSettings.PaintTextureWeightIndex : (int32)PaintSettings->VertexPaintSettings.EraseTextureWeightIndex;

				// Clamp the weight index to fall within the total weight count
				Params.PaintWeightIndex = FMath::Clamp(PaintWeightIndex, 0, Params.TotalWeightCount - 1);
			}

			// @todo MeshPaint: Ideally we would default to: TexturePaintingCurrentMeshComponent->StaticMesh->LightMapCoordinateIndex
			//		Or we could indicate in the GUI which channel is the light map set (button to set it?)
			Params.UVChannel = PaintSettings->TexturePaintSettings.UVChannel;
		}

		// Iterate over the selected meshes under the cursor and paint them!
		for (UMeshComponent* HoveredComponent : HoveredComponents)
		{
			IMeshPaintGeometryAdapter* MeshAdapter = ComponentToAdapterMap.FindRef(HoveredComponent).Get();
			if (!ensure(MeshAdapter))
			{
				continue;
			}

			if (PaintSettings->PaintMode == EPaintMode::Vertices && MeshAdapter->SupportsVertexPaint())
			{

				FPerVertexPaintActionArgs Args;
				Args.Adapter = MeshAdapter;
				Args.CameraPosition = InCameraOrigin;
				Args.HitResult = BestTraceResult;
				Args.BrushSettings = BrushSettings;
				Args.Action = PaintAction;

				bPaintApplied |= MeshPaintHelpers::ApplyPerVertexPaintAction(Args, FPerVertexPaintAction::CreateRaw(this, &FPaintModePainter::ApplyVertexColor, Params));
			}
			else if (PaintSettings->PaintMode == EPaintMode::Textures&& MeshAdapter->SupportsTexturePaint())
			{
				TArray<const UTexture*> Textures;
				const UTexture2D* TargetTexture2D = PaintSettings->TexturePaintSettings.PaintTexture;
				if (TargetTexture2D)
				{
					Textures.Add(TargetTexture2D);

					FPaintTexture2DData* TextureData = GetPaintTargetData(TargetTexture2D);
					if (TextureData)
					{
						Textures.Add(TextureData->PaintRenderTargetTexture);
					}

					TArray<FTexturePaintMeshSectionInfo> MaterialSections;
					TexturePaintHelpers::RetrieveMeshSectionsForTextures(HoveredComponent, CachedLODIndex, Textures, MaterialSections);

					TArray<FTexturePaintTriangleInfo> TrianglePaintInfoArray;
					bPaintApplied |= MeshPaintHelpers::ApplyPerTrianglePaintAction(MeshAdapter, InCameraOrigin, BestTraceResult.Location, BrushSettings, FPerTrianglePaintAction::CreateRaw(this, &FPaintModePainter::GatherTextureTriangles, &TrianglePaintInfoArray, &MaterialSections, PaintSettings->TexturePaintSettings.UVChannel));

					// Painting textures
					if ((TexturePaintingCurrentMeshComponent != nullptr) && (TexturePaintingCurrentMeshComponent != HoveredComponent))
					{
						// Mesh has changed, so finish up with our previous texture
						FinishPaintingTexture();
					}

					if (TexturePaintingCurrentMeshComponent == nullptr)
					{
						StartPaintingTexture(HoveredComponent, *MeshAdapter);
					}

					if (TexturePaintingCurrentMeshComponent != nullptr)
					{
						Params.bWriteRed = PaintSettings->TexturePaintSettings.bWriteRed;
						Params.bWriteGreen = PaintSettings->TexturePaintSettings.bWriteGreen;
						Params.bWriteBlue = PaintSettings->TexturePaintSettings.bWriteBlue;
						Params.bWriteAlpha = PaintSettings->TexturePaintSettings.bWriteAlpha;

						PaintTexture(Params, TrianglePaintInfoArray, *MeshAdapter);
					}
				}
			}
		}
	}

	return bPaintApplied;
}

void FPaintModePainter::ApplyVertexColor(FPerVertexPaintActionArgs& InArgs, int32 VertexIndex, FMeshPaintParameters Parameters)
{
	/** Retrieve vertex position and color for applying vertex painting */
	FColor PaintColor;
	FVector Position;
	InArgs.Adapter->GetVertexPosition(VertexIndex, Position);
	Position = InArgs.Adapter->GetComponentToWorldMatrix().TransformPosition(Position);
	InArgs.Adapter->GetVertexColor(VertexIndex, PaintColor, true);			
	MeshPaintHelpers::PaintVertex(Position, Parameters, PaintColor);
	InArgs.Adapter->SetVertexColor(VertexIndex, PaintColor, true);
}

void FPaintModePainter::GatherTextureTriangles(IMeshPaintGeometryAdapter* Adapter, int32 TriangleIndex, const int32 VertexIndices[3], TArray<FTexturePaintTriangleInfo>* TriangleInfo, TArray<FTexturePaintMeshSectionInfo>* SectionInfos, int32 UVChannelIndex)
{
	/** Retrieve triangles eligible for texture painting */
	bool bAdd = SectionInfos->Num() == 0;
	for (const FTexturePaintMeshSectionInfo& SectionInfo : *SectionInfos)
	{
		if (TriangleIndex >= SectionInfo.FirstIndex && TriangleIndex < SectionInfo.LastIndex)
		{
			bAdd = true;
			break;			
		}
	}

	if (bAdd)
	{
		FTexturePaintTriangleInfo Info;
		Adapter->GetVertexPosition(VertexIndices[0], Info.TriVertices[0]);
		Adapter->GetVertexPosition(VertexIndices[1], Info.TriVertices[1]);
		Adapter->GetVertexPosition(VertexIndices[2], Info.TriVertices[2]);
		Info.TriVertices[0] = Adapter->GetComponentToWorldMatrix().TransformPosition(Info.TriVertices[0]);
		Info.TriVertices[1] = Adapter->GetComponentToWorldMatrix().TransformPosition(Info.TriVertices[1]);
		Info.TriVertices[2] = Adapter->GetComponentToWorldMatrix().TransformPosition(Info.TriVertices[2]);				
		Adapter->GetTextureCoordinate(VertexIndices[0], UVChannelIndex, Info.TriUVs[0]);
		Adapter->GetTextureCoordinate(VertexIndices[1], UVChannelIndex, Info.TriUVs[1]);
		Adapter->GetTextureCoordinate(VertexIndices[2], UVChannelIndex, Info.TriUVs[2]);
		TriangleInfo->Add(Info);
	}
}

void FPaintModePainter::Reset()
{		
	//If we're painting vertex colors then propagate the painting done on LOD0 to all lower LODs. 
	//Then stop forcing the LOD level of the mesh to LOD0.
	ApplyForcedLODIndex(-1);
	if (!PaintSettings->VertexPaintSettings.bPaintOnSpecificLOD)
	{
		for (auto Pair : ComponentToAdapterMap)
		{
			MeshPaintHelpers::ApplyVertexColorsToAllLODs(*Pair.Value.Get(), Pair.Key);
		}
	}

	// If the user has pending changes and the editor is not exiting, we want to do the commit for all the modified textures.
	if ((GetNumberOfPendingPaintChanges() > 0) && !GIsRequestingExit)
	{
		CommitAllPaintedTextures();
	}
	else
	{
		ClearAllTextureOverrides();
	}

	PaintTargetData.Empty();

	// Remove any existing texture targets
	TexturePaintTargetList.Empty();
	
	// Cleanup all cached 
	for (auto MeshAdapterPair : ComponentToAdapterMap)
	{
		MeshAdapterPair.Value->OnRemoved();
	}
	ComponentToAdapterMap.Empty();
}

TSharedPtr<IMeshPaintGeometryAdapter> FPaintModePainter::GetMeshAdapterForComponent(const UMeshComponent* Component)
{
	return ComponentToAdapterMap.FindChecked(Component);
}

bool FPaintModePainter::ContainsDuplicateMeshes(TArray<UMeshComponent*>& Components) const
{
	TArray<UObject*> Objects;	
	
	bool bDuplicates = false;
	// Check for components painting to the same static/skeletal mesh
	for (UMeshComponent* Component : Components)
	{
		UObject* Object = nullptr;
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			Object = StaticMeshComponent->GetStaticMesh();
		}
		else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component))
		{
			Object = SkeletalMeshComponent->SkeletalMesh;
		}

		if (Object)
		{
			if (Objects.Contains(Object))
			{
				bDuplicates = true;
				break;
			}
			else
			{
				Objects.Add(Object);
			}
		}
	}

	return bDuplicates;
}

int32 FPaintModePainter::GetMaxLODIndexToPaint() const
{
	//The maximum LOD we can paint is decide by the lowest number of LOD in the selection
	int32 LODMin = TNumericLimits<int32>::Max();

	TArray<const UMeshComponent*> SelectedComponents = GetSelectedComponents<const UMeshComponent>();

	for (const UMeshComponent* MeshComponent : SelectedComponents )
	{
		LODMin = FMath::Min(LODMin, MeshPaintHelpers::GetNumberOfLODs(MeshComponent) - 1);
	}

	if (LODMin == TNumericLimits<int32>::Max())
	{
		LODMin = 1;
	}
	
	return LODMin;
}

void FPaintModePainter::LODPaintStateChanged(const bool bLODPaintingEnabled)
{
	checkf(PaintSettings->PaintMode == EPaintMode::Vertices, TEXT("Can only change this state in vertex paint mode"));
	bool AbortChange = false;
	if (!bLODPaintingEnabled)
	{
		//Only show the lost data warning if there is actually some data to loose
		if (bSelectionContainsPerLODColors)
		{
			//Warn the user he will loose is custom painting data
			FSuppressableWarningDialog::FSetupInfo SetupInfo(LOCTEXT("LooseLowersLODsVertexColorsPrompt_Message", "Changing from custom LODs to base LOD only painting will propagate the base lod vertex color to all lowers LODs. This mean all lowers LODs custom vertex painting will be lost."),
				LOCTEXT("LooseLowersLODsVertexColorsPrompt_Title", "Warning: Lowers LODs custom vertex painting will be lost!"), "Warning_LooseLowersLODsVertexColorsPrompt");

			SetupInfo.ConfirmText = LOCTEXT("LooseLowersLODsVertexColorsPrompt_ConfirmText", "Continue");
			SetupInfo.CancelText = LOCTEXT("LooseLowersLODsVertexColorsPrompt_CancelText", "Abort");
			SetupInfo.CheckBoxText = LOCTEXT("LooseLowersLODsVertexColorsPrompt_CheckBoxText", "Always copy vertex colors without prompting");

			FSuppressableWarningDialog LooseLowersLODsVertexColorsWarning(SetupInfo);

			// Prompt the user to see if they really want to propagate the base lod vert colors to the lowers LODs.
			if (LooseLowersLODsVertexColorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel)
			{
				AbortChange = true;
			}
			else
			{
				//Remove painting on all lowers LODs before doing the propagation
				for (UMeshComponent* SelectedComponent : PaintableComponents)
				{
					UStaticMeshComponent *StaticMeshComponent = Cast<UStaticMeshComponent>(SelectedComponent);
					if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
					{
						// Mark the mesh component as modified
						StaticMeshComponent->Modify();

						// If this is called from the Remove button being clicked the SMC wont be in a Reregister context,
						// but when it gets called from a Paste or Copy to Source operation it's already inside a more specific
						// SMCRecreateScene context so we shouldn't put it inside another one.
						if (StaticMeshComponent->IsRenderStateCreated())
						{
							// Detach all instances of this static mesh from the scene.
							FComponentReregisterContext ComponentReregisterContext(StaticMeshComponent);

							for (int32 LODIndex = 1; LODIndex < StaticMeshComponent->LODData.Num(); ++LODIndex)
							{
								StaticMeshComponent->RemoveInstanceVertexColorsFromLOD(LODIndex);
							}
						}
						else
						{
							for (int32 LODIndex = 1; LODIndex < StaticMeshComponent->LODData.Num(); ++LODIndex)
							{
								StaticMeshComponent->RemoveInstanceVertexColorsFromLOD(LODIndex);
							}
						}
					}
				}
			}
		}

		//The user cancel the change, avoid changing the value
		if (AbortChange)
		{
			return;
		}
	}

	for (UMeshComponent* SelectedComponent : PaintableComponents)
	{
		//Always propagate the base LOD when we switch the painting to all LODs
		//In case we go from false to true, we want to tweak the propagation of the base LOD
		//In case we go from true to false, we want to force a propagate to be sure the custom painting is done over what the user already paint
		//In case the staticmesh has some data comming from fbx, the propagate will not do anything
		//if there is nothing that was overrite in the base LOD. So the user will be able to paint
		//over the painting done in a DCC.
		TSharedPtr<IMeshPaintGeometryAdapter> MeshAdapter = ComponentToAdapterMap.FindChecked(SelectedComponent);
		MeshPaintHelpers::ApplyVertexColorsToAllLODs(*MeshAdapter, SelectedComponent);
	}

	// Set actual flag in the settings struct
	PaintSettings->VertexPaintSettings.bPaintOnSpecificLOD = bLODPaintingEnabled;

	ApplyForcedLODIndex(bLODPaintingEnabled ? CachedLODIndex : -1);
	TUniquePtr< FComponentReregisterContext > ComponentReregisterContext;
	//Make sure all static mesh render is dirty since we change the force LOD
	for (UMeshComponent* SelectedComponent : PaintableComponents)
	{
		ComponentReregisterContext.Reset(new FComponentReregisterContext(SelectedComponent));
	}

	Refresh();
}

void FPaintModePainter::PaintLODChanged()
{
	// Enforced LOD for painting
	if (CachedLODIndex != PaintSettings->VertexPaintSettings.LODIndex)
	{
		CachedLODIndex = PaintSettings->VertexPaintSettings.LODIndex;
		ApplyForcedLODIndex(bCachedForceLOD ? CachedLODIndex : -1);

		TUniquePtr< FComponentReregisterContext > ComponentReregisterContext;
		//Make sure all static mesh render is dirty since we change the force LOD
		for (UMeshComponent* SelectedComponent : PaintableComponents)
		{
			ComponentReregisterContext.Reset(new FComponentReregisterContext(SelectedComponent));
		}

		Refresh();
	}
}

int32 FPaintModePainter::GetMaxUVIndexToPaint() const
{
	if (PaintableComponents.Num() == 1)
	{
		return MeshPaintHelpers::GetNumberOfUVs(PaintableComponents[0], CachedLODIndex) - 1;
	}
	
	return 0;
}

void FPaintModePainter::StartPaintingTexture(UMeshComponent* InMeshComponent, const IMeshPaintGeometryAdapter& GeometryInfo)
{
	check(InMeshComponent != nullptr);
	check(TexturePaintingCurrentMeshComponent == nullptr);
	check(PaintingTexture2D == nullptr);

	const auto FeatureLevel = InMeshComponent->GetWorld()->FeatureLevel;

	UTexture2D* Texture2D = PaintSettings->TexturePaintSettings.PaintTexture;
	if (Texture2D == nullptr)
	{
		return;
	}

	bool bStartedPainting = false;
	FPaintTexture2DData* TextureData = GetPaintTargetData(Texture2D);

	// Check all the materials on the mesh to see if the user texture is there
	int32 MaterialIndex = 0;
	UMaterialInterface* MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	while (MaterialToCheck != nullptr)
	{
		bool bIsTextureUsed = TexturePaintHelpers::DoesMeshComponentUseTexture(InMeshComponent, Texture2D);

		if (!bIsTextureUsed && (TextureData != nullptr) && (TextureData->PaintRenderTargetTexture != nullptr))
		{
			bIsTextureUsed = TexturePaintHelpers::DoesMeshComponentUseTexture(InMeshComponent, TextureData->PaintRenderTargetTexture);
		}

		if (bIsTextureUsed && !bStartedPainting)
		{
			bool bIsSourceTextureStreamedIn = Texture2D->IsFullyStreamedIn();

			if (!bIsSourceTextureStreamedIn)
			{
				// We found that this texture is used in one of the meshes materials but not fully loaded, we will
				//   attempt to fully stream in the texture before we try to do anything with it.
				Texture2D->SetForceMipLevelsToBeResident(30.0f);
				Texture2D->WaitForStreaming();

				// We do a quick sanity check to make sure it is streamed fully streamed in now.
				bIsSourceTextureStreamedIn = Texture2D->IsFullyStreamedIn();

			}

			if (bIsSourceTextureStreamedIn)
			{
				const int32 TextureWidth = Texture2D->Source.GetSizeX();
				const int32 TextureHeight = Texture2D->Source.GetSizeY();

				if (TextureData == nullptr)
				{
					TextureData = AddPaintTargetData(Texture2D);
				}
				check(TextureData != nullptr);

				// Create our render target texture
				if (TextureData->PaintRenderTargetTexture == nullptr ||
					TextureData->PaintRenderTargetTexture->GetSurfaceWidth() != TextureWidth ||
					TextureData->PaintRenderTargetTexture->GetSurfaceHeight() != TextureHeight)
				{
					TextureData->PaintRenderTargetTexture = nullptr;
					TextureData->PaintRenderTargetTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
					TextureData->PaintRenderTargetTexture->bNeedsTwoCopies = true;
					const bool bForceLinearGamma = true;
					TextureData->PaintRenderTargetTexture->InitCustomFormat(TextureWidth, TextureHeight, PF_A16B16G16R16, bForceLinearGamma);
					TextureData->PaintRenderTargetTexture->UpdateResourceImmediate();

					//Duplicate the texture we are painting and store it in the transient package. This texture is a backup of the data incase we want to revert before commiting.
					TextureData->PaintingTexture2DDuplicate = (UTexture2D*)StaticDuplicateObject(Texture2D, GetTransientPackage(), *FString::Printf(TEXT("%s_TEMP"), *Texture2D->GetName()));
				}
				TextureData->PaintRenderTargetTexture->AddressX = Texture2D->AddressX;
				TextureData->PaintRenderTargetTexture->AddressY = Texture2D->AddressY;

				const int32 BrushTargetTextureWidth = TextureWidth;
				const int32 BrushTargetTextureHeight = TextureHeight;

				// Create the rendertarget used to store our paint delta
				if (BrushRenderTargetTexture == nullptr ||
					BrushRenderTargetTexture->GetSurfaceWidth() != BrushTargetTextureWidth ||
					BrushRenderTargetTexture->GetSurfaceHeight() != BrushTargetTextureHeight)
				{
					BrushRenderTargetTexture = nullptr;
					BrushRenderTargetTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
					const bool bForceLinearGamma = true;
					BrushRenderTargetTexture->ClearColor = FLinearColor::Black;
					BrushRenderTargetTexture->bNeedsTwoCopies = true;
					BrushRenderTargetTexture->InitCustomFormat(BrushTargetTextureWidth, BrushTargetTextureHeight, PF_A16B16G16R16, bForceLinearGamma);
					BrushRenderTargetTexture->UpdateResourceImmediate();
					BrushRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
					BrushRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;
				}

				if (PaintSettings->TexturePaintSettings.bEnableSeamPainting)
				{
					// Create the rendertarget used to store a mask for our paint delta area 
					if (BrushMaskRenderTargetTexture == nullptr ||
						BrushMaskRenderTargetTexture->GetSurfaceWidth() != BrushTargetTextureWidth ||
						BrushMaskRenderTargetTexture->GetSurfaceHeight() != BrushTargetTextureHeight)
					{
						BrushMaskRenderTargetTexture = nullptr;
						BrushMaskRenderTargetTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
						const bool bForceLinearGamma = true;
						BrushMaskRenderTargetTexture->ClearColor = FLinearColor::Black;
						BrushMaskRenderTargetTexture->bNeedsTwoCopies = true;
						BrushMaskRenderTargetTexture->InitCustomFormat(BrushTargetTextureWidth, BrushTargetTextureHeight, PF_B8G8R8A8, bForceLinearGamma);
						BrushMaskRenderTargetTexture->UpdateResourceImmediate();
						BrushMaskRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
						BrushMaskRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;
					}

					// Create the rendertarget used to store a texture seam mask
					if (SeamMaskRenderTargetTexture == nullptr ||
						SeamMaskRenderTargetTexture->GetSurfaceWidth() != TextureWidth ||
						SeamMaskRenderTargetTexture->GetSurfaceHeight() != TextureHeight)
					{
						SeamMaskRenderTargetTexture = nullptr;
						SeamMaskRenderTargetTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
						const bool bForceLinearGamma = true;
						SeamMaskRenderTargetTexture->ClearColor = FLinearColor::Black;
						SeamMaskRenderTargetTexture->bNeedsTwoCopies = true;
						SeamMaskRenderTargetTexture->InitCustomFormat(BrushTargetTextureWidth, BrushTargetTextureHeight, PF_B8G8R8A8, bForceLinearGamma);
						SeamMaskRenderTargetTexture->UpdateResourceImmediate();
						SeamMaskRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
						SeamMaskRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;
					}

					bGenerateSeamMask = true;
				}

				bStartedPainting = true;
			}
		}

		// @todo MeshPaint: Here we override the textures on the mesh with the render target.  The problem is that other meshes in the scene that use
		//    this texture do not get the override. Do we want to extend this to all other selected meshes or maybe even to all meshes in the scene?
		if (bIsTextureUsed && bStartedPainting && !TextureData->PaintingMaterials.Contains(MaterialToCheck))
		{
			TextureData->PaintingMaterials.AddUnique(MaterialToCheck);

			GeometryInfo.ApplyOrRemoveTextureOverride(Texture2D, TextureData->PaintRenderTargetTexture);
		}

		MaterialIndex++;
		MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	}

	if (bStartedPainting)
	{
		TexturePaintingCurrentMeshComponent = InMeshComponent;

		check(Texture2D != nullptr);
		PaintingTexture2D = Texture2D;
		// OK, now we need to make sure our render target is filled in with data
		TexturePaintHelpers::SetupInitialRenderTargetData(TextureData->PaintingTexture2D, TextureData->PaintRenderTargetTexture);
	}
}

void FPaintModePainter::PaintTexture(const FMeshPaintParameters& InParams, TArray<FTexturePaintTriangleInfo>& InInfluencedTriangles, const IMeshPaintGeometryAdapter& GeometryInfo)
{
	// We bail early if there are no influenced triangles
	if (InInfluencedTriangles.Num() <= 0)
	{
		return;
	}

	const auto FeatureLevel = GEditor->GetEditorWorldContext().World()->FeatureLevel;


	FPaintTexture2DData* TextureData = GetPaintTargetData(PaintingTexture2D);
	check(TextureData != nullptr && TextureData->PaintRenderTargetTexture != nullptr);

	// Copy the current image to the brush rendertarget texture.
	{
		check(BrushRenderTargetTexture != nullptr);
		TexturePaintHelpers::CopyTextureToRenderTargetTexture(TextureData->PaintRenderTargetTexture, BrushRenderTargetTexture, FeatureLevel);
	}

	const bool bEnableSeamPainting = PaintSettings->TexturePaintSettings.bEnableSeamPainting;
	const FMatrix WorldToBrushMatrix = InParams.InverseBrushToWorldMatrix;

	// Grab the actual render target resource from the textures.  Note that we're absolutely NOT ALLOWED to
	// dereference these pointers.  We're just passing them along to other functions that will use them on the render
	// thread.  The only thing we're allowed to do is check to see if they are nullptr or not.
	FTextureRenderTargetResource* BrushRenderTargetResource = BrushRenderTargetTexture->GameThread_GetRenderTargetResource();
	check(BrushRenderTargetResource != nullptr);

	// Create a canvas for the brush render target.
	FCanvas BrushPaintCanvas(BrushRenderTargetResource, nullptr, 0, 0, 0, FeatureLevel);

	// Parameters for brush paint
	TRefCountPtr< FMeshPaintBatchedElementParameters > MeshPaintBatchedElementParameters(new FMeshPaintBatchedElementParameters());
	{
		MeshPaintBatchedElementParameters->ShaderParams.CloneTexture = BrushRenderTargetTexture;
		MeshPaintBatchedElementParameters->ShaderParams.WorldToBrushMatrix = WorldToBrushMatrix;
		MeshPaintBatchedElementParameters->ShaderParams.BrushRadius = InParams.InnerBrushRadius + InParams.BrushRadialFalloffRange;
		MeshPaintBatchedElementParameters->ShaderParams.BrushRadialFalloffRange = InParams.BrushRadialFalloffRange;
		MeshPaintBatchedElementParameters->ShaderParams.BrushDepth = InParams.InnerBrushDepth + InParams.BrushDepthFalloffRange;
		MeshPaintBatchedElementParameters->ShaderParams.BrushDepthFalloffRange = InParams.BrushDepthFalloffRange;
		MeshPaintBatchedElementParameters->ShaderParams.BrushStrength = InParams.BrushStrength;
		MeshPaintBatchedElementParameters->ShaderParams.BrushColor = InParams.BrushColor;
		MeshPaintBatchedElementParameters->ShaderParams.RedChannelFlag = InParams.bWriteRed;
		MeshPaintBatchedElementParameters->ShaderParams.GreenChannelFlag = InParams.bWriteGreen;
		MeshPaintBatchedElementParameters->ShaderParams.BlueChannelFlag = InParams.bWriteBlue;
		MeshPaintBatchedElementParameters->ShaderParams.AlphaChannelFlag = InParams.bWriteAlpha;
		MeshPaintBatchedElementParameters->ShaderParams.GenerateMaskFlag = false;
	}

	FBatchedElements* BrushPaintBatchedElements = BrushPaintCanvas.GetBatchedElements(FCanvas::ET_Triangle, MeshPaintBatchedElementParameters, nullptr, SE_BLEND_Opaque);
	BrushPaintBatchedElements->AddReserveVertices(InInfluencedTriangles.Num() * 3);
	BrushPaintBatchedElements->AddReserveTriangles(InInfluencedTriangles.Num(), nullptr, SE_BLEND_Opaque);

	FHitProxyId BrushPaintHitProxyId = BrushPaintCanvas.GetHitProxyId();

	TSharedPtr<FCanvas> BrushMaskCanvas;
	TRefCountPtr< FMeshPaintBatchedElementParameters > MeshPaintMaskBatchedElementParameters;
	FBatchedElements* BrushMaskBatchedElements = nullptr;
	FHitProxyId BrushMaskHitProxyId;
	FTextureRenderTargetResource* BrushMaskRenderTargetResource = nullptr;

	if (bEnableSeamPainting)
	{
		BrushMaskRenderTargetResource = BrushMaskRenderTargetTexture->GameThread_GetRenderTargetResource();
		check(BrushMaskRenderTargetResource != nullptr);

		// Create a canvas for the brush mask rendertarget and clear it to black.
		BrushMaskCanvas = TSharedPtr<FCanvas>(new FCanvas(BrushMaskRenderTargetResource, nullptr, 0, 0, 0, FeatureLevel));
		BrushMaskCanvas->Clear(FLinearColor::Black);

		// Parameters for the mask
		MeshPaintMaskBatchedElementParameters = TRefCountPtr< FMeshPaintBatchedElementParameters >(new FMeshPaintBatchedElementParameters());
		{
			MeshPaintMaskBatchedElementParameters->ShaderParams.CloneTexture = TextureData->PaintRenderTargetTexture;
			MeshPaintMaskBatchedElementParameters->ShaderParams.WorldToBrushMatrix = WorldToBrushMatrix;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushRadius = InParams.InnerBrushRadius + InParams.BrushRadialFalloffRange;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushRadialFalloffRange = InParams.BrushRadialFalloffRange;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushDepth = InParams.InnerBrushDepth + InParams.BrushDepthFalloffRange;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushDepthFalloffRange = InParams.BrushDepthFalloffRange;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushStrength = InParams.BrushStrength;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BrushColor = InParams.BrushColor;
			MeshPaintMaskBatchedElementParameters->ShaderParams.RedChannelFlag = InParams.bWriteRed;
			MeshPaintMaskBatchedElementParameters->ShaderParams.GreenChannelFlag = InParams.bWriteGreen;
			MeshPaintMaskBatchedElementParameters->ShaderParams.BlueChannelFlag = InParams.bWriteBlue;
			MeshPaintMaskBatchedElementParameters->ShaderParams.AlphaChannelFlag = InParams.bWriteAlpha;
			MeshPaintMaskBatchedElementParameters->ShaderParams.GenerateMaskFlag = true;
		}

		BrushMaskBatchedElements = BrushMaskCanvas->GetBatchedElements(FCanvas::ET_Triangle, MeshPaintMaskBatchedElementParameters, nullptr, SE_BLEND_Opaque);
		BrushMaskBatchedElements->AddReserveVertices(InInfluencedTriangles.Num() * 3);
		BrushMaskBatchedElements->AddReserveTriangles(InInfluencedTriangles.Num(), nullptr, SE_BLEND_Opaque);

		BrushMaskHitProxyId = BrushMaskCanvas->GetHitProxyId();
	}

	// Process the influenced triangles - storing off a large list is much slower than processing in a single loop
	for (int32 CurIndex = 0; CurIndex < InInfluencedTriangles.Num(); ++CurIndex)
	{
		FTexturePaintTriangleInfo& CurTriangle = InInfluencedTriangles[CurIndex];		

		FVector2D UVMin(99999.9f, 99999.9f);
		FVector2D UVMax(-99999.9f, -99999.9f);

		// Transform the triangle and update the UV bounds
		for (int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum)
		{
			// Update bounds
			float U = CurTriangle.TriUVs[TriVertexNum].X;
			float V = CurTriangle.TriUVs[TriVertexNum].Y;

			if (U < UVMin.X)
			{
				UVMin.X = U;
			}
			if (U > UVMax.X)
			{
				UVMax.X = U;
			}
			if (V < UVMin.Y)
			{
				UVMin.Y = V;
			}
			if (V > UVMax.Y)
			{
				UVMax.Y = V;
			}
		}

		// If the triangle lies entirely outside of the 0.0-1.0 range, we'll transpose it back
		FVector2D UVOffset(0.0f, 0.0f);
		if (UVMax.X > 1.0f)
		{
			UVOffset.X = -FMath::FloorToFloat(UVMin.X);
		}
		else if (UVMin.X < 0.0f)
		{
			UVOffset.X = 1.0f + FMath::FloorToFloat(-UVMax.X);
		}

		if (UVMax.Y > 1.0f)
		{
			UVOffset.Y = -FMath::FloorToFloat(UVMin.Y);
		}
		else if (UVMin.Y < 0.0f)
		{
			UVOffset.Y = 1.0f + FMath::FloorToFloat(-UVMax.Y);
		}

		// Note that we "wrap" the texture coordinates here to handle the case where the user
		// is painting on a tiling texture, or with the UVs out of bounds.  Ideally all of the
		// UVs would be in the 0.0 - 1.0 range but sometimes content isn't setup that way.
		// @todo MeshPaint: Handle triangles that cross the 0.0-1.0 UV boundary?
		for (int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum)
		{
			CurTriangle.TriUVs[TriVertexNum].X += UVOffset.X;
			CurTriangle.TriUVs[TriVertexNum].Y += UVOffset.Y;

			// @todo: Need any half-texel offset adjustments here? Some info about offsets and MSAA here: http://drilian.com/2008/11/25/understanding-half-pixel-and-half-texel-offsets/
			// @todo: MeshPaint: Screen-space texture coords: http://diaryofagraphicsprogrammer.blogspot.com/2008/09/calculating-screen-space-texture.html
			CurTriangle.TrianglePoints[TriVertexNum].X = CurTriangle.TriUVs[TriVertexNum].X * TextureData->PaintRenderTargetTexture->GetSurfaceWidth();
			CurTriangle.TrianglePoints[TriVertexNum].Y = CurTriangle.TriUVs[TriVertexNum].Y * TextureData->PaintRenderTargetTexture->GetSurfaceHeight();
		}

		// Vertex positions
		FVector4 Vert0(CurTriangle.TrianglePoints[0].X, CurTriangle.TrianglePoints[0].Y, 0, 1);
		FVector4 Vert1(CurTriangle.TrianglePoints[1].X, CurTriangle.TrianglePoints[1].Y, 0, 1);
		FVector4 Vert2(CurTriangle.TrianglePoints[2].X, CurTriangle.TrianglePoints[2].Y, 0, 1);

		// Vertex color
		FLinearColor Col0(CurTriangle.TriVertices[0].X, CurTriangle.TriVertices[0].Y, CurTriangle.TriVertices[0].Z);
		FLinearColor Col1(CurTriangle.TriVertices[1].X, CurTriangle.TriVertices[1].Y, CurTriangle.TriVertices[1].Z);
		FLinearColor Col2(CurTriangle.TriVertices[2].X, CurTriangle.TriVertices[2].Y, CurTriangle.TriVertices[2].Z);

		// Brush Paint triangle
		{
			int32 V0 = BrushPaintBatchedElements->AddVertex(Vert0, CurTriangle.TriUVs[0], Col0, BrushPaintHitProxyId);
			int32 V1 = BrushPaintBatchedElements->AddVertex(Vert1, CurTriangle.TriUVs[1], Col1, BrushPaintHitProxyId);
			int32 V2 = BrushPaintBatchedElements->AddVertex(Vert2, CurTriangle.TriUVs[2], Col2, BrushPaintHitProxyId);

			BrushPaintBatchedElements->AddTriangle(V0, V1, V2, MeshPaintBatchedElementParameters, SE_BLEND_Opaque);
		}

		// Brush Mask triangle
		if (bEnableSeamPainting)
		{
			int32 V0 = BrushMaskBatchedElements->AddVertex(Vert0, CurTriangle.TriUVs[0], Col0, BrushMaskHitProxyId);
			int32 V1 = BrushMaskBatchedElements->AddVertex(Vert1, CurTriangle.TriUVs[1], Col1, BrushMaskHitProxyId);
			int32 V2 = BrushMaskBatchedElements->AddVertex(Vert2, CurTriangle.TriUVs[2], Col2, BrushMaskHitProxyId);

			BrushMaskBatchedElements->AddTriangle(V0, V1, V2, MeshPaintMaskBatchedElementParameters, SE_BLEND_Opaque);
		}
	}

	// Tell the rendering thread to draw any remaining batched elements
	{
		BrushPaintCanvas.Flush_GameThread(true);

		TextureData->bIsPaintingTexture2DModified = true;
	}

	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand1,
			FTextureRenderTargetResource*, BrushRenderTargetResource, BrushRenderTargetResource,
			{
				// Copy (resolve) the rendered image from the frame buffer to its render target texture
				RHICmdList.CopyToResolveTarget(
					BrushRenderTargetResource->GetRenderTargetTexture(),	// Source texture
					BrushRenderTargetResource->TextureRHI,
					true,													// Do we need the source image content again?
					FResolveParams());										// Resolve parameters
			});
	}


	if (bEnableSeamPainting)
	{
		BrushMaskCanvas->Flush_GameThread(true);

		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				UpdateMeshPaintRTCommand2,
				FTextureRenderTargetResource*, BrushMaskRenderTargetResource, BrushMaskRenderTargetResource,
				{
					// Copy (resolve) the rendered image from the frame buffer to its render target texture
					RHICmdList.CopyToResolveTarget(
						BrushMaskRenderTargetResource->GetRenderTargetTexture(),		// Source texture
						BrushMaskRenderTargetResource->TextureRHI,
						true,												// Do we need the source image content again?
						FResolveParams());									// Resolve parameters
				});

		}
	}

	if (!bEnableSeamPainting)
	{
		// Seam painting is not enabled so we just copy our delta paint info to the paint target.
		TexturePaintHelpers::CopyTextureToRenderTargetTexture(BrushRenderTargetTexture, TextureData->PaintRenderTargetTexture, FeatureLevel);
	}
	else
	{

		// Constants used for generating quads across entire paint rendertarget
		const float MinU = 0.0f;
		const float MinV = 0.0f;
		const float MaxU = 1.0f;
		const float MaxV = 1.0f;
		const float MinX = 0.0f;
		const float MinY = 0.0f;
		const float MaxX = TextureData->PaintRenderTargetTexture->GetSurfaceWidth();
		const float MaxY = TextureData->PaintRenderTargetTexture->GetSurfaceHeight();

		if (bGenerateSeamMask == true)
		{
			// Generate the texture seam mask.  This is a slow operation when the object has many triangles so we only do it
			//  once when painting is started.

			FPaintTexture2DData* SeamTextureData = GetPaintTargetData(PaintSettings->TexturePaintSettings.PaintTexture);

			TexturePaintHelpers::GenerateSeamMask(TexturePaintingCurrentMeshComponent, InParams.UVChannel, SeamMaskRenderTargetTexture, PaintSettings->TexturePaintSettings.PaintTexture, SeamTextureData != nullptr ? SeamTextureData->PaintRenderTargetTexture : nullptr);
			bGenerateSeamMask = false;
		}

		FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
		check(RenderTargetResource != nullptr);
		// Dilate the paint stroke into the texture seams.
		{
			// Create a canvas for the render target.
			FCanvas Canvas3(RenderTargetResource, nullptr, 0, 0, 0, FeatureLevel);


			TRefCountPtr< FMeshPaintDilateBatchedElementParameters > MeshPaintDilateBatchedElementParameters(new FMeshPaintDilateBatchedElementParameters());
			{
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture0 = BrushRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture1 = SeamMaskRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture2 = BrushMaskRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.WidthPixelOffset = (float)(1.0f / TextureData->PaintRenderTargetTexture->GetSurfaceWidth());
				MeshPaintDilateBatchedElementParameters->ShaderParams.HeightPixelOffset = (float)(1.0f / TextureData->PaintRenderTargetTexture->GetSurfaceHeight());

			}

			// Draw a quad to copy the texture over to the render target
			TArray< FCanvasUVTri >	TriangleList;
			FCanvasUVTri SingleTri;
			SingleTri.V0_Pos = FVector2D(MinX, MinY);
			SingleTri.V0_UV = FVector2D(MinU, MinV);
			SingleTri.V0_Color = FLinearColor::White;

			SingleTri.V1_Pos = FVector2D(MaxX, MinY);
			SingleTri.V1_UV = FVector2D(MaxU, MinV);
			SingleTri.V1_Color = FLinearColor::White;

			SingleTri.V2_Pos = FVector2D(MaxX, MaxY);
			SingleTri.V2_UV = FVector2D(MaxU, MaxV);
			SingleTri.V2_Color = FLinearColor::White;
			TriangleList.Add(SingleTri);

			SingleTri.V0_Pos = FVector2D(MaxX, MaxY);
			SingleTri.V0_UV = FVector2D(MaxU, MaxV);
			SingleTri.V0_Color = FLinearColor::White;

			SingleTri.V1_Pos = FVector2D(MinX, MaxY);
			SingleTri.V1_UV = FVector2D(MinU, MaxV);
			SingleTri.V1_Color = FLinearColor::White;

			SingleTri.V2_Pos = FVector2D(MinX, MinY);
			SingleTri.V2_UV = FVector2D(MinU, MinV);
			SingleTri.V2_Color = FLinearColor::White;
			TriangleList.Add(SingleTri);

			FCanvasTriangleItem TriItemList(TriangleList, nullptr);
			TriItemList.BatchedElementParameters = MeshPaintDilateBatchedElementParameters;
			TriItemList.BlendMode = SE_BLEND_Opaque;
			Canvas3.DrawItem(TriItemList);


			// Tell the rendering thread to draw any remaining batched elements
			Canvas3.Flush_GameThread(true);

		}

		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				UpdateMeshPaintRTCommand3,
				FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
				{
					// Copy (resolve) the rendered image from the frame buffer to its render target texture
					RHICmdList.CopyToResolveTarget(
						RenderTargetResource->GetRenderTargetTexture(),		// Source texture
						RenderTargetResource->TextureRHI,
						true,												// Do we need the source image content again?
						FResolveParams());									// Resolve parameters
				});

		}

	}
	FlushRenderingCommands();
}

void FPaintModePainter::FinishPaintingTexture()
{
	if (TexturePaintingCurrentMeshComponent != nullptr)
	{
		check(PaintingTexture2D != nullptr);

		FPaintTexture2DData* TextureData = GetPaintTargetData(PaintingTexture2D);
		check(TextureData);

		// Commit to the texture source art but don't do any compression, compression is saved for the CommitAllPaintedTextures function.
		if (TextureData->bIsPaintingTexture2DModified == true)
		{
			const int32 TexWidth = TextureData->PaintRenderTargetTexture->SizeX;
			const int32 TexHeight = TextureData->PaintRenderTargetTexture->SizeY;
			TArray< FColor > TexturePixels;
			TexturePixels.AddUninitialized(TexWidth * TexHeight);

			FlushRenderingCommands();
			// NOTE: You are normally not allowed to dereference this pointer on the game thread! Normally you can only pass the pointer around and
			//  check for NULLness.  We do it in this context, however, and it is only ok because this does not happen every frame and we make sure to flush the
			//  rendering thread.
			FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
			check(RenderTargetResource != nullptr);
			RenderTargetResource->ReadPixels(TexturePixels);
			
			{
				FScopedTransaction Transaction(LOCTEXT("MeshPaintMode_TexturePaint_Transaction", "Texture Paint"));

				// For undo
				TextureData->PaintingTexture2D->SetFlags(RF_Transactional);
				TextureData->PaintingTexture2D->Modify();

				// Store source art
				FColor* Colors = (FColor*)TextureData->PaintingTexture2D->Source.LockMip(0);
				check(TextureData->PaintingTexture2D->Source.CalcMipSize(0) == TexturePixels.Num() * sizeof(FColor));
				FMemory::Memcpy(Colors, TexturePixels.GetData(), TexturePixels.Num() * sizeof(FColor));
				TextureData->PaintingTexture2D->Source.UnlockMip(0);

				// If render target gamma used was 1.0 then disable SRGB for the static texture
				TextureData->PaintingTexture2D->SRGB = FMath::Abs(RenderTargetResource->GetDisplayGamma() - 1.0f) >= KINDA_SMALL_NUMBER;

				TextureData->PaintingTexture2D->bHasBeenPaintedInEditor = true;
			}
		}

		PaintingTexture2D = nullptr;
		TexturePaintingCurrentMeshComponent = nullptr;
	}
}

FPaintTexture2DData* FPaintModePainter::GetPaintTargetData(const UTexture2D* InTexture)
{
	checkf(InTexture != nullptr, TEXT("Invalid Texture ptr"));
	/** Retrieve target paint data for the given texture */
	FPaintTexture2DData* TextureData = PaintTargetData.Find(InTexture);
	return TextureData;
}

FPaintTexture2DData* FPaintModePainter::AddPaintTargetData(UTexture2D* InTexture)
{
	checkf(InTexture != nullptr, TEXT("Invalid Texture ptr"));

	/** Only create new target if we haven't gotten one already  */
	FPaintTexture2DData* TextureData = GetPaintTargetData(InTexture);
	if (TextureData == nullptr)
	{
		// If we didn't find data associated with this texture we create a new entry and return a reference to it.
		//   Note: This reference is only valid until the next change to any key in the map.
		TextureData = &PaintTargetData.Add(InTexture, FPaintTexture2DData(InTexture, false));
	}
	return TextureData;
}

UTexture2D* FPaintModePainter::GetOriginalTextureFromRenderTarget(UTextureRenderTarget2D* InTexture)
{
	checkf(InTexture != nullptr, TEXT("Invalid Texture ptr"));

	UTexture2D* Texture2D = nullptr;

	// We loop through our data set and see if we can find this rendertarget.  If we can, then we add the corresponding UTexture2D to the UI list.
	for (TMap< UTexture2D*, FPaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		FPaintTexture2DData* TextureData = &It.Value();

		if (TextureData->PaintRenderTargetTexture != nullptr &&
			TextureData->PaintRenderTargetTexture == InTexture)
		{
			Texture2D = TextureData->PaintingTexture2D;

			// We found the the matching texture so we can stop searching
			break;
		}
	}

	return Texture2D;
}

void FPaintModePainter::CommitAllPaintedTextures()
{
	if (PaintTargetData.Num() > 0)
	{
		check(PaintingTexture2D == nullptr);

		FScopedTransaction Transaction(LOCTEXT("MeshPaintMode_TexturePaint_Transaction", "Texture Paint"));

		GWarn->BeginSlowTask(LOCTEXT("BeginMeshPaintMode_TexturePaint_CommitTask", "Committing Texture Paint Changes"), true);

		int32 CurStep = 1;
		int32 TotalSteps = GetNumberOfPendingPaintChanges();

		for (TMap< UTexture2D*, FPaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
		{
			FPaintTexture2DData* TextureData = &It.Value();

			// Commit the texture
			if (TextureData->bIsPaintingTexture2DModified == true)
			{
				GWarn->StatusUpdate(CurStep++, TotalSteps, FText::Format(LOCTEXT("MeshPaintMode_TexturePaint_CommitStatus", "Committing Texture Paint Changes: {0}"), FText::FromName(TextureData->PaintingTexture2D->GetFName())));

				const int32 TexWidth = TextureData->PaintRenderTargetTexture->SizeX;
				const int32 TexHeight = TextureData->PaintRenderTargetTexture->SizeY;
				TArray< FColor > TexturePixels;
				TexturePixels.AddUninitialized(TexWidth * TexHeight);

				// Copy the contents of the remote texture to system memory
				// NOTE: OutRawImageData must be a preallocated buffer!

				FlushRenderingCommands();
				// NOTE: You are normally not allowed to dereference this pointer on the game thread! Normally you can only pass the pointer around and
				//  check for NULLness.  We do it in this context, however, and it is only ok because this does not happen every frame and we make sure to flush the
				//  rendering thread.
				FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
				check(RenderTargetResource != nullptr);
				RenderTargetResource->ReadPixels(TexturePixels);

				{
					// For undo
					TextureData->PaintingTexture2D->SetFlags(RF_Transactional);
					TextureData->PaintingTexture2D->Modify();

					// Store source art
					FColor* Colors = (FColor*)TextureData->PaintingTexture2D->Source.LockMip(0);
					check(TextureData->PaintingTexture2D->Source.CalcMipSize(0) == TexturePixels.Num() * sizeof(FColor));
					FMemory::Memcpy(Colors, TexturePixels.GetData(), TexturePixels.Num() * sizeof(FColor));
					TextureData->PaintingTexture2D->Source.UnlockMip(0);

					// If render target gamma used was 1.0 then disable SRGB for the static texture
					// @todo MeshPaint: We are not allowed to dereference the RenderTargetResource pointer, figure out why we need this when the GetDisplayGamma() function is hard coded to return 2.2.
					TextureData->PaintingTexture2D->SRGB = FMath::Abs(RenderTargetResource->GetDisplayGamma() - 1.0f) >= KINDA_SMALL_NUMBER;

					TextureData->PaintingTexture2D->bHasBeenPaintedInEditor = true;

					// Update the texture (generate mips, compress if needed)
					TextureData->PaintingTexture2D->PostEditChange();

					TextureData->bIsPaintingTexture2DModified = false;

					// Reduplicate the duplicate so that if we cancel our future changes, it will restore to how the texture looked at this point.
					TextureData->PaintingTexture2DDuplicate = (UTexture2D*)StaticDuplicateObject(TextureData->PaintingTexture2D, GetTransientPackage(), *FString::Printf(TEXT("%s_TEMP"), *TextureData->PaintingTexture2D->GetName()));

				}
			}
		}

		ClearAllTextureOverrides();

		GWarn->EndSlowTask();
	}
}

void FPaintModePainter::ClearAllTextureOverrides()
{
	/** Remove all texture overrides which are currently stored and active */
	for (TMap< UTexture2D*, FPaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		FPaintTexture2DData* TextureData = &It.Value();

		for (int32 MaterialIndex = 0; MaterialIndex < TextureData->PaintingMaterials.Num(); MaterialIndex++)
		{
			UMaterialInterface* PaintingMaterialInterface = TextureData->PaintingMaterials[MaterialIndex];
			PaintingMaterialInterface->OverrideTexture(TextureData->PaintingTexture2D, nullptr, GEditor->GetEditorWorldContext().World()->FeatureLevel);//findme
		}

		TextureData->PaintingMaterials.Empty();
	}
}

void FPaintModePainter::SetAllTextureOverrides(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		TArray<UTexture*> UsedTextures;
		InMeshComponent->GetUsedTextures(/*out*/ UsedTextures, EMaterialQualityLevel::High);

		for (UTexture* Texture : UsedTextures)
		{
			if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
			{
				if (FPaintTexture2DData* TextureData = GetPaintTargetData(Texture2D))
				{
					GeometryInfo.ApplyOrRemoveTextureOverride(Texture2D, TextureData->PaintRenderTargetTexture);
				}
			}
		}
	}
}

void FPaintModePainter::SetSpecificTextureOverrideForMesh(const IMeshPaintGeometryAdapter& GeometryInfo, UTexture2D* Texture)
{
	// If there is texture data, that means we have an override ready, so set it. 
	// If there is no data, then remove the override so we can at least see the texture without the changes to the other texture.
	// This is important because overrides are shared between material instances with the same parent. We want to disable a override in place,
	// making the action more comprehensive to the user.
	FPaintTexture2DData* TextureData = GetPaintTargetData(Texture);
	UTextureRenderTarget2D* TextureForOverrideOrNull = ((TextureData != nullptr) && (TextureData->PaintingMaterials.Num() > 0)) ? TextureData->PaintRenderTargetTexture : nullptr;

	GeometryInfo.ApplyOrRemoveTextureOverride(Texture, TextureForOverrideOrNull);
}

void FPaintModePainter::RestoreRenderTargets()
{
	bDoRestoreRenTargets = true;
}

int32 FPaintModePainter::GetNumberOfPendingPaintChanges() const
{
	int32 Result = 0;
	for (TMap< UTexture2D*, FPaintTexture2DData >::TConstIterator It(PaintTargetData); It; ++It)
	{
		const FPaintTexture2DData* TextureData = &It.Value();

		// Commit the texture
		if (TextureData->bIsPaintingTexture2DModified == true)
		{
			Result++;
		}
	}
	return Result;
}

void FPaintModePainter::ApplyForcedLODIndex(int32 ForcedLODIndex)
{	
	for (UMeshComponent* SelectedComponent : PaintableComponents)
	{
		MeshPaintHelpers::ForceRenderMeshLOD(SelectedComponent, ForcedLODIndex);
	}
}

void FPaintModePainter::UpdatePaintTargets(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	AActor* Actor = Cast<AActor>(InObject);
	if (InPropertyChangedEvent.Property && 
		InPropertyChangedEvent.Property->GetName() == GET_MEMBER_NAME_CHECKED(USceneComponent, bVisible).ToString())
	{
		Refresh();
	}
}

void FPaintModePainter::FillWithVertexColor()
{
	FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionFillInstColors", "Filling Per-Instance Vertex Colors"));
	const TArray<UMeshComponent*> MeshComponents = GetSelectedComponents<UMeshComponent>();

	static const bool bConvertSRGB = false;
	FColor FillColor = PaintSettings->VertexPaintSettings.PaintColor.ToFColor(bConvertSRGB);

	if (PaintSettings->VertexPaintSettings.MeshPaintMode == EMeshPaintMode::PaintWeights)
	{
		FillColor = MeshPaintHelpers::GenerateColorForTextureWeight((int32)PaintSettings->VertexPaintSettings.TextureWeightType, (int32)PaintSettings->VertexPaintSettings.PaintTextureWeightIndex).ToFColor(bConvertSRGB);
	}

	TUniquePtr< FComponentReregisterContext > ComponentReregisterContext;
	/** Fill each mesh component with the given vertex color */
	for (UMeshComponent* Component : MeshComponents)
	{
		checkf(Component != nullptr, TEXT("Invalid Mesh Component"));
		Component->Modify();
		ComponentReregisterContext = MakeUnique<FComponentReregisterContext>(Component);
		MeshPaintHelpers::FillVertexColors(Component, FillColor, true);
	}
}

void FPaintModePainter::PropagateVertexColorsToAsset()
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	FSuppressableWarningDialog::FSetupInfo SetupInfo(LOCTEXT("PushInstanceVertexColorsPrompt_Message", "Copying the instance vertex colors to the source mesh will replace any of the source mesh's pre-existing vertex colors and affect every instance of the source mesh."),
		LOCTEXT("PushInstanceVertexColorsPrompt_Title", "Warning: Copying vertex data overwrites all instances"), "Warning_PushInstanceVertexColorsPrompt");

	SetupInfo.ConfirmText = LOCTEXT("PushInstanceVertexColorsPrompt_ConfirmText", "Continue");
	SetupInfo.CancelText = LOCTEXT("PushInstanceVertexColorsPrompt_CancelText", "Abort");
	SetupInfo.CheckBoxText = LOCTEXT("PushInstanceVertexColorsPrompt_CheckBoxText", "Always copy vertex colors without prompting");

	FSuppressableWarningDialog VertexColorCopyWarning(SetupInfo);

	// Prompt the user to see if they really want to push the vert colors to the source mesh and to explain
	// the ramifications of doing so. This uses a suppressible dialog so that the user has the choice to always ignore the warning.
	if (VertexColorCopyWarning.ShowModal() != FSuppressableWarningDialog::Cancel)
	{
		FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionPropogateColors", "Propagating Vertex Colors To Source Meshes"));
		bool SomePaintWasPropagated = false;
		TUniquePtr< FComponentReregisterContext > ComponentReregisterContext;
		for (UStaticMeshComponent* Component : StaticMeshComponents)
		{
			checkf(Component != nullptr, TEXT("Invalid Static Mesh Component"));
			UStaticMesh* Mesh = Component->GetStaticMesh();
			for (int32 LODIndex = 0; LODIndex < Mesh->RenderData->LODResources.Num(); LODIndex++)
			{
				FStaticMeshComponentLODInfo& InstanceMeshLODInfo = Component->LODData[LODIndex];
				if (InstanceMeshLODInfo.OverrideVertexColors)
				{
					Mesh->Modify();
					// Try using the mapping generated when building the mesh.
					if (MeshPaintHelpers::PropagateColorsToRawMesh(Mesh, LODIndex, InstanceMeshLODInfo))
					{
						SomePaintWasPropagated = true;
					}
				}
			}

			if (SomePaintWasPropagated)
			{
				ComponentReregisterContext = MakeUnique<FComponentReregisterContext>(Component);
				MeshPaintHelpers::RemoveComponentInstanceVertexColors(Component);
				Mesh->Build();
			}
		}
	}
}

void FPaintModePainter::ImportVertexColors()
{
	const TArray<UMeshComponent*> MeshComponents = GetSelectedComponents<UMeshComponent>();
	if (MeshComponents.Num() == 1)
	{
		/** Import vertex color to single selected mesh component */
		FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionImportColors", "Importing Vertex Colors From Texture"));
		MeshPaintHelpers::ImportVertexColorsFromTexture(MeshComponents[0]);
	}
}

void FPaintModePainter::SavePaintedAssets()
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	const TArray<USkeletalMeshComponent*> SkeletalMeshComponents = GetSelectedComponents<USkeletalMeshComponent>();

	/** Try and save outstanding dirty packages for currently selected mesh components */
	TArray<UObject*> ObjectsToSave;
	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
		{
			ObjectsToSave.Add(StaticMeshComponent->GetStaticMesh());
		}
	}

	for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh)
		{
			ObjectsToSave.Add(SkeletalMeshComponent->SkeletalMesh);
		}
	}

	if (ObjectsToSave.Num() > 0)
	{
		PackageTools::SavePackagesForObjects(ObjectsToSave);
	}
}

void FPaintModePainter::SaveModifiedTextures()
{
	UTexture2D* SelectedTexture = PaintSettings->TexturePaintSettings.PaintTexture;

	if (nullptr != SelectedTexture)
	{
		TArray<UObject*> TexturesToSaveArray;
		TexturesToSaveArray.Add(SelectedTexture);
		PackageTools::SavePackagesForObjects(TexturesToSaveArray);
	}
}

bool FPaintModePainter::CanSaveModifiedTextures() const
{
	/** Check whether or not the current selected paint texture requires saving */
	bool bRequiresSaving = false;
	const UTexture2D* SelectedTexture = PaintSettings->TexturePaintSettings.PaintTexture;
	if (nullptr != SelectedTexture)
	{
		bRequiresSaving = SelectedTexture->GetOutermost()->IsDirty();
	}
	return bRequiresSaving;
}

void FPaintModePainter::Refresh()
{
	// Ensure that we call OnRemoved while adapter/components are still valid
	PaintableComponents.Empty();
	for (auto MeshAdapterPair : ComponentToAdapterMap)
	{
		MeshAdapterPair.Value->OnRemoved();
	}
	ComponentToAdapterMap.Empty();

	bRefreshCachedData = true;
}

void FPaintModePainter::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	IMeshPainter::Tick(ViewportClient, DeltaTime);
	
	if (bRefreshCachedData)
	{
		bRefreshCachedData = false;
		CacheSelectionData();
		CacheTexturePaintData();
		
		bDoRestoreRenTargets = true;
	}

	// Will set the texture override up for the selected texture, important for the drop down combo-list and selecting between material instances.
	if (PaintSettings->PaintMode == EPaintMode::Textures && PaintableComponents.Num() == 1 && PaintSettings->TexturePaintSettings.PaintTexture)
	{
		for (UMeshComponent* MeshComponent : PaintableComponents)
		{
			TSharedPtr<IMeshPaintGeometryAdapter>* MeshAdapter = ComponentToAdapterMap.Find(MeshComponent);
			if (MeshAdapter)
			{
				SetSpecificTextureOverrideForMesh(*MeshAdapter->Get(), PaintSettings->TexturePaintSettings.PaintTexture);
			}
		}
	}

	if (bDoRestoreRenTargets && PaintSettings->PaintMode == EPaintMode::Textures)
	{
		if (PaintingTexture2D == nullptr)
		{
			for (TMap< UTexture2D*, FPaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
			{
				FPaintTexture2DData* TextureData = &It.Value();
				if (TextureData->PaintRenderTargetTexture != nullptr)
				{

					bool bIsSourceTextureStreamedIn = TextureData->PaintingTexture2D->IsFullyStreamedIn();

					if (!bIsSourceTextureStreamedIn)
					{
						//   Make sure it is fully streamed in before we try to do anything with it.
						TextureData->PaintingTexture2D->SetForceMipLevelsToBeResident(30.0f);
						TextureData->PaintingTexture2D->WaitForStreaming();
					}

					//Use the duplicate texture here because as we modify the texture and do undo's, it will be different over the original.
					TexturePaintHelpers::SetupInitialRenderTargetData(TextureData->PaintingTexture2D, TextureData->PaintRenderTargetTexture);

				}
			}
		}
		// We attempted a restore of the rendertargets so go ahead and clear the flag
		bDoRestoreRenTargets = false;
	}
}


FInstanceTexturePaintSettings& FPaintModePainter::AddOrRetrieveInstanceTexturePaintSettings(UMeshComponent* Component)
{
	return ComponentToTexturePaintSettingsMap.FindOrAdd(Component);
}

void FPaintModePainter::CacheSelectionData()
{
	ensure(ComponentToAdapterMap.Num() == 0 && PaintableComponents.Num() == 0);
	const TArray<UMeshComponent*> SelectedMeshComponents = GetSelectedComponents<UMeshComponent>();

	// Update (cached) Paint LOD level if necessary
	PaintSettings->VertexPaintSettings.LODIndex = FMath::Min<int32>(PaintSettings->VertexPaintSettings.LODIndex, GetMaxLODIndexToPaint());
	CachedLODIndex = PaintSettings->VertexPaintSettings.LODIndex;

	// Determine LOD level to use for painting (can only paint on LODs in vertex mode)
	const int32 PaintLODIndex = (PaintSettings->PaintMode == EPaintMode::Vertices && PaintSettings->VertexPaintSettings.bPaintOnSpecificLOD) ? PaintSettings->VertexPaintSettings.LODIndex : 0;
	// Determine UV channel to use while painting textures
	const int32 UVChannel = PaintSettings->PaintMode == EPaintMode::Textures ? PaintSettings->TexturePaintSettings.UVChannel : 0;

	bSelectionContainsPerLODColors = false;

	TUniquePtr< FComponentReregisterContext > ComponentReregisterContext;
	for (UMeshComponent* MeshComponent : SelectedMeshComponents)
	{
		TSharedPtr<IMeshPaintGeometryAdapter> MeshAdapter = FMeshPaintAdapterFactory::CreateAdapterForMesh(MeshComponent, PaintLODIndex);
		if (MeshComponent->IsVisible() && MeshAdapter.IsValid() && MeshAdapter->IsValid())
		{
			PaintableComponents.Add(MeshComponent);
			ComponentToAdapterMap.Add(MeshComponent, MeshAdapter);
			MeshAdapter->OnAdded();
			MeshPaintHelpers::ForceRenderMeshLOD(MeshComponent, PaintLODIndex);
			ComponentReregisterContext.Reset(new FComponentReregisterContext(MeshComponent));
			bSelectionContainsPerLODColors |= MeshPaintHelpers::DoesMeshComponentContainPerLODColors(MeshComponent);
		}
	}
}

void FPaintModePainter::CacheTexturePaintData()
{
	TArray<UMeshComponent*> SelectedMeshComponents = GetSelectedComponents<UMeshComponent>();

	PaintableTextures.Empty();
	if (PaintableComponents.Num() == 1)
	{
		const UMeshComponent* Component = PaintableComponents[0];
		TSharedPtr<IMeshPaintGeometryAdapter> Adapter = ComponentToAdapterMap.FindChecked(Component);
		TexturePaintHelpers::RetrieveTexturesForComponent(Component, Adapter.Get(), PaintableTextures);
	}

	// Ensure that the selection remains valid or is invalidated
	if (!PaintableTextures.Contains(PaintSettings->TexturePaintSettings.PaintTexture))
	{
		UTexture2D* NewTexture = nullptr;
		if (PaintableTextures.Num() > 0)
		{
			NewTexture = Cast<UTexture2D>(PaintableTextures[0].Texture);
		}
		PaintSettings->TexturePaintSettings.PaintTexture = NewTexture;
	}
}

void FPaintModePainter::ResetPaintingState()
{
	bArePainting = false;
	TimeSinceStartedPainting = 0.0f;
	PaintableComponents.Empty();
}

template<typename ComponentClass>
TArray<ComponentClass*> FPaintModePainter::GetSelectedComponents() const
{
	TArray<ComponentClass*> Components;

	if (PaintSettings->PaintMode == EPaintMode::Textures)
	{
		USelection* ComponentSelection = GEditor->GetSelectedComponents();
		for (int32 SelectionIndex = 0; SelectionIndex < ComponentSelection->Num(); ++SelectionIndex)
		{
			ComponentClass* SelectedComponent = Cast<ComponentClass>(ComponentSelection->GetSelectedObject(SelectionIndex));
			if (SelectedComponent)
			{
				Components.AddUnique(SelectedComponent);
			}
		}
	}

	if (Components.Num() == 0)
	{
		USelection* ActorSelection = GEditor->GetSelectedActors();
		for (int32 SelectionIndex = 0; SelectionIndex < ActorSelection->Num(); ++SelectionIndex)
		{
			AActor* SelectedActor = Cast<AActor>(ActorSelection->GetSelectedObject(SelectionIndex));
			if (SelectedActor)
			{
				TArray<UActorComponent*> ActorComponents = SelectedActor->GetComponentsByClass(ComponentClass::StaticClass());
				for (UActorComponent* Component : ActorComponents)
				{
					Components.AddUnique(CastChecked<ComponentClass>(Component));
				}
			}
		}
	}

	return Components;
}

template TArray<UStaticMeshComponent*> FPaintModePainter::GetSelectedComponents<UStaticMeshComponent>() const;
template TArray<USkeletalMeshComponent*> FPaintModePainter::GetSelectedComponents<USkeletalMeshComponent>() const;
template TArray<UMeshComponent*> FPaintModePainter::GetSelectedComponents<UMeshComponent>() const;

#undef LOCTEXT_NAMESPACE // "PaintModePainter"
