// NvFlow begin

/*=============================================================================
D3D11NvFlow.cpp: D3D device RHI NvFlow interop implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"

#include "GameWorks/RHINvFlowD3D11.h"

inline D3D11_VIEWPORT NvFlowGetViewport(ID3D11DeviceContext* context)
{
	unsigned int numViewPorts = 1u;
	D3D11_VIEWPORT viewport;
	context->RSGetViewports(&numViewPorts, &viewport);
	return viewport;
}

void FD3D11DynamicRHI::NvFlowGetDeviceDesc(FRHINvFlowDeviceDesc* desc)
{
	FRHINvFlowDeviceDescD3D11* descD3D11 = static_cast<FRHINvFlowDeviceDescD3D11*>(desc);
	descD3D11->device = Direct3DDevice;
	descD3D11->deviceContext = Direct3DDeviceIMContext;
}

void FD3D11DynamicRHI::NvFlowGetDepthStencilViewDesc(FTexture2DRHIParamRef depthSurface, FTexture2DRHIParamRef depthTexture, FRHINvFlowDepthStencilViewDesc* desc)
{
	check(depthSurface != nullptr);
	check(depthTexture != nullptr);
	FRHINvFlowDepthStencilViewDescD3D11* descD3D11 = static_cast<FRHINvFlowDepthStencilViewDescD3D11*>(desc);
	descD3D11->dsv = GetD3D11TextureFromRHITexture(depthSurface)->GetDepthStencilView(CurrentDSVAccessType);
	descD3D11->srv = GetD3D11TextureFromRHITexture(depthTexture)->GetShaderResourceView();
	descD3D11->viewport = NvFlowGetViewport(Direct3DDeviceIMContext);
}

void FD3D11DynamicRHI::NvFlowGetRenderTargetViewDesc(FRHINvFlowRenderTargetViewDesc* desc)
{
	FRHINvFlowRenderTargetViewDescD3D11* descD3D11 = static_cast<FRHINvFlowRenderTargetViewDescD3D11*>(desc);
	descD3D11->rtv = CurrentRenderTargets[0];
	descD3D11->viewport = NvFlowGetViewport(Direct3DDeviceIMContext);
}

namespace
{
	class FEmptyResource : public FRHIResource, public FD3D11BaseShaderResource
	{
	public:
		FEmptyResource()
		{
		}

		// IRefCountedObject interface.
		virtual uint32 AddRef() const
		{
			return FRHIResource::AddRef();
		}
		virtual uint32 Release() const
		{
			return FRHIResource::Release();
		}
		virtual uint32 GetRefCount() const
		{
			return FRHIResource::GetRefCount();
		}
	};
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::NvFlowCreateSRV(const FRHINvFlowResourceViewDesc* desc)
{
	const FRHINvFlowResourceViewDescD3D11* descD3D11 = static_cast<const FRHINvFlowResourceViewDescD3D11*>(desc);

	TRefCountPtr<FD3D11BaseShaderResource> Resource = new FEmptyResource();

	TRefCountPtr<ID3D11ShaderResourceView> SRV = descD3D11->srv;
	return new FD3D11ShaderResourceView(SRV, Resource);
}

FRHINvFlowResourceRW* FD3D11DynamicRHI::NvFlowCreateResourceRW(const FRHINvFlowResourceRWViewDesc* desc, FShaderResourceViewRHIRef* pRHIRefSRV, FUnorderedAccessViewRHIRef* pRHIRefUAV)
{
	const FRHINvFlowResourceRWViewDescD3D11* descD3D11 = static_cast<const FRHINvFlowResourceRWViewDescD3D11*>(desc);

	TRefCountPtr<FD3D11BaseShaderResource> Resource = new FEmptyResource();

	if (pRHIRefSRV)
	{
		TRefCountPtr<ID3D11ShaderResourceView> SRV = descD3D11->srv;
		*pRHIRefSRV = new FD3D11ShaderResourceView(SRV, Resource);
	}
	if (pRHIRefUAV)
	{
		TRefCountPtr<ID3D11UnorderedAccessView> UAV = descD3D11->uav;
		*pRHIRefUAV = new FD3D11UnorderedAccessView(UAV, Resource);
	}
	return nullptr;
}

// NvFlow end