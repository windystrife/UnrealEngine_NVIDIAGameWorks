// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SkeletalMeshTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogClothingMeshUtils, Log, All);

namespace ClothingMeshUtils
{
	struct CLOTHINGSYSTEMRUNTIME_API ClothMeshDesc
	{
		ClothMeshDesc(TArrayView<const FVector> InPositions, TArrayView<const FVector> InNormals,  TArrayView<const uint32> InIndices)
			: Positions(InPositions)
			, Normals(InNormals)
			, Indices(InIndices)
		{}

		bool HasValidMesh() const
		{
			return Positions.Num() == Normals.Num() && Indices.Num() % 3 == 0;
		}

		TArrayView<const FVector> Positions;
		TArrayView<const FVector> Normals;
		TArrayView<const uint32> Indices;
	};

	/**
	* Given mesh information for two meshes, generate a list of skinning data to embed mesh0 in mesh1
	* @param OutSkinningData	- Final skinning data to map mesh0 to mesh1
	* @param TargetMesh		- Mesh data for the mesh we are embedding
	* @param TargetTangents		- Optional Tangents for the mesh we are embedding
	* @param Mesh1Normals		- Vertex normals for Mesh1
	* @param Mesh1Indices		- Triangle indices for Mesh1
	*/
	void CLOTHINGSYSTEMRUNTIME_API GenerateMeshToMeshSkinningData(
		TArray<FMeshToMeshVertData>& OutSkinningData,
		const ClothMeshDesc& TargetMesh,
		const TArray<FVector>* TargetTangents,
		const ClothMeshDesc& SourceMesh);

	/** 
	 * Embeds a list of positions into a source mesh
	 * @param SourceMesh The mesh to embed in
	 * @param Positions The positions to embed in SourceMesh
	 * @param OutEmbeddedPositions Embedded version of the original positions, a barycentric coordinate and distance along the normal of the triangle
	 * @param OutSourceIndices Source index list for the embedded positions, 3 per position to denote the source triangle
	 */
	void CLOTHINGSYSTEMRUNTIME_API GenerateEmbeddedPositions(
		const ClothMeshDesc& SourceMesh, 
		TArrayView<const FVector> Positions, 
		TArray<FVector4>& OutEmbeddedPositions, 
		TArray<int32>& OutSourceIndices);

	/**
	* Given a triangle ABC with normals at each vertex NA, NB and NC, get a barycentric coordinate
	* and corresponding distance from the triangle encoded in an FVector4 where the components are
	* (BaryX, BaryY, BaryZ, Dist)
	* @param A		- Position of triangle vertex A
	* @param B		- Position of triangle vertex B
	* @param C		- Position of triangle vertex C
	* @param NA	- Normal at vertex A
	* @param NB	- Normal at vertex B
	* @param NC	- Normal at vertex C
	* @param Point	- Point to calculate Bary+Dist for
	*/
	FVector4 GetPointBaryAndDist(
		const FVector& A,
		const FVector& B,
		const FVector& C,
		const FVector& NA,
		const FVector& NB,
		const FVector& NC,
		const FVector& Point);

	/** 
	 * Object used to map vertex parameters between two meshes using the
	 * same barycentric mesh to mesh mapping data we use for clothing
	 */
	class CLOTHINGSYSTEMRUNTIME_API FVertexParameterMapper
	{
	public:
		FVertexParameterMapper() = delete;
		FVertexParameterMapper(const FVertexParameterMapper& Other) = delete;

		FVertexParameterMapper(TArrayView<const FVector> InMesh0Positions,
			TArrayView<const FVector> InMesh0Normals,
			TArrayView<const FVector> InMesh1Positions,
			TArrayView<const FVector> InMesh1Normals,
			TArrayView<const uint32> InMesh1Indices)
			: Mesh0Positions(InMesh0Positions)
			, Mesh0Normals(InMesh0Normals)
			, Mesh1Positions(InMesh1Positions)
			, Mesh1Normals(InMesh1Normals)
			, Mesh1Indices(InMesh1Indices)
		{

		}

		/** Generic mapping function, can be used to map any type with a provided callable */
		template<typename T, typename Lambda>
		void Map(TArrayView<const T>& SourceData, TArray<T>& DestData, const Lambda& Func)
		{
			// Enforce the interp func signature (returns T and takes a bary and 3 Ts)
			// If you hit this then either the return type isn't T or your arguments aren't convertible to T
			static_assert(TAreTypesEqual<T, typename TDecay<decltype(Func(DeclVal<FVector>(), DeclVal<T>(), DeclVal<T>(), DeclVal<T>()))>::Type>::Value, "Invalid Lambda signature passed to Map");

			const int32 NumMesh0Positions = Mesh0Positions.Num();
			const int32 NumMesh0Normals = Mesh0Normals.Num();

			const int32 NumMesh1Positions = Mesh1Positions.Num();
			const int32 NumMesh1Normals = Mesh1Normals.Num();
			const int32 NumMesh1Indices = Mesh1Indices.Num();

			// Validate mesh data
			check(NumMesh0Positions == NumMesh0Normals);
			check(NumMesh1Positions == NumMesh1Normals);
			check(NumMesh1Indices % 3 == 0);
			check(SourceData.Num() == NumMesh1Positions);

			if(DestData.Num() != NumMesh0Positions)
			{
				DestData.Reset();
				DestData.AddUninitialized(NumMesh0Positions);
			}

			ClothMeshDesc SourceMeshDesc(Mesh1Positions, Mesh1Normals, Mesh1Indices);

			TArray<FVector4> EmbeddedPositions;
			TArray<int32> SourceIndices;
			ClothingMeshUtils::GenerateEmbeddedPositions(SourceMeshDesc, Mesh0Positions, EmbeddedPositions, SourceIndices);

			for(int32 DestVertIndex = 0 ; DestVertIndex < NumMesh0Positions ; ++DestVertIndex)
			{
				// Truncate the distance from the position data
				FVector Bary = EmbeddedPositions[DestVertIndex];

				const int32 SourceTriBaseIdx = DestVertIndex * 3;
				T A = SourceData[SourceIndices[SourceTriBaseIdx + 0]];
				T B = SourceData[SourceIndices[SourceTriBaseIdx + 1]];
				T C = SourceData[SourceIndices[SourceTriBaseIdx + 2]];

				T& DestVal = DestData[DestVertIndex];

				// If we're super close to a vertex just take it's value.
				// Otherwise call the provided interp lambda
				const static FVector OneVec(1.0f, 1.0f, 1.0f);
				FVector DiffVec = OneVec - Bary;
				if(FMath::Abs(DiffVec.X) <= SMALL_NUMBER)
				{
					DestVal = A;
				}
				else if(FMath::Abs(DiffVec.Y) <= SMALL_NUMBER)
				{
					DestVal = B;
				}
				else if(FMath::Abs(DiffVec.Z) <= SMALL_NUMBER)
				{
					DestVal = C;
				}
				else
				{
					DestData[DestVertIndex] = Func(Bary, A, B, C);
				}
			}
		}

		// Defined type mappings for brevity
		void Map(TArrayView<const float> Source, TArray<float>& Dest);

	private:

		TArrayView<const FVector> Mesh0Positions;
		TArrayView<const FVector> Mesh0Normals;
		TArrayView<const FVector> Mesh1Positions;
		TArrayView<const FVector> Mesh1Normals;
		TArrayView<const uint32> Mesh1Indices;
	};
}