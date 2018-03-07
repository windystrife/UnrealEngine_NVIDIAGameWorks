// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn_DDC.cpp: Stream in helper for 2D textures loading DDC files.
=============================================================================*/

#include "Streaming/Texture2DStreamIn_DDC.h"
#include "RenderUtils.h"
#include "DerivedDataCacheInterface.h"
#include "Serialization/MemoryReader.h"

#if WITH_EDITORONLY_DATA

void FTexture2DStreamIn_DDC::DoLoadNewMipsFromDDC(const FContext& Context)
{
	if (Context.Texture && Context.Resource)
	{
		const TIndirectArray<FTexture2DMipMap>& OwnerMips = Context.Texture->GetPlatformMips();
		const int32 CurrentFirstMip = Context.Resource->GetCurrentFirstMip();
		const FTexture2DRHIRef Texture2DRHI = Context.Resource->GetTexture2DRHI();

		for (int32 MipIndex = PendingFirstMip; MipIndex < CurrentFirstMip && !IsCancelled(); ++MipIndex)
		{
			const FTexture2DMipMap& MipMap = OwnerMips[MipIndex];
			const int32 ExpectedMipSize = CalcTextureMipMapSize(MipMap.SizeX, MipMap.SizeY, Texture2DRHI->GetFormat(), 0);

			check(MipData[MipIndex]);

			if (!MipMap.DerivedDataKey.IsEmpty())
			{
				// The overhead of doing 2 copy of each mip data (from GetSynchronous() and FMemoryReader) in hidden by other texture DDC ops happening at the same time.
				TArray<uint8> DerivedMipData;
				if (GetDerivedDataCacheRef().GetSynchronous(*MipMap.DerivedDataKey, DerivedMipData))
				{
					FMemoryReader Ar(DerivedMipData, true);

					int32 MipSize = 0;
					Ar << MipSize;
					if (MipSize == ExpectedMipSize)
					{
						Ar.Serialize(MipData[MipIndex], MipSize);
					}
					else
					{
						UE_LOG(LogTexture, Error, TEXT("DDC mip size (%d) not as expected."), MipIndex);
						MarkAsCancelled();
						bDDCIsInvalid = true;
					}
				}
				else
				{
					// UE_LOG(LogTexture, Warning, TEXT("Failed to stream mip data from the derived data cache for %s. Streaming mips will be recached."), Context.Texture->GetPathName() );
					MarkAsCancelled();
					bDDCIsInvalid = true;
				}
			}
			else
			{
				UE_LOG(LogTexture, Error, TEXT("DDC key missing."));
				MarkAsCancelled();
			}
		}
		FPlatformMisc::MemoryBarrier();
	}
}

#endif