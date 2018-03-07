
#include "ClothingMeshUtils.h"

#include "UnrealMathUtility.h"
#include "LogMacros.h"

#if WITH_EDITOR
#include "NotificationManager.h"
#include "SNotificationList.h"
#endif

DEFINE_LOG_CATEGORY(LogClothingMeshUtils)

#define LOCTEXT_NAMESPACE "ClothingMeshUtils"

namespace ClothingMeshUtils
{
	/** 
	 * Gets the best match triangle for a specified position from the triangles in Mesh.
	 * Performs no validation on the incoming mesh data, the mesh data should be verified
	 * to be valid before using this function
	 */
	int32 GetBestTriangleBaseIndex(const ClothMeshDesc& Mesh, const FVector& Position)
	{
		float MinimumDistanceSq = MAX_flt;
		int32 ClosestBaseIndex = INDEX_NONE;

		const int32 NumTriangles = Mesh.Indices.Num() / 3;
		for(int32 TriIdx = 0; TriIdx < NumTriangles; ++TriIdx)
		{
			int32 TriBaseIdx = TriIdx * 3;

			const uint32 IA = Mesh.Indices[TriBaseIdx + 0];
			const uint32 IB = Mesh.Indices[TriBaseIdx + 1];
			const uint32 IC = Mesh.Indices[TriBaseIdx + 2];

			const FVector& A = Mesh.Positions[IA];
			const FVector& B = Mesh.Positions[IB];
			const FVector& C = Mesh.Positions[IC];

			FVector PointOnTri = FMath::ClosestPointOnTriangleToPoint(Position, A, B, C);
			float DistSq = (PointOnTri - Position).SizeSquared();

			if(DistSq < MinimumDistanceSq)
			{
				MinimumDistanceSq = DistSq;
				ClosestBaseIndex = TriBaseIdx;
			}
		}

		return ClosestBaseIndex;
	}

	void GenerateMeshToMeshSkinningData(TArray<FMeshToMeshVertData>& OutSkinningData, const ClothMeshDesc& TargetMesh, const TArray<FVector>* TargetTangents, const ClothMeshDesc& SourceMesh)
	{
		if(!TargetMesh.HasValidMesh())
		{
			UE_LOG(LogClothingMeshUtils, Warning, TEXT("Failed to generate mesh to mesh skinning data. Invalid Target Mesh."));
			return;
		}

		if(!SourceMesh.HasValidMesh())
		{
			UE_LOG(LogClothingMeshUtils, Warning, TEXT("Failed to generate mesh to mesh skinning data. Invalid Source Mesh."));
			return;
		}

		const int32 NumMesh0Verts = TargetMesh.Positions.Num();
		const int32 NumMesh0Normals = TargetMesh.Normals.Num();
		const int32 NumMesh0Tangents = TargetTangents ? TargetTangents->Num() : 0;

		const int32 NumMesh1Verts = SourceMesh.Positions.Num();
		const int32 NumMesh1Normals = SourceMesh.Normals.Num();
		const int32 NumMesh1Indices = SourceMesh.Indices.Num();

		// Check we have properly formed triangles
		check(NumMesh1Indices % 3 == 0);

		const int32 NumMesh1Triangles = NumMesh1Indices / 3;

		// Check mesh data to make sure we have the same number of each element
		if(NumMesh0Verts != NumMesh0Normals || (TargetTangents && NumMesh0Tangents != NumMesh0Verts))
		{
			UE_LOG(LogClothingMeshUtils, Warning, TEXT("Can't generate mesh to mesh skinning data, Mesh0 data is missing verts."));
			return;
		}

		if(NumMesh1Verts != NumMesh1Normals)
		{
			UE_LOG(LogClothingMeshUtils, Warning, TEXT("Can't generate mesh to mesh skinning data, Mesh1 data is missing verts."));
			return;
		}

		OutSkinningData.Reserve(NumMesh0Verts);

		// For all mesh0 verts
		for(int32 VertIdx0 = 0; VertIdx0 < NumMesh0Verts; ++VertIdx0)
		{
			OutSkinningData.AddZeroed();
			FMeshToMeshVertData& SkinningData = OutSkinningData.Last();

			const FVector& VertPosition = TargetMesh.Positions[VertIdx0];
			const FVector& VertNormal = TargetMesh.Normals[VertIdx0];

			FVector VertTangent;
			if(TargetTangents)
			{
				VertTangent = (*TargetTangents)[VertIdx0];
			}
			else
			{
				FVector Tan0, Tan1;
				VertNormal.FindBestAxisVectors(Tan0, Tan1);
				VertTangent = Tan0;
			}

			int32 ClosestTriangleBaseIdx = GetBestTriangleBaseIndex(SourceMesh, VertPosition);

			check(ClosestTriangleBaseIdx != INDEX_NONE);

			const FVector& A = SourceMesh.Positions[SourceMesh.Indices[ClosestTriangleBaseIdx]];
			const FVector& B = SourceMesh.Positions[SourceMesh.Indices[ClosestTriangleBaseIdx + 1]];
			const FVector& C = SourceMesh.Positions[SourceMesh.Indices[ClosestTriangleBaseIdx + 2]];

			const FVector& NA = SourceMesh.Normals[SourceMesh.Indices[ClosestTriangleBaseIdx]];
			const FVector& NB = SourceMesh.Normals[SourceMesh.Indices[ClosestTriangleBaseIdx + 1]];
			const FVector& NC = SourceMesh.Normals[SourceMesh.Indices[ClosestTriangleBaseIdx + 2]];

			// Before generating the skinning data we need to check for a degenerate triangle.
			// If we find _any_ degenerate triangles we will notify and fail to generate the skinning data
			const FVector TriNormal = FVector::CrossProduct(B - A, C - A);
			if(TriNormal.SizeSquared() < SMALL_NUMBER)
			{
				// Failed, we have 2 identical vertices
				OutSkinningData.Reset();

				// Log and toast
				FText Error = FText::Format(LOCTEXT("DegenerateTriangleError", "Failed to generate skinning data, found conincident vertices in triangle A={0} B={1} C={2}"), FText::FromString(A.ToString()), FText::FromString(B.ToString()), FText::FromString(C.ToString()));

				UE_LOG(LogClothingMeshUtils, Warning, TEXT("%s"), *Error.ToString());

#if WITH_EDITOR
				FNotificationInfo Info(Error);
				Info.ExpireDuration = 5.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
#endif
				return;
			}

			SkinningData.PositionBaryCoordsAndDist = GetPointBaryAndDist(A, B, C, NA, NB, NC, VertPosition);
			SkinningData.NormalBaryCoordsAndDist = GetPointBaryAndDist(A, B, C, NA, NB, NC, VertPosition + VertNormal);
			SkinningData.TangentBaryCoordsAndDist = GetPointBaryAndDist(A, B, C, NA, NB, NC, VertPosition + VertTangent);
			SkinningData.SourceMeshVertIndices[0] = SourceMesh.Indices[ClosestTriangleBaseIdx];
			SkinningData.SourceMeshVertIndices[1] = SourceMesh.Indices[ClosestTriangleBaseIdx + 1];
			SkinningData.SourceMeshVertIndices[2] = SourceMesh.Indices[ClosestTriangleBaseIdx + 2];
			SkinningData.SourceMeshVertIndices[3] = 0;
		}
	}

	FVector4 GetPointBaryAndDist(const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC, const FVector& Point)
	{
		FPlane TrianglePlane(A, B, C);
		const FVector PointOnTriPlane = FVector::PointPlaneProject(Point, TrianglePlane);
		const FVector BaryCoords = FMath::ComputeBaryCentric2D(PointOnTriPlane, A, B, C);
		const FVector NormalAtPoint = TrianglePlane;
		FVector TriPointToVert = Point - PointOnTriPlane;
		TriPointToVert = TriPointToVert.ProjectOnTo(NormalAtPoint);
		float Dist = TriPointToVert.Size();

		float Sign = TrianglePlane.PlaneDot(Point) < 0.0f ? -1.0f : 1.0f;

		return FVector4(BaryCoords, TrianglePlane.PlaneDot(Point));
	}

	void GenerateEmbeddedPositions(const ClothMeshDesc& SourceMesh, TArrayView<const FVector> Positions, TArray<FVector4>& OutEmbeddedPositions, TArray<int32>& OutSourceIndices)
	{
		if(!SourceMesh.HasValidMesh())
		{
			// No valid source mesh
			return;
		}

		const int32 NumPositions = Positions.Num();

		OutEmbeddedPositions.Reset();
		OutEmbeddedPositions.AddUninitialized(NumPositions);

		OutSourceIndices.Reset(NumPositions * 3);

		for(int32 PositionIndex = 0 ; PositionIndex < NumPositions ; ++PositionIndex)
		{
			const FVector& Position = Positions[PositionIndex];

			int32 TriBaseIndex = GetBestTriangleBaseIndex(SourceMesh, Position);

			const int32 IA = SourceMesh.Indices[TriBaseIndex];
			const int32 IB = SourceMesh.Indices[TriBaseIndex + 1];
			const int32 IC = SourceMesh.Indices[TriBaseIndex + 2];

			const FVector& A = SourceMesh.Positions[IA];
			const FVector& B = SourceMesh.Positions[IB];
			const FVector& C = SourceMesh.Positions[IC];

			const FVector& NA = SourceMesh.Normals[IA];
			const FVector& NB = SourceMesh.Normals[IB];
			const FVector& NC = SourceMesh.Normals[IC];

			OutEmbeddedPositions[PositionIndex] = GetPointBaryAndDist(A, B, C, NA, NB, NC, Position);
			OutSourceIndices.Add(IA);
			OutSourceIndices.Add(IB);
			OutSourceIndices.Add(IC);
		}
	}

	void FVertexParameterMapper::Map(TArrayView<const float> Source, TArray<float>& Dest)
	{
		Map(Source, Dest, [](FVector Bary, float A, float B, float C)
		{
			return Bary.X * A + Bary.Y * B + Bary.Z * C;
		});
	}

}

#undef LOCTEXT_NAMESPACE
