// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Collision/CollisionConversions.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "CustomPhysXPayload.h"

#if WITH_PHYSX
#include "Collision/PhysXCollision.h"
#include "Collision/CollisionDebugDrawing.h"
#include "Components/LineBatchComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"

// Used to place overlaps into a TMap when deduplicating them
struct FOverlapKey
{
	UPrimitiveComponent* Component;
	int32 ComponentIndex;

	FOverlapKey(UPrimitiveComponent* InComponent, int32 InComponentIndex)
		: Component(InComponent)
		, ComponentIndex(InComponentIndex)
	{
	}

	friend bool operator==(const FOverlapKey& X, const FOverlapKey& Y)
	{
		return (X.ComponentIndex == Y.ComponentIndex) && (X.Component == Y.Component);
	}
};

uint32 GetTypeHash(const FOverlapKey& Key)
{
	return GetTypeHash(Key.Component) ^ GetTypeHash(Key.ComponentIndex);
}


#define DRAW_OVERLAPPING_TRIS (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))
extern int32 CVarShowInitialOverlaps;

// Sentinel for invalid query results.
static const PxQueryHit InvalidQueryHit;

FORCEINLINE_DEBUGGABLE bool IsInvalidFaceIndex(PxU32 faceIndex)
{
	checkfSlow(InvalidQueryHit.faceIndex == 0xFFFFffff, TEXT("Engine code needs fixing: PhysX invalid face index sentinel has changed or is not part of default PxQueryHit!"));
	return (faceIndex == 0xFFFFffff);
}

// Forward declare, I don't want to move the entire function right now or we lose change history.
static bool ConvertOverlappedShapeToImpactHit(const UWorld* World, const PxLocationHit& PHit, const FVector& StartLoc, const FVector& EndLoc, FHitResult& OutResult, const PxGeometry& Geom, const PxTransform& QueryTM, const PxFilterData& QueryFilter, bool bReturnPhysMat);

DECLARE_CYCLE_STAT(TEXT("ConvertQueryHit"), STAT_ConvertQueryImpactHit, STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("ConvertOverlapToHit"), STAT_CollisionConvertOverlapToHit, STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("ConvertOverlap"), STAT_CollisionConvertOverlap, STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("SetHitResultFromShapeAndFaceIndex"), STAT_CollisionSetHitResultFromShapeAndFaceIndex, STATGROUP_Collision);

#define ENABLE_CHECK_HIT_NORMAL  (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))

#if ENABLE_CHECK_HIT_NORMAL
/* Validate Normal of OutResult. We're on hunt for invalid normal */
static void CheckHitResultNormal(const FHitResult& OutResult, const TCHAR* Message, const FVector& Start=FVector::ZeroVector, const FVector& End = FVector::ZeroVector, const PxGeometry* const Geom=NULL)
{
	if(!OutResult.bStartPenetrating && !OutResult.Normal.IsNormalized())
	{
		UE_LOG(LogPhysics, Warning, TEXT("(%s) Non-normalized OutResult.Normal from hit conversion: %s (Component- %s)"), Message, *OutResult.Normal.ToString(), *GetNameSafe(OutResult.Component.Get()));
		UE_LOG(LogPhysics, Warning, TEXT("Start Loc(%s), End Loc(%s), Hit Loc(%s), ImpactNormal(%s)"), *Start.ToString(), *End.ToString(), *OutResult.Location.ToString(), *OutResult.ImpactNormal.ToString() );
		if (Geom != NULL)
		{
			if (Geom->getType() == PxGeometryType::eCAPSULE)
			{
				const PxCapsuleGeometry * Capsule = (PxCapsuleGeometry*)Geom;
				UE_LOG(LogPhysics, Warning, TEXT("Capsule radius (%f), Capsule Halfheight (%f)"), Capsule->radius, Capsule->halfHeight);
			}
		}
		ensure(OutResult.Normal.IsNormalized());
	}
}
#endif // ENABLE_CHECK_HIT_NORMAL


static FORCEINLINE bool PxQuatIsIdentity(PxQuat const& Q)
{
	return
		Q.x == 0.f &&
		Q.y == 0.f &&
		Q.z == 0.f &&
		Q.w == 1.f;
}


/** Helper to transform a normal when non-uniform scale is present. */
static PxVec3 TransformNormalToShapeSpace(const PxMeshScale& meshScale, const PxVec3& nIn)
{
	// Uniform scale makes this unnecessary
	if (meshScale.scale.x == meshScale.scale.y &&
		meshScale.scale.x == meshScale.scale.z)
	{
		return nIn;
	}
	
	if (PxQuatIsIdentity(meshScale.rotation))
	{
		// Inverse transpose: inverse is 1/scale, transpose = original when rotation is identity.
		const PxVec3 tmp = PxVec3(nIn.x / meshScale.scale.x, nIn.y / meshScale.scale.y, nIn.z / meshScale.scale.z);
		const PxReal denom = 1.0f / tmp.magnitude();
		return tmp * denom;
	}
	else
	{
		const PxMat33 rot(meshScale.rotation);
		const PxMat33 diagonal = PxMat33::createDiagonal(meshScale.scale);
		const PxMat33 vertex2Shape = (rot.getTranspose() * diagonal) * rot;

		const PxMat33 shape2Vertex = vertex2Shape.getInverse();
		const PxVec3 tmp = shape2Vertex.transformTranspose(nIn);
		const PxReal denom = 1.0f / tmp.magnitude();
		return tmp * denom;
	}
}

static FVector FindSimpleOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	// We don't compute anything special
	return InNormal;
}

static FVector FindBoxOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	// We require normal info for our algorithm.
	const bool bNormalData = (PHit.flags & PxHitFlag::eNORMAL);
	if (!bNormalData)
	{
		return InNormal;
	}

	PxBoxGeometry PxBoxGeom;
	const bool bReadGeomSuccess = PHit.shape->getBoxGeometry(PxBoxGeom);
	check(bReadGeomSuccess); // This function should only be used for box geometry

	const PxTransform LocalToWorld = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);
	
	// Find which faces were included in the contact normal, and for multiple faces, use the one most opposing the sweep direction.
	const PxVec3 ContactNormalLocal = LocalToWorld.rotateInv(PHit.normal);
	const float* ContactNormalLocalPtr = &ContactNormalLocal.x;
	const PxVec3 TraceDirDenormWorld = U2PVector(TraceDirectionDenorm);
	const float* TraceDirDenormWorldPtr = &TraceDirDenormWorld.x;
	const PxVec3 TraceDirDenormLocal = LocalToWorld.rotateInv(TraceDirDenormWorld);
	const float* TraceDirDenormLocalPtr = &TraceDirDenormLocal.x;

	PxVec3 BestLocalNormal(ContactNormalLocal);
	float* BestLocalNormalPtr = &BestLocalNormal.x;
	float BestOpposingDot = FLT_MAX;

	for (int32 i=0; i < 3; i++)
	{
		// Select axis of face to compare to, based on normal.
		if (ContactNormalLocalPtr[i] > KINDA_SMALL_NUMBER)
		{
			const float TraceDotFaceNormal = TraceDirDenormLocalPtr[i]; // TraceDirDenormLocal.dot(BoxFaceNormal)
			if (TraceDotFaceNormal < BestOpposingDot)
			{
				BestOpposingDot = TraceDotFaceNormal;
				BestLocalNormal = PxVec3(0.f);
				BestLocalNormalPtr[i] = 1.f;
			}
		}
		else if (ContactNormalLocalPtr[i] < -KINDA_SMALL_NUMBER)
		{
			const float TraceDotFaceNormal = -TraceDirDenormLocalPtr[i]; // TraceDirDenormLocal.dot(BoxFaceNormal)
			if (TraceDotFaceNormal < BestOpposingDot)
			{
				BestOpposingDot = TraceDotFaceNormal;
				BestLocalNormal = PxVec3(0.f);
				BestLocalNormalPtr[i] = -1.f;
			}
		}
	}

	// Fill in result
	const PxVec3 WorldNormal = LocalToWorld.rotate(BestLocalNormal);
	return P2UVector(WorldNormal);
}

static FVector FindHeightFieldOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	if (IsInvalidFaceIndex(PHit.faceIndex))
	{
		return InNormal;
	}

	PxHeightFieldGeometry PHeightFieldGeom;
	const bool bReadGeomSuccess = PHit.shape->getHeightFieldGeometry(PHeightFieldGeom);
	check(bReadGeomSuccess);	//we should only call this function when we have a heightfield
	if (PHeightFieldGeom.heightField)
	{
		const PxU32 TriIndex = PHit.faceIndex;
		const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);

		PxTriangle Tri;
		PxMeshQuery::getTriangle(PHeightFieldGeom, PShapeWorldPose, TriIndex, Tri);

		PxVec3 TriNormal;
		Tri.normal(TriNormal);
		return P2UVector(TriNormal);
	}

	return InNormal;
}

static FVector FindConvexMeshOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	if (IsInvalidFaceIndex(PHit.faceIndex))
	{
		return InNormal;
	}

	PxConvexMeshGeometry PConvexMeshGeom;
	bool bSuccess = PHit.shape->getConvexMeshGeometry(PConvexMeshGeom);
	check(bSuccess);	//should only call this function when we have a convex mesh

	if (PConvexMeshGeom.convexMesh)
	{
		check(PHit.faceIndex < PConvexMeshGeom.convexMesh->getNbPolygons());

		const PxU32 PolyIndex = PHit.faceIndex;
		PxHullPolygon PPoly;
		bool bSuccessData = PConvexMeshGeom.convexMesh->getPolygonData(PolyIndex, PPoly);
		if (bSuccessData)
		{
			// Account for non-uniform scale in local space normal.
			const PxVec3 PPlaneNormal(PPoly.mPlane[0], PPoly.mPlane[1], PPoly.mPlane[2]);
			const PxVec3 PLocalPolyNormal = TransformNormalToShapeSpace(PConvexMeshGeom.scale, PPlaneNormal.getNormalized());

			// Convert to world space
			const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);
			const PxVec3 PWorldPolyNormal = PShapeWorldPose.rotate(PLocalPolyNormal);
			const FVector OutNormal = P2UVector(PWorldPolyNormal);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (!OutNormal.IsNormalized())
			{
				UE_LOG(LogPhysics, Warning, TEXT("Non-normalized Normal (Hit shape is ConvexMesh): %s (LocalPolyNormal:%s)"), *OutNormal.ToString(), *P2UVector(PLocalPolyNormal).ToString());
				UE_LOG(LogPhysics, Warning, TEXT("WorldTransform \n: %s"), *P2UTransform(PShapeWorldPose).ToString());
			}
#endif
			return OutNormal;
		}
	}

	return InNormal;
}

static FVector FindTriMeshOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	if (IsInvalidFaceIndex(PHit.faceIndex))
	{
		return InNormal;
	}

	PxTriangleMeshGeometry PTriMeshGeom;
	bool bSuccess = PHit.shape->getTriangleMeshGeometry(PTriMeshGeom);
	check(bSuccess);	//this function should only be called when we have a trimesh

	if (PTriMeshGeom.triangleMesh)
	{
		check(PHit.faceIndex < PTriMeshGeom.triangleMesh->getNbTriangles());

		const PxU32 TriIndex = PHit.faceIndex;
		const void* Triangles = PTriMeshGeom.triangleMesh->getTriangles();

		// Grab triangle indices that we hit
		int32 I0, I1, I2;

		if (PTriMeshGeom.triangleMesh->getTriangleMeshFlags() & PxTriangleMeshFlag::e16_BIT_INDICES)
		{
			PxU16* P16BitIndices = (PxU16*)Triangles;
			I0 = P16BitIndices[(TriIndex * 3) + 0];
			I1 = P16BitIndices[(TriIndex * 3) + 1];
			I2 = P16BitIndices[(TriIndex * 3) + 2];
		}
		else
		{
			PxU32* P32BitIndices = (PxU32*)Triangles;
			I0 = P32BitIndices[(TriIndex * 3) + 0];
			I1 = P32BitIndices[(TriIndex * 3) + 1];
			I2 = P32BitIndices[(TriIndex * 3) + 2];
		}

		// Get verts we hit (local space)
		const PxVec3* PVerts = PTriMeshGeom.triangleMesh->getVertices();
		const PxVec3 V0 = PVerts[I0];
		const PxVec3 V1 = PVerts[I1];
		const PxVec3 V2 = PVerts[I2];

		// Find normal of triangle (local space), and account for non-uniform scale
		const PxVec3 PTempNormal = ((V1 - V0).cross(V2 - V0)).getNormalized();
		const PxVec3 PLocalTriNormal = TransformNormalToShapeSpace(PTriMeshGeom.scale, PTempNormal);

		// Convert to world space
		const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);
		const PxVec3 PWorldTriNormal = PShapeWorldPose.rotate(PLocalTriNormal);
		FVector OutNormal = P2UVector(PWorldTriNormal);

		if (PTriMeshGeom.meshFlags & PxMeshGeometryFlag::eDOUBLE_SIDED)
		{
			//double sided mesh so we need to consider direction of query
			const float sign = FVector::DotProduct(OutNormal, TraceDirectionDenorm) > 0.f ? -1.f : 1.f;
			OutNormal *= sign;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!OutNormal.IsNormalized())
		{
			UE_LOG(LogPhysics, Warning, TEXT("Non-normalized Normal (Hit shape is TriangleMesh): %s (V0:%s, V1:%s, V2:%s)"), *OutNormal.ToString(), *P2UVector(V0).ToString(), *P2UVector(V1).ToString(), *P2UVector(V2).ToString());
			UE_LOG(LogPhysics, Warning, TEXT("WorldTransform \n: %s"), *P2UTransform(PShapeWorldPose).ToString());
		}
#endif
		return OutNormal;
	}

	return InNormal;
}

/**
 * Util to find the normal of the face that we hit. Will use faceIndex from the hit if possible.
 * @param PHit - incoming hit from PhysX
 * @param TraceDirectionDenorm - direction of sweep test (not normalized)
 * @param InNormal - default value in case no new normal is computed.
 * @return New normal we compute for geometry.
 */
static FVector FindGeomOpposingNormal(PxGeometryType::Enum QueryGeomType, const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	// TODO: can we support other shapes here as well?
	if (QueryGeomType == PxGeometryType::eCAPSULE || QueryGeomType == PxGeometryType::eSPHERE)
	{
		PxGeometryType::Enum GeomType = PHit.shape->getGeometryType();
		switch (GeomType)
		{
		case PxGeometryType::eSPHERE:
		case PxGeometryType::eCAPSULE:		return FindSimpleOpposingNormal(PHit, TraceDirectionDenorm, InNormal);
		case PxGeometryType::eBOX:			return FindBoxOpposingNormal(PHit, TraceDirectionDenorm, InNormal);
		case PxGeometryType::eCONVEXMESH:	return FindConvexMeshOpposingNormal(PHit, TraceDirectionDenorm, InNormal);
		case PxGeometryType::eHEIGHTFIELD:	return FindHeightFieldOpposingNormal(PHit, TraceDirectionDenorm, InNormal);
		case PxGeometryType::eTRIANGLEMESH:	return FindTriMeshOpposingNormal(PHit, TraceDirectionDenorm, InNormal);
		default: check(false);	//unsupported geom type
		}
	}

	return InNormal;
}

/** Set info in the HitResult (Actor, Component, PhysMaterial, BoneName, Item) based on the supplied shape and face index */
static void SetHitResultFromShapeAndFaceIndex(const PxShape* PShape,  const PxRigidActor* PActor, const uint32 FaceIndex, FHitResult& OutResult, bool bReturnPhysMat)
{
	SCOPE_CYCLE_COUNTER(STAT_CollisionSetHitResultFromShapeAndFaceIndex);
	
	UPrimitiveComponent* OwningComponent = nullptr;
	if(const FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(PActor->userData))
	{
		BodyInst = BodyInst->GetOriginalBodyInstance(PShape);

		//Normal case where we hit a body
		OutResult.Item = BodyInst->InstanceBodyIndex;
		const UBodySetup* BodySetup = BodyInst->BodySetup.Get();	//this data should be immutable at runtime so ok to check from worker thread.
		if (BodySetup)
		{
			OutResult.BoneName = BodySetup->BoneName;
		}

		OwningComponent = BodyInst->OwnerComponent.Get();
	}
	else if(const FCustomPhysXPayload* CustomPayload = FPhysxUserData::Get<FCustomPhysXPayload>(PShape->userData))
	{
		//Custom payload case
		OwningComponent = CustomPayload->GetOwningComponent().Get();
		if(OwningComponent->bMultiBodyOverlap)
		{
			OutResult.Item = CustomPayload->GetItemIndex();
			OutResult.BoneName = CustomPayload->GetBoneName();
		}
		else
		{
			OutResult.Item = INDEX_NONE;
			OutResult.BoneName = NAME_None;
		
		}
		
	}
	else
	{
		ensureMsgf(false, TEXT("SetHitResultFromShapeAndFaceIndex hit shape with invalid userData"));
	}

	OutResult.PhysMaterial = nullptr;

	// Grab actor/component
	if( OwningComponent )
	{
		OutResult.Actor = OwningComponent->GetOwner();
		OutResult.Component = OwningComponent;

		if (bReturnPhysMat)
		{
			// This function returns the single material in all cases other than trimesh or heightfield
			if(PxMaterial* PxMat = PShape->getMaterialFromInternalFaceIndex(FaceIndex))
			{
				OutResult.PhysMaterial = FPhysxUserData::Get<UPhysicalMaterial>(PxMat->userData);
			}
		}
	}

	OutResult.FaceIndex = INDEX_NONE;
}

EConvertQueryResult ConvertQueryImpactHit(const UWorld* World, const PxLocationHit& PHit, FHitResult& OutResult, float CheckLength, const PxFilterData& QueryFilter, const FVector& StartLoc, const FVector& EndLoc, const PxGeometry* const Geom, const PxTransform& QueryTM, bool bReturnFaceIndex, bool bReturnPhysMat)
{
	SCOPE_CYCLE_COUNTER(STAT_ConvertQueryImpactHit);

#if WITH_EDITOR
	if(bReturnFaceIndex && World->IsGameWorld())
	{
		if(!ensure(UPhysicsSettings::Get()->bSuppressFaceRemapTable == false))
		{
			UE_LOG(LogPhysics, Error, TEXT("A scene query is relying on face indices, but bSuppressFaceRemapTable is true."));
			bReturnFaceIndex = false;
		}
	}
#endif

	checkSlow(PHit.flags & PxHitFlag::eDISTANCE);
	const bool bInitialOverlap = PHit.hadInitialOverlap();
	if (bInitialOverlap && Geom != nullptr)
	{
		ConvertOverlappedShapeToImpactHit(World, PHit, StartLoc, EndLoc, OutResult, *Geom, QueryTM, QueryFilter, bReturnPhysMat);
		return EConvertQueryResult::Valid;
	}

	// See if this is a 'blocking' hit
	const PxFilterData PShapeFilter = PHit.shape->getQueryFilterData();
	const PxQueryHitType::Enum HitType = FPxQueryFilterCallback::CalcQueryHitType(QueryFilter, PShapeFilter);
	OutResult.bBlockingHit = (HitType == PxQueryHitType::eBLOCK); 
	OutResult.bStartPenetrating = bInitialOverlap;

	// calculate the hit time
	const float HitTime = PHit.distance/CheckLength;
	OutResult.Time = HitTime;
	OutResult.Distance = PHit.distance;

	// figure out where the the "safe" location for this shape is by moving from the startLoc toward the ImpactPoint
	const FVector TraceStartToEnd = EndLoc - StartLoc;
	const FVector SafeLocationToFitShape = StartLoc + (HitTime * TraceStartToEnd);
	OutResult.Location = SafeLocationToFitShape;

	const bool bUsePxPoint = ((PHit.flags & PxHitFlag::ePOSITION) && !bInitialOverlap);
	if (bUsePxPoint && !PHit.position.isFinite())
	{
#if ENABLE_NAN_DIAGNOSTIC
		SetHitResultFromShapeAndFaceIndex(PHit.shape, PHit.actor, PHit.faceIndex, OutResult, bReturnPhysMat);
		UE_LOG(LogCore, Error, TEXT("ConvertQueryImpactHit() NaN details:\n>> Actor:%s (%s)\n>> Component:%s\n>> Item:%d\n>> BoneName:%s\n>> Time:%f\n>> Distance:%f\n>> Location:%s\n>> bIsBlocking:%d\n>> bStartPenetrating:%d"),
			*GetNameSafe(OutResult.GetActor()), OutResult.Actor.IsValid() ? *OutResult.GetActor()->GetPathName() : TEXT("no path"),
			*GetNameSafe(OutResult.GetComponent()), OutResult.Item, *OutResult.BoneName.ToString(),
			OutResult.Time, OutResult.Distance, *OutResult.Location.ToString(), OutResult.bBlockingHit ? 1 : 0, OutResult.bStartPenetrating ? 1 : 0);
#endif // ENABLE_NAN_DIAGNOSTIC

		OutResult.Reset();
		logOrEnsureNanError(TEXT("ConvertQueryImpactHit() received NaN/Inf for position: %.2f %.2f %.2f"), PHit.position.x, PHit.position.y, PHit.position.z);
		return EConvertQueryResult::Invalid;
	}

	OutResult.ImpactPoint = bUsePxPoint ? P2UVector(PHit.position) : StartLoc;
	
	// Caution: we may still have an initial overlap, but with null Geom. This is the case for RayCast results.
	const bool bUsePxNormal = ((PHit.flags & PxHitFlag::eNORMAL) && !bInitialOverlap);
	if (bUsePxNormal && !PHit.normal.isFinite())
	{
#if ENABLE_NAN_DIAGNOSTIC
		SetHitResultFromShapeAndFaceIndex(PHit.shape, PHit.actor, PHit.faceIndex, OutResult, bReturnPhysMat);
		UE_LOG(LogCore, Error, TEXT("ConvertQueryImpactHit() NaN details:\n>> Actor:%s (%s)\n>> Component:%s\n>> Item:%d\n>> BoneName:%s\n>> Time:%f\n>> Distance:%f\n>> Location:%s\n>> bIsBlocking:%d\n>> bStartPenetrating:%d"),
			*GetNameSafe(OutResult.GetActor()), OutResult.Actor.IsValid() ? *OutResult.GetActor()->GetPathName() : TEXT("no path"),
			*GetNameSafe(OutResult.GetComponent()), OutResult.Item, *OutResult.BoneName.ToString(),
			OutResult.Time, OutResult.Distance, *OutResult.Location.ToString(), OutResult.bBlockingHit ? 1 : 0, OutResult.bStartPenetrating ? 1 : 0);
#endif // ENABLE_NAN_DIAGNOSTIC

		OutResult.Reset();
		logOrEnsureNanError(TEXT("ConvertQueryImpactHit() received NaN/Inf for normal: %.2f %.2f %.2f"), PHit.normal.x, PHit.normal.y, PHit.normal.z);
		return EConvertQueryResult::Invalid;
	}

	FVector Normal = bUsePxNormal ? P2UVector(PHit.normal).GetSafeNormal() : -TraceStartToEnd.GetSafeNormal();
	OutResult.Normal = Normal;
	OutResult.ImpactNormal = Normal;

	OutResult.TraceStart = StartLoc;
	OutResult.TraceEnd = EndLoc;


#if ENABLE_CHECK_HIT_NORMAL
	CheckHitResultNormal(OutResult, TEXT("Invalid Normal from ConvertQueryImpactHit"), StartLoc, EndLoc, Geom);
#endif // ENABLE_CHECK_HIT_NORMAL

	if (bUsePxNormal && !Normal.IsNormalized())
	{
		// TraceStartToEnd should never be zero, because of the length restriction in the raycast and sweep tests.
		Normal = -TraceStartToEnd.GetSafeNormal();
		OutResult.Normal = Normal;
		OutResult.ImpactNormal = Normal;
	}

	const PxGeometryType::Enum SweptGeometryType = Geom ? Geom->getType() : PxGeometryType::eINVALID;
	OutResult.ImpactNormal = FindGeomOpposingNormal(SweptGeometryType, PHit, TraceStartToEnd, Normal);

	// Fill in Actor, Component, material, etc.
	SetHitResultFromShapeAndFaceIndex(PHit.shape, PHit.actor, PHit.faceIndex, OutResult, bReturnPhysMat);

	PxGeometryType::Enum PGeomType = PHit.shape->getGeometryType();

	if(PGeomType == PxGeometryType::eHEIGHTFIELD)
	{
		// Lookup physical material for heightfields
		if (bReturnPhysMat && PHit.faceIndex != InvalidQueryHit.faceIndex)
		{
			PxMaterial* HitMaterial = PHit.shape->getMaterialFromInternalFaceIndex(PHit.faceIndex);
			if (HitMaterial != NULL)
			{
				OutResult.PhysMaterial = FPhysxUserData::Get<UPhysicalMaterial>(HitMaterial->userData);
			}
		}
	}
	else if (bReturnFaceIndex && PGeomType == PxGeometryType::eTRIANGLEMESH)
	{
		PxTriangleMeshGeometry PTriMeshGeom;
		if(	PHit.shape->getTriangleMeshGeometry(PTriMeshGeom) && 
			PTriMeshGeom.triangleMesh != NULL &&
			PHit.faceIndex < PTriMeshGeom.triangleMesh->getNbTriangles() )
		{
			if (const PxU32* TriangleRemap = PTriMeshGeom.triangleMesh->getTrianglesRemap())
			{
				OutResult.FaceIndex	= TriangleRemap[PHit.faceIndex];
			}
		}
	}

	return EConvertQueryResult::Valid;
}

EConvertQueryResult ConvertRaycastResults(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, PxRaycastHit* Hits, float CheckLength, const PxFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, bool bReturnFaceIndex, bool bReturnPhysMat)
{
	OutHits.Reserve(OutHits.Num() + NumHits);
	EConvertQueryResult ConvertResult = EConvertQueryResult::Valid;
	bool bHadBlockingHit = false;

	PxTransform PStartTM(U2PVector(StartLoc));
	for(int32 i=0; i<NumHits; i++)
	{
		FHitResult& NewResult = OutHits[OutHits.Emplace()];
		const PxRaycastHit& PHit = Hits[i];

		if (ConvertQueryImpactHit(World, PHit, NewResult, CheckLength, QueryFilter, StartLoc, EndLoc, NULL, PStartTM, bReturnFaceIndex, bReturnPhysMat) == EConvertQueryResult::Valid)
		{
			bHadBlockingHit |= NewResult.bBlockingHit;
		}
		else
		{
			// Reject invalid result (this should be rare). Remove from the results.
			OutHits.Pop(/*bAllowShrinking=*/ false);
			ConvertResult = EConvertQueryResult::Invalid;
		}
	}

	// Sort results from first to last hit
	OutHits.Sort( FCompareFHitResultTime() );
	OutHasValidBlockingHit = bHadBlockingHit;
	return ConvertResult;
}

EConvertQueryResult AddSweepResults(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, PxSweepHit* Hits, float CheckLength, const PxFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, const PxGeometry& Geom, const PxTransform& QueryTM, float MaxDistance, bool bReturnFaceIndex, bool bReturnPhysMat)
{
	OutHits.Reserve(OutHits.Num() + NumHits);
	EConvertQueryResult ConvertResult = EConvertQueryResult::Valid;
	bool bHadBlockingHit = false;
	const PxVec3 PDir = U2PVector((EndLoc - StartLoc).GetSafeNormal());

	for(int32 i=0; i<NumHits; i++)
	{
		PxSweepHit& PHit = Hits[i];
		checkSlow(PHit.flags & PxHitFlag::eDISTANCE);
		if(PHit.distance <= MaxDistance)
		{
			PHit.faceIndex = FindFaceIndex(PHit, PDir);

			FHitResult& NewResult = OutHits[OutHits.AddDefaulted()];
			if (ConvertQueryImpactHit(World, PHit, NewResult, CheckLength, QueryFilter, StartLoc, EndLoc, &Geom, QueryTM, bReturnFaceIndex, bReturnPhysMat) == EConvertQueryResult::Valid)
			{
				bHadBlockingHit |= NewResult.bBlockingHit;
			}
			else
			{
				// Reject invalid result (this should be rare). Remove from the results.
				OutHits.Pop(/*bAllowShrinking=*/ false);
				ConvertResult = EConvertQueryResult::Invalid;
			}
			
		}
	}

	// Sort results from first to last hit
	OutHits.Sort( FCompareFHitResultTime() );
	OutHasValidBlockingHit = bHadBlockingHit;
	return ConvertResult;
}

/* Function to find the best normal from the list of triangles that are overlapping our geom. */
template<typename GeomType>
FVector FindBestOverlappingNormal(const UWorld* World, const PxGeometry& Geom, const PxTransform& QueryTM, const GeomType& ShapeGeom, const PxTransform& PShapeWorldPose, PxU32* HitTris, int32 NumTrisHit, bool bCanDrawOverlaps = false)
{
#if DRAW_OVERLAPPING_TRIS
	const float Lifetime = 5.f;
	bCanDrawOverlaps &= World && World->IsGameWorld() && World->PersistentLineBatcher && (World->PersistentLineBatcher->BatchedLines.Num() < 2048);
	if (bCanDrawOverlaps)
	{
		TArray<FOverlapResult> Overlaps;
		DrawGeomOverlaps(World, Geom, QueryTM, Overlaps, Lifetime);
	}
	const FLinearColor LineColor = FLinearColor::Green;
	const FLinearColor NormalColor = FLinearColor::Red;
	const FLinearColor PointColor = FLinearColor::Yellow;
#endif // DRAW_OVERLAPPING_TRIS

	// Track the best triangle plane distance
	float BestPlaneDist = -BIG_NUMBER;
	FVector BestPlaneNormal(0, 0, 1);
	// Iterate over triangles
	for (int32 TriIdx = 0; TriIdx < NumTrisHit; TriIdx++)
	{
		PxTriangle Tri;
		PxMeshQuery::getTriangle(ShapeGeom, PShapeWorldPose, HitTris[TriIdx], Tri);

		const FVector A = P2UVector(Tri.verts[0]);
		const FVector B = P2UVector(Tri.verts[1]);
		const FVector C = P2UVector(Tri.verts[2]);

		FVector TriNormal = ((B - A) ^ (C - A));
		TriNormal = TriNormal.GetSafeNormal();

		const FPlane TriPlane(A, TriNormal);

		const FVector QueryCenter = P2UVector(QueryTM.p);
		const float DistToPlane = TriPlane.PlaneDot(QueryCenter);

		if (DistToPlane > BestPlaneDist)
		{
			BestPlaneDist = DistToPlane;
			BestPlaneNormal = TriNormal;
		}

#if DRAW_OVERLAPPING_TRIS
		if (bCanDrawOverlaps && World && World->PersistentLineBatcher && World->PersistentLineBatcher->BatchedLines.Num() < 2048)
		{
			static const float LineThickness = 0.9f;
			static const float NormalThickness = 0.75f;
			static const float PointThickness = 5.0f;
			World->PersistentLineBatcher->DrawLine(A, B, LineColor, SDPG_Foreground, LineThickness, Lifetime);
			World->PersistentLineBatcher->DrawLine(B, C, LineColor, SDPG_Foreground, LineThickness, Lifetime);
			World->PersistentLineBatcher->DrawLine(C, A, LineColor, SDPG_Foreground, LineThickness, Lifetime);
			const FVector Centroid((A + B + C) / 3.f);
			World->PersistentLineBatcher->DrawLine(Centroid, Centroid + (35.0f*TriNormal), NormalColor, SDPG_Foreground, NormalThickness, Lifetime);
			World->PersistentLineBatcher->DrawPoint(Centroid + (35.0f*TriNormal), NormalColor, PointThickness, SDPG_Foreground, Lifetime);
			World->PersistentLineBatcher->DrawPoint(A, PointColor, PointThickness, SDPG_Foreground, Lifetime);
			World->PersistentLineBatcher->DrawPoint(B, PointColor, PointThickness, SDPG_Foreground, Lifetime);
			World->PersistentLineBatcher->DrawPoint(C, PointColor, PointThickness, SDPG_Foreground, Lifetime);
		}
#endif // DRAW_OVERLAPPING_TRIS
	}

	return BestPlaneNormal;
}



static bool ComputeInflatedMTD_Internal(const float MtdInflation, const PxLocationHit& PHit, FHitResult& OutResult, const PxTransform& QueryTM, const PxGeometry& Geom, const PxTransform& PShapeWorldPose)
{
	PxGeometry* InflatedGeom = NULL;

	PxVec3 PxMtdNormal(0.f);
	PxF32 PxMtdDepth = 0.f;
	PxGeometryHolder Holder = PHit.shape->getGeometry();
	const PxGeometry& POtherGeom = Holder.any();
	const bool bMtdResult = PxGeometryQuery::computePenetration(PxMtdNormal, PxMtdDepth, Geom, QueryTM, POtherGeom, PShapeWorldPose);
	if (bMtdResult)
	{
		if (PxMtdNormal.isFinite())
		{
			OutResult.ImpactNormal = P2UVector(PxMtdNormal);
			OutResult.PenetrationDepth = FMath::Max(FMath::Abs(PxMtdDepth) - MtdInflation, 0.f) + KINDA_SMALL_NUMBER;
			return true;
		}
		else
		{
			UE_LOG(LogPhysics, Verbose, TEXT("Warning: ComputeInflatedMTD_Internal: MTD returned NaN :( normal: (X:%f, Y:%f, Z:%f)"), PxMtdNormal.x, PxMtdNormal.y, PxMtdNormal.z);
		}
	}

	return false;
}


// Compute depenetration vector and distance if possible with a slightly larger geometry
static bool ComputeInflatedMTD(const float MtdInflation, const PxLocationHit& PHit, FHitResult& OutResult, const PxTransform& QueryTM, const PxGeometry& Geom, const PxTransform& PShapeWorldPose)
{
	switch(Geom.getType())
	{
		case PxGeometryType::eCAPSULE:
		{
			const PxCapsuleGeometry* InCapsule = static_cast<const PxCapsuleGeometry*>(&Geom);
			PxCapsuleGeometry InflatedCapsule(InCapsule->radius + MtdInflation, InCapsule->halfHeight); // don't inflate halfHeight, radius is added all around.
			return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedCapsule, PShapeWorldPose);
		}

		case PxGeometryType::eBOX:
		{
			const PxBoxGeometry* InBox = static_cast<const PxBoxGeometry*>(&Geom);
			PxBoxGeometry InflatedBox(InBox->halfExtents + PxVec3(MtdInflation));
			return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedBox, PShapeWorldPose);
		}

		case PxGeometryType::eSPHERE:
		{
			const PxSphereGeometry* InSphere = static_cast<const PxSphereGeometry*>(&Geom);
			PxSphereGeometry InflatedSphere(InSphere->radius + MtdInflation);
			return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedSphere, PShapeWorldPose);
		}

		case PxGeometryType::eCONVEXMESH:
		{
			// We can't exactly inflate the mesh (not easily), so try jittering it a bit to get an MTD result.
			PxVec3 TraceDir = U2PVector(OutResult.TraceEnd - OutResult.TraceStart);
			TraceDir.normalizeSafe();

			// Try forward (in trace direction)
			PxTransform JitteredTM = PxTransform(QueryTM.p + (TraceDir * MtdInflation), QueryTM.q);
			if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
			{
				return true;
			}

			// Try backward (opposite trace direction)
			JitteredTM = PxTransform(QueryTM.p - (TraceDir * MtdInflation), QueryTM.q);
			if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
			{
				return true;
			}

			// Try axial directions.
			// Start with -Z because this is the most common case (objects on the floor).
			for (int32 i=2; i >= 0; i--)
			{
				PxVec3 Jitter(0.f);
				float* JitterPtr = &Jitter.x;
				JitterPtr[i] = MtdInflation;

				JitteredTM = PxTransform(QueryTM.p - Jitter, QueryTM.q);
				if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
				{
					return true;
				}

				JitteredTM = PxTransform(QueryTM.p + Jitter, QueryTM.q);
				if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
				{
					return true;
				}
			}

			return false;
		}

		default:
		{
			return false;
		}
	}
}



static bool CanFindOverlappedTriangle(const PxShape* PShape)
{
	return (PShape && (PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH || PShape->getGeometryType() == PxGeometryType::eHEIGHTFIELD));
}


static bool FindOverlappedTriangleNormal_Internal(const UWorld* World, const PxGeometry& Geom, const PxTransform& QueryTM, const PxShape* PShape, const PxTransform& PShapeWorldPose, FVector& OutNormal, bool bCanDrawOverlaps = false)
{
	if (CanFindOverlappedTriangle(PShape))
	{
		PxTriangleMeshGeometry PTriMeshGeom;
		PxHeightFieldGeometry PHeightfieldGeom;

		if (PShape->getTriangleMeshGeometry(PTriMeshGeom) || PShape->getHeightFieldGeometry(PHeightfieldGeom))
		{
			PxGeometryType::Enum GeometryType = PShape->getGeometryType();
			const bool bIsTriMesh = (GeometryType == PxGeometryType::eTRIANGLEMESH);
			PxU32 HitTris[64];
			bool bOverflow = false;

			const int32 NumTrisHit = bIsTriMesh ?
				PxMeshQuery::findOverlapTriangleMesh(Geom, QueryTM, PTriMeshGeom, PShapeWorldPose, HitTris, ARRAY_COUNT(HitTris), 0, bOverflow) :
				PxMeshQuery::findOverlapHeightField(Geom, QueryTM, PHeightfieldGeom, PShapeWorldPose, HitTris, ARRAY_COUNT(HitTris), 0, bOverflow);

			if (NumTrisHit > 0)
			{
				if (bIsTriMesh)
				{
					OutNormal = FindBestOverlappingNormal(World, Geom, QueryTM, PTriMeshGeom, PShapeWorldPose, HitTris, NumTrisHit, bCanDrawOverlaps);
				}
				else
				{
					OutNormal = FindBestOverlappingNormal(World, Geom, QueryTM, PHeightfieldGeom, PShapeWorldPose, HitTris, NumTrisHit, bCanDrawOverlaps);
				}

				return true;
			}
		}
	}

	return false;
}


static bool FindOverlappedTriangleNormal(const UWorld* World, const PxGeometry& Geom, const PxTransform& QueryTM, const PxShape* PShape, const PxTransform& PShapeWorldPose, FVector& OutNormal, float Inflation, bool bCanDrawOverlaps = false)
{
	bool bSuccess = false;

	if (CanFindOverlappedTriangle(PShape))
	{
		if (Inflation <= 0.f)
		{
			bSuccess = FindOverlappedTriangleNormal_Internal(World, Geom, QueryTM, PShape, PShapeWorldPose, OutNormal, bCanDrawOverlaps);
		}
		else
		{
			// Try a slightly inflated test if possible.
			switch (Geom.getType())
			{
				case PxGeometryType::eCAPSULE:
				{
					const PxCapsuleGeometry* InCapsule = static_cast<const PxCapsuleGeometry*>(&Geom);
					PxCapsuleGeometry InflatedCapsule(InCapsule->radius + Inflation, InCapsule->halfHeight); // don't inflate halfHeight, radius is added all around.
					bSuccess = FindOverlappedTriangleNormal_Internal(World, InflatedCapsule, QueryTM, PShape, PShapeWorldPose, OutNormal, bCanDrawOverlaps);
					break;
				}

				case PxGeometryType::eBOX:
				{
					const PxBoxGeometry* InBox = static_cast<const PxBoxGeometry*>(&Geom);
					PxBoxGeometry InflatedBox(InBox->halfExtents + PxVec3(Inflation));
					bSuccess = FindOverlappedTriangleNormal_Internal(World, InflatedBox, QueryTM, PShape, PShapeWorldPose, OutNormal, bCanDrawOverlaps);
					break;
				}

				case PxGeometryType::eSPHERE:
				{
					const PxSphereGeometry* InSphere = static_cast<const PxSphereGeometry*>(&Geom);
					PxSphereGeometry InflatedSphere(InSphere->radius + Inflation);
					bSuccess = FindOverlappedTriangleNormal_Internal(World, InflatedSphere, QueryTM, PShape, PShapeWorldPose, OutNormal, bCanDrawOverlaps);
					break;
				}

				default:
				{
					// No inflation possible
					break;
				}
			}
		}
	}

	return bSuccess;
}


/** Util to convert an overlapped shape into a sweep hit result, returns whether it was a blocking hit. */
static bool ConvertOverlappedShapeToImpactHit(const UWorld* World, const PxLocationHit& PHit, const FVector& StartLoc, const FVector& EndLoc, FHitResult& OutResult, const PxGeometry& Geom, const PxTransform& QueryTM, const PxFilterData& QueryFilter, bool bReturnPhysMat)
{
	SCOPE_CYCLE_COUNTER(STAT_CollisionConvertOverlapToHit);

	const PxShape* PShape = PHit.shape;
	const PxRigidActor* PActor = PHit.actor;
	const uint32 FaceIdx = PHit.faceIndex;

	// See if this is a 'blocking' hit
	PxFilterData PShapeFilter = PShape->getQueryFilterData();
	PxQueryHitType::Enum HitType = FPxQueryFilterCallback::CalcQueryHitType(QueryFilter, PShapeFilter);
	const bool bBlockingHit = (HitType == PxQueryHitType::eBLOCK); 
	OutResult.bBlockingHit = bBlockingHit;

	// Time of zero because initially overlapping
	OutResult.bStartPenetrating = true;
	OutResult.Time = 0.f;
	OutResult.Distance = 0.f;

	// Return start location as 'safe location'
	OutResult.Location = P2UVector(QueryTM.p);
	OutResult.ImpactPoint = OutResult.Location; // @todo not really sure of a better thing to do here...

	OutResult.TraceStart = StartLoc;
	OutResult.TraceEnd = EndLoc;

	const bool bFiniteNormal = PHit.normal.isFinite();
	const bool bValidNormal = (PHit.flags & PxHitFlag::eNORMAL) && bFiniteNormal;

	// Use MTD result if possible. We interpret the MTD vector as both the direction to move and the opposing normal.
	if (bValidNormal)
	{
		OutResult.ImpactNormal = P2UVector(PHit.normal);
		OutResult.PenetrationDepth = FMath::Abs(PHit.distance);
	}
	else
	{
		// Fallback normal if we can't find it with MTD or otherwise.
		OutResult.ImpactNormal = FVector::UpVector;
		OutResult.PenetrationDepth = 0.f;
		if (!bFiniteNormal)
		{
			UE_LOG(LogPhysics, Verbose, TEXT("Warning: ConvertOverlappedShapeToImpactHit: MTD returned NaN :( normal: (X:%f, Y:%f, Z:%f)"), PHit.normal.x, PHit.normal.y, PHit.normal.z);
		}
	}

#if DRAW_OVERLAPPING_TRIS
	if (CVarShowInitialOverlaps != 0 && World && World->IsGameWorld())
	{
		FVector DummyNormal(0.f);
		const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PShape, *PActor);
		FindOverlappedTriangleNormal(World, Geom, QueryTM, PShape, PShapeWorldPose, DummyNormal, 0.f, true);
	}
#endif

	if (bBlockingHit)
	{
		// Zero-distance hits are often valid hits and we can extract the hit normal.
		// For invalid normals we can try other methods as well (get overlapping triangles).
		if (PHit.distance == 0.f || !bValidNormal)
		{
			const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PShape, *PActor);

			// Try MTD with a small inflation for better accuracy, then a larger one in case the first one fails due to precision issues.
			static const float SmallMtdInflation = 0.250f;
			static const float LargeMtdInflation = 1.750f;

			if (ComputeInflatedMTD(SmallMtdInflation, PHit, OutResult, QueryTM, Geom, PShapeWorldPose) ||
				ComputeInflatedMTD(LargeMtdInflation, PHit, OutResult, QueryTM, Geom, PShapeWorldPose))
			{
				// Success
			}
			else
			{
				static const float SmallOverlapInflation = 0.250f;
				if (FindOverlappedTriangleNormal(World, Geom, QueryTM, PShape, PShapeWorldPose, OutResult.ImpactNormal, 0.f, false) ||
					FindOverlappedTriangleNormal(World, Geom, QueryTM, PShape, PShapeWorldPose, OutResult.ImpactNormal, SmallOverlapInflation, false))
				{
					// Success
				}
				else
				{
					// MTD failed, use point distance. This is not ideal.
					// Note: faceIndex seems to be unreliable for convex meshes in these cases, so not using FindGeomOpposingNormal() for them here.
					PxGeometryHolder Holder = PShape->getGeometry();
					PxGeometry& PGeom = Holder.any();
					PxVec3 PClosestPoint;
					const float Distance = PxGeometryQuery::pointDistance(QueryTM.p, PGeom, PShapeWorldPose, &PClosestPoint);

					if (Distance < KINDA_SMALL_NUMBER)
					{
						UE_LOG(LogCollision, Verbose, TEXT("Warning: ConvertOverlappedShapeToImpactHit: Query origin inside shape, giving poor MTD."));
						PClosestPoint = PxShapeExt::getWorldBounds(*PShape, *PActor).getCenter();
					}

					OutResult.ImpactNormal = (OutResult.Location - P2UVector(PClosestPoint)).GetSafeNormal();
				}
			}
		}
	}
	else
	{
		// non blocking hit (overlap).
		if (!bValidNormal)
		{
			OutResult.ImpactNormal = (StartLoc - EndLoc).GetSafeNormal();
			ensure(OutResult.ImpactNormal.IsNormalized());
		}
	}

	OutResult.Normal = OutResult.ImpactNormal;
	
	SetHitResultFromShapeAndFaceIndex(PShape, PActor, FaceIdx, OutResult, bReturnPhysMat);

	return bBlockingHit;
}


void ConvertQueryOverlap(const PxShape* PShape, const PxRigidActor* PActor, FOverlapResult& OutOverlap, const PxFilterData& QueryFilter)
{
	const bool bBlock = IsBlocking(PShape, QueryFilter);

	// Grab actor/component
	
	// Try body instance
	if (const FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(PActor->userData))
	{
        BodyInst = BodyInst->GetOriginalBodyInstance(PShape);
		if (const UPrimitiveComponent* OwnerComponent = BodyInst->OwnerComponent.Get())
		{
			OutOverlap.Actor = OwnerComponent->GetOwner();
			OutOverlap.Component = BodyInst->OwnerComponent; // Copying weak pointer is faster than assigning raw pointer.
			OutOverlap.ItemIndex = OwnerComponent->bMultiBodyOverlap ? BodyInst->InstanceBodyIndex : INDEX_NONE;
		}
	}
	else if(const FCustomPhysXPayload* CustomPayload = FPhysxUserData::Get<FCustomPhysXPayload>(PShape->userData))
	{
		TWeakObjectPtr<UPrimitiveComponent> OwnerComponent = CustomPayload->GetOwningComponent();
		if (UPrimitiveComponent* OwnerComponentRaw = OwnerComponent.Get())
		{
			OutOverlap.Actor = OwnerComponentRaw->GetOwner();
			OutOverlap.Component = OwnerComponent; // Copying weak pointer is faster than assigning raw pointer.
			OutOverlap.ItemIndex = OwnerComponent->bMultiBodyOverlap ? CustomPayload->GetItemIndex() : INDEX_NONE;
		}
	}
	else
	{
		ensureMsgf(false, TEXT("ConvertQueryOverlap called with bad payload type"));
	}

	// Other info
	OutOverlap.bBlockingHit = bBlock;
}

/** Util to add NewOverlap to OutOverlaps if it is not already there */
static void AddUniqueOverlap(TArray<FOverlapResult>& OutOverlaps, const FOverlapResult& NewOverlap)
{
	// Look to see if we already have this overlap (based on component)
	for(int32 TestIdx=0; TestIdx<OutOverlaps.Num(); TestIdx++)
	{
		FOverlapResult& Overlap = OutOverlaps[TestIdx];

		if (Overlap.ItemIndex == NewOverlap.ItemIndex && Overlap.Component == NewOverlap.Component)
		{
			// These should be the same if the component matches!
			checkSlow(Overlap.Actor == NewOverlap.Actor);

			// If we had a non-blocking overlap with this component, but now we have a blocking one, use that one instead!
			if(!Overlap.bBlockingHit && NewOverlap.bBlockingHit)
			{
				Overlap = NewOverlap;
			}

			return;
		}
	}

	// Not found, so add it 
	OutOverlaps.Add(NewOverlap);
}

bool IsBlocking(const PxShape* PShape, const PxFilterData& QueryFilter)
{
	// See if this is a 'blocking' hit
	const PxFilterData PShapeFilter = PShape->getQueryFilterData();
	const PxQueryHitType::Enum HitType = FPxQueryFilterCallback::CalcQueryHitType(QueryFilter, PShapeFilter);
	const bool bBlock = (HitType == PxQueryHitType::eBLOCK);
	return bBlock;
}

/** Min number of overlaps required to start using a TMap for deduplication */
int32 GNumOverlapsRequiredForTMap = 3;

static FAutoConsoleVariableRef GTestOverlapSpeed(
	TEXT("Engine.MinNumOverlapsToUseTMap"),
	GNumOverlapsRequiredForTMap,
	TEXT("Min number of overlaps required before using a TMap for deduplication")
	);

bool ConvertOverlapResults(int32 NumOverlaps, PxOverlapHit* POverlapResults, const PxFilterData& QueryFilter, TArray<FOverlapResult>& OutOverlaps)
{
	SCOPE_CYCLE_COUNTER(STAT_CollisionConvertOverlap);

	const int32 ExpectedSize = OutOverlaps.Num() + NumOverlaps;
	OutOverlaps.Reserve(ExpectedSize);
	bool bBlockingFound = false;

	if (ExpectedSize >= GNumOverlapsRequiredForTMap)
	{
		// Map from an overlap to the position in the result array (the index has one added to it so 0 can be a sentinel)
		TMap<FOverlapKey, int32, TInlineSetAllocator<64>> OverlapMap;
		OverlapMap.Reserve(ExpectedSize);

		// Fill in the map with existing hits
		for (int32 ExistingIndex = 0; ExistingIndex < OutOverlaps.Num(); ++ExistingIndex)
		{
			const FOverlapResult& ExistingOverlap = OutOverlaps[ExistingIndex];
			OverlapMap.Add(FOverlapKey(ExistingOverlap.Component.Get(), ExistingOverlap.ItemIndex), ExistingIndex + 1);
		}

		for (int32 PResultIndex = 0; PResultIndex < NumOverlaps; ++PResultIndex)
		{
			FOverlapResult NewOverlap;
			ConvertQueryOverlap(POverlapResults[PResultIndex].shape, POverlapResults[PResultIndex].actor, NewOverlap, QueryFilter);

			if (NewOverlap.bBlockingHit)
			{
				bBlockingFound = true;
			}

			// Look for it in the map, newly added elements will start with 0, so we know we need to add it to the results array then (the index is stored as +1)
			int32& DestinationIndex = OverlapMap.FindOrAdd(FOverlapKey(NewOverlap.Component.Get(), NewOverlap.ItemIndex));
			if (DestinationIndex == 0)
			{
				DestinationIndex = OutOverlaps.Add(NewOverlap) + 1;
			}
			else
			{
				FOverlapResult& ExistingOverlap = OutOverlaps[DestinationIndex - 1];

				// If we had a non-blocking overlap with this component, but now we have a blocking one, use that one instead!
				if (!ExistingOverlap.bBlockingHit && NewOverlap.bBlockingHit)
				{
					ExistingOverlap = NewOverlap;
				}
			}
		}
	}
	else
	{
		// N^2 approach, no maps
		for (int32 i = 0; i < NumOverlaps; i++)
		{
			FOverlapResult NewOverlap;
			ConvertQueryOverlap(POverlapResults[i].shape, POverlapResults[i].actor, NewOverlap, QueryFilter);

			if (NewOverlap.bBlockingHit)
			{
				bBlockingFound = true;
			}

			AddUniqueOverlap(OutOverlaps, NewOverlap);
		}
	}

	return bBlockingFound;
}

#endif // WITH_PHYSX
