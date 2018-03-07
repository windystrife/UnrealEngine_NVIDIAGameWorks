#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/Render/Context/Dx12/NvCoDx12RenderInterface.h>

#include <Nv/Common/Render/Dx12/NvCoDx12Handle.h>

#include <Nv/Common/NvCoLogger.h>
#include <Nv/Core/1.0/NvAssert.h>

#include <Nv/Common/Render/Dx12/d3dx12.h>

#include <Nv/Common/NvCoMemory.h>

// this
#include "NvCoDx12RenderContext.h"

namespace nvidia {
namespace Common {

Dx12RenderContext::Dx12RenderContext(Int width, Int height) :
	Parent(width, height),
	m_frameIndex(0),
	m_renderTargetIndex(0),
	m_viewport(),
	m_scissorRect(),
	m_rtvDescriptorSize(0),
	m_resizeResources(true),
	m_swapChainWaitableObject(NV_NULL),
	m_commandListOpenCount(0),
	m_isInitialized(false),
	m_depthStencilUsageFlags(0),
	m_targetUsageFlags(0)
{
	// Set the initial format
	m_targetInfo.init();

	InitInfo initInfo;
	setDx12InitInfo(initInfo);
}

Dx12RenderContext::~Dx12RenderContext()
{
	if (m_isInitialized)
	{
		// Ensure that the GPU is no longer referencing resources that are about to be
		// cleaned up by the destructor.
		waitForGpu();
	}
}

ID3D12Device* Dx12RenderContext::getDx12Device() const
{
	return getDevice();
}
ID3D12CommandQueue* Dx12RenderContext::getDx12CommandQueue() const
{
	return getCommandQueue();
}
ID3D12GraphicsCommandList* Dx12RenderContext::getDx12CommandList() const
{
	return getCommandList();
}
const D3D12_VIEWPORT& Dx12RenderContext::getDx12Viewport() const
{
	return getViewport();
}

const Dx12TargetInfo& Dx12RenderContext::getDx12TargetInfo() const
{
	return m_targetInfo;
}

Dx12ResourceBase& Dx12RenderContext::getDx12Resource(ResourceType type) 
{
	switch (type)
	{
		default:	
		case RESOURCE_TYPE_TARGET:			return *m_renderTargets[m_renderTargetIndex];
		case RESOURCE_TYPE_DEPTH_STENCIL:	return m_depthStencil;
	}
}

Dx12ResourceBase& Dx12RenderContext::getDx12BackBuffer()
{
	return *m_backBuffers[m_renderTargetIndex];
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12RenderContext::getDx12CpuHandle(ResourceType type)
{
	switch (type)
	{
		default:
		case RESOURCE_TYPE_TARGET:				return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_renderTargetIndex, m_rtvDescriptorSize);
		case RESOURCE_TYPE_DEPTH_STENCIL:		return m_depthStencilView;
	}
}

Void* Dx12RenderContext::getInterface(EApiType apiType)
{
	if (apiType == ApiType::DX12)
	{
		return static_cast<Dx12RenderInterface*>(this);
	}
	return NV_NULL;
}

static Int _clamp(Int in, Int min, Int max)
{
	in = (in < min) ? min : in;
	in = (in > max) ? max : in;
	return in;
}

static Int _getDefault(Int in, Int def)
{
	return (in <= 0) ? def : in;
}

Result Dx12RenderContext::initialize(const RenderContextOptions& optionsIn, Void* windowHandle)
{
	NV_CORE_ASSERT(windowHandle);
	m_hwnd = HWND(windowHandle);

	RenderContextOptions options(optionsIn);
	options.m_numRenderFrames = _clamp(_getDefault(options.m_numRenderFrames, 3), 1, MAX_NUM_RENDER_FRAMES);
	options.m_numBackBuffers = _clamp(_getDefault(options.m_numBackBuffers, 2), 2, MAX_NUM_RENDER_TARGETS);
	
	// Set up number frames/backbuffers
	m_numRenderFrames = options.m_numRenderFrames;
	m_numRenderTargets = options.m_numBackBuffers;

	NV_RETURN_ON_FAIL(Parent::initialize(options, windowHandle));
	NV_RETURN_ON_FAIL(_loadPipeline());

	m_isInitialized = true;
	return NV_OK;
}

Result Dx12RenderContext::setDx12InitInfo(const InitInfo& info)
{
	if (m_isInitialized)
	{
		NV_CORE_ALWAYS_ASSERT("Device has been initialized - cannot set");
		return NV_FAIL;
	}

	// Set up usage flags
	m_depthStencilUsageFlags = 0;
	m_targetUsageFlags = 0;

	m_depthStencilUsageFlags |= (info.m_depthStencilCanSrv ? DxFormatUtil::USAGE_FLAG_SRV : 0);
	m_targetUsageFlags |= (info.m_backBufferCanSrv ? DxFormatUtil::USAGE_FLAG_SRV : 0);

	m_initInfo = info;
	return NV_OK;
}

// Load the rendering pipeline dependencies.
Result Dx12RenderContext::_loadPipeline()
{
	//UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
	// Enable the D3D11 debug layer.
	//d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	// Enable the D3D12 debug layer.
	{
		if (NV_SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(m_dxDebug.writeRef()))))
		{
			m_dxDebug->EnableDebugLayer();
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	NV_RETURN_ON_FAIL(CreateDXGIFactory1(IID_PPV_ARGS(factory.writeRef())));

	if (m_options.m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		NV_RETURN_ON_FAIL(factory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.writeRef())));
		NV_RETURN_ON_FAIL(D3D12CreateDevice(warpAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(m_device.writeRef())));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		NV_RETURN_ON_FAIL(findHardwareAdapter(factory, hardwareAdapter));
		NV_RETURN_ON_FAIL(D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(m_device.writeRef())));
	}


	{
		ComPtr<ID3D12InfoQueue> infoQueue;
		m_device->QueryInterface(infoQueue.writeRef());
		if (infoQueue)
		{
			// Enable if you want the app to break on dx12 errors
#if 0
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
#endif

#if 0
			D3D12_INFO_QUEUE_FILTER filter = {};
			D3D12_MESSAGE_ID ids[] =
			{
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_DEPTHSTENCILVIEW_NOT_SET, // Benign.
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET, // Benign.
				D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_TYPE_MISMATCH, // Benign.
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, // Benign.
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, //
				D3D12_MESSAGE_ID_UNSTABLE_POWER_STATE,
			};
			filter.DenyList.NumIDs = NV_COUNT_OF(ids);
			filter.DenyList.pIDList = ids;
			infoQueue->PushStorageFilter(&filter);
#endif
			//infoQueue->SetMuteDebugOutput(true);

		}
	}

	// Looks up multi-sampling possibilities
	if (isMultiSampled())
	{
		// Lets have a look at the multi-sampling options
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
		qualityLevels.Format = m_targetInfo.m_renderTargetFormats[0];
		qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		qualityLevels.NumQualityLevels = 0;
		qualityLevels.SampleCount = m_options.m_numMsaaSamples;

		m_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualityLevels, sizeof(qualityLevels));

		if (qualityLevels.NumQualityLevels > 0)
		{
			m_options.m_msaaQuality = _clamp(m_options.m_msaaQuality, 0, qualityLevels.NumQualityLevels - 1);
		}
		else
		{
			// Ok can't do multi sampling
			m_options.m_numMsaaSamples = 1;
			m_options.m_msaaQuality = 0;
			NV_CORE_ASSERT(!isMultiSampled());
		}
	}

	// Set the same on the target info
	m_targetInfo.m_numSamples = m_options.m_numMsaaSamples;
	m_targetInfo.m_sampleQuality = m_options.m_msaaQuality;

	// If it's multi-sampled update the flags, and the associated formats
	if (isMultiSampled())
	{
		m_depthStencilUsageFlags |= DxFormatUtil::USAGE_FLAG_MULTI_SAMPLE;
		m_targetUsageFlags |= DxFormatUtil::USAGE_FLAG_MULTI_SAMPLE;
	}
	
	// It's not possible to setup multisampling directly on the swap chain. 
	// Multi sample buffers should be created, and then resampled to the swapchain backbuffer
	// https://msdn.microsoft.com/en-us/library/windows/apps/dn458384.aspx

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	NV_RETURN_ON_FAIL(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.writeRef())));

	// Describe the swap chain.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = m_numRenderTargets;
	swapChainDesc.BufferDesc.Width = m_width;
	swapChainDesc.BufferDesc.Height = m_height;
	swapChainDesc.BufferDesc.Format = m_targetInfo.m_renderTargetFormats[0];
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = m_hwnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	Bool hasVsync = true;
	
	if (m_options.m_fullSpeed)
	{
		hasVsync = false;
		m_options.m_allowFullScreen = false;
	}
	
	if (!hasVsync)
	{
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
	}

	// Swap chain needs the queue so that it can force a flush on it.
	ComPtr<IDXGISwapChain> swapChain;
	NV_RETURN_ON_FAIL(factory->CreateSwapChain(m_commandQueue, &swapChainDesc, swapChain.writeRef()));
	NV_RETURN_ON_FAIL(swapChain->QueryInterface(m_swapChain.writeRef()));

	if (!hasVsync)
	{
		m_swapChainWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
		m_swapChain->SetMaximumFrameLatency(m_numRenderTargets - 2);
	}

	// This sample does not support fullscreen transitions.
	NV_RETURN_ON_FAIL(factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER));

	m_renderTargetIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};

		rtvHeapDesc.NumDescriptors = m_numRenderTargets;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		NV_RETURN_ON_FAIL(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.writeRef())));
		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	{
		// Describe and create a depth stencil view (DSV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		NV_RETURN_ON_FAIL(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.writeRef())));

		m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	// Setup frame resources
	{
		NV_RETURN_ON_FAIL(createFrameResources());
	}

	// Setup fence, and close the command list (as default state without begin/endRender is closed)
	{
		NV_RETURN_ON_FAIL(m_fence.init(m_device));
		// Create the command list. When command lists are created they are open, so close it.
		FrameInfo& frame = m_frameInfos[m_frameIndex];
		NV_RETURN_ON_FAIL(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.m_commandAllocator, NV_NULL, IID_PPV_ARGS(m_commandList.writeRef())));
		m_commandList->Close();
	}

	NV_CORE_ASSERT(m_commandListOpenCount == 0);

	return NV_OK;
}

void Dx12RenderContext::executeAndWait(ID3D12GraphicsCommandList* commandList)
{
	//FrameInfo& frame = m_frameInfos[m_frameIndex];

	// Close the command list so it can be executed.
	NV_RETURN_VOID_ON_FAIL(commandList->Close());
	
	// Submit it
	ID3D12CommandList* commandLists[] = { commandList };
	m_commandQueue->ExecuteCommandLists(NV_COUNT_OF(commandLists), commandLists);

	if (m_listener)
	{
		m_listener->onGpuWorkSubmitted(Dx12Type::wrap(m_commandQueue));
	}

	// Wait for the command list to execute; we are reusing the same command 
	// list in our main loop but for now, we just want to wait for setup to 
	// complete before continuing.
	waitForGpu();
}

void Dx12RenderContext::prepareRenderTarget()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_renderTargetIndex, m_rtvDescriptorSize);
	if (m_depthStencil)
	{
		m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_depthStencilView);
	}
	else
	{
		m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, NV_NULL);
	}

	// Set necessary state.
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
}

void Dx12RenderContext::clearRenderTarget(const AlignedVec4* clearColorRgba)
{
	const Float32* clearColor = clearColorRgba ? &clearColorRgba->x : &m_clearColor.x;
	// Record commands
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_renderTargetIndex, m_rtvDescriptorSize);
	m_commandList->ClearRenderTargetView(rtvHandle, (const Float32*)clearColor, 0, NV_NULL);
	if (m_depthStencil)
	{
		m_commandList->ClearDepthStencilView(m_depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NV_NULL);
	}
}

void Dx12RenderContext::beginRender()
{
	// Should currently not be open!
	NV_CORE_ASSERT(m_commandListOpenCount == 0);

	getFrame().m_commandAllocator->Reset();
	beginGpuWork();

	// Indicate that the render target needs to be writable
	{
		Dx12BarrierSubmitter submitter(m_commandList);
		m_renderTargets[m_renderTargetIndex]->transition(D3D12_RESOURCE_STATE_RENDER_TARGET, submitter);	
	}
}

void Dx12RenderContext::endRender()
{
	NV_CORE_ASSERT(m_commandListOpenCount == 1);

	Dx12Resource& backBuffer = *m_backBuffers[m_renderTargetIndex];
	if (isMultiSampled())
	{
		// MSAA resolve	
		Dx12Resource& renderTarget = *m_renderTargets[m_renderTargetIndex];
		NV_CORE_ASSERT(&renderTarget != &backBuffer);
		// Barriers to wait for the render target, and the backbuffer to be in correct state
		{
			Dx12BarrierSubmitter submitter(m_commandList);
			renderTarget.transition(D3D12_RESOURCE_STATE_RESOLVE_SOURCE, submitter);
			backBuffer.transition(D3D12_RESOURCE_STATE_RESOLVE_DEST, submitter);
		}
		// Do the resolve...
		m_commandList->ResolveSubresource(backBuffer, 0, renderTarget, 0, m_targetInfo.m_renderTargetFormats[0]);
	}

	// Make the back buffer presentable
	{
		Dx12BarrierSubmitter submitter(m_commandList);
		backBuffer.transition(D3D12_RESOURCE_STATE_PRESENT, submitter);
	}

	NV_CORE_ASSERT_VOID_ON_FAIL(m_commandList->Close());

	{
		// Execute the command list.
		ID3D12CommandList* commandLists[] = { m_commandList };
		m_commandQueue->ExecuteCommandLists(NV_COUNT_OF(commandLists), commandLists);
	}

	if (m_listener)
	{
		m_listener->onGpuWorkSubmitted(Dx12Type::wrap(m_commandQueue));
	}

	NV_CORE_ASSERT(m_commandListOpenCount == 1);
	// Must be 0
	m_commandListOpenCount = 0;
}

void Dx12RenderContext::beginGpuWork()
{
	if (m_commandListOpenCount == 0)
	{
		// It's not open so open it
		const FrameInfo& frame = getFrame();
		ID3D12GraphicsCommandList* commandList = getCommandList();
		commandList->Reset(frame.m_commandAllocator, NV_NULL);
	}
	m_commandListOpenCount++;
}

void Dx12RenderContext::endGpuWork()
{
	NV_CORE_ASSERT(m_commandListOpenCount);
	ID3D12GraphicsCommandList* commandList = getCommandList();

	NV_CORE_ASSERT_VOID_ON_FAIL(commandList->Close());
	{
		// Execute the command list.
		ID3D12CommandList* commandLists[] = { commandList };
		m_commandQueue->ExecuteCommandLists(NV_COUNT_OF(commandLists), commandLists);
	}
	if (m_listener)
	{
		m_listener->onGpuWorkSubmitted(Dx12Type::wrap(m_commandQueue));
	}
	// Closes and wait
	waitForGpu();

	// Dec the count. If >0 it needs to still be open
	--m_commandListOpenCount;

	// Reopen if needs to be open
	if (m_commandListOpenCount)
	{
		// Reopen
		commandList->Reset(getFrame().m_commandAllocator, NV_NULL);
	}
}

void Dx12RenderContext::submitGpuWork() 
{
	NV_CORE_ASSERT(m_commandListOpenCount);
	ID3D12GraphicsCommandList* commandList = getCommandList();

	NV_CORE_ASSERT_VOID_ON_FAIL(commandList->Close());
	{
		// Execute the command list.
		ID3D12CommandList* commandLists[] = { commandList };
		m_commandQueue->ExecuteCommandLists(NV_COUNT_OF(commandLists), commandLists);
	}
	if (m_listener)
	{
		m_listener->onGpuWorkSubmitted(Dx12Type::wrap(m_commandQueue));
	}
	// Reopen
	commandList->Reset(getFrame().m_commandAllocator, NV_NULL);
}

void Dx12RenderContext::waitForGpu()
{
	m_fence.nextSignalAndWait(m_commandQueue);
}

void Dx12RenderContext::present()
{
	NV_CORE_ASSERT(m_commandListOpenCount == 0);

	if (m_swapChainWaitableObject)
	{
		// check if now is good time to present
		// This doesn't wait - because the wait time is 0. If it returns WAIT_TIMEOUT it means that no frame is waiting to be be displayed
		// so there is no point doing a present.
		const Bool shouldPresent = (WaitForSingleObjectEx(m_swapChainWaitableObject, 0, TRUE) != WAIT_TIMEOUT);
		if (shouldPresent)
		{
			m_swapChain->Present(0, 0);
		}
	}
	else
	{
		NV_CORE_ASSERT_VOID_ON_FAIL(m_swapChain->Present(1, 0));
	}

	// Increment the fence value. Save on the frame - we'll know that frame is done when the fence value >= 
	m_frameInfos[m_frameIndex].m_fenceValue = m_fence.nextSignal(m_commandQueue);

	// increment frame index after signal
	m_frameIndex = (m_frameIndex + 1) % m_numRenderFrames;
	// Update the render target index.
	m_renderTargetIndex = m_swapChain->GetCurrentBackBufferIndex();

	// On the current frame wait until it is completed 
	{
		FrameInfo& frame = m_frameInfos[m_frameIndex];
		// If the next frame is not ready to be rendered yet, wait until it is ready.
		m_fence.waitUntilCompleted(frame.m_fenceValue);
	}
	// We will just reset to 0 -> as we don't want to halt on complete
	//m_commandListOpenCount = 0;
}

void Dx12RenderContext::onSizeChanged(Int width, Int height, Bool minimized)
{
	// Determine if the swap buffers and other resources need to be resized or not.
	if ((width != m_width || height != m_height) && !minimized)
	{
		// Flush all current GPU commands.
		waitForGpu();

  		releaseFrameResources();

		// Resize the swap chain to the desired dimensions.
		DXGI_SWAP_CHAIN_DESC desc = {};
		m_swapChain->GetDesc(&desc);
		NV_CORE_ASSERT_VOID_ON_FAIL(m_swapChain->ResizeBuffers(m_numRenderTargets, width, height, desc.BufferDesc.Format, desc.Flags));

		// Reset the frame index to the current back buffer index.
		m_renderTargetIndex = m_swapChain->GetCurrentBackBufferIndex();

		// Update the width, height, and aspect ratio member variables.
		updateForSizeChange(width, height);

		// Create frame resources.
		createFrameResources();

		m_resizeResources = true;
	}
}

void Dx12RenderContext::releaseFrameResources()
{
	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb174577%28v=vs.85%29.aspx

	// Release the resources holding references to the swap chain (requirement of
	// IDXGISwapChain::ResizeBuffers) and reset the frame fence values to the
	// current fence value.
	for (IndexT i = 0; i < m_numRenderFrames; i++)
	{
		FrameInfo& info = m_frameInfos[i];
		info.reset();
		info.m_fenceValue = m_fence.getCurrentValue();
	}
	for (IndexT i = 0; i < m_numRenderTargets; i++)
	{
		m_backBuffers[i]->setResourceNull();
		m_renderTargets[i]->setResourceNull();
	}
}

Result Dx12RenderContext::createFrameResources()
{
	// Create back buffers
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvStart(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV, and a command allocator for each frame.
		for (IndexT i = 0; i < m_numRenderTargets; i++)
		{
			// Get the back buffer
			ComPtr<ID3D12Resource> backBuffer;
			NV_RETURN_ON_FAIL(m_swapChain->GetBuffer(UINT(i), IID_PPV_ARGS(backBuffer.writeRef())));
			
			// Set up resource for back buffer
			m_backBufferResources[i].setResource(backBuffer, D3D12_RESOURCE_STATE_COMMON);
			m_backBuffers[i] = &m_backBufferResources[i];
			// Assume they are the same thing for now...
			m_renderTargets[i] = &m_backBufferResources[i];

			// If we are multi-sampling - create a render target separate from the back buffer
			if (isMultiSampled())
			{
				CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
				D3D12_RESOURCE_DESC desc = backBuffer->GetDesc();
			
				DXGI_FORMAT resourceFormat = DxFormatUtil::calcResourceFormat(DxFormatUtil::USAGE_TARGET, m_targetUsageFlags, desc.Format);
				DXGI_FORMAT targetFormat = DxFormatUtil::calcFormat(DxFormatUtil::USAGE_TARGET, resourceFormat);

				// Set the target format
				m_targetInfo.m_renderTargetFormats[0] = targetFormat;

				D3D12_CLEAR_VALUE clearValue = {};
				clearValue.Format = targetFormat;
				
				// Don't know targets alignment, so just memory copy
				Memory::copy(clearValue.Color, &getClearColor(), sizeof(AlignedVec4));
				
				desc.Format = resourceFormat;
				desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				desc.SampleDesc.Count = m_targetInfo.m_numSamples;
				desc.SampleDesc.Quality = m_targetInfo.m_sampleQuality;
				desc.Alignment = 0;

				NV_RETURN_ON_FAIL(m_renderTargetResources[i].initCommitted(m_device, heapProps, D3D12_HEAP_FLAG_NONE, desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue));
				m_renderTargets[i] = &m_renderTargetResources[i];
			}

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvStart, UINT(i), m_rtvDescriptorSize);
			m_device->CreateRenderTargetView(*m_renderTargets[i], NV_NULL, rtvHandle);
		}
	}

	// Set up frames
	for (IndexT i = 0; i < m_numRenderFrames; i++)
	{
		FrameInfo& frame = m_frameInfos[i];
		NV_RETURN_ON_FAIL(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frame.m_commandAllocator.writeRef())));
	}

	{
		D3D12_RESOURCE_DESC desc = m_backBuffers[0]->getResource()->GetDesc();
		NV_CORE_ASSERT(desc.Width == UINT64(m_width) && desc.Height == UINT64(m_height));
	}

	// Create the depth stencil view.
	{		
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

		DXGI_FORMAT resourceFormat = DxFormatUtil::calcResourceFormat(DxFormatUtil::USAGE_DEPTH_STENCIL, m_depthStencilUsageFlags, m_targetInfo.m_depthStencilFormat);
		DXGI_FORMAT depthStencilFormat = DxFormatUtil::calcFormat(DxFormatUtil::USAGE_DEPTH_STENCIL, resourceFormat);

		// Set the depth stencil format
		m_targetInfo.m_depthStencilFormat = depthStencilFormat;

		// Setup default clear
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = depthStencilFormat;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		CD3DX12_RESOURCE_DESC resourceDesc(CD3DX12_RESOURCE_DESC::Tex2D(resourceFormat, m_width, m_height, 1, 1, m_targetInfo.m_numSamples, m_targetInfo.m_sampleQuality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));

		NV_RETURN_ON_FAIL(m_depthStencil.initCommitted(m_device, heapProps, D3D12_HEAP_FLAG_NONE, resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue));
		
		// Set the depth stencil
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format = depthStencilFormat;
		depthStencilDesc.ViewDimension = isMultiSampled() ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

		// Set up as the depth stencil view
		m_device->CreateDepthStencilView(m_depthStencil, &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		m_depthStencilView = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	m_viewport.Width = static_cast<float>(m_width);
	m_viewport.Height = static_cast<float>(m_height);
	m_viewport.MaxDepth = 1.0f;

	m_scissorRect.right = static_cast<LONG>(m_width);
	m_scissorRect.bottom = static_cast<LONG>(m_height);

	return NV_OK;
}

Result Dx12RenderContext::toggleFullScreen()
{
	BOOL fullScreenState;
	NV_RETURN_ON_FAIL(m_swapChain->GetFullscreenState(&fullScreenState, NV_NULL));
	if (NV_FAILED(m_swapChain->SetFullscreenState(!fullScreenState, NV_NULL)))
	{
		// Transitions to fullscreen mode can fail when running apps over
		// terminal services or for some other unexpected reason.  Consider
		// notifying the user in some way when this happens.
		NV_CO_LOG_ERROR("Fullscreen transition failed");
		assert(false);
	}
	return NV_OK;
}

Bool Dx12RenderContext::isFullScreen()
{
	BOOL fullScreenState = false;
	m_swapChain->GetFullscreenState(&fullScreenState, NV_NULL);
	return fullScreenState != 0;
}

/* static */Result Dx12RenderContext::findHardwareAdapter(IDXGIFactory2* factory, ComPtr<IDXGIAdapter1>& adapterOut)
{
	ComPtr<IDXGIAdapter1> adapter;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, adapter.writeRef()); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (NV_SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), NV_NULL)))
		{
			adapterOut.swap(adapter);
			return NV_OK;
		}
	}

	adapterOut.setNull();
	return NV_FAIL;
}

} // namespace Common 
} // namespace nvidia
