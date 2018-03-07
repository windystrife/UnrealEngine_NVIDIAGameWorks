// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeomFitUtils.cpp: Utilities for fitting collision models to static meshes.
=============================================================================*/

#include "GeomFitUtils.h"
#include "EngineDefines.h"
#include "Misc/MessageDialog.h"
#include "UObject/UObjectIterator.h"
#include "Components/StaticMeshComponent.h"
#include "Model.h"
#include "Engine/Polys.h"
#include "StaticMeshResources.h"
#include "EditorSupportDelegates.h"
#include "BSPOps.h"
#include "RawMesh.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"

#define LOCAL_EPS (0.01f)
static void AddVertexIfNotPresent(TArray<FVector> &vertices, FVector &newVertex)
{
	bool isPresent = 0;

	for(int32 i=0; i<vertices.Num() && !isPresent; i++)
	{
		float diffSqr = (newVertex - vertices[i]).SizeSquared();
		if(diffSqr < LOCAL_EPS * LOCAL_EPS)
			isPresent = 1;
	}

	if(!isPresent)
		vertices.Add(newVertex);

}

static bool PromptToRemoveExistingCollision(UStaticMesh* StaticMesh)
{
	check(StaticMesh);
	UBodySetup* bs = StaticMesh->BodySetup;
	if(bs && (bs->AggGeom.GetElementCount() > 0))
	{
		// If we already have some simplified collision for this mesh - check before we clobber it.
		/*const EAppReturnType::Type ret = FMessageDialog::Open(EAppMsgType::YesNoCancel, NSLOCTEXT("UnrealEd", "StaticMeshAlreadyHasGeom", "Static Mesh already has simple collision.\nDo you want to replace it?"));
		if (ret == EAppReturnType::Yes)
		{
			bs->RemoveSimpleCollision();
		}
		else if (ret == EAppReturnType::Cancel)
		{
			return false;
		}*/
	}
	else
	{
		// Otherwise, create one here.
		StaticMesh->CreateBodySetup();
		bs = StaticMesh->BodySetup;
	}
	return true;
}

/* ******************************** KDOP ******************************** */

// This function takes the current collision model, and fits a k-DOP around it.
// It uses the array of k unit-length direction vectors to define the k bounding planes.

// THIS FUNCTION REPLACES EXISTING SIMPLE COLLISION MODEL WITH KDOP
#define MY_FLTMAX (3.402823466e+38F)

int32 GenerateKDopAsSimpleCollision(UStaticMesh* StaticMesh, const TArray<FVector> &Dirs)
{
	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	if (!PromptToRemoveExistingCollision(StaticMesh))
	{
		return INDEX_NONE;
	}

	UBodySetup* bs = StaticMesh->BodySetup;

	// Do k- specific stuff.
	int32 kCount = Dirs.Num();
	TArray<float> maxDist;
	for(int32 i=0; i<kCount; i++)
		maxDist.Add(-MY_FLTMAX);

	// Construct temporary UModel for kdop creation. We keep no refs to it, so it can be GC'd.
	auto TempModel = NewObject<UModel>();
	TempModel->Initialize(nullptr, 1);

	// For each vertex, project along each kdop direction, to find the max in that direction.
	const FStaticMeshLODResources& RenderData = StaticMesh->RenderData->LODResources[0];
	for(int32 i=0; i<RenderData.GetNumVertices(); i++)
	{
		for(int32 j=0; j<kCount; j++)
		{
			float dist = RenderData.PositionVertexBuffer.VertexPosition(i) | Dirs[j];
			maxDist[j] = FMath::Max(dist, maxDist[j]);
		}
	}

	// Inflate kdop to ensure it is no degenerate
	const float MinSize = 0.1f;
	for(int32 i=0; i<kCount; i++)
	{
		maxDist[i] += MinSize;
	}

	// Now we have the planes of the kdop, we work out the face polygons.
	TArray<FPlane> planes;
	for(int32 i=0; i<kCount; i++)
		planes.Add( FPlane(Dirs[i], maxDist[i]) );

	for(int32 i=0; i<planes.Num(); i++)
	{
		FPoly*	Polygon = new(TempModel->Polys->Element) FPoly();
		FVector Base, AxisX, AxisY;

		Polygon->Init();
		Polygon->Normal = planes[i];
		Polygon->Normal.FindBestAxisVectors(AxisX,AxisY);

		Base = planes[i] * planes[i].W;

		new(Polygon->Vertices) FVector(Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector(Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector(Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector(Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);

		for(int32 j=0; j<planes.Num(); j++)
		{
			if(i != j)
			{
				if(!Polygon->Split(-FVector(planes[j]), planes[j] * planes[j].W))
				{
					Polygon->Vertices.Empty();
					break;
				}
			}
		}

		if(Polygon->Vertices.Num() < 3)
		{
			// If poly resulted in no verts, remove from array
			TempModel->Polys->Element.RemoveAt(TempModel->Polys->Element.Num()-1);
		}
		else
		{
			// Other stuff...
			Polygon->iLink = i;
			Polygon->CalcNormal(1);
		}
	}

	if(TempModel->Polys->Element.Num() < 4)
	{
		TempModel = NULL;
		return INDEX_NONE;
	}

	// Build bounding box.
	TempModel->BuildBound();

	// Build BSP for the brush.
	FBSPOps::bspBuild(TempModel,FBSPOps::BSP_Good,15,70,1,0);
	FBSPOps::bspRefresh(TempModel,1);
	FBSPOps::bspBuildBounds(TempModel);

	bs->Modify();

	bs->CreateFromModel(TempModel, false);
	
	// create all body instances
	RefreshCollisionChange(*StaticMesh);

	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();

	return bs->AggGeom.ConvexElems.Num() - 1;
}

/* ******************************** BOX ******************************** */

static void CalcBoundingBox(const FRawMesh& RawMesh, FVector& Center, FVector& Extents, FVector& LimitVec)
{
	FBox Box(ForceInit);

	for (uint32 i = 0; i < (uint32)RawMesh.VertexPositions.Num(); i++)
	{
		Box += RawMesh.VertexPositions[i] * LimitVec;
	}

	Box.GetCenterAndExtents(Center, Extents);
}

void ComputeBoundingBox(UStaticMesh* StaticMesh, FVector& Center, FVector& Extents)
{
	// Calculate bounding Box.
	FRawMesh RawMesh;
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

	FVector unitVec = FVector(1.f);
	CalcBoundingBox(RawMesh, Center, Extents, unitVec);
}

int32 GenerateBoxAsSimpleCollision(UStaticMesh* StaticMesh)
{
	if (!PromptToRemoveExistingCollision(StaticMesh))
	{
		return INDEX_NONE;
	}

	UBodySetup* bs = StaticMesh->BodySetup;

	// Calculate bounding Box.
	FRawMesh RawMesh;
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

	FVector unitVec = bs->BuildScale3D;
	FVector Center, Extents;
	CalcBoundingBox(RawMesh, Center, Extents, unitVec);

	bs->Modify();

	// Create new GUID
	bs->InvalidatePhysicsData();

	FKBoxElem BoxElem;
	BoxElem.Center = Center;
	BoxElem.X = Extents.X * 2.0f;
	BoxElem.Y = Extents.Y * 2.0f;
	BoxElem.Z = Extents.Z * 2.0f;
	bs->AggGeom.BoxElems.Add(BoxElem);

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(*StaticMesh);

	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();

	StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization

	return bs->AggGeom.BoxElems.Num() - 1;
}

/* ******************************** SPHERE ******************************** */

// Can do bounding circles as well... Set elements of limitVect to 1.f for directions to consider, and 0.f to not consider.
// Have 2 algorithms, seem better in different cirumstances

// This algorithm taken from Ritter, 1990
// This one seems to do well with asymmetric input.
static void CalcBoundingSphere(const FRawMesh& RawMesh, FSphere& sphere, FVector& LimitVec)
{
	if(RawMesh.VertexPositions.Num() == 0)
		return;

	FBox Box;

	// First, find AABB, remembering furthest points in each dir.
	Box.Min = RawMesh.VertexPositions[0] * LimitVec;
	Box.Max = Box.Min;

	FIntVector minIx, maxIx; // Extreme points.

	minIx = FIntVector::ZeroValue;
	maxIx = FIntVector::ZeroValue;

	for(uint32 i=1; i<(uint32)RawMesh.VertexPositions.Num(); i++) 
	{
		FVector p = RawMesh.VertexPositions[i] * LimitVec;

		// X //
		if(p.X < Box.Min.X)
		{
			Box.Min.X = p.X;
			minIx.X = i;
		}
		else if(p.X > Box.Max.X)
		{
			Box.Max.X = p.X;
			maxIx.X = i;
		}

		// Y //
		if(p.Y < Box.Min.Y)
		{
			Box.Min.Y = p.Y;
			minIx.Y = i;
		}
		else if(p.Y > Box.Max.Y)
		{
			Box.Max.Y = p.Y;
			maxIx.Y = i;
		}

		// Z //
		if(p.Z < Box.Min.Z)
		{
			Box.Min.Z = p.Z;
			minIx.Z = i;
		}
		else if(p.Z > Box.Max.Z)
		{
			Box.Max.Z = p.Z;
			maxIx.Z = i;
		}
	}

	const FVector Extremes[3]={ (RawMesh.VertexPositions[maxIx.X] - RawMesh.VertexPositions[minIx.X]) * LimitVec,
								(RawMesh.VertexPositions[maxIx.Y] - RawMesh.VertexPositions[minIx.Y]) * LimitVec,
								(RawMesh.VertexPositions[maxIx.Z] - RawMesh.VertexPositions[minIx.Z]) * LimitVec };

	// Now find extreme points furthest apart, and initial center and radius of sphere.
	float d2 = 0.f;
	for(int32 i=0; i<3; i++)
	{
		const float tmpd2 = Extremes[i].SizeSquared();
		if(tmpd2 > d2)
		{
			d2 = tmpd2;
			sphere.Center = (RawMesh.VertexPositions[minIx(i)] + (0.5f * Extremes[i])) * LimitVec;
			sphere.W = 0.f;
		}
	}

	const FVector Extents = FVector(Extremes[0].X, Extremes[1].Y, Extremes[2].Z);

	// radius and radius squared
	float r = 0.5f * Extents.GetMax();
	float r2 = FMath::Square(r);

	// Now check each point lies within this sphere. If not - expand it a bit.
	for(uint32 i=0; i<(uint32)RawMesh.VertexPositions.Num(); i++) 
	{
		const FVector cToP = (RawMesh.VertexPositions[i] * LimitVec) - sphere.Center;

		const float pr2 = cToP.SizeSquared();

		// If this point is outside our current bounding sphere's radius
		if(pr2 > r2)
		{
			// ..expand radius just enough to include this point.
			const float pr = FMath::Sqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = FMath::Square(r);

			sphere.Center += ((pr-r)/pr * cToP);
		}
	}

	sphere.W = r;
}

// This is the one thats already used by unreal.
// Seems to do better with more symmetric input...
static void CalcBoundingSphere2(const FRawMesh& RawMesh, FSphere& sphere, FVector& LimitVec)
{
	FVector Center, Extents;
	CalcBoundingBox(RawMesh, Center, Extents, LimitVec);

	sphere.Center = Center;
	sphere.W = 0.0f;

	for( uint32 i=0; i<(uint32)RawMesh.VertexPositions.Num(); i++ )
	{
		float Dist = FVector::DistSquared(RawMesh.VertexPositions[i] * LimitVec, sphere.Center);
		if( Dist > sphere.W )
			sphere.W = Dist;
	}
	sphere.W = FMath::Sqrt(sphere.W);
}

// // //

int32 GenerateSphereAsSimpleCollision(UStaticMesh* StaticMesh)
{
	if (!PromptToRemoveExistingCollision(StaticMesh))
	{
		return INDEX_NONE;
	}

	UBodySetup* bs = StaticMesh->BodySetup;

	// Calculate bounding sphere.
	FRawMesh RawMesh;
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

	FSphere bSphere, bSphere2, bestSphere;
	FVector unitVec = bs->BuildScale3D;
	CalcBoundingSphere(RawMesh, bSphere, unitVec);
	CalcBoundingSphere2(RawMesh, bSphere2, unitVec);

	if(bSphere.W < bSphere2.W)
		bestSphere = bSphere;
	else
		bestSphere = bSphere2;

	// Dont use if radius is zero.
	if(bestSphere.W <= 0.f)
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Prompt_10", "Could not create geometry.") );
		return INDEX_NONE;
	}

	bs->Modify();

	// Create new GUID
	bs->InvalidatePhysicsData();

	FKSphereElem SphereElem;
	SphereElem.Center = bestSphere.Center;
	SphereElem.Radius = bestSphere.W;
	bs->AggGeom.SphereElems.Add(SphereElem);

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(*StaticMesh);

	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();

	StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
	return bs->AggGeom.SphereElems.Num() - 1;
}

/* ******************************** SPHYL ******************************** */

static void CalcBoundingSphyl(const FRawMesh& RawMesh, FSphere& sphere, float& length, FRotator& rotation, FVector& LimitVec)
{
	if (RawMesh.VertexPositions.Num() == 0)
		return;

	FVector Center, Extents;
	CalcBoundingBox(RawMesh, Center, Extents, LimitVec);

	// @todo sphere.Center could perhaps be adjusted to best fit if model is non-symmetric on it's longest axis
	sphere.Center = Center;

	// Work out best axis aligned orientation (longest side)
	float Extent = Extents.GetMax();
	if (Extent == Extents.X)
	{
		rotation = FRotator(90.f, 0.f, 0.f);
		Extents.X = 0.0f;
	}
	else if (Extent == Extents.Y)
	{
		rotation = FRotator(0.f, 0.f, 90.f);
		Extents.Y = 0.0f;
	}
	else
	{
		rotation = FRotator(0.f, 0.f, 0.f);
		Extents.Z = 0.0f;
	}

	// Cleared the largest axis above, remaining determines the radius
	float r = Extents.GetMax();
	float r2 = FMath::Square(r);
	
	// Now check each point lies within this the radius. If not - expand it a bit.
	for (uint32 i = 0; i<(uint32)RawMesh.VertexPositions.Num(); i++)
	{
		FVector cToP = (RawMesh.VertexPositions[i] * LimitVec) - sphere.Center;
		cToP = rotation.UnrotateVector(cToP);

		const float pr2 = cToP.SizeSquared2D();	// Ignore Z here...

		// If this point is outside our current bounding sphere's radius
		if (pr2 > r2)
		{
			// ..expand radius just enough to include this point.
			const float pr = FMath::Sqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = FMath::Square(r);
		}
	}
	
	// The length is the longest side minus the radius.
	float hl = FMath::Max(0.0f, Extent - r);

	// Now check each point lies within the length. If not - expand it a bit.
	for (uint32 i = 0; i<(uint32)RawMesh.VertexPositions.Num(); i++)
	{
		FVector cToP = (RawMesh.VertexPositions[i] * LimitVec) - sphere.Center;
		cToP = rotation.UnrotateVector(cToP);

		// If this point is outside our current bounding sphyl's length
		if (FMath::Abs(cToP.Z) > hl)
		{
			const bool bFlip = (cToP.Z < 0.f ? true : false);
			const FVector cOrigin(0.f, 0.f, (bFlip ? -hl : hl));

			const float pr2 = (cOrigin - cToP).SizeSquared();

			// If this point is outside our current bounding sphyl's radius
			if (pr2 > r2)
			{
				FVector cPoint;
				FMath::SphereDistToLine(cOrigin, r, cToP, (bFlip ? FVector(0.f, 0.f, 1.f) : FVector(0.f, 0.f, -1.f)), cPoint);

				// Don't accept zero as a valid diff when we know it's outside the sphere (saves needless retest on further iterations of like points)
				hl += FMath::Max(FMath::Abs(cToP.Z - cPoint.Z), 1.e-6f);
			}
		}
	}

	sphere.W = r;
	length = hl * 2.0f;
}

// // //

int32 GenerateSphylAsSimpleCollision(UStaticMesh* StaticMesh)
{
	if (!PromptToRemoveExistingCollision(StaticMesh))
	{
		return INDEX_NONE;
	}

	UBodySetup* bs = StaticMesh->BodySetup;

	// Calculate bounding box.
	FRawMesh RawMesh;
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

	FSphere sphere;
	float length;
	FRotator rotation;
	FVector unitVec = bs->BuildScale3D;
	CalcBoundingSphyl(RawMesh, sphere, length, rotation, unitVec);

	// Dont use if radius is zero.
	if (sphere.W <= 0.f)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Prompt_10", "Could not create geometry."));
		return INDEX_NONE;
	}

	// If height is zero, then a sphere would be better (should we just create one instead?)
	if (length <= 0.f)
	{
		length = SMALL_NUMBER;
	}

	bs->Modify();

	// Create new GUID
	bs->InvalidatePhysicsData();

	FKSphylElem SphylElem;
	SphylElem.Center = sphere.Center;
	SphylElem.Rotation = rotation;
	SphylElem.Radius = sphere.W;
	SphylElem.Length = length;
	bs->AggGeom.SphylElems.Add(SphylElem);

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(*StaticMesh);

	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();

	StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization

	return bs->AggGeom.SphylElems.Num() - 1;
}

void RefreshCollisionChange(UStaticMesh& StaticMesh)
{
	StaticMesh.CreateNavCollision(/*bIsUpdate=*/true);

	for (FObjectIterator Iter(UStaticMeshComponent::StaticClass()); Iter; ++Iter)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(*Iter);
		if (StaticMeshComponent->GetStaticMesh() == &StaticMesh)
		{
			// it needs to recreate IF it already has been created
			if (StaticMeshComponent->IsPhysicsStateCreated())
			{
				StaticMeshComponent->RecreatePhysicsState();
			}
		}
	}

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

/* *************************** DEPRECATED ******************************** */
void RefreshCollisionChange(const UStaticMesh* StaticMesh)
{
	if (StaticMesh)
	{
		RefreshCollisionChange(const_cast<UStaticMesh&>(*StaticMesh));
	}
}
