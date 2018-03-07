// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
#include "OculusHMD.h"

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FCustomPresentD3D12
//-------------------------------------------------------------------------------------------------

class FD3D12CustomPresent : public FCustomPresent
{
public:
	FD3D12CustomPresent(FOculusHMD* InOculusHMD);

	// Implementation of FCustomPresent, called by Plugin itself
	virtual bool IsUsingCorrectDisplayAdapter() const override;
	virtual void* GetOvrpDevice() const override;
	virtual FTextureRHIRef CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 InTexCreateFlags) override;
	virtual void AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture) override;
};


FD3D12CustomPresent::FD3D12CustomPresent(FOculusHMD* InOculusHMD) :
	FCustomPresent(InOculusHMD, ovrpRenderAPI_D3D12, PF_B8G8R8A8, true)
{
}


bool FD3D12CustomPresent::IsUsingCorrectDisplayAdapter() const
{
	const void* luid;

	if (OVRP_SUCCESS(ovrp_GetDisplayAdapterId2(&luid)) && luid)
	{
		TRefCountPtr<ID3D12Device> D3DDevice;

		ExecuteOnRenderThread([&D3DDevice]()
		{
			D3DDevice = (ID3D12Device*) RHIGetNativeDevice();
		});

		if (D3DDevice)
		{
			LUID AdapterLuid = D3DDevice->GetAdapterLuid();
			return !FMemory::Memcmp(luid, &AdapterLuid, sizeof(LUID));
		}
	}

	// Not enough information.  Assume that we are using the correct adapter.
	return true;
}

void* FD3D12CustomPresent::GetOvrpDevice() const
{
	FD3D12DynamicRHI* DynamicRHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);
	return DynamicRHI->RHIGetD3DCommandQueue();
}


FTextureRHIRef FD3D12CustomPresent::CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 InTexCreateFlags)
{
	CheckInRenderThread();

	FD3D12DynamicRHI* DynamicRHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);

	switch (InResourceType)
	{
	case RRT_Texture2D:
		return DynamicRHI->RHICreateTexture2DFromResource(InFormat, InTexCreateFlags, InBinding, (ID3D12Resource*) InTexture).GetReference();

	case RRT_TextureCube:
		return DynamicRHI->RHICreateTextureCubeFromResource(InFormat, InTexCreateFlags, InBinding, (ID3D12Resource*) InTexture).GetReference();

	default:
		return nullptr;
	}
}


void FD3D12CustomPresent::AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture)
{
	CheckInRHIThread();

	FD3D12DynamicRHI* DynamicRHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);
	DynamicRHI->RHIAliasTextureResources(DestTexture, SrcTexture);
}


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FCustomPresent* CreateCustomPresent_D3D12(FOculusHMD* InOculusHMD)
{
	return new FD3D12CustomPresent(InOculusHMD);
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
