// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "D3D12LLM.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

struct FLLMTagInfoD3D12
{
	const TCHAR* Name;
	FName StatName;				// shows in the LLMFULL stat group
	FName SummaryStatName;		// shows in the LLM summary stat group
};

DECLARE_LLM_MEMORY_STAT(TEXT("D3D12 Committed Resources"), STAT_D3D12CommittedResourcesLLM, STATGROUP_LLMPlatform);

// *** order must match ELLMTagD3D12 enum ***
const FLLMTagInfoD3D12 ELLMTagNamesD3D12[] =
{
	// csv name									// stat name										// summary stat name						// enum value
	{ TEXT("D3D12 Committed Resources"),		GET_STATFNAME(STAT_D3D12CommittedResourcesLLM),		GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagD3D12::CommittedResources
};

/*
 * Register D3D12 tags with LLM
 */
void D3D12LLM::Initialise()
{
	int32 TagCount = sizeof(ELLMTagNamesD3D12) / sizeof(FLLMTagInfoD3D12);

	for (int32 Index = 0; Index < TagCount; ++Index)
	{
		int32 Tag = (int32)ELLMTag::PlatformRHITagStart + Index;
		const FLLMTagInfoD3D12& TagInfo = ELLMTagNamesD3D12[Index];

		FLowLevelMemTracker::Get().RegisterPlatformTag(Tag, TagInfo.Name, TagInfo.StatName, TagInfo.SummaryStatName);
	}
}

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

