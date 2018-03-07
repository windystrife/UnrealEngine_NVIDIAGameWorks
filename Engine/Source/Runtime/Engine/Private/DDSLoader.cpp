// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "DDSLoader.h"
#include "Engine/Texture2D.h"
#include "RenderUtils.h"

FDDSLoadHelper::FDDSLoadHelper(const uint8* Buffer, uint32 Length) 
	: DDSHeader(0)
{
	check(Buffer);

	const FDDSFileHeader* DDS = (FDDSFileHeader *)(Buffer + 4);

	uint32 AlwaysRequiredFlags = DDSF_Caps | DDSF_Height | DDSF_Width | DDSF_PixelFormat;

	if(Length >= sizeof(FDDSFileHeader) + 4 &&
		Buffer[0]=='D' && Buffer[1]=='D' && Buffer[2]=='S' && Buffer[3]==' ' &&
		DDS->dwSize == sizeof(FDDSFileHeader) &&
		DDS->ddpf.dwSize == sizeof(FDDSPixelFormatHeader) &&
		(DDS->dwFlags & AlwaysRequiredFlags) == AlwaysRequiredFlags)
	{
		DDSHeader = DDS;
	}

}

bool FDDSLoadHelper::IsValid() const
{
	return DDSHeader != 0;
}

EPixelFormat FDDSLoadHelper::ComputePixelFormat() const
{
	EPixelFormat Format = PF_Unknown;

	if(!IsValid())
	{
		return Format;
	}

	if((DDSHeader->ddpf.dwFlags & DDSPF_RGB) != 0 &&
		DDSHeader->ddpf.dwRGBBitCount == 32 &&
		DDSHeader->ddpf.dwRBitMask == 0x00ff0000 &&
		DDSHeader->ddpf.dwGBitMask == 0x0000ff00 &&
		DDSHeader->ddpf.dwBBitMask == 0x000000ff)
	{
		Format = PF_B8G8R8A8;
	}

	if((DDSHeader->ddpf.dwFlags & DDSPF_FourCC) != 0)
	{
		if(DDSHeader->ddpf.dwFourCC == DDSPF_DXT1)
		{
			Format = PF_DXT1;
		}
		else if(DDSHeader->ddpf.dwFourCC == DDSPF_DXT3)
		{
			Format = PF_DXT3;
		}
		else if(DDSHeader->ddpf.dwFourCC == DDSPF_DXT5)
		{
			Format = PF_DXT5;
		}
		else if(DDSHeader->ddpf.dwFourCC == MAKEFOURCC('A','T','I','2') ||
			DDSHeader->ddpf.dwFourCC == MAKEFOURCC('B','C','5','S'))
		{
			Format = PF_BC5;
		}
		else if(DDSHeader->ddpf.dwFourCC == MAKEFOURCC('B','C','4','U') ||
			DDSHeader->ddpf.dwFourCC == MAKEFOURCC('B','C','4','S'))
		{
			Format = PF_BC4;
		}
		else if(DDSHeader->ddpf.dwFourCC == 0x71)
		{
			Format = PF_FloatRGBA;
		}

	}
	return Format; 
}

ETextureSourceFormat FDDSLoadHelper::ComputeSourceFormat() const
{
	ETextureSourceFormat Format = TSF_Invalid;

	if(!IsValid())
	{
		return Format;
	}

	if((DDSHeader->ddpf.dwFlags & DDSPF_RGB) != 0 &&
		DDSHeader->ddpf.dwRGBBitCount == 32 &&
		DDSHeader->ddpf.dwRBitMask == 0x00ff0000 &&
		DDSHeader->ddpf.dwGBitMask == 0x0000ff00 &&
		DDSHeader->ddpf.dwBBitMask == 0x000000ff)
	{
		Format = TSF_BGRA8;
	}

	if((DDSHeader->ddpf.dwFlags & DDSPF_FourCC) != 0)
	{
		if(DDSHeader->ddpf.dwFourCC == 0x71)
		{
			Format = TSF_RGBA16F;
		}

	}
	return Format; 
}

bool FDDSLoadHelper::IsValidCubemapTexture() const
{
	if(IsValid() && (DDSHeader->dwCaps2 & DDSC_CubeMap) != 0 && (DDSHeader->dwCaps2 & DDSC_CubeMap_AllFaces) != 0)
	{
		return true;
	}

	return false;
}

bool FDDSLoadHelper::IsValid2DTexture() const
{
	if(IsValid() && (DDSHeader->dwCaps2 & DDSC_CubeMap) == 0 && (DDSHeader->dwCaps2 & DDSC_Volume) == 0)
	{
		return true;
	}

	return false;
}

uint32 FDDSLoadHelper::ComputeMipMapCount() const
{
	return (DDSHeader->dwMipMapCount > 0) ? DDSHeader->dwMipMapCount : 1;
}

const uint8* FDDSLoadHelper::GetDDSDataPointer(ECubeFace Face) const
{
	uint32 SliceSize = CalcTextureSize(DDSHeader->dwWidth, DDSHeader->dwHeight, ComputePixelFormat(), ComputeMipMapCount());

	const uint8* Ptr = (const uint8*)DDSHeader + sizeof(FDDSFileHeader);

	// jump over the not requested slices / cube map sides
	Ptr += SliceSize * Face;

	return Ptr;
}


const uint8* FDDSLoadHelper::GetDDSDataPointer(const UTexture2D& Texture) const
{
	if(IsValidCubemapTexture())
	{
		FString Name = Texture.GetName();
		ECubeFace Face = GetCubeFaceFromName(Name);

		return GetDDSDataPointer(Face);
	}

	return GetDDSDataPointer();
}
