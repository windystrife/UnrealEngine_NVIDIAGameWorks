// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if OCULUS_HMD_SUPPORTED_PLATFORMS

//-------------------------------------------------------------------------------------------------
// D3D11
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
#include "D3D11RHIPrivate.h"
#include "D3D11Util.h"
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11


//-------------------------------------------------------------------------------------------------
// D3D12
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
#define GetD3D11CubeFace GetD3D12CubeFace
#define VerifyD3D11Result VerifyD3D12Result
#define GetD3D11TextureFromRHITexture GetD3D12TextureFromRHITexture
#define FRingAllocation FRingAllocation_D3D12
#define GetRenderTargetFormat GetRenderTargetFormat_D3D12
#define ED3D11ShaderOffsetBuffer ED3D12ShaderOffsetBuffer
#define FindShaderResourceDXGIFormat FindShaderResourceDXGIFormat_D3D12
#define FindUnorderedAccessDXGIFormat FindUnorderedAccessDXGIFormat_D3D12
#define FindDepthStencilDXGIFormat FindDepthStencilDXGIFormat_D3D12
#define HasStencilBits HasStencilBits_D3D12
#define FVector4VertexDeclaration FVector4VertexDeclaration_D3D12
#define GLOBAL_CONSTANT_BUFFER_INDEX GLOBAL_CONSTANT_BUFFER_INDEX_D3D12
#define MAX_CONSTANT_BUFFER_SLOTS MAX_CONSTANT_BUFFER_SLOTS_D3D12
#define FD3DGPUProfiler FD3D12GPUProfiler
#define FRangeAllocator FRangeAllocator_D3D12

#include "D3D12RHIPrivate.h"
#include "D3D12Util.h"

#undef GetD3D11CubeFace
#undef VerifyD3D11Result
#undef GetD3D11TextureFromRHITexture
#undef FRingAllocation
#undef GetRenderTargetFormat
#undef ED3D11ShaderOffsetBuffer
#undef FindShaderResourceDXGIFormat
#undef FindUnorderedAccessDXGIFormat
#undef FindDepthStencilDXGIFormat
#undef HasStencilBits
#undef FVector4VertexDeclaration
#undef GLOBAL_CONSTANT_BUFFER_INDEX
#undef MAX_CONSTANT_BUFFER_SLOTS
#undef FD3DGPUProfiler
#undef FRangeAllocator
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12


//-------------------------------------------------------------------------------------------------
// OpenGL
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL


//-------------------------------------------------------------------------------------------------
// Vulkan
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
#include "VulkanRHIPrivate.h"
#include "VulkanResources.h"
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS