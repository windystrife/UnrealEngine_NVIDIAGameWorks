// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Slate/SlateDataSheet.h"
#include "Engine/Texture2D.h"
#include "RenderUtils.h"


const FUpdateTextureRegion2D USlateDataSheet::DataSheetUpdateRegion(
	0,
	0,
	0,
	0,
	USlateDataSheet::Data_Width,
	USlateDataSheet::Data_Height);

USlateDataSheet::USlateDataSheet(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}


void USlateDataSheet::Init()
{
	// Ensure that we have a texture resource
	static const EPixelFormat DataFormat = EPixelFormat::PF_B8G8R8A8;
	ensure(GPixelFormats[DataFormat].BlockBytes == Data_PixelSize);

	DataTexture = UTexture2D::CreateTransient(Data_Width, Data_Height, DataFormat);
#if WITH_EDITORONLY_DATA
	DataTexture->MipGenSettings = TMGS_NoMipmaps;
#endif
	DataTexture->SRGB = false;
	DataTexture->AddressX = TA_Clamp;
	DataTexture->AddressY = TA_Clamp;
	DataTexture->Filter = TF_Nearest;
	DataTexture->CompressionSettings = TC_EditorIcon;
	DataTexture->NeverStream = true;
	DataTexture->UpdateResource();

}

void USlateDataSheet::EnqueueUpdateToGPU()
{
	if (ensureMsgf(DataTexture != nullptr, TEXT("Make sure to call Init()")))
	{
		const uint32 DataPitch = Data_Width * Data_PixelSize;
		const uint32 DataSize = DataPitch*Data_Height;
		uint8* SrcData = (uint8*)FMemory::Malloc(DataSize);
		FMemory::Memcpy(SrcData, Data, DataSize);

		auto DataCleanup = [](uint8* InSrcData, const FUpdateTextureRegion2D* Regions)
		{
			FMemory::Free(InSrcData);
		};
		DataTexture->UpdateTextureRegions(0, 1, &DataSheetUpdateRegion, DataPitch, Data_PixelSize, SrcData, DataCleanup);
	}
}


UTexture2D* USlateDataSheet::GetTexture() const
{
	return DataTexture;
}
