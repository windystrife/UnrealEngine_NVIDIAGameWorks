// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialEditor/MaterialEditorMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"

FBoxSphereBounds UMaterialEditorMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (GetStaticMesh())
	{
		// Graphics bounds.
		FBoxSphereBounds NewBounds = GetStaticMesh()->GetBounds().TransformBy(LocalToWorld);

		// Add bounds of collision geometry (if present).
		if (GetStaticMesh()->BodySetup)
		{
			// Use more accurate but expensive bounds function for the material editor only
			FBoxSphereBounds AggGeomBounds;
			GetStaticMesh()->BodySetup->AggGeom.CalcBoxSphereBounds(AggGeomBounds, LocalToWorld);
			if (AggGeomBounds.SphereRadius != 0.f)
			{
				NewBounds = Union(NewBounds,AggGeomBounds);
			}
		}

		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}
