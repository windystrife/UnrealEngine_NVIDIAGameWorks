// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
FlexFluidSurfaceComponent.cpp: Fluid surface implementation.
=============================================================================*/

#include "PhysicsEngine/FlexFluidSurfaceComponent.h"
#include "PhysicsEngine/FlexContainerInstance.h"
#include "PhysicsEngine/FlexFluidSurface.h"
#include "PhysicsEngine/FlexFluidSurfaceActor.h"

#include "Components/BillboardComponent.h"
#include "Net/UnrealNetwork.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "PhysicsPublic.h"
#include "ParticleEmitterInstances.h"
#include "FlexFluidSurfaceSceneProxy.h"

/*=============================================================================
Helper
=============================================================================*/

const FTexture2DRHIRef& GetTexture(TRefCountPtr<IPooledRenderTarget>& RenderTarget)
{
	return (const FTexture2DRHIRef&)RenderTarget->GetRenderTargetItem().ShaderResourceTexture;
}

/*=============================================================================
FFlexFluidSurfaceVertexFactory
=============================================================================*/

class FFlexFluidSurfaceVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FFlexFluidSurfaceVertexFactory);

public:
	struct DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;
	};

	FFlexFluidSurfaceVertexFactory(FFlexFluidSurfaceSceneProxy* InProxy) : FVertexFactory(), Proxy(InProxy) {}

	// FRenderResource interface.
	virtual void InitRHI() override
	{
		VertexBuffer.InitResource();
		{
			uint32 Size = 4 * sizeof(FVector4);

			FRHIResourceCreateInfo CreateInfo;
			VertexBuffer.VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);

			// fill out the verts (vertices of view frustum on near plane in ndc), UE4 has z = 1 in ndc
			FVector4* Vertices = (FVector4*)RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, Size, RLM_WriteOnly);
			float zNearplaneOffset = 0.01f;
			Vertices[0] = FVector4(1, -1, 1 - zNearplaneOffset, 1);
			Vertices[1] = FVector4(1, 1, 1 - zNearplaneOffset, 1);
			Vertices[2] = FVector4(-1, -1, 1 - zNearplaneOffset, 1);
			Vertices[3] = FVector4(-1, 1, 1 - zNearplaneOffset, 1);
			RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}

		Data.PositionComponent = FVertexStreamComponent(&VertexBuffer, 0, sizeof(FVector4), VET_Float4);
		UpdateRHI();

		FVertexDeclarationElementList Elements;
		check(Data.PositionComponent.VertexBuffer != NULL);
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));

		check(Streams.Num() > 0);
		InitDeclaration(Elements);
		check(IsValidRef(GetDeclaration()));
	}

	virtual void ReleaseRHI() override
	{
		FVertexFactory::ReleaseRHI();
		VertexBuffer.ReleaseResource();
	}

	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return Material->IsUsedWithFlexFluidSurfaces() || Material->IsSpecialEngineMaterial();
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("FLEX_FLUID_SURFACE_FACTORY"), TEXT("1"));
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

public:
	FFlexFluidSurfaceSceneProxy* Proxy;

protected:
	DataType Data;
	FVertexBuffer VertexBuffer;

};

IMPLEMENT_VERTEX_FACTORY_TYPE(FFlexFluidSurfaceVertexFactory, "/Engine/Private/FlexFluidSurfaceVertexFactory.ush", true, false, true, false, false);

/*=============================================================================
FVertexFactoryShaderParameters
=============================================================================*/

class FFlexFluidSurfaceVertexFactoryShaderParametersPS : public FVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		DepthTexture.Bind(ParameterMap, TEXT("FlexFluidSurfaceDepthTexture"));
		DepthTextureSampler.Bind(ParameterMap, TEXT("FlexFluidSurfaceDepthTextureSampler"));
		ThicknessTexture.Bind(ParameterMap, TEXT("FlexFluidSurfaceThicknessTexture"));
		ThicknessTextureSampler.Bind(ParameterMap, TEXT("FlexFluidSurfaceThicknessTextureSampler"));
		ClipXYAndViewDepthToViewXY.Bind(ParameterMap, TEXT("ClipXYAndViewDepthToViewXY"));
		InvTexResScale.Bind(ParameterMap, TEXT("InvTexResScale"));
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << DepthTexture;
		Ar << DepthTextureSampler;
		Ar << ThicknessTexture;
		Ar << ThicknessTextureSampler;
		Ar << ClipXYAndViewDepthToViewXY;
		Ar << InvTexResScale;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		FFlexFluidSurfaceVertexFactory* SurfaceVF = (FFlexFluidSurfaceVertexFactory*)VertexFactory;
		FPixelShaderRHIParamRef PixelShaderRHI = Shader->GetPixelShader();
		FFlexFluidSurfaceTextures* Textures = SurfaceVF->Proxy->Textures;
		bool bDownSampledTexture = SurfaceVF->Proxy->TexResScale != 1.0f;
		FSamplerStateRHIParamRef samplerStateRHI = TStaticSamplerState<SF_Point, AM_Border, AM_Border, AM_Clamp>::GetRHI();
		
		if (DepthTexture.IsBound() && Textures)
		{
			FTextureRHIParamRef TextureRHI = bDownSampledTexture ? GetTexture(Textures->UpSampledDepth) : GetTexture(Textures->SmoothDepth);
			SetTextureParameter(RHICmdList, PixelShaderRHI, DepthTexture, DepthTextureSampler,
				samplerStateRHI, TextureRHI);
		}

		if (ThicknessTexture.IsBound() && Textures)
		{
			FTextureRHIParamRef TextureRHI = GetTexture(Textures->Thickness);
			SetTextureParameter(RHICmdList, PixelShaderRHI, ThicknessTexture, ThicknessTextureSampler,
				samplerStateRHI, TextureRHI);
		}

		if (ClipXYAndViewDepthToViewXY.IsBound())
		{
			float FOV = PI / 4.0f;
			float AspectRatio = 1.0f;

			if (View.IsPerspectiveProjection())
			{
				// Derive FOV and aspect ratio from the perspective projection matrix
				FOV = FMath::Atan(1.0f / View.ViewMatrices.GetProjectionMatrix().M[0][0]);
				AspectRatio = View.ViewMatrices.GetProjectionMatrix().M[1][1] / View.ViewMatrices.GetProjectionMatrix().M[0][0];
			}

			FVector2D VecMult(FMath::Tan(FOV*0.5f)*AspectRatio, FMath::Tan(FOV*0.5f));

			SetShaderValue(RHICmdList, PixelShaderRHI, ClipXYAndViewDepthToViewXY, VecMult);
		}

		if (InvTexResScale.IsBound())
		{
			SetShaderValue(RHICmdList, PixelShaderRHI, InvTexResScale, 1.0f / SurfaceVF->Proxy->TexResScale);
		}
	}

	virtual uint32 GetSize() const { return sizeof(*this); }

	FShaderResourceParameter DepthTexture;
	FShaderResourceParameter DepthTextureSampler;
	FShaderResourceParameter ThicknessTexture;
	FShaderResourceParameter ThicknessTextureSampler;
	FShaderParameter ClipXYAndViewDepthToViewXY;
	FShaderParameter InvTexResScale;
};

FVertexFactoryShaderParameters* FFlexFluidSurfaceVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Pixel ? new FFlexFluidSurfaceVertexFactoryShaderParametersPS() : NULL;
}

/*=============================================================================
FFlexFluidSurfaceSceneProxy
=============================================================================*/

FFlexFluidSurfaceSceneProxy::FFlexFluidSurfaceSceneProxy(UFlexFluidSurfaceComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, VertexFactory(NULL)
	, MeshBatch(NULL)
	, Textures(NULL)
	, SurfaceMaterial(NULL)
{
	bFlexFluidSurface = true;
	bVerifyUsedMaterials = false;
}

FFlexFluidSurfaceSceneProxy::~FFlexFluidSurfaceSceneProxy()
{
	if (VertexFactory)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
	}

	delete MeshBatch;

	if (Textures)
	{
		Textures->Depth.SafeRelease();
		Textures->DepthStencil.SafeRelease();
		Textures->Thickness.SafeRelease();
		Textures->SmoothDepth.SafeRelease();
		Textures->DownSampledSceneDepth.SafeRelease();
		Textures->UpSampledDepth.SafeRelease();
	}
}

void FFlexFluidSurfaceSceneProxy::CreateRenderThreadResources()
{
	check(!VertexFactory);
	VertexFactory = new FFlexFluidSurfaceVertexFactory(this);
	VertexFactory->InitResource();
	MeshBatch = new FMeshBatch();
	Textures = new FFlexFluidSurfaceTextures();
}

void ConfigureMeshBatch(FMeshBatch* MeshBatch, FMaterialRenderProxy* MaterialRenderProxy, FFlexFluidSurfaceVertexFactory* VertexFactory)
{
	MeshBatch->VertexFactory = VertexFactory;
	MeshBatch->DynamicVertexStride = 0;
	MeshBatch->ReverseCulling = false;
	MeshBatch->UseDynamicData = false;
	MeshBatch->Type = PT_TriangleStrip;
	MeshBatch->DepthPriorityGroup = SDPG_Foreground;
	MeshBatch->MaterialRenderProxy = MaterialRenderProxy;
	MeshBatch->bSelectable = false;

	FMeshBatchElement& BatchElement = MeshBatch->Elements[0];
	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = 2;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = 3;
	BatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
}

void FFlexFluidSurfaceSceneProxy::SetEmitterDynamicData_RenderThread(class FParticleSystemSceneProxy* PSysSceneProxy,
	struct FDynamicEmitterDataBase* DynamicEmitterData)
{
	check(PSysSceneProxy != nullptr);
	
	if (DynamicEmitterData != nullptr)
	{
		ParticleSystemSceneProxyArray.Add(PSysSceneProxy);
		DynamicEmitterDataArray.Add(DynamicEmitterData);
	}
	else
	{
		//clear DynamicEmitterData associated with PSysSceneProxy
		for (int32 i = ParticleSystemSceneProxyArray.Num()-1; i >= 0; --i)
		{
			if (ParticleSystemSceneProxyArray[i] == PSysSceneProxy)
			{
				ParticleSystemSceneProxyArray.RemoveAtSwap(i);
				DynamicEmitterDataArray.RemoveAtSwap(i);
			}
		}
	}
}

void FFlexFluidSurfaceSceneProxy::SetDynamicData_RenderThread(FFlexFluidSurfaceProperties SurfaceProperties)
{
	SurfaceMaterial = SurfaceProperties.Material;

	if (SurfaceMaterial)
	{
		ConfigureMeshBatch(MeshBatch, SurfaceMaterial->GetRenderProxy(false), VertexFactory);
	}

	bCastDynamicShadow = SurfaceProperties.ReceiveShadows;

	SmoothingRadius = SurfaceProperties.SmoothingRadius;
	MaxRadialSamples = SurfaceProperties.MaxRadialSamples;
	DepthEdgeFalloff = SurfaceProperties.DepthEdgeFalloff;
	ThicknessParticleScale = SurfaceProperties.ThicknessParticleScale;
	DepthParticleScale = SurfaceProperties.DepthParticleScale;
	TexResScale = SurfaceProperties.TexResScale;
}

void FFlexFluidSurfaceSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if (SurfaceMaterial)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				FMeshBatch& TmpMeshBatch = Collector.AllocateMesh();
				ConfigureMeshBatch(&TmpMeshBatch, SurfaceMaterial->GetRenderProxy(false), VertexFactory);
				Collector.AddMesh(ViewIndex, TmpMeshBatch);
			}
		}
	}
}

FPrimitiveViewRelevance FFlexFluidSurfaceSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = false;
	Result.bDynamicRelevance = true;

	if (SurfaceMaterial)
	{
		FMaterialRelevance MaterialRelevance = SurfaceMaterial->GetRelevance_Concurrent(View->FeatureLevel);
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
	}

	return Result;
}

extern TGlobalResource<FIdentityPrimitiveUniformBuffer> GIdentityPrimitiveUniformBuffer;

/*=============================================================================
UFlexFluidSurface
=============================================================================*/

UFlexFluidSurface::UFlexFluidSurface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SmoothingRadius = 10.0f;
	MaxRadialSamples = 5;
	DepthEdgeFalloff = 0.05f;
	ThicknessParticleScale = 2.0f;
	DepthParticleScale = 1.0f;
	HalfRes = false;
	ReceiveShadows = false;
	Material = NULL;
}

#if WITH_EDITOR
void UFlexFluidSurface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SmoothingRadius = FMath::Clamp(SmoothingRadius, 0.0f, 1000.0f);
	MaxRadialSamples = FMath::Clamp(MaxRadialSamples, 0, 100);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

/*=============================================================================
UFlexFluidSurfaceComponent
=============================================================================*/

UFlexFluidSurfaceComponent::UFlexFluidSurfaceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FlexFluidSurface(NULL)
	, bReferenceCountingEnabled(true)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;
}

void UFlexFluidSurfaceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Call this because bounds have changed
	UpdateComponentToWorld();

#if WITH_EDITOR
	if (GIsEditor || GIsPlayInEditorWorld)
	{
		//push all surface properties to proxy
		MarkRenderDynamicDataDirty();
	}
#endif
}

void UFlexFluidSurfaceComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();
	if (SceneProxy)
	{
		FFlexFluidSurfaceProperties SurfaceProperties;
		if (FlexFluidSurface)
		{
			UMaterialInterface* RenderMaterial = FlexFluidSurface->Material;
			if (RenderMaterial == NULL || (RenderMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_FlexFluidSurfaces) == false))
			{
				RenderMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			SurfaceProperties.Material = RenderMaterial;
			SurfaceProperties.ReceiveShadows = FlexFluidSurface->ReceiveShadows;
			SurfaceProperties.SmoothingRadius = FlexFluidSurface->SmoothingRadius;
			SurfaceProperties.MaxRadialSamples = FlexFluidSurface->MaxRadialSamples;
			SurfaceProperties.DepthEdgeFalloff = FlexFluidSurface->DepthEdgeFalloff;
			SurfaceProperties.ThicknessParticleScale = FlexFluidSurface->ThicknessParticleScale;
			SurfaceProperties.DepthParticleScale = FlexFluidSurface->DepthParticleScale;
			SurfaceProperties.TexResScale = FlexFluidSurface->HalfRes ? 0.5f: 1.0f;
		}
		else
		{
			FMemory::Memset(SurfaceProperties, 0);
		}

		// Enqueue command to send to render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FSendFlexFluidSurfaceDynamicData,
			FFlexFluidSurfaceSceneProxy*, FlexFluidSurfaceSceneProxy, (FFlexFluidSurfaceSceneProxy*)SceneProxy,
			FFlexFluidSurfaceProperties, SurfaceProperties, SurfaceProperties,
			{
				FlexFluidSurfaceSceneProxy->SetDynamicData_RenderThread(SurfaceProperties);
			});
	}
}

void UFlexFluidSurfaceComponent::SendRenderEmitterDynamicData_Concurrent(
	FParticleSystemSceneProxy* PSysSceneProxy,
	FDynamicEmitterDataBase* DynamicEmitterData)
{
	check(PSysSceneProxy != nullptr);

	if (!SceneProxy)
	{
		return;
	}
	// Enqueue command to send to render thread
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		FSendFlexFluidSurfaceEmitterDynamicData,
		FFlexFluidSurfaceSceneProxy*, FlexFluidSurfaceSceneProxy, (FFlexFluidSurfaceSceneProxy*)SceneProxy,
		FParticleSystemSceneProxy*, PSysSceneProxy, PSysSceneProxy,
		FDynamicEmitterDataBase*, DynamicEmitterData, DynamicEmitterData,
		{
			FlexFluidSurfaceSceneProxy->SetEmitterDynamicData_RenderThread(PSysSceneProxy, DynamicEmitterData);
		});
}

void UFlexFluidSurfaceComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const 
{
	Super::GetUsedMaterials(OutMaterials, bGetDebugMaterials);

	if (FlexFluidSurface && FlexFluidSurface->Material)
		OutMaterials.Add(FlexFluidSurface->Material);
}

FBoxSphereBounds UFlexFluidSurfaceComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	if (EmitterInstances.Num() > 0)
	{
		FBox NewBounds = EmitterInstances[0]->GetBoundingBox();
		for (int32 i = 1; i < EmitterInstances.Num(); i++)
		{
			NewBounds += EmitterInstances[i]->GetBoundingBox();
		}

		return FBoxSphereBounds(NewBounds);
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(0.0f, 0.0f, 0.0f), 0.0f);
	}
}

FPrimitiveSceneProxy* UFlexFluidSurfaceComponent::CreateSceneProxy()
{
	return new FFlexFluidSurfaceSceneProxy(this);
}

void UFlexFluidSurfaceComponent::RegisterEmitterInstance(struct FParticleEmitterInstance* EmitterInstance)
{
	check(EmitterInstance);
	int32 EmitterIndex = EmitterInstances.Find(EmitterInstance);
	if (EmitterIndex == INDEX_NONE) //emitters are sometimes reinitialized
	{
		EmitterInstances.Add(EmitterInstance);
		MarkRenderDynamicDataDirty();
	}
}

void UFlexFluidSurfaceComponent::UnregisterEmitterInstance(struct FParticleEmitterInstance* EmitterInstance)
{
	check(EmitterInstance);
	int32 EmitterIndex = EmitterInstances.Find(EmitterInstance);

	if (EmitterIndex != INDEX_NONE)
	{
		EmitterInstances.RemoveSingleSwap(EmitterInstance);
		MarkRenderDynamicDataDirty();

		if (bReferenceCountingEnabled && EmitterInstances.Num() == 0 && GetWorld() != NULL)
		{
			//this will destroy the actor. 
			GetWorld()->RemoveFlexFluidSurface(this);

			//no other operations should go here.
		}
	}
}

void UFlexFluidSurfaceComponent::SetEnabledReferenceCounting(bool bEnable)
{
	bReferenceCountingEnabled = bEnable;

	if (bReferenceCountingEnabled && EmitterInstances.Num() == 0 && GetWorld() != NULL)
	{
		//this will destroy the actor. 
		GetWorld()->RemoveFlexFluidSurface(this);

		//no other operations should go here.
	}
}

bool UFlexFluidSurfaceComponent::GetEnabledReferenceCounting()
{
	return bReferenceCountingEnabled;
}

/*=============================================================================
AFlexFluidSurfaceActor
=============================================================================*/

AFlexFluidSurfaceActor::AFlexFluidSurfaceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Component = ObjectInitializer.CreateDefaultSubobject<UFlexFluidSurfaceComponent>(this, TEXT("FlexFluidSurfaceComponent0"));
	RootComponent = Component;

	bHidden = false;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet() && (GetSpriteComponent() != NULL))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> FogTextureObject;
			FName ID_Fog;
			FText NAME_Fog;
			FConstructorStatics()
				: FogTextureObject(TEXT("/Engine/EditorResources/S_ExpoHeightFog"))
				, ID_Fog(TEXT("Fog"))
				, NAME_Fog(NSLOCTEXT("SpriteCategory", "Fog", "Fog"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		GetSpriteComponent()->Sprite = ConstructorStatics.FogTextureObject.Get();
		GetSpriteComponent()->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_Fog;
		GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Fog;
		
		FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepRelative, false);
		GetSpriteComponent()->AttachToComponent(Component, AttachmentRules);

	}
#endif // WITH_EDITORONLY_DATA
}

void AFlexFluidSurfaceActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	bEnabled = (Component != NULL) ? Component->bVisible : false;
}

void AFlexFluidSurfaceActor::PostActorCreated()
{
	Super::PostActorCreated();
}

void AFlexFluidSurfaceActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFlexFluidSurfaceActor, bEnabled);
}

void AFlexFluidSurfaceActor::OnRep_bEnabled()
{
	Component->SetVisibility(bEnabled);
}

/** Returns Component subobject **/
UFlexFluidSurfaceComponent* AFlexFluidSurfaceActor::GetComponent() const { return Component; }
