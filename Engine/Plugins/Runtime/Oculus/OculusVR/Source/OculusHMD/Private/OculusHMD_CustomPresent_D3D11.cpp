// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
#include "OculusHMD.h"

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FD3D11CustomPresent
//-------------------------------------------------------------------------------------------------

class FD3D11CustomPresent : public FCustomPresent
{
public:
	FD3D11CustomPresent(FOculusHMD* InOculusHMD);

	// Implementation of FCustomPresent, called by Plugin itself
	virtual bool IsUsingCorrectDisplayAdapter() const override;
	virtual void* GetOvrpDevice() const override;
	virtual FTextureRHIRef CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 InTexCreateFlags) override;
	virtual void AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture) override;
};


FD3D11CustomPresent::FD3D11CustomPresent(FOculusHMD* InOculusHMD) :
	FCustomPresent(InOculusHMD, ovrpRenderAPI_D3D11, PF_B8G8R8A8, true)
{
}


bool FD3D11CustomPresent::IsUsingCorrectDisplayAdapter() const
{
	const void* luid;

	if (OVRP_SUCCESS(ovrp_GetDisplayAdapterId2(&luid)) && luid)
	{
		TRefCountPtr<ID3D11Device> D3D11Device;

		ExecuteOnRenderThread([&D3D11Device]()
		{
			D3D11Device = (ID3D11Device*) RHIGetNativeDevice();
		});

		if (D3D11Device)
		{
			TRefCountPtr<IDXGIDevice> DXGIDevice;
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if (SUCCEEDED(D3D11Device->QueryInterface(__uuidof(IDXGIDevice), (void**) DXGIDevice.GetInitReference())) &&
				SUCCEEDED(DXGIDevice->GetAdapter(DXGIAdapter.GetInitReference())) &&
				SUCCEEDED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)))
			{
				return !FMemory::Memcmp(luid, &DXGIAdapterDesc.AdapterLuid, sizeof(LUID));
			}
		}
	}

	// Not enough information.  Assume that we are using the correct adapter.
	return true;
}


void* FD3D11CustomPresent::GetOvrpDevice() const
{
	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	return DynamicRHI->GetDevice();
}


FTextureRHIRef FD3D11CustomPresent::CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 InTexCreateFlags)
{
	CheckInRenderThread();

	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);

	switch (InResourceType)
	{
	case RRT_Texture2D:
		return DynamicRHI->RHICreateTexture2DFromResource(InFormat, InTexCreateFlags, InBinding, (ID3D11Texture2D*) InTexture).GetReference();

	case RRT_TextureCube:
		return DynamicRHI->RHICreateTextureCubeFromResource(InFormat, InTexCreateFlags, InBinding, (ID3D11Texture2D*) InTexture).GetReference();

	default:
		return nullptr;
	}
}


void FD3D11CustomPresent::AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture)
{
	CheckInRHIThread();

	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	DynamicRHI->RHIAliasTextureResources(DestTexture, SrcTexture);
}


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FCustomPresent* CreateCustomPresent_D3D11(FOculusHMD* InOculusHMD)
{
	return new FD3D11CustomPresent(InOculusHMD);
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
