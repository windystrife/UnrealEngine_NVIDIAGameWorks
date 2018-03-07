/*
* This code contains NVIDIA Confidential Information and is disclosed
* under the Mutual Non-Disclosure Agreement.
*
* Notice
* ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES
* NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
* THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
*
* NVIDIA Corporation assumes no responsibility for the consequences of use of such
* information or for any infringement of patents or other rights of third parties that may
* result from its use. No license is granted by implication or otherwise under any patent
* or patent rights of NVIDIA Corporation. No third party distribution is allowed unless
* expressly authorized by NVIDIA.  Details are subject to change without notice.
* This code supersedes and replaces all information previously supplied.
* NVIDIA Corporation products are not authorized for use as critical
* components in life support devices or systems without express written approval of
* NVIDIA Corporation.
*
* Copyright ?2008- 2016 NVIDIA Corporation. All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software and related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is
* strictly prohibited.
*/

/*=============================================================================
WaveWorksShaderParameters.cpp: WaveWorks Shader Parameters
=============================================================================*/

#pragma once
#include "WaveWorksShaderParameters.h"
#include "RendererPrivate.h"
#include "StringConv.h"
#include "GFSDK_WaveWorks.h"

// Determines ShaderInputMappings based on ParameterMap
void FWaveWorksShaderParameters::Bind(const FShaderParameterMap& ParameterMap, EShaderFrequency Frequency, EShaderParameterFlags Flags)
{
	const TArray<WaveWorksShaderInput>* ShaderInput = GDynamicRHI->RHIGetDefaultContext()->RHIGetWaveWorksShaderInput();

	// waveworks simulation input
	if (ShaderInput != nullptr)
	{
		uint32 NumFound = 0;
		uint32 Count = ShaderInput->Num();
		ShaderInputMappings.SetNumZeroed(Count);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			uint32 InputMapping = GFSDK_WaveWorks_UnusedShaderInputRegisterMapping;
			if (Frequency == (*ShaderInput)[Index].Frequency)
			{
				uint16 BufferIndex = 0;
				uint16 BaseIndex = 0;
				uint16 NumBytes = 0;

				const TCHAR* Name = ANSI_TO_TCHAR((*ShaderInput)[Index].Name.GetPlainANSIString());
				if (ParameterMap.FindParameterAllocation(Name, BufferIndex, BaseIndex, NumBytes))
				{
					++NumFound;
					check(BufferIndex == 0 || BaseIndex == 0);
					InputMapping = BufferIndex + BaseIndex;					
				}

				ShaderInputMappings[Index] = InputMapping;
			}			
		}

		bIsBound = NumFound > 0;
		if (!bIsBound && Flags == SPF_Mandatory)
		{
			if (!UE_LOG_ACTIVE(LogShaders, Log))
			{
				UE_LOG(LogShaders, Fatal, TEXT("Failure to bind non-optional WaveWorks shader resources! \
				The parameters are either not present in the shader, or the shader compiler optimized it out."));
			}
			else
			{
				// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *(NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter",
					"Failure to bind non-optional WaveWorks shader resources! The parameter is either not present in the shader, \
				or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!").ToString()), TEXT("Warning"));
			}
		}
	}


	// Quad Tree Shader Input
	const TArray<WaveWorksShaderInput>* QuadTreeShaderInput = GDynamicRHI->RHIGetDefaultContext()->RHIGetWaveWorksQuadTreeShaderInput();
	if (nullptr != QuadTreeShaderInput)
	{
		uint32 Count = QuadTreeShaderInput->Num();
		QuadTreeShaderInputMappings.SetNumZeroed(Count);
		uint32 NumFound = 0;
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			uint32 InputMapping = GFSDK_WaveWorks_UnusedShaderInputRegisterMapping;
			if (Frequency == (*QuadTreeShaderInput)[Index].Frequency)
			{
				uint16 BufferIndex = 0;
				uint16 BaseIndex = 0;
				uint16 NumBytes = 0;

				const TCHAR* Name = ANSI_TO_TCHAR((*QuadTreeShaderInput)[Index].Name.GetPlainANSIString());
				if (ParameterMap.FindParameterAllocation(Name, BufferIndex, BaseIndex, NumBytes))
				{
					++NumFound;
					check(BufferIndex == 0 || BaseIndex == 0);
					InputMapping = BufferIndex + BaseIndex;					
				}

				QuadTreeShaderInputMappings[Index] = InputMapping;
			}			
		}

		if (!bIsBound && Flags == SPF_Mandatory)
		{
			if (!UE_LOG_ACTIVE(LogShaders, Log))
			{
				UE_LOG(LogShaders, Fatal, TEXT("Failure to bind non-optional WaveWorks shader resources!  \
					The parameters are either not present in the shader, or the shader compiler optimized it out."));
			}
			else
			{
				// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *(NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter",
					"Failure to bind non-optional WaveWorks shader resources! The parameter is either not present in the shader, \
					or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!").ToString()), TEXT("Warning"));
			}
		}
	}

	ShorelineDistanceFieldTexture.Bind(ParameterMap, TEXT("ShorelineDistanceFieldTexture"));
	ShorelineDistanceFieldTextureSampler.Bind(ParameterMap, TEXT("ShorelineDistanceFieldSampler"));

	uint16 BufferIndex = 0;	uint16 BaseIndex = 0; uint16 NumBytes = 0;
	ParameterMap.FindParameterAllocation(L"nv_waveworks_quad0", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_quad3",  BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr0", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr5", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr6", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr7", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr8", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr9", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr10", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr11", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr12", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr15", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr26", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr27", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr28", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr29", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr30", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr31", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr32", BufferIndex, BaseIndex, NumBytes);
	ParameterMap.FindParameterAllocation(L"nv_waveworks_attr33", BufferIndex, BaseIndex, NumBytes);
}

void FWaveWorksShaderParameters::Serialize(FArchive& Ar)
{
	Ar << bIsBound << ShaderInputMappings << QuadTreeShaderInputMappings;
	Ar << ShorelineDistanceFieldTexture << ShorelineDistanceFieldTextureSampler;
}
