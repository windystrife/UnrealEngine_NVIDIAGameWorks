#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/NvCoLogger.h>

#include "NvCoDx11on12RenderContext.h"

namespace nvidia {
namespace Common {

#pragma comment( lib, "D3D11.lib" ) 

Dx11on12RenderContext::Dx11on12RenderContext(Int width, Int height) :
	Parent(width, height)
{
}

Result Dx11on12RenderContext::initialize(const RenderContextOptions& options, Void* windowHandle)
{
	// Set up Dx11
	NV_RETURN_ON_FAIL(Parent::initialize(options, windowHandle));

	UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
	// Enable the D3D11 debug layer.
	d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// Create an 11 device wrapped around the 12 device and share
	// 12's command queue.

	NV_RETURN_ON_FAIL(D3D11On12CreateDevice(m_device, d3d11DeviceFlags,	NV_NULL, 0,	reinterpret_cast<IUnknown*const*>(m_commandQueue.readRef()),
		1, 0, m_d3d11Device.writeRef(), m_d3d11DeviceContext.writeRef(), NV_NULL));

	// Query the 11On12 device from the 11 device.
	NV_RETURN_ON_FAIL(m_d3d11Device->QueryInterface(m_d3d11On12Device.writeRef()));

	return NV_OK;
}

ID3D11Device* Dx11on12RenderContext::getDx11Device() const
{
	return m_d3d11Device;
}

ID3D11DeviceContext* Dx11on12RenderContext::getDx11DeviceContext() const
{
	return m_d3d11DeviceContext;
}

Void* Dx11on12RenderContext::getInterface(EApiType apiType)
{
	switch (apiType)
	{
		case ApiType::DX12: return static_cast<Dx12RenderInterface*>(this);
		case ApiType::DX11: return static_cast<Dx11RenderInterface*>(this);
		default: return NV_NULL;	
	}
}

Result Dx11on12RenderContext::createFrameResources()
{
	NV_RETURN_ON_FAIL(Parent::createFrameResources());

	// Create back buffers
	{
		// Create a RTV, and a command allocator for each frame.
		for (Int i = 0; i < m_numRenderTargets; i++)
		{
			Target& target = m_targets[i];

			D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
			NV_RETURN_ON_FAIL(m_d3d11On12Device->CreateWrappedResource(*m_renderTargets[i], &d3d11Flags, D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(target.m_renderTarget.writeRef())));

			// Create the render target view
			NV_RETURN_ON_FAIL(m_d3d11Device->CreateRenderTargetView(target.m_renderTarget, NV_NULL, target.m_renderTargetView.writeRef()));
		}
	}

	// Create the depth stencil view.
	{
		// Create the d3d11 depth stencil
		D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_DEPTH_STENCIL };
		NV_RETURN_ON_FAIL(m_d3d11On12Device->CreateWrappedResource(m_depthStencil, &d3d11Flags,
			D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE, IID_PPV_ARGS(m_d3d11DepthStencil.writeRef())));
		// Create the render target view
		NV_RETURN_ON_FAIL(m_d3d11Device->CreateDepthStencilView(m_d3d11DepthStencil, NV_NULL, m_d3d11DepthStencilView.writeRef()));
	}

	return NV_OK;
}

void Dx11on12RenderContext::releaseFrameResources()
{
	// Detach the render target
	{
		ID3D11RenderTargetView* rtvs[1] = {};
		m_d3d11DeviceContext->OMSetRenderTargets(1, rtvs, nullptr);
	}

	for (Int i = 0; i < m_numRenderTargets; i++)
	{
		m_targets[i].reset();
	}
	// Free dx11 buffers
	m_d3d11DepthStencilView.setNull();
	m_d3d11DepthStencil.setNull();
	// Make sure all state is flushed, and 'clear the state' (as mentioned in MS docs)
	m_d3d11DeviceContext->Flush();
	m_d3d11DeviceContext->ClearState();

#if 0
	// Do a dump of bound resources
	{
		ComPtr<ID3D11Debug> d3dDebug;
		m_d3d11Device->QueryInterface(d3dDebug.writeRef()); ;
		if (d3dDebug)
		{
			d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		}
	}
#endif	

	Parent::releaseFrameResources();
}

void Dx11on12RenderContext::beginRender()
{
	Parent::beginRender();

	//const FrameInfo& frame = m_frameInfos[m_frameIndex];
	const Target& target = m_targets[m_renderTargetIndex];

	// Acquire our wrapped render target resource for the current back buffer.
	m_d3d11On12Device->AcquireWrappedResources(target.m_renderTarget.readRef(), 1);
	if (m_d3d11DepthStencil)
	{
		m_d3d11On12Device->AcquireWrappedResources(m_d3d11DepthStencil.readRef(), 1);
	}

	m_d3d11DeviceContext->OMSetRenderTargets(1, target.m_renderTargetView.readRef(), m_d3d11DepthStencilView);

	{
		// Viewport
		D3D11_VIEWPORT vp;
		vp.Width = (FLOAT)(m_width);
		vp.Height = (FLOAT)(m_height);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		m_d3d11DeviceContext->RSSetViewports(1, &vp);

		m_d3d11DeviceContext->OMSetDepthStencilState(m_d3d11DepthStencilState, 0);
		m_d3d11DeviceContext->RSSetState(m_d3d11RasterizerState);

		float zero[] = { 0,0,0,0 };
		m_d3d11DeviceContext->OMSetBlendState(m_d3d11BlendState, zero, 0xffffffff);
	}
}

void Dx11on12RenderContext::endRender()
{
	// NOTE! This doesn't call the parents onPostRender because that would 
	// add a barrier to transition from render target to present. 
	// We don't need/want this on dx11on12 as when we release the wrapped render target it will do this for us.

	const Target& target = m_targets[m_renderTargetIndex];

	{
		// Close and execute dx12 command list
		NV_CORE_ASSERT_VOID_ON_FAIL(m_commandList->Close());
		ID3D12CommandList* commandLists[] = { m_commandList };
		m_commandQueue->ExecuteCommandLists(NV_COUNT_OF(commandLists), commandLists);
	}

	// Release our wrapped render target resource. Releasing 
	// transitions the back buffer resource to the state specified
	// as the OutState when the wrapped resource was created.
	// IE this adds the transition from render target to present barrier
	if (m_d3d11DepthStencil)
	{
		m_d3d11On12Device->ReleaseWrappedResources(m_d3d11DepthStencil.readRef(), 1);
	}
	m_d3d11On12Device->ReleaseWrappedResources(target.m_renderTarget.readRef(), 1);

	// Flush the Dx11 context
	m_d3d11DeviceContext->Flush();

	// Present the frame.
	present();
}

} // namespace Common 
} // namespace nvidia



