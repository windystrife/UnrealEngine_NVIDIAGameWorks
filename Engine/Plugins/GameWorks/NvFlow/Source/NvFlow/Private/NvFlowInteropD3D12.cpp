#include "NvFlowCommon.h"

#include "RHI.h"
#include "NvFlowInterop.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#include <d3d12.h>
#include "HideWindowsPlatformTypes.h"

#include "GameWorks/RHINvFlowD3D12.h"

#include "NvFlowContextD3D12.h"

class NvFlowInteropD3D12 : public NvFlowInterop
{
public:
	static NvFlowDescriptorReserveHandleD3D12 ReserveDescriptors(void* userdata, NvFlowUint numDescriptors, NvFlowUint64 lastFenceCompleted, NvFlowUint64 nextFenceValue)
	{
		auto CmdCtx = static_cast<IRHICommandContext*>(userdata);
		FRHINvFlowDescriptorReserveHandleD3D12 handle = {};
		CmdCtx->NvFlowReserveDescriptors(&handle, numDescriptors, lastFenceCompleted, nextFenceValue);
		NvFlowDescriptorReserveHandleD3D12 dstHandle = {};
		dstHandle.heap = handle.heap;
		dstHandle.descriptorSize = handle.descriptorSize;
		dstHandle.cpuHandle = handle.cpuHandle;
		dstHandle.gpuHandle = handle.gpuHandle;
		return dstHandle;
	}

	void updateContextDesc(IRHICommandContext& RHICmdCtx, NvFlowContextDescD3D12& desc, FRHINvFlowDeviceDescD3D12& deviceDesc)
	{
		desc.device = deviceDesc.device;
		desc.commandQueue = deviceDesc.commandQueue;
		desc.commandQueueFence = deviceDesc.commandQueueFence;
		desc.commandList = deviceDesc.commandList;
		desc.lastFenceCompleted = deviceDesc.lastFenceCompleted;
		desc.nextFenceValue = deviceDesc.nextFenceValue;

		desc.dynamicHeapCbvSrvUav.userdata = &RHICmdCtx;
		desc.dynamicHeapCbvSrvUav.reserveDescriptors = ReserveDescriptors;
	}

	void updateDepthStencilViewDesc(NvFlowDepthStencilViewDescD3D12& desc, FRHINvFlowDepthStencilViewDescD3D12& dsvDesc)
	{
		desc.dsvHandle = dsvDesc.dsvHandle;
		desc.dsvDesc = dsvDesc.dsvDesc;
		desc.dsvResource = dsvDesc.dsvResource;
		desc.dsvCurrentState = dsvDesc.dsvCurrentState;
		desc.srvHandle = dsvDesc.srvHandle;
		desc.srvDesc = dsvDesc.srvDesc;
		desc.srvResource = dsvDesc.srvResource;
		desc.srvCurrentState = dsvDesc.srvCurrentState;
		desc.viewport = dsvDesc.viewport;
	}

	void updateRenderTargetViewDesc(NvFlowRenderTargetViewDescD3D12& desc, FRHINvFlowRenderTargetViewDescD3D12& rtvDesc)
	{
		desc.rtvHandle = rtvDesc.rtvHandle;
		desc.rtvDesc = rtvDesc.rtvDesc;
		desc.resource = rtvDesc.resource;
		desc.currentState = rtvDesc.currentState;
		desc.viewport = rtvDesc.viewport;
		desc.scissor = rtvDesc.scissor;
	}

	virtual NvFlowContext* CreateContext(IRHICommandContext& RHICmdCtx)
	{
		FRHINvFlowDeviceDescD3D12 deviceDesc = {};
		RHICmdCtx.NvFlowGetDeviceDesc(&deviceDesc);
		NvFlowContextDescD3D12 desc = {};
		updateContextDesc(RHICmdCtx, desc, deviceDesc);
		return NvFlowCreateContextD3D12(NV_FLOW_VERSION, &desc);
	}

	virtual NvFlowDepthStencilView* CreateDepthStencilView(IRHICommandContext& RHICmdCtx, FTexture2DRHIParamRef depthSurface, FTexture2DRHIParamRef depthTexture, NvFlowContext* context)
	{
		FRHINvFlowDepthStencilViewDescD3D12 dsvDesc = {};
		RHICmdCtx.NvFlowGetDepthStencilViewDesc(depthSurface, depthTexture, &dsvDesc);
		NvFlowDepthStencilViewDescD3D12 desc = {};
		updateDepthStencilViewDesc(desc, dsvDesc);
		return NvFlowCreateDepthStencilViewD3D12(context, &desc);
	}

	virtual NvFlowRenderTargetView* CreateRenderTargetView(IRHICommandContext& RHICmdCtx, NvFlowContext* context)
	{
		FRHINvFlowRenderTargetViewDescD3D12 rtvDesc = {};
		RHICmdCtx.NvFlowGetRenderTargetViewDesc(&rtvDesc);
		NvFlowRenderTargetViewDescD3D12 desc = {};
		updateRenderTargetViewDesc(desc, rtvDesc);
		return NvFlowCreateRenderTargetViewD3D12(context, &desc);
	}

	virtual void UpdateContext(IRHICommandContext& RHICmdCtx, NvFlowContext* context)
	{
		FRHINvFlowDeviceDescD3D12 deviceDesc = {};
		RHICmdCtx.NvFlowGetDeviceDesc(&deviceDesc);
		NvFlowContextDescD3D12 desc = {};
		updateContextDesc(RHICmdCtx, desc, deviceDesc);
		NvFlowUpdateContextD3D12(context, &desc);
	}

	virtual void UpdateDepthStencilView(IRHICommandContext& RHICmdCtx, FTexture2DRHIParamRef depthSurface, FTexture2DRHIParamRef depthTexture, NvFlowContext* context, NvFlowDepthStencilView* view)
	{
		FRHINvFlowDepthStencilViewDescD3D12 dsvDesc = {};
		RHICmdCtx.NvFlowGetDepthStencilViewDesc(depthSurface, depthTexture, &dsvDesc);
		NvFlowDepthStencilViewDescD3D12 desc = {};
		updateDepthStencilViewDesc(desc, dsvDesc);
		NvFlowUpdateDepthStencilViewD3D12(context, view, &desc);
	}

	virtual void UpdateRenderTargetView(IRHICommandContext& RHICmdCtx, NvFlowContext* context, NvFlowRenderTargetView* view)
	{
		FRHINvFlowRenderTargetViewDescD3D12 rtvDesc = {};
		RHICmdCtx.NvFlowGetRenderTargetViewDesc(&rtvDesc);
		NvFlowRenderTargetViewDescD3D12 desc = {};
		updateRenderTargetViewDesc(desc, rtvDesc);
		NvFlowUpdateRenderTargetViewD3D12(context, view, &desc);
	}

	virtual void Push(IRHICommandContext& RHICmdCtx, NvFlowContext* context)
	{
		NvFlowContextPush(context);
	}

	virtual void Pop(IRHICommandContext& RHICmdCtx, NvFlowContext* context)
	{
		NvFlowContextPop(context);

		RHICmdCtx.NvFlowRestoreState();
	}

	virtual void CleanupFunc(IRHICommandContext& RHICmdCtx, void(*func)(void*), void* ptr)
	{
		RHICmdCtx.NvFlowCleanup.Set(func, ptr);
	}

	virtual FShaderResourceViewRHIRef CreateSRV(IRHICommandContext& RHICmdCtx, NvFlowContext* context, NvFlowResource* resource)
	{
		if (resource)
		{
			NvFlowResourceViewDescD3D12 viewDesc;
			NvFlowUpdateResourceViewDescD3D12(context, resource, &viewDesc);

			FRHINvFlowResourceViewDescD3D12 viewDescRHI;
			viewDescRHI.srvHandle = viewDesc.srvHandle;
			viewDescRHI.srvDesc = viewDesc.srvDesc;
			viewDescRHI.resource = viewDesc.resource;
			viewDescRHI.currentState = viewDesc.currentState;
			return RHICmdCtx.NvFlowCreateSRV(&viewDescRHI);
		}
		return FShaderResourceViewRHIRef();
	}

	virtual FRHINvFlowResourceRW* CreateResourceRW(IRHICommandContext& RHICmdCtx, NvFlowContext* context, NvFlowResourceRW* resourceRW, FShaderResourceViewRHIRef* pRHIRefSRV, FUnorderedAccessViewRHIRef* pRHIRefUAV)
	{
		if (resourceRW)
		{
			NvFlowResourceRWViewDescD3D12 viewDesc;
			NvFlowUpdateResourceRWViewDescD3D12(context, resourceRW, &viewDesc);

			FRHINvFlowResourceRWViewDescD3D12 viewDescRHI;
			viewDescRHI.resourceView.srvHandle = viewDesc.resourceView.srvHandle;
			viewDescRHI.resourceView.srvDesc = viewDesc.resourceView.srvDesc;
			viewDescRHI.resourceView.resource = viewDesc.resourceView.resource;
			viewDescRHI.resourceView.currentState = viewDesc.resourceView.currentState;
			viewDescRHI.uavHandle = viewDesc.uavHandle;
			viewDescRHI.uavDesc = viewDesc.uavDesc;
			return RHICmdCtx.NvFlowCreateResourceRW(&viewDescRHI, pRHIRefSRV, pRHIRefUAV);
		}
		return nullptr;
	}

	virtual void ReleaseResourceRW(IRHICommandContext& RHICmdCtx, FRHINvFlowResourceRW* pRHIResourceRW)
	{
		if (pRHIResourceRW)
		{
			RHICmdCtx.NvFlowReleaseResourceRW(pRHIResourceRW);
		}
	}

};

NvFlowInterop* NvFlowCreateInteropD3D12()
{
	return new NvFlowInteropD3D12();
}

#endif
