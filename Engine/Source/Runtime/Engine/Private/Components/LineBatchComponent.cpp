// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*-----------------------------------------------------------------------------
	ULineBatchComponent implementation.
-----------------------------------------------------------------------------*/

#include "Components/LineBatchComponent.h"
#include "EngineGlobals.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Engine/Engine.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Engine/CollisionProfile.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"

FLineBatcherSceneProxy::FLineBatcherSceneProxy(const ULineBatchComponent* InComponent) :
	FPrimitiveSceneProxy(InComponent), Lines(InComponent->BatchedLines), 
	Points(InComponent->BatchedPoints), Meshes(InComponent->BatchedMeshes)
{
	bWillEverBeLit = false;
}

void FLineBatcherSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER( STAT_LineBatcherSceneProxy_GetDynamicMeshElements );

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			for (int32 i = 0; i < Lines.Num(); i++)
			{
				PDI->DrawLine(Lines[i].Start, Lines[i].End, Lines[i].Color, Lines[i].DepthPriority, Lines[i].Thickness);
			}

			for (int32 i = 0; i < Points.Num(); i++)
			{
				PDI->DrawPoint(Points[i].Position, Points[i].Color, Points[i].PointSize, Points[i].DepthPriority);
			}

			for (int32 i = 0; i < Meshes.Num(); i++)
			{
				static FVector const PosX(1.f,0,0);
				static FVector const PosY(0,1.f,0);
				static FVector const PosZ(0,0,1.f);

				FBatchedMesh const& M = Meshes[i];

				// this seems far from optimal in terms of perf, but it's for debugging
				FDynamicMeshBuilder MeshBuilder;

				// set up geometry
				for (int32 VertIdx=0; VertIdx < M.MeshVerts.Num(); ++VertIdx)
				{
					MeshBuilder.AddVertex( M.MeshVerts[VertIdx], FVector2D::ZeroVector, PosX, PosY, PosZ, FColor::White );
				}
				//MeshBuilder.AddTriangles(M.MeshIndices);
				for (int32 Idx=0; Idx < M.MeshIndices.Num(); Idx+=3)
				{
					MeshBuilder.AddTriangle( M.MeshIndices[Idx], M.MeshIndices[Idx+1], M.MeshIndices[Idx+2] );
				}

				FMaterialRenderProxy* const MaterialRenderProxy = new(FMemStack::Get()) FColoredMaterialRenderProxy(GEngine->DebugMeshMaterial->GetRenderProxy(false), M.Color);
				MeshBuilder.GetMesh(FMatrix::Identity, MaterialRenderProxy, M.DepthPriority, false, false, ViewIndex, Collector);
			}
		}
	}
}

/**
*  Returns a struct that describes to the renderer when to draw this proxy.
*	@param		Scene view to use to determine our relevence.
*  @return		View relevance struct
*/
FPrimitiveViewRelevance FLineBatcherSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance ViewRelevance;
	ViewRelevance.bDrawRelevance = IsShown(View);
	ViewRelevance.bDynamicRelevance = true;
	// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
	ViewRelevance.bSeparateTranslucencyRelevance = ViewRelevance.bNormalTranslucencyRelevance = true;
	return ViewRelevance;
}

uint32 FLineBatcherSceneProxy::GetMemoryFootprint( void ) const
{
	return( sizeof( *this ) + GetAllocatedSize() );
}

uint32 FLineBatcherSceneProxy::GetAllocatedSize( void ) const 
{ 
	return( FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize() + Points.GetAllocatedSize() + Meshes.GetAllocatedSize() ); 
}




ULineBatchComponent::ULineBatchComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = true;
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bUseEditorCompositing = true;
	bGenerateOverlapEvents = false;
	bCalculateAccurateBounds = true;
}

void ULineBatchComponent::DrawLine(const FVector& Start, const FVector& End, const FLinearColor& Color, uint8 DepthPriority, const float Thickness, const float LifeTime)
{
	new(BatchedLines) FBatchedLine(Start, End, Color, LifeTime, Thickness, DepthPriority);

	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	MarkRenderStateDirty();
}

void ULineBatchComponent::DrawLines(const TArray<FBatchedLine>& InLines)
{
	BatchedLines.Append(InLines);
	
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	MarkRenderStateDirty();
}

void ULineBatchComponent::DrawPoint(
	const FVector& Position,
	const FLinearColor& Color,
	float PointSize,
	uint8 DepthPriority,
	float LifeTime
	)
{
	new(BatchedPoints) FBatchedPoint(Position, Color, PointSize, LifeTime, DepthPriority);
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	MarkRenderStateDirty();
}

void ULineBatchComponent::DrawBox(const FBox& Box, const FMatrix& TM, const FColor& Color, uint8 InDepthPriorityGroup)
{
	FVector	B[2], P, Q;
	int32 ai, aj;
	B[0] = Box.Min;
	B[1] = Box.Max;

	for( ai=0; ai<2; ai++ ) for( aj=0; aj<2; aj++ )
	{
		P.X=B[ai].X; Q.X=B[ai].X;
		P.Y=B[aj].Y; Q.Y=B[aj].Y;
		P.Z=B[0].Z; Q.Z=B[1].Z;
		new(BatchedLines) FBatchedLine(TM.TransformPosition(P), TM.TransformPosition(Q), Color, DefaultLifeTime, 0.0f, InDepthPriorityGroup);

		P.Y=B[ai].Y; Q.Y=B[ai].Y;
		P.Z=B[aj].Z; Q.Z=B[aj].Z;
		P.X=B[0].X; Q.X=B[1].X;
		new(BatchedLines) FBatchedLine(TM.TransformPosition(P), TM.TransformPosition(Q), Color, DefaultLifeTime, 0.0f, InDepthPriorityGroup);

		P.Z=B[ai].Z; Q.Z=B[ai].Z;
		P.X=B[aj].X; Q.X=B[aj].X;
		P.Y=B[0].Y; Q.Y=B[1].Y;
		new(BatchedLines) FBatchedLine(TM.TransformPosition(P), TM.TransformPosition(Q), Color, DefaultLifeTime, 0.0f, InDepthPriorityGroup);
	}
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	MarkRenderStateDirty();
}

void ULineBatchComponent::DrawSolidBox(const FBox& Box, const FTransform& Xform, const FColor& Color, uint8 DepthPriority, float LifeTime)
{
	int32 const NewMeshIdx = BatchedMeshes.Add(FBatchedMesh());
	FBatchedMesh& BM = BatchedMeshes[NewMeshIdx];

	BM.Color = Color;
	BM.DepthPriority = DepthPriority;
	BM.RemainingLifeTime = LifeTime;

	BM.MeshVerts.AddUninitialized(8);
	BM.MeshVerts[0] = Xform.TransformPosition( FVector(Box.Min.X, Box.Min.Y, Box.Max.Z) );
	BM.MeshVerts[1] = Xform.TransformPosition( FVector(Box.Max.X, Box.Min.Y, Box.Max.Z) );
	BM.MeshVerts[2] = Xform.TransformPosition( FVector(Box.Min.X, Box.Min.Y, Box.Min.Z) );
	BM.MeshVerts[3] = Xform.TransformPosition( FVector(Box.Max.X, Box.Min.Y, Box.Min.Z) );
	BM.MeshVerts[4] = Xform.TransformPosition( FVector(Box.Min.X, Box.Max.Y, Box.Max.Z) );
	BM.MeshVerts[5] = Xform.TransformPosition( FVector(Box.Max.X, Box.Max.Y, Box.Max.Z) );
	BM.MeshVerts[6] = Xform.TransformPosition( FVector(Box.Min.X, Box.Max.Y, Box.Min.Z) );
	BM.MeshVerts[7] = Xform.TransformPosition( FVector(Box.Max.X, Box.Max.Y, Box.Min.Z) );

	// clockwise
	BM.MeshIndices.AddUninitialized(36);
	int32 const Indices[36] = {	3,2,0,
		3,0,1,
		7,3,1,
		7,1,5,
		6,7,5,
		6,5,4,
		2,6,4,
		2,4,0,
		1,0,4,
		1,4,5,
		7,6,2,
		7,2,3	};

	for (int32 Idx=0; Idx<36; ++Idx)
	{
		BM.MeshIndices[Idx] = Indices[Idx];
	}

	MarkRenderStateDirty();
}

void ULineBatchComponent::DrawMesh(TArray<FVector> const& Verts, TArray<int32> const& Indices, FColor const& Color, uint8 DepthPriority, float LifeTime)
{
	// modifying array element directly to avoid copying arrays
	int32 const NewMeshIdx = BatchedMeshes.Add(FBatchedMesh());
	FBatchedMesh& BM = BatchedMeshes[NewMeshIdx];

	BM.MeshIndices = Indices;
	BM.MeshVerts = Verts;
	BM.Color = Color;
	BM.DepthPriority = DepthPriority;
	BM.RemainingLifeTime = LifeTime;

	MarkRenderStateDirty();
}

void ULineBatchComponent::DrawDirectionalArrow(const FMatrix& ArrowToWorld,FColor InColor,float Length,float ArrowSize,uint8 DepthPriority)
{
	const FVector Tip = ArrowToWorld.TransformPosition(FVector(Length,0,0));
	new(BatchedLines) FBatchedLine(Tip,ArrowToWorld.TransformPosition(FVector::ZeroVector),InColor,DefaultLifeTime,0.0f,DepthPriority);
	new(BatchedLines) FBatchedLine(Tip,ArrowToWorld.TransformPosition(FVector(Length-ArrowSize,+ArrowSize,+ArrowSize)),InColor,DefaultLifeTime,0.0f,DepthPriority);
	new(BatchedLines) FBatchedLine(Tip,ArrowToWorld.TransformPosition(FVector(Length-ArrowSize,+ArrowSize,-ArrowSize)),InColor,DefaultLifeTime,0.0f,DepthPriority);
	new(BatchedLines) FBatchedLine(Tip,ArrowToWorld.TransformPosition(FVector(Length-ArrowSize,-ArrowSize,+ArrowSize)),InColor,DefaultLifeTime,0.0f,DepthPriority);
	new(BatchedLines) FBatchedLine(Tip,ArrowToWorld.TransformPosition(FVector(Length-ArrowSize,-ArrowSize,-ArrowSize)),InColor,DefaultLifeTime,0.0f,DepthPriority);
	MarkRenderStateDirty();
}

/** Draw a circle */
void ULineBatchComponent::DrawCircle(const FVector& Base,const FVector& X,const FVector& Y,FColor Color,float Radius,int32 NumSides,uint8 DepthPriority)
{
	const float	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(int32 SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		const FVector Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;
		new(BatchedLines) FBatchedLine(LastVertex,Vertex,Color,DefaultLifeTime,0.0f,DepthPriority);
		LastVertex = Vertex;
	}

	MarkRenderStateDirty();
}

void ULineBatchComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	bool bDirty = false;
	// Update the life time of batched lines, removing the lines which have expired.
	for(int32 LineIndex=0; LineIndex < BatchedLines.Num(); LineIndex++)
	{
		FBatchedLine& Line = BatchedLines[LineIndex];
		if (Line.RemainingLifeTime > 0.0f)
		{
			Line.RemainingLifeTime -= DeltaTime;
			if(Line.RemainingLifeTime <= 0.0f)
			{
				// The line has expired, remove it.
				BatchedLines.RemoveAtSwap(LineIndex--);
				bDirty = true;
			}
		}
	}

	// Update the life time of batched points, removing the points which have expired.
	for(int32 PtIndex=0; PtIndex < BatchedPoints.Num(); PtIndex++)
	{
		FBatchedPoint& Pt = BatchedPoints[PtIndex];
		if (Pt.RemainingLifeTime > 0.0f)
		{
			Pt.RemainingLifeTime -= DeltaTime;
			if(Pt.RemainingLifeTime <= 0.0f)
			{
				// The point has expired, remove it.
				BatchedPoints.RemoveAtSwap(PtIndex--);
				bDirty = true;
			}
		}
	}

	// Update the life time of batched meshes, removing the meshes which have expired.
	for(int32 MeshIndex=0; MeshIndex < BatchedMeshes.Num(); MeshIndex++)
	{
		FBatchedMesh& Mesh = BatchedMeshes[MeshIndex];
		if (Mesh.RemainingLifeTime > 0.0f)
		{
			Mesh.RemainingLifeTime -= DeltaTime;
			if(Mesh.RemainingLifeTime <= 0.0f)
			{
				// The mesh has expired, remove it.
				BatchedMeshes.RemoveAtSwap(MeshIndex--);
				bDirty = true;
			}
		}
	}

	if(bDirty)
	{
		MarkRenderStateDirty();
	}
}

void ULineBatchComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	bool bDirty = false;
	for (FBatchedLine& Line : BatchedLines)
	{
		Line.Start += InOffset;
		Line.End += InOffset;
		bDirty = true;
	}

	for (FBatchedPoint& Point : BatchedPoints)
	{
		Point.Position += InOffset;
		bDirty = true;
	}

	for (FBatchedMesh& Mesh : BatchedMeshes)
	{
		for (FVector& Vert : Mesh.MeshVerts)
		{
			Vert += InOffset;
			bDirty = true;
		}
	}

	if (bDirty)
	{
		MarkRenderStateDirty();
	}
}

/**
* Creates a new scene proxy for the line batcher component.
* @return	Pointer to the FLineBatcherSceneProxy
*/
FPrimitiveSceneProxy* ULineBatchComponent::CreateSceneProxy()
{
	return new FLineBatcherSceneProxy(this);
}

FBoxSphereBounds ULineBatchComponent::CalcBounds( const FTransform& LocalToWorld ) const 
{
	if (!bCalculateAccurateBounds)
	{
		const FVector BoxExtent(HALF_WORLD_MAX);
		return FBoxSphereBounds(FVector::ZeroVector, BoxExtent, BoxExtent.Size());
	}

	FBox BBox(ForceInit);
	for (const FBatchedLine& Line : BatchedLines)
	{
		BBox += Line.Start;
		BBox += Line.End;
	}

	for (const FBatchedPoint& Point : BatchedPoints)
	{
		BBox += Point.Position;
	}

	for (const FBatchedMesh& Mesh : BatchedMeshes)
	{
		for (const FVector& Vert : Mesh.MeshVerts)
		{
			BBox += Vert;
		}
	}

	if (BBox.IsValid)
	{
		// Points are in world space, so no need to transform.
		return FBoxSphereBounds(BBox);
	}
	else
	{
		const FVector BoxExtent(1.f);
		return FBoxSphereBounds(LocalToWorld.GetLocation(), BoxExtent, 1.f);
	}
}

void ULineBatchComponent::Flush()
{
	if (BatchedLines.Num() > 0 || BatchedPoints.Num() > 0 || BatchedMeshes.Num() > 0)
	{
		BatchedLines.Empty();
		BatchedPoints.Empty();
		BatchedMeshes.Empty();
		MarkRenderStateDirty();
	}
}
