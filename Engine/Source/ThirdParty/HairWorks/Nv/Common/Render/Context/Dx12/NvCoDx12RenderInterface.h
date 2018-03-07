#ifndef NV_CO_DX12_RENDER_INTERFACE_H
#define NV_CO_DX12_RENDER_INTERFACE_H

//#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <D3Dcompiler.h>
//#include "d3dx12.h"

#include <Nv/Common/NvCoApiHandle.h>

#include <Nv/Common/Render/Dx12/NvCoDx12Handle.h>
#include <Nv/Common/Render/Dx12/NvCoDx12Resource.h>

namespace nvidia {
namespace Common {

class Dx12RenderInterface
{
	public:

	enum { API_TYPE = NvCo::ApiType::DX12 };

	struct InitInfo
	{
		InitInfo():
			m_backBufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM),
			m_depthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT),
			m_backBufferCanSrv(false),
			m_depthStencilCanSrv(false)
		{
		}

		bool m_backBufferCanSrv;					///< If set allows access to the backbuffer as srv		
		bool m_depthStencilCanSrv;					///< If set allows access to depth stencil as srv
		
		DXGI_FORMAT m_backBufferFormat;
		DXGI_FORMAT m_depthStencilFormat;
	};

	enum ResourceType
	{	
		RESOURCE_TYPE_DEPTH_STENCIL,
		RESOURCE_TYPE_TARGET,
	};

		/// Can only be called before the context has been initialized
	virtual NvResult setDx12InitInfo(const InitInfo& info) = 0;
	virtual ID3D12Device* getDx12Device() const = 0;
	virtual ID3D12CommandQueue* getDx12CommandQueue() const = 0;
	virtual ID3D12GraphicsCommandList* getDx12CommandList() const = 0;
	virtual const D3D12_VIEWPORT& getDx12Viewport() const = 0;
	virtual const Dx12TargetInfo& getDx12TargetInfo() const = 0;
	virtual Dx12ResourceBase& getDx12Resource(ResourceType type) = 0;
	virtual Dx12ResourceBase& getDx12BackBuffer() = 0;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE getDx12CpuHandle(ResourceType type) = 0;
	
	virtual ~Dx12RenderInterface() {}
};

} // namespace Common 
} // namespace nvidia

#endif // NV_CO_DX12_RENDER_INTERFACE_H
