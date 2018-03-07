// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkyLightComponent.cpp: SkyLightComponent implementation.
=============================================================================*/

#include "Components/SkyLightComponent.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/ScopeLock.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/SkyLight.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Net/UnrealNetwork.h"
#include "Misc/MapErrors.h"
#include "ShaderCompiler.h"
#include "Components/BillboardComponent.h"
#include "ReleaseObjectVersion.h"

#define LOCTEXT_NAMESPACE "SkyLightComponent"

void FSkyTextureCubeResource::InitRHI()
{
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		FRHIResourceCreateInfo CreateInfo;
		TextureCubeRHI = RHICreateTextureCube(Size, Format, NumMips, 0, CreateInfo);
		TextureRHI = TextureCubeRHI;

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer
		(
			SF_Trilinear,
			AM_Clamp,
			AM_Clamp,
			AM_Clamp
		);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}
}

void FSkyTextureCubeResource::Release()
{
	check( IsInGameThread() );
	checkSlow(NumRefs > 0);
	if(--NumRefs == 0)
	{
		BeginReleaseResource(this);
		// Have to defer actual deletion until above rendering command has been processed, we will use the deferred cleanup interface for that
		BeginCleanup(this);
	}
}

void UWorld::UpdateAllSkyCaptures()
{
	TArray<USkyLightComponent*> UpdatedComponents;

	for (TObjectIterator<USkyLightComponent> It; It; ++It)
	{
		USkyLightComponent* CaptureComponent = *It;

		if (ContainsActor(CaptureComponent->GetOwner()) && !CaptureComponent->IsPendingKill())
		{
			// Purge cached derived data and force an update
			CaptureComponent->SetCaptureIsDirty();
			UpdatedComponents.Add(CaptureComponent);
		}
	}

	USkyLightComponent::UpdateSkyCaptureContents(this);
}

void FSkyLightSceneProxy::Initialize(
	float InBlendFraction, 
	const FSHVectorRGB3* InIrradianceEnvironmentMap, 
	const FSHVectorRGB3* BlendDestinationIrradianceEnvironmentMap,
	const float* InAverageBrightness,
	const float* BlendDestinationAverageBrightness)
{
	BlendFraction = FMath::Clamp(InBlendFraction, 0.0f, 1.0f);

	if (BlendFraction > 0 && BlendDestinationProcessedTexture != NULL)
	{
		if (BlendFraction < 1)
		{
			IrradianceEnvironmentMap = (*InIrradianceEnvironmentMap) * (1 - BlendFraction) + (*BlendDestinationIrradianceEnvironmentMap) * BlendFraction;
			AverageBrightness = *InAverageBrightness * (1 - BlendFraction) + (*BlendDestinationAverageBrightness) * BlendFraction;
		}
		else
		{
			// Blend is full destination, treat as source to avoid blend overhead in shaders
			IrradianceEnvironmentMap = *BlendDestinationIrradianceEnvironmentMap;
			AverageBrightness = *BlendDestinationAverageBrightness;
		}
	}
	else
	{
		// Blend is full source
		IrradianceEnvironmentMap = *InIrradianceEnvironmentMap;
		AverageBrightness = *InAverageBrightness;
		BlendFraction = 0;
	}
}

FSkyLightSceneProxy::FSkyLightSceneProxy(const USkyLightComponent* InLightComponent)
	: LightComponent(InLightComponent)
	, ProcessedTexture(InLightComponent->ProcessedSkyTexture)
	, BlendDestinationProcessedTexture(InLightComponent->BlendDestinationProcessedSkyTexture)
	, SkyDistanceThreshold(InLightComponent->SkyDistanceThreshold)
	, bCastShadows(InLightComponent->CastShadows)
	, bWantsStaticShadowing(InLightComponent->Mobility == EComponentMobility::Stationary)
	, bHasStaticLighting(InLightComponent->HasStaticLighting())
	, bCastVolumetricShadow(InLightComponent->bCastVolumetricShadow)
	, LightColor(FLinearColor(InLightComponent->LightColor) * InLightComponent->Intensity)
	, IndirectLightingIntensity(InLightComponent->IndirectLightingIntensity)
	, VolumetricScatteringIntensity(FMath::Max(InLightComponent->VolumetricScatteringIntensity, 0.0f))
	, OcclusionMaxDistance(InLightComponent->OcclusionMaxDistance)
	, Contrast(InLightComponent->Contrast)
	, OcclusionExponent(FMath::Clamp(InLightComponent->OcclusionExponent, .1f, 10.0f))
	, MinOcclusion(FMath::Clamp(InLightComponent->MinOcclusion, 0.0f, 1.0f))
	, OcclusionTint(InLightComponent->OcclusionTint)
	, OcclusionCombineMode(InLightComponent->OcclusionCombineMode)
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	, bCastVxgiIndirectLighting(InLightComponent->bCastVxgiIndirectLighting)
#endif
	// NVCHANGE_END: Add VXGI
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER(
		FInitSkyProxy,
		const FSHVectorRGB3*,InIrradianceEnvironmentMap,&InLightComponent->IrradianceEnvironmentMap,
		const FSHVectorRGB3*,BlendDestinationIrradianceEnvironmentMap,&InLightComponent->BlendDestinationIrradianceEnvironmentMap,
		const float*,InAverageBrightness,&InLightComponent->AverageBrightness,
		const float*,BlendDestinationAverageBrightness,&InLightComponent->BlendDestinationAverageBrightness,
		float,InBlendFraction,InLightComponent->BlendFraction,
		FSkyLightSceneProxy*,LightSceneProxy,this,
	{
		// Only access the irradiance maps on the RT, even though they belong to the USkyLightComponent, 
		// Because FScene::UpdateSkyCaptureContents does not block the RT so the writes could still be in flight
		LightSceneProxy->Initialize(InBlendFraction, InIrradianceEnvironmentMap, BlendDestinationIrradianceEnvironmentMap, InAverageBrightness, BlendDestinationAverageBrightness);
	});
}

USkyLightComponent::USkyLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/SkyLight"));
		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 1.0f;
		DynamicEditorTexture = StaticTexture.Object;
		DynamicEditorTextureScale = 1.0f;
	}
#endif

	Brightness_DEPRECATED = 1;
	Intensity = 1;
	IndirectLightingIntensity = 1.0f;
	SkyDistanceThreshold = 150000;
	Mobility = EComponentMobility::Stationary;
	bLowerHemisphereIsBlack = true;
	bSavedConstructionScriptValuesValid = true;
	bHasEverCaptured = false;
	OcclusionMaxDistance = 1000;
	MinOcclusion = 0;
	OcclusionExponent = 1;
	OcclusionTint = FColor::Black;
	CubemapResolution = 128;
	LowerHemisphereColor = FLinearColor::Black;
	AverageBrightness = 1.0f;
	BlendDestinationAverageBrightness = 1.0f;
	bCastVolumetricShadow = true;
	// NVCHANGE_BEGIN: Add VXGI
	bCastVxgiIndirectLighting = true;
	// NVCHANGE_END: Add VXGI
}

FSkyLightSceneProxy* USkyLightComponent::CreateSceneProxy() const
{
	if (ProcessedSkyTexture)
	{
		return new FSkyLightSceneProxy(this);
	}

	return NULL;
}

void USkyLightComponent::SetCaptureIsDirty()
{ 
	if (bVisible && bAffectsWorld)
	{
		FScopeLock Lock(&SkyCapturesToUpdateLock);

		SkyCapturesToUpdate.AddUnique(this);

		// Mark saved values as invalid, in case a sky recapture is requested in a construction script between a save / restore of sky capture state
		bSavedConstructionScriptValuesValid = false;
	}
}

void USkyLightComponent::SanitizeCubemapSize()
{
	static const int32 MaxCubemapResolution = 1024;
	static const int32 MinCubemapResolution = 64;
	CubemapResolution = FMath::Clamp(int32(FMath::RoundUpToPowerOfTwo(CubemapResolution)), MinCubemapResolution, MaxCubemapResolution);
}

void USkyLightComponent::SetBlendDestinationCaptureIsDirty()
{ 
	if (bVisible && bAffectsWorld && BlendDestinationCubemap)
	{
		SkyCapturesToUpdateBlendDestinations.AddUnique(this); 

		// Mark saved values as invalid, in case a sky recapture is requested in a construction script between a save / restore of sky capture state
		bSavedConstructionScriptValuesValid = false;
	}
}

TArray<USkyLightComponent*> USkyLightComponent::SkyCapturesToUpdate;
TArray<USkyLightComponent*> USkyLightComponent::SkyCapturesToUpdateBlendDestinations;
FCriticalSection USkyLightComponent::SkyCapturesToUpdateLock;

void USkyLightComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	bool bHidden = false;
#if WITH_EDITORONLY_DATA
	bHidden = GetOwner() ? GetOwner()->bHiddenEdLevel : false;
#endif // WITH_EDITORONLY_DATA

	if(!ShouldComponentAddToScene())
	{
		bHidden = true;
	}

	const bool bIsValid = SourceType != SLS_SpecifiedCubemap || Cubemap != NULL;

	if (bAffectsWorld && bVisible && !bHidden && bIsValid)
	{
		// Create the light's scene proxy.
		SceneProxy = CreateSceneProxy();

		if (SceneProxy)
		{
			// Add the light to the scene.
			GetWorld()->Scene->SetSkyLight(SceneProxy);
		}
	}
}

void USkyLightComponent::PostInitProperties()
{
	// Skip default object or object belonging to a default object (eg default ASkyLight's component)
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		// Enqueue an update by default, so that newly placed components will get an update
		// PostLoad will undo this for components loaded from disk
		FScopeLock Lock(&SkyCapturesToUpdateLock);
		SkyCapturesToUpdate.AddUnique(this);
	}

	Super::PostInitProperties();
}

void USkyLightComponent::PostLoad()
{
	Super::PostLoad();

	SanitizeCubemapSize();

	// All components are queued for update on creation by default, remove if needed
	if (!bVisible || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		FScopeLock Lock(&SkyCapturesToUpdateLock);
		SkyCapturesToUpdate.Remove(this);
	}
}

/** 
 * Fast path for updating light properties that doesn't require a re-register,
 * Which would otherwise cause the scene's static draw lists to be recreated.
 */
void USkyLightComponent::UpdateLimitedRenderingStateFast()
{
	if (SceneProxy)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
			FFastUpdateSkyLightCommand,
			FSkyLightSceneProxy*,LightSceneProxy,SceneProxy,
			FLinearColor,LightColor,FLinearColor(LightColor) * Intensity,
			float,IndirectLightingIntensity,IndirectLightingIntensity,
			float,VolumetricScatteringIntensity,VolumetricScatteringIntensity,
		{
			LightSceneProxy->LightColor = LightColor;
			LightSceneProxy->IndirectLightingIntensity = IndirectLightingIntensity;
			LightSceneProxy->VolumetricScatteringIntensity = VolumetricScatteringIntensity;
		});
	}
}

/** 
* This is called when property is modified by InterpPropertyTracks
*
* @param PropertyThatChanged	Property that changed
*/
void USkyLightComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	static FName LightColorName(TEXT("LightColor"));
	static FName IntensityName(TEXT("Intensity"));
	static FName IndirectLightingIntensityName(TEXT("IndirectLightingIntensity"));
	static FName VolumetricScatteringIntensityName(TEXT("VolumetricScatteringIntensity"));

	FName PropertyName = PropertyThatChanged->GetFName();
	if (PropertyName == LightColorName
		|| PropertyName == IntensityName
		|| PropertyName == IndirectLightingIntensityName
		|| PropertyName == VolumetricScatteringIntensityName)
	{
		UpdateLimitedRenderingStateFast();
	}
	else
	{
		Super::PostInterpChange(PropertyThatChanged);
	}
}

void USkyLightComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();

	if (SceneProxy)
	{
		GetWorld()->Scene->DisableSkyLight(SceneProxy);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDestroySkyLightCommand,
			FSkyLightSceneProxy*,LightSceneProxy,SceneProxy,
		{
			delete LightSceneProxy;
		});

		SceneProxy = NULL;
	}
}

#if WITH_EDITOR
void USkyLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SanitizeCubemapSize();
	SetCaptureIsDirty();
}

bool USkyLightComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (FCString::Strcmp(*PropertyName, TEXT("Cubemap")) == 0
			|| FCString::Strcmp(*PropertyName, TEXT("SourceCubemapAngle")) == 0)
		{
			return SourceType == SLS_SpecifiedCubemap;
		}

		if (FCString::Strcmp(*PropertyName, TEXT("LowerHemisphereColor")) == 0)
		{
			return bLowerHemisphereIsBlack;
		}

		if (FCString::Strcmp(*PropertyName, TEXT("Contrast")) == 0
			|| FCString::Strcmp(*PropertyName, TEXT("OcclusionMaxDistance")) == 0
			|| FCString::Strcmp(*PropertyName, TEXT("MinOcclusion")) == 0
			|| FCString::Strcmp(*PropertyName, TEXT("OcclusionTint")) == 0)
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
			return Mobility == EComponentMobility::Movable && CastShadows && CVar->GetValueOnGameThread() != 0;
		}
	}

	return Super::CanEditChange(InProperty);
}

void USkyLightComponent::CheckForErrors()
{
	AActor* Owner = GetOwner();

	if (Owner && bVisible && bAffectsWorld)
	{
		UWorld* ThisWorld = Owner->GetWorld();
		bool bMultipleFound = false;

		if (ThisWorld)
		{
			for (TObjectIterator<USkyLightComponent> ComponentIt; ComponentIt; ++ComponentIt)
			{
				USkyLightComponent* Component = *ComponentIt;

				if (Component != this 
					&& !Component->IsPendingKill()
					&& Component->bVisible
					&& Component->bAffectsWorld
					&& Component->GetOwner() 
					&& ThisWorld->ContainsActor(Component->GetOwner())
					&& !Component->GetOwner()->IsPendingKill())
				{
					bMultipleFound = true;
					break;
				}
			}
		}

		if (bMultipleFound)
		{
			FMessageLog("MapCheck").Error()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_MultipleSkyLights", "Multiple sky lights are active, only one can be enabled per world." )))
				->AddToken(FMapErrorToken::Create(FMapErrors::MultipleSkyLights));
		}
	}
}

#endif // WITH_EDITOR

void USkyLightComponent::BeginDestroy()
{
	// Deregister the component from the update queue
	{
		FScopeLock Lock(&SkyCapturesToUpdateLock); 
		SkyCapturesToUpdate.Remove(this);
	}
	
	SkyCapturesToUpdateBlendDestinations.Remove(this);

	// Release reference
	ProcessedSkyTexture = NULL;

	// Begin a fence to track the progress of the above BeginReleaseResource being completed on the RT
	ReleaseResourcesFence.BeginFence();

	Super::BeginDestroy();
}

bool USkyLightComponent::IsReadyForFinishDestroy()
{
	// Wait until the fence is complete before allowing destruction
	return Super::IsReadyForFinishDestroy() && ReleaseResourcesFence.IsFenceComplete();
}

/** Used to store lightmap data during RerunConstructionScripts */
class FPrecomputedSkyLightInstanceData : public FSceneComponentInstanceData
{
public:
	FPrecomputedSkyLightInstanceData(const USkyLightComponent* SourceComponent)
		: FSceneComponentInstanceData(SourceComponent)
	{}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<USkyLightComponent>(Component)->ApplyComponentInstanceData(this);
	}

	FGuid LightGuid;
	// This has to be refcounted to keep it alive during the handoff without doing a deep copy
	TRefCountPtr<FSkyTextureCubeResource> ProcessedSkyTexture;
	FSHVectorRGB3 IrradianceEnvironmentMap;
	float AverageBrightness;
};

FActorComponentInstanceData* USkyLightComponent::GetComponentInstanceData() const
{
	FPrecomputedSkyLightInstanceData* InstanceData = new FPrecomputedSkyLightInstanceData(this);
	InstanceData->LightGuid = LightGuid;
	InstanceData->ProcessedSkyTexture = ProcessedSkyTexture;

	// Block until the rendering thread has completed its writes from a previous capture
	IrradianceMapFence.Wait();
	InstanceData->IrradianceEnvironmentMap = IrradianceEnvironmentMap;
	InstanceData->AverageBrightness = AverageBrightness;

	return InstanceData;
}

void USkyLightComponent::ApplyComponentInstanceData(FPrecomputedSkyLightInstanceData* LightMapData)
{
	check(LightMapData);

	LightGuid = LightMapData->LightGuid;
	ProcessedSkyTexture = LightMapData->ProcessedSkyTexture;
	IrradianceEnvironmentMap = LightMapData->IrradianceEnvironmentMap;
	AverageBrightness = LightMapData->AverageBrightness;

	if (ProcessedSkyTexture && bSavedConstructionScriptValuesValid)
	{
		// We have valid capture state, remove the queued update
		FScopeLock Lock(&SkyCapturesToUpdateLock);
		SkyCapturesToUpdate.Remove(this);
	}

	MarkRenderStateDirty();
}

void USkyLightComponent::UpdateSkyCaptureContentsArray(UWorld* WorldToUpdate, TArray<USkyLightComponent*>& ComponentArray, bool bOperateOnBlendSource)
{
	const bool bIsCompilingShaders = GShaderCompilingManager != NULL && GShaderCompilingManager->IsCompiling();

	// Iterate backwards so we can remove elements without changing the index
	for (int32 CaptureIndex = ComponentArray.Num() - 1; CaptureIndex >= 0; CaptureIndex--)
	{
		USkyLightComponent* CaptureComponent = ComponentArray[CaptureIndex];
		AActor* Owner = CaptureComponent->GetOwner();

		if ((!Owner || !Owner->GetLevel() || (WorldToUpdate->ContainsActor(Owner) && Owner->GetLevel()->bIsVisible))
			// Only process sky capture requests once async shader compiling completes, otherwise we will capture the scene with temporary shaders
			&& (!bIsCompilingShaders || CaptureComponent->SourceType == SLS_SpecifiedCubemap))
		{
			// Only capture valid sky light components
			if (CaptureComponent->SourceType != SLS_SpecifiedCubemap || CaptureComponent->Cubemap)
			{
				if (bOperateOnBlendSource)
				{
					ensure(!CaptureComponent->ProcessedSkyTexture || CaptureComponent->ProcessedSkyTexture->GetSizeX() == CaptureComponent->ProcessedSkyTexture->GetSizeY());

					// Allocate the needed texture on first capture
					if (!CaptureComponent->ProcessedSkyTexture || CaptureComponent->ProcessedSkyTexture->GetSizeX() != CaptureComponent->CubemapResolution)
					{
						CaptureComponent->ProcessedSkyTexture = new FSkyTextureCubeResource();
						CaptureComponent->ProcessedSkyTexture->SetupParameters(CaptureComponent->CubemapResolution, FMath::CeilLogTwo(CaptureComponent->CubemapResolution) + 1, PF_FloatRGBA);
						BeginInitResource(CaptureComponent->ProcessedSkyTexture);
						CaptureComponent->MarkRenderStateDirty();
					}

					WorldToUpdate->Scene->UpdateSkyCaptureContents(CaptureComponent, CaptureComponent->bCaptureEmissiveOnly, CaptureComponent->Cubemap, CaptureComponent->ProcessedSkyTexture, CaptureComponent->AverageBrightness, CaptureComponent->IrradianceEnvironmentMap, NULL);
				}
				else
				{
					ensure(!CaptureComponent->BlendDestinationProcessedSkyTexture || CaptureComponent->BlendDestinationProcessedSkyTexture->GetSizeX() == CaptureComponent->BlendDestinationProcessedSkyTexture->GetSizeY());

					// Allocate the needed texture on first capture
					if (!CaptureComponent->BlendDestinationProcessedSkyTexture || CaptureComponent->BlendDestinationProcessedSkyTexture->GetSizeX() != CaptureComponent->CubemapResolution)
					{
						CaptureComponent->BlendDestinationProcessedSkyTexture = new FSkyTextureCubeResource();
						CaptureComponent->BlendDestinationProcessedSkyTexture->SetupParameters(CaptureComponent->CubemapResolution, FMath::CeilLogTwo(CaptureComponent->CubemapResolution) + 1, PF_FloatRGBA);
						BeginInitResource(CaptureComponent->BlendDestinationProcessedSkyTexture);
						CaptureComponent->MarkRenderStateDirty(); 
					}

					WorldToUpdate->Scene->UpdateSkyCaptureContents(CaptureComponent, CaptureComponent->bCaptureEmissiveOnly, CaptureComponent->BlendDestinationCubemap, CaptureComponent->BlendDestinationProcessedSkyTexture, CaptureComponent->BlendDestinationAverageBrightness, CaptureComponent->BlendDestinationIrradianceEnvironmentMap, NULL);
				}

				CaptureComponent->IrradianceMapFence.BeginFence();
				CaptureComponent->bHasEverCaptured = true;
				CaptureComponent->MarkRenderStateDirty();
			}

			// Only remove queued update requests if we processed it for the right world
			ComponentArray.RemoveAt(CaptureIndex);
		}
	}
}

void USkyLightComponent::UpdateSkyCaptureContents(UWorld* WorldToUpdate)
{
	if (WorldToUpdate->Scene)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_SkylightCaptures);
		if (SkyCapturesToUpdate.Num() > 0)
		{
			FScopeLock Lock(&SkyCapturesToUpdateLock);
			UpdateSkyCaptureContentsArray(WorldToUpdate, SkyCapturesToUpdate, true);
		}
		
		if (SkyCapturesToUpdateBlendDestinations.Num() > 0)
		{
			UpdateSkyCaptureContentsArray(WorldToUpdate, SkyCapturesToUpdateBlendDestinations, false);
		}
	}
}

void USkyLightComponent::CaptureEmissiveRadianceEnvironmentCubeMap(FSHVectorRGB3& OutIrradianceMap, TArray<FFloat16Color>& OutRadianceMap) const
{
	OutIrradianceMap = FSHVectorRGB3();
	if (GetScene() && (SourceType != SLS_SpecifiedCubemap || Cubemap))
	{
		float UnusedAverageBrightness = 1.0f;
		// Capture emissive scene lighting only for the lighting build
		// This is necessary to avoid a feedback loop with the last lighting build results
		GetScene()->UpdateSkyCaptureContents(this, true, Cubemap, NULL, UnusedAverageBrightness, OutIrradianceMap, &OutRadianceMap);
		// Wait until writes to OutIrradianceMap have completed
		FlushRenderingCommands();
	}
}

/** Set brightness of the light */
void USkyLightComponent::SetIntensity(float NewIntensity)
{
	// Can't set brightness on a static light
	if (AreDynamicDataChangesAllowed()
		&& Intensity != NewIntensity)
	{
		Intensity = NewIntensity;
		UpdateLimitedRenderingStateFast();
	}
}

void USkyLightComponent::SetIndirectLightingIntensity(float NewIntensity)
{
	// Can't set brightness on a static light
	if (AreDynamicDataChangesAllowed()
		&& IndirectLightingIntensity != NewIntensity)
	{
		IndirectLightingIntensity = NewIntensity;
		UpdateLimitedRenderingStateFast();
	}
}

void USkyLightComponent::SetVolumetricScatteringIntensity(float NewIntensity)
{
	// Can't set brightness on a static light
	if (AreDynamicDataChangesAllowed()
		&& VolumetricScatteringIntensity != NewIntensity)
	{
		VolumetricScatteringIntensity = NewIntensity;
		UpdateLimitedRenderingStateFast();
	}
}

/** Set color of the light */
void USkyLightComponent::SetLightColor(FLinearColor NewLightColor)
{
	FColor NewColor(NewLightColor.ToFColor(true));

	// Can't set color on a static light
	if (AreDynamicDataChangesAllowed()
		&& LightColor != NewColor)
	{
		LightColor = NewColor;
		UpdateLimitedRenderingStateFast();
	}
}

void USkyLightComponent::SetCubemap(UTextureCube* NewCubemap)
{
	// Can't set on a static light
	if (AreDynamicDataChangesAllowed()
		&& Cubemap != NewCubemap)
	{
		Cubemap = NewCubemap;
		MarkRenderStateDirty();
		// Note: this will cause the cubemap to be reprocessed including readback from the GPU
		SetCaptureIsDirty();
	}
}

void USkyLightComponent::SetCubemapBlend(UTextureCube* SourceCubemap, UTextureCube* DestinationCubemap, float InBlendFraction)
{
	if (AreDynamicDataChangesAllowed()
		&& (Cubemap != SourceCubemap || BlendDestinationCubemap != DestinationCubemap || BlendFraction != InBlendFraction)
		&& SourceType == SLS_SpecifiedCubemap)
	{
		if (Cubemap != SourceCubemap)
		{
			Cubemap = SourceCubemap;
			SetCaptureIsDirty();
		}

		if (BlendDestinationCubemap != DestinationCubemap)
		{
			BlendDestinationCubemap = DestinationCubemap;
			SetBlendDestinationCaptureIsDirty();
		}

		if (BlendFraction != InBlendFraction)
		{
			BlendFraction = InBlendFraction;

			if (SceneProxy)
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER(
					FUpdateSkyProxy,
					const FSHVectorRGB3*,InIrradianceEnvironmentMap,&IrradianceEnvironmentMap,
					const FSHVectorRGB3*,BlendDestinationIrradianceEnvironmentMap,&BlendDestinationIrradianceEnvironmentMap,
					const float*,InAverageBrightness,&AverageBrightness,
					const float*,BlendDestinationAverageBrightness,&BlendDestinationAverageBrightness,
					float,InBlendFraction,BlendFraction,
					FSkyLightSceneProxy*,LightSceneProxy,SceneProxy,
				{
					// Only access the irradiance maps on the RT, even though they belong to the USkyLightComponent, 
					// Because FScene::UpdateSkyCaptureContents does not block the RT so the writes could still be in flight
					LightSceneProxy->Initialize(InBlendFraction, InIrradianceEnvironmentMap, BlendDestinationIrradianceEnvironmentMap, InAverageBrightness, BlendDestinationAverageBrightness);
				});
			}
		}
	}
}

void USkyLightComponent::SetOcclusionTint(const FColor& InTint)
{
	// Can't set on a static light
	if (AreDynamicDataChangesAllowed()
		&& OcclusionTint != InTint)
	{
		OcclusionTint = InTint;
		MarkRenderStateDirty();
	}
}

void USkyLightComponent::SetOcclusionContrast(float InOcclusionContrast)
{
	if (AreDynamicDataChangesAllowed()
		&& Contrast != InOcclusionContrast)
	{
		Contrast = InOcclusionContrast;
		MarkRenderStateDirty();
	}
}

void USkyLightComponent::SetOcclusionExponent(float InOcclusionExponent)
{
	if (AreDynamicDataChangesAllowed()
		&& OcclusionExponent != InOcclusionExponent)
	{
		OcclusionExponent = InOcclusionExponent;
		MarkRenderStateDirty();
	}
}

void USkyLightComponent::SetMinOcclusion(float InMinOcclusion)
{
	// Can't set on a static light
	if (AreDynamicDataChangesAllowed()
		&& MinOcclusion != InMinOcclusion)
	{
		MinOcclusion = InMinOcclusion;
		MarkRenderStateDirty();
	}
}

void USkyLightComponent::OnVisibilityChanged()
{
	Super::OnVisibilityChanged();

	if (bVisible && !bHasEverCaptured)
	{
		// Capture if we are being enabled for the first time
		SetCaptureIsDirty();
		SetBlendDestinationCaptureIsDirty();
	}
}

void USkyLightComponent::RecaptureSky()
{
	SetCaptureIsDirty();
}

void USkyLightComponent::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	Super::Serialize(Ar);

	// if version is between VER_UE4_SKYLIGHT_MOBILE_IRRADIANCE_MAP and FReleaseObjectVersion::SkyLightRemoveMobileIrradianceMap then handle aborted attempt to serialize irradiance data on mobile.
	if (Ar.UE4Ver() >= VER_UE4_SKYLIGHT_MOBILE_IRRADIANCE_MAP && !(Ar.CustomVer(FReleaseObjectVersion::GUID) >= FReleaseObjectVersion::SkyLightRemoveMobileIrradianceMap))
	{
		FSHVectorRGB3 DummyIrradianceEnvironmentMap;
		Ar << DummyIrradianceEnvironmentMap;
	}
}



ASkyLight::ASkyLight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LightComponent = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLightComponent0"));
	RootComponent = LightComponent;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SkyLightTextureObject;
		FName ID_Sky;
		FText NAME_Sky;

		FConstructorStatics()
				: SkyLightTextureObject(TEXT("/Engine/EditorResources/LightIcons/SkyLight"))
				, ID_Sky(TEXT("Sky"))
			, NAME_Sky(NSLOCTEXT( "SpriteCategory", "Sky", "Sky" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

		if (GetSpriteComponent())
		{
			GetSpriteComponent()->Sprite = ConstructorStatics.SkyLightTextureObject.Get();
			GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_Sky;
			GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Sky;
			GetSpriteComponent()->SetupAttachment(LightComponent);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void ASkyLight::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( ASkyLight, bEnabled );
}

void ASkyLight::OnRep_bEnabled()
{
	LightComponent->SetVisibility(bEnabled);
}

#undef LOCTEXT_NAMESPACE
