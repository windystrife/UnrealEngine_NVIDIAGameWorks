// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	VectorField.cpp: Implementation of vector fields.
==============================================================================*/

#include "VectorField.h"
#include "PrimitiveViewRelevance.h"
#include "RenderingThread.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "PrimitiveSceneProxy.h"
#include "Containers/ResourceArray.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "SceneManagement.h"
#include "Engine/CollisionProfile.h"
#include "ComponentReregisterContext.h"
#include "VectorFieldVisualization.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"
#include "FXSystem.h"
#include "VectorField/VectorField.h"
#include "VectorField/VectorFieldAnimated.h"
#include "VectorField/VectorFieldStatic.h"
#include "Components/VectorFieldComponent.h"
#include "PrimitiveSceneProxy.h"

#if WITH_EDITORONLY_DATA
	#include "EditorFramework/AssetImportData.h"
#endif
	
#define MAX_GLOBAL_VECTOR_FIELDS (16)
DEFINE_LOG_CATEGORY(LogVectorField)

/*------------------------------------------------------------------------------
	FVectorFieldResource implementation.
------------------------------------------------------------------------------*/

/**
 * Release RHI resources.
 */
void FVectorFieldResource::ReleaseRHI()
{
	VolumeTextureRHI.SafeRelease();
}

/*------------------------------------------------------------------------------
	FVectorFieldInstance implementation.
------------------------------------------------------------------------------*/

/** Destructor. */
FVectorFieldInstance::~FVectorFieldInstance()
{
	if (Resource && bInstancedResource)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDestroyVectorFieldResourceCommand,
			FVectorFieldResource*,Resource,Resource,
		{
			Resource->ReleaseResource();
			delete Resource;
		});
		Resource = NULL;
	}
}

/**
 * Initializes the instance for the given resource.
 * @param InResource - The resource to be used by this instance.
 * @param bInstanced - true if the resource is instanced and ownership is being transferred.
 */
void FVectorFieldInstance::Init(FVectorFieldResource* InResource, bool bInstanced)
{
	check(Resource == NULL);
	Resource = InResource;
	bInstancedResource = bInstanced;
}

/**
 * Update the transforms for this vector field instance.
 * @param LocalToWorld - Transform from local space to world space.
 */
void FVectorFieldInstance::UpdateTransforms(const FMatrix& LocalToWorld)
{
	check(Resource);
	const FVector VolumeOffset = Resource->LocalBounds.Min;
	const FVector VolumeScale = Resource->LocalBounds.Max - Resource->LocalBounds.Min;
	VolumeToWorldNoScale = LocalToWorld.GetMatrixWithoutScale().RemoveTranslation();
	VolumeToWorld = FScaleMatrix(VolumeScale) * FTranslationMatrix(VolumeOffset)
		* LocalToWorld;
	WorldToVolume = VolumeToWorld.InverseFast();
}

/*------------------------------------------------------------------------------
	UVectorField implementation.
------------------------------------------------------------------------------*/


UVectorField::UVectorField(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Intensity = 1.0f;
}

/**
 * Initializes an instance for use with this vector field.
 */
void UVectorField::InitInstance(class FVectorFieldInstance* Instance, bool bPreviewInstance)
{
	UE_LOG(LogVectorField, Fatal,TEXT("%s must override InitInstance."), *(GetClass()->GetName()));
}

/*------------------------------------------------------------------------------
	UVectorFieldStatic implementation.
------------------------------------------------------------------------------*/


/**
 * Bulk data interface for initializing a static vector field volume texture.
 */
class FVectorFieldStaticResourceBulkDataInterface : public FResourceBulkDataInterface
{
public:

	/** Initialization constructor. */
	FVectorFieldStaticResourceBulkDataInterface( void* InBulkData, uint32 InBulkDataSize )
		: BulkData(InBulkData)
		, BulkDataSize(InBulkDataSize)
	{
	}

	/** 
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const override
	{
		check(BulkData != NULL);
		return BulkData;
	}

	/** 
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const override
	{
		check(BulkDataSize > 0);
		return BulkDataSize;
	}

	/**
	 * Free memory after it has been used to initialize RHI resource 
	 */
	virtual void Discard() override
	{
	}

private:

	/** Pointer to the bulk data. */
	void* BulkData;
	/** Size of the bulk data in bytes. */
	uint32 BulkDataSize;
};

/**
 * Resource for static vector fields.
 */
class FVectorFieldStaticResource : public FVectorFieldResource
{
public:

	/** Initialization constructor. */
	explicit FVectorFieldStaticResource( UVectorFieldStatic* InVectorField )
		: VolumeData(NULL)
	{
		// Copy vector field properties.
		SizeX = InVectorField->SizeX;
		SizeY = InVectorField->SizeY;
		SizeZ = InVectorField->SizeZ;
		Intensity = InVectorField->Intensity;
		LocalBounds = InVectorField->Bounds;

		// Grab a copy of the static volume data.
		InVectorField->SourceData.GetCopy(&VolumeData, /*bDiscardInternalCopy=*/ true);
	}

	/** Destructor. */
	virtual ~FVectorFieldStaticResource()
	{
		FMemory::Free(VolumeData);
		VolumeData = NULL;
	}

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if (VolumeData && GSupportsTexture3D)
		{
			const uint32 DataSize = SizeX * SizeY * SizeZ * sizeof(FFloat16Color);
			FVectorFieldStaticResourceBulkDataInterface BulkDataInterface(VolumeData, DataSize);
			FRHIResourceCreateInfo CreateInfo(&BulkDataInterface);
			VolumeTextureRHI = RHICreateTexture3D(
				SizeX, SizeY, SizeZ, PF_FloatRGBA,
				/*NumMips=*/ 1,
				/*Flags=*/ TexCreate_ShaderResource,
				/*BulkData=*/ CreateInfo );
			FMemory::Free(VolumeData);
			VolumeData = NULL;
		}
	}

	/**
	 * Update this resource based on changes to the asset.
	 */
	void UpdateResource(UVectorFieldStatic* InVectorField)
	{
		struct FUpdateParams
		{
			int32 SizeX;
			int32 SizeY;
			int32 SizeZ;
			float Intensity;
			FBox Bounds;
			void* VolumeData;
		};

		FUpdateParams UpdateParams;
		UpdateParams.SizeX = InVectorField->SizeX;
		UpdateParams.SizeY = InVectorField->SizeY;
		UpdateParams.SizeZ = InVectorField->SizeZ;
		UpdateParams.Intensity = InVectorField->Intensity;
		UpdateParams.Bounds = InVectorField->Bounds;
		UpdateParams.VolumeData = NULL;
		InVectorField->SourceData.GetCopy(&UpdateParams.VolumeData, /*bDiscardInternalCopy=*/ true);

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FUpdateStaticVectorFieldCommand,
			FVectorFieldStaticResource*, Resource, this,
			FUpdateParams, UpdateParams, UpdateParams,
		{
			// Free any existing volume data on the resource.
			FMemory::Free(Resource->VolumeData);
			
			// Update settings on this resource.
			Resource->SizeX = UpdateParams.SizeX;
			Resource->SizeY = UpdateParams.SizeY;
			Resource->SizeZ = UpdateParams.SizeZ;
			Resource->Intensity = UpdateParams.Intensity;
			Resource->LocalBounds = UpdateParams.Bounds;
			Resource->VolumeData = UpdateParams.VolumeData;

			// Update RHI resources.
			Resource->UpdateRHI();
		});
	}

private:

	/** Static volume texture data. */
	void* VolumeData;
};

UVectorFieldStatic::UVectorFieldStatic(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVectorFieldStatic::InitInstance(FVectorFieldInstance* Instance, bool bPreviewInstance)
{
	Instance->Init(Resource, /*bInstanced=*/ false);
}


void UVectorFieldStatic::InitResource()
{
	check(Resource == NULL);
	Resource = new FVectorFieldStaticResource( this );
	BeginInitResource( Resource );
}


void UVectorFieldStatic::UpdateResource()
{
	check(Resource != NULL);
	FVectorFieldStaticResource* StaticResource = (FVectorFieldStaticResource*)Resource;
	StaticResource->UpdateResource(this);
}


void UVectorFieldStatic::ReleaseResource()
{
	if ( Resource != NULL )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReleaseVectorFieldCommand,
			FRenderResource*,Resource,Resource,
		{
			Resource->ReleaseResource();
			delete Resource;
		});
	}
	Resource = NULL;
}

void UVectorFieldStatic::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Store bulk data inline for streaming (to prevent PostLoad spikes)
	if (Ar.IsCooking())
	{
		SourceData.SetBulkDataFlags(BULKDATA_ForceInlinePayload | BULKDATA_SingleUse);
	}

	SourceData.Serialize(Ar,this);
}

void UVectorFieldStatic::PostLoad()
{
	Super::PostLoad();

	if ( !IsTemplate() )
	{
		InitResource();
	}

#if WITH_EDITORONLY_DATA
	if (!SourceFilePath_DEPRECATED.IsEmpty() && AssetImportData)
	{
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(SourceFilePath_DEPRECATED));
		AssetImportData->SourceData = MoveTemp(Info);
	}
#endif
}

void UVectorFieldStatic::BeginDestroy()
{
	ReleaseResource();
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UVectorFieldStatic::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateResource();
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UVectorFieldStatic::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}

	Super::GetAssetRegistryTags(OutTags);
}

void UVectorFieldStatic::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	Super::PostInitProperties();
}
#endif

class FVectorFieldCollectorResources : public FOneFrameResource
{
public:
	FVectorFieldVisualizationVertexFactory VisualizationVertexFactory;

	virtual ~FVectorFieldCollectorResources()
	{
		VisualizationVertexFactory.ReleaseResource();
	}
};

/*------------------------------------------------------------------------------
	Scene proxy for visualizing vector fields.
------------------------------------------------------------------------------*/

class FVectorFieldSceneProxy : public FPrimitiveSceneProxy
{
public:

	/** Initialization constructor. */
	explicit FVectorFieldSceneProxy( UVectorFieldComponent* VectorFieldComponent )
		: FPrimitiveSceneProxy(VectorFieldComponent)
	{
		bWillEverBeLit = false;
		VectorFieldInstance = VectorFieldComponent->VectorFieldInstance;
		check(VectorFieldInstance->Resource);
		check(VectorFieldInstance);
	}

	/** Destructor. */
	~FVectorFieldSceneProxy()
	{
		VisualizationVertexFactory.ReleaseResource();
	}

	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void CreateRenderThreadResources() override
	{
		VisualizationVertexFactory.InitResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{	
		QUICK_SCOPE_CYCLE_COUNTER( STAT_VectorFieldSceneProxy_GetDynamicMeshElements );

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				DrawVectorFieldBounds(PDI, View, VectorFieldInstance);

				// Draw a visualization of the vectors contained in the field when selected.
				if (IsSelected() || View->Family->EngineShowFlags.VectorFields)
				{
					FVectorFieldCollectorResources& CollectorResources = Collector.AllocateOneFrameResource<FVectorFieldCollectorResources>();
					CollectorResources.VisualizationVertexFactory.InitResource();

					GetVectorFieldMesh(&CollectorResources.VisualizationVertexFactory, VectorFieldInstance, ViewIndex, Collector);
				}
			}
		}
	}

	/**
	 * Computes view relevance for this scene proxy.
	 */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View); 
		Result.bDynamicRelevance = true;
		Result.bOpaqueRelevance = true;
		return Result;
	}

	/**
	 * Computes the memory footprint of this scene proxy.
	 */
	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

private:

	/** The vector field instance which this proxy is visualizing. */
	FVectorFieldInstance* VectorFieldInstance;
	/** Vertex factory for visualization. */
	FVectorFieldVisualizationVertexFactory VisualizationVertexFactory;
};

/*------------------------------------------------------------------------------
	UVectorFieldComponent implementation.
------------------------------------------------------------------------------*/
UVectorFieldComponent::UVectorFieldComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bHiddenInGame = true;
	Intensity = 1.0f;
}


FPrimitiveSceneProxy* UVectorFieldComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if (VectorFieldInstance)
	{
		Proxy = new FVectorFieldSceneProxy(this);
	}
	return Proxy;
}

FBoxSphereBounds UVectorFieldComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector(0.0f);
	NewBounds.BoxExtent = FVector(0.0f);
	NewBounds.SphereRadius = 0.0f;

	if ( VectorField )
	{
		VectorField->Bounds.GetCenterAndExtents( NewBounds.Origin, NewBounds.BoxExtent );
		NewBounds.SphereRadius = NewBounds.BoxExtent.Size();
	}

	return NewBounds.TransformBy( LocalToWorld );
}


void UVectorFieldComponent::OnRegister()
{
	Super::OnRegister();

	if (VectorField)
	{
		if (bPreviewVectorField)
		{
			FVectorFieldInstance* Instance = new FVectorFieldInstance();
			VectorField->InitInstance(Instance, /*bPreviewInstance=*/ true);
			Instance->UpdateTransforms(GetComponentTransform().ToMatrixWithScale());
			VectorFieldInstance = Instance;
		}
		else
		{
			UWorld* World = GetWorld();
			if (World && World->Scene && World->Scene->GetFXSystem())
			{
				// Store the FX system for the world in which this component is registered.
				check(FXSystem == NULL);
				FXSystem = World->Scene->GetFXSystem();
				check(FXSystem != NULL);

				// Add this component to the FX system.
				FXSystem->AddVectorField(this);
			}
		}
	}
}


void UVectorFieldComponent::OnUnregister()
{
	if (bPreviewVectorField)
	{
		if (VectorFieldInstance)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				FDestroyVectorFieldInstanceCommand,
				FVectorFieldInstance*, VectorFieldInstance, VectorFieldInstance,
			{
				delete VectorFieldInstance;
			});
			VectorFieldInstance = NULL;
		}
	}
	else if (VectorFieldInstance)
	{
		check(FXSystem != NULL);
		// Remove the component from the FX system.
		FXSystem->RemoveVectorField(this);
	}
	FXSystem = NULL;
	Super::OnUnregister();
}


void UVectorFieldComponent::SendRenderTransform_Concurrent()
{
	Super::SendRenderTransform_Concurrent();
	if (FXSystem)
	{
		FXSystem->UpdateVectorField(this);
	}
}


void UVectorFieldComponent::SetIntensity(float NewIntensity)
{
	Intensity = NewIntensity;
	if (FXSystem)
	{
		FXSystem->UpdateVectorField(this);
	}
}


void UVectorFieldComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	static const FName IntensityPropertyName(TEXT("Intensity"));

	if (FXSystem && PropertyThatChanged
		&& PropertyThatChanged->GetFName() == IntensityPropertyName)
	{
		FXSystem->UpdateVectorField(this);
	}

	Super::PostInterpChange(PropertyThatChanged);
}

#if WITH_EDITOR
void UVectorFieldComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == TEXT("VectorField"))
	{
		if (VectorField && !VectorField->IsA(UVectorFieldStatic::StaticClass()))
		{
			VectorField = NULL;
		}
	}
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	AVectorFieldVolume implementation.
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
	Shader for constructing animated vector fields.
------------------------------------------------------------------------------*/

BEGIN_UNIFORM_BUFFER_STRUCT( FCompositeAnimatedVectorFieldUniformParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, FrameA )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, FrameB )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector, VoxelSize )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, FrameLerp )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, NoiseScale )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, NoiseMax )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, Op )
END_UNIFORM_BUFFER_STRUCT( FCompositeAnimatedVectorFieldUniformParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FCompositeAnimatedVectorFieldUniformParameters,TEXT("CVF"));

typedef TUniformBufferRef<FCompositeAnimatedVectorFieldUniformParameters> FCompositeAnimatedVectorFieldUniformBufferRef;

/** The number of threads per axis launched to construct the animated vector field. */
#define THREADS_PER_AXIS 8

/**
 * Compute shader used to generate an animated vector field.
 */
class FCompositeAnimatedVectorFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeAnimatedVectorFieldCS,Global);
public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("THREADS_X"), THREADS_PER_AXIS );
		OutEnvironment.SetDefine( TEXT("THREADS_Y"), THREADS_PER_AXIS );
		OutEnvironment.SetDefine( TEXT("THREADS_Z"), THREADS_PER_AXIS );
		OutEnvironment.SetDefine( TEXT("COMPOSITE_ANIMATED"), 1 );
	}

	/** Default constructor. */
	FCompositeAnimatedVectorFieldCS()
	{
	}

	/** Initialization constructor. */
	explicit FCompositeAnimatedVectorFieldCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		AtlasTexture.Bind( Initializer.ParameterMap, TEXT("AtlasTexture") );
		AtlasTextureSampler.Bind( Initializer.ParameterMap, TEXT("AtlasTextureSampler") );
		NoiseVolumeTexture.Bind( Initializer.ParameterMap, TEXT("NoiseVolumeTexture") );
		NoiseVolumeTextureSampler.Bind( Initializer.ParameterMap, TEXT("NoiseVolumeTextureSampler") );
		OutVolumeTexture.Bind( Initializer.ParameterMap, TEXT("OutVolumeTexture") );
		OutVolumeTextureSampler.Bind( Initializer.ParameterMap, TEXT("OutVolumeTextureSampler") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << AtlasTexture;
		Ar << AtlasTextureSampler;
		Ar << NoiseVolumeTexture;
		Ar << NoiseVolumeTextureSampler;
		Ar << OutVolumeTexture;
		Ar << OutVolumeTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set parameters for this shader.
	 * @param UniformBuffer - Uniform buffer containing parameters for compositing vector fields.
	 * @param AtlasTextureRHI - The atlas texture with which to create the vector field.
	 * @param NoiseVolumeTextureRHI - The volume texture to use to add noise to the vector field.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FCompositeAnimatedVectorFieldUniformBufferRef& UniformBuffer,
		FTextureRHIParamRef AtlasTextureRHI,
		FTextureRHIParamRef NoiseVolumeTextureRHI )
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		FSamplerStateRHIParamRef SamplerStateLinear = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FCompositeAnimatedVectorFieldUniformParameters>(), UniformBuffer );
		SetTextureParameter(RHICmdList, ComputeShaderRHI, AtlasTexture, AtlasTextureSampler, SamplerStateLinear, AtlasTextureRHI );
		SetTextureParameter(RHICmdList, ComputeShaderRHI, NoiseVolumeTexture, NoiseVolumeTextureSampler, SamplerStateLinear, NoiseVolumeTextureRHI );
	}

	/**
	 * Set output buffer for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef VolumeTextureUAV)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutVolumeTexture.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVolumeTexture.GetBaseIndex(), VolumeTextureUAV);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutVolumeTexture.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVolumeTexture.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** Vector field volume textures to composite. */
	FShaderResourceParameter AtlasTexture;
	FShaderResourceParameter AtlasTextureSampler;
	/** Volume texture to sample as noise. */
	FShaderResourceParameter NoiseVolumeTexture;
	FShaderResourceParameter NoiseVolumeTextureSampler;
	/** The global vector field volume texture to write to. */
	FShaderResourceParameter OutVolumeTexture;
	FShaderResourceParameter OutVolumeTextureSampler;
};
IMPLEMENT_SHADER_TYPE(,FCompositeAnimatedVectorFieldCS,TEXT("/Engine/Private/VectorFieldCompositeShaders.usf"),TEXT("CompositeAnimatedVectorField"),SF_Compute);

/*------------------------------------------------------------------------------
	Animated vector field asset.
------------------------------------------------------------------------------*/


enum
{
	/** Minimum volume size used for animated vector fields. */
	MIN_ANIMATED_VECTOR_FIELD_SIZE = 16,
	/** Maximum volume size used for animated vector fields. */
	MAX_ANIMATED_VECTOR_FIELD_SIZE = 64
};

class FVectorFieldAnimatedResource : public FVectorFieldResource
{
public:

	/** Unordered access view in to the volume texture. */
	FUnorderedAccessViewRHIRef VolumeTextureUAV;
	/** The animated vector field asset. */
	UVectorFieldAnimated* AnimatedVectorField;
	/** The accumulated frame time of the animation. */
	float FrameTime;

	/** Initialization constructor. */
	explicit FVectorFieldAnimatedResource(UVectorFieldAnimated* InVectorField)
		: AnimatedVectorField(InVectorField)
		, FrameTime(0.0f)
	{
		SizeX = InVectorField->VolumeSizeX;
		SizeY = InVectorField->VolumeSizeY;
		SizeZ = InVectorField->VolumeSizeZ;
		Intensity = InVectorField->Intensity;
		LocalBounds = InVectorField->Bounds;
	}

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if (GSupportsTexture3D)
		{
			check(SizeX > 0);
			check(SizeY > 0);
			check(SizeZ > 0);
			UE_LOG(LogVectorField,Verbose,TEXT("InitRHI for 0x%016x %dx%dx%d"),(PTRINT)this,SizeX,SizeY,SizeZ);

			uint32 TexCreateFlags = 0;
			if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
			{
				TexCreateFlags = TexCreate_ShaderResource | TexCreate_UAV;
			}

			FRHIResourceCreateInfo CreateInfo;
			VolumeTextureRHI = RHICreateTexture3D(
				SizeX, SizeY, SizeZ,
				PF_FloatRGBA,
				/*NumMips=*/ 1,
				TexCreateFlags,
				CreateInfo);

			if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
			{
				VolumeTextureUAV = RHICreateUnorderedAccessView(VolumeTextureRHI);
			}
		}
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		VolumeTextureUAV.SafeRelease();
		FVectorFieldResource::ReleaseRHI();
	}

	/**
	 * Updates the vector field.
	 * @param DeltaSeconds - Elapsed time since the last update.
	 */
	virtual void Update(FRHICommandListImmediate& RHICmdList, float DeltaSeconds) override
	{
		check(IsInRenderingThread());

		if (GetFeatureLevel() == ERHIFeatureLevel::SM5 && AnimatedVectorField && AnimatedVectorField->Texture && AnimatedVectorField->Texture->Resource)
		{
			SCOPED_DRAW_EVENT(RHICmdList, AnimateVectorField);

			// Move frame time forward.
			FrameTime += AnimatedVectorField->FramesPerSecond * DeltaSeconds;

			// Compute the two frames to lerp.
			const int32 FrameA_Unclamped = FMath::TruncToInt(FrameTime);
			const int32 FrameA = AnimatedVectorField->bLoop ?
				(FrameA_Unclamped % AnimatedVectorField->FrameCount) :
				FMath::Min<int32>(FrameA_Unclamped, AnimatedVectorField->FrameCount - 1);
			const int32 FrameA_X = FrameA % AnimatedVectorField->SubImagesX;
			const int32 FrameA_Y = FrameA / AnimatedVectorField->SubImagesX;

			const int32 FrameB_Unclamped = FrameA + 1;
			const int32 FrameB = AnimatedVectorField->bLoop ?
				(FrameB_Unclamped % AnimatedVectorField->FrameCount) :
				FMath::Min<int32>(FrameB_Unclamped, AnimatedVectorField->FrameCount - 1);
			const int32 FrameB_X = FrameB % AnimatedVectorField->SubImagesX;
			const int32 FrameB_Y = FrameB / AnimatedVectorField->SubImagesX;

			FCompositeAnimatedVectorFieldUniformParameters Parameters;
			const FVector2D AtlasScale(
				1.0f / AnimatedVectorField->SubImagesX,
				1.0f / AnimatedVectorField->SubImagesY);
			Parameters.FrameA = FVector4(
				AtlasScale.X,
				AtlasScale.Y,
				FrameA_X * AtlasScale.X,
				FrameA_Y * AtlasScale.Y );
			Parameters.FrameB = FVector4(
				AtlasScale.X,
				AtlasScale.Y,
				FrameB_X * AtlasScale.X,
				FrameB_Y * AtlasScale.Y );
			Parameters.VoxelSize = FVector(1.0f / SizeX, 1.0f / SizeY, 1.0f / SizeZ);
			Parameters.FrameLerp = FMath::Fractional(FrameTime);
			Parameters.NoiseScale = AnimatedVectorField->NoiseScale;
			Parameters.NoiseMax = AnimatedVectorField->NoiseMax;
			Parameters.Op = (uint32)AnimatedVectorField->ConstructionOp;

			FCompositeAnimatedVectorFieldUniformBufferRef UniformBuffer = 
				FCompositeAnimatedVectorFieldUniformBufferRef::CreateUniformBufferImmediate(Parameters, UniformBuffer_SingleDraw);

			TShaderMapRef<FCompositeAnimatedVectorFieldCS> CompositeCS(GetGlobalShaderMap(GetFeatureLevel()));
			FTextureRHIParamRef NoiseVolumeTextureRHI = GBlackVolumeTexture->TextureRHI;
			if (AnimatedVectorField->NoiseField && AnimatedVectorField->NoiseField->Resource)
			{
				NoiseVolumeTextureRHI = AnimatedVectorField->NoiseField->Resource->VolumeTextureRHI;
			}

			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, VolumeTextureUAV);
			RHICmdList.SetComputeShader(CompositeCS->GetComputeShader());
			CompositeCS->SetOutput(RHICmdList, VolumeTextureUAV);
			/// ?
			CompositeCS->SetParameters(
				RHICmdList,
				UniformBuffer,
				AnimatedVectorField->Texture->Resource->TextureRHI,
				NoiseVolumeTextureRHI );
			DispatchComputeShader(
				RHICmdList,
				*CompositeCS,
				SizeX / THREADS_PER_AXIS,
				SizeY / THREADS_PER_AXIS,
				SizeZ / THREADS_PER_AXIS );
			CompositeCS->UnbindBuffers(RHICmdList);
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, VolumeTextureUAV);
		}
	}

	/**
	 * Resets the vector field simulation.
	 */
	virtual void ResetVectorField() override
	{
		FrameTime = 0.0f;
	}
};

UVectorFieldAnimated::UVectorFieldAnimated(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VolumeSizeX = 16;
	VolumeSizeY = 16;
	VolumeSizeZ = 16;
	Bounds.Min = FVector(-0.5f, -0.5f, -0.5f);
	Bounds.Max = FVector(0.5, 0.5, 0.5);

}


void UVectorFieldAnimated::InitInstance(FVectorFieldInstance* Instance, bool bPreviewInstance)
{
	FVectorFieldAnimatedResource* Resource = new FVectorFieldAnimatedResource(this);
	if (!bPreviewInstance)
	{
		BeginInitResource(Resource);
	}
	Instance->Init(Resource, /*bInstanced=*/ true);
}

static int32 ClampVolumeSize(int32 InVolumeSize)
{
	const int32 ClampedVolumeSize = FMath::Clamp<int32>(1 << FMath::CeilLogTwo(InVolumeSize),
		MIN_ANIMATED_VECTOR_FIELD_SIZE, MAX_ANIMATED_VECTOR_FIELD_SIZE);
	return ClampedVolumeSize;
}

#if WITH_EDITOR
void UVectorFieldAnimated::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	VolumeSizeX = ClampVolumeSize(VolumeSizeX);
	VolumeSizeY = ClampVolumeSize(VolumeSizeY);
	VolumeSizeZ = ClampVolumeSize(VolumeSizeZ);
	SubImagesX = FMath::Max<int32>(SubImagesX, 1);
	SubImagesY = FMath::Max<int32>(SubImagesY, 1);
	FrameCount = FMath::Max<int32>(FrameCount, 0);

	// If the volume size changes, all components must be reattached to ensure
	// that all volumes are resized.
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == TEXT("VolumeSize"))
	{
		FGlobalComponentReregisterContext ReregisterComponents;
	}
}
#endif // WITH_EDITOR
