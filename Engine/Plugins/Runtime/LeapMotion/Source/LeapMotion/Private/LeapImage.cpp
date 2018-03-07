// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapImage.h"
#include "LeapInterfaceUtility.h"
#include "Engine/Texture2D.h"
#include "RHI.h"

class FPrivateLeapImage
{
public:
	Leap::Image LeapImage;

	FUpdateTextureRegion2D UpdateTextureRegion;

	FDateTime ImageTimeUtc;

	UTexture2D* validImagePointer(UTexture2D* Pointer, int32 PWidth, int32 PHeight, EPixelFormat Format, bool GammaCorrectionUsed = true);

	UTexture2D* Texture8FromLeapImage(int32 SrcWidth, int32 SrcHeight, uint8* ImageBuffer);
	UTexture2D* Texture32FromLeapImage(int32 SrcWidth, int32 SrcHeight, uint8* ImageBuffer);
	void UpdateTextureRegions(UTexture2D* Texture, int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData);
	UTexture2D* EnqueuedTexture32FromLeapImage(int32 SrcWidth, int32 SrcHeight, uint8* ImageBuffer);
	UTexture2D* Texture32FromLeapDistortion(int32 SrcWidth, int32 SrcHeight, float* ImageBuffer);		//optimized, requires shader channel flipping TODO: change from shader to CPU cost as distortions are fetched once!
	UTexture2D* Texture32PrettyFromLeapDistortion(int32 SrcWidth, int32 SrcHeight, float* ImageBuffer);	//unoptimized roughly +1ms
	UTexture2D* Texture128FromLeapDistortion(int32 SrcWidth, int32 SrcHeight, float* ImageBuffer);		//4x float 32
	UTexture2D* TextureFFromLeapDistortion(int32 SrcWidth, int32 SrcHeight, float* ImageBuffer);

	UTexture2D* Texture32FromLeapImageWithGrid(int32 SrcWidth, int32 SrcHeight, uint8* ImageBuffer);
	
	int32 InvalidSizesReported = 0;
	bool IgnoreTwoInvalidSizesDone = false;
	ULeapImage* Self;
};

ULeapImage::ULeapImage(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateLeapImage())
{
	Private->Self = this;

	//default not using gamma correction
	UseGammaCorrection = false;
}

ULeapImage::~ULeapImage()
{
	delete Private;
}

UTexture2D* FPrivateLeapImage::validImagePointer(UTexture2D* Pointer, int32 PWidth, int32 PHeight, EPixelFormat Format, bool GammaCorrectionUsed)
{
	//Make sure we're valid
	if (!Self->IsValid) 
	{
		UE_LOG(LeapPluginLog, Error, TEXT("Warning! Invalid Image."));
		return nullptr;
	}
	//Instantiate the texture if needed
	if (Pointer == nullptr)
	{
		if (PWidth == 0 || PHeight == 0)
		{
			//Spit out only valid errors, two size 0 textures will be attempted per pointer,
			//unable to filter the messages out without this (or a pointer to Leap Controller, but this uses less resources).
			if (IgnoreTwoInvalidSizesDone)
			{
				UE_LOG(LeapPluginLog, Error, TEXT("Warning! Leap Image SDK access is denied, please enable image support from the Leap Controller before events emit (e.g. at BeginPlay)."));
			}
			else
			{
				InvalidSizesReported++;

				if (InvalidSizesReported == 2)
				{
					IgnoreTwoInvalidSizesDone = true;
				}
			}
			
			return nullptr;
		}
		UE_LOG(LeapPluginLog, Log, TEXT("Created Leap Image Texture Sized: %d, %d, format %d"), PWidth, PHeight, (int)Format);
		Pointer = UTexture2D::CreateTransient(PWidth, PHeight, Format); //PF_B8G8R8A8
		Pointer->UpdateResource();
		UpdateTextureRegion = FUpdateTextureRegion2D(0, 0, 0, 0, PWidth, PHeight);
		
		//change texture settings
		if (!GammaCorrectionUsed)
		{
			Pointer->SRGB = 0;
		}
		
		//keep it around?
	}

	//If the size changed, recreate the image (NB: GC may release the platform data in which case we need to recreate it (since 4.7)
	if (!UtilityPointerIsValid(Pointer->PlatformData) ||
		Pointer->PlatformData->SizeX != PWidth ||
		Pointer->PlatformData->SizeY != PHeight)
	{
		UE_LOG(LeapPluginLog, Log, TEXT("ReCreated Leap Image Texture Sized: %d, %d. Old Size: %d, %d"), PWidth, PHeight, Pointer->PlatformData->SizeX, Pointer->PlatformData->SizeY);
		Pointer = UTexture2D::CreateTransient(PWidth, PHeight, Format);
		Pointer->UpdateResource();
		UpdateTextureRegion = FUpdateTextureRegion2D(0, 0, 0, 0, PWidth, PHeight);
	}
	return Pointer;
}

//Grayscale average texture
UTexture2D* FPrivateLeapImage::Texture32FromLeapImage(int32 SrcWidth, int32 SrcHeight, uint8* ImageBuffer)
{
	// Lock the texture so it can be modified
	if (Self->PImagePointer == nullptr) {
		UE_LOG(LeapPluginLog, Log, TEXT("Image is null!"));
		return nullptr;
	}


	uint8* MipData = static_cast<uint8*>(Self->PImagePointer->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	
	// Create base mip.
	uint8* DestPtr = nullptr;
	const uint8* SrcPtr = nullptr;

	for (int32 y = 0; y<SrcHeight; y++)
	{
		DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];
		SrcPtr = const_cast<uint8*>(&ImageBuffer[(SrcHeight - 1 - y) * SrcWidth]);
		for (int32 x = 0; x<SrcWidth; x++)
		{
			//Grayscale, copy to all channels
			*DestPtr++ = *SrcPtr;
			*DestPtr++ = *SrcPtr;
			*DestPtr++ = *SrcPtr;
			*DestPtr++ = 0xFF;
			SrcPtr++;
		}
	}

	// Unlock the texture
	Self->PImagePointer->PlatformData->Mips[0].BulkData.Unlock();
	Self->PImagePointer->UpdateResource();

	return Self->PImagePointer;
}

void FPrivateLeapImage::UpdateTextureRegions(UTexture2D* Texture, int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData)
{
	if (Texture->Resource)
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			uint32 NumRegions;
			FUpdateTextureRegion2D* Regions;
			uint32 SrcPitch;
			uint32 SrcBpp;
			uint8* SrcData;
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

		RegionData->Texture2DResource = (FTexture2DResource*)Texture->Resource;
		RegionData->MipIndex = MipIndex;
		RegionData->NumRegions = NumRegions;
		RegionData->Regions = Regions;
		RegionData->SrcPitch = SrcPitch;
		RegionData->SrcBpp = SrcBpp;
		RegionData->SrcData = SrcData;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateTextureRegionsData,
			FUpdateTextureRegionsData*, RegionData, RegionData,
			bool, bFreeData, bFreeData,
			{
				for (uint32 RegionIndex = 0; RegionIndex < RegionData->NumRegions; ++RegionIndex)
				{
					int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
					if (RegionData->MipIndex >= CurrentFirstMip)
					{
						RHIUpdateTexture2D(
							RegionData->Texture2DResource->GetTexture2DRHI(),
							RegionData->MipIndex - CurrentFirstMip,
							RegionData->Regions[RegionIndex],
							RegionData->SrcPitch,
							RegionData->SrcData
							+ RegionData->Regions[RegionIndex].SrcY * RegionData->SrcPitch
							+ RegionData->Regions[RegionIndex].SrcX * RegionData->SrcBpp
							);
					}
				}
		if (bFreeData)
		{
			FMemory::Free(RegionData->Regions);
			FMemory::Free(RegionData->SrcData);
		}
		delete RegionData;
			});
	}
}

//Efficiency upgrade - Use render thread to update image - Not currently working properly TODO: fix this version of image fetching
UTexture2D* FPrivateLeapImage::EnqueuedTexture32FromLeapImage(int32 SrcWidth, int32 SrcHeight, uint8* ImageBuffer)
{
	
	// Lock the texture so it can be modified
	if (Self->PImagePointer == nullptr)
	{
		return nullptr;
	}

	UpdateTextureRegions(Self->PImagePointer, 0, 1, &UpdateTextureRegion, 4 * SrcWidth, 4, ImageBuffer, false);
	return Self->PImagePointer;
}

//Used to help alignment tests
UTexture2D* FPrivateLeapImage::Texture32FromLeapImageWithGrid(int32 SrcWidth, int32 SrcHeight, uint8* ImageBuffer)
{

	// Lock the texture so it can be modified
	if (Self->PImagePointer == nullptr)
		return nullptr;

	uint8* MipData = static_cast<uint8*>(Self->PImagePointer->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Create base mip.
	uint8* DestPtr = nullptr;
	const uint8* SrcPtr = nullptr;

	//mix in the grid
	//Leap images are 320x240 with 4,8,10,20,40 as common factors
	int gridSize = 40;

	for (int32 y = 0; y < SrcHeight; y++)
	{
		DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];
		SrcPtr = const_cast<uint8*>(&ImageBuffer[(SrcHeight - 1 - y) * SrcWidth]);

		for (int32 x = 0; x < SrcWidth; x++)
		{
			//Grid pixel 0x77 to make the grid, vary color by row
			if( (y % gridSize) == 0 || (x % gridSize) == 0)
			{
				//if(y % gridSize) == 0 || (x % gridSize) == 0)
				//float fractionColor1 = ((float)y / (float)SrcHeight) * (float)0x77;
				//float fractionColor2 = ((float)x / (float)SrcWidth) * (float)0x77;
				*DestPtr++ = (uint8) 0x77;
				*DestPtr++ = (uint8) 0x77;
				*DestPtr++ = (uint8) 0x77;
				*DestPtr++ = 0x77;
			}
			else
			{
				//Grayscale, copy to all channels
				*DestPtr++ = *SrcPtr;
				*DestPtr++ = *SrcPtr;
				*DestPtr++ = *SrcPtr;
				*DestPtr++ = 0xFF;
				/**DestPtr++ = 0x00;
				*DestPtr++ = 0x00;
				*DestPtr++ = 0x00;
				*DestPtr++ = 0xFF;*/
			}

			SrcPtr++;
		}
	}

	// Unlock the texture
	Self->PImagePointer->PlatformData->Mips[0].BulkData.Unlock();
	Self->PImagePointer->UpdateResource();

	return Self->PImagePointer;
}

//This gives a good looking distortion but it does not cull invalid values nor does it flip the V channel to UE format
//32bit, 8 per channel. Downsampled.
UTexture2D* FPrivateLeapImage::Texture32PrettyFromLeapDistortion(int32 SrcWidth, int32 SrcHeight, float* ImageBuffer)
{
	// Lock the texture so it can be modified
	if (Self->PDistortionPointer == nullptr)
	{
		return nullptr;
	}

	int32 DestWidth = SrcWidth / 2; // Put 2 floats in the R and G channels
	int32 DestHeight = SrcHeight;

	// Lock the texture so it can be modified
	uint8* MipData = static_cast<uint8*>(Self->PDistortionPointer->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Create base mip.
	uint8* DestPtr = nullptr;
	const float* SrcPtr = ImageBuffer;
	DestPtr = MipData;

	for (int d = 0; d < SrcWidth * SrcHeight; d += 2)
	{
		float dX = ImageBuffer[d];
		float dY = ImageBuffer[d + 1];
		int destIndex = d * 2;

		if (!((dX < 0) || (dX > 1)) && !((dY < 0) || (dY > 1))) {
			//R
			DestPtr[destIndex] = (uint8)FMath::Clamp<int32>(FMath::TruncToInt(dX *255.f), 0, 255);

			//G
			DestPtr[destIndex + 1] = (uint8)FMath::Clamp<int32>(255 - FMath::TruncToInt(dY *255.f), 0, 255);

			//B
			DestPtr[destIndex + 2] = 0;

			//A
			DestPtr[destIndex + 3] = 255;

			//if (d == (SrcHeight / 2) * SrcWidth + (SrcWidth / 2))
			//	UE_LOG(LeapPluginLog, Log, TEXT("(%d, %d, %d, %d), (%1.3f,%1.3f) @pos: %d"), DestPtr[destIndex], DestPtr[destIndex + 1], DestPtr[destIndex + 2], DestPtr[destIndex + 3],dX, dY, d);
		}
		else
		{
			//R
			DestPtr[destIndex] = 0;
			
			//G
			DestPtr[destIndex + 1] = 0;

			//B
			DestPtr[destIndex + 2] = 255;

			//A
			DestPtr[destIndex + 3] = 255;
		}
	}

	// Unlock the texture
	Self->PDistortionPointer->PlatformData->Mips[0].BulkData.Unlock();
	Self->PDistortionPointer->UpdateResource();

	return Self->PDistortionPointer;
}

//Optimized 128bit format, one float per channel, two channels unused but the function works as expected.
UTexture2D* FPrivateLeapImage::Texture128FromLeapDistortion(int32 SrcWidth, int32 SrcHeight, float* ImageBuffer)
{
	// Lock the texture so it can be modified
	if (Self->PDistortionPointer == nullptr)
	{
		return nullptr;
	}

	int32 DestWidth = SrcWidth / 2; // Put 2 floats in the R and G channels
	int32 DestHeight = SrcHeight;

	// Lock the texture so it can be modified
	float* MipData = static_cast<float*>(Self->PDistortionPointer->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Create base mip.
	float* DestPtr = nullptr;
	const float* SrcPtr = ImageBuffer;
	DestPtr = MipData;
	int32 length = SrcWidth * SrcHeight;

	for (int d = 0; d < length; d += 2)
	{
		float dX = ImageBuffer[d];
		float dY = ImageBuffer[d + 1];
		int destIndex = d * 2;

		//R
		DestPtr[destIndex] = dX;

		//G
		DestPtr[destIndex + 1] = dY;

		//B
		DestPtr[destIndex + 2] = 0.f;

		//A
		DestPtr[destIndex + 3] = 0.f;
	}

	// Unlock the texture
	Self->PDistortionPointer->PlatformData->Mips[0].BulkData.Unlock();
	Self->PDistortionPointer->UpdateResource();

	return Self->PDistortionPointer;
}

UTexture2D* ULeapImage::Texture()
{

	PImagePointer = Private->validImagePointer(PImagePointer, Width, Height, PF_B8G8R8A8, UseGammaCorrection);
	
	//Normal
	return Private->Texture32FromLeapImage(Width, Height, (uint8*)Private->LeapImage.data());

	//Enqueued - render thread, Todo: enable a working version of this optimization
	//return _private->EnqueuedTexture32FromLeapImage(Width, Height, (uint8*)_private->leapImage.data());
}

UTexture2D* ULeapImage::Distortion()
{
	PDistortionPointer = Private->validImagePointer(PDistortionPointer, DistortionWidth / 2, DistortionHeight, PF_A32B32G32R32F);		//32bit per channel
	
	//return _private->Texture32FromLeapDistortion(DistortionWidth, DistortionHeight, (float*)_private->leapImage.distortion());	
	return Private->Texture128FromLeapDistortion(DistortionWidth, DistortionHeight, (float*)Private->LeapImage.distortion());
}

UTexture2D* ULeapImage::DistortionUE()
{
	PDistortionPointer = Private->validImagePointer(PDistortionPointer, DistortionWidth / 2, DistortionHeight, PF_R8G8B8A8);

	return Private->Texture32PrettyFromLeapDistortion(DistortionWidth, DistortionHeight, (float*)Private->LeapImage.distortion());
}


FVector ULeapImage::Rectify(FVector uv) const
{
	Leap::Vector vect = Leap::Vector(uv.X, uv.Y, uv.Z);
	vect = Private->LeapImage.rectify(vect);
	return FVector(vect.x, vect.y, vect.z);
}

FVector ULeapImage::Warp(FVector xy) const
{
	Leap::Vector vect = Leap::Vector(xy.X, xy.Y, xy.Z);
	vect = Private->LeapImage.warp(vect);
	return FVector(vect.x, vect.y, vect.z);
}

void ULeapImage::SetLeapImage(const Leap::Image &LeapImage)
{
	Private->LeapImage = LeapImage;

	DistortionHeight = Private->LeapImage.distortionHeight();
	DistortionWidth = Private->LeapImage.distortionWidth();
	Height = Private->LeapImage.height();
	Id = Private->LeapImage.id();
	IsValid = Private->LeapImage.isValid();
	RayOffsetX = Private->LeapImage.rayOffsetX();
	RayOffsetY = Private->LeapImage.rayOffsetY();
	RayScaleX = Private->LeapImage.rayScaleX();
	RayScaleY = Private->LeapImage.rayScaleY();
	//Timestamp = Private->LeapImage.timestamp(); //TODO: fix did leap image remove timestamp in 3.0?
	Width = Private->LeapImage.width();
}