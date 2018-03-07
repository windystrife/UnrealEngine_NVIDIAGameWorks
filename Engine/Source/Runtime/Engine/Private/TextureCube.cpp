// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureCube.cpp: UTextureCube implementation.
=============================================================================*/

#include "Engine/TextureCube.h"
#include "RenderUtils.h"
#include "TextureResource.h"
#include "EngineUtils.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"

UTextureCube::UTextureCube(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SRGB = true;
}

void UTextureCube::Serialize(FArchive& Ar)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UTextureCube::Serialize"), STAT_TextureCube_Serialize, STATGROUP_LoadTime);

	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);
	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (bCooked || Ar.IsCooking())
	{
		SerializeCookedPlatformData(Ar);
	}

#if WITH_EDITOR
	if (Ar.IsLoading() && !Ar.IsTransacting() && !bCooked)
	{
		BeginCachePlatformData();
	}
#endif // #if WITH_EDITOR
}

void UTextureCube::PostLoad()
{
#if WITH_EDITOR
	FinishCachePlatformData();
#endif // #if WITH_EDITOR

	Super::PostLoad();
}

void UTextureCube::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
#if WITH_EDITOR
	int32 SizeX = Source.GetSizeX();
	int32 SizeY = Source.GetSizeY();
#else
	int32 SizeX = 0;
	int32 SizeY = 0;
#endif

	const FString Dimensions = FString::Printf(TEXT("%dx%d"), SizeX, SizeY);
	OutTags.Add( FAssetRegistryTag("Dimensions", Dimensions, FAssetRegistryTag::TT_Dimensional) );
	OutTags.Add( FAssetRegistryTag("Format", GPixelFormats[GetPixelFormat()].Name, FAssetRegistryTag::TT_Alphabetical) );

	Super::GetAssetRegistryTags(OutTags);
}

void UTextureCube::UpdateResource()
{
#if WITH_EDITOR
	// Recache platform data if the source has changed.
	CachePlatformData();
#endif // #if WITH_EDITOR

	// Route to super.
	Super::UpdateResource();
}

FString UTextureCube::GetDesc()
{
	return FString::Printf(TEXT("Cube: %dx%d [%s]"),
		GetSizeX(),
		GetSizeY(),
		GPixelFormats[GetPixelFormat()].Name
		);
}

uint32 UTextureCube::CalcTextureMemorySize( int32 MipCount ) const
{
	uint32 Size = 0;
	if (PlatformData)
	{
		int32 SizeX = GetSizeX();
		int32 SizeY = GetSizeY();
		int32 NumMips = GetNumMips();
		EPixelFormat Format = GetPixelFormat();

		ensureMsgf(SizeX == SizeY, TEXT("Cubemap faces expected to be square.  Actual sizes are: %i, %i"), SizeX, SizeY);

		// Figure out what the first mip to use is.
		int32 FirstMip	= FMath::Max( 0, NumMips - MipCount );		
		FIntPoint MipExtents = CalcMipMapExtent(SizeX, SizeY, Format, FirstMip);
		
		uint32 TextureAlign = 0;
		uint64 TextureSize = RHICalcTextureCubePlatformSize(MipExtents.X, Format, MipCount, 0, TextureAlign);
		Size = (uint32)TextureSize;
	}
	return Size;
}


uint32 UTextureCube::CalcTextureMemorySizeEnum( ETextureMipCount Enum ) const
{
	if ( Enum == TMC_ResidentMips || Enum == TMC_AllMipsBiased )
	{
		return CalcTextureMemorySize( GetNumMips() - GetCachedLODBias() );
	}
	else
	{
		return CalcTextureMemorySize( GetNumMips() );
	}
}

class FTextureCubeResource : public FTextureResource
{
	/** The FName of the LODGroup-specific stat	*/
	FName					LODGroupStatName;

public:
	/**
	 * Minimal initialization constructor.
	 * @param InOwner - The UTextureCube which this FTextureCubeResource represents.
	 */
	FTextureCubeResource(UTextureCube* InOwner)
	:	Owner(InOwner)
	,	TextureSize(0)
	{
		//Initialize the MipData array
		for ( int32 FaceIndex=0;FaceIndex<6; FaceIndex++)
		{
			for( int32 MipIndex=0; MipIndex<ARRAY_COUNT(MipData[FaceIndex]); MipIndex++ )
			{
				MipData[FaceIndex][MipIndex] = NULL;
			}
		}

		check(Owner->GetNumMips() > 0);

		TIndirectArray<FTexture2DMipMap>& Mips = InOwner->PlatformData->Mips;
		for( int32 MipIndex=0; MipIndex<Mips.Num(); MipIndex++ )
		{
			FTexture2DMipMap& Mip = Mips[MipIndex];
			if( Mip.BulkData.GetBulkDataSize() <= 0 )
			{
				UE_LOG(LogTexture, Error, TEXT("Corrupt texture [%s]! Missing bulk data for MipIndex=%d"),*InOwner->GetFullName(),MipIndex );
			}
			else			
			{
				TextureSize += Mip.BulkData.GetBulkDataSize();
				uint32 MipSize = Mip.BulkData.GetBulkDataSize() / 6;

				uint8* In = (uint8*)Mip.BulkData.Lock(LOCK_READ_ONLY);

				for(uint32 Face = 0; Face < 6; ++Face)
				{
					MipData[Face][MipIndex] = FMemory::Malloc(MipSize);
					FMemory::Memcpy(MipData[Face][MipIndex], In + MipSize * Face, MipSize);
				}

				Mip.BulkData.Unlock();
			}
		}
		STAT( LODGroupStatName = TextureGroupStatFNames[InOwner->LODGroup] );
	}

	/**
	 * Destructor, freeing MipData in the case of resource being destroyed without ever 
	 * having been initialized by the rendering thread via InitRHI.
	 */	
	~FTextureCubeResource()
	{
		// Make sure we're not leaking memory if InitRHI has never been called.
		for (int32 i=0; i<6; i++)
		{
			for( int32 MipIndex=0; MipIndex<ARRAY_COUNT(MipData[i]); MipIndex++ )
			{
				// free any mip data that was copied 
				if( MipData[i][MipIndex] )
				{
					FMemory::Free( MipData[i][MipIndex] );
				}
				MipData[i][MipIndex] = NULL;
			}
		}
	}
	
	/**
	 * Called when the resource is initialized. This is only called by the rendering thread.
	 */
	virtual void InitRHI() override
	{
		INC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
		INC_DWORD_STAT_FNAME_BY( LODGroupStatName, TextureSize );

		// Create the RHI texture.
		uint32 TexCreateFlags = (Owner->SRGB ? TexCreate_SRGB : 0)  | TexCreate_OfflineProcessed;
		FRHIResourceCreateInfo CreateInfo;
		TextureCubeRHI = RHICreateTextureCube( Owner->GetSizeX(), Owner->GetPixelFormat(), Owner->GetNumMips(), TexCreateFlags, CreateInfo );
		TextureRHI = TextureCubeRHI;
		TextureRHI->SetName(Owner->GetFName());
		RHIBindDebugLabelName(TextureRHI, *Owner->GetName());
		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,TextureRHI);

		// Read the mip-levels into the RHI texture.
		int32 NumMips = Owner->GetNumMips();
		for( int32 FaceIndex=0; FaceIndex<6; FaceIndex++ )
		{
			for(int32 MipIndex=0; MipIndex < NumMips; MipIndex++)
			{
				if( MipData[FaceIndex][MipIndex] != NULL )
				{
					uint32 DestStride;
					void* TheMipData = RHILockTextureCubeFace( TextureCubeRHI, FaceIndex, 0, MipIndex, RLM_WriteOnly, DestStride, false );
					GetData( FaceIndex, MipIndex, TheMipData, DestStride );
					RHIUnlockTextureCubeFace( TextureCubeRHI, FaceIndex, 0, MipIndex, false );
				}
			}
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer
		(
			(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter( Owner ),
			AM_Clamp,
			AM_Clamp,
			AM_Clamp
		);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

		// Set the greyscale format flag appropriately.
		EPixelFormat PixelFormat = Owner->GetPixelFormat();
		bGreyScaleFormat = (PixelFormat == PF_G8) || (PixelFormat == PF_BC4);
	}

	virtual void ReleaseRHI() override
	{
		DEC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
		DEC_DWORD_STAT_FNAME_BY( LODGroupStatName, TextureSize );
		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,FTextureRHIParamRef());
		TextureCubeRHI.SafeRelease();
		FTextureResource::ReleaseRHI();
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override
	{
		return Owner->GetSizeX();
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const override
	{
		return Owner->GetSizeY();
	}

private:

	/** A reference to the texture's RHI resource as a cube-map texture. */
	FTextureCubeRHIRef TextureCubeRHI;

	/** Local copy/ cache of mip data. Only valid between creation and first call to InitRHI */
	void* MipData[6][MAX_TEXTURE_MIP_COUNT];

	/** The UTextureCube which this resource represents. */
	const UTextureCube* Owner;

	// Cached texture size for stats. */
	int32	TextureSize;

	/**
	 * Writes the data for a single mip-level into a destination buffer.
	 * @param FaceIndex		The index of the face of the mip-level to read.
	 * @param MipIndex		The index of the mip-level to read.
	 * @param Dest			The address of the destination buffer to receive the mip-level's data.
	 * @param DestPitch		Number of bytes per row
	 */
	void GetData( int32 FaceIndex, int32 MipIndex, void* Dest, uint32 DestPitch )
	{
		check( MipData[FaceIndex][MipIndex] );

		// for platforms that returned 0 pitch from Lock, we need to just use the bulk data directly, never do 
		// runtime block size checking, conversion, or the like
		if (DestPitch == 0)
		{
			FMemory::Memcpy(Dest, MipData[FaceIndex][MipIndex], Owner->PlatformData->Mips[MipIndex].BulkData.GetBulkDataSize() / 6);
		}
		else
		{
			EPixelFormat PixelFormat = Owner->GetPixelFormat();
			uint32 NumRows = 0;
			uint32 SrcPitch = 0;
			uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;	// Block width in pixels
			uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;	// Block height in pixels
			uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;

			FIntPoint MipExtent = CalcMipMapExtent(Owner->GetSizeX(), Owner->GetSizeY(), PixelFormat, MipIndex);

			uint32 NumColumns = (MipExtent.X + BlockSizeX - 1) / BlockSizeX;	// Num-of columns in the source data (in blocks)
			NumRows    = (MipExtent.Y + BlockSizeY - 1) / BlockSizeY;	// Num-of rows in the source data (in blocks)
			SrcPitch   = NumColumns * BlockBytes;		// Num-of bytes per row in the source data
				
			SIZE_T MipSizeInBytes = CalcTextureMipMapSize(MipExtent.X, MipExtent.Y, PixelFormat, 0);

			if (SrcPitch == DestPitch)
			{

				// Copy data, not taking into account stride!
				FMemory::Memcpy(Dest, MipData[FaceIndex][MipIndex], MipSizeInBytes);
			}
			else
			{
				// Copy data, taking the stride into account!
				uint8 *Src = (uint8*) MipData[FaceIndex][MipIndex];
				uint8 *Dst = (uint8*) Dest;
				for ( uint32 Row=0; Row < NumRows; ++Row )
				{
					FMemory::Memcpy( Dst, Src, SrcPitch );
					Src += SrcPitch;
					Dst += DestPitch;
				}
				check( (PTRINT(Src) - PTRINT(MipData[FaceIndex][MipIndex])) == PTRINT(MipSizeInBytes) );
			}
		}

		FMemory::Free( MipData[FaceIndex][MipIndex] );
		MipData[FaceIndex][MipIndex] = NULL;
	}
};

FTextureResource* UTextureCube::CreateResource()
{
	FTextureResource* NewResource = NULL;
	if (GetNumMips() > 0)
	{
		NewResource = new FTextureCubeResource(this);
	}
	return NewResource;
}

void UTextureCube::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
	CumulativeResourceSize.AddUnknownMemoryBytes(CalcTextureMemorySizeEnum(TMC_ResidentMips));
}

#if WITH_EDITOR
uint32 UTextureCube::GetMaximumDimension() const
{
	return GetMaxCubeTextureDimension();
}
#endif // #if WITH_EDITOR
