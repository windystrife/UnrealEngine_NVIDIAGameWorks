// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PhysicsEngine/ShapeElem.h"
#include "SphylElem.generated.h"

class FMaterialRenderProxy;
class FMeshElementCollector;

/** Capsule shape used for collision. Z axis is capsule axis. */
USTRUCT()
struct FKSphylElem : public FKShapeElem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FMatrix TM_DEPRECATED;
	UPROPERTY()
	FQuat Orientation_DEPRECATED;

	/** Position of the capsule's origin */
	UPROPERTY(Category=Capsule, EditAnywhere)
	FVector Center;

	/** Rotation of the capsule */
	UPROPERTY(Category = Capsule, EditAnywhere, meta = (ClampMin = "-360", ClampMax = "360"))
	FRotator Rotation;

	/** Radius of the capsule */
	UPROPERTY(Category= Capsule, EditAnywhere)
	float Radius;

	/** This is of line-segment ie. add Radius to both ends to find total length. */
	UPROPERTY(Category= Capsule, EditAnywhere)
	float Length;

	FKSphylElem()
	: FKShapeElem(EAggCollisionShape::Sphyl)
	, Orientation_DEPRECATED( FQuat::Identity )
	, Center( FVector::ZeroVector )
	, Rotation(FRotator::ZeroRotator)
	, Radius(1), Length(1)

	{

	}

	FKSphylElem( float InRadius, float InLength )
	: FKShapeElem(EAggCollisionShape::Sphyl)
	, Orientation_DEPRECATED( FQuat::Identity )
	, Center( FVector::ZeroVector )
	, Rotation(FRotator::ZeroRotator)
	, Radius(InRadius), Length(InLength)
	{

	}

	void FixupDeprecated( FArchive& Ar );

	friend bool operator==( const FKSphylElem& LHS, const FKSphylElem& RHS )
	{
		return ( LHS.Center == RHS.Center &&
			LHS.Rotation == RHS.Rotation &&
			LHS.Radius == RHS.Radius &&
			LHS.Length == RHS.Length );
	};

	// Utility function that builds an FTransform from the current data
	FTransform GetTransform() const
	{
		return FTransform(Rotation, Center );
	};

	void SetTransform( const FTransform& InTransform )
	{
		ensure(InTransform.IsValid());
		Rotation = InTransform.Rotator();
		Center = InTransform.GetLocation();
	}

	FORCEINLINE float GetVolume(const FVector& Scale) const { float ScaledRadius = Radius * Scale.GetMin(); return PI * FMath::Square(ScaledRadius) * ( 1.3333f * ScaledRadius + (Length * Scale.GetMin())); }

	ENGINE_API void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FColor Color) const;
	ENGINE_API void GetElemSolid(const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy, int32 ViewIndex, FMeshElementCollector& Collector) const;
	ENGINE_API void	DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy) const;
	ENGINE_API FBox CalcAABB(const FTransform& BoneTM, float Scale) const;

	ENGINE_API void ScaleElem(FVector DeltaSize, float MinSize);

	ENGINE_API FKSphylElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const;
	
	/** Returns the scaled radius for this Sphyl, which is determined by the Max scale on X/Y and clamped by half the total length */
	ENGINE_API float GetScaledRadius(const FVector& Scale3D) const;
	/** Returns the scaled length of the cylinder part of the Sphyl **/
	ENGINE_API float GetScaledCylinderLength(const FVector& Scale3D) const;
	/** Returns half of the total scaled length of the Sphyl, which includes the scaled top and bottom caps */
	ENGINE_API float GetScaledHalfLength(const FVector& Scale3D) const;

	/**	
	 * Finds the shortest distance between the element and a world position. Input and output are given in world space
	 * @param	WorldPosition	The point we are trying to get close to
	 * @param	BodyToWorldTM	The transform to convert BodySetup into world space
	 * @return					The distance between WorldPosition and the shape. 0 indicates WorldPosition is inside one of the shapes.
	 */

	ENGINE_API float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const;

	/**	
	 * Finds the closest point on the shape given a world position. Input and output are given in world space
	 * @param	WorldPosition			The point we are trying to get close to
	 * @param	BodyToWorldTM			The transform to convert BodySetup into world space
	 * @param	ClosestWorldPosition	The closest point on the shape in world space
	 * @param	Normal					The normal of the feature associated with ClosestWorldPosition.
	 * @return							The distance between WorldPosition and the shape. 0 indicates WorldPosition is inside the shape.
	 */
	ENGINE_API float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const;

	ENGINE_API static EAggCollisionShape::Type StaticShapeType;
};