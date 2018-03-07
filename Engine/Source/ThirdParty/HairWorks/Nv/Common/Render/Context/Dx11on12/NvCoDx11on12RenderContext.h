#ifndef NV_CO_DX11_ON_12_RENDER_CONTEXT_H
#define NV_CO_DX11_ON_12_RENDER_CONTEXT_H

#include "NvCoDx11on12RenderInterface.h"

#include <Nv/Common/Render/Context/Dx12/NvCoDx12RenderContext.h>
#include <Nv/Common/Render/Context/Dx11/NvCoDx11RenderInterface.h>
#include <Nv/Common/NvCoApiHandle.h>

namespace nvidia {
namespace Common {

class Dx11on12RenderContext : public Dx12RenderContext, public Dx11RenderInterface
{	
	NV_CO_DECLARE_POLYMORPHIC_CLASS(Dx11on12RenderContext, Dx12RenderContext)

	Dx11on12RenderContext(Int width, Int height);

	// RenderContext
	virtual Result initialize(const RenderContextOptions& options, Void* windowHandle) NV_OVERRIDE;
	void endRender() NV_OVERRIDE;
	void beginRender() NV_OVERRIDE;

	virtual Void* getInterface(EApiType apiType) NV_OVERRIDE;

	// Dx11RenderInterface
	ID3D11Device* getDx11Device() const NV_OVERRIDE;
	ID3D11DeviceContext* getDx11DeviceContext() const NV_OVERRIDE;

protected:

	// Dx12RenderContext
	Result createFrameResources() NV_OVERRIDE;
	void releaseFrameResources() NV_OVERRIDE;

	struct Target
	{
		Void reset()
		{
			m_renderTarget.setNull();
			m_renderTargetView.setNull();
		}

		ComPtr<ID3D11RenderTargetView> m_renderTargetView;		///< Render target view		
		ComPtr<ID3D11Resource> m_renderTarget;					///< Wraps the dx12 render target
	};

	Target m_targets[MAX_NUM_RENDER_TARGETS];

	ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
	ComPtr<ID3D11On12Device> m_d3d11On12Device;
	ComPtr<ID3D11Device> m_d3d11Device;

	ComPtr<ID3D11DepthStencilState> m_d3d11DepthStencilState;
	ComPtr<ID3D11RasterizerState> m_d3d11RasterizerState;
	ComPtr<ID3D11BlendState> m_d3d11BlendState;

	ComPtr<ID3D11Resource> m_d3d11DepthStencil;
	ComPtr<ID3D11DepthStencilView> m_d3d11DepthStencilView;
};

} // namespace Common 
} // namespace nvidia

#endif // NV_CO_DX11_ON_12_RENDER_CONTEXT_H