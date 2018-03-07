// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperSpriteSceneProxy.h"
#include "Materials/Material.h"
#include "SceneManagement.h"
#include "PhysicsEngine/BodySetup.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "PaperSpriteComponent.h"

//////////////////////////////////////////////////////////////////////////
// FPaperSpriteSceneProxy

FPaperSpriteSceneProxy::FPaperSpriteSceneProxy(UPaperSpriteComponent* InComponent)
	: FPaperRenderSceneProxy(InComponent)
	, BodySetup(InComponent->GetBodySetup())
{
	WireframeColor = InComponent->GetWireframeColor();

	Material = InComponent->GetMaterial(0);
	if (Material == nullptr)
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	AlternateMaterial = InComponent->GetMaterial(1);
	if (AlternateMaterial == nullptr)
	{
		AlternateMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	MaterialSplitIndex = INDEX_NONE;
	MaterialRelevance = InComponent->GetMaterialRelevance(GetScene().GetFeatureLevel());
}

void FPaperSpriteSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if (BodySetup != nullptr)
	{
		// Show 3D physics
		if ((ViewFamily.EngineShowFlags.Collision /*@TODO: && bIsCollisionEnabled*/) && AllowDebugViewmodes())
		{
			if (FMath::Abs(GetLocalToWorld().Determinant()) < SMALL_NUMBER)
			{
				// Catch this here or otherwise GeomTransform below will assert
				// This spams so commented out
				//UE_LOG(LogStaticMesh, Log, TEXT("Zero scaling not supported (%s)"), *StaticMesh->GetPathName());
			}
			else
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						// Make a material for drawing solid collision stuff
						const UMaterial* LevelColorationMaterial = ViewFamily.EngineShowFlags.Lighting 
							? GEngine->ShadedLevelColorationLitMaterial : GEngine->ShadedLevelColorationUnlitMaterial;

						auto CollisionMaterialInstance = new FColoredMaterialRenderProxy(
							LevelColorationMaterial->GetRenderProxy(IsSelected(), IsHovered()),
							WireframeColor
							);

						Collector.RegisterOneFrameMaterialProxy(CollisionMaterialInstance);

						// Draw the sprite body setup.

						// Get transform without scaling.
						FTransform GeomTransform(GetLocalToWorld());

						// In old wireframe collision mode, always draw the wireframe highlighted (selected or not).
						bool bDrawWireSelected = IsSelected();
						if (ViewFamily.EngineShowFlags.Collision)
						{
							bDrawWireSelected = true;
						}

						// Differentiate the color based on bBlockNonZeroExtent.  Helps greatly with skimming a level for optimization opportunities.
						const FColor CollisionColor = FColor(220,149,223,255);

						const bool bUseSeparateColorPerHull = (Owner == nullptr);
						const bool bDrawSolid = false;
						BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(CollisionColor, bDrawWireSelected, IsHovered()).ToFColor(true), CollisionMaterialInstance, bUseSeparateColorPerHull, bDrawSolid, false, ViewIndex, Collector);
					}
				}
			}
		}
	}

	FPaperRenderSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
}

void FPaperSpriteSceneProxy::SetSprite_RenderThread(const FSpriteDrawCallRecord& NewDynamicData, int32 SplitIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_PaperRender_SetSpriteRT);

	BatchedSprites.Empty();
	AlternateBatchedSprites.Empty();

	if (SplitIndex != INDEX_NONE)
	{
		FSpriteDrawCallRecord& Record = *new (BatchedSprites) FSpriteDrawCallRecord;
		FSpriteDrawCallRecord& AltRecord = *new (AlternateBatchedSprites) FSpriteDrawCallRecord;
		
		Record.Color = NewDynamicData.Color;
		Record.Destination = NewDynamicData.Destination;
		Record.BaseTexture = NewDynamicData.BaseTexture;
		Record.AdditionalTextures = NewDynamicData.AdditionalTextures;
		Record.RenderVerts.Append(NewDynamicData.RenderVerts.GetData(), SplitIndex);

		AltRecord.Color = NewDynamicData.Color;
		AltRecord.Destination = NewDynamicData.Destination;
		AltRecord.BaseTexture = NewDynamicData.BaseTexture;
		AltRecord.AdditionalTextures = NewDynamicData.AdditionalTextures;
		AltRecord.RenderVerts.Append(NewDynamicData.RenderVerts.GetData() + SplitIndex, NewDynamicData.RenderVerts.Num() - SplitIndex);
	}
	else
	{
		FSpriteDrawCallRecord& Record = *new (BatchedSprites) FSpriteDrawCallRecord;
		Record = NewDynamicData;
	}
}

void FPaperSpriteSceneProxy::GetDynamicMeshElementsForView(const FSceneView* View, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	if (Material != nullptr)
	{
		GetBatchMesh(View, Material, BatchedSprites, ViewIndex, Collector);
	}

	if ((AlternateMaterial != nullptr) && (AlternateBatchedSprites.Num() > 0))
	{
		GetBatchMesh(View, AlternateMaterial, AlternateBatchedSprites, ViewIndex, Collector);
	}
}
