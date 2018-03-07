#pragma once

// NvFlow begin
struct FRHINvFlowDescriptorReserveHandleD3D12 : FRHINvFlowDescriptorReserveHandle
{
	ID3D12DescriptorHeap* heap;
	uint32 descriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
};

struct FRHINvFlowDeviceDescD3D12 : FRHINvFlowDeviceDesc
{
	ID3D12Device* device;						//!< The desired d3d12 device to use
	ID3D12CommandQueue* commandQueue;			//!< The commandQueue commandList will be submit on
	ID3D12Fence* commandQueueFence;				//!< Fence marking events on this queue
	ID3D12GraphicsCommandList* commandList;		//!< The commandlist for recording
	UINT64 lastFenceCompleted;					//!< The last fence completed on commandQueue
	UINT64 nextFenceValue;						//!< The fence value signaled after commandList is submitted
};

struct FRHINvFlowDepthStencilViewDescD3D12 : FRHINvFlowDepthStencilViewDesc
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	ID3D12Resource* dsvResource;
	D3D12_RESOURCE_STATES dsvCurrentState;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ID3D12Resource* srvResource;
	D3D12_RESOURCE_STATES srvCurrentState;
	D3D12_VIEWPORT viewport;
};

struct FRHINvFlowRenderTargetViewDescD3D12 : FRHINvFlowRenderTargetViewDesc
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	ID3D12Resource* resource;
	D3D12_RESOURCE_STATES currentState;
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissor;
};

struct FRHINvFlowResourceViewDescD3D12 : FRHINvFlowResourceViewDesc
{
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ID3D12Resource* resource;
	D3D12_RESOURCE_STATES* currentState;
};

struct FRHINvFlowResourceRWViewDescD3D12 : FRHINvFlowResourceRWViewDesc
{
	FRHINvFlowResourceViewDescD3D12 resourceView;
	D3D12_CPU_DESCRIPTOR_HANDLE uavHandle;
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
};
// NvFlow end