#include <Nv/Common/NvCoCommon.h>

#include "NvCoDx11RenderContext.h"

#include <Nv/Common/NvCoMemory.h>
#include <Nv/Common/NvCoLogger.h>

namespace nvidia {
namespace Common {

Dx11RenderContext::Dx11RenderContext(Int width, Int height) :
	Parent(width, height),
	m_scopeCount(0),
	m_hwnd(0)
{
}

ID3D11Device* Dx11RenderContext::getDx11Device() const
{
	return m_device;
}

ID3D11DeviceContext* Dx11RenderContext::getDx11DeviceContext() const
{
	return m_deviceContext;
}

Void* Dx11RenderContext::getInterface(EApiType apiType)
{
	if (apiType == ApiType::DX11)
	{
		return static_cast<Dx11RenderInterface*>(this);
	}
	return NV_NULL;
}

Result Dx11RenderContext::initialize(const RenderContextOptions& options, Void* windowHandle)
{
	NV_RETURN_ON_FAIL(Parent::initialize(options, windowHandle));

	NV_CORE_ASSERT(windowHandle);
	m_hwnd = HWND(windowHandle);

	{
		Memory::zero(&m_deviceSettings, sizeof(m_deviceSettings));

		DeviceSettings& settings = m_deviceSettings;

		settings.m_adapterOrdinal = 0;
		settings.m_autoCreateDepthStencil = true;
		settings.m_autoDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#if defined(DEBUG) || defined(_DEBUG)
		settings.m_createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		settings.m_driverType = D3D_DRIVER_TYPE_HARDWARE;
		settings.m_output = 0;
		settings.m_presentFlags = 0;

		DXGI_SWAP_CHAIN_DESC& sd = settings.m_swapChainDesc;

		sd.BufferCount = 1; // 2;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; 
		//sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		sd.BufferDesc.Height = m_height;
		sd.BufferDesc.Width = m_width;
		sd.BufferDesc.RefreshRate.Numerator = 0;
		sd.BufferDesc.RefreshRate.Denominator = 0;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.OutputWindow = m_hwnd;
		sd.SampleDesc.Count =  (m_options.m_numMsaaSamples <= 0) ? 1 : m_options.m_numMsaaSamples;
		sd.SampleDesc.Quality = 0;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		sd.Windowed = TRUE;

		settings.m_syncInterval = 0;
	}
	
	{
		NV_RETURN_ON_FAIL(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, m_deviceSettings.m_createFlags, NV_NULL, 0, D3D11_SDK_VERSION,
			&m_deviceSettings.m_swapChainDesc, m_swapChain.writeRef(), m_device.writeRef(), NV_NULL, m_deviceContext.writeRef()));
	}
	
	NV_RETURN_ON_FAIL(createFrameResources());

	return NV_OK;
}

Result Dx11RenderContext::createFrameResources()
{
	// Create a render target view
	NV_RETURN_ON_FAIL(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)m_renderTarget.writeRef()));
	NV_RETURN_ON_FAIL(m_device->CreateRenderTargetView(m_renderTarget, NV_NULL, m_renderTargetView.writeRef()));

	if (m_deviceSettings.m_autoCreateDepthStencil)
	{
		// Create depth stencil texture
		D3D11_TEXTURE2D_DESC descDepth;
		descDepth.Width = m_width;
		descDepth.Height = m_height;
		descDepth.MipLevels = 1;
		descDepth.ArraySize = 1;
		descDepth.Format = m_deviceSettings.m_autoDepthStencilFormat;
		descDepth.SampleDesc.Count = m_deviceSettings.m_swapChainDesc.SampleDesc.Count;
		descDepth.SampleDesc.Quality = m_deviceSettings.m_swapChainDesc.SampleDesc.Quality;
		descDepth.Usage = D3D11_USAGE_DEFAULT;
		descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		descDepth.CPUAccessFlags = 0;
		descDepth.MiscFlags = 0;
		// If sample count is 1, change the depth desc to allow MSAA compositing
		if (descDepth.SampleDesc.Count == 1)
		{
			descDepth.Format = DXGI_FORMAT_R24G8_TYPELESS;
			descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		}
		NV_RETURN_ON_FAIL(m_device->CreateTexture2D(&descDepth, NV_NULL, m_depthStencil.writeRef()));

		// Create the depth stencil view
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Format = m_deviceSettings.m_autoDepthStencilFormat;
		dsvDesc.Flags = 0;
		if (descDepth.SampleDesc.Count > 1)
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		else
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		NV_RETURN_ON_FAIL(m_device->CreateDepthStencilView(m_depthStencil, &dsvDesc, m_depthStencilView.writeRef()));
	}
	else
	{
		m_depthStencilView.setNull();
	}

	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.readRef(), m_depthStencilView);

	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = m_depthStencilView.get() != NV_NULL;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS;

		m_device->CreateDepthStencilState(&desc, m_depthStencilState.writeRef());
		m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 0);
	}
	{
		D3D11_RASTERIZER_DESC desc = {};

		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = 0;
		desc.SlopeScaledDepthBias = 0.0f;
		desc.DepthBiasClamp = 0.0f;
		desc.DepthClipEnable = false;
		desc.ScissorEnable = false;
		desc.MultisampleEnable = false;
		desc.AntialiasedLineEnable = false;

		m_device->CreateRasterizerState(&desc, m_rasterizerState.writeRef());
		m_deviceContext->RSSetState(m_rasterizerState);
	}

	return NV_OK;
}

void Dx11RenderContext::releaseFrameResources()
{
	m_depthStencilView.setNull();
	m_depthStencil.setNull();

	m_renderTargetView.setNull();
	m_renderTarget.setNull();
}

void Dx11RenderContext::beginRender()
{
	NV_CORE_ASSERT(m_scopeCount == 0);
	m_scopeCount++;
}

void Dx11RenderContext::endRender()
{
	NV_CORE_ASSERT(m_scopeCount == 1);
	m_scopeCount--;
}

void Dx11RenderContext::beginGpuWork()
{
	// Can only start in begin/EndRender
	NV_CORE_ASSERT(m_scopeCount >= 0);
	m_scopeCount++;
}

void Dx11RenderContext::endGpuWork()
{
	// Must be greater than 1 because must be at least one on exit
	NV_CORE_ASSERT(m_scopeCount >= 1);
	m_scopeCount--;
}

void Dx11RenderContext::waitForGpu()
{
	// Dx11 does all the syncing for us :)
}


void Dx11RenderContext::submitGpuWork() 
{
	// Dx11 does all the syncing for us :)
}

void Dx11RenderContext::prepareRenderTarget()
{
	// Set up default render states	
	m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 0);
	m_deviceContext->RSSetState(m_rasterizerState);

	// Set back the render target
	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.readRef(), m_depthStencilView);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)m_width;
	vp.Height = (FLOAT)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	// Set necessary state.
	m_deviceContext->RSSetViewports(1, &vp);
	{
		D3D11_RECT rect;
		rect.top = 0;
		rect.left = 0;
		rect.bottom = m_height;
		rect.right = m_width;
		m_deviceContext->RSSetScissorRects(1, &rect);
	}
}

void Dx11RenderContext::clearRenderTarget(const NvCo::AlignedVec4* clearColorRgba)
{
	const Float32* clearColor = clearColorRgba ? &clearColorRgba->x : &m_clearColor.x;
	// Clear
	m_deviceContext->ClearRenderTargetView(m_renderTargetView, clearColor);
	if (m_depthStencilView)
	{
		m_deviceContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
}
	
void Dx11RenderContext::present()
{
	NV_CORE_ASSERT(m_scopeCount == 0);
	m_swapChain->Present(0, 0);
}

void Dx11RenderContext::onSizeChanged(Int width, Int height, Bool minimized)
{
	releaseFrameResources();
	
	// Determine if we're fullscreen
	//pDevSettings->d3d11.sd.Windowed = !bFullScreen;
	
	UINT flags = 0;
	
	DXGI_SWAP_CHAIN_DESC& swapChainDesc = m_deviceSettings.m_swapChainDesc;

		// ResizeBuffers
	NV_RETURN_VOID_ON_FAIL(m_swapChain->ResizeBuffers(swapChainDesc.BufferCount, width, height, swapChainDesc.BufferDesc.Format, flags));

	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;

	// Update the width, height, and aspect ratio member variables.
	updateForSizeChange(width, height);

	NV_RETURN_VOID_ON_FAIL(createFrameResources());

	//m_windowVisible = !minimized;
}

Result Dx11RenderContext::toggleFullScreen()
{
	BOOL fullScreenState;
	NV_RETURN_ON_FAIL(m_swapChain->GetFullscreenState(&fullScreenState, NV_NULL));
	if (NV_FAILED(m_swapChain->SetFullscreenState(!fullScreenState, NV_NULL)))
	{
		// Transitions to fullscreen mode can fail when running apps over
		// terminal services or for some other unexpected reason.  Consider
		// notifying the user in some way when this happens.
		OutputDebugString(L"Fullscreen transition failed");
		assert(false);
	}
	return NV_OK;
}

Bool Dx11RenderContext::isFullScreen()
{
	BOOL fullScreenState = false;
	m_swapChain->GetFullscreenState(&fullScreenState, NV_NULL);
	return fullScreenState != 0;
}

} // namespace Common 
} // namespace nvidia
