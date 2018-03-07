// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "RenderCommandFence.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "VertexFactory.h"
#include "RenderUtils.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DynamicMeshBuilder.h"
#include "PhysicsPublic.h"
#include "PhysXPublic.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/PhysicsAsset.h"

static const int32 DrawCollisionSides = 16;
static const int32 DrawConeLimitSides = 40;

static const float DebugJointPosSize = 5.0f;
static const float DebugJointAxisSize = 20.0f;

static const float JointRenderThickness = 0.1f;
static const float UnselectedJointRenderSize = 4.f;
static const float SelectedJointRenderSize = 10.f;
static const float LimitRenderSize = 0.16f;

static const FColor JointUnselectedColor(255, 0, 255);
static const FColor JointRed(FColor::Red);
static const FColor JointGreen(FColor::Green);
static const FColor JointBlue(FColor::Blue);

static const FColor	JointLimitColor(FColor::Green);
static const FColor	JointRefColor(FColor::Yellow);
static const FColor JointLockedColor(255,128,10);

/////////////////////////////////////////////////////////////////////////////////////
// FKSphereElem
/////////////////////////////////////////////////////////////////////////////////////

void FKSphereElem::DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FColor Color) const
{
	FVector ElemCenter = ElemTM.GetLocation();
	FVector X = ElemTM.GetScaledAxis(EAxis::X);
	FVector Y = ElemTM.GetScaledAxis(EAxis::Y);
	FVector Z = ElemTM.GetScaledAxis(EAxis::Z);

	const float ScaleRadius = Scale3D.GetAbsMin();

	DrawCircle(PDI, ElemCenter, X, Y, Color, ScaleRadius*Radius, DrawCollisionSides, SDPG_World);
	DrawCircle(PDI, ElemCenter, X, Z, Color, ScaleRadius*Radius, DrawCollisionSides, SDPG_World);
	DrawCircle(PDI, ElemCenter, Y, Z, Color, ScaleRadius*Radius, DrawCollisionSides, SDPG_World);
}


void FKSphereElem::DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy) const
{
	DrawSphere(PDI, ElemTM.GetLocation(), FRotator::ZeroRotator, FVector(this->Radius * Scale3D.GetAbsMin()), DrawCollisionSides, DrawCollisionSides / 2, MaterialRenderProxy, SDPG_World);
}

void FKSphereElem::GetElemSolid(const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	GetSphereMesh(ElemTM.GetLocation(), FVector(this->Radius * Scale3D.GetAbsMin()), DrawCollisionSides, DrawCollisionSides / 2, MaterialRenderProxy, SDPG_World, false, ViewIndex, Collector);
}

void FKBoxElem::DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FColor Color) const
{
	FVector	B[2], P, Q, Radii;

	// X,Y,Z member variables are LENGTH not RADIUS
	Radii.X = Scale3D.X*0.5f*X;
	Radii.Y = Scale3D.Y*0.5f*Y;
	Radii.Z = Scale3D.Z*0.5f*Z;

	B[0] = Radii; // max
	B[1] = -1.0f * Radii; // min

	for (int32 i = 0; i < 2; i++)
	{
		for (int32 j = 0; j < 2; j++)
		{
			P.X = B[i].X; Q.X = B[i].X;
			P.Y = B[j].Y; Q.Y = B[j].Y;
			P.Z = B[0].Z; Q.Z = B[1].Z;
			PDI->DrawLine(ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);

			P.Y = B[i].Y; Q.Y = B[i].Y;
			P.Z = B[j].Z; Q.Z = B[j].Z;
			P.X = B[0].X; Q.X = B[1].X;
			PDI->DrawLine(ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);

			P.Z = B[i].Z; Q.Z = B[i].Z;
			P.X = B[j].X; Q.X = B[j].X;
			P.Y = B[0].Y; Q.Y = B[1].Y;
			PDI->DrawLine(ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// FKBoxElem
/////////////////////////////////////////////////////////////////////////////////////

void FKBoxElem::DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy) const
{
	DrawBox(PDI, ElemTM.ToMatrixWithScale(), Scale3D * 0.5f * FVector(X, Y, Z), MaterialRenderProxy, SDPG_World);
}

void FKBoxElem::GetElemSolid(const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	GetBoxMesh(ElemTM.ToMatrixWithScale(), Scale3D * 0.5f * FVector(X, Y, Z), MaterialRenderProxy, SDPG_World, ViewIndex, Collector);
}

/////////////////////////////////////////////////////////////////////////////////////
// FKSphylElem
/////////////////////////////////////////////////////////////////////////////////////

static void DrawHalfCircle(FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, const FColor Color, float Radius)
{
	float	AngleDelta = 2.0f * (float)PI / ((float)DrawCollisionSides);
	FVector	LastVertex = Base + X * Radius;

	for(int32 SideIndex = 0; SideIndex < (DrawCollisionSides/2); SideIndex++)
	{
		FVector	Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;
		PDI->DrawLine(LastVertex, Vertex, Color, SDPG_World);
		LastVertex = Vertex;
	}	
}

void FKSphylElem::DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FColor Color) const
{
	const FVector Origin = ElemTM.GetLocation();
	const FVector XAxis = ElemTM.GetScaledAxis(EAxis::X);
	const FVector YAxis = ElemTM.GetScaledAxis(EAxis::Y);
	const FVector ZAxis = ElemTM.GetScaledAxis(EAxis::Z);
	const float ScaledHalfLength = GetScaledCylinderLength(Scale3D) * .5f;
	const float ScaledRadius = GetScaledRadius(Scale3D);
	
	// Draw top and bottom circles
	const FVector TopEnd = Origin + (ScaledHalfLength * ZAxis);
	const FVector BottomEnd = Origin - (ScaledHalfLength * ZAxis);

	DrawCircle(PDI, TopEnd, XAxis, YAxis, Color, ScaledRadius, DrawCollisionSides, SDPG_World);
	DrawCircle(PDI, BottomEnd, XAxis, YAxis, Color, ScaledRadius, DrawCollisionSides, SDPG_World);

	// Draw domed caps
	DrawHalfCircle(PDI, TopEnd, YAxis, ZAxis, Color, ScaledRadius);
	DrawHalfCircle(PDI, TopEnd, XAxis, ZAxis, Color, ScaledRadius);

	const FVector NegZAxis = -ZAxis;

	DrawHalfCircle(PDI, BottomEnd, YAxis, NegZAxis, Color, ScaledRadius);
	DrawHalfCircle(PDI, BottomEnd, XAxis, NegZAxis, Color,ScaledRadius);

	// Draw connecty lines
	PDI->DrawLine(TopEnd + ScaledRadius*XAxis, BottomEnd + ScaledRadius*XAxis, Color, SDPG_World);
	PDI->DrawLine(TopEnd - ScaledRadius*XAxis, BottomEnd - ScaledRadius*XAxis, Color, SDPG_World);
	PDI->DrawLine(TopEnd + ScaledRadius*YAxis, BottomEnd + ScaledRadius*YAxis, Color, SDPG_World);
	PDI->DrawLine(TopEnd - ScaledRadius*YAxis, BottomEnd - ScaledRadius*YAxis, Color, SDPG_World);
}

void FKSphylElem::GetElemSolid(const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	const FVector Scale3DAbs = Scale3D.GetAbs();
	const float ScaleRadius = FMath::Max(Scale3DAbs.X, Scale3DAbs.Y);
	const float ScaleLength = Scale3DAbs.Z;

	const int32 NumSides = DrawCollisionSides;
	const int32 NumRings = (DrawCollisionSides / 2) + 1;

	// The first/last arc are on top of each other.
	const int32 NumVerts = (NumSides + 1) * (NumRings + 1);
	FDynamicMeshVertex* Verts = (FDynamicMeshVertex*)FMemory::Malloc(NumVerts * sizeof(FDynamicMeshVertex));

	// Calculate verts for one arc
	FDynamicMeshVertex* ArcVerts = (FDynamicMeshVertex*)FMemory::Malloc((NumRings + 1) * sizeof(FDynamicMeshVertex));

	for (int32 RingIdx = 0; RingIdx < NumRings + 1; RingIdx++)
	{
		FDynamicMeshVertex* ArcVert = &ArcVerts[RingIdx];

		float Angle;
		float ZOffset;
		if (RingIdx <= DrawCollisionSides / 4)
		{
			Angle = ((float)RingIdx / (NumRings - 1)) * PI;
			ZOffset = 0.5 * ScaleLength * Length;
		}
		else
		{
			Angle = ((float)(RingIdx - 1) / (NumRings - 1)) * PI;
			ZOffset = -0.5 * ScaleLength * Length;
		}

		// Note- unit sphere, so position always has mag of one. We can just use it for normal!		
		FVector SpherePos;
		SpherePos.X = 0.0f;
		SpherePos.Y = ScaleRadius * Radius * FMath::Sin(Angle);
		SpherePos.Z = ScaleRadius * Radius * FMath::Cos(Angle);

		ArcVert->Position = SpherePos + FVector(0, 0, ZOffset);

		ArcVert->SetTangents(
			FVector(1, 0, 0),
			FVector(0.0f, -SpherePos.Z, SpherePos.Y),
			SpherePos
			);

		ArcVert->TextureCoordinate.X = 0.0f;
		ArcVert->TextureCoordinate.Y = ((float)RingIdx / NumRings);
	}

	// Then rotate this arc NumSides+1 times.
	for (int32 SideIdx = 0; SideIdx < NumSides + 1; SideIdx++)
	{
		const FRotator ArcRotator(0, 360.f * ((float)SideIdx / NumSides), 0);
		const FRotationMatrix ArcRot(ArcRotator);
		const float XTexCoord = ((float)SideIdx / NumSides);

		for (int32 VertIdx = 0; VertIdx < NumRings + 1; VertIdx++)
		{
			int32 VIx = (NumRings + 1)*SideIdx + VertIdx;

			Verts[VIx].Position = ArcRot.TransformPosition(ArcVerts[VertIdx].Position);

			Verts[VIx].SetTangents(
				ArcRot.TransformVector(ArcVerts[VertIdx].TangentX),
				ArcRot.TransformVector(ArcVerts[VertIdx].GetTangentY()),
				ArcRot.TransformVector(ArcVerts[VertIdx].TangentZ)
				);

			Verts[VIx].TextureCoordinate.X = XTexCoord;
			Verts[VIx].TextureCoordinate.Y = ArcVerts[VertIdx].TextureCoordinate.Y;
		}
	}

	FDynamicMeshBuilder MeshBuilder;
	{
		// Add all of the vertices to the mesh.
		for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
		{
			MeshBuilder.AddVertex(Verts[VertIdx]);
		}

		// Add all of the triangles to the mesh.
		for (int32 SideIdx = 0; SideIdx < NumSides; SideIdx++)
		{
			const int32 a0start = (SideIdx + 0) * (NumRings + 1);
			const int32 a1start = (SideIdx + 1) * (NumRings + 1);

			for (int32 RingIdx = 0; RingIdx < NumRings; RingIdx++)
			{
				MeshBuilder.AddTriangle(a0start + RingIdx + 0, a1start + RingIdx + 0, a0start + RingIdx + 1);
				MeshBuilder.AddTriangle(a1start + RingIdx + 0, a1start + RingIdx + 1, a0start + RingIdx + 1);
			}
		}

	}
	MeshBuilder.GetMesh(ElemTM.ToMatrixWithScale(), MaterialRenderProxy, SDPG_World, false, false, ViewIndex, Collector);

	FMemory::Free(Verts);
	FMemory::Free(ArcVerts);
}

void FKSphylElem::DrawElemSolid(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy) const
{
	const FVector Scale3DAbs = Scale3D.GetAbs();
	const float ScaleRadius = FMath::Max(Scale3DAbs.X, Scale3DAbs.Y);
	const float ScaleLength = Scale3DAbs.Z;

	const int32 NumSides = DrawCollisionSides;
	const int32 NumRings = (DrawCollisionSides / 2) + 1;

	// The first/last arc are on top of each other.
	const int32 NumVerts = (NumSides + 1) * (NumRings + 1);
	FDynamicMeshVertex* Verts = (FDynamicMeshVertex*)FMemory::Malloc(NumVerts * sizeof(FDynamicMeshVertex));

	// Calculate verts for one arc
	FDynamicMeshVertex* ArcVerts = (FDynamicMeshVertex*)FMemory::Malloc((NumRings + 1) * sizeof(FDynamicMeshVertex));

	for (int32 RingIdx = 0; RingIdx < NumRings + 1; RingIdx++)
	{
		FDynamicMeshVertex* ArcVert = &ArcVerts[RingIdx];

		float Angle;
		float ZOffset;
		if (RingIdx <= DrawCollisionSides / 4)
		{
			Angle = ((float)RingIdx / (NumRings - 1)) * PI;
			ZOffset = 0.5 * ScaleLength * Length;
		}
		else
		{
			Angle = ((float)(RingIdx - 1) / (NumRings - 1)) * PI;
			ZOffset = -0.5 * ScaleLength * Length;
		}

		// Note- unit sphere, so position always has mag of one. We can just use it for normal!		
		FVector SpherePos;
		SpherePos.X = 0.0f;
		SpherePos.Y = ScaleRadius * Radius * FMath::Sin(Angle);
		SpherePos.Z = ScaleRadius * Radius * FMath::Cos(Angle);

		ArcVert->Position = SpherePos + FVector(0, 0, ZOffset);

		ArcVert->SetTangents(
			FVector(1, 0, 0),
			FVector(0.0f, -SpherePos.Z, SpherePos.Y),
			SpherePos
			);

		ArcVert->TextureCoordinate.X = 0.0f;
		ArcVert->TextureCoordinate.Y = ((float)RingIdx / NumRings);
	}

	// Then rotate this arc NumSides+1 times.
	for (int32 SideIdx = 0; SideIdx < NumSides + 1; SideIdx++)
	{
		const FRotator ArcRotator(0, 360.f * ((float)SideIdx / NumSides), 0);
		const FRotationMatrix ArcRot(ArcRotator);
		const float XTexCoord = ((float)SideIdx / NumSides);

		for (int32 VertIdx = 0; VertIdx < NumRings + 1; VertIdx++)
		{
			int32 VIx = (NumRings + 1)*SideIdx + VertIdx;

			Verts[VIx].Position = ArcRot.TransformPosition(ArcVerts[VertIdx].Position);

			Verts[VIx].SetTangents(
				ArcRot.TransformVector(ArcVerts[VertIdx].TangentX),
				ArcRot.TransformVector(ArcVerts[VertIdx].GetTangentY()),
				ArcRot.TransformVector(ArcVerts[VertIdx].TangentZ)
				);

			Verts[VIx].TextureCoordinate.X = XTexCoord;
			Verts[VIx].TextureCoordinate.Y = ArcVerts[VertIdx].TextureCoordinate.Y;
		}
	}

	FDynamicMeshBuilder MeshBuilder;
	{
		// Add all of the vertices to the mesh.
		for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
		{
			MeshBuilder.AddVertex(Verts[VertIdx]);
		}

		// Add all of the triangles to the mesh.
		for (int32 SideIdx = 0; SideIdx < NumSides; SideIdx++)
		{
			const int32 a0start = (SideIdx + 0) * (NumRings + 1);
			const int32 a1start = (SideIdx + 1) * (NumRings + 1);

			for (int32 RingIdx = 0; RingIdx < NumRings; RingIdx++)
			{
				MeshBuilder.AddTriangle(a0start + RingIdx + 0, a1start + RingIdx + 0, a0start + RingIdx + 1);
				MeshBuilder.AddTriangle(a1start + RingIdx + 0, a1start + RingIdx + 1, a0start + RingIdx + 1);
			}
		}

	}
	MeshBuilder.Draw(PDI, ElemTM.ToMatrixWithScale(), MaterialRenderProxy, SDPG_World,0.f);


	FMemory::Free(Verts);
	FMemory::Free(ArcVerts);
}

/////////////////////////////////////////////////////////////////////////////////////
// FKConvexElem
/////////////////////////////////////////////////////////////////////////////////////

void FKConvexElem::DrawElemWire(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const float Scale, const FColor Color) const
{
#if WITH_PHYSX

	PxConvexMesh* Mesh = ConvexMesh;

	if(Mesh)
	{
		// Draw each triangle that makes up the convex hull
		PxU32 NbVerts = Mesh->getNbVertices();
		const PxVec3* Vertices = Mesh->getVertices();
		
		// ElemTM is element transform, but geometry is stored in body space, so we need to remove body->element transform
		FTransform RenderTM = Transform.GetRelativeTransformReverse(ElemTM);

		TArray<FVector> TransformedVerts;
		TransformedVerts.AddUninitialized(NbVerts);
		for(PxU32 i=0; i<NbVerts; i++)
		{
			TransformedVerts[i] = RenderTM.TransformPosition(P2UVector(Vertices[i]) * Scale);
		}
						
		const PxU8* PIndexBuffer = Mesh->getIndexBuffer();
		PxU32 NbPolygons = Mesh->getNbPolygons();

		for(PxU32 i=0;i<NbPolygons;i++)
		{
			PxHullPolygon Data;
			bool bStatus = Mesh->getPolygonData(i, Data);
			check(bStatus);

			const PxU8* PIndices = PIndexBuffer + Data.mIndexBase;
		
			for(PxU16 j=0;j<Data.mNbVerts;j++)
			{
				// Get the verts that make up this line.
				int32 I0 = PIndices[j];
				int32 I1 = (j == Data.mNbVerts - 1) ? PIndices[0] : PIndices[j + 1];

				// Loop back last and first vertices
				if(j==Data.mNbVerts - 1)
				{
					I1 = PIndices[0];
				}

				PDI->DrawLine( TransformedVerts[I0], TransformedVerts[I1], Color, SDPG_World );
			}
		}
	}
	else
	{
		UE_LOG(LogPhysics, Log, TEXT("FKConvexElem::DrawElemWire : No ConvexMesh, so unable to draw."));
	}
#endif // WITH_PHYSX
}

void FKConvexElem::AddCachedSolidConvexGeom(TArray<FDynamicMeshVertex>& VertexBuffer, TArray<int32>& IndexBuffer, const FColor VertexColor) const
{
#if WITH_PHYSX
	// We always want to generate 'non-mirrored geometry', so if all we have is flipped, we have to un-flip it in this function
	bool bIsMirrored = false;
	const PxConvexMesh* ConvexMeshToUse = nullptr; 
	if (ConvexMesh != nullptr)
	{
		ConvexMeshToUse = ConvexMesh;
	}
	else if (ConvexMeshNegX != nullptr)
	{
		ConvexMeshToUse = ConvexMeshNegX;
		bIsMirrored = true;
	}

	if(ConvexMeshToUse)
	{
		int32 StartVertOffset = VertexBuffer.Num();

		// get PhysX data
		const PxVec3* PVertices = ConvexMeshToUse->getVertices();
		const PxU8* PIndexBuffer = ConvexMeshToUse->getIndexBuffer();
		PxU32 NbPolygons = ConvexMeshToUse->getNbPolygons();

		FVector Scale3D = bIsMirrored ? FVector(-1, 1, 1) : FVector(1, 1, 1);

		for(PxU32 i=0;i<NbPolygons;i++)
		{
			PxHullPolygon Data;
			bool bStatus = ConvexMeshToUse->getPolygonData(i, Data);
			check(bStatus);

			const PxU8* indices = PIndexBuffer + Data.mIndexBase;

			// create tangents from the first and second vertices of each polygon
			const FVector TangentX = P2UVector(PVertices[indices[1]]-PVertices[indices[0]]).GetSafeNormal();
			const FVector TangentZ = FVector(Data.mPlane[0], Data.mPlane[1], Data.mPlane[2]).GetSafeNormal();
			const FVector TangentY = (TangentX ^ TangentZ).GetSafeNormal();

			// add vertices 
			for(PxU32 j=0;j<Data.mNbVerts;j++)
			{
				int32 VertIndex = indices[j];

				FDynamicMeshVertex Vert1;
				Vert1.Position = P2UVector(PVertices[VertIndex]) * Scale3D;
				Vert1.Color = VertexColor;
				Vert1.SetTangents(
					TangentX,
					TangentY,
					TangentZ
					);
				VertexBuffer.Add(Vert1);
			}

			// Add indices
			PxU32 nbTris = Data.mNbVerts - 2;
			for(PxU32 j=0;j<nbTris;j++)
			{
				IndexBuffer.Add(StartVertOffset+0);

				if (bIsMirrored)
				{
					IndexBuffer.Add(StartVertOffset + j + 1);
					IndexBuffer.Add(StartVertOffset + j + 2);
				}
				else
				{
					IndexBuffer.Add(StartVertOffset + j + 2);
					IndexBuffer.Add(StartVertOffset + j + 1);
				}
			}

			StartVertOffset += Data.mNbVerts;
		}
	}
	else
	{
		UE_LOG(LogPhysics, Log, TEXT("FKConvexElem::AddCachedSolidConvexGeom : No ConvexMesh, so unable to draw."));
	}
#endif // WITH_PHYSX
}

/////////////////////////////////////////////////////////////////////////////////////
// FKAggregateGeom
/////////////////////////////////////////////////////////////////////////////////////


void FConvexCollisionVertexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* VertexBufferData = nullptr;
	VertexBufferRHI = RHICreateAndLockVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex), BUF_Static, CreateInfo, VertexBufferData);

	// Copy the vertex data into the vertex buffer.	
	FMemory::Memcpy(VertexBufferData,Vertices.GetData(),Vertices.Num() * sizeof(FDynamicMeshVertex));
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

void FConvexCollisionIndexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* Buffer = nullptr;
	IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32),Indices.Num() * sizeof(int32),BUF_Static,CreateInfo, Buffer);

	// Write the indices to the index buffer.	
	FMemory::Memcpy(Buffer,Indices.GetData(),Indices.Num() * sizeof(int32));
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FConvexCollisionVertexFactory::InitConvexVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer)
{
	if(IsInRenderingThread())
	{
		// Initialize the vertex factory's stream components.
		FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
		NewData.TextureCoordinates.Add(
			FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
			);
		NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
		NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
		SetData(NewData);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitConvexCollisionVertexFactory,
			FConvexCollisionVertexFactory*,VertexFactory,this,
			const FConvexCollisionVertexBuffer*,VertexBuffer,VertexBuffer,
			{
				// Initialize the vertex factory's stream components.
				FDataType NewData;
				NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
				NewData.TextureCoordinates.Add(
					FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
					);
				NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
				NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
				VertexFactory->SetData(NewData);
			});
	}
}

void FKAggregateGeom::GetAggGeom(const FTransform& Transform, const FColor Color, const FMaterialRenderProxy* MatInst, bool bPerHullColor, bool bDrawSolid, bool bUseEditorDepthTest, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	const FVector Scale3D = Transform.GetScale3D();
	FTransform ParentTM = Transform;
	ParentTM.RemoveScaling();

	for (int32 i = 0; i < SphereElems.Num(); i++)
	{
		FTransform ElemTM = SphereElems[i].GetTransform();
		ElemTM.ScaleTranslation(Scale3D);
		ElemTM *= ParentTM;

		if (bDrawSolid)
			SphereElems[i].GetElemSolid(ElemTM, Scale3D, MatInst, ViewIndex, Collector);
		else
			SphereElems[i].DrawElemWire(Collector.GetPDI(ViewIndex), ElemTM, Scale3D, Color);
	}

	for (int32 i = 0; i < BoxElems.Num(); i++)
	{
		FTransform ElemTM = BoxElems[i].GetTransform();
		ElemTM.ScaleTranslation(Scale3D);
		ElemTM *= ParentTM;

		if (bDrawSolid)
			BoxElems[i].GetElemSolid(ElemTM, Scale3D, MatInst, ViewIndex, Collector);
		else
			BoxElems[i].DrawElemWire(Collector.GetPDI(ViewIndex), ElemTM, Scale3D, Color);
	}

	for (int32 i = 0; i < SphylElems.Num(); i++)
	{
		FTransform ElemTM = SphylElems[i].GetTransform();
		ElemTM.ScaleTranslation(Scale3D);
		ElemTM *= ParentTM;

		if (bDrawSolid)
			SphylElems[i].GetElemSolid(ElemTM, Scale3D, MatInst, ViewIndex, Collector);
		else
			SphylElems[i].DrawElemWire(Collector.GetPDI(ViewIndex), ElemTM, Scale3D, Color);
	}
	
	if(ConvexElems.Num() > 0)
	{
		if(bDrawSolid)
		{
			// Cache collision vertex/index buffer
			if(!RenderInfo)
			{
				//@todo - parallelrendering, remove const cast
				FKAggregateGeom& ThisGeom = const_cast<FKAggregateGeom&>(*this);
				ThisGeom.RenderInfo = new FKConvexGeomRenderInfo();
				ThisGeom.RenderInfo->VertexBuffer = new FConvexCollisionVertexBuffer();
				ThisGeom.RenderInfo->IndexBuffer = new FConvexCollisionIndexBuffer();

				for(int32 i=0; i<ConvexElems.Num(); i++)
				{
					// Get vertices/triangles from this hull.
					ConvexElems[i].AddCachedSolidConvexGeom(ThisGeom.RenderInfo->VertexBuffer->Vertices, ThisGeom.RenderInfo->IndexBuffer->Indices, FColor::White);
				}

				// Only continue if we actually got some valid geometry
				// Will crash if we try to init buffers with no data
				if(ThisGeom.RenderInfo->HasValidGeometry())
				{
					ThisGeom.RenderInfo->VertexBuffer->InitResource();
					ThisGeom.RenderInfo->IndexBuffer->InitResource();

					ThisGeom.RenderInfo->CollisionVertexFactory = new FConvexCollisionVertexFactory(RenderInfo->VertexBuffer);
					ThisGeom.RenderInfo->CollisionVertexFactory->InitResource();
				}
			}

			// If we have geometry to draw, do so
			if(RenderInfo->HasValidGeometry())
			{
				// Calculate transform
				FTransform LocalToWorld = FTransform(FQuat::Identity, FVector::ZeroVector, Scale3D) * ParentTM;

				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = RenderInfo->IndexBuffer;
				Mesh.VertexFactory = RenderInfo->CollisionVertexFactory;
				Mesh.MaterialRenderProxy = MatInst;
				FBoxSphereBounds WorldBounds, LocalBounds;
				CalcBoxSphereBounds(WorldBounds, LocalToWorld);
				CalcBoxSphereBounds(LocalBounds, FTransform::Identity);
			    BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(LocalToWorld.ToMatrixWithScale(), WorldBounds, LocalBounds, true, bUseEditorDepthTest);
			 	// previous l2w not used so treat as static
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = RenderInfo->IndexBuffer->Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = RenderInfo->VertexBuffer->Vertices.Num() - 1;
				Mesh.ReverseCulling = LocalToWorld.GetDeterminant() < 0.0f ? true : false;
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
		else
		{
			for(int32 i=0; i<ConvexElems.Num(); i++)
			{
				FColor ConvexColor = bPerHullColor ? DebugUtilColor[i%NUM_DEBUG_UTIL_COLORS] : Color;
				FTransform ElemTM = ConvexElems[i].GetTransform();
				ElemTM *= Transform;
				ConvexElems[i].DrawElemWire(Collector.GetPDI(ViewIndex), ElemTM, 1.f, ConvexColor);	//we pass in 1 for scale because the ElemTM already has the scale baked into it
			}
		}
	}
}

/** Release the RenderInfo (if its there) and safely clean up any resources. Call on the game thread. */
void FKAggregateGeom::FreeRenderInfo()
{
	// See if we have rendering resources to free
	if(RenderInfo)
	{
		// Should always have these if RenderInfo exists
		check(RenderInfo->VertexBuffer);
		check(RenderInfo->IndexBuffer);

		// Fire off commands to free these resources
		BeginReleaseResource(RenderInfo->VertexBuffer);
		BeginReleaseResource(RenderInfo->IndexBuffer);

		// May not exist if no geometry was available
		if(RenderInfo->CollisionVertexFactory != NULL)
		{
			BeginReleaseResource(RenderInfo->CollisionVertexFactory);
		}

		// Wait until those commands have been processed
		FRenderCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();

		// Release memory.
		delete RenderInfo->VertexBuffer;
		delete RenderInfo->IndexBuffer;

		if (RenderInfo->CollisionVertexFactory != NULL)
		{
			delete RenderInfo->CollisionVertexFactory;
		}

		delete RenderInfo;
		RenderInfo = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// UPhysicsAsset
/////////////////////////////////////////////////////////////////////////////////////

FTransform GetSkelBoneTransform(int32 BoneIndex, const TArray<FTransform>& SpaceBases, const FTransform& LocalToWorld)
{
	if(BoneIndex != INDEX_NONE && BoneIndex < SpaceBases.Num())
	{
		return SpaceBases[BoneIndex] * LocalToWorld;
	}
	else
	{
		return FTransform::Identity;
	}
}

void UPhysicsAsset::GetCollisionMesh(int32 ViewIndex, FMeshElementCollector& Collector, const USkeletalMesh* SkelMesh, const TArray<FTransform>& SpaceBases, const FTransform& LocalToWorld, const FVector& Scale3D)
{
	for( int32 i=0; i<SkeletalBodySetups.Num(); i++)
	{
		int32 BoneIndex = SkelMesh->RefSkeleton.FindBoneIndex( SkeletalBodySetups[i]->BoneName );
		
		FColor* BoneColor = (FColor*)( &SkeletalBodySetups[i] );

		FTransform BoneTransform = GetSkelBoneTransform(BoneIndex, SpaceBases, LocalToWorld);
		// SkelBoneTransform should have the appropriate scale baked in from Component and Import Transform.
		// BoneTransform.SetScale3D(Scale3D);
		if (SkeletalBodySetups[i]->bCreatedPhysicsMeshes)
		{
			SkeletalBodySetups[i]->AggGeom.GetAggGeom(BoneTransform, *BoneColor, NULL, false, false, false, ViewIndex, Collector);
		}
	}
}

void UPhysicsAsset::GetUsedMaterials(TArray<UMaterialInterface*>& Materials)
{
	for (int32 i = 0; i < ConstraintSetup.Num(); i++)
	{
		FConstraintInstance& Instance = ConstraintSetup[i]->DefaultInstance;
		Instance.GetUsedMaterials(Materials);
	}
}

void UPhysicsAsset::DrawConstraints(int32 ViewIndex, FMeshElementCollector& Collector, const USkeletalMesh* SkelMesh, const TArray<FTransform>& SpaceBases, const FTransform& LocalToWorld, float Scale)
{
	for (int32 i = 0; i < ConstraintSetup.Num(); i++)
	{
		FConstraintInstance& Instance = ConstraintSetup[i]->DefaultInstance;

		// Get each constraint frame in world space.
		FTransform Con1Frame = FTransform::Identity;
		int32 Bone1Index = SkelMesh->RefSkeleton.FindBoneIndex(Instance.ConstraintBone1);
		if (Bone1Index != INDEX_NONE)
		{
			FTransform Body1TM = GetSkelBoneTransform(Bone1Index, SpaceBases, LocalToWorld);
			Body1TM.RemoveScaling();
			Con1Frame = Instance.GetRefFrame(EConstraintFrame::Frame1) * Body1TM;
		}

		FTransform Con2Frame = FTransform::Identity;
		int32 Bone2Index = SkelMesh->RefSkeleton.FindBoneIndex(Instance.ConstraintBone2);
		if (Bone2Index != INDEX_NONE)
		{
			FTransform Body2TM = GetSkelBoneTransform(Bone2Index, SpaceBases, LocalToWorld);
			Body2TM.RemoveScaling();
			Con2Frame = Instance.GetRefFrame(EConstraintFrame::Frame2) * Body2TM;
		}


		Instance.DrawConstraint(ViewIndex, Collector, Scale, 1.f, true, true, Con1Frame, Con2Frame, false);
	}
}
static void DrawLinearLimit(FPrimitiveDrawInterface* PDI, const FVector& Origin, const FVector& Axis, const FVector& Orth, float LinearLimitRadius, bool bLinearLimited, float DrawScale)
{
	float ScaledLimitSize = LimitRenderSize * DrawScale;

	if (bLinearLimited)
	{
		FVector Start = Origin - LinearLimitRadius * Axis;
		FVector End = Origin + LinearLimitRadius * Axis;

		PDI->DrawLine(  Start, End, JointLimitColor, SDPG_World );

		// Draw ends indicating limit.
		PDI->DrawLine(  Start - (0.2f * ScaledLimitSize * Orth), Start + (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
		PDI->DrawLine(  End - (0.2f * ScaledLimitSize * Orth), End + (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
	}
	else
	{
		FVector Start = Origin - 1.5f * ScaledLimitSize * Axis;
		FVector End = Origin + 1.5f * ScaledLimitSize * Axis;

		PDI->DrawLine(  Start, End, JointRefColor, SDPG_World );

		// Draw arrow heads.
		PDI->DrawLine(  Start, Start + (0.2f * ScaledLimitSize * Axis) + (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
		PDI->DrawLine(  Start, Start + (0.2f * ScaledLimitSize * Axis) - (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );

		PDI->DrawLine(  End, End - (0.2f * ScaledLimitSize * Axis) + (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
		PDI->DrawLine(  End, End - (0.2f * ScaledLimitSize * Axis) - (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
	}
}

//creates fan shape along visualized axis for rotation axis of length Length
FMatrix HelpBuildFan(const FTransform& Con1Frame, const FTransform& Con2Frame, EAxis::Type DrawOnAxis, EAxis::Type RotationAxis, float Length)
{
	FVector Con1DrawOnAxis = Con1Frame.GetScaledAxis(DrawOnAxis);
	FVector Con2DrawOnAxis = Con2Frame.GetScaledAxis(DrawOnAxis);

	FVector Con1RotationAxis = Con1Frame.GetScaledAxis(RotationAxis);
	FVector Con2RotationAxis = Con2Frame.GetScaledAxis(RotationAxis);

	// Rotate parent twist ref axis
	FQuat Con2ToCon1Rot = FQuat::FindBetween(Con2RotationAxis, Con1RotationAxis);
	FVector Con2InCon1DrawOnAxis = Con2ToCon1Rot.RotateVector(Con2DrawOnAxis);

	FTransform ConeLimitTM(Con2InCon1DrawOnAxis, Con1RotationAxis ^ Con2InCon1DrawOnAxis, Con1RotationAxis, Con1Frame.GetTranslation());
	FMatrix ConeToWorld = FScaleMatrix(FVector(Length * 0.9f)) * ConeLimitTM.ToMatrixWithScale();
	return ConeToWorld;
}

//builds radians for limit based on limit type
float HelpBuildAngle(float LimitAngle, EAngularConstraintMotion LimitType)
{
	switch (LimitType)
	{
		case ACM_Free: return PI;
		case ACM_Locked: return 0.f;
		default: return FMath::DegreesToRadians(LimitAngle);
	}
}


FPrimitiveDrawInterface* FConstraintInstance::FPDIOrCollector::GetPDI() const
{
	return PDI ? PDI : Collector->GetPDI(ViewIndex);
}

void FConstraintInstance::FPDIOrCollector::DrawCylinder(const FVector& Start, const FVector& End, float Thickness, FMaterialRenderProxy* MaterialProxy, ESceneDepthPriorityGroup DepthPriority) const
{
	if (HasCollector())
	{
		GetCylinderMesh(Start, End, Thickness, 4, MaterialProxy, DepthPriority, ViewIndex, *Collector);
	}
	else
	{
		::DrawCylinder(PDI, Start, End, Thickness, 4, MaterialProxy, DepthPriority);
	}
}

void FConstraintInstance::GetUsedMaterials(TArray<UMaterialInterface*>& Materials)
{
	Materials.AddUnique(GEngine->ConstraintLimitMaterialX);
	Materials.AddUnique(GEngine->ConstraintLimitMaterialXAxis);
	Materials.AddUnique(GEngine->ConstraintLimitMaterialY);
	Materials.AddUnique(GEngine->ConstraintLimitMaterialYAxis);
	Materials.AddUnique(GEngine->ConstraintLimitMaterialZ);
	Materials.AddUnique(GEngine->ConstraintLimitMaterialZAxis);
}

void FConstraintInstance::DrawConstraintImp(const FPDIOrCollector& PDIOrCollector, float Scale, float LimitDrawScale, bool bDrawLimits, bool bDrawSelected, const FTransform& Con1Frame, const FTransform& Con2Frame, bool bDrawAsPoint) const
{
	// Do nothing if we're shipping
#if UE_BUILD_SHIPPING
	return;
#endif

	const ESceneDepthPriorityGroup Layer = ESceneDepthPriorityGroup::SDPG_World;
	FPrimitiveDrawInterface* PDI = PDIOrCollector.GetPDI();

	check((GEngine->ConstraintLimitMaterialX != nullptr) && (GEngine->ConstraintLimitMaterialY != nullptr) && (GEngine->ConstraintLimitMaterialZ != nullptr));

	static UMaterialInterface * LimitMaterialX = GEngine->ConstraintLimitMaterialX;
	static UMaterialInterface * LimitMaterialXAxis = GEngine->ConstraintLimitMaterialXAxis;
	static UMaterialInterface * LimitMaterialY = GEngine->ConstraintLimitMaterialY;
	static UMaterialInterface * LimitMaterialYAxis = GEngine->ConstraintLimitMaterialYAxis;
	static UMaterialInterface * LimitMaterialZ = GEngine->ConstraintLimitMaterialZ;
	static UMaterialInterface * LimitMaterialZAxis = GEngine->ConstraintLimitMaterialZAxis;
	
	FVector Con1Pos = Con1Frame.GetTranslation();
	FVector Con2Pos = Con2Frame.GetTranslation();

	float Length = (bDrawSelected ? SelectedJointRenderSize : UnselectedJointRenderSize) * Scale;
	float Thickness = JointRenderThickness;

	// Special mode for drawing joints just as points..
	if(bDrawAsPoint && !bDrawSelected)
	{
		PDI->DrawPoint( Con1Frame.GetTranslation(), JointUnselectedColor, 4.f, ESceneDepthPriorityGroup::SDPG_Foreground );
		PDI->DrawPoint( Con2Frame.GetTranslation(), JointUnselectedColor, 4.f, ESceneDepthPriorityGroup::SDPG_Foreground );

		// do nothing else in this mode.
		return;
	}

	if (bDrawLimits)
	{

		//////////////////////////////////////////////////////////////////////////
		// ANGULAR DRAWING

		//Draw limits first as they are transparent and need to be under coordinate axes
		const bool bLockSwing1 = GetAngularSwing1Motion() == ACM_Locked;
		const bool bLockSwing2 = GetAngularSwing2Motion() == ACM_Locked;
		const bool bLockAllSwing = bLockSwing1 && bLockSwing2;

		// If swing is limited (but not locked) - draw the limit cone.
		if (!bLockAllSwing)
		{
			if (ProfileInstance.ConeLimit.Swing1Motion == ACM_Free && ProfileInstance.ConeLimit.Swing2Motion == ACM_Free)
			{
				if (PDIOrCollector.HasCollector())
				{
					GetSphereMesh(Con1Pos, FVector(Length*0.9f), DrawConeLimitSides, DrawConeLimitSides, LimitMaterialX->GetRenderProxy(false), Layer, false, PDIOrCollector.ViewIndex, *PDIOrCollector.Collector);
				}
				else
				{
					DrawSphere(PDI, Con1Pos, FRotator::ZeroRotator, FVector(Length * 0.9f), DrawConeLimitSides, DrawConeLimitSides, LimitMaterialX->GetRenderProxy(false), Layer);
				}
			}
			else
			{
				FTransform ConeLimitTM = Con2Frame;
				ConeLimitTM.SetTranslation(Con1Frame.GetTranslation());

				const float Swing1Ang = HelpBuildAngle(GetAngularSwing1Limit(), GetAngularSwing1Motion());
				const float Swing2Ang = HelpBuildAngle(GetAngularSwing2Limit(), GetAngularSwing2Motion());
				FMatrix ConeToWorld = FScaleMatrix(FVector(Length * 0.9f)) * ConeLimitTM.ToMatrixWithScale();

				if (PDIOrCollector.HasCollector())
				{
					GetConeMesh(ConeToWorld, FMath::RadiansToDegrees(Swing1Ang), FMath::RadiansToDegrees(Swing2Ang), DrawConeLimitSides, LimitMaterialX->GetRenderProxy(false), Layer, PDIOrCollector.ViewIndex, *PDIOrCollector.Collector);
				}
				else
				{
					DrawCone(PDI, ConeToWorld, Swing1Ang, Swing2Ang, DrawConeLimitSides, false, JointLimitColor, LimitMaterialX->GetRenderProxy(false), Layer);
				}
			}
		}

		//twist
		if (GetAngularTwistMotion() != ACM_Locked)
		{
			FMatrix ConeToWorld = HelpBuildFan(Con1Frame, Con2Frame, EAxis::Y, EAxis::X, Length);
			float Limit = HelpBuildAngle(GetAngularTwistLimit(), GetAngularTwistMotion());
			if (PDIOrCollector.HasCollector())
			{
				GetConeMesh(ConeToWorld, FMath::RadiansToDegrees(Limit), 0, DrawConeLimitSides, LimitMaterialY->GetRenderProxy(false), Layer, PDIOrCollector.ViewIndex, *PDIOrCollector.Collector);
			}
			else
			{
				DrawCone(PDI, ConeToWorld, Limit, 0, DrawConeLimitSides, false, JointLimitColor, LimitMaterialY->GetRenderProxy(false), Layer);
			}
		}
	}


	//////////////////////////////////////////////////////////////////////////
	// COORDINATE AXES
	FVector Position = Con1Frame.GetTranslation();

	PDIOrCollector.DrawCylinder(Position, Position + Length * Con1Frame.GetScaledAxis(EAxis::X), Thickness, LimitMaterialXAxis->GetRenderProxy(false), Layer);
	PDIOrCollector.DrawCylinder(Position, Position + Length * Con1Frame.GetScaledAxis(EAxis::Y), Thickness, LimitMaterialYAxis->GetRenderProxy(false), Layer);
	PDIOrCollector.DrawCylinder(Position, Position + Length * Con1Frame.GetScaledAxis(EAxis::Z), Thickness, LimitMaterialZAxis->GetRenderProxy(false), Layer);

	PDIOrCollector.DrawCylinder(Position, Position + Length * Con2Frame.GetScaledAxis(EAxis::X), Thickness, LimitMaterialXAxis->GetRenderProxy(false), Layer);
	PDIOrCollector.DrawCylinder(Position, Position + Length * Con2Frame.GetScaledAxis(EAxis::Y), Thickness, LimitMaterialYAxis->GetRenderProxy(false), Layer);
	PDIOrCollector.DrawCylinder(Position, Position + Length * Con2Frame.GetScaledAxis(EAxis::Z), Thickness, LimitMaterialZAxis->GetRenderProxy(false), Layer);


	//Draw arrow on twist axist
	{
		FTransform ConeLimitTM = Con2Frame;
		ConeLimitTM.SetTranslation(Con1Frame.GetTranslation() + Length*1.05*Con2Frame.GetScaledAxis(EAxis::X));

		const float Swing1Ang = PI / 4;
		const float Swing2Ang = PI / 4;
		FMatrix ConeToWorld = FScaleMatrix(FVector(Length * -0.1f)) * ConeLimitTM.ToMatrixWithScale();

		if (PDIOrCollector.HasCollector())
		{
			GetConeMesh(ConeToWorld, FMath::RadiansToDegrees(Swing1Ang), FMath::RadiansToDegrees(Swing2Ang), DrawConeLimitSides, LimitMaterialXAxis->GetRenderProxy(false), Layer, PDIOrCollector.ViewIndex, *PDIOrCollector.Collector);
		}
		else
		{
			DrawCone(PDI, ConeToWorld, Swing1Ang, Swing2Ang, DrawConeLimitSides, false, JointLimitColor, LimitMaterialXAxis->GetRenderProxy(false), Layer);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// LINEAR DRAWING

	//TODO: Move this all into a draw function on linear constraint
	bool bLinearXLocked = (GetLinearXMotion() == LCM_Locked) || (GetLinearXMotion() == LCM_Limited && GetLinearLimit() < RB_MinSizeToLockDOF);
	bool bLinearYLocked = (GetLinearYMotion() == LCM_Locked) || (GetLinearYMotion() == LCM_Limited && GetLinearLimit() < RB_MinSizeToLockDOF);
	bool bLinearZLocked = (GetLinearZMotion() == LCM_Locked) || (GetLinearZMotion() == LCM_Limited && GetLinearLimit() < RB_MinSizeToLockDOF);

	if(!bLinearXLocked)
	{
		bool bLinearXLimited = ( GetLinearXMotion() == LCM_Limited && GetLinearLimit() >= RB_MinSizeToLockDOF );
		DrawLinearLimit(PDI, Con2Frame.GetTranslation(), Con2Frame.GetScaledAxis( EAxis::X ), Con2Frame.GetScaledAxis( EAxis::Z ), GetLinearLimit(), bLinearXLimited, LimitDrawScale);
	}

	if(!bLinearYLocked)
	{
		bool bLinearYLimited = ( GetLinearYMotion() == LCM_Limited && GetLinearLimit() >= RB_MinSizeToLockDOF );
		DrawLinearLimit(PDI, Con2Frame.GetTranslation(), Con2Frame.GetScaledAxis( EAxis::Y ), Con2Frame.GetScaledAxis( EAxis::Z ), GetLinearLimit(), bLinearYLimited, LimitDrawScale);
	}

	if(!bLinearZLocked)
	{
		bool bLinearZLimited = ( GetLinearZMotion() == LCM_Limited && GetLinearLimit() >= RB_MinSizeToLockDOF );
		DrawLinearLimit(PDI, Con2Frame.GetTranslation(), Con2Frame.GetScaledAxis( EAxis::Z ), Con2Frame.GetScaledAxis( EAxis::X ), GetLinearLimit(), bLinearZLimited, LimitDrawScale);
	}
}
