/*
* Copyright (c) 2014-2017, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "NvFlowContext.h"

//! GPU Graphics and compute interface
struct NvFlowContext;

struct NvFlowConstantBuffer;
struct NvFlowVertexBuffer;
struct NvFlowIndexBuffer;
struct NvFlowResource;
struct NvFlowResourceRW;
struct NvFlowBuffer;
struct NvFlowTexture1D;
struct NvFlowTexture2D;
struct NvFlowTexture3D;

//! Interface to manage resource reference count and lifetime
struct NvFlowContextObject;

NV_FLOW_API NvFlowUint NvFlowContextObjectAddRef(NvFlowContextObject* object);

NV_FLOW_API NvFlowUint NvFlowContextObjectRelease(NvFlowContextObject* object);

NV_FLOW_API NvFlowUint64 NvFlowContextObjectGetGPUBytesUsed(NvFlowContextObject* object);

//! Handle for mapped pitched data
struct NvFlowMappedData
{
	void* data;
	NvFlowUint rowPitch;
	NvFlowUint depthPitch;
};

//! A constant buffer
struct NvFlowConstantBuffer;

struct NvFlowConstantBufferDesc
{
	NvFlowUint sizeInBytes;
	bool uploadAccess;
};

NV_FLOW_API void NvFlowConstantBufferGetDesc(NvFlowConstantBuffer* buffer, NvFlowConstantBufferDesc* desc);

NV_FLOW_API NvFlowConstantBuffer* NvFlowCreateConstantBuffer(NvFlowContext* context, const NvFlowConstantBufferDesc* desc);

NV_FLOW_API void NvFlowReleaseConstantBuffer(NvFlowConstantBuffer* buffer);

NV_FLOW_API NvFlowContextObject* NvFlowConstantBufferGetContextObject(NvFlowConstantBuffer* buffer);

NV_FLOW_API void* NvFlowConstantBufferMap(NvFlowContext* context, NvFlowConstantBuffer* constantBuffer);

NV_FLOW_API void NvFlowConstantBufferUnmap(NvFlowContext* context, NvFlowConstantBuffer* constantBuffer);

//! A vertex buffer
struct NvFlowVertexBuffer;

struct NvFlowVertexBufferDesc
{
	const void* data;
	NvFlowUint sizeInBytes;
};

NV_FLOW_API void NvFlowVertexBufferGetDesc(NvFlowVertexBuffer* buffer, NvFlowVertexBufferDesc* desc);

NV_FLOW_API NvFlowVertexBuffer* NvFlowCreateVertexBuffer(NvFlowContext* context, const NvFlowVertexBufferDesc* desc);

NV_FLOW_API void NvFlowReleaseVertexBuffer(NvFlowVertexBuffer* vertexBuffer);

NV_FLOW_API NvFlowContextObject* NvFlowVertexBufferGetContextObject(NvFlowVertexBuffer* buffer);

NV_FLOW_API void* NvFlowVertexBufferMap(NvFlowContext* context, NvFlowVertexBuffer* vertexBuffer);

NV_FLOW_API void NvFlowVertexBufferUnmap(NvFlowContext* context, NvFlowVertexBuffer* vertexBuffer);

//! An index buffer
struct NvFlowIndexBuffer;

struct NvFlowIndexBufferDesc
{
	const void* data;
	NvFlowUint sizeInBytes;
	NvFlowFormat format;
};

NV_FLOW_API void NvFlowIndexBufferGetDesc(NvFlowIndexBuffer* index, NvFlowIndexBufferDesc* desc);

NV_FLOW_API NvFlowIndexBuffer* NvFlowCreateIndexBuffer(NvFlowContext* context, const NvFlowIndexBufferDesc* desc);

NV_FLOW_API void NvFlowReleaseIndexBuffer(NvFlowIndexBuffer* buffer);

NV_FLOW_API NvFlowContextObject* NvFlowIndexBufferGetContextObject(NvFlowIndexBuffer* buffer);

NV_FLOW_API void* NvFlowIndexBufferMap(NvFlowContext* context, NvFlowIndexBuffer* indexBuffer);

NV_FLOW_API void NvFlowIndexBufferUnmap(NvFlowContext* context, NvFlowIndexBuffer* indexBuffer);

//! A read only resource interface
struct NvFlowResource;

NV_FLOW_API NvFlowContextObject* NvFlowResourceGetContextObject(NvFlowResource* resource);

//! A read/write resource interface
struct NvFlowResourceRW;

NV_FLOW_API NvFlowContextObject* NvFlowResourceRWGetContextObject(NvFlowResourceRW* resourceRW);

NV_FLOW_API NvFlowResource* NvFlowResourceRWGetResource(NvFlowResourceRW* resourceRW);

//! A render target interface
struct NvFlowRenderTarget;

//! Viewport description for rendering
struct NvFlowViewport
{
	float topLeftX;
	float topLeftY;
	float width;
	float height;
	float minDepth;
	float maxDepth;
};

struct NvFlowRenderTargetDesc
{
	NvFlowViewport viewport;
	NvFlowFormat rt_format;
};

NV_FLOW_API void NvFlowRenderTargetGetDesc(NvFlowRenderTarget* rt, NvFlowRenderTargetDesc* desc);

NV_FLOW_API void NvFlowRenderTargetSetViewport(NvFlowRenderTarget* rt, const NvFlowViewport* viewport);

//! A depth stencil inteface
struct NvFlowDepthStencil;

struct NvFlowDepthStencilDesc
{
	NvFlowFormat ds_format;
	NvFlowViewport viewport;
	NvFlowUint width;
	NvFlowUint height;
};

NV_FLOW_API void NvFlowDepthStencilGetDesc(NvFlowDepthStencil* ds, NvFlowDepthStencilDesc* desc);

NV_FLOW_API void NvFlowDepthStencilSetViewport(NvFlowDepthStencil* ds, const NvFlowViewport* viewport);

//! A buffer
struct NvFlowBuffer;

struct NvFlowBufferDesc
{
	NvFlowFormat format;
	NvFlowUint dim;
	bool uploadAccess;
	bool downloadAccess;
};

struct NvFlowBufferViewDesc
{
	NvFlowFormat format;
};

NV_FLOW_API void NvFlowBufferGetDesc(NvFlowBuffer* buffer, NvFlowBufferDesc* desc);

NV_FLOW_API NvFlowBuffer* NvFlowCreateBuffer(NvFlowContext* context, const NvFlowBufferDesc* desc);

NV_FLOW_API NvFlowBuffer* NvFlowCreateBufferView(NvFlowContext* context, NvFlowBuffer* buffer, const NvFlowBufferViewDesc* desc);

NV_FLOW_API void NvFlowReleaseBuffer(NvFlowBuffer* buffer);

NV_FLOW_API NvFlowContextObject* NvFlowBufferGetContextObject(NvFlowBuffer* buffer);

NV_FLOW_API NvFlowResource* NvFlowBufferGetResource(NvFlowBuffer* buffer);

NV_FLOW_API NvFlowResourceRW* NvFlowBufferGetResourceRW(NvFlowBuffer* buffer);

NV_FLOW_API void* NvFlowBufferMap(NvFlowContext* context, NvFlowBuffer* buffer);

NV_FLOW_API void NvFlowBufferUnmap(NvFlowContext* context, NvFlowBuffer* buffer);

NV_FLOW_API void NvFlowBufferUnmapRange(NvFlowContext* context, NvFlowBuffer* buffer, NvFlowUint offset, NvFlowUint numBytes);

NV_FLOW_API void NvFlowBufferDownload(NvFlowContext* context, NvFlowBuffer* buffer);

NV_FLOW_API void NvFlowBufferDownloadRange(NvFlowContext* context, NvFlowBuffer* buffer, NvFlowUint offset, NvFlowUint numBytes);

NV_FLOW_API void* NvFlowBufferMapDownload(NvFlowContext* context, NvFlowBuffer* buffer);

NV_FLOW_API void NvFlowBufferUnmapDownload(NvFlowContext* context, NvFlowBuffer* buffer);

//! A 1D Texture
struct NvFlowTexture1D;

struct NvFlowTexture1DDesc
{
	NvFlowFormat format;
	NvFlowUint dim;
	bool uploadAccess;
};

NV_FLOW_API void NvFlowTexture1DGetDesc(NvFlowTexture1D* tex, NvFlowTexture1DDesc* desc);

NV_FLOW_API NvFlowTexture1D* NvFlowCreateTexture1D(NvFlowContext* context, const NvFlowTexture1DDesc* desc);

NV_FLOW_API void NvFlowReleaseTexture1D(NvFlowTexture1D* tex);

NV_FLOW_API NvFlowContextObject* NvFlowTexture1DGetContextObject(NvFlowTexture1D* tex);

NV_FLOW_API NvFlowResource* NvFlowTexture1DGetResource(NvFlowTexture1D* tex);

NV_FLOW_API NvFlowResourceRW* NvFlowTexture1DGetResourceRW(NvFlowTexture1D* tex);

NV_FLOW_API void* NvFlowTexture1DMap(NvFlowContext* context, NvFlowTexture1D* tex);

NV_FLOW_API void NvFlowTexture1DUnmap(NvFlowContext* context, NvFlowTexture1D* tex);

//! A 2D Texture
struct NvFlowTexture2D;

struct NvFlowTexture2DDesc
{
	NvFlowFormat format;
	NvFlowUint width;
	NvFlowUint height;
};

NV_FLOW_API void NvFlowTexture2DGetDesc(NvFlowTexture2D* tex, NvFlowTexture2DDesc* desc);

NV_FLOW_API NvFlowTexture2D* NvFlowCreateTexture2D(NvFlowContext* context, const NvFlowTexture2DDesc* desc);

NV_FLOW_API NvFlowTexture2D* NvFlowShareTexture2D(NvFlowContext* context, NvFlowTexture2D* sharedTexture);

NV_FLOW_API NvFlowTexture2D* NvFlowCreateTexture2DCrossAPI(NvFlowContext* context, const NvFlowTexture2DDesc* desc);

NV_FLOW_API NvFlowTexture2D* NvFlowShareTexture2DCrossAPI(NvFlowContext* context, NvFlowTexture2D* sharedTexture);

NV_FLOW_API void NvFlowReleaseTexture2D(NvFlowTexture2D* tex);

NV_FLOW_API NvFlowContextObject* NvFlowTexture2DGetContextObject(NvFlowTexture2D* tex);

NV_FLOW_API NvFlowResource* NvFlowTexture2DGetResource(NvFlowTexture2D* tex);

NV_FLOW_API NvFlowResourceRW* NvFlowTexture2DGetResourceRW(NvFlowTexture2D* tex);

//! A 3D Texture
struct NvFlowTexture3D;

struct NvFlowTexture3DDesc
{
	NvFlowFormat format;
	NvFlowDim dim;
	bool uploadAccess;
	bool downloadAccess;
};

NV_FLOW_API void NvFlowTexture3DGetDesc(NvFlowTexture3D* tex, NvFlowTexture3DDesc* desc);

NV_FLOW_API NvFlowTexture3D* NvFlowCreateTexture3D(NvFlowContext* context, const NvFlowTexture3DDesc* desc);

NV_FLOW_API void NvFlowReleaseTexture3D(NvFlowTexture3D* tex);

NV_FLOW_API NvFlowContextObject* NvFlowTexture3DGetContextObject(NvFlowTexture3D* tex);

NV_FLOW_API NvFlowResource* NvFlowTexture3DGetResource(NvFlowTexture3D* tex);

NV_FLOW_API NvFlowResourceRW* NvFlowTexture3DGetResourceRW(NvFlowTexture3D* tex);

NV_FLOW_API NvFlowMappedData NvFlowTexture3DMap(NvFlowContext* context, NvFlowTexture3D* tex);

NV_FLOW_API void NvFlowTexture3DUnmap(NvFlowContext* context, NvFlowTexture3D* tex);

NV_FLOW_API void NvFlowTexture3DDownload(NvFlowContext* context, NvFlowTexture3D* tex);

NV_FLOW_API NvFlowMappedData NvFlowTexture3DMapDownload(NvFlowContext* context, NvFlowTexture3D* tex);

NV_FLOW_API void NvFlowTexture3DUnmapDownload(NvFlowContext* context, NvFlowTexture3D* tex);

//! A memory heap for 3D Hardware Sparse Texture
struct NvFlowHeapSparse;

struct NvFlowHeapSparseDesc
{
	NvFlowUint sizeInBytes;
};

NV_FLOW_API void NvFlowHeapSparseGetDesc(NvFlowHeapSparse* heap, NvFlowHeapSparseDesc* desc);

NV_FLOW_API NvFlowHeapSparse* NvFlowCreateHeapSparse(NvFlowContext* context, const NvFlowHeapSparseDesc* desc);

NV_FLOW_API void NvFlowReleaseHeapSparse(NvFlowHeapSparse* heap);

NV_FLOW_API NvFlowContextObject* NvFlowHeapSparseGetContextObject(NvFlowHeapSparse* heap);

//! A 3D Hardware Sparse Texture
struct NvFlowTexture3DSparse;

struct NvFlowTexture3DSparseDesc
{
	NvFlowFormat format;
	NvFlowDim dim;
};

NV_FLOW_API void NvFlowTexture3DSparseGetDesc(NvFlowTexture3DSparse* tex, NvFlowTexture3DSparseDesc* desc);

NV_FLOW_API NvFlowTexture3DSparse* NvFlowCreateTexture3DSparse(NvFlowContext* context, const NvFlowTexture3DSparseDesc* desc);

NV_FLOW_API void NvFlowReleaseTexture3DSparse(NvFlowTexture3DSparse* tex);

NV_FLOW_API NvFlowContextObject* NvFlowTexture3DSparseGetContextObject(NvFlowTexture3DSparse* tex);

NV_FLOW_API NvFlowResource* NvFlowTexture3DSparseGetResource(NvFlowTexture3DSparse* tex);

NV_FLOW_API NvFlowResourceRW* NvFlowTexture3DSparseGetResourceRW(NvFlowTexture3DSparse* tex);

//! A 2D texture with render target support
struct NvFlowColorBuffer;

struct NvFlowColorBufferDesc
{
	NvFlowFormat format;
	NvFlowUint width;
	NvFlowUint height;
};

NV_FLOW_API void NvFlowColorBufferGetDesc(NvFlowColorBuffer* tex, NvFlowColorBufferDesc* desc);

NV_FLOW_API NvFlowColorBuffer* NvFlowCreateColorBuffer(NvFlowContext* context, const NvFlowColorBufferDesc* desc);

NV_FLOW_API void NvFlowReleaseColorBuffer(NvFlowColorBuffer* tex);

NV_FLOW_API NvFlowContextObject* NvFlowColorBufferGetContextObject(NvFlowColorBuffer* tex);

NV_FLOW_API NvFlowResource* NvFlowColorBufferGetResource(NvFlowColorBuffer* tex);

NV_FLOW_API NvFlowResourceRW* NvFlowColorBufferGetResourceRW(NvFlowColorBuffer* tex);

NV_FLOW_API NvFlowRenderTarget* NvFlowColorBufferGetRenderTarget(NvFlowColorBuffer* tex);

//! A 2D texture with depth stencil support
struct NvFlowDepthBuffer;

struct NvFlowDepthBufferDesc
{
	NvFlowFormat format_resource;
	NvFlowFormat format_dsv;
	NvFlowFormat format_srv;
	NvFlowUint width;
	NvFlowUint height;
};

NV_FLOW_API void NvFlowDepthBufferGetDesc(NvFlowDepthBuffer* depthBuffer, NvFlowDepthBufferDesc* desc);

NV_FLOW_API NvFlowDepthBuffer* NvFlowCreateDepthBuffer(NvFlowContext* context, const NvFlowDepthBufferDesc* desc);

NV_FLOW_API void NvFlowReleaseDepthBuffer(NvFlowDepthBuffer* depthBuffer);

NV_FLOW_API NvFlowContextObject* NvFlowDepthBufferGetContextObject(NvFlowDepthBuffer* depthBuffer);

NV_FLOW_API NvFlowResource* NvFlowDepthBufferGetResource(NvFlowDepthBuffer* depthBuffer);

NV_FLOW_API NvFlowDepthStencil* NvFlowDepthBufferGetDepthStencil(NvFlowDepthBuffer* depthBuffer);

//! A depth stencil imported from the app
struct NvFlowDepthStencilView;

NV_FLOW_API NvFlowResource* NvFlowDepthStencilViewGetResource(NvFlowDepthStencilView* dsv);

NV_FLOW_API NvFlowDepthStencil* NvFlowDepthStencilViewGetDepthStencil(NvFlowDepthStencilView* dsv);

NV_FLOW_API void NvFlowDepthStencilViewGetDepthBufferDesc(NvFlowDepthStencilView* dsv, NvFlowDepthBufferDesc* desc);

//! A render target imported from the app
struct NvFlowRenderTargetView;

NV_FLOW_API NvFlowRenderTarget* NvFlowRenderTargetViewGetRenderTarget(NvFlowRenderTargetView* rtv);

//! Constants for dispatch and draw commands

#define NV_FLOW_DISPATCH_MAX_READ_TEXTURES ( 16u )
#define NV_FLOW_DISPATCH_MAX_WRITE_TEXTURES ( 8u )

#define NV_FLOW_DRAW_MAX_READ_TEXTURES ( 16u )
#define NV_FLOW_DRAW_MAX_WRITE_TEXTURES ( 1u )
#define NV_FLOW_MAX_RENDER_TARGETS ( 8u )

//! A compute shader
struct NvFlowComputeShader;

struct NvFlowComputeShaderDesc
{
	const void* cs;
	NvFlowUint64 cs_length;
	const wchar_t* label;
};

struct NvFlowDispatchParams
{
	NvFlowComputeShader* shader;
	NvFlowDim gridDim;
	NvFlowConstantBuffer* rootConstantBuffer;
	NvFlowConstantBuffer* secondConstantBuffer;
	NvFlowResource* readOnly[NV_FLOW_DISPATCH_MAX_READ_TEXTURES];
	NvFlowResourceRW* readWrite[NV_FLOW_DISPATCH_MAX_WRITE_TEXTURES];
};

NV_FLOW_API NvFlowComputeShader* NvFlowCreateComputeShader(NvFlowContext* context, const NvFlowComputeShaderDesc* desc);

NV_FLOW_API void NvFlowReleaseComputeShader(NvFlowComputeShader* computeShader);

//! A graphics shader pipeline
struct NvFlowGraphicsShader;

struct NvFlowInputElementDesc
{
	const char* semanticName;
	NvFlowFormat format;
};

enum NvFlowBlendEnum
{
	eNvFlowBlend_Zero = 1,
	eNvFlowBlend_One = 2,
	eNvFlowBlend_SrcAlpha = 3,
	eNvFlowBlend_InvSrcAlpha = 4,
	eNvFlowBlend_DstAlpha = 5,
	eNvFlowBlend_InvDstAlpha = 6,

	eNvFlowBlend_EnumCount = 7,
};

enum NvFlowBlendOpEnum
{
	eNvFlowBlendOp_Add = 1,
	eNvFlowBlendOp_Subtract = 2,
	eNvFlowBlendOp_RevSubtract = 3,
	eNvFlowBlendOp_Min = 4,
	eNvFlowBlendOp_Max = 5,

	eNvFlowBlendOp_EnumCount = 6
};

enum NvFlowComparisonEnum
{
	eNvFlowComparison_Never = 1,
	eNvFlowComparison_Less = 2,
	eNvFlowComparison_Equal = 3,
	eNvFlowComparison_LessEqual = 4,
	eNvFlowComparison_Greater = 5,
	eNvFlowComparison_NotEqual = 6,
	eNvFlowComparison_GreaterEqual = 7,
	eNvFlowComparison_Always = 8,

	eNvFlowComparison_EnumCount = 9
};

struct NvFlowBlendStateDesc
{
	bool enable;
	NvFlowBlendEnum srcBlendColor;
	NvFlowBlendEnum dstBlendColor;
	NvFlowBlendOpEnum blendOpColor;
	NvFlowBlendEnum srcBlendAlpha;
	NvFlowBlendEnum dstBlendAlpha;
	NvFlowBlendOpEnum blendOpAlpha;
};

enum NvFlowDepthWriteMask
{
	eNvFlowDepthWriteMask_Zero = 0,
	eNvFlowDepthWriteMask_All = 1
};

struct NvFlowDepthStateDesc
{
	bool depthEnable;
	NvFlowDepthWriteMask depthWriteMask;
	NvFlowComparisonEnum depthFunc;
};

struct NvFlowGraphicsShaderDesc
{
	const void* vs;
	NvFlowUint64 vs_length;
	const void* ps;
	NvFlowUint64 ps_length;
	const wchar_t* label;

	NvFlowUint numInputElements;
	NvFlowInputElementDesc* inputElementDescs;

	NvFlowBlendStateDesc blendState;
	NvFlowDepthStateDesc depthState;
	NvFlowUint numRenderTargets;
	NvFlowFormat renderTargetFormat[NV_FLOW_MAX_RENDER_TARGETS];
	NvFlowFormat depthStencilFormat;

	bool uavTarget;
	bool depthClipEnable;
	bool lineList;
};

NV_FLOW_API void NvFlowGraphicsShaderGetDesc(NvFlowGraphicsShader* shader, NvFlowGraphicsShaderDesc* desc);

struct NvFlowDrawParams
{
	NvFlowGraphicsShader* shader;
	NvFlowConstantBuffer* rootConstantBuffer;
	NvFlowResource* vs_readOnly[NV_FLOW_DRAW_MAX_READ_TEXTURES];
	NvFlowResource* ps_readOnly[NV_FLOW_DRAW_MAX_READ_TEXTURES];
	NvFlowResourceRW* ps_readWrite[NV_FLOW_DRAW_MAX_WRITE_TEXTURES];
	bool frontCounterClockwise;
};

NV_FLOW_API NvFlowGraphicsShader* NvFlowCreateGraphicsShader(NvFlowContext* context, const NvFlowGraphicsShaderDesc* desc);

NV_FLOW_API void NvFlowReleaseGraphicsShader(NvFlowGraphicsShader* shader);

NV_FLOW_API void NvFlowGraphicsShaderSetFormats(NvFlowContext* context, NvFlowGraphicsShader* shader, NvFlowFormat renderTargetFormat, NvFlowFormat depthStencilFormat);

//! A timer for work submitted to a context
struct NvFlowContextTimer;

NV_FLOW_API NvFlowContextTimer* NvFlowCreateContextTimer(NvFlowContext* context);

NV_FLOW_API void NvFlowReleaseContextTimer(NvFlowContextTimer* timer);

//! A queue of context events
struct NvFlowContextEventQueue;

NV_FLOW_API NvFlowContextEventQueue* NvFlowCreateContextEventQueue(NvFlowContext* context);

NV_FLOW_API void NvFlowReleaseContextEventQueue(NvFlowContextEventQueue* eventQueue);

//! An interface that create resource and submit GPU work
struct NvFlowContext;

NV_FLOW_API void NvFlowContextCopyConstantBuffer(NvFlowContext* context, NvFlowConstantBuffer* dst, NvFlowBuffer* src);

NV_FLOW_API void NvFlowContextCopyBuffer(NvFlowContext* context, NvFlowBuffer* dst, NvFlowBuffer* src, NvFlowUint offset, NvFlowUint numBytes);

NV_FLOW_API void NvFlowContextCopyTexture3D(NvFlowContext* context, NvFlowTexture3D* dst, NvFlowTexture3D* src);

NV_FLOW_API void NvFlowContextCopyResource(NvFlowContext* context, NvFlowResourceRW* resourceRW, NvFlowResource* resource);

NV_FLOW_API void NvFlowContextDispatch(NvFlowContext* context, const NvFlowDispatchParams* params);

NV_FLOW_API void NvFlowContextSetVertexBuffer(NvFlowContext* context, NvFlowVertexBuffer* vertexBuffer, NvFlowUint stride, NvFlowUint offset);

NV_FLOW_API void NvFlowContextSetIndexBuffer(NvFlowContext* context, NvFlowIndexBuffer* indexBuffer, NvFlowUint offset);

NV_FLOW_API void NvFlowContextDrawIndexedInstanced(NvFlowContext* context, NvFlowUint indicesPerInstance, NvFlowUint numInstances, const NvFlowDrawParams* params);

NV_FLOW_API void NvFlowContextSetRenderTarget(NvFlowContext* context, NvFlowRenderTarget* rt, NvFlowDepthStencil* ds);

NV_FLOW_API void NvFlowContextSetViewport(NvFlowContext* context, const NvFlowViewport* viewport);

NV_FLOW_API void NvFlowContextClearRenderTarget(NvFlowContext* context, NvFlowRenderTarget* rt, const NvFlowFloat4 color);

NV_FLOW_API void NvFlowContextClearDepthStencil(NvFlowContext* context, NvFlowDepthStencil* ds, const float depth);

NV_FLOW_API void NvFlowContextRestoreResourceState(NvFlowContext* context, NvFlowResource* resource);

NV_FLOW_API bool NvFlowContextIsSparseTextureSupported(NvFlowContext* context);

NV_FLOW_API void NvFlowContextUpdateSparseMapping(NvFlowContext* context, NvFlowTexture3DSparse* tex, NvFlowHeapSparse* heap, NvFlowUint* blockTableImage, NvFlowUint rowPitch, NvFlowUint depthPitch);

NV_FLOW_API void NvFlowContextTimerBegin(NvFlowContext* context, NvFlowContextTimer* timer);

NV_FLOW_API void NvFlowContextTimerEnd(NvFlowContext* context, NvFlowContextTimer* timer);

NV_FLOW_API void NvFlowContextTimerGetResult(NvFlowContext* context, NvFlowContextTimer* timer, float* timeGPU, float* timeCPU);

NV_FLOW_API void NvFlowContextEventQueuePush(NvFlowContext* context, NvFlowContextEventQueue* eventQueue, NvFlowUint64 uid);

NV_FLOW_API NvFlowResult NvFlowContextEventQueuePop(NvFlowContext* context, NvFlowContextEventQueue* eventQueue, NvFlowUint64* pUid);

NV_FLOW_API void NvFlowContextProfileGroupBegin(NvFlowContext* context, const wchar_t* label);

NV_FLOW_API void NvFlowContextProfileGroupEnd(NvFlowContext* context);

NV_FLOW_API void NvFlowContextProfileItemBegin(NvFlowContext* context, const wchar_t* label);

NV_FLOW_API void NvFlowContextProfileItemEnd(NvFlowContext* context);

//! A fence for queue synchronization
struct NvFlowFence;

struct NvFlowFenceDesc
{
	bool crossAdapterShared;
};

NV_FLOW_API void NvFlowFenceGetDesc(NvFlowFence* fence, NvFlowFenceDesc* desc);

NV_FLOW_API NvFlowFence* NvFlowCreateFence(NvFlowContext* context, const NvFlowFenceDesc* desc);

NV_FLOW_API NvFlowFence* NvFlowShareFence(NvFlowContext* context, NvFlowFence* fence);

NV_FLOW_API void NvFlowReleaseFence(NvFlowFence* fence);

NV_FLOW_API void NvFlowContextWaitOnFence(NvFlowContext* context, NvFlowFence* fence, NvFlowUint64 fenceValue);

NV_FLOW_API void NvFlowContextSignalFence(NvFlowContext* context, NvFlowFence* fence, NvFlowUint64 fenceValue);

//! A cross adapter shared 2d texture
struct NvFlowTexture2DCrossAdapter;

NV_FLOW_API NvFlowTexture2DCrossAdapter* NvFlowCreateTexture2DCrossAdapter(NvFlowContext* context, const NvFlowTexture2DDesc* desc);

NV_FLOW_API NvFlowTexture2DCrossAdapter* NvFlowShareTexture2DCrossAdapter(NvFlowContext* context, NvFlowTexture2DCrossAdapter* sharedTexture);

NV_FLOW_API void NvFlowReleaseTexture2DCrossAdapter(NvFlowTexture2DCrossAdapter* tex);

NV_FLOW_API void NvFlowContextTransitionToCommonState(NvFlowContext* context, NvFlowResource* resource);

NV_FLOW_API void NvFlowContextCopyToTexture2DCrossAdapter(NvFlowContext* context, NvFlowTexture2DCrossAdapter* dst, NvFlowTexture2D* src, NvFlowUint height);

NV_FLOW_API void NvFlowContextCopyFromTexture2DCrossAdapter(NvFlowContext* context, NvFlowTexture2D* dst, NvFlowTexture2DCrossAdapter* src, NvFlowUint height);

//! An opaque reference to another resource, for proper interqueue lifetime
struct NvFlowResourceReference;

NV_FLOW_API NvFlowResourceReference* NvFlowShareResourceReference(NvFlowContext* context, NvFlowResource* resource);

NV_FLOW_API void NvFlowReleaseResourceReference(NvFlowResourceReference* resource);