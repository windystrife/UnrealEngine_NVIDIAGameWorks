// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "RHI.h"
#include "SlateDataSheet.generated.h"

class UTexture2D;

/**
 * A texture used for communicating data to the GPU.
 * Used in combination with SlateVectorArtData and SlateVectorArtInstanceData to
 * pass data to UI materials.
 */
UCLASS()
class UMG_API USlateDataSheet : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void Init();

	void SetUnitFloat(uint32 Address, int32 Value);
	
	FORCEINLINE_DEBUGGABLE void SetUnitFloat(uint32 Address, float Value)
	{
		SetUINT24(Address, FMath::RoundToInt(Value * (256 * 256 * 256 - 1)));
	}

	FORCEINLINE_DEBUGGABLE void SetUINT24(uint32 Address, int32 Value)
	{
		FColor& Texel = (Data[Address]);
		Texel.R = (uint8)(Value & 0x000000FF);
		Texel.G = (uint8)((Value & 0x0000FF00) >> 8);
		Texel.B = (uint8)((Value & 0x00FF0000) >> 16);
		Texel.A = (uint8)((Value & 0x00000000));
	}
	
	void SetUINT24(uint32 Address, float Value);

	void EnqueueUpdateToGPU();

	UTexture2D* GetTexture() const;

protected:

	static const uint32 Data_Width = 256;
	static const uint32 Data_Height = 1;
	static const uint32 Data_PixelSize = sizeof(FColor);
	static const FUpdateTextureRegion2D DataSheetUpdateRegion;

	UPROPERTY(Transient)
	UTexture2D* DataTexture;

	FColor Data[Data_Width * Data_Height];


	

	



};


