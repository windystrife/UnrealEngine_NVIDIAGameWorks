// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "FlexContainerInstance.h"

#include "DrawDebugHelpers.h"
#include "PhysXSupport.h"
#include "WorldCollision.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/FlexAsset.h"
#include "PhysicsEngine/FlexContainer.h"

#if WITH_FLEX

bool FFlexContainerInstance::sGlobalDebugDraw = false;

#if STATS

enum EFlexGpuStats
{
	// gpu stats
	STAT_Flex_ContainerGpuTickTime,
	STAT_Flex_Predict,
	STAT_Flex_CreateCellIndices,
	STAT_Flex_SortCellIndices,		
	STAT_Flex_CreateGrid,				
	STAT_Flex_Reorder,					
	STAT_Flex_CollideParticles,
	STAT_Flex_CollideConvexes,			
	STAT_Flex_CollideTriangles,		
	STAT_Flex_CollideFields,			
	STAT_Flex_CalculateDensity,		
	STAT_Flex_SolveDensities,			
	STAT_Flex_SolveVelocities,			
	STAT_Flex_SolveShapes,				
	STAT_Flex_SolveSprings,			
	STAT_Flex_SolveContacts,			
	STAT_Flex_SolveInflatables,		
	STAT_Flex_CalculateAnisotropy,		
	STAT_Flex_UpdateDiffuse,			
	STAT_Flex_UpdateTriangles,
	STAT_Flex_Finalize,				
	STAT_Flex_UpdateBounds,		
};

#endif

// CPU stats, use "stat flex" to enable
DECLARE_CYCLE_STAT(TEXT("Gather Collision Shapes Time (CPU)"), STAT_Flex_GatherCollisionShapes, STATGROUP_Flex);
DECLARE_CYCLE_STAT(TEXT("Update Collision Shapes Time (CPU)"), STAT_Flex_UpdateCollisionShapes, STATGROUP_Flex);
DECLARE_CYCLE_STAT(TEXT("Update Actors Time (CPU)"), STAT_Flex_UpdateActors, STATGROUP_Flex);
DECLARE_CYCLE_STAT(TEXT("Update Data Time (CPU)"), STAT_Flex_DeviceUpdateTime, STATGROUP_Flex);
DECLARE_CYCLE_STAT(TEXT("Solver Tick Time (CPU)"), STAT_Flex_SolverUpdateTime, STATGROUP_Flex);
DECLARE_CYCLE_STAT(TEXT("Solve Sync Time (CPU)"), STAT_Flex_SolverSynchronizeTime, STATGROUP_Flex);

// Counters
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Container Count"), STAT_Flex_ContainerCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Instance Count"), STAT_Flex_InstanceCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Particle Count"), STAT_Flex_ParticleCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Spring Count"), STAT_Flex_SpringCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Shape Count"), STAT_Flex_ShapeCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Static Shape Count"), STAT_Flex_StaticShapeCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Static Convex Mesh Count"), STAT_Flex_StaticConvexMeshCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Static Triangle Mesh Count"), STAT_Flex_StaticTriangleMeshCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Force Field Count"), STAT_Flex_ForceFieldCount, STATGROUP_Flex);

// GPU stats, use "stat group enable flexgpu", and "stat flexgpu" to enable via console
// note that the the GPU counters will introduce significant synchronization overhead
DECLARE_CYCLE_STAT(TEXT("Predict"), STAT_Flex_Predict, STATGROUP_FlexGpu);
DECLARE_CYCLE_STAT(TEXT("CreateCellIndices"), STAT_Flex_CreateCellIndices, STATGROUP_FlexGpu);
DECLARE_CYCLE_STAT(TEXT("SortCellIndices"), STAT_Flex_SortCellIndices, STATGROUP_FlexGpu);		
DECLARE_CYCLE_STAT(TEXT("CreateGrid"), STAT_Flex_CreateGrid, STATGROUP_FlexGpu);				
DECLARE_CYCLE_STAT(TEXT("Reorder"), STAT_Flex_Reorder, STATGROUP_FlexGpu);					
DECLARE_CYCLE_STAT(TEXT("Collide Particles"), STAT_Flex_CollideParticles, STATGROUP_FlexGpu);
DECLARE_CYCLE_STAT(TEXT("Collide Convexes"), STAT_Flex_CollideConvexes, STATGROUP_FlexGpu);			
DECLARE_CYCLE_STAT(TEXT("Collide Triangles"), STAT_Flex_CollideTriangles, STATGROUP_FlexGpu);		
DECLARE_CYCLE_STAT(TEXT("Collide Fields"), STAT_Flex_CollideFields, STATGROUP_FlexGpu);			
DECLARE_CYCLE_STAT(TEXT("Calculate Density"), STAT_Flex_CalculateDensity, STATGROUP_FlexGpu);		
DECLARE_CYCLE_STAT(TEXT("Solve Density"), STAT_Flex_SolveDensities, STATGROUP_FlexGpu);			
DECLARE_CYCLE_STAT(TEXT("Solve Velocities"), STAT_Flex_SolveVelocities, STATGROUP_FlexGpu);			
DECLARE_CYCLE_STAT(TEXT("Solve Shapes"), STAT_Flex_SolveShapes, STATGROUP_FlexGpu);				
DECLARE_CYCLE_STAT(TEXT("Solve Springs"), STAT_Flex_SolveSprings, STATGROUP_FlexGpu);			
DECLARE_CYCLE_STAT(TEXT("Solve Contacts"), STAT_Flex_SolveContacts, STATGROUP_FlexGpu);			
DECLARE_CYCLE_STAT(TEXT("Solve Inflatables"), STAT_Flex_SolveInflatables, STATGROUP_FlexGpu);		
DECLARE_CYCLE_STAT(TEXT("Calculate Anisotropy"), STAT_Flex_CalculateAnisotropy, STATGROUP_FlexGpu);		
DECLARE_CYCLE_STAT(TEXT("Update Diffuse"), STAT_Flex_UpdateDiffuse, STATGROUP_FlexGpu);			
DECLARE_CYCLE_STAT(TEXT("Finalize"), STAT_Flex_Finalize, STATGROUP_FlexGpu);				
DECLARE_CYCLE_STAT(TEXT("Update Bounds"), STAT_Flex_UpdateBounds, STATGROUP_FlexGpu);
DECLARE_CYCLE_STAT(TEXT("Update Triangles"), STAT_Flex_UpdateTriangles, STATGROUP_FlexGpu);
DECLARE_CYCLE_STAT(TEXT("Total GPU Kernel Time"), STAT_Flex_ContainerGpuTickTime, STATGROUP_FlexGpu);

namespace 
{
	// helpers to find actor, shape pairs in a TSet
	bool operator == (const PxActorShape& lhs, const PxActorShape& rhs) { return lhs.actor == rhs.actor && lhs.shape == rhs.shape; }
	uint32 GetTypeHash(const PxActorShape& h) { return ::GetTypeHash((void*)(h.actor)) ^ ::GetTypeHash((void*)(h.shape)); }
}

// fwd decl
PxFilterData CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer & InCollisionResponseContainer, const struct FCollisionObjectQueryParams & ObjectParam, const bool bMultitrace);

// implementation of PxSceneDeletionListener, we need to invalidate our cached shapes here
void FFlexContainerInstance::onRelease(const PxBase* observed, void* userData, PxDeletionEventFlag::Enum deletionEvent)
{
	// note: this is a memory release callback, we can't inspect the type of the observed object (it is deleted)
	//		 so we must simply check if its value is in the cache

	NvFlexTriangleMeshId* Mesh = TriangleMeshes.Find(observed);

	if (Mesh)
	{
		// destroy and remove from cache
		NvFlexDestroyTriangleMesh(GFlexLib, *Mesh);
		TriangleMeshes.Remove(observed);

		DEC_DWORD_STAT(STAT_Flex_StaticTriangleMeshCount);
	}

	NvFlexConvexMeshId* Convex = ConvexMeshes.Find(observed);

	if (Convex)
	{
		// destroy and remove from cache
		NvFlexDestroyConvexMesh(GFlexLib, *Convex);
		ConvexMeshes.Remove(observed);

		DEC_DWORD_STAT(STAT_Flex_StaticConvexMeshCount);
	}
}

const NvFlexTriangleMeshId FFlexContainerInstance::GetTriangleMesh(const PxHeightField* HeightField)
{
	verify(HeightField)

	NvFlexTriangleMeshId* Mesh = TriangleMeshes.Find(HeightField);

	if (Mesh)
	{
		return *Mesh;
	}
	else
	{
		NvFlexTriangleMeshId NewMesh = NvFlexCreateTriangleMesh(GFlexLib);

		// clear temporary arrays for building trimesh data
		TriMeshVerts.map();
		TriMeshIndices.map();

		TriMeshVerts.resize(0);
		TriMeshIndices.resize(0);

		const PxU32		NumCols = HeightField->getNbColumns();
		const PxU32		NumRows = HeightField->getNbRows();
		const PxU32		NumVerts = NumRows * NumCols;
		const PxU32     NumFaces = (NumCols - 1) * (NumRows - 1) * 2;

		PxHeightFieldSample* sampleBuffer = new PxHeightFieldSample[NumVerts];
		HeightField->saveCells(sampleBuffer, NumVerts * sizeof(PxHeightFieldSample));

		PxBounds3 LocalBounds = PxBounds3::empty();

		for (PxU32 i = 0; i < NumRows; i++)
		{
			for (PxU32 j = 0; j < NumCols; j++)
			{
				FVector vert = FVector(float(i), float(sampleBuffer[j + (i*NumCols)].height), float(j));

				TriMeshVerts.push_back(FVector4(vert));

				LocalBounds.include(U2PVector(vert));
			}
		}

		for (PxU16 i = 0; i < (NumCols - 1); ++i)
		{
			for (PxU16 j = 0; j < (NumRows - 1); ++j)
			{
				PxU8 tessFlag = sampleBuffer[i + j*NumCols].tessFlag();
				PxU16 i0 = j*NumCols + i;
				PxU16 i1 = j*NumCols + i + 1;
				PxU16 i2 = (j + 1) * NumCols + i;
				PxU16 i3 = (j + 1) * NumCols + i + 1;
				// i2---i3
				// |    |
				// |    |
				// i0---i1
				// this is really a corner vertex index, not triangle index
				PxU16 mat0 = HeightField->getTriangleMaterialIndex((j*NumCols + i) * 2);
				PxU16 mat1 = HeightField->getTriangleMaterialIndex((j*NumCols + i) * 2 + 1);
				bool hole0 = (mat0 == PxHeightFieldMaterial::eHOLE);
				bool hole1 = (mat1 == PxHeightFieldMaterial::eHOLE);
							
				TriMeshIndices.push_back((hole0 ? i0 : i2));
				TriMeshIndices.push_back(i0);
				TriMeshIndices.push_back((tessFlag ? i3 : i1));

				TriMeshIndices.push_back((hole1 ? i1 : i3));
				TriMeshIndices.push_back((tessFlag ? i0 : i2));
				TriMeshIndices.push_back(i1);
							
			}
		}

		delete[] sampleBuffer;


		TriMeshVerts.unmap();
		TriMeshIndices.unmap();

		NvFlexUpdateTriangleMesh(GFlexLib, NewMesh, TriMeshVerts.buffer, TriMeshIndices.buffer, TriMeshVerts.size(), TriMeshIndices.size()/3,  &LocalBounds.minimum.x, &LocalBounds.maximum.x);

		// add to cache
		TriangleMeshes.Add((void*)HeightField, NewMesh);

		INC_DWORD_STAT(STAT_Flex_StaticTriangleMeshCount);

		return NewMesh;
	}
}

const NvFlexTriangleMeshId FFlexContainerInstance::GetTriangleMesh(const PxTriangleMesh* TriMesh)
{
	verify(TriMesh)

	NvFlexTriangleMeshId* Mesh = TriangleMeshes.Find(TriMesh);

	if (Mesh)
	{
		return *Mesh;
	}
	else
	{
		NvFlexTriangleMeshId NewMesh = NvFlexCreateTriangleMesh(GFlexLib);

		int32 NumVerts = TriMesh->getNbVertices();
		int32 NumIndices = TriMesh->getNbTriangles()*3;

		const PxVec3* Verts = TriMesh->getVertices();

		// clear temporary arrays for building trimesh data
		TriMeshVerts.map();
		TriMeshIndices.map();

		TriMeshVerts.resize(0);
		TriMeshIndices.resize(0);

		for (int32 v=0; v < NumVerts; ++v)
			TriMeshVerts.push_back(FVector4(P2UVector(Verts[v])));

		if (TriMesh->getTriangleMeshFlags() & PxTriangleMeshFlag::e16_BIT_INDICES)
		{
			const uint16* Indices = (const uint16*)TriMesh->getTriangles();

			for (int32 t=0; t < NumIndices; ++t)
				TriMeshIndices.push_back(Indices[t]);
		}
		else
		{
			const uint32* Indices = (const uint32*)TriMesh->getTriangles();

			for (int32 t=0; t < NumIndices; ++t)
				TriMeshIndices.push_back(Indices[t]);
		}										

		TriMeshVerts.unmap();
		TriMeshIndices.unmap();

		PxBounds3 LocalBounds = TriMesh->getLocalBounds();

		NvFlexUpdateTriangleMesh(GFlexLib, NewMesh, TriMeshVerts.buffer, TriMeshIndices.buffer, TriMeshVerts.size(), TriMeshIndices.size()/3,  &LocalBounds.minimum.x, &LocalBounds.maximum.x);

		// add to cache
		TriangleMeshes.Add((void*)TriMesh, NewMesh);

		INC_DWORD_STAT(STAT_Flex_StaticTriangleMeshCount);

		return NewMesh;
	}

}

const NvFlexTriangleMeshId FFlexContainerInstance::GetConvexMesh(const PxConvexMesh* ConvexMesh)
{
	verify(ConvexMesh)

	NvFlexTriangleMeshId* Mesh = ConvexMeshes.Find(ConvexMesh);

	if (Mesh)
	{
		return *Mesh;
	}
	else
	{
		NvFlexConvexMeshId NewMesh = NvFlexCreateConvexMesh(GFlexLib);

		ConvexMeshPlanes.map();
		ConvexMeshPlanes.resize(0);

		int32 NumPolygons = ConvexMesh->getNbPolygons();

		for (int32 p=0; p < NumPolygons; ++p)
		{
			PxHullPolygon Poly;
			ConvexMesh->getPolygonData(p, Poly);

			// transform plane from mesh space to shape space
			FVector4 ShapePlane = FVector4(Poly.mPlane[0], Poly.mPlane[1], Poly.mPlane[2], Poly.mPlane[3]); 
			ConvexMeshPlanes.push_back(ShapePlane);
		}	

		ConvexMeshPlanes.unmap();

		PxBounds3 ConvexBounds = ConvexMesh->getLocalBounds();
		
		NvFlexUpdateConvexMesh(GFlexLib, NewMesh, ConvexMeshPlanes.buffer, ConvexMeshPlanes.size(), &ConvexBounds.minimum.x, &ConvexBounds.maximum.x);

		ConvexMeshes.Add((void*)ConvexMesh, NewMesh);

		INC_DWORD_STAT(STAT_Flex_StaticConvexMeshCount);

		return NewMesh;
	}
}

// send bodies from synchronous PhysX scene to Flex scene
void FFlexContainerInstance::UpdateCollisionData()
{	
	// skip empty containers
	int32 NumActive = NvFlexGetActiveCount(Solver);
	if (NumActive == 0 && Components.Num() == 0)
		return;

	// modify global geometry counts
	DEC_DWORD_STAT_BY(STAT_Flex_StaticShapeCount, ShapePositions.size());

	// map buffers for write
	ShapeGeometry.map();
	ShapePositions.map();
	ShapeRotations.map();
	ShapePositionsPrev.map();
	ShapeRotationsPrev.map();
	ShapeFlags.map();

	ShapeGeometry.resize(0);
	ShapePositions.resize(0);
	ShapeRotations.resize(0);
	ShapePositionsPrev.resize(0);
	ShapeRotationsPrev.resize(0);
	ShapeFlags.resize(0);

	ShapeReportIndices.Reset();
	ShapeReportComponents.Reset();

	FBox MergedActorBounds(ForceInit);
	FBox TriMeshBounds(ForceInit);

	// used to test if an actor shape pair has already been reported
	TSet<PxActorShape> OverlapSet;
	
	// buffer for overlaps
	TArray<FOverlapResult> Overlaps;
	TArray<FOverlapResult> PerComponentOverlaps;
	TArray<PxShape*> Shapes;

	// expand bounds to catch any potential collisions (assume 60fps)
	const FVector Expand = FVector(Template->MaxVelocity / 60.0f + Template->CollisionDistance + Template->CollisionMarginShapes);

	// lock the scene to perform scene queries
	SCENE_LOCK_READ(Owner->GetPhysXScene(PST_Sync));

	{
		SCOPE_CYCLE_COUNTER(STAT_Flex_GatherCollisionShapes);

		// gather shapes from the scene
		for (int32 ActorIndex=0; ActorIndex < Components.Num(); ActorIndex++)
		{
			IFlexContainerClient* Component = Components[ActorIndex];
			if (!Component->IsEnabled())
				continue;

			FBoxSphereBounds ComponentBounds = Component->GetBounds();

			const FVector Center = ComponentBounds.Origin;
			const FVector HalfEdge = ComponentBounds.BoxExtent + Expand;

			// if particles explode, the bound will be very big and cause a hang in the overlap code below or crash
			if(HalfEdge.SizeSquared2D() > Template->MaxContainerBound)
			{
				UE_LOG(LogFlex, Warning, TEXT("Flex container bound grows bigger than %f") , Template->MaxContainerBound);
				continue;
			}

			if (Template->bUseMergedBounds)
			{
				MergedActorBounds += FBox(Center - HalfEdge, Center + HalfEdge);
			}
			else
			{
				FCollisionShape CollisionShape;
				CollisionShape.SetBox(HalfEdge);
				
				Owner->GetOwningWorld()->OverlapMultiByChannel(PerComponentOverlaps, Center, FQuat::Identity, Template->ObjectType, CollisionShape, FCollisionQueryParams(), FCollisionResponseParams(Template->ResponseToChannels));
				Overlaps.Append(PerComponentOverlaps);
				PerComponentOverlaps.Reset();
			}
		}

		if (Template->bUseMergedBounds)
		{
			FCollisionShape CollisionShape;
			CollisionShape.SetBox(MergedActorBounds.GetExtent());
    
			Owner->GetOwningWorld()->OverlapMultiByChannel(Overlaps, MergedActorBounds.GetCenter(), FQuat::Identity, Template->ObjectType, CollisionShape, FCollisionQueryParams(), FCollisionResponseParams(Template->ResponseToChannels));
		}

		for (int32 OverlapIndex=0; OverlapIndex < Overlaps.Num(); ++OverlapIndex)
		{
			const FOverlapResult& hit = Overlaps[OverlapIndex];
			
			const UPrimitiveComponent* PrimComp = hit.Component.Get();
			if (!PrimComp)
				continue;

			//OverlapMultiple returns ECollisionResponse::ECR_Overlap types, which we want to ignore
			ECollisionResponse Response = PrimComp->GetCollisionResponseToChannel(Template->ObjectType);
			if (Response == ECollisionResponse::ECR_Ignore)
				continue;

			bool bIsOverlap = (Response == ECollisionResponse::ECR_Overlap);
			bool bReportShape = PrimComp->bFlexEnableParticleCounter || PrimComp->bFlexParticleDrain;

			//Currently we are just interested in overlaps that correspond to triggers. Overlap response is also used for auto attachments.
			if (bIsOverlap && !bReportShape)
				continue;

			FBodyInstance* Body = NULL;

			if (hit.ItemIndex != INDEX_NONE)
			{
				const USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(PrimComp);
				if (SkeletalMeshComp)
				{
					Body = SkeletalMeshComp->Bodies[hit.ItemIndex];
				}

				// todo: try destructible actors
			}
			else
			{
				Body = PrimComp->GetBodyInstance();
			}
			
			if (!Body)
				continue;

			PxRigidActor* Actor = Body->GetPxRigidActor_AssumesLocked();
			if (!Actor)
				continue;

			Shapes.Reset();

			int32 NumSyncShapes;			
			NumSyncShapes = Body->GetAllShapes_AssumesLocked(Shapes);

			for (int ShapeIndex = 0; ShapeIndex < Shapes.Num(); ++ShapeIndex)
			{
				PxShape* Shape = Shapes[ShapeIndex];

				if (!Actor || !Shape)
					continue;

				// check if we've already processed this actor-shape pair
				bool alreadyProcessed = false;
				OverlapSet.Add(PxActorShape(Actor, Shape), &alreadyProcessed);
				if (alreadyProcessed)
					continue;
		
				const PxTransform& ActorTransform = Actor->getGlobalPose();
				const PxTransform& ShapeTransform = Shape->getLocalPose();
		
				PxFilterData Filter = Shape->getQueryFilterData();

				// only process complex collision shapes if enabled on the container
				if (Template->ComplexCollision)
				{
					if ((Filter.word3 & EPDF_ComplexCollision) == 0)
						continue;
				}
				else
				{
					if ((Filter.word3 & EPDF_SimpleCollision) == 0)
						continue;
				}

				PxTransform DeltaTransform = PxTransform(PxIdentity);
			
				// for components that act as a localization parent we ignore the velocity as it 
				// makes friction and CCD behave incorrectly, we should actually
				// just factor out the parent's velocity to allow sub-bodies (like a ragdoll) to have some relative motion
				if (!PrimComp->bIsFlexParent)
				{
					// generate previous frame's transform from rigid body velocities and time-step
					const PxVec3 LinearVelocity = U2PVector(Body->GetUnrealWorldVelocity_AssumesLocked());
					const PxVec3 AngularVelocity = U2PVector(Body->GetUnrealWorldAngularVelocityInRadians_AssumesLocked());

					// generate finite transform from velocities
					const float RadiansPerSecond = AngularVelocity.magnitude();
					const float Dt = AverageDeltaTime;

					DeltaTransform = PxTransform(-LinearVelocity*Dt, PxQuat(-RadiansPerSecond*Dt, FMath::RadiansToDegrees(AngularVelocity).getNormalized()));
				}
		
				switch(Shape->getGeometryType())
				{
					case PxGeometryType::eSPHERE:
					case PxGeometryType::eCAPSULE:
					case PxGeometryType::eBOX:
					{							
						PxTransform WorldTransform = ActorTransform*ShapeTransform;
						PxTransform WorldTransformPrev = PxTransform(WorldTransform.p + DeltaTransform.p, DeltaTransform.q*WorldTransform.q);

						ShapePositions.push_back(FVector4(WorldTransform.p.x, WorldTransform.p.y, WorldTransform.p.z, 1.0f));
						ShapeRotations.push_back(FQuat(WorldTransform.q.x, WorldTransform.q.y, WorldTransform.q.z, WorldTransform.q.w));

						// previous transforms
						ShapePositionsPrev.push_back(FVector4(WorldTransformPrev.p.x, WorldTransformPrev.p.y, WorldTransformPrev.p.z, 1.0f));
						ShapeRotationsPrev.push_back(FQuat(WorldTransformPrev.q.x, WorldTransformPrev.q.y, WorldTransformPrev.q.z, WorldTransformPrev.q.w));
						
						int32 ShapeReportIndex = -1;
						if (bReportShape)
						{
							ShapeReportIndex = ShapeReportComponents.Num();
							ShapeReportComponents.Push(TWeakObjectPtr<UPrimitiveComponent>(PrimComp));
						}
						ShapeReportIndices.Push(ShapeReportIndex);

						if (Shape->getGeometryType() == PxGeometryType::eCAPSULE)
						{
							PxCapsuleGeometry CapsuleGeometry;
							Shape->getCapsuleGeometry(CapsuleGeometry);

							NvFlexCollisionGeometry Geo;
							Geo.capsule.halfHeight = CapsuleGeometry.halfHeight;
							Geo.capsule.radius = CapsuleGeometry.radius;

							ShapeGeometry.push_back(Geo);

							int32 Flags = NvFlexMakeShapeFlags(NvFlexCollisionShapeType::eNvFlexShapeCapsule, Actor->is<PxRigidStatic>() == NULL) | (bIsOverlap ? eNvFlexShapeFlagTrigger : 0);
							ShapeFlags.push_back(Flags);

						}
						else if (Shape->getGeometryType() == PxGeometryType::eSPHERE)
						{
							PxSphereGeometry SphereGeometry;
							Shape->getSphereGeometry(SphereGeometry);

							NvFlexCollisionGeometry Geo;
							Geo.sphere.radius = SphereGeometry.radius;

							ShapeGeometry.push_back(Geo);

							int32 Flags = NvFlexMakeShapeFlags(NvFlexCollisionShapeType::eNvFlexShapeSphere, Actor->is<PxRigidStatic>() == NULL) | (bIsOverlap ? eNvFlexShapeFlagTrigger : 0);
							ShapeFlags.push_back(Flags);

						}
						else if (Shape->getGeometryType() == PxGeometryType::eBOX)
						{
							PxBoxGeometry BoxGeometry;
							Shape->getBoxGeometry(BoxGeometry);							

							NvFlexCollisionGeometry Geo;
							Geo.box.halfExtents[0] = BoxGeometry.halfExtents.x;
							Geo.box.halfExtents[1] = BoxGeometry.halfExtents.y;
							Geo.box.halfExtents[2] = BoxGeometry.halfExtents.z;

							ShapeGeometry.push_back(Geo);

							int32 Flags = NvFlexMakeShapeFlags(NvFlexCollisionShapeType::eNvFlexShapeBox, Actor->is<PxRigidStatic>() == NULL) | (bIsOverlap ? eNvFlexShapeFlagTrigger : 0);
							ShapeFlags.push_back(Flags);
						}
						break;
					}
					case PxGeometryType::eCONVEXMESH:
					{
						PxConvexMeshGeometry ConvexMesh;
						Shape->getConvexMeshGeometry(ConvexMesh);
					
						PxTransform WorldTransform = ActorTransform*ShapeTransform;
						PxTransform WorldTransformPrev = PxTransform(WorldTransform.p + DeltaTransform.p, DeltaTransform.q*WorldTransform.q);

						if (ConvexMesh.convexMesh)
						{
							// current transforms
							ShapePositions.push_back(FVector4(WorldTransform.p.x, WorldTransform.p.y, WorldTransform.p.z, 1.0f));
							ShapeRotations.push_back(FQuat(WorldTransform.q.x, WorldTransform.q.y, WorldTransform.q.z, WorldTransform.q.w));

							// previous transforms
							ShapePositionsPrev.push_back(FVector4(WorldTransformPrev.p.x, WorldTransformPrev.p.y, WorldTransformPrev.p.z, 1.0f));
							ShapeRotationsPrev.push_back(FQuat(WorldTransformPrev.q.x, WorldTransformPrev.q.y, WorldTransformPrev.q.z, WorldTransformPrev.q.w));

							int32 ShapeReportIndex = -1;
							if (bReportShape)
							{
								ShapeReportIndex = ShapeReportComponents.Num();
								ShapeReportComponents.Push(TWeakObjectPtr<UPrimitiveComponent>(PrimComp));
							}
							ShapeReportIndices.Push(ShapeReportIndex);

							// look up mesh in cache (or create)
							NvFlexConvexMeshId Mesh = GetConvexMesh(ConvexMesh.convexMesh);
							
							NvFlexCollisionGeometry Geometry;
							Geometry.convexMesh.mesh= Mesh;
							Geometry.convexMesh.scale[0] = ConvexMesh.scale.scale.x;
							Geometry.convexMesh.scale[1] = ConvexMesh.scale.scale.y;
							Geometry.convexMesh.scale[2] = ConvexMesh.scale.scale.z;

							ShapeGeometry.push_back(Geometry);
														
							int32 Flags = NvFlexMakeShapeFlags(NvFlexCollisionShapeType::eNvFlexShapeConvexMesh, Actor->is<PxRigidStatic>() == NULL) | (bIsOverlap ? eNvFlexShapeFlagTrigger : 0);
							ShapeFlags.push_back(Flags);
						}
						
						break;
					}
					case PxGeometryType::eTRIANGLEMESH:
					{
						PxTriangleMeshGeometry TriMesh;							
						Shape->getTriangleMeshGeometry(TriMesh);

						PxTransform WorldTransform = ActorTransform*ShapeTransform;
						PxTransform WorldTransformPrev = PxTransform(WorldTransform.p + DeltaTransform.p, DeltaTransform.q*WorldTransform.q);

						// current transforms
						ShapePositions.push_back(FVector4(WorldTransform.p.x, WorldTransform.p.y, WorldTransform.p.z, 1.0f));
						ShapeRotations.push_back(FQuat(WorldTransform.q.x, WorldTransform.q.y, WorldTransform.q.z, WorldTransform.q.w));

						// previous transforms
						ShapePositionsPrev.push_back(FVector4(WorldTransformPrev.p.x, WorldTransformPrev.p.y, WorldTransformPrev.p.z, 1.0f));
						ShapeRotationsPrev.push_back(FQuat(WorldTransformPrev.q.x, WorldTransformPrev.q.y, WorldTransformPrev.q.z, WorldTransformPrev.q.w));

						// find or convert matching FlexTriangleMesh
						NvFlexTriangleMeshId Mesh = GetTriangleMesh(TriMesh.triangleMesh);

						NvFlexCollisionGeometry Geometry;
						Geometry.triMesh.mesh = Mesh;
						Geometry.triMesh.scale[0] = TriMesh.scale.scale.x;
						Geometry.triMesh.scale[1] = TriMesh.scale.scale.y;
						Geometry.triMesh.scale[2] = TriMesh.scale.scale.z;
						
						ShapeGeometry.push_back(Geometry);
						
						int32 Flags = NvFlexMakeShapeFlags(NvFlexCollisionShapeType::eNvFlexShapeTriangleMesh, Actor->is<PxRigidStatic>() == NULL) | (bIsOverlap ? eNvFlexShapeFlagTrigger : 0);							
						ShapeFlags.push_back(Flags);

						int32 ShapeReportIndex = -1;
						if (bReportShape)
						{
							ShapeReportIndex = ShapeReportComponents.Num();
							ShapeReportComponents.Push(TWeakObjectPtr<UPrimitiveComponent>(PrimComp));
						}
						ShapeReportIndices.Push(ShapeReportIndex);					
						break;
					}
					case PxGeometryType::eHEIGHTFIELD:
					{
						PxHeightFieldGeometry HeightFieldGeom;
						Shape->getHeightFieldGeometry(HeightFieldGeom);

						PxTransform WorldTransform = ActorTransform*ShapeTransform;
						PxTransform WorldTransformPrev = PxTransform(WorldTransform.p + DeltaTransform.p, DeltaTransform.q*WorldTransform.q);

						// current transforms
						ShapePositions.push_back(FVector4(WorldTransform.p.x, WorldTransform.p.y, WorldTransform.p.z, 1.0f));
						ShapeRotations.push_back(FQuat(WorldTransform.q.x, WorldTransform.q.y, WorldTransform.q.z, WorldTransform.q.w));

						// previous transforms
						ShapePositionsPrev.push_back(FVector4(WorldTransformPrev.p.x, WorldTransformPrev.p.y, WorldTransformPrev.p.z, 1.0f));
						ShapeRotationsPrev.push_back(FQuat(WorldTransformPrev.q.x, WorldTransformPrev.q.y, WorldTransformPrev.q.z, WorldTransformPrev.q.w));

						// scaled mesh bounds
						//PxBounds3 MeshBounds = PxBounds3(PxVec3(0.0f), U2PVector(Scale));						
						//DrawDebugBox(GWorld, P2UVector(WorldTransform.p), P2UVector(MeshBounds.getExtents()), FColor::Cyan);					

						// find or convert matching FlexTriangleMesh
						NvFlexTriangleMeshId Mesh = GetTriangleMesh(HeightFieldGeom.heightField);

						NvFlexCollisionGeometry Geometry;
						Geometry.triMesh.mesh = Mesh;
						Geometry.triMesh.scale[0] = HeightFieldGeom.rowScale;
						Geometry.triMesh.scale[1] = HeightFieldGeom.heightScale;
						Geometry.triMesh.scale[2] = HeightFieldGeom.columnScale;

						ShapeGeometry.push_back(Geometry);
						
						int32 Flags = NvFlexMakeShapeFlags(NvFlexCollisionShapeType::eNvFlexShapeTriangleMesh, Actor->is<PxRigidStatic>() == NULL) | (bIsOverlap ? eNvFlexShapeFlagTrigger : 0);							
						ShapeFlags.push_back(Flags);

						int32 ShapeReportIndex = -1;
						if (bReportShape)
						{
							ShapeReportIndex = ShapeReportComponents.Num();
							ShapeReportComponents.Push(TWeakObjectPtr<UPrimitiveComponent>(PrimComp));
						}
						ShapeReportIndices.Push(ShapeReportIndex);

						break;
					}
				}
			}
		}
	}

	SCENE_UNLOCK_READ(Owner->GetPhysXScene(PST_Sync));

	// push to flex
	{
		SCOPE_CYCLE_COUNTER(STAT_Flex_UpdateCollisionShapes);
		
		ShapeGeometry.unmap();
		ShapePositions.unmap();
		ShapeRotations.unmap();
		ShapePositionsPrev.unmap();
		ShapeRotationsPrev.unmap();
		ShapeFlags.unmap();

		if (ShapeFlags.size())
		{
			NvFlexSetShapes(Solver, ShapeGeometry.buffer, ShapePositions.buffer, ShapeRotations.buffer, ShapePositionsPrev.buffer, ShapeRotationsPrev.buffer, ShapeFlags.buffer, ShapeFlags.size());
		}
		else
		{
			NvFlexSetShapes(Solver, NULL, NULL, NULL, NULL, NULL, NULL, 0);
		}
	}

	// increase  global geometry counters
	INC_DWORD_STAT_BY(STAT_Flex_StaticShapeCount, ShapePositions.size());

}

FFlexContainerInstance::FFlexContainerInstance(UFlexContainer* InTemplate, FPhysScene* OwnerScene)
	: Bounds(FVector(0.0f), FVector(0.0f), 0.0f),
	Anisotropy1(GFlexLib),
	Anisotropy2(GFlexLib),
	Anisotropy3(GFlexLib),
	SmoothPositions(GFlexLib),
	ShapeGeometry(GFlexLib),
	ShapeFlags(GFlexLib),
	ShapePositions(GFlexLib),
	ShapeRotations(GFlexLib),
	ShapePositionsPrev(GFlexLib),
	ShapeRotationsPrev(GFlexLib),
	TriMeshVerts(GFlexLib),
	TriMeshIndices(GFlexLib),
	ConvexMeshPlanes(GFlexLib),
	ContactIndices(GFlexLib),
	ContactVelocities(GFlexLib),
	ContactCounts(GFlexLib)
{
	INC_DWORD_STAT(STAT_Flex_ContainerCount);

	UE_LOG(LogFlex, Display, TEXT("Creating FLEX container: %s"), *InTemplate->GetName());

	TemplateRef = InTemplate;
	Template = InTemplate;


    NvFlexSolverDesc SolverDesc;
    NvFlexSetSolverDescDefaults(&SolverDesc);

    SolverDesc.maxParticles = Template->MaxParticles;
    SolverDesc.maxDiffuseParticles = 0;
    SolverDesc.featureMode = eNvFlexFeatureModeDefault;

    Solver = NvFlexCreateSolver(GFlexLib, &SolverDesc);
    Container = NvFlexExtCreateContainer(GFlexLib, Solver, Template->MaxParticles);

	ForceFieldCallback = NvFlexExtCreateForceFieldCallback(Solver);
	
	if (Template->AnisotropyScale > 0.0f)
	{
		Anisotropy1.init(Template->MaxParticles);
		Anisotropy2.init(Template->MaxParticles);
		Anisotropy3.init(Template->MaxParticles);
	}

	if (Template->PositionSmoothing > 0.0f)
	{
		SmoothPositions.init(Template->MaxParticles);
	}

	ContactIndices.init(Template->MaxParticles);
	ContactVelocities.init(Template->MaxParticles*MaxContactsPerParticle);
	ContactCounts.init(Template->MaxParticles);
	ContactCounted.SetNum(Template->MaxParticles);

	GroupCounter = 0;
	LeftOverTime = 0.0f;
	
	// assume initial time-step at 60hz, will quickly adapt to true rate depending on time-step smoothing
	AverageDeltaTime = (1.0f/60.0f);

	// zero mapped pointers
	Particles = NULL;
	ParticleRestPositions = NULL;
	Velocities = NULL;
	Normals = NULL;
	Phases = NULL;

	Owner = OwnerScene;

	// data starts mapped
	Map();

	GPhysXSDK->registerDeletionListener(*this, PxDeletionEventFlag::eMEMORY_RELEASE);
}

FFlexContainerInstance::~FFlexContainerInstance()
{
	DEC_DWORD_STAT(STAT_Flex_ContainerCount);
	DEC_DWORD_STAT_BY(STAT_Flex_StaticShapeCount, ShapePositions.size());

	UE_LOG(LogFlex, Display, TEXT("Destroying a FLEX system for.."));
	
	GPhysXSDK->unregisterDeletionListener(*this);

	DEC_DWORD_STAT_BY(STAT_Flex_StaticTriangleMeshCount, TriangleMeshes.Num());
	DEC_DWORD_STAT_BY(STAT_Flex_StaticConvexMeshCount, ConvexMeshes.Num());

	// destroy any remaining cached triangle meshes
	for (auto It = TriangleMeshes.CreateIterator(); It; ++It)
	{
		NvFlexDestroyTriangleMesh(GFlexLib, It->Value);
	}

	TriangleMeshes.Reset();

	for (auto It = ConvexMeshes.CreateIterator(); It; ++It)
	{
		NvFlexDestroyConvexMesh(GFlexLib, It->Value);
	}

	ConvexMeshes.Reset();

	if (ForceFieldCallback)
		NvFlexExtDestroyForceFieldCallback(ForceFieldCallback);

	if (Container)
		NvFlexExtDestroyContainer(Container);

	if (Solver)
		NvFlexDestroySolver(Solver);
}

int32 FFlexContainerInstance::CreateParticle(const FVector4& Pos, const FVector& Vel, int32 Phase)
{
	verify(Container);
	verify(IsMapped());

	int32 index;
	int32 n = NvFlexExtAllocParticles(Container, 1, &index);

	if (n == 0)
	{
		// not enough space in container to allocate
		return -1;
	}
	else
	{
		INC_DWORD_STAT(STAT_Flex_ParticleCount);

		Particles[index] = Pos;
		Velocities[index] = Vel;
		Normals[index] = FVector4(0.0f);
		Phases[index] = Phase;
		ContactIndices[index] = -1;
		ContactCounted[index] = false;

		return index;
	}
}

void FFlexContainerInstance::DestroyParticle(int32 Index)
{
	verify(Container);
	verify(Index >=0 && Index < Template->MaxParticles);

	// destruction is deferred so we do not need to be mapped here

	NvFlexExtFreeParticles(Container, 1, &Index);

	DEC_DWORD_STAT(STAT_Flex_ParticleCount);
}

void FFlexContainerInstance::CopyParticle(int32 Source, int32 Dest)
{
	check(Source < GetMaxParticleCount());
	check(Dest < GetMaxParticleCount());
	verify(IsMapped());

	Particles[Dest] = Particles[Source];
	ParticleRestPositions[Dest] = ParticleRestPositions[Source];
	Velocities[Dest] = Velocities[Source];
	Normals[Dest] = Normals[Source];
	Phases[Dest] = Phases[Source];
}
 
NvFlexExtInstance* FFlexContainerInstance::CreateInstance(const NvFlexExtAsset* Asset, const FMatrix& Mat, const FVector& Velocity, int32 Phase)
{
	verify(IsMapped());

	// spawn into the container
	NvFlexExtInstance* Inst = NvFlexExtCreateInstance(Container, &MappedData, Asset, (const float*)&Mat, Velocity.X, Velocity.Y, Velocity.Z, Phase, 1.0f);

	// creation will fail if instance cannot fit inside container
	if (Inst)
	{
		INC_DWORD_STAT(STAT_Flex_InstanceCount);
		INC_DWORD_STAT_BY(STAT_Flex_ParticleCount, Inst->asset->numParticles);
		INC_DWORD_STAT_BY(STAT_Flex_SpringCount, Inst->asset->numSprings);
		INC_DWORD_STAT_BY(STAT_Flex_ShapeCount, Inst->asset->numShapes);
	}
	else
	{
		// disabled warning text to stop spamming the log
		//debugf(TEXT("Could not create Flex object, not enough space remaining in container."));
	}

	return Inst;
}

void FFlexContainerInstance::DestroyInstance(NvFlexExtInstance* Inst)
{
	// destruction is deferred so we do not need to be mapped here

	DEC_DWORD_STAT(STAT_Flex_InstanceCount);
	DEC_DWORD_STAT_BY(STAT_Flex_ParticleCount, Inst->asset->numParticles);
	DEC_DWORD_STAT_BY(STAT_Flex_SpringCount, Inst->asset->numSprings);
	DEC_DWORD_STAT_BY(STAT_Flex_ShapeCount, Inst->asset->numShapes);

	NvFlexExtDestroyInstance(Container, Inst);
}

int32 FFlexContainerInstance::GetPhase(const FFlexPhase& Phase)
{
	int Group = Phase.Group;

	// if required then auto-assign a new group
	if (Phase.AutoAssignGroup)
		Group = GroupCounter++;

	int Flags = 0;
	if (Phase.SelfCollide)
		Flags |= eNvFlexPhaseSelfCollide;

	if (Phase.IgnoreRestCollisions)
		Flags |= eNvFlexPhaseSelfCollideFilter;

	if (Phase.Fluid)
		Flags |= eNvFlexPhaseFluid;

	return NvFlexMakePhase(Group, Flags);
}


void FFlexContainerInstance::ComputeSteppingParam(float& Dt, int32& NumSubsteps, float& NewLeftOverTime, float DeltaTime) const
{
	// clamp DeltaTime to a minimum to avoid taking an 
	// excessive number of substeps during frame-rate spikes
	DeltaTime = FMath::Min(DeltaTime, 1.0f/float(Template->MinFrameRate));

	// convert substeps parameter to substeps per-second
	// a value of 2 corresponds to 120 substeps/second
	const float StepsPerSecond = Template->NumSubsteps*60.0f;
	const float SubstepDt = 1.0f/StepsPerSecond;
	const float ElapsedTime = LeftOverTime + DeltaTime;

	if (Template->FixedTimeStep)
	{
		NumSubsteps = int32(ElapsedTime/SubstepDt);		
		Dt = NumSubsteps*SubstepDt;

		// don't carry over more than 1 substep worth of time
		NewLeftOverTime = FMath::Min(ElapsedTime - NumSubsteps*SubstepDt, SubstepDt);
	}
	else
	{
		NumSubsteps = Template->NumSubsteps;
		Dt = DeltaTime;
		
		NewLeftOverTime = 0.0f;
	}
}

void FFlexContainerInstance::UpdateSimData()
{
	SCOPE_CYCLE_COUNTER(STAT_Flex_DeviceUpdateTime);

	//map the surface tension to a comfortable scale
	static float SurfaceTensionFactor = 1e-6f;

	NvFlexParams params;
	FMemory::Memset(&params, 0, sizeof(params));

	params.gravity[0] = Template->Gravity.X;
	params.gravity[1] = Template->Gravity.Y;
	params.gravity[2] = Template->Gravity.Z;

	params.wind[0] = Template->Wind.X;
	params.wind[1] = Template->Wind.Y;
	params.wind[2] = Template->Wind.Z;

	params.radius = Template->Radius;
	params.viscosity = Template->Viscosity;
	params.dynamicFriction = Template->ShapeFriction;
	params.staticFriction = Template->ShapeFriction;
	params.particleFriction = Template->ParticleFriction;
	params.drag = Template->Drag;	
	params.lift = Template->Lift;
	params.damping = Template->Damping;
	params.numIterations = Template->NumIterations;
	params.solidRestDistance = Template->Radius;
	params.fluidRestDistance = Template->Radius*Template->RestDistance;
	params.dissipation = Template->Dissipation;
	params.particleCollisionMargin = Template->CollisionMarginParticles;
	params.shapeCollisionMargin = FMath::Max(Template->CollisionMarginShapes, FMath::Max(Template->CollisionDistance*0.25f, 1.0f)); // ensure a minimum collision distance for generating contacts against shapes, we need some margin to avoid jittering as contacts activate/deactivate
	params.collisionDistance = Template->CollisionDistance;
	params.sleepThreshold = Template->SleepThreshold;
	params.shockPropagation = Template->ShockPropagation;
	params.restitution = Template->Restitution;
	params.smoothing = Template->PositionSmoothing;
	params.maxSpeed = Template->MaxVelocity;
	params.relaxationMode = Template->RelaxationMode == EFlexSolverRelaxationMode::Local ? eNvFlexRelaxationLocal : eNvFlexRelaxationGlobal;
	params.relaxationFactor = Template->RelaxationFactor;
	params.solidPressure = Template->SolidPressure;
	params.anisotropyScale = Template->AnisotropyScale;
	params.anisotropyMin = Template->AnisotropyMin;
	params.anisotropyMax = Template->AnisotropyMax;
	params.adhesion = Template->Adhesion;
	params.cohesion = Template->Cohesion;
	params.surfaceTension = Template->SurfaceTension * SurfaceTensionFactor;
	params.vorticityConfinement = Template->VorticityConfinement;
	params.diffuseThreshold = 0.0f;
	params.buoyancy = 1.0f;
	params.maxAcceleration = FLT_MAX;

	params.planes[0][0] = 0.0f;
	params.planes[0][1] = 0.0f;
	params.planes[0][2] = 1.0f;
	params.planes[0][3] = 0.0f;
	params.numPlanes = 0;

	// update params
	NvFlexSetParams(Solver, &params);

	// force fields
	NvFlexExtSetForceFields(ForceFieldCallback, ForceFields.GetData(), ForceFields.Num());
		
	// move particle data to GPU, async
	NvFlexExtPushToDevice(Container);
}

void FFlexContainerInstance::Simulate(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_Flex_SolverUpdateTime);

	// ensure that all data is unmapped before sending to the GPU
	if (IsMapped())
		Unmap();

	// only catpure perf counters if stats are visible (significant perf. cost)
	bool TimerGatherEnable = false;

#if STATS
	// only gather GPU stats if enabled as this has a high perf. overhead
	static TStatId FlexGpuStatId = IStatGroupEnableManager::Get().GetHighPerformanceEnableForStat(FName(), STAT_GROUP_TO_FStatGroup(STATGROUP_FlexGpu)::GetGroupName(), STAT_GROUP_TO_FStatGroup(STATGROUP_UObjects)::GetGroupCategory(), STAT_GROUP_TO_FStatGroup(STATGROUP_FlexGpu)::DefaultEnable, true, EStatDataType::ST_int64, TEXT("Flex GPU Stats"), true);
	
	TimerGatherEnable = !FlexGpuStatId.IsNone();
#endif

	// compute smoothed time-step
	AverageDeltaTime = FMath::Lerp(DeltaTime, AverageDeltaTime, Template->TimeStepSmoothingFactor);

	// compute simulation time
	float Dt;
	int32 NumSubsteps;
	ComputeSteppingParam(Dt, NumSubsteps, LeftOverTime, AverageDeltaTime);

	// updates collision shapes in flex
	UpdateCollisionData();
	
	// updates particle data on the device
	UpdateSimData();

	// tick container, note that the GPU update happens asynchronously
	// to the calling thread, FFlexContainerInstance::Synchronize() 
	// will be called from FPhysScene when the GPU work has completed
	NvFlexUpdateSolver(Solver, Dt, NumSubsteps, TimerGatherEnable);

	// read back data asynchronously
	NvFlexExtPullFromDevice(Container);
	
	if (Template->AnisotropyScale > 0.0f)
	{
		NvFlexGetAnisotropy(Solver, Anisotropy1.buffer, Anisotropy2.buffer, Anisotropy3.buffer, NULL);
	}

	if (Template->PositionSmoothing > 0.0f)
	{
		NvFlexGetSmoothParticles(Solver, SmoothPositions.buffer, NULL);
	}

	if (ShapeReportComponents.Num() > 0)
	{
		NvFlexGetContacts(Solver, nullptr, ContactVelocities.buffer, ContactIndices.buffer, ContactCounts.buffer);
	}

	// ensure copies have been kicked off
	NvFlexFlush(GFlexLib);

	SET_DWORD_STAT(STAT_Flex_ForceFieldCount, ForceFields.Num());

	//reset force fields
	ForceFields.SetNum(0);
}

void FFlexContainerInstance::Synchronize()
{
	SCOPE_CYCLE_COUNTER(STAT_Flex_SolverSynchronizeTime);

	// ensure data is mapped, this is a GPU sync point
	if (!IsMapped())
		Map();

	// output any debug information
	DebugDraw();
	
	// get container bounds
	FVector Lower(MappedData.lower[0], MappedData.lower[1], MappedData.lower[2]);
	FVector Upper(MappedData.upper[0], MappedData.upper[1], MappedData.upper[2]);

	Bounds = FBoxSphereBounds(FBox(Lower, Upper));

	NvFlexExtUpdateInstances(Container);

	{
		SCOPE_CYCLE_COUNTER(STAT_Flex_UpdateActors);
		
		// process components
		for (int32 i=0; i < Components.Num(); ++i)
			Components[i]->Synchronize();
	}
}

// maps particle data, synchronizing with GPU, should only be called by Synchronize()
void FFlexContainerInstance::Map()
{
	// map all data
	MappedData = NvFlexExtMapParticleData(Container);

#if STATS
	// given that we've already waited for the GPU, fetch the GPU timers 
	NvFlexTimers Timers;
	FMemory::Memset(&Timers, 0, sizeof(Timers));
	NvFlexGetTimers(Solver, &Timers);
#endif

	// pointers into extension managed particle data, only valid during synchronize
	Particles = (FVector4*)MappedData.particles;
	ParticleRestPositions = (FVector4*)MappedData.restParticles;
	Velocities = (FVector*)MappedData.velocities;
	Normals = (FVector4*)MappedData.normals;
	Phases = MappedData.phases;

	ContactIndices.map();
	ContactVelocities.map();
	ContactCounts.map();

	// map fluid buffers manually
	Anisotropy1.map();
	Anisotropy2.map();
	Anisotropy3.map();
	SmoothPositions.map();

#if STATS
	float scale = 0.001f / FPlatformTime::GetSecondsPerCycle();

	SET_CYCLE_COUNTER(STAT_Flex_Predict, FMath::TruncToInt(Timers.predict*scale));
	SET_CYCLE_COUNTER(STAT_Flex_CreateCellIndices, FMath::TruncToInt(Timers.createCellIndices * scale));
	SET_CYCLE_COUNTER(STAT_Flex_SortCellIndices, FMath::TruncToInt(Timers.sortCellIndices* scale));
	SET_CYCLE_COUNTER(STAT_Flex_CreateGrid, FMath::TruncToInt(Timers.createGrid * scale));
	SET_CYCLE_COUNTER(STAT_Flex_Reorder, FMath::TruncToInt(Timers.reorder * scale));
	SET_CYCLE_COUNTER(STAT_Flex_CollideParticles, FMath::TruncToInt(Timers.collideParticles* scale));
	SET_CYCLE_COUNTER(STAT_Flex_CollideConvexes, FMath::TruncToInt(Timers.collideShapes * scale));
	SET_CYCLE_COUNTER(STAT_Flex_CollideTriangles, FMath::TruncToInt(Timers.collideTriangles * scale));
	SET_CYCLE_COUNTER(STAT_Flex_CollideFields, FMath::TruncToInt(Timers.collideFields * scale));
	SET_CYCLE_COUNTER(STAT_Flex_CalculateDensity, FMath::TruncToInt(Timers.calculateDensity* scale));
	SET_CYCLE_COUNTER(STAT_Flex_SolveDensities, FMath::TruncToInt(Timers.solveDensities * scale));
	SET_CYCLE_COUNTER(STAT_Flex_SolveVelocities, FMath::TruncToInt(Timers.solveVelocities* scale));
	SET_CYCLE_COUNTER(STAT_Flex_SolveShapes, FMath::TruncToInt(Timers.solveShapes * scale));
	SET_CYCLE_COUNTER(STAT_Flex_SolveSprings, FMath::TruncToInt(Timers.solveSprings * scale));
	SET_CYCLE_COUNTER(STAT_Flex_SolveContacts, FMath::TruncToInt(Timers.solveContacts * scale));
	SET_CYCLE_COUNTER(STAT_Flex_SolveInflatables, FMath::TruncToInt(Timers.solveInflatables * scale));
	SET_CYCLE_COUNTER(STAT_Flex_CalculateAnisotropy, FMath::TruncToInt(Timers.calculateAnisotropy * scale));
	SET_CYCLE_COUNTER(STAT_Flex_UpdateDiffuse, FMath::TruncToInt(Timers.updateDiffuse * scale));
	SET_CYCLE_COUNTER(STAT_Flex_Finalize, FMath::TruncToInt(Timers.finalize * scale));
	SET_CYCLE_COUNTER(STAT_Flex_UpdateBounds, FMath::TruncToInt(Timers.updateBounds * scale));
#endif
}

// unmaps data, should only be called by Simulate()
void FFlexContainerInstance::Unmap()
{
	// unmap fluid buffers
	Anisotropy1.unmap();
	Anisotropy2.unmap();
	Anisotropy3.unmap();
	SmoothPositions.unmap();

	ContactCounts.unmap();
	ContactVelocities.unmap();
	ContactIndices.unmap();

	// unlock extensions data
	NvFlexExtUnmapParticleData(Container);

	// reset data pointers to catch any illegal access
	Particles = NULL;
	ParticleRestPositions = NULL;
	Velocities = NULL;
	Normals = NULL;
	Phases = NULL;
}

// returns true if data is mapped, if so then reads/writes may occur, otherwise they are illegal
bool FFlexContainerInstance::IsMapped()
{
	return Particles != NULL;
}


void FFlexContainerInstance::Register(IFlexContainerClient* Comp)
{
	Components.Push(Comp);
}

void FFlexContainerInstance::Unregister(IFlexContainerClient* Comp)
{
	Components.RemoveAt(Components.Find(Comp));
}


void FFlexContainerInstance::DebugDraw()
{
	if (Template->DebugDraw || sGlobalDebugDraw)
	{
		// draw instance bounds
		for (int32 i=0; i < Components.Num(); ++i)
		{
			IFlexContainerClient* Component = Components[i];
			if (!Component->IsEnabled())
				continue;

			FBoxSphereBounds ComponentBounds = Component->GetBounds();

			DrawDebugBox(Owner->GetOwningWorld(), ComponentBounds.Origin, ComponentBounds.BoxExtent, FColor(0, 255, 0), true);
		}

		ShapeGeometry.map();
		ShapeFlags.map();
		ShapePositions.map();
		ShapeRotations.map();


		// draw shape bounds
		for (int32 i=0; i < ShapeGeometry.size(); ++i)
		{
			NvFlexCollisionGeometry Geo = ShapeGeometry[i];
			int Type = ShapeFlags[i]&eNvFlexShapeFlagTypeMask;

			FVector Translation = ShapePositions[i];
			FQuat Rotation = ShapeRotations[i];

			FVector HalfExtents(0.0f);
			FVector Center;

			if (Type == eNvFlexShapeConvexMesh)
			{
				int ConvexMeshId = Geo.convexMesh.mesh;

				FVector Lower, Upper;
				NvFlexGetConvexMeshBounds(GFlexLib, ConvexMeshId, &Lower.X, &Upper.X);

				FVector Scale = (FVector&)Geo.convexMesh.scale;
				Lower *= Scale;
				Upper *= Scale;

				FVector Edges = Upper-Lower;
				FVector LocalCenter = (Upper+Lower)*0.5f;

				Center = Rotation*LocalCenter + Translation;
				HalfExtents = Edges*0.5f;
			}
			else if (Type == eNvFlexShapeBox)
			{
				HalfExtents = (FVector&)Geo.box.halfExtents;
				Center = Translation;
			}
			
			if (HalfExtents.X != 0.0f)
				DrawDebugBox(Owner->GetOwningWorld(), Center, HalfExtents, Rotation, FColor(255, 0, 0), true);
		}

		ShapeGeometry.unmap();
		ShapeFlags.unmap();
		ShapePositions.unmap();
		ShapeRotations.unmap();


		//draw container bounds
		DrawDebugBox(Owner->GetOwningWorld(), Bounds.Origin, Bounds.BoxExtent, FColor(255, 255, 255), true);

		// draw particles
		const FColor Colors[8] = 
		{
			FLinearColor(0.0f, 0.5f, 1.0f).ToFColor(false),
			FLinearColor(0.797f, 0.354f, 0.000f).ToFColor(false),
			FLinearColor(0.092f, 0.465f, 0.820f).ToFColor(false),
			FLinearColor(0.000f, 0.349f, 0.173f).ToFColor(false),
			FLinearColor(0.875f, 0.782f, 0.051f).ToFColor(false),
			FLinearColor(0.000f, 0.170f, 0.453f).ToFColor(false),
			FLinearColor(0.673f, 0.111f, 0.000f).ToFColor(false),
			FLinearColor(0.612f, 0.194f, 0.394f).ToFColor(false)
		};

		TArray<int32> ActiveIndices;
		ActiveIndices.SetNum(Template->MaxParticles);

		int32 NumActive = NvFlexExtGetActiveList(Container, &ActiveIndices[0]);
		
		// draw particles colored by phase
		for (int32 i = 0; i < NumActive; ++i)
			DrawDebugPoint(Owner->GetOwningWorld(), Particles[ActiveIndices[i]], 10.0f, Colors[Phases[ActiveIndices[i]]%8], true);

		NvFlexVector<FPlane> TmpContactPlanes(GFlexLib, Template->MaxParticles*MaxContactsPerParticle);
		NvFlexVector<int32> TmpContactIndices(GFlexLib, Template->MaxParticles);
		NvFlexVector<uint32> TmpContactCounts(GFlexLib, Template->MaxParticles);
	
		NvFlexGetContacts(Solver, TmpContactPlanes.buffer, NULL, TmpContactIndices.buffer, TmpContactCounts.buffer);

		TmpContactPlanes.map();
		TmpContactIndices.map();
		TmpContactCounts.map();

		for (int i=0 ; i < NumActive; ++i)
		{
			const int32 ContactIndex = TmpContactIndices[ActiveIndices[i]];
			const uint32 Count = TmpContactCounts[ContactIndex];

			const float Scale = 10.0f;

			for (uint32 c=0; c < Count; ++c)
			{
				FPlane Plane = TmpContactPlanes[ContactIndex*MaxContactsPerParticle + c];

				DrawDebugLine(Owner->GetOwningWorld(), Particles[ActiveIndices[i]],Particles[ActiveIndices[i]] + FVector(Plane.X, Plane.Y, Plane.Z)*Scale, FColor::Green, true);
			}
		}

		TmpContactPlanes.unmap();
		TmpContactIndices.unmap();
		TmpContactCounts.unmap();
	}
}

void FFlexContainerInstance::AddRadialForce(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff)
{
	ForceFields.AddUninitialized(1);
	NvFlexExtForceField& Force = ForceFields.Top();

	(FVector&)Force.mPosition = Origin;
	Force.mRadius = Radius;
	Force.mStrength = Strength;
	Force.mLinearFalloff = (Falloff != RIF_Constant);
	Force.mMode = eNvFlexExtModeForce;
}

void FFlexContainerInstance::AddRadialImpulse(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange)
{
	ForceFields.AddUninitialized(1);
	NvFlexExtForceField& Force = ForceFields.Top();

	(FVector&)Force.mPosition = Origin;
	Force.mRadius = Radius;
	Force.mStrength = Strength;
	Force.mLinearFalloff = (Falloff != RIF_Constant);
	Force.mMode = bVelChange ? eNvFlexExtModeVelocityChange : eNvFlexExtModeImpulse;
}

int FFlexContainerInstance::GetActiveParticleCount()
{
	return NvFlexGetActiveCount(Solver);
}

int FFlexContainerInstance::GetMaxParticleCount()
{
	return Template->MaxParticles;
}


#endif //WITH_FLEX

