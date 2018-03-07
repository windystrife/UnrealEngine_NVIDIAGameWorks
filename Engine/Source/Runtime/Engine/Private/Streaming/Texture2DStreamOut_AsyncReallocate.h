// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamOut_AsyncReallocate.h: Stream out logic for texture 2D.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Texture2DUpdate.h"

class FTexture2DStreamOut_AsyncReallocate : public FTexture2DUpdate
{
public:

	FTexture2DStreamOut_AsyncReallocate(UTexture2D* InTexture, int32 InRequestedMips);

protected:

	// ****************************
	// ******* Update Steps *******
	// ****************************

	// Create an intermediate smaller copy of the texture. (RenderThread)
	void AsyncReallocate(const FContext& Context);
	// Apply the intermediate texture and cleanup. (RenderThread)
	void Finalize(const FContext& Context);

	// ****************************
	// ******* Cancel Steps *******
	// ****************************

	// Cancel the update. (RenderThread)
	void Cancel(const FContext& Context);
};
