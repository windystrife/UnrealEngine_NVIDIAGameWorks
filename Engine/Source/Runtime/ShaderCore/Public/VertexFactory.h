// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VertexFactory.h: Vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/List.h"
#include "Misc/SecureHash.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ShaderCore.h"
#include "Shader.h"

class FMaterial;

/**
 * A typed data source for a vertex factory which streams data from a vertex buffer.
 */
struct FVertexStreamComponent
{
	/** The vertex buffer to stream data from.  If null, no data can be read from this stream. */
	const FVertexBuffer* VertexBuffer;

	/** The offset of the data, relative to the beginning of each element in the vertex buffer. */
	uint8 Offset;

	/** The stride of the data. */
	uint8 Stride;

	/** The type of the data read from this stream. */
	TEnumAsByte<EVertexElementType> Type;

	/** true if the stream should be indexed by instance index instead of vertex index. */
	bool bUseInstanceIndex;

	/** true if the stream is set by the vertex factory and skipped by FVertexFactory::Set */
	bool bSetByVertexFactoryInSetMesh;

	/**
	 * Initializes the data stream to null.
	 */
	FVertexStreamComponent():
		VertexBuffer(NULL),
		Offset(0),
		Stride(0),
		Type(VET_None),
		bUseInstanceIndex(false),
		bSetByVertexFactoryInSetMesh(false)
	{}

	/**
	 * Minimal initialization constructor.
	 */
	FVertexStreamComponent(const FVertexBuffer* InVertexBuffer, uint32 InOffset, uint32 InStride, EVertexElementType InType, bool bInUseInstanceIndex = false, bool bInSetByVertexFactoryInSetMesh = false) :
		VertexBuffer(InVertexBuffer),
		Offset(InOffset),
		Stride(InStride),
		Type(InType),
		bUseInstanceIndex(bInUseInstanceIndex),
		bSetByVertexFactoryInSetMesh(bInSetByVertexFactoryInSetMesh)
	{}
};

/**
 * A macro which initializes a FVertexStreamComponent to read a member from a struct.
 */
#define STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,VertexType,Member,MemberType) \
	FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(VertexType,Member),sizeof(VertexType),MemberType)

/**
 * An interface to the parameter bindings for the vertex factory used by a shader.
 */
class SHADERCORE_API FVertexFactoryShaderParameters
{
public:
	virtual ~FVertexFactoryShaderParameters() {}
	virtual void Bind(const class FShaderParameterMap& ParameterMap) = 0;
	virtual void Serialize(FArchive& Ar) = 0;
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* VertexShader,const class FVertexFactory* VertexFactory,const class FSceneView& View,const struct FMeshBatchElement& BatchElement,uint32 DataFlags) const = 0;
	virtual uint32 GetSize() const { return sizeof(*this); }
};

/**
 * An object used to represent the type of a vertex factory.
 */
class FVertexFactoryType
{
public:

	typedef FVertexFactoryShaderParameters* (*ConstructParametersType)(EShaderFrequency ShaderFrequency);
	typedef bool (*ShouldCacheType)(EShaderPlatform, const class FMaterial*, const class FShaderType*);
	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, const class FMaterial*, FShaderCompilerEnvironment&);
	typedef bool (*SupportsTessellationShadersType)();

	/**
	 * @return The global shader factory list.
	 */
	static SHADERCORE_API TLinkedList<FVertexFactoryType*>*& GetTypeList();

	/**
	 * Finds a FVertexFactoryType by name.
	 */
	static SHADERCORE_API FVertexFactoryType* GetVFByName(const FString& VFName);

	/** Initialize FVertexFactoryType static members, this must be called before any VF types are created. */
	static void Initialize(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables);

	/** Uninitializes FVertexFactoryType cached data. */
	static void Uninitialize();

	/**
	 * Minimal initialization constructor.
	 * @param bInUsedWithMaterials - True if the vertex factory will be rendered in combination with a material.
	 * @param bInSupportsStaticLighting - True if the vertex factory will be rendered with static lighting.
	 */
	SHADERCORE_API FVertexFactoryType(
		const TCHAR* InName,
		const TCHAR* InShaderFilename,
		bool bInUsedWithMaterials,
		bool bInSupportsStaticLighting,
		bool bInSupportsDynamicLighting,
		bool bInSupportsPrecisePrevWorldPos,
		bool bInSupportsPositionOnly,
		ConstructParametersType InConstructParameters,
		ShouldCacheType InShouldCache,
		ModifyCompilationEnvironmentType InModifyCompilationEnvironment,
		SupportsTessellationShadersType InSupportsTessellationShaders
		);

	SHADERCORE_API virtual ~FVertexFactoryType();

	// Accessors.
	const TCHAR* GetName() const { return Name; }
	FName GetFName() const { return TypeName; }
	const TCHAR* GetShaderFilename() const { return ShaderFilename; }
	FVertexFactoryShaderParameters* CreateShaderParameters(EShaderFrequency ShaderFrequency) const { return (*ConstructParameters)(ShaderFrequency); }
	bool IsUsedWithMaterials() const { return bUsedWithMaterials; }
	bool SupportsStaticLighting() const { return bSupportsStaticLighting; }
	bool SupportsDynamicLighting() const { return bSupportsDynamicLighting; }
	bool SupportsPrecisePrevWorldPos() const { return bSupportsPrecisePrevWorldPos; }
	bool SupportsPositionOnly() const { return bSupportsPositionOnly; }
	/** Returns an int32 specific to this vertex factory type. */
	inline int32 GetId() const { return HashIndex; }
	static int32 GetNumVertexFactoryTypes() { return NextHashIndex; }

	const FSerializationHistory* GetSerializationHistory(EShaderFrequency Frequency) const
	{
		return &SerializationHistory[Frequency];
	}

	// Hash function.
	friend uint32 GetTypeHash(FVertexFactoryType* Type)
	{ 
		return Type ? Type->HashIndex : 0;
	}

	/** Calculates a Hash based on this vertex factory type's source code and includes */
	SHADERCORE_API const FSHAHash& GetSourceHash() const;

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return (*ShouldCacheRef)(Platform, Material, ShaderType); 
	}

	/**
	* Calls the function ptr for the shader type on the given environment
	* @param Environment - shader compile environment to modify
	*/
	void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		// Set up the mapping from VertexFactory.usf to the vertex factory type's source code.
		FString VertexFactoryIncludeString = FString::Printf( TEXT("#include \"%s\""), GetShaderFilename() );
		OutEnvironment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/VertexFactory.ush"), StringToArray<ANSICHAR>(*VertexFactoryIncludeString, VertexFactoryIncludeString.Len() + 1));

		OutEnvironment.SetDefine(TEXT("HAS_PRIMITIVE_UNIFORM_BUFFER"), 1);

		(*ModifyCompilationEnvironmentRef)(Platform, Material, OutEnvironment);
	}

	/**
	 * Does this vertex factory support tessellation shaders?
	 */
	bool SupportsTessellationShaders() const
	{
		return (*SupportsTessellationShadersRef)(); 
	}

	/** Adds include statements for uniform buffers that this shader type references, and builds a prefix for the shader file with the include statements. */
	SHADERCORE_API void AddReferencedUniformBufferIncludes(FShaderCompilerEnvironment& OutEnvironment, FString& OutSourceFilePrefix, EShaderPlatform Platform);

	void FlushShaderFileCache(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
	{
		ReferencedUniformBufferStructsCache.Empty();
		GenerateReferencedUniformBuffers(ShaderFilename, Name, ShaderFileToUniformBufferVariables, ReferencedUniformBufferStructsCache);

		for (int32 Platform = 0; Platform < SP_NumPlatforms; Platform++)
		{
			bCachedUniformBufferStructDeclarations[Platform] = false;
		}
	}

	const TMap<const TCHAR*, FCachedUniformBufferDeclaration>& GetReferencedUniformBufferStructsCache() const
	{
		return ReferencedUniformBufferStructsCache;
	}

private:
	static SHADERCORE_API uint32 NextHashIndex;

	/** Tracks whether serialization history for all shader types has been initialized. */
	static bool bInitializedSerializationHistory;

	uint32 HashIndex;
	const TCHAR* Name;
	const TCHAR* ShaderFilename;
	FName TypeName;
	uint32 bUsedWithMaterials : 1;
	uint32 bSupportsStaticLighting : 1;
	uint32 bSupportsDynamicLighting : 1;
	uint32 bSupportsPrecisePrevWorldPos : 1;
	uint32 bSupportsPositionOnly : 1;
	ConstructParametersType ConstructParameters;
	ShouldCacheType ShouldCacheRef;
	ModifyCompilationEnvironmentType ModifyCompilationEnvironmentRef;
	SupportsTessellationShadersType SupportsTessellationShadersRef;

	TLinkedList<FVertexFactoryType*> GlobalListLink;

	/** 
	 * Cache of referenced uniform buffer includes.  
	 * These are derived from source files so they need to be flushed when editing and recompiling shaders on the fly. 
	 * FVertexFactoryType::Initialize will add an entry for each referenced uniform buffer, but the declarations are added on demand as shaders are compiled.
	 */
	TMap<const TCHAR*, FCachedUniformBufferDeclaration> ReferencedUniformBufferStructsCache;

	/** Tracks what platforms ReferencedUniformBufferStructsCache has had declarations cached for. */
	bool bCachedUniformBufferStructDeclarations[SP_NumPlatforms];

	/** 
	 * Stores a history of serialization sizes for this vertex factory's shader parameter class. 
	 * This is used to invalidate shaders when serialization changes.
	 */
	FSerializationHistory SerializationHistory[SF_NumFrequencies];
};

/**
 * Serializes a reference to a vertex factory type.
 */
extern SHADERCORE_API FArchive& operator<<(FArchive& Ar,FVertexFactoryType*& TypeRef);

/**
 * Find the vertex factory type with the given name.
 * @return NULL if no vertex factory type matched, otherwise a vertex factory type with a matching name.
 */
extern SHADERCORE_API FVertexFactoryType* FindVertexFactoryType(FName TypeName);

/**
 * A macro for declaring a new vertex factory type, for use in the vertex factory class's definition body.
 */
#define DECLARE_VERTEX_FACTORY_TYPE(FactoryClass) \
	public: \
	static FVertexFactoryType StaticType; \
	virtual FVertexFactoryType* GetType() const override { return &StaticType; }

/**
 * A macro for implementing the static vertex factory type object, and specifying parameters used by the type.
 * @param bUsedWithMaterials - True if the vertex factory will be rendered in combination with a material.
 * @param bSupportsStaticLighting - True if the vertex factory will be rendered with static lighting.
 */
#define IMPLEMENT_VERTEX_FACTORY_TYPE(FactoryClass,ShaderFilename,bUsedWithMaterials,bSupportsStaticLighting,bSupportsDynamicLighting,bPrecisePrevWorldPos,bSupportsPositionOnly) \
	FVertexFactoryShaderParameters* Construct##FactoryClass##ShaderParameters(EShaderFrequency ShaderFrequency) { return FactoryClass::ConstructShaderParameters(ShaderFrequency); } \
	FVertexFactoryType FactoryClass::StaticType( \
		TEXT(#FactoryClass), \
		TEXT(ShaderFilename), \
		bUsedWithMaterials, \
		bSupportsStaticLighting, \
		bSupportsDynamicLighting, \
		bPrecisePrevWorldPos, \
		bSupportsPositionOnly, \
		Construct##FactoryClass##ShaderParameters, \
		FactoryClass::ShouldCache, \
		FactoryClass::ModifyCompilationEnvironment, \
		FactoryClass::SupportsTessellationShaders \
		);

/** Encapsulates a dependency on a vertex factory type and saved state from that vertex factory type. */
class FVertexFactoryTypeDependency
{
public:

	FVertexFactoryTypeDependency() :
		VertexFactoryType(NULL)
	{}

	FVertexFactoryType* VertexFactoryType;

	/** Used to detect changes to the vertex factory source files. */
	FSHAHash VFSourceHash;

	friend FArchive& operator<<(FArchive& Ar,class FVertexFactoryTypeDependency& Ref)
	{
		Ar << Ref.VertexFactoryType << Ref.VFSourceHash;
		return Ar;
	}

	bool operator==(const FVertexFactoryTypeDependency& Reference) const
	{
		return VertexFactoryType == Reference.VertexFactoryType && VFSourceHash == Reference.VFSourceHash;
	}
};

/** Used to compare two Vertex Factory types by name. */
class FCompareVertexFactoryTypes											
{																				
public:		
	FORCEINLINE bool operator()(const FVertexFactoryType& A, const FVertexFactoryType& B ) const
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

/**
 * Encapsulates a vertex data source which can be linked into a vertex shader.
 */
class SHADERCORE_API FVertexFactory : public FRenderResource
{
public:
	FVertexFactory() 
		: bNeedsDeclaration(true)
	{
	}

	FVertexFactory(ERHIFeatureLevel::Type InFeatureLevel) 
		: FRenderResource(InFeatureLevel)
		, bNeedsDeclaration(true)
	{
	}

	virtual FVertexFactoryType* GetType() const { return NULL; }

	/**
	 * Activates the vertex factory.
	 */
	void Set(FRHICommandList& RHICmdList) const;

	/**
	 * Call SetStreamSource on instance streams to offset the read pointer
	 */
	void OffsetInstanceStreams(FRHICommandList& RHICmdList, uint32 FirstVertex) const;

	/**
	* Sets the position stream as the current stream source.
	*/
	void SetPositionStream(FRHICommandList& RHICmdList) const;

	/**
	* Call SetStreamSource on instance streams to offset the read pointer
	 */
	void OffsetPositionInstanceStreams(FRHICommandList& RHICmdList, uint32 FirstVertex) const;

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment ) {}

	/**
	* Can be overridden by FVertexFactory subclasses to enable HS/DS in D3D11
	*/
	static bool SupportsTessellationShaders() { return false; }

	// FRenderResource interface.
	virtual void ReleaseRHI();

	// Accessors.
	FVertexDeclarationRHIRef& GetDeclaration() { return Declaration; }
	void SetDeclaration(FVertexDeclarationRHIRef& NewDeclaration) { Declaration = NewDeclaration; }

	const FVertexDeclarationRHIRef& GetDeclaration() const { return Declaration; }
	const FVertexDeclarationRHIRef& GetPositionDeclaration() const { return PositionDeclaration; }

	virtual bool IsGPUSkinned() const { return false; }

	/** Indicates whether the vertex factory supports a position-only stream. */
	bool SupportsPositionOnlyStream() const { return !!PositionStream.Num(); }

	/** Indicates whether the vertex factory supports a null pixel shader. */
	virtual bool SupportsNullPixelShader() const { return true; }

	virtual bool RendersPrimitivesAsCameraFacingSprites() const { return false; }

	/**
	* Fill in array of strides from this factory's vertex streams without shadow/light maps
	* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
	*/
	int32 GetStreamStrides(uint32 *OutStreamStrides, bool bPadWithZeroes=true) const;
	/**
	* Fill in array of strides from this factory's position only vertex streams
	* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
	*/
	void GetPositionStreamStride(uint32 *OutStreamStrides) const;

	/**
	 * Get a bitmask representing the visibility of each FMeshBatch element.
	 * FMeshBatch.bRequiresPerElementVisibility must be set for this to be called.
	 */
	virtual uint64 GetStaticBatchElementVisibility(const class FSceneView& View, const struct FMeshBatch* Batch) const { return 1; }

	bool NeedsDeclaration() const { return bNeedsDeclaration; }
protected:

	/**
	 * Creates a vertex element for a vertex stream components.  Adds a unique stream index for the vertex buffer used by the component.
	 * @param Component - The vertex stream component.
	 * @param AttributeIndex - The attribute index to which the stream component is bound.
	 * @return The vertex element which corresponds to Component.
	 */
	FVertexElement AccessStreamComponent(const FVertexStreamComponent& Component,uint8 AttributeIndex);

	/**
	 * Creates a vertex element for a position vertex stream component.  Adds a unique position stream index for the vertex buffer used by the component.
	 * @param Component - The position vertex stream component.
	 * @param Usage - The vertex element usage semantics.
	 * @param AttributeIndex - The attribute index to which the stream component is bound.
	 * @return The vertex element which corresponds to Component.
	 */
	FVertexElement AccessPositionStreamComponent(const FVertexStreamComponent& Component,uint8 AttributeIndex);

	/**
	 * Initializes the vertex declaration.
	 * @param Elements - The elements of the vertex declaration.
	 */
	void InitDeclaration(FVertexDeclarationElementList& Elements);

	/**
	 * Initializes the position-only vertex declaration.
	 * @param Elements - The elements of the vertex declaration.
	 */
	void InitPositionDeclaration(const FVertexDeclarationElementList& Elements);

	/**
	 * Information needed to set a vertex stream.
	 */
	struct FVertexStream
	{
		const FVertexBuffer* VertexBuffer;
		uint32 Stride;
		uint32 Offset;
		bool bUseInstanceIndex;
		bool bSetByVertexFactoryInSetMesh; // Do not call SetStreamSource FVertexFactory::Set

		friend bool operator==(const FVertexStream& A,const FVertexStream& B)
		{
			return A.VertexBuffer == B.VertexBuffer && A.Stride == B.Stride && A.Offset == B.Offset && A.bUseInstanceIndex == B.bUseInstanceIndex;
		}

		FVertexStream()
			: VertexBuffer(nullptr)
			, Stride(0)
			, Offset(0)
			, bUseInstanceIndex(false)
			, bSetByVertexFactoryInSetMesh(false)
		{
		}
	};

	/** The vertex streams used to render the factory. */
	TArray<FVertexStream,TFixedAllocator<MaxVertexElementCount> > Streams;

	/* VF can explicitly set this to false to avoid errors without decls; this is for VFs that fetch from buffers directly (e.g. Niagara) */
	bool bNeedsDeclaration;

private:

	/** The position only vertex stream used to render the factory during depth only passes. */
	TArray<FVertexStream,TFixedAllocator<MaxVertexElementCount> > PositionStream;

	/** The RHI vertex declaration used to render the factory normally. */
	FVertexDeclarationRHIRef Declaration;

	/** The RHI vertex declaration used to render the factory during depth only passes. */
	FVertexDeclarationRHIRef PositionDeclaration;
};

/**
 * An encapsulation of the vertex factory parameters for a shader.
 * Handles serialization and binding of the vertex factory parameters for the shader's vertex factory type.
 */
class SHADERCORE_API FVertexFactoryParameterRef
{
public:
	FVertexFactoryParameterRef(FVertexFactoryType* InVertexFactoryType,const FShaderParameterMap& ParameterMap, EShaderFrequency InShaderFrequency);

	FVertexFactoryParameterRef():
		Parameters(NULL),
		VertexFactoryType(NULL)
	{}

	~FVertexFactoryParameterRef()
	{
		delete Parameters;
	}

	void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const class FSceneView& View,const struct FMeshBatchElement& BatchElement,uint32 DataFlags) const
	{
		if(Parameters)
		{
			Parameters->SetMesh(RHICmdList, Shader,VertexFactory,View,BatchElement,DataFlags);
		}
	}

	const FVertexFactoryType* GetVertexFactoryType() const { return VertexFactoryType; }

	/** Returns the hash of the vertex factory shader file that this shader was compiled with. */
	const FSHAHash& GetHash() const;

	friend SHADERCORE_API bool operator<<(FArchive& Ar,FVertexFactoryParameterRef& Ref);

	uint32 GetAllocatedSize() const
	{
		return Parameters ? Parameters->GetSize() : 0;
	}

private:
	FVertexFactoryShaderParameters* Parameters;
	FVertexFactoryType* VertexFactoryType;
	EShaderFrequency ShaderFrequency;

	// Hash of the vertex factory's source file at shader compile time, used by the automatic versioning system to detect changes
	FSHAHash VFHash;
};
