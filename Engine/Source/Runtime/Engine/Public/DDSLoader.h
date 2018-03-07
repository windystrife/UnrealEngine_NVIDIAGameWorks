// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"

class UTexture2D;

enum EDDSFlags
{
	DDSF_Caps			= 0x00000001,
	DDSF_Height			= 0x00000002,
	DDSF_Width			= 0x00000004,
	DDSF_PixelFormat	= 0x00001000
};

enum EDDSCaps
{
	DDSC_CubeMap			= 0x00000200,
	DDSC_CubeMap_AllFaces	= 0x00000400 | 0x00000800 | 0x00001000 | 0x00002000 | 0x00004000 | 0x00008000,
	DDSC_Volume				= 0x00200000,
};

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)\
	((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |\
	((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))
#endif

enum EDDSPixelFormat
{
	DDSPF_FourCC		= 0x00000004,
	DDSPF_RGB			= 0x00000040,
	DDSPF_DXT1			= MAKEFOURCC('D','X','T','1'),
	DDSPF_DXT3			= MAKEFOURCC('D','X','T','3'),
	DDSPF_DXT5			= MAKEFOURCC('D','X','T','5')
};

// .DDS subheader.
#pragma pack(push,1)
struct FDDSPixelFormatHeader
{
	uint32 dwSize;
	uint32 dwFlags;
	uint32 dwFourCC;
	uint32 dwRGBBitCount;
	uint32 dwRBitMask;
	uint32 dwGBitMask;
	uint32 dwBBitMask;
	uint32 dwABitMask;
};
#pragma pack(pop)

// .DDS header.
#pragma pack(push,1)
struct FDDSFileHeader
{
	uint32 dwSize;
	uint32 dwFlags;
	uint32 dwHeight;
	uint32 dwWidth;
	uint32 dwLinearSize;
	uint32 dwDepth;
	uint32 dwMipMapCount;
	uint32 dwReserved1[11];
	FDDSPixelFormatHeader ddpf;
	uint32 dwCaps;
	uint32 dwCaps2;
	uint32 dwCaps3;
	uint32 dwCaps4;
	uint32 dwReserved2;
};
#pragma pack(pop)

class FDDSLoadHelper
{
public:
	/** @param Buffer must not be 0 */
	ENGINE_API FDDSLoadHelper(const uint8* Buffer, uint32 Length);

	bool IsValid() const;
		
	/** @return PF_Unknown for DDS that are not valid or where the format is not [yet] supported */
	ENGINE_API EPixelFormat ComputePixelFormat() const;

	/** @return TSF_Invalid for DDS that are not valid or where the format is not [yet] supported */
	ENGINE_API ETextureSourceFormat ComputeSourceFormat() const;

	/** @param DDS, must not be 0 */
	ENGINE_API uint32 ComputeMipMapCount() const;

	ENGINE_API bool IsValidCubemapTexture() const;

	ENGINE_API bool IsValid2DTexture() const;

	/** @param Face should only be provide for cube map textures. */
	ENGINE_API const uint8* GetDDSDataPointer(ECubeFace Face = CubeFace_PosX) const;

	/** Extracts the cube map size from the texture name */
	ENGINE_API const uint8* GetDDSDataPointer(const UTexture2D& Texture) const;
	
// for now we allow direct access to the header but that could be changed

	/** !=0 if valid */
	const FDDSFileHeader* DDSHeader;
};
