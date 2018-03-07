// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn_DDC.h: Stream in helper for 2D textures loading DDC files.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Texture2DStreamIn.h"

#if WITH_EDITORONLY_DATA

class FTexture2DStreamIn_DDC : public FTexture2DStreamIn
{
public:

	FTexture2DStreamIn_DDC(UTexture2D* InTexture, int32 InRequestedMips)
	: FTexture2DStreamIn(InTexture, InRequestedMips)
	, bDDCIsInvalid(false)
	{
	}

	/** Returns whether DDC of this texture needs to be regenerated.  */
	bool DDCIsInvalid() const override { return bDDCIsInvalid; }

protected:

	// Whether the DDC data was compatible or not.
	bool bDDCIsInvalid;

	// ****************************
	// ********* Helpers **********
	// ****************************

	// Load from DDC into MipData
	void DoLoadNewMipsFromDDC(const FContext& Context);
};

#endif // WITH_EDITORONLY_DATA
