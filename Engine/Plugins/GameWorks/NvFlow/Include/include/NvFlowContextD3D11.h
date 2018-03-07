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

// --------------------------- NvFlowContextD3D11 -------------------------------
///@defgroup NvFlowContextD3D11
///@{

struct NvFlowDepthStencilViewDescD3D11
{
	ID3D11DepthStencilView* dsv;
	ID3D11ShaderResourceView* srv;
	D3D11_VIEWPORT viewport;
};

struct NvFlowRenderTargetViewDescD3D11
{
	ID3D11RenderTargetView* rtv;
	D3D11_VIEWPORT viewport;
};

struct NvFlowContextDescD3D11
{
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
};

struct NvFlowResourceViewDescD3D11
{
	ID3D11ShaderResourceView* srv;
};

struct NvFlowResourceRWViewDescD3D11
{
	NvFlowResourceViewDescD3D11 resourceView;
	ID3D11UnorderedAccessView* uav;
};

/**
 * Creates a graphics/compute context for Flow.
 *
 * @param[in] version Should be set by app to NV_FLOW_VERSION.
 * @param[in] desc A graphics-API dependent structure containing data needed for a FlowContext to interoperate with the app.
 *
 * @return The created Flow context.
 */
NV_FLOW_API NvFlowContext* NvFlowCreateContextD3D11(NvFlowUint version, const NvFlowContextDescD3D11* desc);

/**
 * Creates a Flow depth stencil view based on information provided by the application.
 *
 * @param[in] context The Flow context to create and use the depth stencil view.
 * @param[in] desc The graphics API dependent description.
 *
 * @return The created Flow depth stencil view.
 */
NV_FLOW_API NvFlowDepthStencilView* NvFlowCreateDepthStencilViewD3D11(NvFlowContext* context, const NvFlowDepthStencilViewDescD3D11* desc);

/**
 * Creates a Flow render target view based on information provided by the application.
 *
 * @param[in] context The Flow context to create and use the render target view.
 * @param[in] desc The graphics API dependent description.
 *
 * @return The created Flow render target view.
 */
NV_FLOW_API NvFlowRenderTargetView* NvFlowCreateRenderTargetViewD3D11(NvFlowContext* context, const NvFlowRenderTargetViewDescD3D11* desc);

/**
 * Updates a Flow context with information provided by the application.
 *
 * @param[in] context The Flow context to update.
 * @param[in] desc The graphics API dependent description.
 */
NV_FLOW_API void NvFlowUpdateContextD3D11(NvFlowContext* context, const NvFlowContextDescD3D11* desc);

/**
 * Gets a Flow context description from a Flow context.
 *
 * @param[in] context The Flow context.
 * @param[out] desc The graphics API dependent description.
 */
NV_FLOW_API void NvFlowUpdateContextDescD3D11(NvFlowContext* context, NvFlowContextDescD3D11* desc);

/**
 * Updates a Flow depth stencil view with information provided by the application.
 *
 * @param[in] context The Flow context used to create the depth stencil view.
 * @param[in] view The Flow depth stencil view to update.
 * @param[in] desc The graphics API dependent description.
 */
NV_FLOW_API void NvFlowUpdateDepthStencilViewD3D11(NvFlowContext* context, NvFlowDepthStencilView* view, const NvFlowDepthStencilViewDescD3D11* desc);

/**
 * Updates a Flow render target view with information provided by the application.
 *
 * @param[in] context The Flow context used to create the render target view.
 * @param[in] view The Flow render target view to update.
 * @param[in] desc The graphics API dependent description.
 */
NV_FLOW_API void NvFlowUpdateRenderTargetViewD3D11(NvFlowContext* context, NvFlowRenderTargetView* view, const NvFlowRenderTargetViewDescD3D11* desc);

/**
 * Updates an application visible description with internal Flow resource information.
 *
 * @param[in] context The Flow context that created the resource.
 * @param[in] resource The Flow resource to describe.
 * @param[out] desc The graphics API dependent Flow resource description.
 */
NV_FLOW_API void NvFlowUpdateResourceViewDescD3D11(NvFlowContext* context, NvFlowResource* resource, NvFlowResourceViewDescD3D11* desc);

/**
 * Updates an application visible description with internal Flow resourceRW information.
 *
 * @param[in] context The Flow context that created the resourceRW.
 * @param[in] resourceRW The Flow resourceRW to describe.
 * @param[out] desc The graphics API dependent Flow resourceRW description.
 */
NV_FLOW_API void NvFlowUpdateResourceRWViewDescD3D11(NvFlowContext* context, NvFlowResourceRW* resourceRW, NvFlowResourceRWViewDescD3D11* desc);

///@}