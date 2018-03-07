// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshRendering.cpp: Mesh rendering implementation.
=============================================================================*/

#include "MeshRendering.h"
#include "EngineDefines.h"
#include "ShowFlags.h"
#include "RHI.h"
#include "RenderResource.h"
#include "HitProxies.h"
#include "RenderingThread.h"
#include "VertexFactory.h"
#include "TextureResource.h"
#include "PackedNormal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/App.h"
#include "MaterialUtilities.h"
#include "Misc/FileHelper.h"
#include "RawMesh.h"
#include "SceneView.h"
#include "SkeletalMeshTypes.h"
#include "MeshBatch.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "LocalVertexFactory.h"
#include "SkeletalMeshTypes.h"
#include "DrawingPolicy.h"

#include "RendererInterface.h"
#include "EngineModule.h"
#include "LightMapHelpers.h"

#define SHOW_WIREFRAME_MESH 0
#define SAVE_INTERMEDIATE_TEXTURES 0

FColor BoxBlurSample(TArray<FColor>& InBMP, int32 X, int32 Y, int32 InImageWidth, int32 InImageHeight, bool bIsNormalMap)
{
	const int32 SampleCount = 8;
	static int32 PixelIndices[SampleCount] = { -(InImageWidth + 1), -InImageWidth, -(InImageWidth - 1),
									 -1, 1,
									 (InImageWidth + -1), InImageWidth, (InImageWidth + 1) };

	static int32 PixelOffsetX[SampleCount] = { -1, 0, 1,
									-1, 1,
									-1, 0, 1 };

	int32 PixelsSampled = 0;
	FLinearColor CombinedColor = FColor::Black;
	
	// Take samples for blur with square indices
	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const int32 PixelIndex = ((Y * InImageWidth) + X) + PixelIndices[SampleIndex];

		const int32 XIndex = X + PixelOffsetX[SampleIndex];

		// Check if we are not out of texture bounds
		if (InBMP.IsValidIndex(PixelIndex) && XIndex >= 0 && XIndex < InImageWidth)
		{
			FLinearColor SampledColor = InBMP[PixelIndex].ReinterpretAsLinear();
			// Check if the pixel is a rendered one (not clear colour)
			if ((!(SampledColor.R == 1.0f && SampledColor.B == 1.0f && SampledColor.G == 0.0f)) && (!bIsNormalMap || SampledColor.B != 0.0f))
			{
				CombinedColor += SampledColor;
				++PixelsSampled;
			}
		}
	}
	CombinedColor /= PixelsSampled;

	if (PixelsSampled == 0)
	{
		return InBMP[((Y * InImageWidth) + X)];
	}

	return CombinedColor.ToFColor(false);
}

void PerformUVBorderSmear(TArray<FColor>& InBMP, int32 InImageWidth, int32 InImageHeight, bool IsNormalMap)
{
	TArray<FColor> Swap;
	Swap.Append(InBMP);

	TArray<FColor>* Current = &InBMP;
	TArray<FColor>* Scratch = &Swap;

	bool bSwap = false;

	int32 MagentaPixels = 1;

	int32 LoopCount = 0;
	const int32 MaxIterations = 32;
	
	// Sampling
	while (MagentaPixels && (LoopCount <= MaxIterations))
	{
		MagentaPixels = 0;
		// Left / right, Top / down
		for (int32 Y = 0; Y < InImageHeight; Y++)
		{		
			for (int32 X = 0; X < InImageWidth; X++)
			{
				int32 PixelIndex = (Y * InImageWidth) + X;

				FColor& Color = (*Current)[PixelIndex];
				if ((Color.R == 255 && Color.B == 255 && Color.G == 0) || (IsNormalMap && Color.B == 0))
				{
					MagentaPixels++;
					FColor SampledColor = BoxBlurSample(*Scratch, X, Y, InImageWidth, InImageHeight, IsNormalMap);
					// If it's a valid pixel
					if ((!(SampledColor.R == 255 && SampledColor.B == 255 && SampledColor.G == 0)) && (!IsNormalMap || SampledColor.B != 0))
					{
						Color = SampledColor;
					}
					else
					{
						// If we are at the end of our iterations, replace the pixels with black
						if (LoopCount == (MaxIterations - 1))
						{
							Color = FColor::Black;
						}
					}
				}
			}
		}

		TArray<FColor>& Temp = *Scratch;
		Scratch = Current;
		Current = &Temp;

		LoopCount++;
	}

	if (Current != &InBMP)
	{
		InBMP.Empty();
		InBMP.Append(*Current);
	}
}

/**
* Vertex data for a screen quad.
*/
struct FMaterialMeshVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
		TangentZ;
	uint32			Color;
	FVector2D		TextureCoordinate[MAX_STATIC_TEXCOORDS];
	FVector2D		LightMapCoordinate;

	void SetTangents(const FVector& InTangentX, const FVector& InTangentY, const FVector& InTangentZ)
	{
		TangentX = InTangentX;
		TangentZ = InTangentZ;
		// store determinant of basis in w component of normal vector
		TangentZ.Vector.W = GetBasisDeterminantSign(InTangentX, InTangentY, InTangentZ) < 0.0f ? 0 : 255;
	}
};

/**
* A dummy vertex buffer used to give the FMeshVertexFactory something to reference as a stream source.
*/
class FMaterialMeshVertexBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FMaterialMeshVertex), BUF_Static, CreateInfo);
	}
};
TGlobalResource<FMaterialMeshVertexBuffer> GDummyMeshRendererVertexBuffer;

/**
* Vertex factory for rendering meshes with materials.
*/
class FMeshVertexFactory : public FLocalVertexFactory
{
public:

	/** Default constructor. */
	FMeshVertexFactory()
	{
		FLocalVertexFactory::FDataType VertexData;

		// position
		VertexData.PositionComponent = FVertexStreamComponent(
			&GDummyMeshRendererVertexBuffer,
			STRUCT_OFFSET(FMaterialMeshVertex, Position),
			sizeof(FMaterialMeshVertex),
			VET_Float3
			);
		// tangents
		VertexData.TangentBasisComponents[0] = FVertexStreamComponent(
			&GDummyMeshRendererVertexBuffer,
			STRUCT_OFFSET(FMaterialMeshVertex, TangentX),
			sizeof(FMaterialMeshVertex),
			VET_PackedNormal
			);
		VertexData.TangentBasisComponents[1] = FVertexStreamComponent(
			&GDummyMeshRendererVertexBuffer,
			STRUCT_OFFSET(FMaterialMeshVertex, TangentZ),
			sizeof(FMaterialMeshVertex),
			VET_PackedNormal
			);
		// color
		VertexData.ColorComponent = FVertexStreamComponent(
			&GDummyMeshRendererVertexBuffer,
			STRUCT_OFFSET(FMaterialMeshVertex, Color),
			sizeof(FMaterialMeshVertex),
			VET_Color
			);
		// UVs
		int32 UVIndex;
		for (UVIndex = 0; UVIndex < MAX_STATIC_TEXCOORDS - 1; UVIndex += 2)
		{
			VertexData.TextureCoordinates.Add(FVertexStreamComponent(
				&GDummyMeshRendererVertexBuffer,
				STRUCT_OFFSET(FMaterialMeshVertex, TextureCoordinate) + sizeof(FVector2D)* UVIndex,
				sizeof(FMaterialMeshVertex),
				VET_Float4
				));
		}
		// possible last UV channel if we have an odd number (by the way, MAX_STATIC_TEXCOORDS is even value, so most
		// likely the following code will never be executed)
		if (UVIndex < MAX_STATIC_TEXCOORDS)
		{
			VertexData.TextureCoordinates.Add(FVertexStreamComponent(
				&GDummyMeshRendererVertexBuffer,
				STRUCT_OFFSET(FMaterialMeshVertex, TextureCoordinate) + sizeof(FVector2D)* UVIndex,
				sizeof(FMaterialMeshVertex),
				VET_Float2
				));
		}

		VertexData.LightMapCoordinateComponent = FVertexStreamComponent(
			&GDummyMeshRendererVertexBuffer,
			STRUCT_OFFSET(FMaterialMeshVertex, LightMapCoordinate),
			sizeof(FMaterialMeshVertex),
			VET_Float2
		);


		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FMeshVertexFactoryConstructor,
			FMeshVertexFactory*, FactoryParam, this,
			FLocalVertexFactory::FDataType, DataParam, VertexData,
			{
				FactoryParam->SetData(DataParam);
			}
		);

		FlushRenderingCommands();
	}
};
TGlobalResource<FMeshVertexFactory> GMeshVertexFactory;


class FMeshRenderInfo : public FLightCacheInterface
{
public:
	FMeshRenderInfo(const FLightMap* InLightMap, const FShadowMap* InShadowMap, FUniformBufferRHIRef Buffer)
		: FLightCacheInterface(InLightMap, InShadowMap)
	{
		SetPrecomputedLightingBuffer(Buffer);
	}

	virtual FLightInteraction GetInteraction(const class FLightSceneProxy* LightSceneProxy) const override
	{
		return LIT_CachedLightMap;
	}
};

/**
* Canvas render item enqueued into renderer command list.
*/
class FMeshMaterialRenderItem : public FCanvasBaseRenderItem
{
public:
	FMeshMaterialRenderItem(FSceneViewFamily* InViewFamily, const FRawMesh* InMesh, const FStaticLODModel* InLODModel, int32 LightMapIndex, int32 InMaterialIndex, const FBox2D& InTexcoordBounds, const TArray<FVector2D>& InTexCoords, const FVector2D& InSize, const FMaterialRenderProxy* InMaterialRenderProxy, const FCanvas::FTransformEntry& InTransform /*= FCanvas::FTransformEntry(FMatrix::Identity)*/, FLightMapRef LightMap, FShadowMapRef ShadowMap, FUniformBufferRHIRef Buffer) : Data(new FRenderData(
		InViewFamily,
		InMesh,
		InLODModel,
		LightMapIndex,
		InMaterialIndex,
		InTexcoordBounds,
		InTexCoords,
		InSize,
		InMaterialRenderProxy,
		InTransform,
		new FMeshRenderInfo(LightMap, ShadowMap, Buffer)))
	{
	}

	~FMeshMaterialRenderItem()
	{
	}

private:
	class FRenderData
	{
	public:
		FRenderData(
			FSceneViewFamily* InViewFamily,
			const FRawMesh* InMesh,
			const FStaticLODModel* InLODModel,
			int32 InLightMapIndex,
			int32 InMaterialIndex,
			const FBox2D& InTexcoordBounds,
			const TArray<FVector2D>& InTexCoords,
			const FVector2D& InSize,
			const FMaterialRenderProxy* InMaterialRenderProxy = nullptr,
			const FCanvas::FTransformEntry& InTransform = FCanvas::FTransformEntry(FMatrix::Identity),
			FLightCacheInterface* InLCI = nullptr)
			: ViewFamily(InViewFamily)
			, StaticMesh(InMesh)
			, SkeletalMesh(InLODModel)
			, LightMapIndex(InLightMapIndex)
			, MaterialIndex(InMaterialIndex)
			, TexcoordBounds(InTexcoordBounds)
			, TexCoords(InTexCoords)
			, Size(InSize)
			, MaterialRenderProxy(InMaterialRenderProxy)
			, Transform(InTransform)
			, LCI(InLCI)

		{}
		FSceneViewFamily* ViewFamily;
		const FRawMesh* StaticMesh;
		const FStaticLODModel* SkeletalMesh;
		int32 LightMapIndex;
		int32 MaterialIndex;
		FBox2D TexcoordBounds;
		const TArray<FVector2D>& TexCoords;
		FVector2D Size;
		const FMaterialRenderProxy* MaterialRenderProxy;
		
		FCanvas::FTransformEntry Transform;
		FLightCacheInterface* LCI;
	};
	FRenderData* Data;
public:

	static void EnqueueMaterialRender(class FCanvas* InCanvas, FSceneViewFamily* InViewFamily, const FRawMesh* InMesh, const FStaticLODModel* InLODModel, int32 LightMapIndex, int32 InMaterialIndex, const FBox2D& InTexcoordBounds, const TArray<FVector2D>& InTexCoords, const FVector2D& InSize, const FMaterialRenderProxy* InMaterialRenderProxy, FLightMapRef LightMap, FShadowMapRef ShadowMap, FUniformBufferRHIRef Buffer)
	{
		// get sort element based on the current sort key from top of sort key stack
		FCanvas::FCanvasSortElement& SortElement = InCanvas->GetSortElement(InCanvas->TopDepthSortKey());
		// get the current transform entry from top of transform stack
		const FCanvas::FTransformEntry& TopTransformEntry = InCanvas->GetTransformStack().Top();
		// create a render batch
		FMeshMaterialRenderItem* RenderBatch = new FMeshMaterialRenderItem(
			InViewFamily,
			InMesh,
			InLODModel,
			LightMapIndex,
			InMaterialIndex,
			InTexcoordBounds,
			InTexCoords,
			InSize,
			InMaterialRenderProxy,
			TopTransformEntry,
			LightMap,
			ShadowMap, 
			Buffer);
		SortElement.RenderBatchArray.Add(RenderBatch);
	}

	static int32 FillStaticMeshData(bool bDuplicateTris, const FRawMesh& RawMesh, FRenderData& Data, TArray<FMaterialMeshVertex>& OutVerts, TArray<int32>& OutIndices)
	{
		// count triangles for selected material
		int32 NumTris = 0;
		int32 TotalNumFaces = RawMesh.FaceMaterialIndices.Num();
		for (int32 FaceIndex = 0; FaceIndex < TotalNumFaces; FaceIndex++)
		{
			if (RawMesh.FaceMaterialIndices[FaceIndex] == Data.MaterialIndex)
			{
				NumTris++;
			}
		}
		if (NumTris == 0)
		{
			// there's nothing to do here
			return 0;
		}

		// vertices are not shared between triangles in FRawMesh, so NumVerts is NumTris * 3
		int32 NumVerts = NumTris * 3;

		// reserve renderer data
		OutVerts.Empty(NumVerts);
		OutIndices.Empty(bDuplicateTris ? NumVerts * 2 : NumVerts);

		float U = Data.TexcoordBounds.Min.X;
		float V = Data.TexcoordBounds.Min.Y;
		float SizeU = Data.TexcoordBounds.Max.X - Data.TexcoordBounds.Min.X;
		float SizeV = Data.TexcoordBounds.Max.Y - Data.TexcoordBounds.Min.Y;
		float ScaleX = (SizeU != 0) ? Data.Size.X / SizeU : 1.0;
		float ScaleY = (SizeV != 0) ? Data.Size.Y / SizeV : 1.0;
		uint32 DefaultColor = FColor::White.DWColor();

		// count number of texture coordinates for this mesh
		int32 NumTexcoords = 1;
		for (NumTexcoords = 1; NumTexcoords < MAX_STATIC_TEXCOORDS; NumTexcoords++)
		{
			if (RawMesh.WedgeTexCoords[NumTexcoords].Num() == 0)
				break;
		}

		// check if we should use NewUVs or original UV set
		bool bUseNewUVs = Data.TexCoords.Num() > 0;
		if (bUseNewUVs)
		{
			check(Data.TexCoords.Num() == RawMesh.WedgeTexCoords[0].Num());
			ScaleX = Data.Size.X;
			ScaleY = Data.Size.Y;
		}

		// add vertices
		int32 VertIndex = 0;
		bool bHasVertexColor = (RawMesh.WedgeColors.Num() > 0);
		for (int32 FaceIndex = 0; FaceIndex < TotalNumFaces; FaceIndex++)
		{
			if (RawMesh.FaceMaterialIndices[FaceIndex] == Data.MaterialIndex)
			{
				for (int32 Corner = 0; Corner < 3; Corner++)
				{
					int32 SrcVertIndex = FaceIndex * 3 + Corner;
					// add vertex
					FMaterialMeshVertex* Vert = new(OutVerts)FMaterialMeshVertex();
					if (!bUseNewUVs)
					{
						// compute vertex position from original UV
						const FVector2D& UV = RawMesh.WedgeTexCoords[0][SrcVertIndex];
						Vert->Position.Set((UV.X - U) * ScaleX, (UV.Y - V) * ScaleY, 0);
					}
					else
					{
						const FVector2D& UV = Data.TexCoords[SrcVertIndex];
						Vert->Position.Set(UV.X * ScaleX, UV.Y * ScaleY, 0);
					}
					Vert->SetTangents(RawMesh.WedgeTangentX[SrcVertIndex], RawMesh.WedgeTangentY[SrcVertIndex], RawMesh.WedgeTangentZ[SrcVertIndex]);
					for (int32 TexcoordIndex = 0; TexcoordIndex < NumTexcoords; TexcoordIndex++)
						Vert->TextureCoordinate[TexcoordIndex] = RawMesh.WedgeTexCoords[TexcoordIndex][SrcVertIndex];

					Vert->TextureCoordinate[6].X = RawMesh.VertexPositions[RawMesh.WedgeIndices[SrcVertIndex]].X;
					Vert->TextureCoordinate[6].Y = RawMesh.VertexPositions[RawMesh.WedgeIndices[SrcVertIndex]].Y;
					Vert->TextureCoordinate[7].X = RawMesh.VertexPositions[RawMesh.WedgeIndices[SrcVertIndex]].Z;
					
					Vert->LightMapCoordinate = RawMesh.WedgeTexCoords[Data.LightMapIndex][SrcVertIndex];

					Vert->Color = bHasVertexColor ? RawMesh.WedgeColors[SrcVertIndex].DWColor() : DefaultColor;
					// add index
					OutIndices.Add(VertIndex);
					VertIndex++;
				}
				if (bDuplicateTris)
				{
					// add the same triangle with opposite vertex order
					OutIndices.Add(VertIndex - 3);
					OutIndices.Add(VertIndex - 1);
					OutIndices.Add(VertIndex - 2);
				}
			}
		}

		return NumTris;
	}

	static int32 FillSkeletalMeshData(bool bDuplicateTris, const FStaticLODModel& LODModel, FRenderData& Data, TArray<FMaterialMeshVertex>& OutVerts, TArray<int32>& OutIndices)
	{
		TArray<FSoftSkinVertex> Vertices;
		FMultiSizeIndexContainerData IndexData;
		LODModel.GetVertices(Vertices);
		LODModel.MultiSizeIndexContainer.GetIndexBufferData(IndexData);

		int32 NumTris = 0;
		int32 NumVerts = 0;

		const int32 SectionCount = LODModel.NumNonClothingSections();

		// count triangles and vertices for selected material
		for (int32 SectionIndex = 0; SectionIndex < SectionCount; SectionIndex++)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
			if (Section.MaterialIndex == Data.MaterialIndex)
			{
				NumTris += Section.NumTriangles;
				NumVerts += Section.GetNumVertices();
			}
		}

		if (NumTris == 0)
		{
			// there's nothing to do here
			return 0;
		}

		bool bUseNewUVs = Data.TexCoords.Num() > 0;

		if (bUseNewUVs)
		{
			// we should split all merged vertices because UVs are prepared per-corner, i.e. has
			// (NumTris * 3) vertices
			NumVerts = NumTris * 3;
		}
		
		// reserve renderer data
		OutVerts.Empty(NumVerts);
		OutIndices.Empty(bDuplicateTris ? NumVerts * 2 : NumVerts);

		float U = Data.TexcoordBounds.Min.X;
		float V = Data.TexcoordBounds.Min.Y;
		float SizeU = Data.TexcoordBounds.Max.X - Data.TexcoordBounds.Min.X;
		float SizeV = Data.TexcoordBounds.Max.Y - Data.TexcoordBounds.Min.Y;
		float ScaleX = (SizeU != 0) ? Data.Size.X / SizeU : 1.0;
		float ScaleY = (SizeV != 0) ? Data.Size.Y / SizeV : 1.0;
		uint32 DefaultColor = FColor::White.DWColor();

		int32 NumTexcoords = LODModel.NumTexCoords;

		// check if we should use NewUVs or original UV set
		if (bUseNewUVs)
		{
			ScaleX = Data.Size.X;
			ScaleY = Data.Size.Y;
		}

		// add vertices
		if (!bUseNewUVs)
		{
			// Use original UV from mesh, render indexed mesh as indexed mesh.

			uint32 FirstVertex = 0;
			uint32 OutVertexIndex = 0;

			for (int32 SectionIndex = 0; SectionIndex < SectionCount; SectionIndex++)
			{
				const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

				const int32 NumVertsInSection = Section.GetNumVertices();

				if (Section.MaterialIndex == Data.MaterialIndex)
				{
					// offset to remap source mesh vertex index to destination vertex index
					int32 IndexOffset = FirstVertex - OutVertexIndex;

					// copy vertices
					int32 SrcVertIndex = FirstVertex;
					for (int32 VertIndex = 0; VertIndex < NumVertsInSection; VertIndex++)
					{
						const FSoftSkinVertex& SrcVert = Vertices[SrcVertIndex];
						FMaterialMeshVertex* DstVert = new(OutVerts)FMaterialMeshVertex();

						// compute vertex position from original UV
						const FVector2D& UV = SrcVert.UVs[0];
						DstVert->Position.Set((UV.X - U) * ScaleX, (UV.Y - V) * ScaleY, 0);

						DstVert->SetTangents(SrcVert.TangentX, SrcVert.TangentY, SrcVert.TangentZ);
						for (int32 TexcoordIndex = 0; TexcoordIndex < NumTexcoords; TexcoordIndex++)
							DstVert->TextureCoordinate[TexcoordIndex] = SrcVert.UVs[TexcoordIndex];
						DstVert->Color = SrcVert.Color.DWColor();

						SrcVertIndex++;
						OutVertexIndex++;
					}

					// copy indices
					int32 Index = Section.BaseIndex;
					for (uint32 TriIndex = 0; TriIndex < Section.NumTriangles; TriIndex++)
					{
						uint32 Index0 = IndexData.Indices[Index++] - IndexOffset;
						uint32 Index1 = IndexData.Indices[Index++] - IndexOffset;
						uint32 Index2 = IndexData.Indices[Index++] - IndexOffset;
						OutIndices.Add(Index0);
						OutIndices.Add(Index1);
						OutIndices.Add(Index2);
						if (bDuplicateTris)
						{
							// add the same triangle with opposite vertex order
							OutIndices.Add(Index0);
							OutIndices.Add(Index2);
							OutIndices.Add(Index1);
						}
					}
				}
				FirstVertex += NumVertsInSection;
			}
		}
		else // bUseNewUVs
		{
			// Use external UVs. These UVs are prepared per-corner, so we should convert indexed mesh to non-indexed, without
			// sharing of vertices between triangles.

			uint32 OutVertexIndex = 0;

			for (int32 SectionIndex = 0; SectionIndex < SectionCount; SectionIndex++)
			{
				const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

				if (Section.MaterialIndex == Data.MaterialIndex)
				{
					// copy vertices
					int32 LastIndex = Section.BaseIndex + Section.NumTriangles * 3;
					for (int32 Index = Section.BaseIndex; Index < LastIndex; Index += 3)
					{
						for (int32 Corner = 0; Corner < 3; Corner++)
						{
							int32 CornerIndex = Index + Corner;
							int32 SrcVertIndex = IndexData.Indices[CornerIndex];
							const FSoftSkinVertex& SrcVert = Vertices[SrcVertIndex];
							FMaterialMeshVertex* DstVert = new(OutVerts)FMaterialMeshVertex();

							const FVector2D& UV = Data.TexCoords[CornerIndex];
							DstVert->Position.Set(UV.X * ScaleX, UV.Y * ScaleY, 0);

							DstVert->SetTangents(SrcVert.TangentX, SrcVert.TangentY, SrcVert.TangentZ);
							for (int32 TexcoordIndex = 0; TexcoordIndex < NumTexcoords; TexcoordIndex++)
								DstVert->TextureCoordinate[TexcoordIndex] = SrcVert.UVs[TexcoordIndex];
							DstVert->Color = SrcVert.Color.DWColor();

							OutIndices.Add(OutVertexIndex);
							OutVertexIndex++;
						}
						if (bDuplicateTris)
						{
							// add the same triangle with opposite vertex order
							OutIndices.Add(OutVertexIndex - 3);
							OutIndices.Add(OutVertexIndex - 1);
							OutIndices.Add(OutVertexIndex - 2);
						}
					}
				}
			}
		}

		return NumTris;
	}

	static int32 FillQuadData(FRenderData& Data, TArray<FMaterialMeshVertex>& OutVerts, TArray<int32>& OutIndices)
	{
		OutVerts.Empty(4);
		OutIndices.Empty(6);

		float U = Data.TexcoordBounds.Min.X;
		float V = Data.TexcoordBounds.Min.Y;
		float SizeU = Data.TexcoordBounds.Max.X - Data.TexcoordBounds.Min.X;
		float SizeV = Data.TexcoordBounds.Max.Y - Data.TexcoordBounds.Min.Y;
		float ScaleX = (SizeU != 0) ? Data.Size.X / SizeU : 1.0;
		float ScaleY = (SizeV != 0) ? Data.Size.Y / SizeV : 1.0;
		uint32 DefaultColor = FColor::White.DWColor();

		// add vertices
		for (int32 VertIndex = 0; VertIndex < 4; VertIndex++)
		{
			FMaterialMeshVertex* Vert = new(OutVerts)FMaterialMeshVertex();

			int X = VertIndex & 1;
			int Y = (VertIndex >> 1) & 1;

			Vert->Position.Set(ScaleX * X, ScaleY * Y, 0);
			Vert->SetTangents(FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1));
			FMemory::Memzero(&Vert->TextureCoordinate, sizeof(Vert->TextureCoordinate));
			Vert->TextureCoordinate[0].Set(U + SizeU * X, V + SizeV * Y);
			Vert->Color = DefaultColor;
		}

		// add indices
		static const int32 Indices[6] = { 0, 2, 1, 2, 3, 1 };
		OutIndices.Append(Indices, 6);

		return 2;
	}

	static void RenderMaterial(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, FRenderData& Data)
	{
		FMeshBatch MeshElement;
		MeshElement.VertexFactory = &GMeshVertexFactory;
		MeshElement.DynamicVertexStride = sizeof(FMaterialMeshVertex);
		MeshElement.ReverseCulling = false;
		MeshElement.UseDynamicData = true;
		MeshElement.Type = PT_TriangleList;
		MeshElement.DepthPriorityGroup = SDPG_Foreground;		
		Data.LCI->SetPrecomputedLightingBuffer(LightMapHelpers::CreateDummyPrecomputedLightingUniformBuffer(UniformBuffer_SingleFrame, GMaxRHIFeatureLevel, Data.LCI));
		MeshElement.LCI = Data.LCI;
		FMeshBatchElement& BatchElement = MeshElement.Elements[0];
		BatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
	#if SHOW_WIREFRAME_MESH
		MeshElement.bWireframe = true;
	#endif

		// Check if material is TwoSided - single-sided materials should be rendered with normal and reverse
		// triangle corner orders, to avoid problems with inside-out meshes or mesh parts. Note:
		// FExportMaterialProxy::GetMaterial() (which is really called here) ignores 'InFeatureLevel' parameter.
		const FMaterial* Material = Data.MaterialRenderProxy->GetMaterial(GMaxRHIFeatureLevel);
		bool bIsMaterialTwoSided = Material->IsTwoSided();

		TArray<FMaterialMeshVertex> Verts;
		TArray<int32> Indices;

		int32 NumTris = 0;
		if (Data.StaticMesh != nullptr)
		{
			check(Data.SkeletalMesh == nullptr)
				NumTris = FillStaticMeshData(!bIsMaterialTwoSided, *Data.StaticMesh, Data, Verts, Indices);
		}
		else if (Data.SkeletalMesh != nullptr)
		{
			NumTris = FillSkeletalMeshData(!bIsMaterialTwoSided, *Data.SkeletalMesh, Data, Verts, Indices);
		}
		else
		{
			// both are null, use simple rectangle
			NumTris = FillQuadData(Data, Verts, Indices);
		}
		if (NumTris == 0)
		{
			// there's nothing to do here
			return;
		}

		MeshElement.UseDynamicData = true;
		MeshElement.DynamicVertexData = Verts.GetData();
		MeshElement.MaterialRenderProxy = Data.MaterialRenderProxy;

		// an attempt to use index data
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = bIsMaterialTwoSided ? NumTris : NumTris * 2;
		BatchElement.DynamicIndexData = Indices.GetData();
		BatchElement.DynamicIndexStride = sizeof(int32);
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = Verts.Num() - 1;

		GetRendererModule().DrawTileMesh(RHICmdList, DrawRenderState, View, MeshElement, false /*bIsHitTesting*/, FHitProxyId());
	}

	virtual bool Render_RenderThread(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FCanvas* Canvas)
	{
		checkSlow(Data);
		// current render target set for the canvas
		const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
		FIntRect ViewRect(FIntPoint(0, 0), CanvasRenderTarget->GetSizeXY());

		// make a temporary view
		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = Data->ViewFamily;
		ViewInitOptions.SetViewRectangle(ViewRect);
		ViewInitOptions.ViewOrigin = FVector::ZeroVector;
		ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
		ViewInitOptions.ProjectionMatrix = Data->Transform.GetMatrix();
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverlayColor = FLinearColor::White;

		bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && !Canvas->GetAllowSwitchVerticalAxis();
		check(bNeedsToSwitchVerticalAxis == false);

		FSceneView* View = new FSceneView(ViewInitOptions);

		RenderMaterial(RHICmdList, DrawRenderState, *View, *Data);

		delete View;
		if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
		{
			delete Data;
		}
		if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
		{
			Data = NULL;
		}
		return true;
	}

	virtual bool Render_GameThread(const FCanvas* Canvas)
	{
		checkSlow(Data);
		// current render target set for the canvas
		const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
		FIntRect ViewRect(FIntPoint(0, 0), CanvasRenderTarget->GetSizeXY());

		// make a temporary view
		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = Data->ViewFamily;
		ViewInitOptions.SetViewRectangle(ViewRect);
		ViewInitOptions.ViewOrigin = FVector::ZeroVector;
		ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
		ViewInitOptions.ProjectionMatrix = Data->Transform.GetMatrix();
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverlayColor = FLinearColor::White;

		FSceneView* View = new FSceneView(ViewInitOptions);

		bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && !Canvas->GetAllowSwitchVerticalAxis();
		check(bNeedsToSwitchVerticalAxis == false);
		struct FDrawMaterialParameters
		{
			FSceneView* View;
			FRenderData* RenderData;
			uint32 AllowedCanvasModes;
		};
		FDrawMaterialParameters DrawMaterialParameters =
		{
			View,
			Data,
			Canvas->GetAllowedModes()
		};

		FDrawMaterialParameters Parameters = DrawMaterialParameters;
		ENQUEUE_RENDER_COMMAND(DrawMaterialCommand)(
			[Parameters](FRHICommandListImmediate& RHICmdList)
			{
				FDrawingPolicyRenderState DrawRenderState(*Parameters.View);

				// disable depth test & writes
				DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

				RenderMaterial(RHICmdList, DrawRenderState, *Parameters.View, *Parameters.RenderData);

				delete Parameters.View;
				if (Parameters.AllowedCanvasModes & FCanvas::Allow_DeleteOnRender)
				{
					delete Parameters.RenderData;
				}
			});
		if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
		{
			Data = NULL;
		}
		return true;
	}
};

bool FMeshRenderer::RenderMaterial(struct FMaterialMergeData& InMaterialData, FMaterialRenderProxy* InMaterialProxy, EMaterialProperty InMaterialProperty, UTextureRenderTarget2D* InRenderTarget, TArray<FColor>& OutBMP)
{
	check(IsInGameThread());
	check(InRenderTarget);
	FTextureRenderTargetResource* RTResource = InRenderTarget->GameThread_GetRenderTargetResource();

	{
		// Create a canvas for the render target and clear it to black
		FCanvas Canvas(RTResource, NULL, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime, GMaxRHIFeatureLevel);

#if 0	// original FFlattenMaterial code - kept here for comparison

#if !SHOW_WIREFRAME_MESH
		Canvas.Clear(InRenderTarget->ClearColor);
#else
		Canvas.Clear(FLinearColor::Yellow);
#endif

		FVector2D UV0(InMaterialData.TexcoordBounds.Min.X, InMaterialData.TexcoordBounds.Min.Y);
		FVector2D UV1(InMaterialData.TexcoordBounds.Max.X, InMaterialData.TexcoordBounds.Max.Y);
		FCanvasTileItem TileItem(FVector2D(0.0f, 0.0f), InMaterialProxy, FVector2D(InRenderTarget->SizeX, InRenderTarget->SizeY), UV0, UV1);
		TileItem.bFreezeTime = true;
		Canvas.DrawItem(TileItem);

		Canvas.Flush_GameThread();
#else

		// create ViewFamily
		float CurrentRealTime = 0.f;
		float CurrentWorldTime = 0.f;
		float DeltaWorldTime = 0.f;

		const FRenderTarget* CanvasRenderTarget = Canvas.GetRenderTarget();
		FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(
			CanvasRenderTarget,
			NULL,
			FEngineShowFlags(ESFIM_Game))
			.SetWorldTimes(CurrentWorldTime, DeltaWorldTime, CurrentRealTime)
			.SetGammaCorrection(CanvasRenderTarget->GetDisplayGamma()));
		
#if !SHOW_WIREFRAME_MESH
		Canvas.Clear(InRenderTarget->ClearColor);
#else
		Canvas.Clear(FLinearColor::Yellow);
#endif

		// add item for rendering
		FMeshMaterialRenderItem::EnqueueMaterialRender(
			&Canvas,
			&ViewFamily,
			InMaterialData.Mesh,
			InMaterialData.LODModel,
			InMaterialData.LightMapIndex,
			InMaterialData.MaterialIndex,
			InMaterialData.TexcoordBounds,
			InMaterialData.TexCoords,
			FVector2D(InRenderTarget->SizeX, InRenderTarget->SizeY),
			InMaterialProxy,
			InMaterialData.LightMap,
			InMaterialData.ShadowMap,
			InMaterialData.Buffer
			);

		// rendering is performed here
		Canvas.Flush_GameThread();
#endif

		FlushRenderingCommands();
		Canvas.SetRenderTarget_GameThread(NULL);
		FlushRenderingCommands();
	}

	bool bNormalmap = (InMaterialProperty == MP_Normal);
	FReadSurfaceDataFlags ReadPixelFlags(bNormalmap ? RCM_SNorm : RCM_UNorm);
	ReadPixelFlags.SetLinearToGamma(false);

	bool result = false;

	if (InMaterialProperty != MP_EmissiveColor)
	{
		// Read normal color image
		result = RTResource->ReadPixels(OutBMP, ReadPixelFlags);
	}
	else
	{
		// Read HDR emissive image
		TArray<FFloat16Color> Color16;
		result = RTResource->ReadFloat16Pixels(Color16);
		// Find color scale value
		float MaxValue = 0;
		for (int32 PixelIndex = 0; PixelIndex < Color16.Num(); PixelIndex++)
		{
			FFloat16Color& Pixel16 = Color16[PixelIndex];
			float R = Pixel16.R.GetFloat();
			float G = Pixel16.G.GetFloat();
			float B = Pixel16.B.GetFloat();
			float Max = FMath::Max3(R, G, B);
			if (Max > MaxValue)
			{
				MaxValue = Max;
			}
		}
		if (MaxValue <= 0.01f)
		{
			// Black emissive, drop it
			return false;
		}
		// Now convert Float16 to Color
		OutBMP.SetNumUninitialized(Color16.Num());
		float Scale = 255.0f / MaxValue;
		for (int32 PixelIndex = 0; PixelIndex < Color16.Num(); PixelIndex++)
		{
			FFloat16Color& Pixel16 = Color16[PixelIndex];
			FColor& Pixel8 = OutBMP[PixelIndex];
			Pixel8.R = (uint8)FMath::RoundToInt(Pixel16.R.GetFloat() * Scale);
			Pixel8.G = (uint8)FMath::RoundToInt(Pixel16.G.GetFloat() * Scale);
			Pixel8.B = (uint8)FMath::RoundToInt(Pixel16.B.GetFloat() * Scale);
		}
		
	}

	PerformUVBorderSmear(OutBMP, InRenderTarget->GetSurfaceWidth(), InRenderTarget->GetSurfaceHeight(), bNormalmap);
#ifdef SAVE_INTERMEDIATE_TEXTURES
	FString FilenameString = FString::Printf(
		TEXT( "D:/TextureTest/%s-mat%d-prop%d.bmp"),
		*InMaterialProxy->GetFriendlyName(), InMaterialData.MaterialIndex, (int32)InMaterialProperty);
	FFileHelper::CreateBitmap(*FilenameString, InRenderTarget->GetSurfaceWidth(), InRenderTarget->GetSurfaceHeight(), OutBMP.GetData());
#endif // SAVE_INTERMEDIATE_TEXTURES
	return result;
}

bool FMeshRenderer::RenderMaterialTexCoordScales(struct FMaterialMergeData& InMaterialData, FMaterialRenderProxy* InMaterialProxy, UTextureRenderTarget2D* InRenderTarget, TArray<FFloat16Color>& OutScales)
{
	check(IsInGameThread());
	check(InRenderTarget);
	// create ViewFamily
	float CurrentRealTime = 0.f;
	float CurrentWorldTime = 0.f;
	float DeltaWorldTime = 0.f;

	// Create a canvas for the render target and clear it to black
	FTextureRenderTargetResource* RTResource = InRenderTarget->GameThread_GetRenderTargetResource();
	FCanvas Canvas(RTResource, NULL, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime, GMaxRHIFeatureLevel);
	const FRenderTarget* CanvasRenderTarget = Canvas.GetRenderTarget();
	Canvas.Clear(FLinearColor::Black);

	// Set show flag view mode to output tex coord scale
	FEngineShowFlags ShowFlags(ESFIM_Game);
	ApplyViewMode(VMI_MaterialTextureScaleAccuracy, false, ShowFlags);
	ShowFlags.OutputMaterialTextureScales = true; // This will bind the DVSM_OutputMaterialTextureScales

	FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(CanvasRenderTarget, nullptr, ShowFlags)
		.SetWorldTimes(CurrentWorldTime, DeltaWorldTime, CurrentRealTime)
		.SetGammaCorrection(CanvasRenderTarget->GetDisplayGamma()));

	// The next line ensures a constant view vector of (0,0,1) for all pixels. Required because here SVPositionToTranslatedWorld is identity, making excessive view angle increase per pixel.
	// That creates bad side effects for anything that depends on the view vector, like parallax or bump offset mappings. For those, we want the tangent
	// space view vector to be perpendicular to the surface in order to generate the same results as if the feature was turned off. Which gives the good results
	// since any sub height sampling would in pratice requires less and less texture resolution, where as we are only concerned about the highest resolution the material needs.
	// This can be seen in the debug view mode, by a checkboard of white and cyan (up to green) values. The white value meaning the highest resolution taken is the good one
	// (blue meaning the texture has more resolution than required). Checkboard are only possible when a texture is sampled several times, like in parallax.
	//
	// Additionnal to affecting the view vector, it also forces a constant world position value, zeroing any textcoord scales that depends on the world position (as the UV don't change).
	// This is alright thought since the uniform quad can obviously not compute a valid mapping for world space texture mapping (only rendering the mesh at its world position could fix that).
	// The zero scale will be caught as an error, and the computed scale will fallback to 1.f
	ViewFamily.bNullifyWorldSpacePosition = true;

	// add item for rendering
	FMeshMaterialRenderItem::EnqueueMaterialRender(
		&Canvas,
		&ViewFamily,
		InMaterialData.Mesh,
		InMaterialData.LODModel,
		InMaterialData.LightMapIndex,
		InMaterialData.MaterialIndex,
		InMaterialData.TexcoordBounds,
		InMaterialData.TexCoords,
		FVector2D(InRenderTarget->SizeX, InRenderTarget->SizeY),
		InMaterialProxy,
		InMaterialData.LightMap,
		InMaterialData.ShadowMap,
		InMaterialData.Buffer
		);

	// rendering is performed here
	Canvas.Flush_GameThread();

	FlushRenderingCommands();
	Canvas.SetRenderTarget_GameThread(NULL);
	FlushRenderingCommands();

	return RTResource->ReadFloat16Pixels(OutScales);
}