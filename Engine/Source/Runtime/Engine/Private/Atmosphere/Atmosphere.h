// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ResourceArray.h"
#include "RHI.h"
#include "RenderResource.h"
#include "Atmosphere/AtmosphericFogComponent.h"

// Shared by Engine class and Renderer class, need to be core?

/**
 * Bulk data interface for initializing a static vector field volume texture.
 */
class FAtmosphereResourceBulkDataInterface : public FResourceBulkDataInterface
{
public:

	/** Initialization constructor. */
	FAtmosphereResourceBulkDataInterface( void* InBulkData, uint32 InBulkDataSize )
		: BulkData(InBulkData)
		, BulkDataSize(InBulkDataSize)
	{
	}

	/** 
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const override
	{
		check(BulkData != NULL);
		return BulkData;
	}

	/** 
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const override
	{
		check(BulkDataSize > 0);
		return BulkDataSize;
	}

	/**
	 * Free memory after it has been used to initialize RHI resource 
	 */
	virtual void Discard() override
	{
	}

private:

	/** Pointer to the bulk data. */
	void* BulkData;
	/** Size of the bulk data in bytes. */
	uint32 BulkDataSize;
};

class FAtmosphereTextureResource : public FRenderResource
{
public:

	enum ETextureType
	{
		E_Transmittance = 0,
		E_Irradiance,
		E_Inscatter
	};

	/** Texture type */
	ETextureType TexType;
	FTextureRHIRef TextureRHI;

	/** Size of the vector field (X). */
	int32 SizeX;
	/** Size of the vector field (Y). */
	int32 SizeY;
	/** Size of the vector field (Z). */
	int32 SizeZ;

	/** Initialization constructor. */
	explicit FAtmosphereTextureResource( const FAtmospherePrecomputeParameters& PrecomputeParams, FByteBulkData& InTextureData, ETextureType Type )
		: TexType(Type), TextureData(NULL)
	{
		int32 DataSize = sizeof(FColor);
		switch(TexType)
		{
		default:
		case E_Transmittance:
			{
				SizeX = PrecomputeParams.TransmittanceTexWidth;
				SizeY = PrecomputeParams.TransmittanceTexHeight;
				SizeZ = 1;
			}
			break;
		case E_Irradiance:
			{
				SizeX = PrecomputeParams.IrradianceTexWidth;
				SizeY = PrecomputeParams.IrradianceTexHeight;
				SizeZ = 1;
			}
			break;
		case E_Inscatter:
			{
				SizeX = PrecomputeParams.InscatterMuSNum * PrecomputeParams.InscatterNuNum;
				SizeY = PrecomputeParams.InscatterMuNum;
				SizeZ = PrecomputeParams.InscatterAltitudeSampleNum;
				DataSize = sizeof(FFloat16Color);
			}
			break;
		}

		int32 TotalSize = SizeX * SizeY * SizeZ * DataSize;
		if (InTextureData.GetElementCount() == TotalSize)
		{
			// Grab a copy of the static volume data.
			InTextureData.GetCopy(&TextureData, false);
		}
		else
		{
			// Memzero...
			InTextureData.Lock(LOCK_READ_WRITE);
			void* TempVolume = InTextureData.Realloc(TotalSize);
			FMemory::Memzero(TempVolume, TotalSize);
			InTextureData.Unlock();
		}
	}

	/** Destructor. */
	virtual ~FAtmosphereTextureResource()
	{
		FMemory::Free(TextureData);
		TextureData = NULL;
	}

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if (TextureData && GetFeatureLevel() >= ERHIFeatureLevel::SM4)
		{
			switch(TexType)
			{
			default:
			case E_Transmittance:
				{
					const uint32 DataSize = SizeX * SizeY * sizeof(FColor);
					FAtmosphereResourceBulkDataInterface BulkDataInterface(TextureData, DataSize);
					FRHIResourceCreateInfo CreateInfo(&BulkDataInterface);
					TextureRHI = RHICreateTexture2D(
						SizeX, SizeY, PF_B8G8R8A8,
						/*NumMips=*/ 1,
						/*NumSamples=*/ 1,
						/*Flags=*/ TexCreate_ShaderResource,
						/*BulkData=*/ CreateInfo );
					RHIBindDebugLabelName(TextureRHI, TEXT("E_Transmittance"));
				}
				break;
			case E_Irradiance:
				{
					const uint32 DataSize = SizeX * SizeY * sizeof(FColor);
					FAtmosphereResourceBulkDataInterface BulkDataInterface(TextureData, DataSize);
					FRHIResourceCreateInfo CreateInfo(&BulkDataInterface);
					TextureRHI = RHICreateTexture2D(
						SizeX, SizeY, PF_B8G8R8A8,
						/*NumMips=*/ 1,
						/*NumSamples=*/ 1,
						/*Flags=*/ TexCreate_ShaderResource,
													/*BulkData=*/ CreateInfo );
					RHIBindDebugLabelName(TextureRHI, TEXT("E_Irradiance"));
				}
				break;
			case E_Inscatter:
				{
					const uint32 DataSize = SizeX * SizeY * SizeZ * sizeof(FFloat16Color);
					FAtmosphereResourceBulkDataInterface BulkDataInterface(TextureData, DataSize);
					FRHIResourceCreateInfo CreateInfo(&BulkDataInterface);
					TextureRHI = RHICreateTexture3D(
						SizeX, SizeY, SizeZ, PF_FloatRGBA,
						/*NumMips=*/ 1,
						/*Flags=*/ TexCreate_ShaderResource,
													/*BulkData=*/ CreateInfo );
					RHIBindDebugLabelName(TextureRHI, TEXT("E_Inscatter"));
				}
				break;
			}

			FMemory::Free(TextureData);
			TextureData = NULL;
		}
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		TextureRHI.SafeRelease();
	}

private:

	/** Static texture data. */
	void* TextureData;
};

