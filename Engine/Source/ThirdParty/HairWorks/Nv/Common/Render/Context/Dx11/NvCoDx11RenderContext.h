#ifndef NV_CO_DX11_RENDER_CONTEXT_H
#define NV_CO_DX11_RENDER_CONTEXT_H

#include <Nv/Common/Render/Context/Dx11/NvCoDx11RenderInterface.h>

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoComPtr.h>
#include <Nv/Common/NvCoApiHandle.h>

#include <Nv/Core/1.0/NvResult.h>

#include <Nv/Common/Render/Context/NvCoRenderContext.h>

namespace nvidia {
namespace Common {

class Dx11RenderContext : public RenderContext, public Dx11RenderInterface
{	
	NV_CO_DECLARE_POLYMORPHIC_CLASS(Dx11RenderContext, RenderContext)

	struct DeviceSettings
	{
		Int m_adapterOrdinal;
		D3D_DRIVER_TYPE m_driverType;
		Int m_output;
		DXGI_SWAP_CHAIN_DESC m_swapChainDesc;		///< For MSAA set SampleDesc.Count > 1
		UInt32 m_createFlags;						///< One of D3D11_CREATE_DEVICE_FLAG
		UInt32 m_syncInterval;						///< If 0 vsync is enabled
		UInt m_presentFlags;						
		Bool m_autoCreateDepthStencil; 
		DXGI_FORMAT m_autoDepthStencilFormat;
		D3D_FEATURE_LEVEL m_deviceFeatureLevel;
		D3D_FEATURE_LEVEL m_minimumFeatureLevel;
	};


	// Dx11AppInterface
	virtual ID3D11Device* getDx11Device() const NV_OVERRIDE;
	virtual ID3D11DeviceContext* getDx11DeviceContext() const NV_OVERRIDE;

	// RenderContext impl
	virtual Void* getInterface(EApiType apiType) NV_OVERRIDE;
	virtual void onSizeChanged(Int width, Int height, Bool minimized) NV_OVERRIDE;
	virtual void waitForGpu() NV_OVERRIDE;
	virtual void beginGpuWork() NV_OVERRIDE;
	virtual void endGpuWork() NV_OVERRIDE;
	virtual void beginRender() NV_OVERRIDE;
	virtual void endRender() NV_OVERRIDE;
	virtual void submitGpuWork() NV_OVERRIDE;
	virtual void prepareRenderTarget() NV_OVERRIDE;
	virtual void clearRenderTarget(const AlignedVec4* clearColorRgba) NV_OVERRIDE;
	virtual void present() NV_OVERRIDE;
	virtual Result toggleFullScreen() NV_OVERRIDE;
	virtual Bool isFullScreen() NV_OVERRIDE;
	virtual Result initialize(const RenderContextOptions& options, Void* windowHandle) NV_OVERRIDE;

	Dx11RenderContext(Int width, Int height);

protected:

	Result createFrameResources();
	void releaseFrameResources();

	ComPtr<IDXGISwapChain> m_swapChain;

	ComPtr<ID3D11Device> m_device;            
	ComPtr<ID3D11DeviceContext>	m_deviceContext;
	
	ComPtr<ID3D11Texture2D> m_renderTarget;
	ComPtr<ID3D11Texture2D> m_depthStencil;

	ComPtr<ID3D11DepthStencilView> m_depthStencilView;
	ComPtr<ID3D11RenderTargetView> m_renderTargetView;

	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	DeviceSettings m_deviceSettings;

	Int m_scopeCount;

	HWND m_hwnd;
};

} // namespace Common 
} // namespace nvidia

#endif // NV_CO_DX11_RENDER_CONTEXT_H