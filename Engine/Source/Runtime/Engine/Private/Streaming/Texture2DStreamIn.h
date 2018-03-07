// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn.h: Stream in helper for 2D textures.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Texture2DUpdate.h"

// Base StreamIn framework exposing MipData
class FTexture2DStreamIn : public FTexture2DUpdate
{
public:

	FTexture2DStreamIn(UTexture2D* InTexture, int32 InRequestedMips);
	~FTexture2DStreamIn();


protected:

	// StreamIn_Default : Locked mips of the intermediate textures, used as disk load destination.
	void* MipData[MAX_TEXTURE_MIP_COUNT];


	// ****************************
	// ********* Helpers **********
	// ****************************

	// Allocate memory for each mip.
	void DoAllocateNewMips(const FContext& Context);
	// Free allocated memory for each mip.
	void DoFreeNewMips(const FContext& Context);

	// Lock each streamed mips into MipData.
	void DoLockNewMips(const FContext& Context);
	// Unlock each streamed mips from MipData.
	void DoUnlockNewMips(const FContext& Context);

	// Copy each shared mip to the intermediate texture.
	void DoCopySharedMips(const FContext& Context);

	// Async create the texture to the requested size.
	void DoAsyncCreateWithNewMips(const FContext& Context);
};
