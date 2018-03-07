// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12RootSignatureDefinitions.h: D3D12 utilities for Root signatures.
=============================================================================*/

#pragma once

#include "D3D12RHI.h"

namespace D3D12ShaderUtils
{
	// Simple base class to help write out a root signature (subclass to generate either to a binary struct or a #define)
	struct FRootSignatureCreator
	{
		virtual ~FRootSignatureCreator() { }
		virtual void AddRootFlag(D3D12_ROOT_SIGNATURE_FLAGS Flag) = 0;
		enum EType
		{
			CBV,
			SRV,
			UAV,
			Sampler,
		};
		virtual void AddTable(EShaderFrequency Stage, EType Type, int32 NumDescriptors/*, bool bAppend*/) = 0;
	};

	// Fat/Static Gfx Root Signature
	inline void CreateGfxRootSignature(FRootSignatureCreator* Creator)
	{
		Creator->AddRootFlag(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		//Creator->AddRootFlag(D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS);
		//Creator->AddRootFlag(D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS);
		Creator->AddTable(SF_Pixel, FRootSignatureCreator::SRV, MAX_SRVS);
		Creator->AddTable(SF_Pixel, FRootSignatureCreator::CBV, MAX_CBS);
		Creator->AddTable(SF_Pixel, FRootSignatureCreator::Sampler, MAX_SAMPLERS);
		Creator->AddTable(SF_Vertex, FRootSignatureCreator::SRV, MAX_SRVS);
		Creator->AddTable(SF_Vertex, FRootSignatureCreator::CBV, MAX_CBS);
		Creator->AddTable(SF_Vertex, FRootSignatureCreator::Sampler, MAX_SAMPLERS);
		Creator->AddTable(SF_Geometry, FRootSignatureCreator::SRV, MAX_SRVS);
		Creator->AddTable(SF_Geometry, FRootSignatureCreator::CBV, MAX_CBS);
		Creator->AddTable(SF_Geometry, FRootSignatureCreator::Sampler, MAX_SAMPLERS);
		Creator->AddTable(SF_Hull, FRootSignatureCreator::SRV, MAX_SRVS);
		Creator->AddTable(SF_Hull, FRootSignatureCreator::CBV, MAX_CBS);
		Creator->AddTable(SF_Hull, FRootSignatureCreator::Sampler, MAX_SAMPLERS);
		Creator->AddTable(SF_Domain, FRootSignatureCreator::SRV, MAX_SRVS);
		Creator->AddTable(SF_Domain, FRootSignatureCreator::CBV, MAX_CBS);
		Creator->AddTable(SF_Domain, FRootSignatureCreator::Sampler, MAX_SAMPLERS);
		Creator->AddTable(SF_NumFrequencies, FRootSignatureCreator::UAV, MAX_UAVS);
	}

	// Fat/Static Compute Root Signature
	inline void CreateComputeRootSignature(FRootSignatureCreator* Creator)
	{
		Creator->AddRootFlag(D3D12_ROOT_SIGNATURE_FLAG_NONE);
		Creator->AddTable(SF_NumFrequencies, FRootSignatureCreator::SRV, MAX_SRVS);
		Creator->AddTable(SF_NumFrequencies, FRootSignatureCreator::CBV, MAX_CBS);
		Creator->AddTable(SF_NumFrequencies, FRootSignatureCreator::Sampler, MAX_SAMPLERS);
		Creator->AddTable(SF_NumFrequencies, FRootSignatureCreator::UAV, MAX_UAVS);
	}

	inline D3D12_SHADER_VISIBILITY TranslateShaderVisibility(EShaderFrequency Stage)
	{
		switch (Stage)
		{
		case SF_Vertex:
			return D3D12_SHADER_VISIBILITY_VERTEX;
		case SF_Pixel:
			return D3D12_SHADER_VISIBILITY_PIXEL;
		case SF_Geometry:
			return D3D12_SHADER_VISIBILITY_GEOMETRY;
		case SF_Domain:
			return D3D12_SHADER_VISIBILITY_DOMAIN;
		case SF_Hull:
			return D3D12_SHADER_VISIBILITY_HULL;
		default:
			return D3D12_SHADER_VISIBILITY_ALL;
		}
	}

	struct FBinaryRootSignatureCreator : public FRootSignatureCreator
	{
		TArray<D3D12_DESCRIPTOR_RANGE/*1*/> DescriptorRanges;
		TArray<D3D12_ROOT_PARAMETER/*1*/> Parameters;
		TMap<uint32, uint32> ParameterToRangeMap;

		D3D12_ROOT_SIGNATURE_FLAGS Flags;

		FBinaryRootSignatureCreator() : Flags(D3D12_ROOT_SIGNATURE_FLAG_NONE) {}

		virtual void AddRootFlag(D3D12_ROOT_SIGNATURE_FLAGS Flag) override
		{
			Flags |= Flag;
		}

		D3D12_DESCRIPTOR_RANGE_TYPE GetD3D12Type(EType Type)
		{
			switch (Type)
			{
			case D3D12ShaderUtils::FRootSignatureCreator::SRV:
				return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			case D3D12ShaderUtils::FRootSignatureCreator::UAV:
				return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			case D3D12ShaderUtils::FRootSignatureCreator::Sampler:
				return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			default:
				return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			}
		}

		virtual void AddTable(EShaderFrequency Stage, EType Type, int32 NumDescriptors)
		{
			int32 ParameterIndex = Parameters.AddZeroed();
			int32 RangeIndex = DescriptorRanges.AddZeroed();

			D3D12_ROOT_PARAMETER/*1*/& Parameter = Parameters[ParameterIndex];
			D3D12_DESCRIPTOR_RANGE/*1*/& Range = DescriptorRanges[ParameterIndex];

			Parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			Parameter.DescriptorTable.NumDescriptorRanges = 1;

			// Pointer will be filled in last
			//Parameter.DescriptorTable.pDescriptorRanges = &DescriptorRanges[0];
			ParameterToRangeMap.Add(ParameterIndex, RangeIndex);

			Range.RangeType = GetD3D12Type(Type);
			Range.NumDescriptors = NumDescriptors;
			//Range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
			//Range.OffsetInDescriptorsFromTableStart = UINT32_MAX;
			Parameter.ShaderVisibility =  D3D12ShaderUtils::TranslateShaderVisibility(Stage);
		}

		void Compile()
		{
			D3D12ShaderUtils::CreateGfxRootSignature(this);

			// Patch pointers
			for (auto& Pair : ParameterToRangeMap)
			{
				Parameters[Pair.Key].DescriptorTable.pDescriptorRanges = &DescriptorRanges[Pair.Value];
			}
		}
	};
}
