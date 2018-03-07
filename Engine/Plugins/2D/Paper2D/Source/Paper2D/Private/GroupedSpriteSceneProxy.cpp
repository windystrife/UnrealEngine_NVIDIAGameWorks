// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GroupedSpriteSceneProxy.h"
#include "PaperGroupedSpriteComponent.h"
#include "PhysicsEngine/BodySetup.h"

//////////////////////////////////////////////////////////////////////////
// FGroupedSpriteSceneProxy

FSpriteRenderSection& FGroupedSpriteSceneProxy::FindOrAddSection(FSpriteDrawCallRecord& InBatch, UMaterialInterface* InMaterial)
{
	// Check the existing sections, starting with the most recent
	for (int32 SectionIndex = BatchedSections.Num() - 1; SectionIndex >= 0; --SectionIndex)
	{
		FSpriteRenderSection& TestSection = BatchedSections[SectionIndex];

		if (TestSection.Material == InMaterial)
		{
			if (TestSection.BaseTexture == InBatch.BaseTexture)
			{
				if (TestSection.AdditionalTextures == InBatch.AdditionalTextures)
				{
					return TestSection;
				}
			}
		}
	}

	// Didn't find a matching section, create one
	FSpriteRenderSection& NewSection = *(new (BatchedSections) FSpriteRenderSection());
	NewSection.Material = InMaterial;
	NewSection.BaseTexture = InBatch.BaseTexture;
	NewSection.AdditionalTextures = InBatch.AdditionalTextures;
	NewSection.VertexOffset = VertexBuffer.Vertices.Num();

	return NewSection;
}

FGroupedSpriteSceneProxy::FGroupedSpriteSceneProxy(UPaperGroupedSpriteComponent* InComponent)
	: FPaperRenderSceneProxy(InComponent)
	, MyComponent(InComponent)
{
	MaterialRelevance = InComponent->GetMaterialRelevance(GetScene().GetFeatureLevel());

	NumInstances = InComponent->PerInstanceSpriteData.Num();

	const bool bAllowCollisionRendering = AllowDebugViewmodes() && InComponent->IsCollisionEnabled();

	if (bAllowCollisionRendering)
	{
		BodySetupTransforms.Reserve(NumInstances);
		BodySetups.Reserve(NumInstances);
	}

	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		const FSpriteInstanceData InstanceData = InComponent->PerInstanceSpriteData[InstanceIndex];

		UBodySetup* BodySetup = nullptr;
		if (UPaperSprite* SourceSprite = InstanceData.SourceSprite)
		{
			FSpriteDrawCallRecord Record;
			Record.BuildFromSprite(SourceSprite);

			UMaterialInterface* SpriteMaterial = InComponent->GetMaterial(InstanceData.MaterialIndex);

			FSpriteRenderSection& Section = FindOrAddSection(Record, SpriteMaterial);
			
			const int32 NumNewVerts = Record.RenderVerts.Num();
			Section.NumVertices += NumNewVerts;

			const FPackedNormal TangentX = InstanceData.Transform.GetUnitAxis(EAxis::X);
			FPackedNormal TangentZ = InstanceData.Transform.GetUnitAxis(EAxis::Y);
			TangentZ.Vector.W = (InstanceData.Transform.Determinant() < 0.0f) ? 0 : 255;

			const FColor VertColor(InstanceData.VertexColor);
			for (const FVector4& SourceVert : Record.RenderVerts)
			{
				const FVector LocalPos((PaperAxisX * SourceVert.X) + (PaperAxisY * SourceVert.Y));
				const FVector ComponentSpacePos = InstanceData.Transform.TransformPosition(LocalPos);
				const FVector2D UV(SourceVert.Z, SourceVert.W);

				new (VertexBuffer.Vertices) FPaperSpriteVertex(ComponentSpacePos, UV, VertColor, TangentX, TangentZ);
			}

			BodySetup = SourceSprite->BodySetup;
		}

		if (bAllowCollisionRendering)
		{
			BodySetupTransforms.Add(InstanceData.Transform);
			BodySetups.Add(BodySetup);
		}
	}
	
	if (VertexBuffer.Vertices.Num() > 0)
	{
		// Init the vertex factory
		MyVertexFactory.Init(&VertexBuffer);

		// Enqueue initialization of render resources
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&MyVertexFactory);
	}
}

void FGroupedSpriteSceneProxy::DebugDrawCollision(const FSceneView* View, int32 ViewIndex, FMeshElementCollector& Collector, bool bDrawSolid) const
{
	for (int32 InstanceIndex = 0; InstanceIndex < BodySetups.Num(); ++InstanceIndex)
	{
		if (UBodySetup* BodySetup = BodySetups[InstanceIndex].Get())
		{
			const FColor CollisionColor = FColor(157, 149, 223, 255);
			const FMatrix GeomTransform = BodySetupTransforms[InstanceIndex] * GetLocalToWorld();
			DebugDrawBodySetup(View, ViewIndex, Collector, BodySetup, GeomTransform, CollisionColor, bDrawSolid);
		}
	}
}
