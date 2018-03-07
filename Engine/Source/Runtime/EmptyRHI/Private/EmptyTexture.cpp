// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyVertexBuffer.cpp: Empty texture RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"

FORCEINLINE int32 GetEmptyCubeFace(ECubeFace Face)
{
	switch (Face)
	{
		case CubeFace_NegX:	return 0;
		case CubeFace_NegY:	return 1;
		case CubeFace_NegZ:	return 2;
		case CubeFace_PosX:	return 3;
		case CubeFace_PosY:	return 4;
		case CubeFace_PosZ:	return 5;
		default:			return -1;
	}
}

/** Given a pointer to a RHI texture that was created by the Empty RHI, returns a pointer to the FEmptyTextureBase it encapsulates. */
FEmptySurface& GetEmptySurfaceFromRHITexture(FRHITexture* Texture)
{
	if(Texture->GetTexture2D())
	{
		return ((FEmptyTexture2D*)Texture)->Surface;
	}
	else if(Texture->GetTexture2DArray())
	{
		return ((FEmptyTexture2DArray*)Texture)->Surface;
	}
	else if(Texture->GetTexture3D())
	{
		return ((FEmptyTexture3D*)Texture)->Surface;
	}
	else if(Texture->GetTextureCube())
	{
		return ((FEmptyTextureCube*)Texture)->Surface;
	}
	else
	{
		UE_LOG(LogEmpty, Fatal, TEXT("Unknown RHI texture type"));
		return ((FEmptyTexture2D*)Texture)->Surface;
	}
}

FEmptySurface::FEmptySurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData)
{

	// upload existing bulkdata
	if (BulkData)
	{
		// copy over bulk data
		// Lock
		// Memcpy
		// Unlock

		// bulk data can be unloaded now
		BulkData->Discard();
	}
}


FEmptySurface::~FEmptySurface()
{

}


void* FEmptySurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	return NULL;
}

void FEmptySurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{

}

uint32 FEmptySurface::GetMemorySize()
{
	return 0;
}

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

void FEmptyDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{

}

bool FEmptyDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	return false;
}

uint32 FEmptyDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	return GetEmptySurfaceFromRHITexture(TextureRHI).GetMemorySize();
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTexture2DRHIRef FEmptyDynamicRHI::RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FEmptyTexture2D((EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, CreateInfo.BulkData);
}

FTexture2DRHIRef FEmptyDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	UE_LOG(LogEmpty, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	return FTexture2DRHIRef();
}

void FEmptyDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{

}

FTexture2DArrayRHIRef FEmptyDynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FEmptyTexture2DArray((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData);
}

FTexture3DRHIRef FEmptyDynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FEmptyTexture3D((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData);
}

void FEmptyDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
}

void FEmptyDynamicRHI::RHIGenerateMips(FTextureRHIParamRef SourceSurfaceRHI)
{

}

FTexture2DRHIRef FEmptyDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	FEmptyTexture2D* OldTexture = ResourceCast(OldTextureRHI);

	return NULL;
}

ETextureReallocationStatus FEmptyDynamicRHI::RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Failed;
}

ETextureReallocationStatus FEmptyDynamicRHI::RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Failed;
}


void* FEmptyDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FEmptyTexture2D* Texture = ResourceCast(TextureRHI);
	return Texture->Surface.Lock(MipIndex, 0, LockMode, DestStride);
}

void FEmptyDynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	FEmptyTexture2D* Texture = ResourceCast(TextureRHI);
	Texture->Surface.Unlock(MipIndex, 0);
}

void* FEmptyDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FEmptyTexture2DArray* Texture = ResourceCast(TextureRHI);
	return Texture->Surface.Lock(MipIndex, TextureIndex, LockMode, DestStride);
}

void FEmptyDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FEmptyTexture2DArray* Texture = ResourceCast(TextureRHI);
	Texture->Surface.Unlock(MipIndex, TextureIndex);
}

void FEmptyDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{	
	FEmptyTexture2D* Texture = ResourceCast(TextureRHI);

}

void FEmptyDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{	
	FEmptyTexture3D* Texture = ResourceCast(TextureRHI);

}


/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FEmptyDynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FEmptyTextureCube((EPixelFormat)Format, Size, false, 1, NumMips, Flags, CreateInfo.BulkData);
}

FTextureCubeRHIRef FEmptyDynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FEmptyTextureCube((EPixelFormat)Format, Size, true, ArraySize, NumMips, Flags, CreateInfo.BulkData);
}

void* FEmptyDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FEmptyTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 EmptyFace = GetEmptyCubeFace((ECubeFace)FaceIndex);
	return TextureCube->Surface.Lock(MipIndex, FaceIndex + 6 * ArrayIndex, LockMode, DestStride);
}

void FEmptyDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FEmptyTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 EmptyFace = GetEmptyCubeFace((ECubeFace)FaceIndex);
	TextureCube->Surface.Unlock(MipIndex, FaceIndex + ArrayIndex * 6);
}

void FEmptyDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
}

void FEmptyDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{

}

void FEmptyDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}

uint64 FEmptyDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	return 0;
}

uint64 FEmptyDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	return 0;
}

uint64 FEmptyDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	return 0;
}

FTextureReferenceRHIRef FEmptyDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return nullptr;
}

void FEmptyDynamicRHI::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture)
{
}
