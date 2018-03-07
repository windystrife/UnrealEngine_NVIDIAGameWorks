// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CrossCompilerCommon.h: Common functionality between compiler & runtime.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderCore.h"

namespace CrossCompiler
{
	enum
	{
		SHADER_STAGE_VERTEX = 0,
		SHADER_STAGE_PIXEL,
		SHADER_STAGE_GEOMETRY,
		SHADER_STAGE_HULL,
		SHADER_STAGE_DOMAIN,
		NUM_NON_COMPUTE_SHADER_STAGES,
		SHADER_STAGE_COMPUTE = NUM_NON_COMPUTE_SHADER_STAGES,
		NUM_SHADER_STAGES,
	};

	enum class EPackedTypeName : int8
	{
		HighP	= 'h',	// Make sure these enums match hlslcc
		MediumP	= 'm',
		LowP	= 'l',
		Int		= 'i',
		Uint	= 'u',

		Invalid = ' ',
	};

	enum class EPackedTypeIndex : int8
	{
		HighP	= 0,
		MediumP	= 1,
		LowP	= 2,
		Int		= 3,
		Uint	= 4,

		Max		= 5,
		Invalid = -1,
	};

	enum
	{
		PACKED_TYPENAME_HIGHP	= (int32)EPackedTypeName::HighP,	// Make sure these enums match hlslcc
		PACKED_TYPENAME_MEDIUMP	= (int32)EPackedTypeName::MediumP,
		PACKED_TYPENAME_LOWP	= (int32)EPackedTypeName::LowP,
		PACKED_TYPENAME_INT		= (int32)EPackedTypeName::Int,
		PACKED_TYPENAME_UINT	= (int32)EPackedTypeName::Uint,
		PACKED_TYPENAME_SAMPLER	= 's',
		PACKED_TYPENAME_IMAGE	= 'g',

		PACKED_TYPEINDEX_HIGHP		= (int32)EPackedTypeIndex::HighP,
		PACKED_TYPEINDEX_MEDIUMP	= (int32)EPackedTypeIndex::MediumP,
		PACKED_TYPEINDEX_LOWP		= (int32)EPackedTypeIndex::LowP,
		PACKED_TYPEINDEX_INT		= (int32)EPackedTypeIndex::Int,
		PACKED_TYPEINDEX_UINT		= (int32)EPackedTypeIndex::Uint,
		PACKED_TYPEINDEX_MAX		= (int32)EPackedTypeIndex::Max,
	};

	static FORCEINLINE uint8 ShaderStageIndexToTypeName(uint8 ShaderStage)
	{
		switch (ShaderStage)
		{
		case SHADER_STAGE_VERTEX:	return 'v';
		case SHADER_STAGE_PIXEL:	return 'p';
		case SHADER_STAGE_GEOMETRY:	return 'g';
		case SHADER_STAGE_HULL:		return 'h';
		case SHADER_STAGE_DOMAIN:	return 'd';
		case SHADER_STAGE_COMPUTE:	return 'c';
		default: break;
		}
		check(0);
		return 0;
	}

	static FORCEINLINE uint8 PackedTypeIndexToTypeName(uint8 ArrayType)
	{
		switch (ArrayType)
		{
		case PACKED_TYPEINDEX_HIGHP:	return PACKED_TYPENAME_HIGHP;
		case PACKED_TYPEINDEX_MEDIUMP:	return PACKED_TYPENAME_MEDIUMP;
		case PACKED_TYPEINDEX_LOWP:		return PACKED_TYPENAME_LOWP;
		case PACKED_TYPEINDEX_INT:		return PACKED_TYPENAME_INT;
		case PACKED_TYPEINDEX_UINT:		return PACKED_TYPENAME_UINT;
		default: break;
		}
		check(0);
		return 0;
	}

	static FORCEINLINE uint8 PackedTypeNameToTypeIndex(uint8 ArrayName)
	{
		switch (ArrayName)
		{
		case PACKED_TYPENAME_HIGHP:		return PACKED_TYPEINDEX_HIGHP;
		case PACKED_TYPENAME_MEDIUMP:	return PACKED_TYPEINDEX_MEDIUMP;
		case PACKED_TYPENAME_LOWP:		return PACKED_TYPEINDEX_LOWP;
		case PACKED_TYPENAME_INT:		return PACKED_TYPEINDEX_INT;
		case PACKED_TYPENAME_UINT:		return PACKED_TYPEINDEX_UINT;
		default: break;
		}
		check(0);
		return 0;
	}

	static FORCEINLINE bool IsValidPackedTypeName(EPackedTypeName TypeName)
	{
		switch (TypeName)
		{
		case EPackedTypeName::HighP:
		case EPackedTypeName::MediumP:
		case EPackedTypeName::LowP:
		case EPackedTypeName::Int:
		case EPackedTypeName::Uint:
			return true;
		default: break;
		}

		return false;
	}

	static FORCEINLINE EPackedTypeName PackedTypeIndexToTypeName(EPackedTypeIndex TypeIndex)
	{
		switch (TypeIndex)
		{
		case EPackedTypeIndex::HighP:	return EPackedTypeName::HighP;
		case EPackedTypeIndex::MediumP:	return EPackedTypeName::MediumP;
		case EPackedTypeIndex::LowP:	return EPackedTypeName::LowP;
		case EPackedTypeIndex::Int:		return EPackedTypeName::Int;
		case EPackedTypeIndex::Uint:	return EPackedTypeName::Uint;
		default: break;
		}

		return EPackedTypeName::Invalid;
	}

	static FORCEINLINE EPackedTypeIndex PackedTypeNameToTypeIndex(EPackedTypeName TypeName)
	{
		switch (TypeName)
		{
		case EPackedTypeName::HighP:	return EPackedTypeIndex::HighP;
		case EPackedTypeName::MediumP:	return EPackedTypeIndex::MediumP;
		case EPackedTypeName::LowP:		return EPackedTypeIndex::LowP;
		case EPackedTypeName::Int:		return EPackedTypeIndex::Int;
		case EPackedTypeName::Uint:		return EPackedTypeIndex::Uint;
		default: break;
		}

		return EPackedTypeIndex::Invalid;
	}

	struct FPackedArrayInfo
	{
		uint16	Size;		// Bytes
		uint8	TypeName;	// PACKED_TYPENAME
		uint8	TypeIndex;	// PACKED_TYPE
	};

	inline FArchive& operator<<(FArchive& Ar, FPackedArrayInfo& Info)
	{
		Ar << Info.Size;
		Ar << Info.TypeName;
		Ar << Info.TypeIndex;
		return Ar;
	}

	struct FShaderBindings
	{
		TArray<TArray<FPackedArrayInfo>>	PackedUniformBuffers;
		TArray<FPackedArrayInfo>			PackedGlobalArrays;
		FShaderCompilerResourceTable		ShaderResourceTable;

		uint16	InOutMask;
		uint8	NumSamplers;
		uint8	NumUniformBuffers;
		uint8	NumUAVs;
		bool	bHasRegularUniformBuffers;
	};

	// Information for copying members from uniform buffers to packed
	struct FUniformBufferCopyInfo
	{
		uint16 SourceOffsetInFloats;
		uint8 SourceUBIndex;
		uint8 DestUBIndex;
		uint8 DestUBTypeName;
		uint8 DestUBTypeIndex;
		uint16 DestOffsetInFloats;
		uint16 SizeInFloats;
	};

	inline FArchive& operator<<(FArchive& Ar, FUniformBufferCopyInfo& Info)
	{
		Ar << Info.SourceOffsetInFloats;
		Ar << Info.SourceUBIndex;
		Ar << Info.DestUBIndex;
		Ar << Info.DestUBTypeName;
		if (Ar.IsLoading())
		{
			Info.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(Info.DestUBTypeName);
		}
		Ar << Info.DestOffsetInFloats;
		Ar << Info.SizeInFloats;
		return Ar;
	}
}
