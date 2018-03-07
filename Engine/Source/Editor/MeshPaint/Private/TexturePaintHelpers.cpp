// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TexturePaintHelpers.h"

#include "Editor.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "Components/SkeletalMeshComponent.h"
#include "Classes/Engine/SkeletalMesh.h"

#include "IMeshPaintGeometryAdapter.h"

#include "Engine/TextureRenderTarget2D.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"
#include "MeshPaintTypes.h"

void TexturePaintHelpers::CopyTextureToRenderTargetTexture(UTexture* SourceTexture, UTextureRenderTarget2D* RenderTargetTexture, ERHIFeatureLevel::Type FeatureLevel)
{
	check(SourceTexture != nullptr);
	check(RenderTargetTexture != nullptr);

	// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
	// dereference this pointer.  We're just passing it along to other functions that will use it on the render
	// thread.  The only thing we're allowed to do is check to see if it's nullptr or not.
	FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
	check(RenderTargetResource != nullptr);
	
	// Create a canvas for the render target and clear it to black
	FCanvas Canvas(RenderTargetResource, nullptr, 0, 0, 0, FeatureLevel);

	const uint32 Width = RenderTargetTexture->GetSurfaceWidth();
	const uint32 Height = RenderTargetTexture->GetSurfaceHeight();

	// @todo MeshPaint: Need full color/alpha writes enabled to get alpha
	// @todo MeshPaint: Texels need to line up perfectly to avoid bilinear artifacts
	// @todo MeshPaint: Potential gamma issues here
	// @todo MeshPaint: Probably using CLAMP address mode when reading from source (if texels line up, shouldn't matter though.)

	// @todo MeshPaint: Should use scratch texture built from original source art (when possible!)
	//		-> Current method will have compression artifacts!
	
	// Grab the texture resource.  We only support 2D textures and render target textures here.
	FTexture* TextureResource = nullptr;
	UTexture2D* Texture2D = Cast<UTexture2D>(SourceTexture);
	if (Texture2D != nullptr)
	{
		TextureResource = Texture2D->Resource;
	}
	else
	{
		UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>(SourceTexture);
		TextureResource = TextureRenderTarget2D->GameThread_GetRenderTargetResource();
	}
	check(TextureResource != nullptr);
	
	// Draw a quad to copy the texture over to the render target
	{
		const float MinU = 0.0f;
		const float MinV = 0.0f;
		const float MaxU = 1.0f;
		const float MaxV = 1.0f;
		const float MinX = 0.0f;
		const float MinY = 0.0f;
		const float MaxX = Width;
		const float MaxY = Height;

		FCanvasUVTri Tri1;
		FCanvasUVTri Tri2;
		Tri1.V0_Pos = FVector2D(MinX, MinY);
		Tri1.V0_UV = FVector2D(MinU, MinV);
		Tri1.V1_Pos = FVector2D(MaxX, MinY);
		Tri1.V1_UV = FVector2D(MaxU, MinV);
		Tri1.V2_Pos = FVector2D(MaxX, MaxY);
		Tri1.V2_UV = FVector2D(MaxU, MaxV);

		Tri2.V0_Pos = FVector2D(MaxX, MaxY);
		Tri2.V0_UV = FVector2D(MaxU, MaxV);
		Tri2.V1_Pos = FVector2D(MinX, MaxY);
		Tri2.V1_UV = FVector2D(MinU, MaxV);
		Tri2.V2_Pos = FVector2D(MinX, MinY);
		Tri2.V2_UV = FVector2D(MinU, MinV);
		Tri1.V0_Color = Tri1.V1_Color = Tri1.V2_Color = Tri2.V0_Color = Tri2.V1_Color = Tri2.V2_Color = FLinearColor::White;
		TArray< FCanvasUVTri > List;
		List.Add(Tri1);
		List.Add(Tri2);
		FCanvasTriangleItem TriItem(List, TextureResource);
		TriItem.BlendMode = SE_BLEND_Opaque;
		Canvas.DrawItem(TriItem);
	}

	// Tell the rendering thread to draw any remaining batched elements
	Canvas.Flush_GameThread(true);
	
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		UpdateMeshPaintRTCommand,
		FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICmdList.CopyToResolveTarget(
				RenderTargetResource->GetRenderTargetTexture(),		// Source texture
				RenderTargetResource->TextureRHI,					// Dest texture
				true,												// Do we need the source image content again?
				FResolveParams());									// Resolve parameters
		});		
}

bool TexturePaintHelpers::GenerateSeamMask(UMeshComponent* MeshComponent, int32 UVSet, UTextureRenderTarget2D* SeamRenderTexture, UTexture2D* Texture, UTextureRenderTarget2D* RenderTargetTexture)
{
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent);
	if (StaticMeshComponent == nullptr)
	{
		return false;
	}

	const int32 PaintingMeshLODIndex = 0;

	check(StaticMeshComponent != nullptr);
	check(StaticMeshComponent->GetStaticMesh() != nullptr);
	check(SeamRenderTexture != nullptr);
	check(StaticMeshComponent->GetStaticMesh()->RenderData->LODResources[PaintingMeshLODIndex].VertexBuffer.GetNumTexCoords() > (uint32)UVSet);

	bool RetVal = false;

	FStaticMeshLODResources& LODModel = StaticMeshComponent->GetStaticMesh()->RenderData->LODResources[PaintingMeshLODIndex];

	const uint32 Width = SeamRenderTexture->GetSurfaceWidth();
	const uint32 Height = SeamRenderTexture->GetSurfaceHeight();

	// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
	// dereference this pointer.  We're just passing it along to other functions that will use it on the render
	// thread.  The only thing we're allowed to do is check to see if it's nullptr or not.
	FTextureRenderTargetResource* RenderTargetResource = SeamRenderTexture->GameThread_GetRenderTargetResource();
	check(RenderTargetResource != nullptr);

	int32 NumElements = StaticMeshComponent->GetNumMaterials();
	UTexture2D* TargetTexture2D = Texture;

	// Store info that tells us if the element material uses our target texture so we don't have to do a usestexture() call for each tri.  We will
	// use this info to eliminate triangles that do not use our texture.
	TArray< bool > ElementUsesTargetTexture;
	ElementUsesTargetTexture.AddZeroed(NumElements);
	for (int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
	{
		ElementUsesTargetTexture[ElementIndex] = false;

		UMaterialInterface* ElementMat = StaticMeshComponent->GetMaterial(ElementIndex);
		if (ElementMat != nullptr)
		{
			ElementUsesTargetTexture[ElementIndex] |= DoesMaterialUseTexture(ElementMat, TargetTexture2D);

			if (ElementUsesTargetTexture[ElementIndex] == false && RenderTargetTexture != nullptr)
			{
				// If we didn't get a match on our selected texture, we'll check to see if the the material uses a
				//  render target texture override that we put on during painting.
				ElementUsesTargetTexture[ElementIndex] |= DoesMaterialUseTexture(ElementMat, RenderTargetTexture);
			}
		}
	}

	// Make sure we're dealing with triangle lists
	FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
	const uint32 NumIndexBufferIndices = Indices.Num();
	check(NumIndexBufferIndices % 3 == 0);
	const uint32 NumTriangles = NumIndexBufferIndices / 3;

	static TArray< int32 > InfluencedTriangles;
	InfluencedTriangles.Empty(NumTriangles);

	// For each triangle in the mesh
	for (uint32 TriIndex = 0; TriIndex < NumTriangles; ++TriIndex)
	{
		// At least one triangle vertex was influenced.
		bool bAddTri = false;

		// Check to see if the sub-element that this triangle belongs to actually uses our paint target texture in its material
		for (int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
		{
			//FStaticMeshElement& Element = LODModel.Elements[ ElementIndex ];
			FStaticMeshSection& Element = LODModel.Sections[ElementIndex];


			if ((TriIndex >= Element.FirstIndex / 3) &&
				(TriIndex < Element.FirstIndex / 3 + Element.NumTriangles))
			{

				// The triangle belongs to this element, now we need to check to see if the element material uses our target texture.
				if (TargetTexture2D != nullptr && ElementUsesTargetTexture[ElementIndex] == true)
				{
					bAddTri = true;
				}

				// Triangles can only be part of one element so we do not need to continue to other elements.
				break;
			}

		}

		if (bAddTri)
		{
			InfluencedTriangles.Add(TriIndex);
		}

	}

	{
		// Create a canvas for the render target and clear it to white
		FCanvas Canvas(RenderTargetResource, nullptr, 0, 0, 0, GEditor->GetEditorWorldContext().World()->FeatureLevel);
		Canvas.Clear(FLinearColor::White);

		TArray<FCanvasUVTri> TriList;
		FCanvasUVTri EachTri;
		EachTri.V0_Color = FLinearColor::Black;
		EachTri.V1_Color = FLinearColor::Black;
		EachTri.V2_Color = FLinearColor::Black;

		for (int32 CurIndex = 0; CurIndex < InfluencedTriangles.Num(); ++CurIndex)
		{
			const int32 TriIndex = InfluencedTriangles[CurIndex];

			// Grab the vertex indices and points for this triangle
			FVector2D TriUVs[3];
			FVector2D UVMin(99999.9f, 99999.9f);
			FVector2D UVMax(-99999.9f, -99999.9f);
			for (int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum)
			{
				const int32 VertexIndex = Indices[TriIndex * 3 + TriVertexNum];
				TriUVs[TriVertexNum] = LODModel.VertexBuffer.GetVertexUV(VertexIndex, UVSet);

				// Update bounds
				float U = TriUVs[TriVertexNum].X;
				float V = TriUVs[TriVertexNum].Y;

				if (U < UVMin.X)
				{
					UVMin.X = U;
				}
				if (U > UVMax.X)
				{
					UVMax.X = U;
				}
				if (V < UVMin.Y)
				{
					UVMin.Y = V;
				}
				if (V > UVMax.Y)
				{
					UVMax.Y = V;
				}

			}

			// If the triangle lies entirely outside of the 0.0-1.0 range, we'll transpose it back
			FVector2D UVOffset(0.0f, 0.0f);
			if (UVMax.X > 1.0f)
			{
				UVOffset.X = -FMath::FloorToInt(UVMin.X);
			}
			else if (UVMin.X < 0.0f)
			{
				UVOffset.X = 1.0f + FMath::FloorToInt(-UVMax.X);
			}

			if (UVMax.Y > 1.0f)
			{
				UVOffset.Y = -FMath::FloorToInt(UVMin.Y);
			}
			else if (UVMin.Y < 0.0f)
			{
				UVOffset.Y = 1.0f + FMath::FloorToInt(-UVMax.Y);
			}


			// Note that we "wrap" the texture coordinates here to handle the case where the user
			// is painting on a tiling texture, or with the UVs out of bounds.  Ideally all of the
			// UVs would be in the 0.0 - 1.0 range but sometimes content isn't setup that way.
			// @todo MeshPaint: Handle triangles that cross the 0.0-1.0 UV boundary?
			FVector2D TrianglePoints[3];
			for (int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum)
			{
				TriUVs[TriVertexNum].X += UVOffset.X;
				TriUVs[TriVertexNum].Y += UVOffset.Y;

				TrianglePoints[TriVertexNum].X = TriUVs[TriVertexNum].X * Width;
				TrianglePoints[TriVertexNum].Y = TriUVs[TriVertexNum].Y * Height;
			}

			EachTri.V0_Pos = TrianglePoints[0];
			EachTri.V0_UV = TriUVs[0];
			EachTri.V0_Color = FLinearColor::Black;
			EachTri.V1_Pos = TrianglePoints[1];
			EachTri.V1_UV = TriUVs[1];
			EachTri.V1_Color = FLinearColor::Black;
			EachTri.V2_Pos = TrianglePoints[2];
			EachTri.V2_UV = TriUVs[2];
			EachTri.V2_Color = FLinearColor::Black;
			TriList.Add(EachTri);
		}
		// Setup the tri render item with the list of tris
		FCanvasTriangleItem TriItem(TriList, RenderTargetResource);
		TriItem.BlendMode = SE_BLEND_Opaque;
		// And render it
		Canvas.DrawItem(TriItem);
		// Tell the rendering thread to draw any remaining batched elements
		Canvas.Flush_GameThread(true);
	}


	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand5,
			FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
			{
				// Copy (resolve) the rendered image from the frame buffer to its render target texture
				RHICmdList.CopyToResolveTarget(
					RenderTargetResource->GetRenderTargetTexture(),		// Source texture
					RenderTargetResource->TextureRHI,
					true,												// Do we need the source image content again?
					FResolveParams());									// Resolve parameters
			});

	}

	return RetVal;
}

UTexture2D* TexturePaintHelpers::CreateTempUncompressedTexture(UTexture2D* SourceTexture)
{
	check(SourceTexture->Source.IsValid());

	// Decompress PNG image
	TArray<uint8> RawData;
	SourceTexture->Source.GetMipData(RawData, 0);

	// We are using the source art so grab the original width/height
	const int32 Width = SourceTexture->Source.GetSizeX();
	const int32 Height = SourceTexture->Source.GetSizeY();
	const bool bUseSRGB = SourceTexture->SRGB;

	check(Width > 0 && Height > 0 && RawData.Num() > 0);

	// Allocate the new texture
	UTexture2D* NewTexture2D = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);

	// Fill in the base mip for the texture we created
	uint8* MipData = (uint8*)NewTexture2D->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	for (int32 y = 0; y < Height; y++)
	{
		uint8* DestPtr = &MipData[(Height - 1 - y) * Width * sizeof(FColor)];
		const FColor* SrcPtr = &((FColor*)(RawData.GetData()))[(Height - 1 - y) * Width];
		for (int32 x = 0; x < Width; x++)
		{
			*DestPtr++ = SrcPtr->B;
			*DestPtr++ = SrcPtr->G;
			*DestPtr++ = SrcPtr->R;
			*DestPtr++ = SrcPtr->A;
			SrcPtr++;
		}
	}
	NewTexture2D->PlatformData->Mips[0].BulkData.Unlock();

	// Set options
	NewTexture2D->SRGB = bUseSRGB;
	NewTexture2D->CompressionNone = true;
	NewTexture2D->MipGenSettings = TMGS_NoMipmaps;
	NewTexture2D->CompressionSettings = TC_Default;

	// Update the remote texture data
	NewTexture2D->UpdateResource();
	return NewTexture2D;
}

void TexturePaintHelpers::SetupInitialRenderTargetData(UTexture2D* InTextureSource, UTextureRenderTarget2D* InRenderTarget)
{
	check(InTextureSource != nullptr);
	check(InRenderTarget != nullptr);

	if (InTextureSource->Source.IsValid())
	{
		// Great, we have source data!  We'll use that as our image source.

		// Create a texture in memory from the source art
		{
			// @todo MeshPaint: This generates a lot of memory thrash -- try to cache this texture and reuse it?
			UTexture2D* TempSourceArtTexture = CreateTempUncompressedTexture(InTextureSource);
			check(TempSourceArtTexture != nullptr);

			// Copy the texture to the render target using the GPU
			CopyTextureToRenderTargetTexture(TempSourceArtTexture, InRenderTarget, GEditor->GetEditorWorldContext().World()->FeatureLevel);

			// NOTE: TempSourceArtTexture is no longer needed (will be GC'd)
		}
	}
	else
	{
		// Just copy (render) the texture in GPU memory to our render target.  Hopefully it's not
		// compressed already!
		check(InTextureSource->IsFullyStreamedIn());
		CopyTextureToRenderTargetTexture(InTextureSource, InRenderTarget, GEditor->GetEditorWorldContext().World()->FeatureLevel);
	}
}

void TexturePaintHelpers::FindMaterialIndicesUsingTexture(const UTexture* Texture, const UMeshComponent* MeshComponent, TArray<int32>& OutIndices)
{
	checkf(Texture && MeshComponent, TEXT("Invalid Texture of MeshComponent"));

	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		const UMaterialInterface* MaterialInterface = MeshComponent->GetMaterial(MaterialIndex);
		const bool bUsesTexture = DoesMaterialUseTexture(MaterialInterface, Texture);
		OutIndices.AddUnique(MaterialIndex);
	}
}

void TexturePaintHelpers::RetrieveMeshSectionsForTextures(const UMeshComponent* MeshComponent, int32 LODIndex, TArray<const UTexture*> Textures, TArray<FTexturePaintMeshSectionInfo>& OutSectionInfo)
{	
	// @todo MeshPaint: if LODs can use different materials/textures then this will cause us problems
	TArray<int32> MaterialIndices;
	for (const UTexture* Texture : Textures)
	{
		TexturePaintHelpers::FindMaterialIndicesUsingTexture(Texture, MeshComponent, MaterialIndices);
	}
	
	if (MaterialIndices.Num())
	{
		TexturePaintHelpers::RetrieveMeshSectionsForMaterialIndices(MeshComponent, LODIndex, MaterialIndices, OutSectionInfo);
	}
}

void TexturePaintHelpers::RetrieveMeshSectionsForMaterialIndices(const UMeshComponent* MeshComponent, int32 LODIndex, const TArray<int32>& MaterialIndices, TArray<FTexturePaintMeshSectionInfo>& OutSectionInfo)
{
	if (const UStaticMeshComponent* StaticMeshComponent = Cast<const UStaticMeshComponent>(MeshComponent))
	{
		const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

		if (StaticMesh)
		{
			//@TODO: Find a better way to move this generically to the adapter
			check(StaticMeshComponent->GetStaticMesh()->GetNumLODs() > (int32)LODIndex);
			const FStaticMeshLODResources& LODModel = StaticMeshComponent->GetStaticMesh()->RenderData->LODResources[LODIndex];
			const int32 NumSections = LODModel.Sections.Num();

			FTexturePaintMeshSectionInfo Info;
			for ( const FStaticMeshSection& Section : LODModel.Sections)
			{
				const bool bSectionUsesTexture = MaterialIndices.Contains(Section.MaterialIndex);
				if (bSectionUsesTexture)
				{
					Info.FirstIndex = (Section.FirstIndex / 3);
					Info.LastIndex = Info.FirstIndex + Section.NumTriangles;
					OutSectionInfo.Add(Info);
				}
			}
		}
	}
	else if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<const USkeletalMeshComponent>(MeshComponent))
	{
		const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
		if (SkeletalMesh)
		{
			const FSkeletalMeshResource* Resource = SkeletalMesh->GetImportedResource();
			checkf(Resource->LODModels.IsValidIndex(LODIndex), TEXT("Invalid index %i for LOD models in Skeletal Mesh"), LODIndex);
			const FStaticLODModel& LODModel = Resource->LODModels[LODIndex];
			FTexturePaintMeshSectionInfo Info;
			for (const FSkelMeshSection& Section : LODModel.Sections)
			{
				Info.FirstIndex = Section.BaseIndex;
				Info.LastIndex = (Section.BaseIndex / 3) + Section.NumTriangles;
				OutSectionInfo.Add(Info);
			}
		}
	}
}

bool TexturePaintHelpers::DoesMeshComponentUseTexture(UMeshComponent* MeshComponent, UTexture* Texture)
{
	TArray<UTexture*> UsedTextures;
	MeshComponent->GetUsedTextures(UsedTextures, EMaterialQualityLevel::High);
	return UsedTextures.Contains(Texture);
}

void TexturePaintHelpers::RetrieveTexturesForComponent(const UMeshComponent* Component, IMeshPaintGeometryAdapter* Adapter, TArray<FPaintableTexture>& OutTextures)
{
	// Get the materials used by the mesh
	TArray<UMaterialInterface*> UsedMaterials;
	Component->GetUsedMaterials(UsedMaterials);

	for (int32 MaterialIndex = 0; MaterialIndex < UsedMaterials.Num(); ++MaterialIndex)
	{
		int32 OutDefaultIndex = 0;
		Adapter->QueryPaintableTextures(MaterialIndex, OutDefaultIndex, OutTextures);
	}
}

