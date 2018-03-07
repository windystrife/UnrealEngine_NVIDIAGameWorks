// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn_DDC_AsyncCreate.h: Load texture 2D mips from the DDC using async create.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Texture2DStreamIn_DDC.h"

#if WITH_EDITORONLY_DATA

class FTexture2DStreamIn_DDC_AsyncCreate : public FTexture2DStreamIn_DDC
{
public:

	FTexture2DStreamIn_DDC_AsyncCreate(UTexture2D* InTexture, int32 InRequestedMips);

protected:

	// ****************************
	// ******* Update Steps *******
	// ****************************

	// Allocate the MipData (AsyncThread)
	void AllocateAndLoadMips(const FContext& Context);
	// Create load requests into each locked mips. (AsyncThread)
	void AsyncCreate(const FContext& Context);
	// Apply the intermediate texture and cleanup. (RenderThread)
	void Finalize(const FContext& Context);

	// ****************************
	// ******* Cancel Steps *******
	// ****************************

	// Cancel the update. (RenderThread)
	void Cancel(const FContext& Context);
};

#endif // WITH_EDITORONLY_DATA
