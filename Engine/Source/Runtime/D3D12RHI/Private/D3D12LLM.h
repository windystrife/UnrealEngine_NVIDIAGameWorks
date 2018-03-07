// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/LowLevelMemTracker.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

#define LLM_SCOPE_D3D12(Tag) LLM_SCOPE((ELLMTag)Tag)
#define LLM_PLATFORM_SCOPE_D3D12(Tag) LLM_PLATFORM_SCOPE((ELLMTag)Tag)

enum class ELLMTagD3D12 : LLM_TAG_TYPE
{
	CommittedResources = (ELLMTagD3D12)ELLMTag::PlatformRHITagStart,

	Count
};

static_assert((int32)ELLMTagD3D12::Count <= (int32)ELLMTag::PlatformTagEnd, "too many ELLMTagD3D12 tags");

namespace D3D12LLM
{
	void Initialise();
}

#else

#define LLM_SCOPE_D3D12(...)
#define LLM_PLATFORM_SCOPE_D3D12(...)

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

