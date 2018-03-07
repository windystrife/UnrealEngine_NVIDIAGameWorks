// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn.cpp: Stream in helper for 2D textures.
=============================================================================*/

#include "Streaming/Texture2DStreamIn.h"
#include "RenderUtils.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

FTexture2DStreamIn::FTexture2DStreamIn(UTexture2D* InTexture, int32 InRequestedMips)
	: FTexture2DUpdate(InTexture, InRequestedMips)
{
	ensure(InRequestedMips > InTexture->GetNumResidentMips());

	FMemory::Memzero(MipData, sizeof(MipData));
}

FTexture2DStreamIn::~FTexture2DStreamIn()
{
#if DO_CHECK
	for (void* ThisMipData : MipData)
	{
		check(!ThisMipData);
	}
#endif
}


void FTexture2DStreamIn::DoAllocateNewMips(const FContext& Context)
{
	if (!IsCancelled() && Context.Resource)
	{
		const TIndirectArray<FTexture2DMipMap>& OwnerMips = Context.Texture->GetPlatformMips();
		const int32 CurrentFirstMip = Context.Resource->GetCurrentFirstMip();

		for (int32 MipIndex = PendingFirstMip; MipIndex < CurrentFirstMip; ++MipIndex)
		{
			const FTexture2DMipMap& MipMap = OwnerMips[MipIndex];
			const int32 MipSize = CalcTextureMipMapSize(MipMap.SizeX, MipMap.SizeY, Context.Resource->GetTexture2DRHI()->GetFormat(), 0);

			check(!MipData[MipIndex]);
			MipData[MipIndex] = FMemory::Malloc(MipSize);
		}
	}
}

void FTexture2DStreamIn::DoFreeNewMips(const FContext& Context)
{
	if (Context.Resource)
	{
		const int32 CurrentFirstMip = Context.Resource->GetCurrentFirstMip();
		for (int32 MipIndex = PendingFirstMip; MipIndex < Context.Resource->GetCurrentFirstMip(); ++MipIndex)
		{
			if (MipData[MipIndex])
			{
				FMemory::Free(MipData[MipIndex]);
				MipData[MipIndex] = nullptr;
			}
		}
	}
}

void FTexture2DStreamIn::DoLockNewMips(const FContext& Context)
{
	check(Context.CurrentThread == TT_Render);

	if (!IsCancelled() && IntermediateTextureRHI && Context.Resource)
	{
		// With virtual textures, all mips exist although they might not be allocated.
		const FTexture2DRHIRef Texture2DRHI = Context.Resource->GetTexture2DRHI();
		const bool bIsVirtualTexture = (Texture2DRHI->GetFlags() & TexCreate_Virtual) == TexCreate_Virtual;
		const int32 MipOffset = bIsVirtualTexture ? 0 : PendingFirstMip;

		const int32 CurrentFirstMip = Context.Resource->GetCurrentFirstMip();
		for (int32 MipIndex = PendingFirstMip; MipIndex < CurrentFirstMip; ++MipIndex)
		{
			check(!MipData[MipIndex]);
			uint32 DestPitch = 0;
			MipData[MipIndex] = RHILockTexture2D(IntermediateTextureRHI, MipIndex - MipOffset, RLM_WriteOnly, DestPitch, false, CVarFlushRHIThreadOnSTreamingTextureLocks.GetValueOnAnyThread() > 0);
		}
	}
}


void FTexture2DStreamIn::DoUnlockNewMips(const FContext& Context)
{
	check(Context.CurrentThread == TT_Render);

	if (IntermediateTextureRHI && Context.Resource)
	{
		// With virtual textures, all mips exist although they might not be allocated.
		const FTexture2DRHIRef Texture2DRHI = Context.Resource->GetTexture2DRHI();
		const bool bIsVirtualTexture = (Texture2DRHI->GetFlags() & TexCreate_Virtual) == TexCreate_Virtual;
		const int32 MipOffset = bIsVirtualTexture ? 0 : PendingFirstMip;

		const int32 CurrentFirstMip = Context.Resource->GetCurrentFirstMip();
		for (int32 MipIndex = PendingFirstMip; MipIndex < CurrentFirstMip; ++MipIndex)
		{
			if (MipData[MipIndex])
			{
				RHIUnlockTexture2D(IntermediateTextureRHI, MipIndex - MipOffset, false, CVarFlushRHIThreadOnSTreamingTextureLocks.GetValueOnAnyThread() > 0 );
				MipData[MipIndex] = nullptr;
			}
		}
	}
}

void FTexture2DStreamIn::DoCopySharedMips(const FContext& Context)
{
	check(Context.CurrentThread == TT_Render);

	if (!IsCancelled() && IntermediateTextureRHI && Context.Resource)
	{
		const FTexture2DRHIRef Texture2DRHI = Context.Resource->GetTexture2DRHI();
		RHICopySharedMips(IntermediateTextureRHI, Texture2DRHI);
	}
}

// Async create the texture to the requested size.
void FTexture2DStreamIn::DoAsyncCreateWithNewMips(const FContext& Context)
{
	check(Context.CurrentThread == TT_Async);

	if (!IsCancelled() && Context.Texture && Context.Resource)
	{
		FTexture2DRHIRef Texture2DRHI = Context.Resource->GetTexture2DRHI();
		if (Texture2DRHI)
		{
			const TIndirectArray<FTexture2DMipMap>& OwnerMips = Context.Texture->GetPlatformMips();
			const FTexture2DMipMap& RequestedMipMap = OwnerMips[PendingFirstMip];
			ensure(!IntermediateTextureRHI);

			const uint32 Flags = (Context.Texture->SRGB ? TexCreate_SRGB : 0) | TexCreate_AllowFailure | TexCreate_DisableAutoDefrag;
			const int32 ResidentMips = OwnerMips.Num() - Context.Resource->GetCurrentFirstMip();

			IntermediateTextureRHI = RHIAsyncCreateTexture2D(
				RequestedMipMap.SizeX,
				RequestedMipMap.SizeY,
				Texture2DRHI->GetFormat(),
				RequestedMips,
				Flags,
				MipData + PendingFirstMip,
				RequestedMips - ResidentMips);
		}
	}
}
