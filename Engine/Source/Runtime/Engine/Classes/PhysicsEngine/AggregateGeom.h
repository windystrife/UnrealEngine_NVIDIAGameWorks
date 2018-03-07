// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "AggregateGeom.generated.h"

class FMaterialRenderProxy;

/** Container for an aggregate of collision shapes */
USTRUCT()
struct ENGINE_API FKAggregateGeom
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Spheres"))
	TArray<FKSphereElem> SphereElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Boxes"))
	TArray<FKBoxElem> BoxElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Capsules"))
	TArray<FKSphylElem> SphylElems;

	UPROPERTY(VisibleAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Convex Elements"))
	TArray<FKConvexElem> ConvexElems;

	class FKConvexGeomRenderInfo* RenderInfo;

	FKAggregateGeom()
		: RenderInfo(NULL)
	{
	}

	FKAggregateGeom(const FKAggregateGeom& Other)
		: RenderInfo(nullptr)
	{
		CloneAgg(Other);
	}

	const FKAggregateGeom& operator=(const FKAggregateGeom& Other)
	{
		FreeRenderInfo();
		CloneAgg(Other);
		return *this;
	}

	int32 GetElementCount() const
	{
		return SphereElems.Num() + SphylElems.Num() + BoxElems.Num() + ConvexElems.Num();
	}

	int32 GetElementCount(EAggCollisionShape::Type Type) const;

	void EmptyElements()
	{
		BoxElems.Empty();
		ConvexElems.Empty();
		SphylElems.Empty();
		SphereElems.Empty();

		FreeRenderInfo();
	}



	void FixupDeprecated(FArchive& Ar);

	void GetAggGeom(const FTransform& Transform, const FColor Color, const FMaterialRenderProxy* MatInst, bool bPerHullColor, bool bDrawSolid, bool bUseEditorDepthTest, int32 ViewIndex, class FMeshElementCollector& Collector) const;

	/** Release the RenderInfo (if its there) and safely clean up any resources. Call on the game thread. */
	void FreeRenderInfo();

	FBox CalcAABB(const FTransform& Transform) const;

	/**
	* Calculates a tight box-sphere bounds for the aggregate geometry; this is more expensive than CalcAABB
	* (tight meaning the sphere may be smaller than would be required to encompass the AABB, but all individual components lie within both the box and the sphere)
	*
	* @param Output The output box-sphere bounds calculated for this set of aggregate geometry
	*	@param LocalToWorld Transform
	*/
	void CalcBoxSphereBounds(FBoxSphereBounds& Output, const FTransform& LocalToWorld) const;

	/** Returns the volume of this element */
	float GetVolume(const FVector& Scale3D) const;

private:

	/** Helper function for safely copying instances */
	void CloneAgg(const FKAggregateGeom& Other)
	{
		SphereElems = Other.SphereElems;
		BoxElems = Other.BoxElems;
		SphylElems = Other.SphylElems;
		ConvexElems = Other.ConvexElems;
	}
};
