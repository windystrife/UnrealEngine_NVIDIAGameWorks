// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintHelpers.h"

#include "ComponentReregisterContext.h"

#include "MeshPaintTypes.h"
#include "MeshPaintSettings.h"
#include "IMeshPaintGeometryAdapter.h"
#include "MeshPaintAdapterFactory.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "StaticMeshResources.h"
#include "RawMesh.h"

#include "GenericOctree.h"
#include "Utils.h"

#include "SlateApplication.h"
#include "SImportVertexColorOptions.h"
#include "EditorViewportClient.h"
#include "Interfaces/IMainFrameModule.h"

#include "Modules/ModuleManager.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "PackageTools.h"
#include "FileHelpers.h"
#include "ISourceControlModule.h"

#include "Editor.h"
#include "LevelEditor.h"
#include "ILevelViewport.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"

#include "VREditorMode.h"
#include "IVREditorModule.h"
#include "ViewportWorldInteraction.h"
#include "ViewportInteractableInterface.h"
#include "VREditorInteractor.h"
#include "EditorWorldExtension.h"

#include "ParallelFor.h"

void MeshPaintHelpers::RemoveInstanceVertexColors(UObject* Obj)
{
	// Currently only static mesh component supports per instance vertex colors so only need to retrieve those and remove colors
	AActor* Actor = Cast<AActor>(Obj);
	if (Actor != nullptr)
	{
		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
		for (const auto& StaticMeshComponent : StaticMeshComponents)
		{
			if (StaticMeshComponent != nullptr)
			{
				MeshPaintHelpers::RemoveComponentInstanceVertexColors(StaticMeshComponent);
			}
		}
	}
}

void MeshPaintHelpers::RemoveComponentInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent)
{
	if (StaticMeshComponent != nullptr && StaticMeshComponent->GetStaticMesh() != nullptr)
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

			StaticMeshComponent->RemoveInstanceVertexColors();
		}
		else
		{
			StaticMeshComponent->RemoveInstanceVertexColors();
		}
	}
}


bool MeshPaintHelpers::PropagateColorsToRawMesh(UStaticMesh* StaticMesh, int32 LODIndex, FStaticMeshComponentLODInfo& ComponentLODInfo)
{
	check(ComponentLODInfo.OverrideVertexColors);
	check(StaticMesh->SourceModels.IsValidIndex(LODIndex));
	check(StaticMesh->RenderData);
	check(StaticMesh->RenderData->LODResources.IsValidIndex(LODIndex));

	bool bPropagatedColors = false;
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
	FStaticMeshRenderData& RenderData = *StaticMesh->RenderData;
	FStaticMeshLODResources& RenderModel = RenderData.LODResources[LODIndex];
	FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;
	if (RenderData.WedgeMap.Num() > 0 && ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices())
	{
		// Use the wedge map if it is available as it is lossless.
		FRawMesh RawMesh;
		SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

		int32 NumWedges = RawMesh.WedgeIndices.Num();
		if (RenderData.WedgeMap.Num() == NumWedges)
		{
			int32 NumExistingColors = RawMesh.WedgeColors.Num();
			if (NumExistingColors < NumWedges)
			{
				RawMesh.WedgeColors.AddUninitialized(NumWedges - NumExistingColors);
			}
			for (int32 i = 0; i < NumWedges; ++i)
			{
				FColor WedgeColor = FColor::White;
				int32 Index = RenderData.WedgeMap[i];
				if (Index != INDEX_NONE)
				{
					WedgeColor = ColorVertexBuffer.VertexColor(Index);
				}
				RawMesh.WedgeColors[i] = WedgeColor;
			}
			SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
			bPropagatedColors = true;
		}
	}
	else
	{
		// If there's no raw mesh data, don't try to do any fixup here
		if (SrcModel.RawMeshBulkData->IsEmpty() || ComponentLODInfo.OverrideMapBuildData == nullptr)
		{
			return false;
		}

		// Fall back to mapping based on position.
		FRawMesh RawMesh;
		SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

		TArray<FColor> NewVertexColors;
		FPositionVertexBuffer TempPositionVertexBuffer;
		TempPositionVertexBuffer.Init(RawMesh.VertexPositions);
		RemapPaintedVertexColors(
			ComponentLODInfo.PaintedVertices,
			*ComponentLODInfo.OverrideVertexColors,
			RenderModel.PositionVertexBuffer,
			RenderModel.VertexBuffer,
			TempPositionVertexBuffer,
			/*OptionalVertexBuffer=*/ nullptr,
			NewVertexColors
		);
		if (NewVertexColors.Num() == RawMesh.VertexPositions.Num())
		{
			int32 NumWedges = RawMesh.WedgeIndices.Num();
			RawMesh.WedgeColors.Empty(NumWedges);
			RawMesh.WedgeColors.AddZeroed(NumWedges);
			for (int32 i = 0; i < NumWedges; ++i)
			{
				int32 Index = RawMesh.WedgeIndices[i];
				RawMesh.WedgeColors[i] = NewVertexColors[Index];
			}
			SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
			bPropagatedColors = true;
		}
	}
	return bPropagatedColors;
}

bool MeshPaintHelpers::PaintVertex(const FVector& InVertexPosition, const FMeshPaintParameters& InParams, FColor& InOutVertexColor)
{
	float SquaredDistanceToVertex2D;
	float VertexDepthToBrush;
	if (MeshPaintHelpers::IsPointInfluencedByBrush(InVertexPosition, InParams, SquaredDistanceToVertex2D, VertexDepthToBrush))
	{
		// Compute amount of paint to apply
		const float PaintAmount = ComputePaintMultiplier(SquaredDistanceToVertex2D, InParams.BrushStrength, InParams.InnerBrushRadius, InParams.BrushRadialFalloffRange, InParams.BrushDepth, InParams.BrushDepthFalloffRange, VertexDepthToBrush);
			
		const FLinearColor OldColor = InOutVertexColor.ReinterpretAsLinear();
		FLinearColor NewColor = OldColor;

		if (InParams.PaintMode == EMeshPaintMode::PaintColors)
		{
			ApplyVertexColorPaint(InParams, OldColor, NewColor, PaintAmount);

		}
		else if (InParams.PaintMode == EMeshPaintMode::PaintWeights)
		{
			ApplyVertexWeightPaint(InParams, OldColor, PaintAmount, NewColor);
		}

		// Save the new color
		InOutVertexColor.R = FMath::Clamp(FMath::RoundToInt(NewColor.R * 255.0f), 0, 255);
		InOutVertexColor.G = FMath::Clamp(FMath::RoundToInt(NewColor.G * 255.0f), 0, 255);
		InOutVertexColor.B = FMath::Clamp(FMath::RoundToInt(NewColor.B * 255.0f), 0, 255);
		InOutVertexColor.A = FMath::Clamp(FMath::RoundToInt(NewColor.A * 255.0f), 0, 255);

		return true;
	}

	// Out of range
	return false;
}

void MeshPaintHelpers::ApplyVertexColorPaint(const FMeshPaintParameters &InParams, const FLinearColor &OldColor, FLinearColor &NewColor, const float PaintAmount)
{
	// Color painting
	if (InParams.bWriteRed)
	{
		NewColor.R = (OldColor.R < InParams.BrushColor.R) ? FMath::Min(InParams.BrushColor.R, OldColor.R + PaintAmount) : FMath::Max(InParams.BrushColor.R, OldColor.R - PaintAmount);
	}

	if (InParams.bWriteGreen)
	{
		NewColor.G = (OldColor.G < InParams.BrushColor.G) ? FMath::Min(InParams.BrushColor.G, OldColor.G + PaintAmount) : FMath::Max(InParams.BrushColor.G, OldColor.G - PaintAmount);
	}

	if (InParams.bWriteBlue)
	{
		NewColor.B = (OldColor.B < InParams.BrushColor.B) ? FMath::Min(InParams.BrushColor.B, OldColor.B + PaintAmount) : FMath::Max(InParams.BrushColor.B, OldColor.B - PaintAmount);
	}

	if (InParams.bWriteAlpha)
	{
		NewColor.A = (OldColor.A < InParams.BrushColor.A) ? FMath::Min(InParams.BrushColor.A, OldColor.A + PaintAmount) : FMath::Max(InParams.BrushColor.A, OldColor.A - PaintAmount);
	}
}

void MeshPaintHelpers::ApplyVertexWeightPaint(const FMeshPaintParameters &InParams, const FLinearColor &OldColor, const float PaintAmount, FLinearColor &NewColor)
{
	// Total number of texture blend weights we're using
	check(InParams.TotalWeightCount > 0);
	check(InParams.TotalWeightCount <= MeshPaintDefs::MaxSupportedWeights);

	// True if we should assume the last weight index is composed of one minus the sum of all
	// of the other weights.  This effectively allows an additional weight with no extra memory
	// used, but potentially requires extra pixel shader instructions to render.
	//
	// NOTE: If you change the default here, remember to update the MeshPaintWindow UI and strings
	//
	// NOTE: Materials must be authored to match the following assumptions!
	const bool bUsingOneMinusTotal =
		InParams.TotalWeightCount == 2 ||		// Two textures: Use a lerp() in pixel shader (single value)
		InParams.TotalWeightCount == 5;			// Five texture: Requires 1.0-sum( R+G+B+A ) in shader
	check(bUsingOneMinusTotal || InParams.TotalWeightCount <= MeshPaintDefs::MaxSupportedPhysicalWeights);

	// Prefer to use RG/RGB instead of AR/ARG when we're only using 2/3 physical weights
	const int32 TotalPhysicalWeights = bUsingOneMinusTotal ? InParams.TotalWeightCount - 1 : InParams.TotalWeightCount;
	const bool bUseColorAlpha =
		TotalPhysicalWeights != 2 &&			// Two physical weights: Use RG instead of AR
		TotalPhysicalWeights != 3;				// Three physical weights: Use RGB instead of ARG

	// Index of the blend weight that we're painting
	check(InParams.PaintWeightIndex >= 0 && InParams.PaintWeightIndex < MeshPaintDefs::MaxSupportedWeights);

	// Convert the color value to an array of weights
	float Weights[MeshPaintDefs::MaxSupportedWeights];
	{
		for (int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex)
		{
			if (CurWeightIndex == TotalPhysicalWeights)
			{
				// This weight's value is one minus the sum of all previous weights
				float OtherWeightsTotal = 0.0f;
				for (int32 OtherWeightIndex = 0; OtherWeightIndex < CurWeightIndex; ++OtherWeightIndex)
				{
					OtherWeightsTotal += Weights[OtherWeightIndex];
				}
				Weights[CurWeightIndex] = 1.0f - OtherWeightsTotal;
			}
			else
			{
				switch (CurWeightIndex)
				{
				case 0:
					Weights[CurWeightIndex] = bUseColorAlpha ? OldColor.A : OldColor.R;
					break;

				case 1:
					Weights[CurWeightIndex] = bUseColorAlpha ? OldColor.R : OldColor.G;
					break;

				case 2:
					Weights[CurWeightIndex] = bUseColorAlpha ? OldColor.G : OldColor.B;
					break;

				case 3:
					check(bUseColorAlpha);
					Weights[CurWeightIndex] = OldColor.B;
					break;
				}
			}
		}
	}

	// Go ahead any apply paint!	
	Weights[InParams.PaintWeightIndex] += PaintAmount;
	Weights[InParams.PaintWeightIndex] = FMath::Clamp(Weights[InParams.PaintWeightIndex], 0.0f, 1.0f);
	

	// Now renormalize all of the other weights	
	float OtherWeightsTotal = 0.0f;
	for (int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex)
	{
		if (CurWeightIndex != InParams.PaintWeightIndex)
		{
			OtherWeightsTotal += Weights[CurWeightIndex];
		}
	}
	const float NormalizeTarget = 1.0f - Weights[InParams.PaintWeightIndex];
	for (int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex)
	{
		if (CurWeightIndex != InParams.PaintWeightIndex)
		{
			if (OtherWeightsTotal == 0.0f)
			{
				Weights[CurWeightIndex] = NormalizeTarget / (InParams.TotalWeightCount - 1);
			}
			else
			{
				Weights[CurWeightIndex] = Weights[CurWeightIndex] / OtherWeightsTotal * NormalizeTarget;
			}
		}
	}

	// The total of the weights should now always equal 1.0	
	float WeightsTotal = 0.0f;
	for (int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex)
	{
		WeightsTotal += Weights[CurWeightIndex];
	}
	check(FMath::IsNearlyEqual(WeightsTotal, 1.0f, 0.01f));
	
	// Convert the weights back to a color value	
	for (int32 CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex)
	{
		// We can skip the non-physical weights as it's already baked into the others
		if (CurWeightIndex != TotalPhysicalWeights)
		{
			switch (CurWeightIndex)
			{
			case 0:
				if (bUseColorAlpha)
				{
					NewColor.A = Weights[CurWeightIndex];
				}
				else
				{
					NewColor.R = Weights[CurWeightIndex];
				}
				break;

			case 1:
				if (bUseColorAlpha)
				{
					NewColor.R = Weights[CurWeightIndex];
				}
				else
				{
					NewColor.G = Weights[CurWeightIndex];
				}
				break;

			case 2:
				if (bUseColorAlpha)
				{
					NewColor.G = Weights[CurWeightIndex];
				}
				else
				{
					NewColor.B = Weights[CurWeightIndex];
				}
				break;

			case 3:
				NewColor.B = Weights[CurWeightIndex];
				break;
			}
		}
	}	
}

FLinearColor MeshPaintHelpers::GenerateColorForTextureWeight(const int32 NumWeights, const int32 WeightIndex)
{
	const bool bUsingOneMinusTotal =
		NumWeights == 2 ||		// Two textures: Use a lerp() in pixel shader (single value)
		NumWeights == 5;			// Five texture: Requires 1.0-sum( R+G+B+A ) in shader
	check(bUsingOneMinusTotal || NumWeights <= MeshPaintDefs::MaxSupportedPhysicalWeights);

	// Prefer to use RG/RGB instead of AR/ARG when we're only using 2/3 physical weights
	const int32 TotalPhysicalWeights = bUsingOneMinusTotal ? NumWeights - 1 : NumWeights;
	const bool bUseColorAlpha =
		TotalPhysicalWeights != 2 &&			// Two physical weights: Use RG instead of AR
		TotalPhysicalWeights != 3;				// Three physical weights: Use RGB instead of ARG

												// Index of the blend weight that we're painting
	check(WeightIndex >= 0 && WeightIndex < MeshPaintDefs::MaxSupportedWeights);

	// Convert the color value to an array of weights
	float Weights[MeshPaintDefs::MaxSupportedWeights];
	{
		for (int32 CurWeightIndex = 0; CurWeightIndex < NumWeights; ++CurWeightIndex)
		{
			if (CurWeightIndex == TotalPhysicalWeights)
			{
				// This weight's value is one minus the sum of all previous weights
				float OtherWeightsTotal = 0.0f;
				for (int32 OtherWeightIndex = 0; OtherWeightIndex < CurWeightIndex; ++OtherWeightIndex)
				{
					OtherWeightsTotal += Weights[OtherWeightIndex];
				}
				Weights[CurWeightIndex] = 1.0f - OtherWeightsTotal;
			}
			else
			{
				if (CurWeightIndex == WeightIndex)
				{
					Weights[CurWeightIndex] = 1.0f;
				}
				else
				{
					Weights[CurWeightIndex] = 0.0f;
				}
			}
		}
	}

	FLinearColor NewColor(FLinearColor::Black);
	// Convert the weights back to a color value	
	for (int32 CurWeightIndex = 0; CurWeightIndex < NumWeights; ++CurWeightIndex)
	{
		// We can skip the non-physical weights as it's already baked into the others
		if (CurWeightIndex != TotalPhysicalWeights)
		{
			switch (CurWeightIndex)
			{
			case 0:
				if (bUseColorAlpha)
				{
					NewColor.A = Weights[CurWeightIndex];
				}
				else
				{
					NewColor.R = Weights[CurWeightIndex];
				}
				break;

			case 1:
				if (bUseColorAlpha)
				{
					NewColor.R = Weights[CurWeightIndex];
				}
				else
				{
					NewColor.G = Weights[CurWeightIndex];
				}
				break;

			case 2:
				if (bUseColorAlpha)
				{
					NewColor.G = Weights[CurWeightIndex];
				}
				else
				{
					NewColor.B = Weights[CurWeightIndex];
				}
				break;

			case 3:
				NewColor.B = Weights[CurWeightIndex];
				break;
			}
		}
	}

	return NewColor;
}

float MeshPaintHelpers::ComputePaintMultiplier(float SquaredDistanceToVertex2D, float BrushStrength, float BrushInnerRadius, float BrushRadialFalloff, float BrushInnerDepth, float BrushDepthFallof, float VertexDepthToBrush)
{
	float PaintAmount = 1.0f;
	
	// Compute the actual distance
	float DistanceToVertex2D = 0.0f;
	if (SquaredDistanceToVertex2D > KINDA_SMALL_NUMBER)
	{
		DistanceToVertex2D = FMath::Sqrt(SquaredDistanceToVertex2D);
	}

	// Apply radial-based falloff
	if (DistanceToVertex2D > BrushInnerRadius)
	{
		const float RadialBasedFalloff = (DistanceToVertex2D - BrushInnerRadius) / BrushRadialFalloff;
		PaintAmount *= 1.0f - RadialBasedFalloff;
	}	

	// Apply depth-based falloff	
	if (VertexDepthToBrush > BrushInnerDepth)
	{
		const float DepthBasedFalloff = (VertexDepthToBrush - BrushInnerDepth) / BrushDepthFallof;
		PaintAmount *= 1.0f - DepthBasedFalloff;
	}

	PaintAmount *= BrushStrength;

	return PaintAmount;
}

bool MeshPaintHelpers::IsPointInfluencedByBrush(const FVector& InPosition, const FMeshPaintParameters& InParams, float& OutSquaredDistanceToVertex2D, float& OutVertexDepthToBrush)
{
	// Project the vertex into the plane of the brush
	FVector BrushSpaceVertexPosition = InParams.InverseBrushToWorldMatrix.TransformPosition(InPosition);
	FVector2D BrushSpaceVertexPosition2D(BrushSpaceVertexPosition.X, BrushSpaceVertexPosition.Y);

	// Is the brush close enough to the vertex to paint?
	const float SquaredDistanceToVertex2D = BrushSpaceVertexPosition2D.SizeSquared();
	if (SquaredDistanceToVertex2D <= InParams.SquaredBrushRadius)
	{
		// OK the vertex is overlapping the brush in 2D space, but is it too close or
		// two far (depth wise) to be influenced?
		const float VertexDepthToBrush = FMath::Abs(BrushSpaceVertexPosition.Z);
		if (VertexDepthToBrush <= InParams.BrushDepth)
		{
			OutSquaredDistanceToVertex2D = SquaredDistanceToVertex2D;
			OutVertexDepthToBrush = VertexDepthToBrush;
			return true;
		}
	}

	return false;
}

bool MeshPaintHelpers::IsPointInfluencedByBrush(const FVector2D& BrushSpacePosition, const float BrushRadius, float& OutInRangeValue)
{
	const float DistanceToBrush = BrushSpacePosition.SizeSquared();
	if (DistanceToBrush <= BrushRadius)
	{
		OutInRangeValue = DistanceToBrush / BrushRadius;
		return true;
	}

	return false;
}

bool MeshPaintHelpers::RetrieveViewportPaintRays(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, TArray<FPaintRay>& OutPaintRays)
{
	checkf(View && Viewport && PDI, TEXT("Invalid Viewport data"));
	FEditorViewportClient* ViewportClient = (FEditorViewportClient*)Viewport->GetClient();
	checkf(ViewportClient != nullptr, TEXT("Unable to retrieve viewport client"));

	if (ViewportClient->IsPerspective())
	{
		// If in VR mode retrieve possible viewport interactors and render widgets for them
		UVREditorMode* VREditorMode = nullptr;
		if (MeshPaintHelpers::IsInVRMode(ViewportClient))
		{
			VREditorMode = Cast<UVREditorMode>(GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(ViewportClient->GetWorld())->FindExtension(UVREditorMode::StaticClass()));

			TArray<UViewportInteractor*> Interactors = VREditorMode->GetWorldInteraction().GetInteractors();

			for (UViewportInteractor* Interactor : Interactors)
			{
				checkf(Interactor, TEXT("Invalid VR Interactor"));
				
				// Don't draw visual cue if we're hovering over a viewport interact able, such as a dockable window selection bar
				bool bShouldDrawInteractor = false;
				FHitResult HitResult = Interactor->GetHitResultFromLaserPointer();
				if (HitResult.Actor.IsValid())
				{
					UViewportWorldInteraction& WorldInteraction = VREditorMode->GetWorldInteraction();

					if (WorldInteraction.IsInteractableComponent(HitResult.GetComponent()))
					{
						AActor* Actor = HitResult.Actor.Get();

						// Make sure we're not hovering over some other viewport interactable, such as a dockable window selection bar or close button
						IViewportInteractableInterface* ActorInteractable = Cast<IViewportInteractableInterface>(Actor);
						bShouldDrawInteractor = (ActorInteractable == nullptr);
					}
				}
				
				// Don't draw visual cue for paint brush when the interactor is hovering over UI
				if (bShouldDrawInteractor && !Interactor->IsHoveringOverPriorityType())
				{
					FVector LaserPointerStart, LaserPointerEnd;
					if (Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd))
					{
						const FVector LaserPointerDirection = (LaserPointerEnd - LaserPointerStart).GetSafeNormal();

						FPaintRay& NewPaintRay = *new(OutPaintRays) FPaintRay();
						NewPaintRay.CameraLocation = VREditorMode->GetHeadTransform().GetLocation();
						NewPaintRay.RayStart = LaserPointerStart;
						NewPaintRay.RayDirection = LaserPointerDirection;
						NewPaintRay.ViewportInteractor = Interactor;
					}
				}
			}
		}
		else
		{
			// Else we're painting with mouse
			// Make sure the cursor is visible OR we're flood filling.  No point drawing a paint cue when there's no cursor.
			if (Viewport->IsCursorVisible())
			{
				if (!PDI->IsHitTesting())
				{
					// Grab the mouse cursor position
					FIntPoint MousePosition;
					Viewport->GetMousePos(MousePosition);

					// Is the mouse currently over the viewport? or flood filling
					if ((MousePosition.X >= 0 && MousePosition.Y >= 0 && MousePosition.X < (int32)Viewport->GetSizeXY().X && MousePosition.Y < (int32)Viewport->GetSizeXY().Y))
					{
						// Compute a world space ray from the screen space mouse coordinates
						FViewportCursorLocation MouseViewportRay(View, ViewportClient, MousePosition.X, MousePosition.Y);

						FPaintRay& NewPaintRay = *new(OutPaintRays) FPaintRay();
						NewPaintRay.CameraLocation = View->ViewMatrices.GetViewOrigin();
						NewPaintRay.RayStart = MouseViewportRay.GetOrigin();
						NewPaintRay.RayDirection = MouseViewportRay.GetDirection();
						NewPaintRay.ViewportInteractor = nullptr;
					}
				}
			}
		}
	}

	return false;
}

uint32 MeshPaintHelpers::GetVertexColorBufferSize(UStaticMeshComponent* MeshComponent, int32 LODIndex, bool bInstance)
{
	checkf(MeshComponent != nullptr, TEXT("Invalid static mesh component ptr"));
	
	uint32 SizeInBytes = 0;

	// Retrieve component instance vertex color buffer size
	if (bInstance)
	{
		if (MeshComponent->LODData.IsValidIndex(LODIndex))
		{
			const FStaticMeshComponentLODInfo& InstanceMeshLODInfo = MeshComponent->LODData[LODIndex];
			if (InstanceMeshLODInfo.OverrideVertexColors)
			{
				SizeInBytes = InstanceMeshLODInfo.OverrideVertexColors->GetAllocatedSize();
			}
		}
	}
	// Retrieve static mesh asset vertex color buffer size
	else
	{
		UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
		checkf(StaticMesh != nullptr, TEXT("Invalid static mesh ptr"));
		if (StaticMesh->RenderData->LODResources.IsValidIndex(LODIndex))
		{
			// count the base mesh color data
			FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[LODIndex];
			SizeInBytes = LODModel.ColorVertexBuffer.GetAllocatedSize();
		}
	}
	
	return SizeInBytes;
}

TArray<FVector> MeshPaintHelpers::GetVerticesForLOD( const UStaticMesh* StaticMesh, int32 LODIndex)
{
	checkf(StaticMesh != nullptr, TEXT("Invalid static mesh ptr"));

	// Retrieve mesh vertices from Static mesh render data 
	TArray<FVector> Vertices;
	if (StaticMesh->RenderData->LODResources.IsValidIndex(LODIndex))
	{
		FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[LODIndex];
		FPositionVertexBuffer* VertexBuffer = &LODModel.PositionVertexBuffer;
		const uint32 NumVertices = VertexBuffer->GetNumVertices();
		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			Vertices.Add(VertexBuffer->VertexPosition(VertexIndex));
		}		
	}
	return Vertices;
}

TArray<FColor> MeshPaintHelpers::GetColorDataForLOD( const UStaticMesh* StaticMesh, int32 LODIndex)
{
	checkf(StaticMesh != nullptr, TEXT("Invalid static mesh ptr"));
	// Retrieve mesh vertex colors from Static mesh render data 
	TArray<FColor> Colors;
	if (StaticMesh->RenderData->LODResources.IsValidIndex(LODIndex))
	{
		const FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[LODIndex];
		const FColorVertexBuffer& ColorBuffer = LODModel.ColorVertexBuffer;
		const uint32 NumColors = ColorBuffer.GetNumVertices();
		for (uint32 ColorIndex = 0; ColorIndex < NumColors; ++ColorIndex)
		{
			Colors.Add(ColorBuffer.VertexColor(ColorIndex));
		}
	}
	return Colors;
}

TArray<FColor> MeshPaintHelpers::GetInstanceColorDataForLOD(const UStaticMeshComponent* MeshComponent, int32 LODIndex)
{
	checkf(MeshComponent != nullptr, TEXT("Invalid static mesh component ptr"));
	TArray<FColor> Colors;
	
	// Retrieve mesh vertex colors from Static Mesh component instance data
	if (MeshComponent->LODData.IsValidIndex(LODIndex))
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = MeshComponent->LODData[LODIndex];
		const FColorVertexBuffer* ColorBuffer = ComponentLODInfo.OverrideVertexColors;
		if (ColorBuffer)
		{
			const uint32 NumColors = ColorBuffer->GetNumVertices();
			for (uint32 ColorIndex = 0; ColorIndex < NumColors; ++ColorIndex)
			{
				Colors.Add(ColorBuffer->VertexColor(ColorIndex));
			}
		}
	}

	return Colors;
}

void MeshPaintHelpers::SetInstanceColorDataForLOD(UStaticMeshComponent* MeshComponent, int32 LODIndex, const TArray<FColor>& Colors)
{
	checkf(MeshComponent != nullptr, TEXT("Invalid static mesh component ptr"));

	const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
	if (Mesh)
	{
		const FStaticMeshLODResources& RenderData = Mesh->RenderData->LODResources[LODIndex];
		FStaticMeshComponentLODInfo& ComponentLodInfo = MeshComponent->LODData[LODIndex];		

		// First release existing buffer
		if (ComponentLodInfo.OverrideVertexColors)
		{
			ComponentLodInfo.ReleaseOverrideVertexColorsAndBlock();
		}

		// If we are adding colors to LOD > 0 we flag the component to have per-lod painted mesh colors
		if (LODIndex > 0)
		{
			MeshComponent->bCustomOverrideVertexColorPerLOD = true;			
		}

		// Initialize vertex buffer from given colors
		ComponentLodInfo.OverrideVertexColors = new FColorVertexBuffer;
		ComponentLodInfo.OverrideVertexColors->InitFromColorArray(Colors);
		BeginInitResource(ComponentLodInfo.OverrideVertexColors);
	}
}

void MeshPaintHelpers::SetInstanceColorDataForLOD(UStaticMeshComponent* MeshComponent, int32 LODIndex, const FColor FillColor)
{
	checkf(MeshComponent != nullptr, TEXT("Invalid static mesh component ptr"));

	const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
	if (Mesh)
	{
		const FStaticMeshLODResources& RenderData = Mesh->RenderData->LODResources[LODIndex];
		// Ensure we have enough LOD data structs
		MeshComponent->SetLODDataCount(LODIndex + 1, MeshComponent->LODData.Num());
		FStaticMeshComponentLODInfo& ComponentLodInfo = MeshComponent->LODData[LODIndex];
		// First release existing buffer
		if (ComponentLodInfo.OverrideVertexColors)
		{
			ComponentLodInfo.ReleaseOverrideVertexColorsAndBlock();
		}

		// If we are adding colors to LOD > 0 we flag the component to have per-lod painted mesh colors
		if (LODIndex > 0)
		{
			MeshComponent->bCustomOverrideVertexColorPerLOD = true;
		}

		// Initialize vertex buffer from given color
		ComponentLodInfo.OverrideVertexColors = new FColorVertexBuffer;
		ComponentLodInfo.OverrideVertexColors->InitFromSingleColor(FillColor, RenderData.GetNumVertices() );	
		BeginInitResource(ComponentLodInfo.OverrideVertexColors);
	}
}

void MeshPaintHelpers::FillVertexColors(UMeshComponent* MeshComponent, const FColor FillColor, bool bInstanced /*= false*/)
{
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
	{
		if (bInstanced)
		{
			UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
			if (Mesh && Mesh->GetNumLODs() > 0)
			{
				const int32 NumLods = Mesh->GetNumLODs();
				for (int32 LODIndex = 0; LODIndex < NumLods; ++LODIndex)
				{
					MeshPaintHelpers::SetInstanceColorDataForLOD(StaticMeshComponent, LODIndex, FillColor);
				}
			}
		}
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent))
	{
		TUniquePtr< FSkeletalMeshComponentRecreateRenderStateContext > RecreateRenderStateContext;
		USkeletalMesh* Mesh = SkeletalMeshComponent->SkeletalMesh;
		if (Mesh)
		{
			// Dirty the mesh
			Mesh->SetFlags(RF_Transactional);
			Mesh->Modify();
			Mesh->bHasVertexColors = true;

			// Release the static mesh's resources.
			Mesh->ReleaseResources();

			// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
			// allocated, and potentially accessing the UStaticMesh.
			Mesh->ReleaseResourcesFence.Wait();

			if (Mesh->LODInfo.Num() > 0)
			{
				RecreateRenderStateContext = MakeUnique<FSkeletalMeshComponentRecreateRenderStateContext>(Mesh);
				const int32 NumLods = Mesh->LODInfo.Num();
				for (int32 LODIndex = 0; LODIndex < NumLods; ++LODIndex)
				{
					MeshPaintHelpers::SetColorDataForLOD(Mesh, LODIndex, FillColor);
				}
				Mesh->InitResources();
			}
		}
	}
}

void MeshPaintHelpers::SetColorDataForLOD(USkeletalMesh* SkeletalMesh, int32 LODIndex, const FColor FillColor)
{
	checkf(SkeletalMesh != nullptr, TEXT("Invalid Skeletal Mesh Ptr"));
	FSkeletalMeshResource* Resource = SkeletalMesh->GetImportedResource();
	if (Resource && Resource->LODModels.IsValidIndex(LODIndex))
	{
		FStaticLODModel& LOD = Resource->LODModels[LODIndex];
		LOD.ColorVertexBuffer.InitFromSingleColor(FillColor, LOD.NumVertices);
		BeginInitResource(&LOD.ColorVertexBuffer);
	}	
}

void MeshPaintHelpers::ImportVertexColorsFromTexture(UMeshComponent* MeshComponent)
{
	checkf(MeshComponent != nullptr, TEXT("Invalid mesh component ptr"));

	// Get TGA texture filepath
	FString ChosenFilename("");
	FString ExtensionStr;
	ExtensionStr += TEXT("TGA Files|*.tga|");

	FString PromptTitle("Pick TGA Texture File");

	// First, display the file open dialog for selecting the file.
	TArray<FString> Filenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			PromptTitle,
			TEXT(""),
			TEXT(""),
			*ExtensionStr,
			EFileDialogFlags::None,
			Filenames
		);
	}

	if (bOpen && Filenames.Num() == 1)
	{
		// Valid file name picked
		const FString FileName = Filenames[0];
		UTexture2D* ColorTexture = ImportObject<UTexture2D>(GEngine, NAME_None, RF_Public, *FileName, nullptr, nullptr, TEXT("NOMIPMAPS=1 NOCOMPRESSION=1"));

		if (ColorTexture && ColorTexture->Source.GetFormat() == TSF_BGRA8)
		{
			// Have a valid texture, now need user to specify options for importing
			TSharedRef<SWindow> Window = SNew(SWindow)
				.Title(FText::FromString(TEXT("Vertex Color Import Options")))
				.SizingRule(ESizingRule::Autosized);

			TSharedPtr<SImportVertexColorOptions> OptionsWindow = SNew(SImportVertexColorOptions).WidgetWindow(Window)
				.WidgetWindow(Window)
				.Component(MeshComponent)
				.FullPath(FText::FromString(ChosenFilename));

			Window->SetContent
			(
				OptionsWindow->AsShared()
			);

			TSharedPtr<SWindow> ParentWindow;
			if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
			{
				IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
				ParentWindow = MainFrame.GetParentWindow();
			}
			FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

			if (OptionsWindow->ShouldImport())
			{
				// Options specified and start importing
				UVertexColorImportOptions* Options = OptionsWindow->GetOptions();
				
				if (MeshComponent->IsA<UStaticMeshComponent>())				
				{ 
					UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent);
					if (StaticMeshComponent)
					{
						if (Options->bImportToInstance)
						{
							// Import colors to static mesh / component
							ImportVertexColorsToStaticMeshComponent(StaticMeshComponent, Options, ColorTexture);
						}
						else
						{
							if (StaticMeshComponent->GetStaticMesh())
							{
								ImportVertexColorsToStaticMesh(StaticMeshComponent->GetStaticMesh(), Options, ColorTexture);
							}
						}						
					}					
				}
				else if (MeshComponent->IsA<USkeletalMeshComponent>())
				{
					USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent);

					if (SkeletalMeshComponent->SkeletalMesh)
					{
						// Import colors to skeletal mesh
						ImportVertexColorsToSkeletalMesh(SkeletalMeshComponent->SkeletalMesh, Options, ColorTexture);
					}			
				}
			}
		}
		else if (!ColorTexture)
		{
			// Unable to import file
		}
		else if (ColorTexture && ColorTexture->Source.GetFormat() != TSF_BGRA8)
		{
			// Able to import file but incorrect format
		}
	}
}

void MeshPaintHelpers::SetViewportColorMode(EMeshPaintColorViewMode ColorViewMode, FEditorViewportClient* ViewportClient)
{
	if (ViewportClient->IsPerspective())
	{
		// Update viewport show flags
		{
			// show flags forced on during vertex color modes
			if (ColorViewMode == EMeshPaintColorViewMode::Normal)
			{
				ColorViewMode = EMeshPaintColorViewMode::Normal;
			}

			if (ColorViewMode == EMeshPaintColorViewMode::Normal)
			{
				if (ViewportClient->EngineShowFlags.VertexColors)
				{
					// If we're transitioning to normal mode then restore the backup
					// Clear the flags relevant to vertex color modes
					ViewportClient->EngineShowFlags.SetVertexColors(false);

					// Restore the vertex color mode flags that were set when we last entered vertex color mode
					ApplyViewMode(ViewportClient->GetViewMode(), ViewportClient->IsPerspective(), ViewportClient->EngineShowFlags);
					GVertexColorViewMode = EVertexColorViewMode::Color;
				}
			}
			else
			{
				ViewportClient->EngineShowFlags.SetMaterials(true);
				ViewportClient->EngineShowFlags.SetLighting(false);
				ViewportClient->EngineShowFlags.SetBSPTriangles(true);
				ViewportClient->EngineShowFlags.SetVertexColors(true);
				ViewportClient->EngineShowFlags.SetPostProcessing(false);
				ViewportClient->EngineShowFlags.SetHMDDistortion(false);

				switch (ColorViewMode)
				{
					case EMeshPaintColorViewMode::RGB:
					{
						GVertexColorViewMode = EVertexColorViewMode::Color;
					}
					break;

					case EMeshPaintColorViewMode::Alpha:
					{
						GVertexColorViewMode = EVertexColorViewMode::Alpha;
					}
					break;

					case EMeshPaintColorViewMode::Red:
					{
						GVertexColorViewMode = EVertexColorViewMode::Red;
					}
					break;

					case EMeshPaintColorViewMode::Green:
					{
						GVertexColorViewMode = EVertexColorViewMode::Green;
					}
					break;

					case EMeshPaintColorViewMode::Blue:
					{
						GVertexColorViewMode = EVertexColorViewMode::Blue;
					}
					break;
				}
			}
		}
	}
}

void MeshPaintHelpers::SetRealtimeViewport(bool bRealtime)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr< ILevelViewport > ViewportWindow = LevelEditorModule.GetFirstActiveViewport();
	const bool bRememberCurrentState = false;
	if (ViewportWindow.IsValid())
	{
		FEditorViewportClient &Viewport = ViewportWindow->GetLevelViewportClient();
		if (Viewport.IsPerspective())
		{
			if (bRealtime)
			{
				Viewport.SetRealtime(bRealtime, bRememberCurrentState);
			}
			else
			{
				const bool bAllowDisable = true;
				Viewport.RestoreRealtime(bAllowDisable);
			}
		}
	}
}


bool MeshPaintHelpers::IsInVRMode(const FEditorViewportClient* ViewportClient)
{
	bool bIsInVRMode = false;
	if (IVREditorModule::IsAvailable())
	{
		UVREditorMode* VREditorMode = Cast<UVREditorMode>(GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(ViewportClient->GetWorld())->FindExtension(UVREditorMode::StaticClass()));
		if (VREditorMode != nullptr && VREditorMode->IsFullyInitialized() && VREditorMode->IsActive())
		{
			bIsInVRMode = true;
		}
	}

	return bIsInVRMode;
}

void MeshPaintHelpers::ForceRenderMeshLOD(UMeshComponent* Component, int32 LODIndex)
{
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
	{
		StaticMeshComponent->ForcedLodModel = LODIndex + 1;
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component))
	{
		SkeletalMeshComponent->ForcedLodModel = LODIndex + 1;
	}	
}

void MeshPaintHelpers::ClearMeshTextureOverrides(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		TArray<UTexture*> UsedTextures;
		InMeshComponent->GetUsedTextures(/*out*/ UsedTextures, EMaterialQualityLevel::High);

		for (UTexture* Texture : UsedTextures)
		{
			if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
			{
				GeometryInfo.ApplyOrRemoveTextureOverride(Texture2D, nullptr);
			}
		}
	}
}

void MeshPaintHelpers::ApplyVertexColorsToAllLODs(IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent)
{
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(InMeshComponent))
	{
		ApplyVertexColorsToAllLODs(GeometryInfo, StaticMeshComponent);
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InMeshComponent))
	{
		ApplyVertexColorsToAllLODs(GeometryInfo, SkeletalMeshComponent);
	}
}

void MeshPaintHelpers::ApplyVertexColorsToAllLODs(IMeshPaintGeometryAdapter& GeometryInfo, UStaticMeshComponent* StaticMeshComponent)
{
	// If a static mesh component was found, apply LOD0 painting to all lower LODs.
	if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
	{
		return;
	}

	if (StaticMeshComponent->LODData.Num() < 1)
	{
		//We need at least some painting on the base LOD to apply it to the lower LODs
		return;
	}
	FStaticMeshComponentLODInfo& SourceCompLODInfo = StaticMeshComponent->LODData[0];
	FStaticMeshLODResources& SourceRenderData = StaticMeshComponent->GetStaticMesh()->RenderData->LODResources[0];
	//Make sure we have something paint in the LOD 0 to apply it to all lower LODs.
	if (SourceCompLODInfo.OverrideVertexColors == nullptr && SourceCompLODInfo.PaintedVertices.Num() <= 0)
	{
		return;
	}

	StaticMeshComponent->bCustomOverrideVertexColorPerLOD = false;

	uint32 NumLODs = StaticMeshComponent->GetStaticMesh()->RenderData->LODResources.Num();
	StaticMeshComponent->Modify();

	// Ensure LODData has enough entries in it, free not required.
	StaticMeshComponent->SetLODDataCount(NumLODs, StaticMeshComponent->LODData.Num());
	for (uint32 i = 1; i < NumLODs; ++i)
	{
		FStaticMeshComponentLODInfo* CurrInstanceMeshLODInfo = &StaticMeshComponent->LODData[i];
		FStaticMeshLODResources& CurrRenderData = StaticMeshComponent->GetStaticMesh()->RenderData->LODResources[i];
		// Destroy the instance vertex  color array if it doesn't fit
		if (CurrInstanceMeshLODInfo->OverrideVertexColors
			&& CurrInstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() != CurrRenderData.GetNumVertices())
		{
			CurrInstanceMeshLODInfo->ReleaseOverrideVertexColorsAndBlock();
		}

		if (CurrInstanceMeshLODInfo->OverrideVertexColors)
		{
			CurrInstanceMeshLODInfo->BeginReleaseOverrideVertexColors();
		}
		else
		{
			// Setup the instance vertex color array if we don't have one yet
			CurrInstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;
		}
	}

	FlushRenderingCommands();
	for (uint32 i = 1; i < NumLODs; ++i)
	{
		FStaticMeshComponentLODInfo& CurCompLODInfo = StaticMeshComponent->LODData[i];
		FStaticMeshLODResources& CurRenderData = StaticMeshComponent->GetStaticMesh()->RenderData->LODResources[i];

		check(CurCompLODInfo.OverrideVertexColors);
		check(SourceCompLODInfo.OverrideVertexColors);

		TArray<FColor> NewOverrideColors;
		
		RemapPaintedVertexColors(
			SourceCompLODInfo.PaintedVertices,
			*SourceCompLODInfo.OverrideVertexColors,
			SourceRenderData.PositionVertexBuffer,
			SourceRenderData.VertexBuffer,
			CurRenderData.PositionVertexBuffer,
			&CurRenderData.VertexBuffer,
			NewOverrideColors
		);
		
		if (NewOverrideColors.Num())
		{
			CurCompLODInfo.OverrideVertexColors->InitFromColorArray(NewOverrideColors);
		}

		// Initialize the vert. colors
		BeginInitResource(CurCompLODInfo.OverrideVertexColors);
	}
}

int32 MeshPaintHelpers::GetNumberOfLODs(const UMeshComponent* MeshComponent)
{
	int32 NumLODs = 1;

	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
	{
		const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (StaticMesh != nullptr)
		{
			NumLODs = StaticMesh->GetNumLODs();
		}
	}
	else if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent))
	{
		const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
		if (SkeletalMesh != nullptr)
		{
			NumLODs = SkeletalMesh->LODInfo.Num();
		}
	}

	return NumLODs;
}

int32 MeshPaintHelpers::GetNumberOfUVs(const UMeshComponent* MeshComponent, int32 LODIndex)
{
	int32 NumUVs = 0;

	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
	{
		const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (StaticMesh != nullptr && StaticMesh->RenderData->LODResources.IsValidIndex(LODIndex))
		{
			NumUVs = StaticMesh->RenderData->LODResources[LODIndex].GetNumTexCoords();
		}
	}
	else if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent))
	{
		const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
		if (SkeletalMesh != nullptr && SkeletalMesh->GetImportedResource() && SkeletalMesh->GetImportedResource()->LODModels.IsValidIndex(LODIndex))
		{
			NumUVs = SkeletalMesh->GetImportedResource()->LODModels[LODIndex].NumTexCoords;
		}
	}

	return NumUVs;
}

bool MeshPaintHelpers::DoesMeshComponentContainPerLODColors(const UMeshComponent* MeshComponent)
{
	bool bPerLODColors = false;

	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
	{
		bPerLODColors = StaticMeshComponent->bCustomOverrideVertexColorPerLOD;
		
		const int32 NumLODs = StaticMeshComponent->LODData.Num();
		bool bInstancedLODColors = false;
		for (int32 LODIndex = 1; LODIndex < NumLODs; ++LODIndex)
		{
			if (StaticMeshComponent->LODData[LODIndex].PaintedVertices.Num() > 0)
			{
				bInstancedLODColors = true;
				break;
			}
		}

		bPerLODColors = bInstancedLODColors;		
	}
	else if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent))
	{
		USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
		if (SkeletalMesh)
		{
			for ( const FSkeletalMeshLODInfo& Info : SkeletalMesh->LODInfo )
			{
				if (Info.bHasPerLODVertexColors)
				{
					bPerLODColors = true;
					break;
				}
			}
		}
	}

	return bPerLODColors;
}

void MeshPaintHelpers::GetInstanceColorDataInfo(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, int32& OutTotalInstanceVertexColorBytes)
{
	checkf(StaticMeshComponent, TEXT("Invalid StaticMeshComponent"));
	OutTotalInstanceVertexColorBytes = 0;
	
	const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh != nullptr && StaticMesh->GetNumLODs() > (int32)LODIndex && StaticMeshComponent->LODData.IsValidIndex(LODIndex))
	{
		// count the instance color data		
		const FStaticMeshComponentLODInfo& InstanceMeshLODInfo = StaticMeshComponent->LODData[LODIndex];
		if (InstanceMeshLODInfo.OverrideVertexColors)
		{
			OutTotalInstanceVertexColorBytes += InstanceMeshLODInfo.OverrideVertexColors->GetAllocatedSize();
		}
	}
}

void MeshPaintHelpers::ImportVertexColorsToStaticMesh(UStaticMesh* StaticMesh, const UVertexColorImportOptions* Options, UTexture2D* Texture)
{
	checkf(StaticMesh && Options && Texture, TEXT("Invalid ptr"));

	// Extract color data from texture
	TArray<uint8> SrcMipData;
	Texture->Source.GetMipData(SrcMipData, 0);
	const uint8* MipData = SrcMipData.GetData();

	TUniquePtr< FStaticMeshComponentRecreateRenderStateContext > RecreateRenderStateContext = MakeUnique<FStaticMeshComponentRecreateRenderStateContext>(StaticMesh);
	const int32 ImportLOD = Options->LODIndex;
	FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[ImportLOD];
	
	// Dirty the mesh
	StaticMesh->Modify();

	// Release the static mesh's resources.
	StaticMesh->ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
	// allocated, and potentially accessing the UStaticMesh.
	StaticMesh->ReleaseResourcesFence.Wait();

	if (LODModel.ColorVertexBuffer.GetNumVertices() == 0)
	{
		// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
		LODModel.ColorVertexBuffer.InitFromSingleColor(FColor::White, LODModel.GetNumVertices());

		// @todo MeshPaint: Make sure this is the best place to do this
		BeginInitResource(&LODModel.ColorVertexBuffer);
	}

	const int32 UVIndex = Options->UVIndex;
	const FColor ColorMask = Options->CreateColorMask();
	for (uint32 VertexIndex = 0; VertexIndex < LODModel.VertexBuffer.GetNumVertices(); ++VertexIndex)
	{
		const FVector2D UV = LODModel.VertexBuffer.GetVertexUV(VertexIndex, UVIndex);
		LODModel.ColorVertexBuffer.VertexColor(VertexIndex) = PickVertexColorFromTextureData(MipData, UV, Texture, ColorMask);
	}

	// Make sure colors are saved into raw mesh

	StaticMesh->InitResources();
}

void MeshPaintHelpers::ImportVertexColorsToStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent, const UVertexColorImportOptions* Options, UTexture2D* Texture)
{
	checkf(StaticMeshComponent && Options && Texture, TEXT("Invalid ptr"));

	// Extract color data from texture
	TArray<uint8> SrcMipData;
	Texture->Source.GetMipData(SrcMipData, 0);
	const uint8* MipData = SrcMipData.GetData();

	TUniquePtr< FComponentReregisterContext > ComponentReregisterContext;
	const UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();	
	if (Mesh)
	{
		ComponentReregisterContext = MakeUnique<FComponentReregisterContext>(StaticMeshComponent);
		StaticMeshComponent->Modify();

		const int32 ImportLOD = Options->LODIndex;
		FStaticMeshLODResources& LODModel = Mesh->RenderData->LODResources[ImportLOD];

		if (!StaticMeshComponent->LODData.IsValidIndex(ImportLOD))
		{
			StaticMeshComponent->SetLODDataCount(ImportLOD + 1, StaticMeshComponent->LODData.Num());
		}		

		FStaticMeshComponentLODInfo& InstanceMeshLODInfo = StaticMeshComponent->LODData[ImportLOD];

		if (InstanceMeshLODInfo.OverrideVertexColors)
		{
			InstanceMeshLODInfo.ReleaseOverrideVertexColorsAndBlock();
		}
		
		// Setup the instance vertex color array
		InstanceMeshLODInfo.OverrideVertexColors = new FColorVertexBuffer;

		if ((int32)LODModel.ColorVertexBuffer.GetNumVertices() == LODModel.GetNumVertices())
		{
			// copy mesh vertex colors to the instance ones
			InstanceMeshLODInfo.OverrideVertexColors->InitFromColorArray(&LODModel.ColorVertexBuffer.VertexColor(0), LODModel.GetNumVertices());
		}
		else
		{
			// Original mesh didn't have any colors, so just use a default color
			MeshPaintHelpers::SetInstanceColorDataForLOD(StaticMeshComponent, ImportLOD, FColor::White);
		}

		const int32 UVIndex = Options->UVIndex;
		const FColor ColorMask = Options->CreateColorMask();
		for (uint32 VertexIndex = 0; VertexIndex < LODModel.VertexBuffer.GetNumVertices(); ++VertexIndex)
		{
			const FVector2D UV = LODModel.VertexBuffer.GetVertexUV(VertexIndex, UVIndex);			
			InstanceMeshLODInfo.OverrideVertexColors->VertexColor(VertexIndex) = PickVertexColorFromTextureData(MipData, UV, Texture, ColorMask);
		}

		BeginInitResource(InstanceMeshLODInfo.OverrideVertexColors);
	}
	else
	{
		// Error
	}
}

void MeshPaintHelpers::ImportVertexColorsToSkeletalMesh(USkeletalMesh* SkeletalMesh, const UVertexColorImportOptions* Options, UTexture2D* Texture)
{
	checkf(SkeletalMesh && Options && Texture, TEXT("Invalid ptr"));

	// Extract color data from texture
	TArray<uint8> SrcMipData;
	Texture->Source.GetMipData(SrcMipData, 0);
	const uint8* MipData = SrcMipData.GetData();

	TUniquePtr< FSkeletalMeshComponentRecreateRenderStateContext > RecreateRenderStateContext;
	FSkeletalMeshResource* Resource = SkeletalMesh->GetImportedResource();
	const int32 ImportLOD = Options->LODIndex;
	if (Resource && Resource->LODModels.IsValidIndex(ImportLOD))
	{
		RecreateRenderStateContext = MakeUnique<FSkeletalMeshComponentRecreateRenderStateContext>(SkeletalMesh);
		SkeletalMesh->Modify();
		SkeletalMesh->ReleaseResources();
		SkeletalMesh->ReleaseResourcesFence.Wait();

		FStaticLODModel& LODModel = Resource->LODModels[ImportLOD];

		if (LODModel.ColorVertexBuffer.GetNumVertices() == 0)
		{
			LODModel.ColorVertexBuffer.InitFromSingleColor(FColor::White, LODModel.NumVertices);
			BeginInitResource(&LODModel.ColorVertexBuffer);
		}

		const int32 UVIndex = Options->UVIndex;
		const FColor ColorMask = Options->CreateColorMask();
		for (uint32 VertexIndex = 0; VertexIndex < LODModel.NumVertices; ++VertexIndex)
		{
			const FVector2D UV = LODModel.VertexBufferGPUSkin.GetVertexUV(VertexIndex, UVIndex);
			LODModel.ColorVertexBuffer.VertexColor(VertexIndex) = PickVertexColorFromTextureData(MipData, UV, Texture, ColorMask);
		}
		
		SkeletalMesh->InitResources();
	}
	else
	{
		// ERROR
	}
}

FColor MeshPaintHelpers::PickVertexColorFromTextureData(const uint8* MipData, const FVector2D& UVCoordinate, const UTexture2D* Texture, const FColor ColorMask)
{
	checkf(MipData, TEXT("Invalid texture MIP data"));
	FColor VertexColor = FColor::Black;

	if ((UVCoordinate.X >= 0.0f) && (UVCoordinate.X < 1.0f) && (UVCoordinate.Y >= 0.0f) && (UVCoordinate.Y < 1.0f))
	{
		const int32 X = Texture->GetSizeX()*UVCoordinate.X;
		const int32 Y = Texture->GetSizeY()*UVCoordinate.Y;

		const int32 Index = ((Y * Texture->GetSizeX()) + X) * 4;
		VertexColor.B = MipData[Index + 0];
		VertexColor.G = MipData[Index + 1];
		VertexColor.R = MipData[Index + 2];
		VertexColor.A = MipData[Index + 3];

		VertexColor.DWColor() &= ColorMask.DWColor();
	}

	return VertexColor;
}

bool MeshPaintHelpers::ApplyPerVertexPaintAction(FPerVertexPaintActionArgs& InArgs, FPerVertexPaintAction Action)
{
	// Retrieve components world matrix
	const FMatrix& ComponentToWorldMatrix = InArgs.Adapter->GetComponentToWorldMatrix();
	
	// Compute the camera position in actor space.  We need this later to check for back facing triangles.
	const FVector ComponentSpaceCameraPosition(ComponentToWorldMatrix.InverseTransformPosition(InArgs.CameraPosition));
	const FVector ComponentSpaceBrushPosition(ComponentToWorldMatrix.InverseTransformPosition(InArgs.HitResult.Location));

	// @todo MeshPaint: Input vector doesn't work well with non-uniform scale
	const float BrushRadius = InArgs.BrushSettings->GetBrushRadius();
	const float ComponentSpaceBrushRadius = ComponentToWorldMatrix.InverseTransformVector(FVector(BrushRadius, 0.0f, 0.0f)).Size();
	const float ComponentSpaceSquaredBrushRadius = ComponentSpaceBrushRadius * ComponentSpaceBrushRadius;

	// Get a list of unique vertices indexed by the influenced triangles
	TSet<int32> InfluencedVertices;
	InArgs.Adapter->GetInfluencedVertexIndices(ComponentSpaceSquaredBrushRadius, ComponentSpaceBrushPosition, ComponentSpaceCameraPosition, InArgs.BrushSettings->bOnlyFrontFacingTriangles, InfluencedVertices);


	const int32 NumParallelFors = 4;
	const int32 NumPerFor = FMath::CeilToInt((float)InfluencedVertices.Num() / NumParallelFors);
	
	// Parallel applying
	/*ParallelFor(NumParallelFors, [=](int32 Index)
	{
		const int32 Start = Index * NumPerFor;
		const int32 End = FMath::Min(Start + NumPerFor, InfluencedVertices.Num());
		
		for (int32 VertexIndex = Start; VertexIndex < End; ++VertexIndex)
		{
			Action.ExecuteIfBound(Adapter, VertexIndex);
		}
	});*/
	if (InfluencedVertices.Num())
	{
		InArgs.Adapter->PreEdit();
		for (const int32 VertexIndex : InfluencedVertices)
		{
		// Apply the action!			
			Action.ExecuteIfBound(InArgs, VertexIndex);
		}
		InArgs.Adapter->PostEdit();
	}

	return (InfluencedVertices.Num() > 0);
}

bool MeshPaintHelpers::ApplyPerTrianglePaintAction(IMeshPaintGeometryAdapter* Adapter, const FVector& CameraPosition, const FVector& HitPosition, const UPaintBrushSettings* Settings, FPerTrianglePaintAction Action)
{
	// Retrieve components world matrix
	const FMatrix& ComponentToWorldMatrix = Adapter->GetComponentToWorldMatrix();

	// Compute the camera position in actor space.  We need this later to check for back facing triangles.
	const FVector ComponentSpaceCameraPosition(ComponentToWorldMatrix.InverseTransformPosition(CameraPosition));
	const FVector ComponentSpaceBrushPosition(ComponentToWorldMatrix.InverseTransformPosition(HitPosition));

	// @todo MeshPaint: Input vector doesn't work well with non-uniform scale
	const float BrushRadius = Settings->GetBrushRadius();
	const float ComponentSpaceBrushRadius = ComponentToWorldMatrix.InverseTransformVector(FVector(BrushRadius, 0.0f, 0.0f)).Size();
	const float ComponentSpaceSquaredBrushRadius = ComponentSpaceBrushRadius * ComponentSpaceBrushRadius;

	// Get a list of (optionally front-facing) triangles that are within a reasonable distance to the brush
	TArray<uint32> InfluencedTriangles = Adapter->SphereIntersectTriangles(
		ComponentSpaceSquaredBrushRadius,
		ComponentSpaceBrushPosition,
		ComponentSpaceCameraPosition,
		Settings->bOnlyFrontFacingTriangles);

	int32 TriangleIndices[3];

	const TArray<uint32> VertexIndices = Adapter->GetMeshIndices();
	for (uint32 TriangleIndex : InfluencedTriangles)
	{
		// Grab the vertex indices and points for this triangle
		for (int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum)
		{
			TriangleIndices[TriVertexNum] = VertexIndices[TriangleIndex * 3 + TriVertexNum];
		}

		Action.Execute(Adapter, TriangleIndex, TriangleIndices);
	}

	return (InfluencedTriangles.Num() > 0);
}

struct FPaintedMeshVertex
{
	FVector Position;
	FPackedNormal Normal;
	FColor Color;
};

/** Helper struct for the mesh component vert position octree */
struct FVertexColorPropogationOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	/**
	 * Get the bounding box of the provided octree element. In this case, the box
	 * is merely the point specified by the element.
	 *
	 * @param	Element	Octree element to get the bounding box for
	 *
	 * @return	Bounding box of the provided octree element
	 */
	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox( const FPaintedMeshVertex& Element )
	{
		return FBoxCenterAndExtent( Element.Position, FVector::ZeroVector );
	}

	/**
	 * Determine if two octree elements are equal
	 *
	 * @param	A	First octree element to check
	 * @param	B	Second octree element to check
	 *
	 * @return	true if both octree elements are equal, false if they are not
	 */
	FORCEINLINE static bool AreElementsEqual( const FPaintedMeshVertex& A, const FPaintedMeshVertex& B )
	{
		return ( A.Position == B.Position && A.Normal == B.Normal && A.Color == B.Color );
	}

	/** Ignored for this implementation */
	FORCEINLINE static void SetElementId( const FPaintedMeshVertex& Element, FOctreeElementId Id )
	{
	}
};
typedef TOctree<FPaintedMeshVertex, FVertexColorPropogationOctreeSemantics> TVertexColorPropogationPosOctree;

void MeshPaintHelpers::ApplyVertexColorsToAllLODs(IMeshPaintGeometryAdapter& GeometryInfo, USkeletalMeshComponent* SkeletalMeshComponent)
{
	checkf(SkeletalMeshComponent != nullptr, TEXT("Invalid Skeletal Mesh Component"));
	USkeletalMesh* Mesh = SkeletalMeshComponent->SkeletalMesh;
	if (Mesh)
	{
		FSkeletalMeshResource* Resource = Mesh->GetImportedResource();
		if (Resource)
		{
			const int32 NumLODs = Resource->LODModels.Num();
			if (NumLODs > 1)
			{
				const FStaticLODModel& BaseLOD = Resource->LODModels[0];
				GeometryInfo.PreEdit();				

				FBox BaseBounds(EForceInit::ForceInitToZero);
				TArray<FSoftSkinVertex> BaseVertices;
				BaseLOD.GetVertices(BaseVertices);

				TArray<FPaintedMeshVertex> PaintedVertices;
				PaintedVertices.Empty(BaseVertices.Num());

				FPaintedMeshVertex PaintedVertex;
				for (int32 VertexIndex = 0; VertexIndex < BaseVertices.Num(); ++VertexIndex )
				{
					const FSoftSkinVertex& Vertex = BaseVertices[VertexIndex];
					BaseBounds += Vertex.Position;
					PaintedVertex.Position = Vertex.Position;
					PaintedVertex.Normal = Vertex.TangentZ;
					PaintedVertex.Color = BaseLOD.ColorVertexBuffer.VertexColor(VertexIndex);
					PaintedVertices.Add(PaintedVertex);
				}

				for (int32 LODIndex = 1; LODIndex < NumLODs; ++LODIndex)
				{
					// Do something
					FStaticLODModel& ApplyLOD = Resource->LODModels[LODIndex];
					FBox CombinedBounds = BaseBounds;
					Mesh->LODInfo[LODIndex].bHasPerLODVertexColors = false;

					if (!ApplyLOD.ColorVertexBuffer.IsInitialized())
					{
						ApplyLOD.ColorVertexBuffer.InitFromSingleColor(FColor::White, ApplyLOD.NumVertices);
					}

					TArray<FSoftSkinVertex> ApplyVertices;
					ApplyLOD.GetVertices(ApplyVertices);
					for (const FSoftSkinVertex& Vertex : ApplyVertices)
					{
						CombinedBounds += Vertex.Position;
					}
					
					TVertexColorPropogationPosOctree VertPosOctree(CombinedBounds.GetCenter(), CombinedBounds.GetExtent().GetMax());
					
					// Add each old vertex to the octree
					for (const FPaintedMeshVertex& Vertex : PaintedVertices)
					{
						VertPosOctree.AddElement(Vertex);
					}

					// Iterate over each new vertex position, attempting to find the old vertex it is closest to, applying
					// the color of the old vertex to the new position if possible.
					const float DistanceOverNormalThreshold = KINDA_SMALL_NUMBER;
					for (int32 VertexIndex = 0; VertexIndex < ApplyVertices.Num(); ++VertexIndex)
					{
						const FSoftSkinVertex& Vertex = ApplyVertices[VertexIndex];
						TArray<FPaintedMeshVertex> PointsToConsider;
						TVertexColorPropogationPosOctree::TConstIterator<> OctreeIter(VertPosOctree);
						const FVector& CurPosition = Vertex.Position;
						FVector CurNormal = Vertex.TangentZ;

						// Iterate through the octree attempting to find the vertices closest to the current new point
						while (OctreeIter.HasPendingNodes())
						{
							const TVertexColorPropogationPosOctree::FNode& CurNode = OctreeIter.GetCurrentNode();
							const FOctreeNodeContext& CurContext = OctreeIter.GetCurrentContext();

							// Find the child of the current node, if any, that contains the current new point
							FOctreeChildNodeRef ChildRef = CurContext.GetContainingChild(FBoxCenterAndExtent(CurPosition, FVector::ZeroVector));

							if (!ChildRef.IsNULL())
							{
								const TVertexColorPropogationPosOctree::FNode* ChildNode = CurNode.GetChild(ChildRef);

								// If the specified child node exists and contains any of the old vertices, push it to the iterator for future consideration
								if (ChildNode && ChildNode->GetInclusiveElementCount() > 0)
								{
									OctreeIter.PushChild(ChildRef);
								}
								// If the child node doesn't have any of the old vertices in it, it's not worth pursuing any further. In an attempt to find
								// anything to match vs. the new point, add all of the children of the current octree node that have old points in them to the
								// iterator for future consideration.
								else
								{
									FOREACH_OCTREE_CHILD_NODE(OctreeChildRef)
									{
										if (CurNode.HasChild(OctreeChildRef))
										{
											OctreeIter.PushChild(OctreeChildRef);
										}
									}
								}
							}

							// Add all of the elements in the current node to the list of points to consider for closest point calculations
							PointsToConsider.Append(CurNode.GetElements());
							OctreeIter.Advance();
						}

						// If any points to consider were found, iterate over each and find which one is the closest to the new point 
						if (PointsToConsider.Num() > 0)
						{
							int32 BestVertexIndex = 0;
							FVector BestVertexNormal = PointsToConsider[BestVertexIndex].Normal;

							float BestDistanceSquared = (PointsToConsider[BestVertexIndex].Position - CurPosition).SizeSquared();
							float BestNormalDot = BestVertexNormal | CurNormal;

							for (int32 ConsiderationIndex = 1; ConsiderationIndex < PointsToConsider.Num(); ++ConsiderationIndex)
							{
								FPaintedMeshVertex& CheckVertex = PointsToConsider[ConsiderationIndex];
								FVector VertexNormal = CheckVertex.Normal;

								const float DistSqrd = (CheckVertex.Position - CurPosition).SizeSquared();
								const float NormalDot = VertexNormal | CurNormal;
								if (DistSqrd < BestDistanceSquared - DistanceOverNormalThreshold)
								{
									BestVertexIndex = ConsiderationIndex;
									
									BestDistanceSquared = DistSqrd;
									BestNormalDot = NormalDot;
								}
								else if (DistSqrd < BestDistanceSquared + DistanceOverNormalThreshold && NormalDot > BestNormalDot)
								{
									BestVertexIndex = ConsiderationIndex;
									BestDistanceSquared = DistSqrd;
									BestNormalDot = NormalDot;
								}
							}

							ApplyLOD.ColorVertexBuffer.VertexColor(VertexIndex) = PointsToConsider[BestVertexIndex].Color;
						}
					}
				}
				
				GeometryInfo.PostEdit();
			}
		}
	}
}
