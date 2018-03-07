// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ClothPainter.h"
#include "MeshPaintSettings.h"
#include "MeshPaintModule.h"
#include "MeshPaintAdapterFactory.h"
#include "MeshPaintHelpers.h"

#include "Assets/ClothingAsset.h"
#include "Classes/Animation/DebugSkelMeshComponent.h"
#include "ClothPaintSettings.h"
#include "ClothMeshAdapter.h"
#include "SClothPaintWidget.h"

#include "EditorViewportClient.h"
#include "ComponentReregisterContext.h"
#include "Package.h"
#include "ClothPaintTools.h"
#include "UICommandList.h"
#include "SlateApplication.h"

#define LOCTEXT_NAMESPACE "ClothPainter"

FClothPainter::FClothPainter()
	: IMeshPainter()
	, SkeletalMeshComponent(nullptr)
	, PaintSettings(nullptr)
	, BrushSettings(nullptr)
{
	VertexPointSize = 3.0f;
	VertexPointColor = FLinearColor::White;
	WidgetLineThickness = .5f;
	bShouldSimulate = false;
	bShowHiddenVerts = false;
}

FClothPainter::~FClothPainter()
{
	SkeletalMeshComponent->ToggleMeshSectionForCloth(SkeletalMeshComponent->SelectedClothingGuidForPainting);

	// Cancel rendering of the paint proxy
	SkeletalMeshComponent->SelectedClothingGuidForPainting = FGuid();
}

void FClothPainter::Init()
{
	BrushSettings = DuplicateObject<UPaintBrushSettings>(GetMutableDefault<UPaintBrushSettings>(), GetTransientPackage());	
	BrushSettings->AddToRoot();
	BrushSettings->bOnlyFrontFacingTriangles = false;
	PaintSettings = DuplicateObject<UClothPainterSettings>(GetMutableDefault<UClothPainterSettings>(), GetTransientPackage());
	PaintSettings->AddToRoot();

	CommandList = MakeShareable(new FUICommandList);

	Tools.Add(MakeShared<FClothPaintTool_Brush>(AsShared()));
	Tools.Add(MakeShared<FClothPaintTool_Gradient>(AsShared()));
	Tools.Add(MakeShared<FClothPaintTool_Smooth>(AsShared()));
	Tools.Add(MakeShared<FClothPaintTool_Fill>(AsShared()));

	SelectedTool = Tools[0];
	SelectedTool->Activate(CommandList);

	Widget = SNew(SClothPaintWidget, this);
}

bool FClothPainter::PaintInternal(const FVector& InCameraOrigin, const FVector& InRayOrigin, const FVector& InRayDirection, EMeshPaintAction PaintAction, float PaintStrength)
{
	bool bApplied = false;

	if(SkeletalMeshComponent->SelectedClothingGuidForPainting.IsValid() && !bShouldSimulate)
	{
		USkeletalMesh* SkelMesh = SkeletalMeshComponent->SkeletalMesh;

		const FHitResult& HitResult = GetHitResult(InRayOrigin, InRayDirection);

		if (HitResult.bBlockingHit)
		{
			// Generic per-vertex painting operations
			if(!IsPainting())
				{
					BeginTransaction(LOCTEXT("MeshPaint", "Painting Cloth Property Values"));
					bArePainting = true;
					Adapter->PreEdit();
				}

				const FMeshPaintParameters Parameters = CreatePaintParameters(HitResult, InCameraOrigin, InRayOrigin, InRayDirection, PaintStrength);

			FPerVertexPaintActionArgs Args;
			Args.Adapter = Adapter.Get();
			Args.CameraPosition = InCameraOrigin;
			Args.HitResult = HitResult;
			Args.BrushSettings = GetBrushSettings();
			Args.Action = PaintAction;

			if(SelectedTool->IsPerVertex())
					{
				bApplied = MeshPaintHelpers::ApplyPerVertexPaintAction(Args, GetPaintAction(Parameters));
					}
			else
					{
				bApplied = true;
				GetPaintAction(Parameters).ExecuteIfBound(Args, INDEX_NONE);
			}
		}
	}

	return bApplied;
}

FPerVertexPaintAction FClothPainter::GetPaintAction(const FMeshPaintParameters& InPaintParams)
{
	if(SelectedTool.IsValid())
	{
		return SelectedTool->GetPaintAction(InPaintParams, PaintSettings);
	}

	return FPerVertexPaintAction();
}

void FClothPainter::SetTool(TSharedPtr<FClothPaintToolBase> InTool)
{
	if(InTool.IsValid() && Tools.Contains(InTool))
	{
		if(SelectedTool.IsValid())
		{
			SelectedTool->Deactivate(CommandList);
		}

		SelectedTool = InTool;
		SelectedTool->Activate(CommandList);
	}
}

void FClothPainter::SetSkeletalMeshComponent(UDebugSkelMeshComponent* InSkeletalMeshComponent)
{
	TSharedPtr<FClothMeshPaintAdapter> Result = MakeShareable(new FClothMeshPaintAdapter());
	Result->Construct(InSkeletalMeshComponent, 0);
	Adapter = Result;

	SkeletalMeshComponent = InSkeletalMeshComponent;

	RefreshClothingAssets();

	if (Widget.IsValid())
	{
		Widget->OnRefresh();
	}
}

USkeletalMesh* FClothPainter::GetSkeletalMesh() const
{
	if(SkeletalMeshComponent)
	{
		return SkeletalMeshComponent->SkeletalMesh;
	}

	return nullptr;
}

void FClothPainter::RefreshClothingAssets()
{
	if(!PaintSettings || !SkeletalMeshComponent)
	{
		return;
	}

	PaintSettings->ClothingAssets.Reset();

	if(USkeletalMesh* Mesh = SkeletalMeshComponent->SkeletalMesh)
	{
		for(UClothingAssetBase* BaseClothingAsset : Mesh->MeshClothingAssets)
		{
			if(UClothingAsset* ActualAsset = Cast<UClothingAsset>(BaseClothingAsset))
			{
				PaintSettings->ClothingAssets.AddUnique(ActualAsset);
			}
		}
	}
}

void FClothPainter::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	IMeshPainter::Tick(ViewportClient, DeltaTime);

	SkeletalMeshComponent->MinClothPropertyView = PaintSettings->GetViewMin();
	SkeletalMeshComponent->MaxClothPropertyView = PaintSettings->GetViewMax();
	SkeletalMeshComponent->bClothFlipNormal = PaintSettings->bFlipNormal;
	SkeletalMeshComponent->bClothCullBackface = PaintSettings->bCullBackface;
	SkeletalMeshComponent->ClothMeshOpacity = PaintSettings->Opacity;

	if ((bShouldSimulate && SkeletalMeshComponent->bDisableClothSimulation) || (!bShouldSimulate && !SkeletalMeshComponent->bDisableClothSimulation))
	{
		if(bShouldSimulate)
		{
			// Need to re-apply our masks here, as they have likely been edited
			for(UClothingAsset* Asset : PaintSettings->ClothingAssets)
			{
				if(Asset)
				{
					Asset->ApplyParameterMasks();
				}
			}

			SkeletalMeshComponent->RebuildClothingSectionsFixedVerts();
		}

		FComponentReregisterContext ReregisterContext(SkeletalMeshComponent);
		SkeletalMeshComponent->ToggleMeshSectionForCloth(SkeletalMeshComponent->SelectedClothingGuidForPainting);
		SkeletalMeshComponent->bDisableClothSimulation = !bShouldSimulate;		
		SkeletalMeshComponent->bShowClothData = !bShouldSimulate;
		ViewportClient->Invalidate();
	}

	
	// We always want up to date CPU skinned verts, so each tick we reinitialize the adapter
	if(Adapter.IsValid())
	{
		Adapter->Initialize();
	}
}

void FClothPainter::FinishPainting()
{
	if (IsPainting())
	{		
		EndTransaction();
		Adapter->PostEdit();

		if(SkeletalMeshComponent)
		{
			FComponentReregisterContext ReregisterContext(SkeletalMeshComponent);

			if(USkeletalMesh* SkelMesh = SkeletalMeshComponent->SkeletalMesh)
			{
				for(UClothingAssetBase* AssetBase : SkelMesh->MeshClothingAssets)
				{
					AssetBase->InvalidateCachedData();
				}
			}
		}
	}

	bArePainting = false;
}

void FClothPainter::Reset()
{	
	if(Widget.IsValid())
	{
		Widget->Reset();
	}

	bArePainting = false;
	SkeletalMeshComponent->ToggleMeshSectionForCloth(SkeletalMeshComponent->SelectedClothingGuidForPainting);
	SkeletalMeshComponent->SelectedClothingGuidForPainting = FGuid();
}

TSharedPtr<IMeshPaintGeometryAdapter> FClothPainter::GetMeshAdapterForComponent(const UMeshComponent* Component)
{
	if (Component == SkeletalMeshComponent)
	{
		return Adapter;
	}

	return nullptr;
}

void FClothPainter::AddReferencedObjects(FReferenceCollector& Collector)
{	
	Collector.AddReferencedObject(SkeletalMeshComponent);
	Collector.AddReferencedObject(BrushSettings);
	Collector.AddReferencedObject(PaintSettings);
}

UPaintBrushSettings* FClothPainter::GetBrushSettings()
{
	return BrushSettings;
}

UMeshPaintSettings* FClothPainter::GetPainterSettings()
{
	return PaintSettings;
}

TSharedPtr<class SWidget> FClothPainter::GetWidget()
{
	return Widget;
}

const FHitResult FClothPainter::GetHitResult(const FVector& Origin, const FVector& Direction)
{
	FHitResult HitResult(1.0f);
	const FVector TraceStart(Origin);
	const FVector TraceEnd(Origin + Direction * HALF_WORLD_MAX);

	if (Adapter.IsValid())
	{
		Adapter->LineTraceComponent(HitResult, TraceStart, TraceEnd, FCollisionQueryParams(SCENE_QUERY_STAT(FClothPainter_GetHitResult), true));
	}
	
	return HitResult;
}

void FClothPainter::Refresh()
{
	RefreshClothingAssets();
	if(Widget.IsValid())
	{
		Widget->OnRefresh();
	}
}

void FClothPainter::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if(SelectedTool.IsValid() && SelectedTool->ShouldRenderInteractors() && !bShouldSimulate)
	{
		RenderInteractors(View, Viewport, PDI, true, SDPG_Foreground);
	}

	const ESceneDepthPriorityGroup DepthPriority = bShowHiddenVerts ? SDPG_Foreground : SDPG_World;
	// Render simulation mesh vertices if not simulating
	if(SkeletalMeshComponent)
	{
		if(!bShouldSimulate)
		{
			if(SelectedTool.IsValid())
			{
				SelectedTool->Render(SkeletalMeshComponent, Adapter.Get(), View, Viewport, PDI);
			}
		}
	}
	
	bShouldSimulate = Viewport->KeyState(EKeys::H);
	bShowHiddenVerts = Viewport->KeyState(EKeys::J);
}

bool FClothPainter::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	bool bHandled = IMeshPainter::InputKey(InViewportClient, InViewport, InKey, InEvent);

	if(SelectedTool.IsValid())
	{
		if(CommandList->ProcessCommandBindings(InKey, FSlateApplication::Get().GetModifierKeys(), InEvent == IE_Repeat))
		{
			bHandled = true;
		}
		else
		{
			// Handle non-action based key actions (holds etc.)
			bHandled |= SelectedTool->InputKey(Adapter.Get(), InViewportClient, InViewport, InKey, InEvent);
		}
	}

	return bHandled;
}

FMeshPaintParameters FClothPainter::CreatePaintParameters(const FHitResult& HitResult, const FVector& InCameraOrigin, const FVector& InRayOrigin, const FVector& InRayDirection, float PaintStrength)
{
	const float BrushStrength = BrushSettings->BrushStrength *  BrushSettings->BrushStrength * PaintStrength;

	const float BrushRadius = BrushSettings->GetBrushRadius();
	const float BrushDepth = BrushRadius * .5f;

	FVector BrushXAxis, BrushYAxis;
	HitResult.Normal.FindBestAxisVectors(BrushXAxis, BrushYAxis);
	// Display settings
	const float VisualBiasDistance = 0.15f;
	const FVector BrushVisualPosition = HitResult.Location + HitResult.Normal * VisualBiasDistance;

	FMeshPaintParameters Params;
	{
		Params.BrushPosition = HitResult.Location;
		
		Params.SquaredBrushRadius = BrushRadius * BrushRadius;
		Params.BrushRadialFalloffRange = BrushSettings->BrushFalloffAmount * BrushRadius;
		Params.InnerBrushRadius = BrushRadius - Params.BrushRadialFalloffRange;
		Params.BrushDepth = BrushDepth;
		Params.BrushDepthFalloffRange = BrushSettings->BrushFalloffAmount * BrushDepth;
		Params.InnerBrushDepth = BrushDepth - Params.BrushDepthFalloffRange;
		Params.BrushStrength = BrushStrength;
		Params.BrushNormal = HitResult.Normal;
		Params.BrushToWorldMatrix = FMatrix(BrushXAxis, BrushYAxis, Params.BrushNormal, Params.BrushPosition);
		Params.InverseBrushToWorldMatrix = Params.BrushToWorldMatrix.InverseFast();
	}

	return Params;
}

float FClothPainter::GetPropertyValue(int32 VertexIndex)
{
	FClothMeshPaintAdapter* ClothAdapter = (FClothMeshPaintAdapter*)Adapter.Get();

	if(FClothParameterMask_PhysMesh* Mask = ClothAdapter->GetCurrentMask())
	{
		return Mask->GetValue(VertexIndex);
	}

	return 0.0f;
}

void FClothPainter::SetPropertyValue(int32 VertexIndex, const float Value)
{
	FClothMeshPaintAdapter* ClothAdapter = (FClothMeshPaintAdapter*)Adapter.Get();

	if(FClothParameterMask_PhysMesh* Mask = ClothAdapter->GetCurrentMask())
	{
		Mask->SetValue(VertexIndex, Value);
	}
}

void FClothPainter::OnAssetSelectionChanged(UClothingAsset* InNewSelectedAsset, int32 InAssetLod, int32 InMaskIndex)
{
	TSharedPtr<FClothMeshPaintAdapter> ClothAdapter = StaticCastSharedPtr<FClothMeshPaintAdapter>(Adapter);
	if(ClothAdapter.IsValid() && InNewSelectedAsset && InNewSelectedAsset->IsValidLod(InAssetLod))
	{
		// Validate the incoming parameters, to make sure we only set a selection if we're going
		// to get a valid paintable surface
		if(InNewSelectedAsset->LodData.IsValidIndex(InAssetLod) &&
			 InNewSelectedAsset->LodData[InAssetLod].ParameterMasks.IsValidIndex(InMaskIndex))
		{
			const FGuid NewGuid = InNewSelectedAsset->GetAssetGuid();
			SkeletalMeshComponent->ToggleMeshSectionForCloth(SkeletalMeshComponent->SelectedClothingGuidForPainting);
			SkeletalMeshComponent->ToggleMeshSectionForCloth(NewGuid);

			SkeletalMeshComponent->bDisableClothSimulation = true;
			SkeletalMeshComponent->bShowClothData = true;
			SkeletalMeshComponent->SelectedClothingGuidForPainting = NewGuid;
			SkeletalMeshComponent->SelectedClothingLodForPainting = InAssetLod;
			SkeletalMeshComponent->SelectedClothingLodMaskForPainting = InMaskIndex;
			SkeletalMeshComponent->RefreshSelectedClothingSkinnedPositions();

			ClothAdapter->SetSelectedClothingAsset(NewGuid, InAssetLod, InMaskIndex);

		}
	}
}

#undef LOCTEXT_NAMESPACE // "ClothPainter"
