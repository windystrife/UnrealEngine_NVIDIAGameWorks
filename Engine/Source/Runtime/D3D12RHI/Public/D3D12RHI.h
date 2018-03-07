// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RHI.h: Public D3D RHI definitions.
=============================================================================*/

#pragma once

///////////////////////////////////////////////////////////////
// Platform agnostic defines
///////////////////////////////////////////////////////////////
#define SUB_ALLOCATED_DEFAULT_ALLOCATIONS	1

#define DEBUG_RESOURCE_STATES	0

// DX12 doesn't support higher MSAA count
#define DX_MAX_MSAA_COUNT	8

// This is a value that should be tweaked to fit the app, lower numbers will have better performance
#define MAX_SRVS		32
#define MAX_SAMPLERS	16
#define MAX_UAVS		8
#define MAX_CBS			16

// This value controls how many root constant buffers can be used per shader stage in a root signature.
// Note: Using root descriptors significantly increases the size of root signatures (each root descriptor is 2 DWORDs).
#define MAX_ROOT_CBVS	MAX_CBS

// So outside callers can override this
#ifndef USE_STATIC_ROOT_SIGNATURE
	#define USE_STATIC_ROOT_SIGNATURE 0
#endif

// How many residency packets can be in flight before the rendering thread
// blocks for them to drain. Should be ~ NumBufferedFrames * AvgNumSubmissionsPerFrame i.e.
// enough to ensure that the GPU is rarely blocked by residency work
#define RESIDENCY_PIPELINE_DEPTH	6

#if PLATFORM_XBOXONE
	#define ENABLE_RESIDENCY_MANAGEMENT				0
	#define ASYNC_DEFERRED_DELETION					0
	#define PLATFORM_SUPPORTS_MGPU					0
	#define ASYNC_DEFERRED_DELETION					0
	#define PIPELINE_STATE_FILE_LOCATION			FPaths::ProjectContentDir()
	#define USE_PIX									XBOXONE_PROFILING_ENABLED

	// The number of sampler descriptors with the maximum value of 2048
	// If the heap type is unbounded the number could be increased to avoid rollovers.
	#define ENABLE_UNBOUNDED_SAMPLER_DESCRIPTORS	0
	#define NUM_SAMPLER_DESCRIPTORS					D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE

	// Xbox doesn't have DXGI but common code needs this defined for headers
	#define DXGI_QUERY_VIDEO_MEMORY_INFO			int
#elif PLATFORM_WINDOWS
	#define ENABLE_RESIDENCY_MANAGEMENT				1
	#define ASYNC_DEFERRED_DELETION					1
	#define PLATFORM_SUPPORTS_MGPU					1
	#define PIPELINE_STATE_FILE_LOCATION			FPaths::ProjectSavedDir()
	#define USE_PIX									D3D12_PROFILING_ENABLED
#else
	#error	Unknown platform!
#endif


#define FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
#define FD3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER

typedef uint16 CBVSlotMask;
static_assert(MAX_ROOT_CBVS <= MAX_CBS, "MAX_ROOT_CBVS must be <= MAX_CBS.");
static_assert((8 * sizeof(CBVSlotMask)) >= MAX_CBS, "CBVSlotMask isn't large enough to cover all CBs. Please increase the size.");
static_assert((8 * sizeof(CBVSlotMask)) >= MAX_ROOT_CBVS, "CBVSlotMask isn't large enough to cover all CBs. Please increase the size.");
static const CBVSlotMask GRootCBVSlotMask = (1 << MAX_ROOT_CBVS) - 1; // Mask for all slots that are used by root descriptors.
static const CBVSlotMask GDescriptorTableCBVSlotMask = static_cast<CBVSlotMask>(-1) & ~(GRootCBVSlotMask); // Mask for all slots that are used by a root descriptor table.

typedef uint32 SRVSlotMask;
static_assert((8 * sizeof(SRVSlotMask)) >= MAX_SRVS, "SRVSlotMask isn't large enough to cover all SRVs. Please increase the size.");

typedef uint16 SamplerSlotMask;
static_assert((8 * sizeof(SamplerSlotMask)) >= MAX_SAMPLERS, "SamplerSlotMask isn't large enough to cover all Samplers. Please increase the size.");

typedef uint8 UAVSlotMask;
static_assert((8 * sizeof(UAVSlotMask)) >= MAX_UAVS, "UAVSlotMask isn't large enough to cover all UAVs. Please increase the size.");
