// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderCacheTypes.h: Shader Cache Types
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIDefinitions.h"

/** Texture type enum for shader cache draw keys */
enum EShaderCacheTextureType
{
	SCTT_Invalid,
	SCTT_Texture1D,
	SCTT_Texture2D,
	SCTT_Texture3D,
	SCTT_TextureCube,
	SCTT_Texture1DArray,
	SCTT_Texture2DArray,
	SCTT_TextureCubeArray,
	SCTT_Buffer, 
	SCTT_TextureExternal2D
};

/** The minimum texture state required for logging shader draw states */
struct SHADERCORE_API FShaderTextureKey
{
	mutable uint32 Hash;
	uint32 X;
	uint32 Y;
	uint32 Z;
	uint32 Flags; // ETextureCreateFlags
	uint32 MipLevels;
	uint32 Samples;
	uint8 Format;
	TEnumAsByte<EShaderCacheTextureType> Type;
	
	FShaderTextureKey()
	: Hash(0)
	, X(0)
	, Y(0)
	, Z(0)
	, Flags(0)
	, MipLevels(0)
	, Samples(0)
	, Format(PF_Unknown)
	, Type(SCTT_Invalid)
	{
	}
	
	friend bool operator ==(const FShaderTextureKey& A,const FShaderTextureKey& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z && A.Flags == B.Flags && A.MipLevels == B.MipLevels && A.Samples == B.Samples && A.Format == B.Format && A.Type == B.Type;
	}
	
	friend uint32 GetTypeHash(const FShaderTextureKey &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = Key.X * 3;
			Key.Hash ^= Key.Y * 2;
			Key.Hash ^= Key.Z;
			Key.Hash ^= Key.Flags;
			Key.Hash ^= (Key.Format << 24);
			Key.Hash ^= (Key.MipLevels << 16);
			Key.Hash ^= (Key.Samples << 8);
			Key.Hash ^= Key.Type;
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderTextureKey& Info )
	{
		return Ar << Info.Format << Info.Type << Info.Samples << Info.MipLevels << Info.Flags << Info.X << Info.Y << Info.Z << Info.Hash;
	}
};

/** SRV state tracked by the shader-cache to properly predraw shaders */
struct SHADERCORE_API FShaderResourceKey
{
	FShaderTextureKey Tex;
	mutable uint32 Hash;
	uint32 BaseMip;
	uint32 MipLevels;
	uint8 Format;
	bool bSRV;
	
	FShaderResourceKey()
	: Hash(0)
	, BaseMip(0)
	, MipLevels(0)
	, Format(0)
	, bSRV(false)
	{
	}
	
	friend bool operator ==(const FShaderResourceKey& A,const FShaderResourceKey& B)
	{
		return A.BaseMip == B.BaseMip && A.MipLevels == B.MipLevels && A.Format == B.Format && A.bSRV == B.bSRV && A.Tex == B.Tex;
	}
	
	friend uint32 GetTypeHash(const FShaderResourceKey &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = GetTypeHash(Key.Tex);
			Key.Hash ^= (Key.BaseMip << 24);
			Key.Hash ^= (Key.MipLevels << 16);
			Key.Hash ^= (Key.Format << 8);
			Key.Hash ^= (uint32)Key.bSRV;
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderResourceKey& Info )
	{
		return Ar << Info.Tex << Info.BaseMip << Info.MipLevels << Info.Format << Info.bSRV << Info.Hash;
	}
};

/** Render target state tracked for predraw */
struct SHADERCORE_API FShaderRenderTargetKey
{
	FShaderTextureKey Texture;
	mutable uint32 Hash;
	uint32 MipLevel;
	uint32 ArrayIndex;
	
	FShaderRenderTargetKey()
	: Hash(0)
	, MipLevel(0)
	, ArrayIndex(0)
	{
		
	}
	
	friend bool operator ==(const FShaderRenderTargetKey& A,const FShaderRenderTargetKey& B)
	{
		return A.MipLevel == B.MipLevel && A.ArrayIndex == B.ArrayIndex && A.Texture == B.Texture;
	}
	
	friend uint32 GetTypeHash(const FShaderRenderTargetKey &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = GetTypeHash(Key.Texture);
			Key.Hash ^= (Key.MipLevel << 8);
			Key.Hash ^= Key.ArrayIndex;
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderRenderTargetKey& Info )
	{
		return Ar << Info.Texture << Info.MipLevel << Info.ArrayIndex << Info.Hash;
	}
};

struct SHADERCORE_API FShaderCacheKey
{
	FShaderCacheKey() : Frequency(SF_NumFrequencies), Hash(0), bActive(false) {}
	
	FSHAHash SHAHash;
	EShaderFrequency Frequency;
	mutable uint32 Hash;
	bool bActive;
	
	friend bool operator ==(const FShaderCacheKey& A,const FShaderCacheKey& B)
	{
		return A.SHAHash == B.SHAHash && A.Frequency == B.Frequency && A.bActive == B.bActive;
	}
	
	friend uint32 GetTypeHash(const FShaderCacheKey &Key)
	{
		if(!Key.Hash)
		{
			uint32 TargetFrequency = Key.Frequency;
			Key.Hash = FCrc::MemCrc_DEPRECATED((const void*)&Key.SHAHash, sizeof(Key.SHAHash)) ^ (GetTypeHash(TargetFrequency) << 16) ^ GetTypeHash(Key.bActive);
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderCacheKey& Info )
	{
		Ar << Info.SHAHash;
		
		uint8 TargetFrequency = Info.Frequency;
		Ar << TargetFrequency;
		Info.Frequency = (EShaderFrequency)TargetFrequency;

		Ar << Info.bActive;

		if (Ar.IsLoading())
		{
			Info.Hash = GetTypeHash(Info);
		}

		return Ar;
	}
};

struct SHADERCORE_API FShaderCacheRasterizerState
{
	FShaderCacheRasterizerState() { FMemory::Memzero(*this); }
	FShaderCacheRasterizerState(FRasterizerStateInitializerRHI const& Other) { operator=(Other); }
	
	float DepthBias;
	float SlopeScaleDepthBias;
	TEnumAsByte<ERasterizerFillMode> FillMode;
	TEnumAsByte<ERasterizerCullMode> CullMode;
	bool bAllowMSAA;
	bool bEnableLineAA;
	
	FShaderCacheRasterizerState& operator=(FRasterizerStateInitializerRHI const& Other)
	{
		DepthBias = Other.DepthBias;
		SlopeScaleDepthBias = Other.SlopeScaleDepthBias;
		FillMode = Other.FillMode;
		CullMode = Other.CullMode;
		bAllowMSAA = Other.bAllowMSAA;
		bEnableLineAA = Other.bEnableLineAA;
		return *this;
	}
	
	operator FRasterizerStateInitializerRHI () const
	{
		FRasterizerStateInitializerRHI Initializer = {FillMode, CullMode, DepthBias, SlopeScaleDepthBias, bAllowMSAA, bEnableLineAA};
		return Initializer;
	}
	
	friend FArchive& operator<<(FArchive& Ar,FShaderCacheRasterizerState& RasterizerStateInitializer)
	{
		Ar << RasterizerStateInitializer.DepthBias;
		Ar << RasterizerStateInitializer.SlopeScaleDepthBias;
		Ar << RasterizerStateInitializer.FillMode;
		Ar << RasterizerStateInitializer.CullMode;
		Ar << RasterizerStateInitializer.bAllowMSAA;
		Ar << RasterizerStateInitializer.bEnableLineAA;
		return Ar;
	}
	
	friend uint32 GetTypeHash(const FShaderCacheRasterizerState &Key)
	{
		uint32 KeyHash = (*((uint32*)&Key.DepthBias) ^ *((uint32*)&Key.SlopeScaleDepthBias));
		KeyHash ^= (Key.FillMode << 8);
		KeyHash ^= Key.CullMode;
		KeyHash ^= Key.bAllowMSAA ? 2 : 0;
		KeyHash ^= Key.bEnableLineAA ? 1 : 0;
		return KeyHash;
	}
};

enum EShaderCacheStateValues
{
	EShaderCacheMaxNumSamplers = 16,
	EShaderCacheMaxNumResources = 128,
	EShaderCacheNullState = ~(0u),
	EShaderCacheInvalidState = ~(1u)
};

struct SHADERCORE_API FShaderPipelineKey
{
	FShaderCacheKey VertexShader;
	FShaderCacheKey PixelShader;
	FShaderCacheKey GeometryShader;
	FShaderCacheKey HullShader;
	FShaderCacheKey DomainShader;
	mutable uint32 Hash;
	
	FShaderPipelineKey() : Hash(0) {}
	
	friend bool operator ==(const FShaderPipelineKey& A,const FShaderPipelineKey& B)
	{
		return A.VertexShader == B.VertexShader && A.PixelShader == B.PixelShader && A.GeometryShader == B.GeometryShader && A.HullShader == B.HullShader && A.DomainShader == B.DomainShader;
	}
	
	friend uint32 GetTypeHash(const FShaderPipelineKey &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash ^= GetTypeHash(Key.VertexShader) ^ GetTypeHash(Key.PixelShader) ^ GetTypeHash(Key.GeometryShader) ^ GetTypeHash(Key.HullShader) ^ GetTypeHash(Key.DomainShader);
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderPipelineKey& Info )
	{
		return Ar << Info.VertexShader << Info.PixelShader << Info.GeometryShader << Info.HullShader << Info.DomainShader << Info.Hash;
	}
};

struct SHADERCORE_API FShaderCacheGraphicsPipelineState
{
	uint32 PrimitiveType;
	uint32 BoundShaderState;
	FBlendStateInitializerRHI BlendState;
	FShaderCacheRasterizerState RasterizerState;
	FDepthStencilStateInitializerRHI DepthStencilState;
	uint32 RenderTargets[MaxSimultaneousRenderTargets];
	uint32 RenderTargetFlags[MaxSimultaneousRenderTargets];
	uint8 RenderTargetLoad[MaxSimultaneousRenderTargets];
	uint8 RenderTargetStore[MaxSimultaneousRenderTargets];
	uint32 DepthStencilTarget;
	uint32 DepthStencilTargetFlags;
	uint8 DepthLoad;
	uint8 DepthStore;
	uint8 StencilLoad;
	uint8 StencilStore;
	uint8 ActiveRenderTargets;
	uint8 SampleCount;
	mutable uint32 Hash;
	
	// Transient - not included in hash or equality
	int32 Index;
	
	FShaderCacheGraphicsPipelineState()
	: PrimitiveType(PT_Num)
	, BoundShaderState(EShaderCacheNullState)
	, DepthStencilTarget(EShaderCacheNullState)
	, DepthStencilTargetFlags(0)
	, DepthLoad((uint8)ERenderTargetLoadAction::ENoAction)
	, DepthStore((uint8)ERenderTargetStoreAction::ENoAction)
	, StencilLoad((uint8)ERenderTargetLoadAction::ENoAction)
	, StencilStore((uint8)ERenderTargetStoreAction::ENoAction)
	, ActiveRenderTargets(0)
	, SampleCount(0)
	, Hash(0)
	, Index(0)
	{
		FMemory::Memzero(&BlendState, sizeof(BlendState));
		FMemory::Memzero(&RasterizerState, sizeof(RasterizerState));
		FMemory::Memzero(&DepthStencilState, sizeof(DepthStencilState));
		FMemory::Memset(RenderTargets, 255, sizeof(RenderTargets));
		FMemory::Memset(RenderTargetFlags, 0, sizeof(RenderTargetFlags));
		FMemory::Memset(RenderTargetLoad, 0, sizeof(RenderTargetLoad));
		FMemory::Memset(RenderTargetStore, 0, sizeof(RenderTargetStore));
	}
	
	friend bool operator ==(const FShaderCacheGraphicsPipelineState& A,const FShaderCacheGraphicsPipelineState& B)
	{
		bool Compare = A.PrimitiveType == B.PrimitiveType && A.BoundShaderState == B.BoundShaderState && A.ActiveRenderTargets == B.ActiveRenderTargets &&
		A.SampleCount == B.SampleCount && A.DepthStencilTarget == B.DepthStencilTarget &&
		A.DepthStencilTargetFlags == B.DepthStencilTargetFlags && A.DepthLoad == B.DepthLoad &&
		A.DepthStore == B.DepthStore && A.StencilLoad == B.StencilLoad && A.StencilStore == B.StencilStore &&
		FMemory::Memcmp(&A.BlendState, &B.BlendState, sizeof(FBlendStateInitializerRHI)) == 0 &&
		FMemory::Memcmp(&A.RasterizerState, &B.RasterizerState, sizeof(FShaderCacheRasterizerState)) == 0 &&
		FMemory::Memcmp(&A.DepthStencilState, &B.DepthStencilState, sizeof(FDepthStencilStateInitializerRHI)) == 0 &&
		FMemory::Memcmp(&A.RenderTargets, &B.RenderTargets, sizeof(A.RenderTargets)) == 0 &&
		FMemory::Memcmp(&A.RenderTargetFlags, &B.RenderTargetFlags, sizeof(A.RenderTargetFlags)) == 0 &&
		FMemory::Memcmp(&A.RenderTargetLoad, &B.RenderTargetLoad, sizeof(A.RenderTargetLoad)) == 0 &&
		FMemory::Memcmp(&A.RenderTargetStore, &B.RenderTargetStore, sizeof(A.RenderTargetStore)) == 0;
		return Compare;
	}
	
	friend uint32 GetTypeHash(const FShaderCacheGraphicsPipelineState &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = GetTypeHash(Key.PrimitiveType);
			Key.Hash ^= GetTypeHash(Key.BoundShaderState);
			Key.Hash ^= GetTypeHash(Key.SampleCount);
			Key.Hash ^= GetTypeHash(Key.ActiveRenderTargets);
			
			Key.Hash ^= (Key.BlendState.bUseIndependentRenderTargetBlendStates ? (1 << 31) : 0);
			for( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
			{
				Key.Hash ^= (Key.BlendState.RenderTargets[i].ColorBlendOp << 24);
				Key.Hash ^= (Key.BlendState.RenderTargets[i].ColorSrcBlend << 16);
				Key.Hash ^= (Key.BlendState.RenderTargets[i].ColorDestBlend << 8);
				Key.Hash ^= (Key.BlendState.RenderTargets[i].ColorWriteMask << 0);
				Key.Hash ^= (Key.BlendState.RenderTargets[i].AlphaBlendOp << 24);
				Key.Hash ^= (Key.BlendState.RenderTargets[i].AlphaSrcBlend << 16);
				Key.Hash ^= (Key.BlendState.RenderTargets[i].AlphaDestBlend << 8);
				Key.Hash ^= Key.RenderTargets[i];
				Key.Hash ^= Key.RenderTargetFlags[i];
				Key.Hash ^= Key.RenderTargetLoad[i] << 24;
				Key.Hash ^= Key.RenderTargetStore[i] << 16;
			}
			
			Key.Hash ^= (Key.DepthStencilState.bEnableDepthWrite ? (1 << 31) : 0);
			Key.Hash ^= (Key.DepthStencilState.DepthTest << 24);
			Key.Hash ^= (Key.DepthStencilState.bEnableFrontFaceStencil ? (1 << 23) : 0);
			Key.Hash ^= (Key.DepthStencilState.FrontFaceStencilTest << 24);
			Key.Hash ^= (Key.DepthStencilState.FrontFaceStencilFailStencilOp << 16);
			Key.Hash ^= (Key.DepthStencilState.FrontFaceDepthFailStencilOp << 8);
			Key.Hash ^= (Key.DepthStencilState.FrontFacePassStencilOp);
			Key.Hash ^= (Key.DepthStencilState.bEnableBackFaceStencil ? (1 << 15) : 0);
			Key.Hash ^= (Key.DepthStencilState.BackFaceStencilTest << 24);
			Key.Hash ^= (Key.DepthStencilState.BackFaceStencilFailStencilOp << 16);
			Key.Hash ^= (Key.DepthStencilState.BackFaceDepthFailStencilOp << 8);
			Key.Hash ^= (Key.DepthStencilState.BackFacePassStencilOp);
			Key.Hash ^= (Key.DepthStencilState.StencilReadMask << 8);
			Key.Hash ^= (Key.DepthStencilState.StencilWriteMask);
			
			Key.Hash ^= GetTypeHash(Key.DepthStencilTarget);
			Key.Hash ^= GetTypeHash(Key.DepthStencilTargetFlags);
			Key.Hash ^= GetTypeHash(Key.DepthLoad << 24);
			Key.Hash ^= GetTypeHash(Key.DepthStore << 16);
			Key.Hash ^= GetTypeHash(Key.StencilLoad << 8);
			Key.Hash ^= GetTypeHash(Key.StencilStore);
			
			Key.Hash ^= GetTypeHash(Key.RasterizerState);
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderCacheGraphicsPipelineState& Info )
	{
		Ar << Info.PrimitiveType;
		Ar << Info.BoundShaderState;
		Ar << Info.SampleCount;
		Ar << Info.ActiveRenderTargets;
		for ( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
		{
			Ar << Info.RenderTargets[i];
			Ar << Info.RenderTargetFlags[i];
			Ar << Info.RenderTargetLoad[i];
			Ar << Info.RenderTargetStore[i];
		}
		Ar << Info.DepthStencilTarget << Info.DepthStencilTargetFlags << Info.DepthLoad << Info.DepthStore << Info.StencilLoad << Info.StencilStore;
		return Ar << Info.BlendState << Info.RasterizerState << Info.DepthStencilState << Info.Hash;
	}
};

struct SHADERCORE_API FShaderDrawKey
{
	FShaderDrawKey()
	: IndexType(0)
	, Hash(0)
	{
		FMemory::Memset(SamplerStates, 255, sizeof(SamplerStates));
		FMemory::Memset(Resources, 255, sizeof(Resources));
		FMemory::Memzero(UsedResourcesLo);
		FMemory::Memzero(UsedResourcesHi);
	}
	
	uint32 SamplerStates[SF_NumFrequencies][EShaderCacheMaxNumSamplers];
    uint32 Resources[SF_NumFrequencies][EShaderCacheMaxNumResources];
	uint64 UsedResourcesLo[SF_NumFrequencies];
	uint64 UsedResourcesHi[SF_NumFrequencies];
    uint8 IndexType;
	mutable uint32 Hash;
	
	static uint32 CurrentMaxResources;
	
	friend bool operator ==(const FShaderDrawKey& A,const FShaderDrawKey& B)
	{
		bool Compare = A.IndexType == B.IndexType &&
		FMemory::Memcmp(&A.UsedResourcesLo, &B.UsedResourcesLo, sizeof(A.UsedResourcesLo)) == 0 &&
		(CurrentMaxResources <= 64 || FMemory::Memcmp(&A.UsedResourcesHi, &B.UsedResourcesHi, sizeof(A.UsedResourcesHi)) == 0) &&
		FMemory::Memcmp(&A.Resources, &B.Resources, (SF_NumFrequencies * CurrentMaxResources * sizeof(uint32))) == 0 &&
		FMemory::Memcmp(&A.SamplerStates, &B.SamplerStates, sizeof(A.SamplerStates)) == 0;
		return Compare;
	}
	
	friend uint32 GetTypeHash(const FShaderDrawKey &Key)
	{
		if(!Key.Hash)
		{
            Key.Hash = GetTypeHash(Key.IndexType);
            
			for( uint32 i = 0; i < SF_NumFrequencies; i++ )
			{
				for( uint32 j = 0; j < EShaderCacheMaxNumSamplers; j++ )
				{
					Key.Hash ^= Key.SamplerStates[i][j];
				}
				
				uint32 NumResources = (Key.UsedResourcesHi[i] == 0) ? (uint32)FMath::FloorLog2_64(Key.UsedResourcesLo[i]) : 63 + (uint32)FMath::FloorLog2_64(Key.UsedResourcesHi[i]);
				for( uint32 j = 0; j < NumResources; j++ )
				{
					Key.Hash ^= Key.Resources[i][j];
				}
			}
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderDrawKey& Info )
	{
        Ar << Info.IndexType;
		for ( uint32 i = 0; i < SF_NumFrequencies; i++ )
		{
			for ( uint32 j = 0; j < EShaderCacheMaxNumSamplers; j++ )
			{
				Ar << Info.SamplerStates[i][j];
			}
			
			Ar << Info.UsedResourcesLo[i];
			Ar << Info.UsedResourcesHi[i];
			
			uint32 NumResources = (Info.UsedResourcesHi[i] == 0) ? (uint32)FMath::FloorLog2_64(Info.UsedResourcesLo[i]) : 63 + (uint32)FMath::FloorLog2_64(Info.UsedResourcesHi[i]);
			for ( uint32 j = 0; j < NumResources; j++ )
			{
				Ar << Info.Resources[i][j];
			}
		}
		return Ar;
	}
};

struct SHADERCORE_API FShaderCodeCache
{
	friend FArchive& operator<<( FArchive& Ar, FShaderCodeCache& Info );
	
	// Serialised
	TMap<FShaderCacheKey, TPair<uint32, TArray<uint8>>> Shaders;
	TMap<FShaderCacheKey, TSet<FShaderPipelineKey>> Pipelines;

	// Non-serialised
#if WITH_EDITORONLY_DATA
	TMap<FShaderCacheKey, TArray<TPair<int32, TArray<uint8>>>> Counts;
#endif

	friend FArchive& operator<<( FArchive& Ar, FShaderCodeCache& Info );
};

struct FShaderCacheBoundState
{
	FVertexDeclarationElementList VertexDeclaration;
	FShaderCacheKey VertexShader;
	FShaderCacheKey PixelShader;
	FShaderCacheKey GeometryShader;
	FShaderCacheKey HullShader;
	FShaderCacheKey DomainShader;
	mutable uint32 Hash;
	
	FShaderCacheBoundState() : Hash(0) {}
	
	friend bool operator ==(const FShaderCacheBoundState& A,const FShaderCacheBoundState& B)
	{
		if(A.VertexDeclaration.Num() == B.VertexDeclaration.Num())
		{
			for(int32 i = 0; i < A.VertexDeclaration.Num(); i++)
			{
				if(FMemory::Memcmp(&A.VertexDeclaration[i], &B.VertexDeclaration[i], sizeof(FVertexElement)))
				{
					return false;
				}
			}
			return A.VertexShader == B.VertexShader && A.PixelShader == B.PixelShader && A.GeometryShader == B.GeometryShader && A.HullShader == B.HullShader && A.DomainShader == B.DomainShader;
		}
		return false;
	}
	
	friend uint32 GetTypeHash(const FShaderCacheBoundState &Key)
	{
		if(!Key.Hash)
		{
			for(auto Element : Key.VertexDeclaration)
			{
				Key.Hash ^= FCrc::MemCrc_DEPRECATED(&Element, sizeof(FVertexElement));
			}
			
			Key.Hash ^= GetTypeHash(Key.VertexShader) ^ GetTypeHash(Key.PixelShader) ^ GetTypeHash(Key.GeometryShader) ^ GetTypeHash(Key.HullShader) ^ GetTypeHash(Key.DomainShader);
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderCacheBoundState& Info )
	{
		return Ar << Info.VertexDeclaration << Info.VertexShader << Info.PixelShader << Info.GeometryShader << Info.HullShader << Info.DomainShader << Info.Hash;
	}
};

struct FSamplerStateInitializerRHIKeyFuncs : TDefaultMapKeyFuncs<FSamplerStateInitializerRHI,int32,false>
{
	/**
	 * @return True if the keys match.
	 */
	static bool Matches(KeyInitType A,KeyInitType B);
	
	/** Calculates a hash index for a key. */
	static uint32 GetKeyHash(KeyInitType Key);
};

struct FShaderStreamingCache
{
	TMap<int32, TSet<int32>> ShaderDrawStates;
	
	friend FArchive& operator<<( FArchive& Ar, FShaderStreamingCache& Info )
	{
		return Ar << Info.ShaderDrawStates;
	}
};

template <class Type, typename KeyFuncs = TDefaultMapKeyFuncs<Type,int32,false>>
class TIndexedSet
{
	TMap<Type, int32, FDefaultSetAllocator, KeyFuncs> Map;
	TArray<Type> Data;
	
public:
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	TIndexedSet() = default;
	TIndexedSet(TIndexedSet&&) = default;
	TIndexedSet(const TIndexedSet&) = default;
	TIndexedSet& operator=(TIndexedSet&&) = default;
	TIndexedSet& operator=(const TIndexedSet&) = default;
#else
	FORCEINLINE TIndexedSet() {}
	FORCEINLINE TIndexedSet(      TIndexedSet&& Other) : Map(MoveTemp(Other.Map)), Data(MoveTemp(Other.Data)) {}
	FORCEINLINE TIndexedSet(const TIndexedSet&  Other) : Map( Other.Map ), Data( Other.Data ) {}
	FORCEINLINE TIndexedSet& operator=(      TIndexedSet&& Other) { Map = MoveTemp(Other.Map); Data = MoveTemp(Other.Data); return *this; }
	FORCEINLINE TIndexedSet& operator=(const TIndexedSet&  Other) { Map = Other.Map; Data = Other.Data; return *this; }
#endif
	
	int32 Add(Type const& Object)
	{
		int32* Index = Map.Find(Object);
		if(Index)
		{
			return *Index;
		}
		else
		{
			int32 NewIndex = Data.Num();
			Data.Push(Object);
			Map.Add(Object, NewIndex);
			return NewIndex;
		}
	}
	
	int32 FindIndex(Type const& Object) const
	{
		int32 const* Index = Map.Find(Object);
		if(Index)
		{
			return *Index;
		}
		else
		{
			return -1;
		}
	}
	
	int32 FindIndexChecked(Type const& Object)
	{
		return Map.FindChecked(Object);
	}

	bool Contains(Type const& Object) const
	{
		return Map.Contains(Object);
	}
	
	Type& operator[](int32 Index)
	{
		return Data[Index];
	}
	
	Type const& operator[](int32 Index) const
	{
		return Data[Index];
	}
	
	uint32 Num() const
	{
		return Data.Num();
	}
	
	friend FORCEINLINE FArchive& operator<<( FArchive& Ar, TIndexedSet& Set )
	{
		Ar << Set.Data;
		
		if (Ar.IsLoading())
		{
			Set.Map.Empty(Set.Data.Num());

			for (int32 i = 0; i < Set.Data.Num(); ++i)
			{
				Set.Map.Add(Set.Data[i], i);
			}
		}

		return Ar;
	}
};

struct FShaderPreDrawEntry
{
	int32 PSOIndex;
	int32 DrawKeyIndex;
	bool bPredrawn;
	
	FShaderPreDrawEntry() : PSOIndex(-1), DrawKeyIndex(-1), bPredrawn(false) {}
	
	friend bool operator ==(const FShaderPreDrawEntry& A,const FShaderPreDrawEntry& B)
	{
		return (A.PSOIndex == B.PSOIndex && A.DrawKeyIndex == B.DrawKeyIndex);
	}
	
	friend uint32 GetTypeHash(const FShaderPreDrawEntry &Key)
	{
		return (Key.PSOIndex ^ Key.DrawKeyIndex);
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderPreDrawEntry& Info )
	{
		if(Ar.IsLoading())
		{
			Info.bPredrawn = false;
		}
		return Ar << Info.PSOIndex << Info.DrawKeyIndex;
	}
};

struct FShaderPlatformCache
{		
	FShaderPlatformCache()
	: ShaderPlatform(SP_NumPlatforms)
	{}

	friend FArchive& operator<<( FArchive& Ar, FShaderPlatformCache& Info );

	EShaderPlatform ShaderPlatform;

	TIndexedSet<FShaderCacheKey> Shaders;
	TIndexedSet<FShaderCacheBoundState> BoundShaderStates;
	TIndexedSet<FShaderDrawKey> DrawStates;
	TIndexedSet<FShaderRenderTargetKey> RenderTargets;
	TIndexedSet<FShaderResourceKey> Resources;
	TIndexedSet<FSamplerStateInitializerRHI, FSamplerStateInitializerRHIKeyFuncs> SamplerStates;
	TIndexedSet<FShaderPreDrawEntry> PreDrawEntries;
	TIndexedSet<FShaderCacheGraphicsPipelineState> PipelineStates;
	
	TMap<int32, TSet<int32>> ShaderStateMembership;
	TMap<uint32, FShaderStreamingCache> StreamingDrawStates;
};

struct FShaderResourceViewBinding
{
	FShaderResourceViewBinding()
	: SRV(nullptr)
	, VertexBuffer(nullptr)
	, Texture(nullptr)
	{}
	
	FShaderResourceViewBinding(FShaderResourceViewRHIParamRef InSrv, FVertexBufferRHIParamRef InVb, FTextureRHIParamRef InTexture)
	: SRV(nullptr)
	, VertexBuffer(nullptr)
	, Texture(nullptr)
	{}
	
	FShaderResourceViewRHIParamRef SRV;
	FVertexBufferRHIParamRef VertexBuffer;
	FTextureRHIParamRef Texture;
};

struct FShaderTextureBinding
{
	FShaderTextureBinding() {}
	FShaderTextureBinding(FShaderResourceViewBinding const& Other)
	{
		operator=(Other);
	}
	
	FShaderTextureBinding& operator=(FShaderResourceViewBinding const& Other)
	{
		SRV = Other.SRV;
		VertexBuffer = Other.VertexBuffer;
		Texture = Other.Texture;
		return *this;
	}
	
	friend bool operator ==(const FShaderTextureBinding& A,const FShaderTextureBinding& B)
	{
		return A.SRV == B.SRV && A.VertexBuffer == B.VertexBuffer && A.Texture == B.Texture;
	}
	
	friend uint32 GetTypeHash(const FShaderTextureBinding &Key)
	{
		uint32 Hash = GetTypeHash(Key.SRV);
		Hash ^= GetTypeHash(Key.VertexBuffer);
		Hash ^= GetTypeHash(Key.Texture);
		return Hash;
	}
	
	FShaderResourceViewRHIRef SRV;
	FVertexBufferRHIRef VertexBuffer;
	FTextureRHIRef Texture;
};

//};
