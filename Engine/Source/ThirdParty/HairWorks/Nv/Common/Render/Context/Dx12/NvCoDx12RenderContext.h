#ifndef NV_CO_DX12_RENDER_CONTEXT_H
#define NV_CO_DX12_RENDER_CONTEXT_H

#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/Render/Dx12/NvCoDx12Resource.h>
#include <Nv/Common/Render/Dx12/NvCoDx12CounterFence.h>

#include "NvCoDx12RenderInterface.h"

#include <Nv/Common/Render/Context/NvCoRenderContext.h>

namespace nvidia {
namespace Common {

class Dx12RenderContext : public RenderContext, public Dx12RenderInterface
{	
	NV_CO_DECLARE_POLYMORPHIC_CLASS(Dx12RenderContext, RenderContext)

	struct FrameInfo
	{
		FrameInfo() :m_fenceValue(0) {}
		void reset()
		{
			m_commandAllocator.setNull();
		}
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;			///< The command allocator for this frame
		UINT64 m_fenceValue;										///< The fence value when rendering this Frame is complete
	};

	// Dx12RenderInterface
	virtual Result setDx12InitInfo(const InitInfo& info) NV_OVERRIDE;
	virtual ID3D12Device* getDx12Device() const NV_OVERRIDE;
	virtual ID3D12CommandQueue* getDx12CommandQueue() const NV_OVERRIDE;
	virtual ID3D12GraphicsCommandList* getDx12CommandList() const NV_OVERRIDE;
	virtual const D3D12_VIEWPORT& getDx12Viewport() const NV_OVERRIDE;
	virtual const Dx12TargetInfo& getDx12TargetInfo() const NV_OVERRIDE;
	virtual Dx12ResourceBase& getDx12Resource(ResourceType type) NV_OVERRIDE;
	virtual Dx12ResourceBase& getDx12BackBuffer() NV_OVERRIDE;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE getDx12CpuHandle(ResourceType type) NV_OVERRIDE;

	// RenderContext impl
	virtual Void* getInterface(EApiType apiType) NV_OVERRIDE;
	
	virtual void onSizeChanged(Int width, Int height, Bool minimized) NV_OVERRIDE;
	virtual void waitForGpu() NV_OVERRIDE;
	virtual void beginGpuWork() NV_OVERRIDE;
	virtual void endGpuWork() NV_OVERRIDE;
	virtual void submitGpuWork() NV_OVERRIDE;

	virtual void beginRender() NV_OVERRIDE;
	virtual void endRender() NV_OVERRIDE;

	virtual void prepareRenderTarget() NV_OVERRIDE;
	virtual void clearRenderTarget(const AlignedVec4* clearColorRgba) NV_OVERRIDE;

	virtual void present() NV_OVERRIDE;

	virtual Result toggleFullScreen() NV_OVERRIDE;
	virtual Bool isFullScreen() NV_OVERRIDE;

	virtual Result initialize(const RenderContextOptions& options, Void* windowHandle) NV_OVERRIDE;

	void executeAndWait(ID3D12GraphicsCommandList* commantList);

	FrameInfo& getFrame() { return m_frameInfos[m_frameIndex]; }
	const FrameInfo& getFrame() const { return m_frameInfos[m_frameIndex]; }

	ID3D12Device* getDevice() const { return m_device; }
	ID3D12CommandQueue* getCommandQueue() const { return m_commandQueue; }
	ID3D12GraphicsCommandList* getCommandList() const { return m_commandList; }
	const D3D12_VIEWPORT& getViewport() const { return m_viewport; }

		/// True if multi sampling is being used
		/// If so it will mean render targets are multi-sampled, and a resolve will be required to convert render targets
		/// into the back buffer format
	Bool isMultiSampled() const { return m_options.m_numMsaaSamples > 1; }

		// Get the debug interface
	NV_FORCE_INLINE ID3D12Debug* getDx12Debug() const { return m_dxDebug; }

		/// Ctor
	Dx12RenderContext(Int width, Int height);
		/// Dtor
	virtual ~Dx12RenderContext();

		/// Find a hardware adapter which supports dx12
	static Result findHardwareAdapter(IDXGIFactory2* factory, ComPtr<IDXGIAdapter1>& adapterOut);

protected:

	virtual Result createFrameResources();
	virtual void releaseFrameResources();

	Result _loadPipeline();

	static const Int MAX_NUM_RENDER_FRAMES = 4;
	static const Int MAX_NUM_RENDER_TARGETS = 3;

	// Pipeline objects.
	D3D12_VIEWPORT m_viewport;

	ComPtr<ID3D12Debug> m_dxDebug;

	ComPtr<ID3D12Device> m_device;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	D3D12_RECT m_scissorRect;

	UINT m_rtvDescriptorSize;
	
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	UINT m_dsvDescriptorSize;

	Int m_commandListOpenCount;			///< If >0 the command lis should be open
	
	// Track the state of the window.
	// If it's minimized the app may decide not to render frames.
	//Bool m_windowVisible;
	Bool m_resizeResources;

	// Synchronization objects.
	Dx12CounterFence m_fence;

	HANDLE m_swapChainWaitableObject;

	// Frame specific data
	Int m_numRenderFrames;
	UINT m_frameIndex;
	FrameInfo m_frameInfos[MAX_NUM_RENDER_FRAMES];
	
	Int m_numRenderTargets;
	Int m_renderTargetIndex;
	
	Dx12Resource* m_backBuffers[MAX_NUM_RENDER_TARGETS];
	Dx12Resource* m_renderTargets[MAX_NUM_RENDER_TARGETS];

	Dx12Resource m_backBufferResources[MAX_NUM_RENDER_TARGETS];
	Dx12Resource m_renderTargetResources[MAX_NUM_RENDER_TARGETS];

	Dx12Resource m_depthStencil;
	D3D12_CPU_DESCRIPTOR_HANDLE m_depthStencilView;

	Int m_depthStencilUsageFlags;	///< DxFormatUtil::UsageFlag combination for depth stencil
	Int m_targetUsageFlags;			///< DxFormatUtil::UsageFlag combination for target

	Dx12RenderInterface::InitInfo m_initInfo;
	Dx12TargetInfo m_targetInfo;

	Bool m_isInitialized;

	HWND m_hwnd;
};

} // namespace Common 
} // namespace nvidia

#endif // NV_CO_DX12_RENDER_CONTEXT_H