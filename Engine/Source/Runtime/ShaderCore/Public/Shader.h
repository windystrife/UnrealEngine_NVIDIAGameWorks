// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Shader.h: Shader definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/List.h"
#include "Misc/SecureHash.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "RenderingThread.h"
#include "ShaderCore.h"
#include "Serialization/ArchiveProxy.h"

// For FShaderUniformBufferParameter

#if WITH_EDITOR
#include "UObject/DebugSerializationFlags.h"
#endif

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
#include "GFSDK_VXGI.h"
#endif
// NVCHANGE_END: Add VXGI

class FGlobalShaderType;
class FMaterialShaderType;
class FNiagaraShaderType;
class FMeshMaterialShaderType;
class FShaderPipelineType;
class FShaderType;
class FVertexFactoryParameterRef;
class FVertexFactoryType;

/** Used to identify the global shader map. */
extern SHADERCORE_API FSHAHash GGlobalShaderMapHash;

/** 
 * Uniquely identifies an FShaderResource.  
 * Used to link FShaders to FShaderResources on load. 
 */
class FShaderResourceId
{
public:

	FShaderResourceId() { SpecificShaderTypeName = nullptr; }

	FShaderResourceId(const FShaderCompilerOutput& Output, const TCHAR* InSpecificShaderTypeName) :
		Target(Output.Target),
		OutputHash(Output.OutputHash),
		SpecificShaderTypeName(InSpecificShaderTypeName)
	{}

	friend inline uint32 GetTypeHash( const FShaderResourceId& Id )
	{
		return FCrc::MemCrc_DEPRECATED((const void*)&Id.OutputHash, sizeof(Id.OutputHash));
	}

	friend bool operator==(const FShaderResourceId& X, const FShaderResourceId& Y)
	{
		return X.Target == Y.Target 
			&& X.OutputHash == Y.OutputHash 
			&& ((X.SpecificShaderTypeName == NULL && Y.SpecificShaderTypeName == NULL)
				|| (FCString::Strcmp(X.SpecificShaderTypeName, Y.SpecificShaderTypeName) == 0));
	}

	friend bool operator!=(const FShaderResourceId& X, const FShaderResourceId& Y)
	{
		return !(X == Y);
	}

	friend FArchive& operator<<(FArchive& Ar, FShaderResourceId& Id)
	{
		Ar << Id.Target << Id.OutputHash;

		if (Ar.IsSaving())
		{
			Id.SpecificShaderTypeStorage = Id.SpecificShaderTypeName ? Id.SpecificShaderTypeName : TEXT("");
		}

		Ar << Id.SpecificShaderTypeStorage;

		if (Ar.IsLoading())
		{
			Id.SpecificShaderTypeName = *Id.SpecificShaderTypeStorage;

			if (FCString::Strcmp(Id.SpecificShaderTypeName, TEXT("")) == 0)
			{
				// Store NULL for empty string to be consistent with FShaderResourceId's created at compile time
				Id.SpecificShaderTypeName = NULL;
			}
		}

		return Ar;
	}

	/** Target platform and frequency. */
	FShaderTarget Target;

	/** Hash of the compiled shader output, which is used to create the FShaderResource. */
	FSHAHash OutputHash;

	/** NULL if type doesn't matter, otherwise the name of the type that this was created specifically for, which is used with geometry shader stream out. */
	const TCHAR* SpecificShaderTypeName;

	/** Stores the memory for SpecificShaderTypeName if this is a standalone Id, otherwise is empty and SpecificShaderTypeName points to an FShaderType name. */
	FString SpecificShaderTypeStorage;
};

/** 
 * Compiled shader bytecode and its corresponding RHI resource. 
 * This can be shared by multiple FShaders with identical compiled output.
 */
class FShaderResource : public FRenderResource, public FDeferredCleanupInterface
{
	friend class FShader;
public:

	/** Constructor used for deserialization. */
	SHADERCORE_API FShaderResource();

	/** Constructor used when creating a new shader resource from compiled output. */
	FShaderResource(const FShaderCompilerOutput& Output, FShaderType* InSpecificType);

	~FShaderResource();

	SHADERCORE_API void Serialize(FArchive& Ar);

	// Reference counting.
	SHADERCORE_API void AddRef();
	SHADERCORE_API void Release();

	SHADERCORE_API void Register();

	/** @return the shader's vertex shader */
	FORCEINLINE const FVertexShaderRHIParamRef GetVertexShader()
	{
		checkSlow(Target.Frequency == SF_Vertex);
		if (!IsInitialized())
		{
			InitializeShaderRHI();
		}
		return VertexShader;
	}
	/** @return the shader's pixel shader */
	FORCEINLINE const FPixelShaderRHIParamRef GetPixelShader()
	{
		checkSlow(Target.Frequency == SF_Pixel);
		if (!IsInitialized())
		{
			InitializeShaderRHI();
		}
		return PixelShader;
	}
	/** @return the shader's hull shader */
	FORCEINLINE const FHullShaderRHIParamRef GetHullShader()
	{
		checkSlow(Target.Frequency == SF_Hull);
		if (!IsInitialized())
		{
			InitializeShaderRHI();
		}
		return HullShader;
	}
	/** @return the shader's domain shader */
	FORCEINLINE const FDomainShaderRHIParamRef GetDomainShader()
	{
		checkSlow(Target.Frequency == SF_Domain);
		if (!IsInitialized())
		{
			InitializeShaderRHI();
		}
		return DomainShader;
	}
	/** @return the shader's geometry shader */
	FORCEINLINE const FGeometryShaderRHIParamRef GetGeometryShader()
	{
		checkSlow(Target.Frequency == SF_Geometry);
		if (!IsInitialized())
		{
			InitializeShaderRHI();
		}
		return GeometryShader;
	}
	/** @return the shader's compute shader */
	FORCEINLINE const FComputeShaderRHIParamRef GetComputeShader()
	{
		checkSlow(Target.Frequency == SF_Compute);
		if (!IsInitialized())
		{
			InitializeShaderRHI();
		}
		return ComputeShader;
	}

	SHADERCORE_API FShaderResourceId GetId() const;

	uint32 GetSizeBytes() const
	{
		return Code.GetAllocatedSize() + sizeof(FShaderResource);
	}

	// FRenderResource interface.
	virtual void InitRHI();
	virtual void ReleaseRHI();

	// FDeferredCleanupInterface implementation.
	virtual void FinishCleanup();

	/** Finds a matching shader resource in memory if possible. */
	SHADERCORE_API static FShaderResource* FindShaderResourceById(const FShaderResourceId& Id);

	/** 
	 * Finds a matching shader resource in memory or creates a new one with the given compiler output.  
	 * SpecificType can be NULL
	 */
	SHADERCORE_API static FShaderResource* FindOrCreateShaderResource(const FShaderCompilerOutput& Output, class FShaderType* SpecificType);

	/** Return a list of all shader Ids currently known */
	SHADERCORE_API static void GetAllShaderResourceId(TArray<FShaderResourceId>& Ids);

	/** Returns true if and only if TargetPlatform is compatible for use with CurrentPlatform. */
	SHADERCORE_API static bool ArePlatformsCompatible(EShaderPlatform CurrentPlatform, EShaderPlatform TargetPlatform);

	void GetShaderCode(TArray<uint8>& OutCode) const
	{
		UncompressCode(OutCode);
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	VXGI::IUserDefinedShaderSet* GetVxgiVoxelizationPixelShaderSet();
	//This works on VS or DS if they are compiled with the right flag
	VXGI::IUserDefinedShaderSet* GetVxgiVoxelizationGeometryShaderSet();
	VXGI::IUserDefinedShaderSet* GetVxgiConeTracingPixelShaderSet();

	const TArray<FShaderParameterMap>& GetParameterMapsForVxgiPS() const { return ParameterMapForVxgiPSPermutation; }
#endif
	// NVCHANGE_END: Add VXGI

private:
	// compression functions
	void UncompressCode(TArray<uint8>& UncompressedCode) const;
	void CompressCode(const TArray<uint8>& UncompressedCode);
	

	/** Conditionally serialize shader code. */
	void SerializeShaderCode(FArchive& Ar);

	/** Reference to the RHI shader.  Only one of these is ever valid, and it is the one corresponding to Target.Frequency. */
	FVertexShaderRHIRef VertexShader;
	FPixelShaderRHIRef PixelShader;
	FHullShaderRHIRef HullShader;
	FDomainShaderRHIRef DomainShader;
	FGeometryShaderRHIRef GeometryShader;
	FComputeShaderRHIRef ComputeShader;

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	VXGI::IUserDefinedShaderSet* VxgiVoxelizationPixelShader;
	VXGI::IUserDefinedShaderSet* VxgiVoxelizationGeometryShader;
	VXGI::IUserDefinedShaderSet* VxgiConeTracingPixelShader;
	bool bIsVxgiPS;
	TArray<FShaderParameterMap> ParameterMapForVxgiPSPermutation;
	TArray<TArray<uint8> > ShaderResouceTableVxgiPSPermutation;
	TArray<bool> UsesGlobalCBForVxgiPSPermutation;
	TArray<uint8> VxgiGSCode;
#endif
	// NVCHANGE_END: Add VXGI

	/** Target platform and frequency. */
	FShaderTarget Target;

	/** Compiled bytecode. */
	TArray<uint8> Code;

	/** Original bytecode size, before compression */
	uint32 UncompressedCodeSize = 0;

	/**
	 * Hash of the compiled bytecode and the generated parameter map.
	 * This is used to find existing shader resources in memory or the DDC.
	 */
	FSHAHash OutputHash;

	/** If not NULL, the shader type this resource must be used with. */
	class FShaderType* SpecificType;

	/** The number of instructions the shader takes to execute. */
	uint32 NumInstructions;

	/** Number of texture samplers the shader uses. */
	uint32 NumTextureSamplers;

	/** The number of references to this shader. */
	mutable uint32 NumRefs;

	/** A 'canary' used to detect when a stale shader resource is being rendered with. */
	uint32 Canary;
	

	/** Whether the shader code is stored in a shader library. */
	bool bCodeInSharedLocation;

	/** Initialize the shader RHI resources. */
	SHADERCORE_API void InitializeShaderRHI();

	/** Tracks loaded shader resources by id. */
	static TMap<FShaderResourceId, FShaderResource*> ShaderResourceIdMap;
	/** Critical section for ShaderResourceIdMap. */
	static FCriticalSection ShaderResourceIdMapCritical;
};

/** Encapsulates information about a shader's serialization behavior, used to detect when C++ serialization changes to auto-recompile. */
class FSerializationHistory
{
public: 

	/** Token stream stored as uint32's.  Each token is 4 bits, with a 0 meaning there's an associated 32 bit value in FullLengths. */
	TArray<uint32> TokenBits;

	/** Number of tokens in TokenBits. */
	int32 NumTokens;

	/** Full size length entries. One of these are used for every token with a value of 0. */
	TArray<uint32> FullLengths;

	FSerializationHistory() :
		NumTokens(0)
	{}

	void AddValue(uint32 InValue)
	{
		const int32 UIntIndex = NumTokens / 8; 

		if (UIntIndex >= TokenBits.Num())
		{
			// Add another uint32 if needed
			TokenBits.AddZeroed();
		}

		uint8 Token = InValue;

		// Anything that does not fit in 4 bits needs to go into FullLengths, with a special token value of 0
		if (InValue > 7)
		{
			Token = 0;
			FullLengths.Add(InValue);
		}

		const uint32 Shift = (NumTokens % 8) * 4;
		// Add the new token bits into the existing uint32
		TokenBits[UIntIndex] = TokenBits[UIntIndex] | (Token << Shift);
		NumTokens++;
	}

	uint8 GetToken(int32 Index) const
	{
		check(Index < NumTokens);
		const int32 UIntIndex = Index / 8; 
		check(UIntIndex < TokenBits.Num());
		const uint32 Shift = (Index % 8) * 4;
		const uint8 Token = (TokenBits[UIntIndex] >> Shift) & 0xF;
		return Token;
	}

	void AppendKeyString(FString& KeyString) const
	{
		KeyString += FString::FromInt(NumTokens);
		KeyString += BytesToHex((uint8*)TokenBits.GetData(), TokenBits.Num() * TokenBits.GetTypeSize());
		KeyString += BytesToHex((uint8*)FullLengths.GetData(), FullLengths.Num() * FullLengths.GetTypeSize());
	}

	inline bool operator==(const FSerializationHistory& Other) const
	{
		return TokenBits == Other.TokenBits && NumTokens == Other.NumTokens && FullLengths == Other.FullLengths;
	}

	friend FArchive& operator<<(FArchive& Ar,class FSerializationHistory& Ref)
	{
		Ar << Ref.TokenBits << Ref.NumTokens << Ref.FullLengths;
		return Ar;
	}
};

/** 
 * Uniquely identifies an FShader.  
 * Used to link FMaterialShaderMaps and FShaders on load. 
 */
class FShaderId
{
public:

	/** 
	 * Hash of the material shader map Id, since this shader depends on the generated material code from that shader map.
	 * A hash is used instead of the full shader map Id to shorten the key length, even though this will result in a hash being hashed when we make a DDC key. 
	 */ 
	FSHAHash MaterialShaderMapHash;

	/** Shader Pipeline linked to this shader, needed since a single shader might be used on different Pipelines. */
	const FShaderPipelineType* ShaderPipeline;

	/** 
	 * Vertex factory type that the shader was created for, 
	 * This is needed in the Id since a single shader type will be compiled for multiple vertex factories within a material shader map.
	 * Will be NULL for global shaders.
	 */
	FVertexFactoryType* VertexFactoryType;

	/** Used to detect changes to the vertex factory source files. */
	FSHAHash VFSourceHash;

	/** 
	 * Used to detect changes to the vertex factory parameter class serialization, or NULL for global shaders. 
	 * Note: This is referencing memory in the VF Type, since it is the same for all shaders using that VF Type.
	 */
	const FSerializationHistory* VFSerializationHistory;

	/** Shader type */
	FShaderType* ShaderType;

	/** Used to detect changes to the shader source files. */
	FSHAHash SourceHash;

	/** Used to detect changes to the shader serialization.  Note: this is referencing memory in the FShaderType. */
	const FSerializationHistory& SerializationHistory;

	/** Shader platform and frequency. */
	FShaderTarget Target;

	/** Create a minimally initialized Id.  Members will have to be assigned individually. */
	FShaderId(const FSerializationHistory& InSerializationHistory) : 
		SerializationHistory(InSerializationHistory)
	{}

	/** Creates an Id for the given material, vertex factory, shader type and target. */
	SHADERCORE_API FShaderId(const FSHAHash& InMaterialShaderMapHash, const FShaderPipelineType* InShaderPipeline, FVertexFactoryType* InVertexFactoryType, FShaderType* InShaderType, FShaderTarget InTarget);

	friend inline uint32 GetTypeHash( const FShaderId& Id )
	{
		return FCrc::MemCrc_DEPRECATED((const void*)&Id.MaterialShaderMapHash, sizeof(Id.MaterialShaderMapHash));
	}

	friend bool operator==(const FShaderId& X, const FShaderId& Y)
	{
		return X.MaterialShaderMapHash == Y.MaterialShaderMapHash
			&& X.ShaderPipeline == Y.ShaderPipeline
			&& X.VertexFactoryType == Y.VertexFactoryType
			&& X.VFSourceHash == Y.VFSourceHash
			&& ((X.VFSerializationHistory == NULL && Y.VFSerializationHistory == NULL)
				|| (X.VFSerializationHistory != NULL && Y.VFSerializationHistory != NULL &&
					*X.VFSerializationHistory == *Y.VFSerializationHistory))
			&& X.ShaderType == Y.ShaderType 
			&& X.SourceHash == Y.SourceHash 
			&& X.SerializationHistory == Y.SerializationHistory
			&& X.Target == Y.Target;
	}

	friend bool operator!=(const FShaderId& X, const FShaderId& Y)
	{
		return !(X == Y);
	}
};

/** Self contained version of FShaderId, which is useful for serializing. */
class FSelfContainedShaderId
{
public:

	/** 
	 * Hash of the material shader map Id, since this shader depends on the generated material code from that shader map.
	 * A hash is used instead of the full shader map Id to shorten the key length, even though this will result in a hash being hashed when we make a DDC key. 
	 */ 
	FSHAHash MaterialShaderMapHash;

	/** 
	 * Name of the vertex factory type that the shader was created for, 
	 * This is needed in the Id since a single shader type will be compiled for multiple vertex factories within a material shader map.
	 * Will be the empty string for global shaders.
	 */
	FString VertexFactoryTypeName;

	// Required to differentiate amongst unique shaders in the global map per Type
	FString ShaderPipelineName;

	/** Used to detect changes to the vertex factory source files. */
	FSHAHash VFSourceHash;

	/** Used to detect changes to the vertex factory parameter class serialization, or empty for global shaders. */
	FSerializationHistory VFSerializationHistory;

	/** Shader type name */
	FString ShaderTypeName;

	/** Used to detect changes to the shader source files. */
	FSHAHash SourceHash;

	/** Used to detect changes to the shader serialization. */
	FSerializationHistory SerializationHistory;

	/** Shader platform and frequency. */
	FShaderTarget Target;

	SHADERCORE_API FSelfContainedShaderId();

	SHADERCORE_API FSelfContainedShaderId(const FShaderId& InShaderId);

	SHADERCORE_API bool IsValid();

	SHADERCORE_API friend FArchive& operator<<(FArchive& Ar,class FSelfContainedShaderId& Ref);
};

class FMaterial;

/** A compiled shader and its parameter bindings. */
class SHADERCORE_API FShader : public FDeferredCleanupInterface
{
	friend class FShaderType;
public:

	struct CompiledShaderInitializerType
	{
		FShaderType* Type;
		FShaderTarget Target;
		const TArray<uint8>& Code;
		const FShaderParameterMap& ParameterMap;
		const FSHAHash& OutputHash;
		FShaderResource* Resource;
		FSHAHash MaterialShaderMapHash;
		const FShaderPipelineType* ShaderPipeline;
		FVertexFactoryType* VertexFactoryType;

		CompiledShaderInitializerType(
			FShaderType* InType,
			const FShaderCompilerOutput& CompilerOutput,
			FShaderResource* InResource,
			const FSHAHash& InMaterialShaderMapHash,
			const FShaderPipelineType* InShaderPipeline,
			FVertexFactoryType* InVertexFactoryType
			):
			Type(InType),
			Target(CompilerOutput.Target),
			Code(CompilerOutput.ShaderCode.GetReadAccess()),
			ParameterMap(CompilerOutput.ParameterMap),
			OutputHash(CompilerOutput.OutputHash),
			Resource(InResource),
			MaterialShaderMapHash(InMaterialShaderMapHash),
			ShaderPipeline(InShaderPipeline),
			VertexFactoryType(InVertexFactoryType)
		{}
	};

	/** 
	 * Used to construct a shader for deserialization.
	 * This still needs to initialize members to safe values since FShaderType::GenerateSerializationHistory uses this constructor.
	 */
	FShader();

	/**
	 * Construct a shader from shader compiler output.
	 */
	FShader(const CompiledShaderInitializerType& Initializer);

	virtual ~FShader();

/*
	/ ** Can be overridden by FShader subclasses to modify their compile environment just before compilation occurs. * /
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment){}
*/

	/** Serializes the shader. */
	bool SerializeBase(FArchive& Ar, bool bShadersInline);

	virtual bool Serialize(FArchive& Ar) { return false; }

	// Reference counting.
	void AddRef();
	void Release();

	/** Registers this shader for lookup by ID. */
	void Register();

	/** Removes this shader from the ID lookup map. */
	void Deregister();

	/** Returns the hash of the shader file that this shader was compiled with. */
	const FSHAHash& GetHash() const;
	
	/** @return If the shader is linked with a vertex factory, returns the vertex factory's parameter object. */
	virtual const FVertexFactoryParameterRef* GetVertexFactoryParameterRef() const { return NULL; }

	/** @return the shader's vertex shader */
	inline const FVertexShaderRHIParamRef GetVertexShader()
	{
		return Resource->GetVertexShader();
	}
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	virtual // We need to make GetPixelShader virtual to override it in TVXGIVoxelizationShaderPermutationPS
#endif
	// NVCHANGE_END: Add VXGI
	/** @return the shader's pixel shader */
	inline const FPixelShaderRHIParamRef GetPixelShader()
	{
		return Resource->GetPixelShader();
	}
	/** @return the shader's hull shader */
	inline const FHullShaderRHIParamRef GetHullShader()
	{
		return Resource->GetHullShader();
	}
	/** @return the shader's domain shader */
	inline const FDomainShaderRHIParamRef GetDomainShader()
	{
		return Resource->GetDomainShader();
	}
	/** @return the shader's geometry shader */
	inline const FGeometryShaderRHIParamRef GetGeometryShader()
	{
		return Resource->GetGeometryShader();
	}
	/** @return the shader's compute shader */
	inline const FComputeShaderRHIParamRef GetComputeShader()
	{
		return Resource->GetComputeShader();
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	VXGI::IUserDefinedShaderSet* GetVxgiVoxelizationPixelShaderSet()
	{
		return Resource->GetVxgiVoxelizationPixelShaderSet();
	}
	//This works on VS or DS if they are compiled with the right flag
	VXGI::IUserDefinedShaderSet* GetVxgiVoxelizationGeometryShaderSet()
	{
		return Resource->GetVxgiVoxelizationGeometryShaderSet();
	}
	VXGI::IUserDefinedShaderSet* GetVxgiConeTracingPixelShaderSet()
	{
		return Resource->GetVxgiConeTracingPixelShaderSet();
	}
#endif
	// NVCHANGE_END: Add VXGI
	
	// Accessors.
	inline FShaderType* GetType() const { return Type; }
	inline uint32 GetNumInstructions() const { return Resource->NumInstructions; }
	inline uint32 GetNumTextureSamplers() const { return Resource->NumTextureSamplers; }
	inline const TArray<uint8>& GetCode() const { return Resource->Code; }
	inline const FShaderTarget GetTarget() const { return Target; }
	inline FSHAHash GetOutputHash() const { return OutputHash; }
	FShaderId GetId() const;
	inline FVertexFactoryType* GetVertexFactoryType() const { return VFType; }
	inline FSHAHash GetMaterialShaderMapHash() const { return MaterialShaderMapHash; }
	inline int32 GetNumRefs() const { return NumRefs; }

	inline FShaderResourceId GetResourceId() const
	{
		return Resource->GetId();
	}

	inline uint32 GetSizeBytes() const
	{
		return GetTypeSize() + GetAllocatedSize();
	}
	
	/** Returns the size of the concrete type of this shader. */
	virtual uint32 GetTypeSize() const
	{
		return sizeof(*this);
	}

	/** Returns the size of all allocations owned by this shader, e.g. TArrays. */
	virtual uint32 GetAllocatedSize() const
	{
		return UniformBufferParameters.GetAllocatedSize() + UniformBufferParameterStructs.GetAllocatedSize();
	}

	uint32 GetResourceSizeBytes() const
	{
		return Resource->GetSizeBytes();
	}

	void SetResource(FShaderResource* InResource);

	/** Called from the main thread to register and set the serialized resource */
	void RegisterSerializedResource();

	// FDeferredCleanupInterface implementation.
	virtual void FinishCleanup();

	/** Implement for geometry shaders that want to use stream out. */
	static void GetStreamOutElements(FStreamOutElementList& ElementList, TArray<uint32>& StreamStrides, int32& RasterizedStream) {}

	void BeginInitializeResources()
	{
		BeginInitResource(Resource);
	}

	/** Finds an automatically bound uniform buffer matching the given uniform buffer type if one exists, or returns an unbound parameter. */
	template<typename UniformBufferStructType>
	FORCEINLINE_DEBUGGABLE const TShaderUniformBufferParameter<UniformBufferStructType>& GetUniformBufferParameter() const
	{
		FUniformBufferStruct* SearchStruct = &UniformBufferStructType::StaticStruct;
		int32 FoundIndex = INDEX_NONE;

		for (int32 StructIndex = 0, Count = UniformBufferParameterStructs.Num(); StructIndex < Count; StructIndex++)
		{
			if (UniformBufferParameterStructs[StructIndex] == SearchStruct)
			{
				FoundIndex = StructIndex;
				break;
			}
		}

		if (FoundIndex != INDEX_NONE)
		{
			const TShaderUniformBufferParameter<UniformBufferStructType>& FoundParameter = (const TShaderUniformBufferParameter<UniformBufferStructType>&)*UniformBufferParameters[FoundIndex];
			FoundParameter.SetParametersId = SetParametersId;
			return FoundParameter;
		}
		else
		{
			// This can happen if the uniform buffer was not bound
			// There's no good way to distinguish not being bound due to temporary debugging / compiler optimizations or an actual code bug,
			// Hence failing silently instead of an error message
			static TShaderUniformBufferParameter<UniformBufferStructType> UnboundParameter;
			UnboundParameter.SetInitialized();
			return UnboundParameter;
		}
	}

	/** Finds an automatically bound uniform buffer matching the given uniform buffer struct if one exists, or returns an unbound parameter. */
	const FShaderUniformBufferParameter& GetUniformBufferParameter(FUniformBufferStruct* SearchStruct) const
	{
		int32 FoundIndex = INDEX_NONE;

		for (int32 StructIndex = 0, Count = UniformBufferParameterStructs.Num(); StructIndex < Count; StructIndex++)
		{
			if (UniformBufferParameterStructs[StructIndex] == SearchStruct)
			{
				FoundIndex = StructIndex;
				break;
			}
		}

		if (FoundIndex != INDEX_NONE)
		{
			const FShaderUniformBufferParameter& FoundParameter = *UniformBufferParameters[FoundIndex];
			FoundParameter.SetParametersId = SetParametersId;
			return FoundParameter;
		}
		else
		{
			static FShaderUniformBufferParameter UnboundParameter;
			UnboundParameter.SetInitialized();
			return UnboundParameter;
		}
	}

	/** Checks that the shader is valid by asserting the canary value is set as expected. */
	inline void CheckShaderIsValid() const;

	/** Checks that the shader is valid and returns itself. */
	inline FShader* GetShaderChecked()
	{
		CheckShaderIsValid();
		return this;
	}

	/** Discards the serialized resource, used when the engine is using NullRHI */
	void DiscardSerializedResource()
	{
		delete SerializedResource;
		SerializedResource = nullptr;
	}

protected:

	/** Indexed the same as UniformBufferParameters.  Packed densely for coherent traversal. */
	TArray<FUniformBufferStruct*> UniformBufferParameterStructs;
	TArray<FShaderUniformBufferParameter*> UniformBufferParameters;

private:

	/** 
	 * Hash of the compiled output from this shader and the resulting parameter map.  
	 * This is used to find a matching resource.
	 */
	FSHAHash OutputHash;

	/** Pointer to the shader resource that has been serialized from disk, to be registered on the main thread later. */
	FShaderResource* SerializedResource;

	/** Reference to the shader resource, which stores the compiled bytecode and the RHI shader resource. */
	TRefCountPtr<FShaderResource> Resource;

	/** Hash of the material shader map this shader belongs to, stored so that an FShaderId can be constructed from this shader. */
	FSHAHash MaterialShaderMapHash;

	/** Shader pipeline this shader belongs to, stored so that an FShaderId can be constructed from this shader. */
	const FShaderPipelineType* ShaderPipeline;

	/** Vertex factory type this shader was created for, stored so that an FShaderId can be constructed from this shader. */
	FVertexFactoryType* VFType;

	/** Vertex factory source hash, stored so that an FShaderId can be constructed from this shader. */
	FSHAHash VFSourceHash;

	/** Shader Type metadata for this shader. */
	FShaderType* Type;

	/** Hash of this shader's source files generated at compile time, and stored to allow creating an FShaderId. */
	FSHAHash SourceHash;

	/** Target platform and frequency. */
	FShaderTarget Target;

	/** The number of references to this shader. */
	mutable uint32 NumRefs;

	/** Transient value used to track when this shader's automatically bound uniform buffer parameters were set last. */
	mutable uint32 SetParametersId;

	/** A 'canary' used to detect when a stale shader is being rendered with. */
	uint32 Canary;

public:
	/** Canary is set to this if the FShader is a valid pointer but uninitialized. */
	static const uint32 ShaderMagic_Uninitialized = 0xbd9922df;
	/** Canary is set to this if the FShader is a valid pointer but in the process of being cleaned up. */
	static const uint32 ShaderMagic_CleaningUp = 0xdc67f93b;
	/** Canary is set to this if the FShader is a valid pointer and initialized. */
	static const uint32 ShaderMagic_Initialized = 0x335b43ab;
};

/** An object which is used to serialize/deserialize, compile, and cache a particular shader class. */
class SHADERCORE_API FShaderType
{
public:
	enum class EShaderTypeForDynamicCast : uint32
	{
		Global,
		Material,
		MeshMaterial,
		Niagara
	};

	typedef class FShader* (*ConstructSerializedType)();
	typedef void (*GetStreamOutElementsType)(FStreamOutElementList& ElementList, TArray<uint32>& StreamStrides, int32& RasterizedStream);

	/** @return The global shader factory list. */
	static TLinkedList<FShaderType*>*& GetTypeList();

	static FShaderType* GetShaderTypeByName(const TCHAR* Name);
	static TArray<FShaderType*> GetShaderTypesByFilename(const TCHAR* Filename);

	/** @return The global shader name to type map */
	static TMap<FName, FShaderType*>& GetNameToTypeMap();

	/** Gets a list of FShaderTypes whose source file no longer matches what that type was compiled with */
	static void GetOutdatedTypes(TArray<FShaderType*>& OutdatedShaderTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes);

	/** Returns true if the source file no longer matches what that type was compiled with */
	bool GetOutdatedCurrentType(TArray<FShaderType*>& OutdatedShaderTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes) const;

	/** Initialize FShaderType static members, this must be called before any shader types are created. */
	static void Initialize(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables);

	/** Uninitializes FShaderType cached data. */
	static void Uninitialize();

	/** Minimal initialization constructor. */
	FShaderType(
		EShaderTypeForDynamicCast InShaderTypeForDynamicCast,
		const TCHAR* InName,
		const TCHAR* InSourceFilename,
		const TCHAR* InFunctionName,
		uint32 InFrequency,
		ConstructSerializedType InConstructSerializedRef,
		GetStreamOutElementsType InGetStreamOutElementsRef);

	virtual ~FShaderType();

	/**
	 * Finds a shader of this type by ID.
	 * @return NULL if no shader with the specified ID was found.
	 */
	FShader* FindShaderById(const FShaderId& Id);

	/** Constructs a new instance of the shader type for deserialization. */
	FShader* ConstructForDeserialization() const;

	/** Calculates a Hash based on this shader type's source code and includes */
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	virtual //We need to override this to add the VXGI hash
#endif
	// NVCHANGE_END: Add VXGI
	const FSHAHash& GetSourceHash() const;

	/** Serializes a shader type reference by name. */
	SHADERCORE_API friend FArchive& operator<<(FArchive& Ar,FShaderType*& Ref);
	
	/** Hashes a pointer to a shader type. */
	friend uint32 GetTypeHash(FShaderType* Ref)
	{
		return Ref ? Ref->HashIndex : 0;
	}

	// Dynamic casts.
	FORCEINLINE FGlobalShaderType* GetGlobalShaderType() 
	{ 
		return (ShaderTypeForDynamicCast == EShaderTypeForDynamicCast::Global) ? reinterpret_cast<FGlobalShaderType*>(this) : nullptr;
	}
	FORCEINLINE const FGlobalShaderType* GetGlobalShaderType() const
	{
		return (ShaderTypeForDynamicCast == EShaderTypeForDynamicCast::Global) ? reinterpret_cast<const FGlobalShaderType*>(this) : nullptr;
	}
	FORCEINLINE FMaterialShaderType* GetMaterialShaderType()
	{
		return (ShaderTypeForDynamicCast == EShaderTypeForDynamicCast::Material) ? reinterpret_cast<FMaterialShaderType*>(this) : nullptr;
	}
	FORCEINLINE const FMaterialShaderType* GetMaterialShaderType() const
	{
		return (ShaderTypeForDynamicCast == EShaderTypeForDynamicCast::Material) ? reinterpret_cast<const FMaterialShaderType*>(this) : nullptr;
	}
	FORCEINLINE FMeshMaterialShaderType* GetMeshMaterialShaderType()
	{
		return (ShaderTypeForDynamicCast == EShaderTypeForDynamicCast::MeshMaterial) ? reinterpret_cast<FMeshMaterialShaderType*>(this) : nullptr;
	}
	FORCEINLINE const FMeshMaterialShaderType* GetMeshMaterialShaderType() const
	{
		return (ShaderTypeForDynamicCast == EShaderTypeForDynamicCast::MeshMaterial) ? reinterpret_cast<const FMeshMaterialShaderType*>(this) : nullptr;
	}
	FORCEINLINE const FNiagaraShaderType* GetNiagaraShaderType() const
	{
		return (ShaderTypeForDynamicCast == EShaderTypeForDynamicCast::Niagara) ? reinterpret_cast<const FNiagaraShaderType*>(this) : nullptr;
	}
	FORCEINLINE FNiagaraShaderType* GetNiagaraShaderType()
	{
		return (ShaderTypeForDynamicCast == EShaderTypeForDynamicCast::Niagara) ? reinterpret_cast<FNiagaraShaderType*>(this) : nullptr;
	}

	// Accessors.
	inline EShaderFrequency GetFrequency() const
	{ 
		return (EShaderFrequency)Frequency; 
	}
	inline const TCHAR* GetName() const
	{ 
		return Name; 
	}
	inline const FName& GetFName() const
	{
		return TypeName;
	}
	inline const TCHAR* GetShaderFilename() const
	{ 
		return SourceFilename; 
	}
	inline const TCHAR* GetFunctionName() const
	{
		return FunctionName;
	}
	inline int32 GetNumShaders() const
	{
		return ShaderIdMap.Num();
	}
	inline const FSerializationHistory& GetSerializationHistory() const
	{
		return SerializationHistory;
	}

	inline const TMap<const TCHAR*, FCachedUniformBufferDeclaration>& GetReferencedUniformBufferStructsCache() const
	{
		return ReferencedUniformBufferStructsCache;
	}

	/** Adds include statements for uniform buffers that this shader type references, and builds a prefix for the shader file with the include statements. */
	void AddReferencedUniformBufferIncludes(FShaderCompilerEnvironment& OutEnvironment, FString& OutSourceFilePrefix, EShaderPlatform Platform);

	void FlushShaderFileCache(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
	{
		ReferencedUniformBufferStructsCache.Empty();
		GenerateReferencedUniformBuffers(SourceFilename, Name, ShaderFileToUniformBufferVariables, ReferencedUniformBufferStructsCache);

		for (int32 Platform = 0; Platform < SP_NumPlatforms; Platform++)
		{
			bCachedUniformBufferStructDeclarations[Platform] = false;
		}
	}

	void AddToShaderIdMap(FShaderId Id, FShader* Shader)
	{
		check(IsInGameThread());
		ShaderIdMap.Add(Id, Shader);
	}

	inline void RemoveFromShaderIdMap(FShaderId Id)
	{
		check(IsInGameThread());
		ShaderIdMap.Remove(Id);
	}

	bool LimitShaderResourceToThisType() const
	{
		return GetStreamOutElementsRef != &FShader::GetStreamOutElements;
	}

	void GetStreamOutElements(FStreamOutElementList& ElementList, TArray<uint32>& StreamStrides, int32& RasterizedStream) 
	{
		(*GetStreamOutElementsRef)(ElementList, StreamStrides, RasterizedStream);
	}

private:
	EShaderTypeForDynamicCast ShaderTypeForDynamicCast;
	uint32 HashIndex;
	const TCHAR* Name;
	FName TypeName;
	const TCHAR* SourceFilename;
	const TCHAR* FunctionName;
	uint32 Frequency;

	ConstructSerializedType ConstructSerializedRef;
	GetStreamOutElementsType GetStreamOutElementsRef;

	/** A map from shader ID to shader.  A shader will be removed from it when deleted, so this doesn't need to use a TRefCountPtr. */
	TMap<FShaderId,FShader*> ShaderIdMap;

	TLinkedList<FShaderType*> GlobalListLink;

	// DumpShaderStats needs to access ShaderIdMap.
	friend void SHADERCORE_API DumpShaderStats( EShaderPlatform Platform, EShaderFrequency Frequency );

	/** 
	 * Cache of referenced uniform buffer includes.  
	 * These are derived from source files so they need to be flushed when editing and recompiling shaders on the fly. 
	 * FShaderType::Initialize will add an entry for each referenced uniform buffer, but the declarations are added on demand as shaders are compiled.
	 */
	TMap<const TCHAR*, FCachedUniformBufferDeclaration> ReferencedUniformBufferStructsCache;

	/** Tracks what platforms ReferencedUniformBufferStructsCache has had declarations cached for. */
	bool bCachedUniformBufferStructDeclarations[SP_NumPlatforms];

	/** 
	 * Stores a history of serialization sizes for this shader type. 
	 * This is used to invalidate shaders when serialization changes.
	 */
	FSerializationHistory SerializationHistory;

	/** Tracks whether serialization history for all shader types has been initialized. */
	static bool bInitializedSerializationHistory;
};

/**
 * A macro to declare a new shader type.  This should be called in the class body of the new shader type.
 * @param ShaderClass - The name of the class representing an instance of the shader type.
 * @param ShaderMetaTypeShortcut - The shortcut for the shader meta type: simple, material, meshmaterial, etc.  The shader meta type
 *	controls 
 */
#define DECLARE_EXPORTED_SHADER_TYPE(ShaderClass,ShaderMetaTypeShortcut,RequiredAPI) \
	public: \
	typedef F##ShaderMetaTypeShortcut##ShaderType ShaderMetaType; \
	static RequiredAPI ShaderMetaType StaticType; \
	static FShader* ConstructSerializedInstance() { return new ShaderClass(); } \
	static FShader* ConstructCompiledInstance(const ShaderMetaType::CompiledShaderInitializerType& Initializer) \
	{ return new ShaderClass(Initializer); } \
	virtual uint32 GetTypeSize() const override { return sizeof(*this); }
#define DECLARE_SHADER_TYPE(ShaderClass,ShaderMetaTypeShortcut) \
	DECLARE_EXPORTED_SHADER_TYPE(ShaderClass,ShaderMetaTypeShortcut,)

#if !UE_BUILD_DOCS
/** A macro to implement a shader type. */
#define IMPLEMENT_SHADER_TYPE(TemplatePrefix,ShaderClass,SourceFilename,FunctionName,Frequency) \
	TemplatePrefix \
	ShaderClass::ShaderMetaType ShaderClass::StaticType( \
		TEXT(#ShaderClass), \
		SourceFilename, \
		FunctionName, \
		Frequency, \
		ShaderClass::ConstructSerializedInstance, \
		ShaderClass::ConstructCompiledInstance, \
		ShaderClass::ModifyCompilationEnvironment, \
		ShaderClass::ShouldCache, \
		ShaderClass::GetStreamOutElements \
		);

/** A macro to implement a shader type. Shader name is got from GetDebugName(), which is helpful for templated shaders. */
#define IMPLEMENT_SHADER_TYPE_WITH_DEBUG_NAME(TemplatePrefix,ShaderClass,SourceFilename,FunctionName,Frequency) \
	TemplatePrefix \
	typename ShaderClass::ShaderMetaType ShaderClass::StaticType( \
		ShaderClass::GetDebugName(), \
		SourceFilename, \
		FunctionName, \
		Frequency, \
		ShaderClass::ConstructSerializedInstance, \
		ShaderClass::ConstructCompiledInstance, \
		ShaderClass::ModifyCompilationEnvironment, \
		ShaderClass::ShouldCache, \
		ShaderClass::GetStreamOutElements \
		);

/** A macro to implement a templated shader type, the function name and the source filename comes from the class. */
#define IMPLEMENT_SHADER_TYPE2(ShaderClass,Frequency) \
	template<> \
	ShaderClass::ShaderMetaType ShaderClass::StaticType( \
	TEXT(#ShaderClass), \
	ShaderClass::GetSourceFilename(), \
	ShaderClass::GetFunctionName(), \
	Frequency, \
	ShaderClass::ConstructSerializedInstance, \
	ShaderClass::ConstructCompiledInstance, \
	ShaderClass::ModifyCompilationEnvironment, \
	ShaderClass::ShouldCache, \
	ShaderClass::GetStreamOutElements \
	);


/** todo: this should replace IMPLEMENT_SHADER_TYPE */
#define IMPLEMENT_SHADER_TYPE3(ShaderClass,Frequency) \
	ShaderClass::ShaderMetaType ShaderClass::StaticType( \
	TEXT(#ShaderClass), \
	ShaderClass::GetSourceFilename(), \
	ShaderClass::GetFunctionName(), \
	Frequency, \
	ShaderClass::ConstructSerializedInstance, \
	ShaderClass::ConstructCompiledInstance, \
	ShaderClass::ModifyCompilationEnvironment, \
	ShaderClass::ShouldCache, \
	ShaderClass::GetStreamOutElements \
	);
#endif


// Binding of a set of shader stages in a single pipeline
class SHADERCORE_API FShaderPipelineType
{
public:
	// Set bShouldOptimizeUnusedOutputs to true if we want unique FShaders for each shader pipeline
	// Set bShouldOptimizeUnusedOutputs to false if the FShaders will point to the individual shaders in the map
	FShaderPipelineType(
		const TCHAR* InName,
		const FShaderType* InVertexShader,
		const FShaderType* InHullShader,
		const FShaderType* InDomainShader,
		const FShaderType* InGeometryShader,
		const FShaderType* InPixelShader,
		bool bInShouldOptimizeUnusedOutputs);
	~FShaderPipelineType();

	FORCEINLINE bool HasTessellation() const { return AllStages[SF_Domain] != nullptr; }
	FORCEINLINE bool HasGeometry() const { return AllStages[SF_Geometry] != nullptr; }
	FORCEINLINE bool HasPixelShader() const { return AllStages[SF_Pixel] != nullptr; }

	FORCEINLINE const FShaderType* GetShader(EShaderFrequency Frequency) const
	{
		check(Frequency < SF_NumFrequencies);
		return AllStages[Frequency];
	}

	FORCEINLINE FName GetFName() const { return TypeName; }
	FORCEINLINE TCHAR const* GetName() const { return Name; }

	// Returns an array of valid stages, sorted from PS->GS->DS->HS->VS, no gaps if missing stages
	FORCEINLINE const TArray<const FShaderType*>& GetStages() const { return Stages; }

	static TLinkedList<FShaderPipelineType*>*& GetTypeList();

	/** @return The global shader pipeline name to type map */
	static TMap<FName, FShaderPipelineType*>& GetNameToTypeMap();
	static const FShaderPipelineType* GetShaderPipelineTypeByName(FName Name);

	/** Initialize static members, this must be called before any shader types are created. */
	static void Initialize();
	static void Uninitialize();

	static TArray<const FShaderPipelineType*> GetShaderPipelineTypesByFilename(const TCHAR* Filename);

	/** Serializes a shader type reference by name. */
	SHADERCORE_API friend FArchive& operator<<(FArchive& Ar, const FShaderPipelineType*& Ref);

	/** Hashes a pointer to a shader type. */
	friend uint32 GetTypeHash(FShaderPipelineType* Ref) { return Ref ? Ref->HashIndex : 0; }
	friend uint32 GetTypeHash(const FShaderPipelineType* Ref) { return Ref ? Ref->HashIndex : 0; }

	// Check if this pipeline is built of specific types
	bool IsGlobalTypePipeline() const { return Stages[0]->GetGlobalShaderType() != nullptr; }
	bool IsMaterialTypePipeline() const { return Stages[0]->GetMaterialShaderType() != nullptr; }
	bool IsMeshMaterialTypePipeline() const { return Stages[0]->GetMeshMaterialShaderType() != nullptr; }
	bool ShouldOptimizeUnusedOutputs() const { return bShouldOptimizeUnusedOutputs; }

	/** Gets a list of FShaderTypes & PipelineTypes whose source file no longer matches what that type was compiled with */
	static void GetOutdatedTypes(TArray<FShaderType*>& OutdatedShaderTypes, TArray<const FShaderPipelineType*>& ShaderPipelineTypesToFlush, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes);

	/** Calculates a Hash based on this shader pipeline type stages' source code and includes */
	const FSHAHash& GetSourceHash() const;

protected:
	const TCHAR* const Name;
	FName TypeName;

	// Pipeline Stages, ordered from lowest (usually PS) to highest (VS). Guaranteed at least one stage (for VS).
	TArray<const FShaderType*> Stages;

	const FShaderType* AllStages[SF_NumFrequencies];

	TLinkedList<FShaderPipelineType*> GlobalListLink;

	uint32 HashIndex;
	bool bShouldOptimizeUnusedOutputs;

	static bool bInitialized;
};

#if !UE_BUILD_DOCS
// Vertex+Pixel
#define IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(PipelineName, VertexShaderType, PixelShaderType, bRemoveUnused)	\
	static FShaderPipelineType PipelineName(TEXT(PREPROCESSOR_TO_STRING(PipelineName)), &VertexShaderType::StaticType, nullptr, nullptr, nullptr, &PixelShaderType::StaticType, bRemoveUnused);
// Only VS
#define IMPLEMENT_SHADERPIPELINE_TYPE_VS(PipelineName, VertexShaderType, bRemoveUnused)	\
	static FShaderPipelineType PipelineName(TEXT(PREPROCESSOR_TO_STRING(PipelineName)), &VertexShaderType::StaticType, nullptr, nullptr, nullptr, nullptr, bRemoveUnused);
// Vertex+Geometry+Pixel
#define IMPLEMENT_SHADERPIPELINE_TYPE_VSGSPS(PipelineName, VertexShaderType, GeometryShaderType, PixelShaderType, bRemoveUnused)	\
	static FShaderPipelineType PipelineName(TEXT(PREPROCESSOR_TO_STRING(PipelineName)), &VertexShaderType::StaticType, nullptr, nullptr, &GeometryShaderType::StaticType, &PixelShaderType::StaticType, bRemoveUnused);
// Vertex+Geometry
#define IMPLEMENT_SHADERPIPELINE_TYPE_VSGS(PipelineName, VertexShaderType, GeometryShaderType, bRemoveUnused)	\
	static FShaderPipelineType PipelineName(TEXT(PREPROCESSOR_TO_STRING(PipelineName)), &VertexShaderType::StaticType, nullptr, nullptr, &GeometryShaderType::StaticType, nullptr, bRemoveUnused);
// Vertex+Hull+Domain+Pixel
#define IMPLEMENT_SHADERPIPELINE_TYPE_VSHSDSPS(PipelineName, VertexShaderType, HullShaderType, DomainShaderType, PixelShaderType, bRemoveUnused)	\
	static FShaderPipelineType PipelineName(TEXT(PREPROCESSOR_TO_STRING(PipelineName)), &VertexShaderType::StaticType, &HullShaderType::StaticType, &DomainShaderType::StaticType, nullptr, &PixelShaderType::StaticType, bRemoveUnused);
// Vertex+Hull+Domain+Geometry+Pixel
#define IMPLEMENT_SHADERPIPELINE_TYPE_VSHSDSGSPS(PipelineName, VertexShaderType, HullShaderType, DomainShaderType, GeometryShaderType, PixelShaderType, bRemoveUnused)	\
	static FShaderPipelineType PipelineName(TEXT(PREPROCESSOR_TO_STRING(PipelineName)), &VertexShaderType::StaticType, &HullShaderType::StaticType, &DomainShaderType::StaticType, &GeometryShaderType::StaticType, &PixelShaderType::StaticType, bRemoveUnused);
// Vertex+Hull+Domain
#define IMPLEMENT_SHADERPIPELINE_TYPE_VSHSDS(PipelineName, VertexShaderType, HullShaderType, DomainShaderType, bRemoveUnused)	\
	static FShaderPipelineType PipelineName(TEXT(PREPROCESSOR_TO_STRING(PipelineName)), &VertexShaderType::StaticType, &HullShaderType::StaticType, &DomainShaderType::StaticType, nullptr, nullptr, bRemoveUnused);
// Vertex+Hull+Domain+Geometry
#define IMPLEMENT_SHADERPIPELINE_TYPE_VSHSDSGS(PipelineName, VertexShaderType, HullShaderType, DomainShaderType, GeometryShaderType, bRemoveUnused)	\
	static FShaderPipelineType PipelineName(TEXT(PREPROCESSOR_TO_STRING(PipelineName)), &VertexShaderType::StaticType, &HullShaderType::StaticType, &DomainShaderType::StaticType, &GeometryShaderType::StaticType, nullptr, bRemoveUnused);
#endif

/** Encapsulates a dependency on a shader type and saved state from that shader type. */
class FShaderTypeDependency
{
public:

	FShaderTypeDependency() :
		ShaderType(nullptr)
	{}

	/** Shader type */
	FShaderType* ShaderType;

	/** Used to detect changes to the shader source files. */
	FSHAHash SourceHash;

	friend FArchive& operator<<(FArchive& Ar,class FShaderTypeDependency& Ref)
	{
		Ar << Ref.ShaderType;
		Ar << Ref.SourceHash;
		return Ar;
	}

	bool operator==(const FShaderTypeDependency& Reference) const
	{
		return ShaderType == Reference.ShaderType && SourceHash == Reference.SourceHash;
	}
};


class FShaderPipelineTypeDependency
{
public:
	FShaderPipelineTypeDependency() :
		ShaderPipelineType(nullptr)
	{}

	/** Shader Pipeline type */
	const FShaderPipelineType* ShaderPipelineType;

	/** Used to detect changes to the shader source files. */
	FSHAHash StagesSourceHash;

	friend FArchive& operator<<(FArchive& Ar, class FShaderPipelineTypeDependency& Ref)
	{
		Ar << Ref.ShaderPipelineType;
		Ar << Ref.StagesSourceHash;
		return Ar;
	}

	bool operator==(const FShaderPipelineTypeDependency& Reference) const
	{
		return ShaderPipelineType == Reference.ShaderPipelineType && StagesSourceHash == Reference.StagesSourceHash;
	}
};

/** Used to compare two shader types by name. */
class FCompareShaderTypes
{																				
public:
	FORCEINLINE bool operator()(const FShaderType& A, const FShaderType& B ) const
	{
		int32 AL = FCString::Strlen(A.GetName());
		int32 BL = FCString::Strlen(B.GetName());
		if ( AL == BL )
		{
			return FCString::Strncmp(A.GetName(), B.GetName(), AL) > 0;
		}
		return AL > BL;
	}
};


/** Used to compare two shader pipeline types by name. */
class FCompareShaderPipelineNameTypes
{
public:
	/*FORCEINLINE*/ bool operator()(const FShaderPipelineType& A, const FShaderPipelineType& B) const
	{
		//#todo-rco: Avoid this by adding an FNullShaderPipelineType
		bool bNullA = &A == nullptr;
		bool bNullB = &B == nullptr;
		if (bNullA && bNullB)
		{
			return false;
		}
		else if (bNullA)
		{
			return true;
		}
		else if (bNullB)
		{
			return false;
		}


		int32 AL = FCString::Strlen(A.GetName());
		int32 BL = FCString::Strlen(B.GetName());
		if (AL == BL)
		{
			return FCString::Strncmp(A.GetName(), B.GetName(), AL) > 0;
		}
		return AL > BL;
	}
};

// A Shader Pipeline instance with compiled stages
class SHADERCORE_API FShaderPipeline
{
public:
	const FShaderPipelineType* PipelineType;
	TRefCountPtr<FShader> VertexShader;
	TRefCountPtr<FShader> HullShader;
	TRefCountPtr<FShader> DomainShader;
	TRefCountPtr<FShader> GeometryShader;
	TRefCountPtr<FShader> PixelShader;

	FShaderPipeline(
		const FShaderPipelineType* InPipelineType,
		FShader* InVertexShader,
		FShader* InHullShader,
		FShader* InDomainShader,
		FShader* InGeometryShader,
		FShader* InPixelShader);

	FShaderPipeline(const FShaderPipelineType* InPipelineType, const TArray<FShader*>& InStages);
	FShaderPipeline(const FShaderPipelineType* InPipelineType, const TArray< TRefCountPtr<FShader> >& InStages);

	~FShaderPipeline();

	// Find a shader inside the pipeline
	template<typename ShaderType>
	ShaderType* GetShader()
	{
		if (PixelShader && PixelShader->GetType() == &ShaderType::StaticType)
		{
			return (ShaderType*)PixelShader.GetReference();
		}
		else if (VertexShader && VertexShader->GetType() == &ShaderType::StaticType)
		{
			return (ShaderType*)VertexShader.GetReference();
		}
		else if (GeometryShader && GeometryShader->GetType() == &ShaderType::StaticType)
		{
			return (ShaderType*)GeometryShader.GetReference();
		}
		else if (HullShader)
		{
			if (HullShader->GetType() == &ShaderType::StaticType)
			{
				return (ShaderType*)HullShader.GetReference();
			}
			else if (DomainShader && DomainShader->GetType() == &ShaderType::StaticType)
			{
				return (ShaderType*)DomainShader.GetReference();
			}
		}

		return nullptr;
	}

	FShader* GetShader(EShaderFrequency Frequency)
	{
		switch (Frequency)
		{
		case SF_Vertex: return VertexShader.GetReference();
		case SF_Domain: return DomainShader.GetReference();
		case SF_Hull: return HullShader.GetReference();
		case SF_Geometry: return GeometryShader.GetReference();
		case SF_Pixel: return PixelShader.GetReference();
		default: check(0);
		}

		return nullptr;
	}

	const FShader* GetShader(EShaderFrequency Frequency) const
	{
		switch (Frequency)
		{
		case SF_Vertex: return VertexShader.GetReference();
		case SF_Domain: return DomainShader.GetReference();
		case SF_Hull: return HullShader.GetReference();
		case SF_Geometry: return GeometryShader.GetReference();
		case SF_Pixel: return PixelShader.GetReference();
		default: check(0);
		}

		return nullptr;
	}

	inline TArray<FShader*> GetShaders() const
	{
		TArray<FShader*> Shaders;

		if (PixelShader)
		{
			Shaders.Add(PixelShader.GetReference());
		}
		if (GeometryShader)
		{
			Shaders.Add(GeometryShader.GetReference());
		}
		if (HullShader)
		{
			Shaders.Add(DomainShader.GetReference());
			Shaders.Add(HullShader.GetReference());
		}

		Shaders.Add(VertexShader.GetReference());

		return Shaders;
	}

	inline uint32 GetSizeBytes() const
	{
		return sizeof(*this);
	}

	void Validate();

	enum EFilter
	{
		EAll,			// All pipelines
		EOnlyShared,	// Only pipelines with shared shaders
		EOnlyUnique,	// Only pipelines with unique shaders
	};
	
	static void CookPipeline(FShaderPipeline* Pipeline);
};

inline bool operator<(const FShaderPipeline& Lhs, const FShaderPipeline& Rhs)
{
	FCompareShaderPipelineNameTypes Comparator;
	return Comparator(*Lhs.PipelineType, *Rhs.PipelineType);
}

/** A collection of shaders of different types, but the same meta type. */
template<typename ShaderMetaType>
class TShaderMap
{
	/** Container for serialized shader pipeline stages to be registered on the game thread */
	struct FSerializedShaderPipeline
	{
		const FShaderPipelineType* ShaderPipelineType;
		TArray< TRefCountPtr<FShader> > ShaderStages;
		FSerializedShaderPipeline()
			: ShaderPipelineType(nullptr)
		{
		}
	};

	/** List of serialzied shaders to be processed and registered on the game thread */
	TArray<FShader*> SerializedShaders;
	/** List of serialzied shader pipeline stages to be processed and registered on the game thread */
	TArray<FSerializedShaderPipeline*> SerializedShaderPipelines;
protected:
	/** The platform this shader map was compiled with */
	EShaderPlatform Platform;
private:
	/** Flag that makes sure this shader map isn't used until all shaders have been registerd */
	bool bHasBeenRegistered;

public:
	/** Default constructor. */
	TShaderMap(EShaderPlatform InPlatform)
		: Platform(InPlatform)
		, bHasBeenRegistered(true)
	{}

	/** Destructor ensures pipelines cleared up. */
	virtual ~TShaderMap()
	{
		EmptyShaderPipelines();
	}

	EShaderPlatform GetShaderPlatform() const { return Platform; }

	/** Finds the shader with the given type.  Asserts on failure. */
	template<typename ShaderType>
	ShaderType* GetShader() const
	{
		check(bHasBeenRegistered);
		const TRefCountPtr<FShader>* ShaderRef = Shaders.Find(&ShaderType::StaticType);
		checkf(ShaderRef != NULL && *ShaderRef != nullptr, TEXT("Failed to find shader type %s in Platform %s"), ShaderType::StaticType.GetName(), *LegacyShaderPlatformToShaderFormat(Platform).ToString());
		return (ShaderType*)((*ShaderRef)->GetShaderChecked());
	}

	/** Finds the shader with the given type.  May return NULL. */
	FShader* GetShader(FShaderType* ShaderType) const
	{
		check(bHasBeenRegistered);
		const TRefCountPtr<FShader>* ShaderRef = Shaders.Find(ShaderType);
		return ShaderRef ? (*ShaderRef)->GetShaderChecked() : nullptr;
	}

	/** Finds the shader with the given type. */
	bool HasShader(FShaderType* Type) const
	{
		check(bHasBeenRegistered);
		const TRefCountPtr<FShader>* ShaderRef = Shaders.Find(Type);
		return ShaderRef != nullptr && *ShaderRef != nullptr;
	}

	inline const TMap<FShaderType*,TRefCountPtr<FShader> >& GetShaders() const
	{
		check(bHasBeenRegistered);
		return Shaders;
	}

	void AddShader(FShaderType* Type, FShader* Shader)
	{
		check(Type);
		Shaders.Add(Type, Shader);
	}

	/**
	 * Removes the shader of the given type from the shader map
	 * @param Type Shader type to remove the entry for 
	 */
	void RemoveShaderType(FShaderType* Type)
	{
		Shaders.Remove(Type);
	}


	void RemoveShaderPipelineType(const FShaderPipelineType* ShaderPipelineType)
	{
		FShaderPipeline** Found = ShaderPipelines.Find(ShaderPipelineType);
		if (Found)
		{
			if (*Found)
			{
				delete *Found;
			}
			ShaderPipelines.Remove(ShaderPipelineType);
		}
	}

	/** Builds a list of the shaders in a shader map. */
	void GetShaderList(TMap<FShaderId, FShader*>& OutShaders) const
	{
		check(bHasBeenRegistered);
		for(TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator ShaderIt(Shaders);ShaderIt;++ShaderIt)
		{
			if(ShaderIt.Value())
			{
				OutShaders.Add(ShaderIt.Value()->GetId(),ShaderIt.Value());
			}
		}
	}

	/** Builds a list of the shader pipelines in a shader map. */
	void GetShaderPipelineList(TArray<FShaderPipeline*>& OutShaderPipelines, FShaderPipeline::EFilter Filter) const
	{
		check(bHasBeenRegistered);
		for (auto Pair : ShaderPipelines)
		{
			FShaderPipeline* Pipeline = Pair.Value;
			if (Pipeline->PipelineType->ShouldOptimizeUnusedOutputs() && Filter == FShaderPipeline::EOnlyShared)
			{
				continue;
			}
			else if (!Pipeline->PipelineType->ShouldOptimizeUnusedOutputs() && Filter == FShaderPipeline::EOnlyUnique)
			{
				continue;
			}
			OutShaderPipelines.Add(Pipeline);
		}
	}

	uint32 GetMaxTextureSamplersShaderMap() const
	{
		check(bHasBeenRegistered);
		uint32 MaxTextureSamplers = 0;

		for (TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator ShaderIt(Shaders);ShaderIt;++ShaderIt)
		{
			if (ShaderIt.Value())
			{
				MaxTextureSamplers = FMath::Max(MaxTextureSamplers, ShaderIt.Value()->GetNumTextureSamplers());
			}
		}

		for (auto Pair : ShaderPipelines)
		{
			const FShaderPipeline* Pipeline = Pair.Value;
			for (const FShaderType* ShaderType : Pair.Key->GetStages())
			{
				MaxTextureSamplers = FMath::Max(MaxTextureSamplers, Pipeline->GetShader(ShaderType->GetFrequency())->GetNumTextureSamplers());
			}
		}

		return MaxTextureSamplers;
	}

	inline void SerializeShaderForSaving(FShader* CurrentShader, FArchive& Ar, bool bHandleShaderKeyChanges, bool bInlineShaderResource)
	{
		int32 SkipOffset = Ar.Tell();

		{
#if WITH_EDITOR
			FArchive::FScopeSetDebugSerializationFlags S(Ar, DSF_IgnoreDiff);
#endif
			// Serialize a placeholder value, we will overwrite this with an offset to the end of the shader
			Ar << SkipOffset;
		}

		if (bHandleShaderKeyChanges)
		{
			FSelfContainedShaderId SelfContainedKey = CurrentShader->GetId();
			Ar << SelfContainedKey;
		}

		CurrentShader->SerializeBase(Ar, bInlineShaderResource);

		// Get the offset to the end of the shader's serialized data
		int32 EndOffset = Ar.Tell();
		// Seek back to the placeholder and write the end offset
		// This allows us to skip over the shader's serialized data at load time without knowing how to deserialize it
		// Which can happen with shaders that were available at cook time, but not on the target platform (shaders in editor module for example)
		Ar.Seek(SkipOffset);
		Ar << EndOffset;
		Ar.Seek(EndOffset);
	}

	inline FShader* SerializeShaderForLoad(FShaderType* Type, FArchive& Ar, bool bHandleShaderKeyChanges, bool bInlineShaderResource)
	{
		int32 EndOffset = 0;
		Ar << EndOffset;

		FSelfContainedShaderId SelfContainedKey;

		if (bHandleShaderKeyChanges)
		{
			Ar << SelfContainedKey;
		}

		FShader* Shader = nullptr;
		if (Type
			// If we are handling shader key changes, only create the shader if the serialized key matches the key the shader would have if created
			// This allows serialization changes between the save and load to be safely handled
			&& (!bHandleShaderKeyChanges || SelfContainedKey.IsValid()))
		{
			Shader = Type->ConstructForDeserialization();
			check(Shader != nullptr);
			Shader->SerializeBase(Ar, bInlineShaderResource);
		}
		else
		{
			// Skip over this shader's serialized data if the type doesn't exist
			// This can happen with shader types in modules that were loaded during cooking but not at run time (editor)
			Ar.Seek(EndOffset);
		}
		return Shader;
	}

	/** 
	 * Used to serialize a shader map inline in a material in a package. 
	 * @param bInlineShaderResource - whether to inline the shader resource's serializations
	 * @param bHandleShaderKeyChanges - whether to serialize the data necessary to detect and gracefully handle shader key changes between saving and loading
	 */
	void SerializeInline(FArchive& Ar, bool bInlineShaderResource, bool bHandleShaderKeyChanges)
	{
		if (Ar.IsSaving())
		{
			int32 NumShaders = Shaders.Num();
			Ar << NumShaders;

			// Sort the shaders by type name before saving, to make sure the saved result is binary equivalent to what is generated on other machines, 
			// Which is a requirement of the Derived Data Cache.
			auto SortedShaders = Shaders;
			SortedShaders.KeySort(FCompareShaderTypes());

			for (TMap<FShaderType*, TRefCountPtr<FShader> >::TIterator ShaderIt(SortedShaders); ShaderIt; ++ShaderIt)
			{
				FShaderType* Type = ShaderIt.Key();
				check(Type);
				checkSlow(FName(Type->GetName()) != NAME_None);

				Ar << Type;
				FShader* CurrentShader = ShaderIt.Value();
				SerializeShaderForSaving(CurrentShader, Ar, bHandleShaderKeyChanges, bInlineShaderResource);
			}

			TArray<FShaderPipeline*> SortedPipelines;
			GetShaderPipelineList(SortedPipelines, FShaderPipeline::EAll);
			int32 NumPipelines = SortedPipelines.Num();
			Ar << NumPipelines;
			// Sort the shader pipelines by type name before saving, to make sure the saved result is binary equivalent to what is generated on other machines, Which is a requirement of the Derived Data Cache.
			SortedPipelines.Sort();
			for (FShaderPipeline* CurrentPipeline : SortedPipelines)
			{
				const FShaderPipelineType* PipelineType = CurrentPipeline->PipelineType;
				Ar << PipelineType;

				auto& PipelineStages = PipelineType->GetStages();
				int32 NumStages = PipelineStages.Num();
				Ar << NumStages;
				for (int32 Index = 0; Index < NumStages; ++Index)
				{
					auto* Shader = CurrentPipeline->GetShader(PipelineStages[Index]->GetFrequency());
					FShaderType* Type = Shader->GetType();
					Ar << Type;
					SerializeShaderForSaving(Shader, Ar, bHandleShaderKeyChanges, bInlineShaderResource);
				}
#if WITH_EDITORONLY_DATA
				if(Ar.IsCooking())
				{
					FShaderPipeline::CookPipeline(CurrentPipeline);
				}
#endif
			}
		}

		if (Ar.IsLoading())
		{
			// Mark as unregistered - about to load new shaders that need to be registered later 
			// on the game thread.
			bHasBeenRegistered = false;

			int32 NumShaders = 0;
			Ar << NumShaders;

			SerializedShaders.Reserve(NumShaders);
			for (int32 ShaderIndex = 0; ShaderIndex < NumShaders; ShaderIndex++)
			{
				FShaderType* Type = nullptr;
				Ar << Type;

				FShader* Shader = SerializeShaderForLoad(Type, Ar, bHandleShaderKeyChanges, bInlineShaderResource);
				if (Shader)
				{
					SerializedShaders.Add(Shader);
				}
			}

			int32 NumPipelines = 0;
			Ar << NumPipelines;
			for (int32 PipelineIndex = 0; PipelineIndex < NumPipelines; ++PipelineIndex)
			{
				const FShaderPipelineType* ShaderPipelineType = nullptr;
				Ar << ShaderPipelineType;
				int32 NumStages = 0;
				Ar << NumStages;
				// Make a list of references so they can be deleted when going out of scope if needed
				TArray< TRefCountPtr<FShader> > ShaderStages;
				for (int32 Index = 0; Index < NumStages; ++Index)
				{
					FShaderType* Type = nullptr;
					Ar << Type;
					FShader* Shader = SerializeShaderForLoad(Type, Ar, bHandleShaderKeyChanges, bInlineShaderResource);
					if (Shader)
					{
						ShaderStages.Add(Shader);
					}
				}

				// ShaderPipelineType can be nullptr if the pipeline existed but now is gone!
				if (ShaderPipelineType && ShaderStages.Num() == ShaderPipelineType->GetStages().Num())
				{
					FSerializedShaderPipeline* SerializedPipeline = new FSerializedShaderPipeline();
					SerializedPipeline->ShaderPipelineType = ShaderPipelineType;
					SerializedPipeline->ShaderStages = MoveTemp(ShaderStages);
					SerializedShaderPipelines.Add(SerializedPipeline);
				}
			}
		}
	}

	/** Registered all shaders that have been serialized (maybe) on another thread */
	virtual void RegisterSerializedShaders()
	{
		bHasBeenRegistered = true;
		check(IsInGameThread());
		for (FShader* Shader : SerializedShaders)
		{
			Shader->RegisterSerializedResource();

			FShaderType* Type = Shader->GetType();
			FShader* ExistingShader = Type->FindShaderById(Shader->GetId());

			if (ExistingShader != nullptr)
			{
				delete Shader;
				Shader = ExistingShader;
			}
			else
			{
				// Register the shader now that it is valid, so that it can be reused
				Shader->Register();
			}
			AddShader(Shader->GetType(), Shader);
		}
		SerializedShaders.Empty();

		for (FSerializedShaderPipeline* SerializedPipeline : SerializedShaderPipelines)
		{
			for (TRefCountPtr<FShader> Shader : SerializedPipeline->ShaderStages)
			{
				Shader->RegisterSerializedResource();
			}
			FShaderPipeline* ShaderPipeline = new FShaderPipeline(SerializedPipeline->ShaderPipelineType, SerializedPipeline->ShaderStages);
			AddShaderPipeline(SerializedPipeline->ShaderPipelineType, ShaderPipeline);

			delete SerializedPipeline;
		}
		SerializedShaderPipelines.Empty();
	}

	/** Discards serialized shaders when they are not going to be used for anything (NullRHI) */
	virtual void DiscardSerializedShaders()
	{
		for (FShader* Shader : SerializedShaders)
		{
			if (Shader)
			{
				Shader->DiscardSerializedResource();
			}
			delete Shader;
		}
		SerializedShaders.Empty();

		for (FSerializedShaderPipeline* SerializedPipeline : SerializedShaderPipelines)
		{
			for (TRefCountPtr<FShader> Shader : SerializedPipeline->ShaderStages)
			{
				Shader->DiscardSerializedResource();
			}
			delete SerializedPipeline;
		}
		SerializedShaderPipelines.Empty();
	}

	/** @return true if the map is empty */
	inline bool IsEmpty() const
	{
		check(bHasBeenRegistered);
		return Shaders.Num() == 0;
	}

	/** @return The number of shaders in the map. */
	inline uint32 GetNumShaders() const
	{
		check(bHasBeenRegistered);
		return Shaders.Num();
	}

	/** @return The number of shader pipelines in the map. */
	inline uint32 GetNumShaderPipelines() const
	{
		check(bHasBeenRegistered);
		return ShaderPipelines.Num();
	}

	/** clears out all shaders and deletes shader pipelines held in the map */
	void Empty()
	{
		Shaders.Empty();
		EmptyShaderPipelines();
	}

	inline FShaderPipeline* GetShaderPipeline(const FShaderPipelineType* PipelineType)
	{
		check(bHasBeenRegistered);
		FShaderPipeline** Found = ShaderPipelines.Find(PipelineType);
		return Found ? *Found : nullptr;
	}

	inline FShaderPipeline* GetShaderPipeline(const FShaderPipelineType* PipelineType) const
	{
		check(bHasBeenRegistered);
		FShaderPipeline* const* Found = ShaderPipelines.Find(PipelineType);
		return Found ? *Found : nullptr;
	}

	// Returns nullptr if not found
	inline bool HasShaderPipeline(const FShaderPipelineType* PipelineType) const
	{
		check(bHasBeenRegistered);
		return (GetShaderPipeline(PipelineType) != nullptr);
	}

	inline void AddShaderPipeline(const FShaderPipelineType* Type, FShaderPipeline* ShaderPipeline)
	{
		check(bHasBeenRegistered);
		check(Type);
		check(!ShaderPipeline || ShaderPipeline->PipelineType == Type);
		ShaderPipelines.Add(Type, ShaderPipeline);
	}

	uint32 GetMaxNumInstructionsForShader(const FShaderType* ShaderType) const
	{
		check(bHasBeenRegistered);
		uint32 MaxNumInstructions = 0;
		auto* FoundShader = Shaders.Find(ShaderType);
		if (FoundShader && *FoundShader)
		{
			MaxNumInstructions = FMath::Max(MaxNumInstructions, (*FoundShader)->GetNumInstructions());
		}

		for (auto& Pair : ShaderPipelines)
		{
			FShaderPipeline* Pipeline = Pair.Value;
			auto* Shader = Pipeline->GetShader(ShaderType->GetFrequency());
			if (Shader)
			{
				MaxNumInstructions = FMath::Max(MaxNumInstructions, Shader->GetNumInstructions());
			}
		}

		return MaxNumInstructions;
	}

protected:
	inline void EmptyShaderPipelines()
	{
		for (auto& Pair : ShaderPipelines)
		{
			if (FShaderPipeline* Pipeline = Pair.Value)
			{
				delete Pipeline;
				Pipeline = nullptr;
			}
		}
		ShaderPipelines.Empty();
	}

	TMap<FShaderType*, TRefCountPtr<FShader> > Shaders;
	TMap<const FShaderPipelineType*, FShaderPipeline*> ShaderPipelines;
};

/** A reference which is initialized with the requested shader type from a shader map. */
template<typename ShaderType>
class TShaderMapRef
{
public:
	TShaderMapRef(const TShaderMap<typename ShaderType::ShaderMetaType>* ShaderIndex):
	 Shader(ShaderIndex->template GetShader<ShaderType>()) // gcc3 needs the template quantifier so it knows the < is not a less-than
	{}
	FORCEINLINE ShaderType* operator->() const
	{
		return Shader;
	}
	FORCEINLINE ShaderType* operator*() const
	{
		return Shader;
	}
private:
	ShaderType* Shader;
};

/** A reference to an optional shader, initialized with a shader type from a shader map if it is available or nullptr if it is not. */
template<typename ShaderType>
class TOptionalShaderMapRef
{
public:
	TOptionalShaderMapRef(const TShaderMap<typename ShaderType::ShaderMetaType>* ShaderIndex):
	Shader((ShaderType*)ShaderIndex->GetShader(&ShaderType::StaticType)) // gcc3 needs the template quantifier so it knows the < is not a less-than
	{}
	FORCEINLINE bool IsValid() const
	{
		return Shader != nullptr;
	}
	FORCEINLINE ShaderType* operator->() const
	{
		return Shader;
	}
	FORCEINLINE ShaderType* operator*() const
	{
		return Shader;
	}
private:
	ShaderType* Shader;
};

/** Tracks state when traversing a FSerializationHistory. */
class FSerializationHistoryTraversalState
{
public:

	const FSerializationHistory& History;
	int32 NextTokenIndex;
	int32 NextFullLengthIndex;

	FSerializationHistoryTraversalState(const FSerializationHistory& InHistory) :
		History(InHistory),
		NextTokenIndex(0),
		NextFullLengthIndex(0)
	{}

	/** Gets the length value from NextTokenIndex + Offset into history. */
	uint32 GetValue(int32 Offset)
	{
		int32 CurrentOffset = Offset;

		// Move to the desired offset
		while (CurrentOffset > 0)
		{
			StepForward();
			CurrentOffset--;
		}

		while (CurrentOffset < 0)
		{
			StepBackward();
			CurrentOffset++;
		}
		check(CurrentOffset == 0);

		// Decode
		const int8 Token = History.GetToken(NextTokenIndex);
		const uint32 Value = Token == 0 ? History.FullLengths[NextFullLengthIndex] : (int32)Token;

		// Restore state
		while (CurrentOffset < Offset)
		{
			StepBackward();
			CurrentOffset++;
		}

		while (CurrentOffset > Offset)
		{
			StepForward();
			CurrentOffset--;
		}
		check(CurrentOffset == Offset);

		return Value;
	}

	void StepForward()
	{
		const int8 Token = History.GetToken(NextTokenIndex);

		if (Token == 0)
		{
			check(NextFullLengthIndex - 1 < History.FullLengths.Num());
			NextFullLengthIndex++;
		}

		// Not supporting seeking past the front most serialization in the history
		check(NextTokenIndex - 1 < History.NumTokens);
		NextTokenIndex++;
	}

	void StepBackward()
	{
		// Not supporting seeking outside of the history tracked
		check(NextTokenIndex > 0);
		NextTokenIndex--;

		const int8 Token = History.GetToken(NextTokenIndex);

		if (Token == 0)
		{
			check(NextFullLengthIndex > 0);
			NextFullLengthIndex--;
		}
	}
};

/** Archive used when saving shaders, which generates data used to detect serialization mismatches on load. */
class FShaderSaveArchive : public FArchiveProxy
{
public:

	FShaderSaveArchive(FArchive& Archive, FSerializationHistory& InHistory) : 
		FArchiveProxy(Archive),
		HistoryTraversalState(InHistory),
		History(InHistory)
	{
		OriginalPosition = Archive.Tell();
	}

	virtual ~FShaderSaveArchive()
	{
		// Seek back to the original archive position so we can undo any serializations that went through this archive
		InnerArchive.Seek(OriginalPosition);
	}

	virtual void Serialize( void* V, int64 Length )
	{
		if (HistoryTraversalState.NextTokenIndex < HistoryTraversalState.History.NumTokens)
		{
			// We are no longer appending (due to a seek), make sure writes match up in size with what's already been written
			check(Length == HistoryTraversalState.GetValue(0));
		}
		else
		{
			// Appending to the archive, track the size of this serialization
			History.AddValue(Length);
		}
		HistoryTraversalState.StepForward();
		
		if (V)
		{
			FArchiveProxy::Serialize(V, Length);
		}
	}

	virtual void Seek( int64 InPos )
	{
		int64 Offset = InPos - Tell();
		if (Offset <= 0)
		{
			// We're seeking backward, walk backward through the serialization history while updating NextSerialization
			while (Offset < 0)
			{
				Offset += HistoryTraversalState.GetValue(-1);
				HistoryTraversalState.StepBackward();
			}
		}
		else
		{
			// We're seeking forward, walk forward through the serialization history while updating NextSerialization
			while (Offset > 0)
			{
				Offset -= HistoryTraversalState.GetValue(-1);
				HistoryTraversalState.StepForward();
			}
			HistoryTraversalState.StepForward();
		}
		check(Offset == 0);
		
		FArchiveProxy::Seek(InPos);
	}

	FSerializationHistoryTraversalState HistoryTraversalState;
	FSerializationHistory& History;

private:
	/** Stored off position of the original archive we are wrapping. */
	int64 OriginalPosition;
};

inline void FShader::CheckShaderIsValid() const
{
	checkf(Canary == ShaderMagic_Initialized,
		TEXT("FShader %s is %s. Canary is 0x%08x."),
		Canary == ShaderMagic_Uninitialized ? GetType()->GetName() : TEXT("[invalid]"),
		Canary == ShaderMagic_Uninitialized ? TEXT("uninitialized") : TEXT("garbage memory"),
		Canary
		);
}

/**
 * Dumps shader stats to the log. Will also print some shader pipeline information.
 * @param Platform  - Platform to dump shader info for, use SP_NumPlatforms for all
 * @param Frequency - Whether to dump PS or VS info, use SF_NumFrequencies to dump both
 */
extern SHADERCORE_API void DumpShaderStats( EShaderPlatform Platform, EShaderFrequency Frequency );

/**
 * Dumps shader pipeline stats to the log. Does not include material (eg shader pipeline instance) information.
 * @param Platform  - Platform to dump shader info for, use SP_NumPlatforms for all
 */
extern SHADERCORE_API void DumpShaderPipelineStats(EShaderPlatform Platform);

/**
 * Finds the shader type with a given name.
 * @param ShaderTypeName - The name of the shader type to find.
 * @return The shader type, or NULL if none matched.
 */
extern SHADERCORE_API FShaderType* FindShaderTypeByName(FName ShaderTypeName);

/** Helper function to dispatch a compute shader while checking that parameters have been set correctly. */
extern SHADERCORE_API void DispatchComputeShader(
	FRHICommandList& RHICmdList,
	FShader* Shader,
	uint32 ThreadGroupCountX,
	uint32 ThreadGroupCountY,
	uint32 ThreadGroupCountZ);

extern SHADERCORE_API void DispatchComputeShader(
	FRHIAsyncComputeCommandListImmediate& RHICmdList,
	FShader* Shader,
	uint32 ThreadGroupCountX,
	uint32 ThreadGroupCountY,
	uint32 ThreadGroupCountZ);

/** Helper function to dispatch a compute shader indirectly while checking that parameters have been set correctly. */
extern SHADERCORE_API void DispatchIndirectComputeShader(
	FRHICommandList& RHICmdList,
	FShader* Shader,
	FVertexBufferRHIParamRef ArgumentBuffer,
	uint32 ArgumentOffset);

/** Returns an array of all target shader formats, possibly from multiple target platforms. */
extern SHADERCORE_API const TArray<FName>& GetTargetShaderFormats();

/** Appends to KeyString for all shaders. */
extern SHADERCORE_API void ShaderMapAppendKeyString(EShaderPlatform Platform, FString& KeyString);
