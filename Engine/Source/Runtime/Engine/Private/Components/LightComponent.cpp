// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightComponent.cpp: LightComponent implementation.
=============================================================================*/

#include "Components/LightComponent.h"
#include "Misc/App.h"
#include "RenderingThread.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Materials/Material.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureLightProfile.h"
#include "SceneManagement.h"
#include "ComponentReregisterContext.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/BillboardComponent.h"
#include "ComponentRecreateRenderStateContext.h"

void FStaticShadowDepthMap::InitRHI()
{
	if (FApp::CanEverRender() && Data && Data->ShadowMapSizeX > 0 && Data->ShadowMapSizeY > 0 && GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		FRHIResourceCreateInfo CreateInfo;
		FTexture2DRHIRef Texture2DRHI = RHICreateTexture2D(Data->ShadowMapSizeX, Data->ShadowMapSizeY, PF_R16F, 1, 1, 0, CreateInfo);
		TextureRHI = Texture2DRHI;

		uint32 DestStride = 0;
		uint8* TextureData = (uint8*)RHILockTexture2D(Texture2DRHI, 0, RLM_WriteOnly, DestStride, false);
		uint32 RowSize = Data->ShadowMapSizeX * GPixelFormats[PF_R16F].BlockBytes;

		for (int32 Y = 0; Y < Data->ShadowMapSizeY; Y++)
		{
			FMemory::Memcpy(TextureData + DestStride * Y, ((uint8*)Data->DepthSamples.GetData()) + RowSize * Y, RowSize);
		}

		RHIUnlockTexture2D(Texture2DRHI, 0, false);
	}
}

void ULightComponentBase::SetCastShadows(bool bNewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& CastShadows != bNewValue)
	{
		CastShadows = bNewValue;
		MarkRenderStateDirty();
	}
}

FLinearColor ULightComponentBase::GetLightColor() const
{
	return FLinearColor(LightColor);
}

void ULightComponentBase::SetCastVolumetricShadow(bool bNewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& bCastVolumetricShadow != bNewValue)
	{
		bCastVolumetricShadow = bNewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_INVERSE_SQUARED_LIGHTS_DEFAULT)
	{
		Intensity = Brightness_DEPRECATED;
	}
}

/**
 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure GUIDs remains globally unique.
 */
void ULightComponentBase::PostDuplicate(EDuplicateMode::Type DuplicateMode) 
{
	Super::PostDuplicate(DuplicateMode);

	if (DuplicateMode == EDuplicateMode::Normal)
	{
		// Create new guids for light.
		UpdateLightGUIDs();
	}
}


#if WITH_EDITOR
/**
 * Called after importing property values for this object (paste, duplicate or .t3d import)
 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
 * are unsupported by the script serialization
 */
void ULightComponentBase::PostEditImport()
{
	Super::PostEditImport();
	// Create new guids for light.
	UpdateLightGUIDs();
}

void ULightComponentBase::UpdateLightSpriteTexture()
{
	if (SpriteComponent != NULL)
	{
		SpriteComponent->SetSprite(GetEditorSprite());

		float SpriteScale = GetEditorSpriteScale();
		SpriteComponent->SetRelativeScale3D(FVector(SpriteScale));
	}
}

void ULightComponentBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Update sprite 
	UpdateLightSpriteTexture();
}

#endif

/**
 * Validates light GUIDs and resets as appropriate.
 */
void ULightComponentBase::ValidateLightGUIDs()
{
	// Validate light guids.
	if( !LightGuid.IsValid() )
	{
		LightGuid = FGuid::NewGuid();
	}
}

void ULightComponentBase::UpdateLightGUIDs()
{
	LightGuid = FGuid::NewGuid();
}

bool ULightComponentBase::HasStaticLighting() const
{
	AActor* Owner = GetOwner();

	return Owner && (Mobility == EComponentMobility::Static);
}

bool ULightComponentBase::HasStaticShadowing() const
{
	AActor* Owner = GetOwner();

	return Owner && (Mobility != EComponentMobility::Movable);
}

#if WITH_EDITOR
void ULightComponentBase::OnRegister()
{
	Super::OnRegister();

	if (SpriteComponent)
	{
		SpriteComponent->SpriteInfo.Category = TEXT("Lighting");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Lighting", "Lighting");

		UpdateLightSpriteTexture();
	}
}

bool ULightComponentBase::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponentBase, VolumetricScatteringIntensity))
		{
			return Mobility != EComponentMobility::Static;
		}
	}

	return Super::CanEditChange(InProperty);
}
#endif

bool ULightComponentBase::ShouldCollideWhenPlacing() const
{
	return true;
}

FBoxSphereBounds ULightComponentBase::GetPlacementExtent() const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector::ZeroVector;
	NewBounds.BoxExtent = FVector(25.0f, 25.0f, 25.0f);
	NewBounds.SphereRadius = 12.5f;
	return NewBounds;
}


FLightSceneProxy::FLightSceneProxy(const ULightComponent* InLightComponent)
	: LightComponent(InLightComponent)
	, SceneInterface(InLightComponent->GetScene())
	, IndirectLightingScale(InLightComponent->IndirectLightingIntensity)
	, VolumetricScatteringIntensity(FMath::Max(InLightComponent->VolumetricScatteringIntensity, 0.0f))
	, ShadowResolutionScale(InLightComponent->ShadowResolutionScale)
	, ShadowBias(InLightComponent->ShadowBias)
	, ShadowSharpen(InLightComponent->ShadowSharpen)
	, ContactShadowLength(InLightComponent->ContactShadowLength)
	, MinRoughness(InLightComponent->MinRoughness)
	, LightGuid(InLightComponent->LightGuid)
	, IESTexture(0)
	, bMovable(InLightComponent->IsMovable())
	, bStaticLighting(InLightComponent->HasStaticLighting())
	, bStaticShadowing(InLightComponent->HasStaticShadowing())
	, bCastDynamicShadow(InLightComponent->CastShadows && InLightComponent->CastDynamicShadows)
	, bCastStaticShadow(InLightComponent->CastShadows && InLightComponent->CastStaticShadows)
	, bCastTranslucentShadows(InLightComponent->CastTranslucentShadows)
	, bCastVolumetricShadow(InLightComponent->bCastVolumetricShadow)
	, bCastShadowsFromCinematicObjectsOnly(InLightComponent->bCastShadowsFromCinematicObjectsOnly)
	, bAffectTranslucentLighting(InLightComponent->bAffectTranslucentLighting)
	, bUsedAsAtmosphereSunLight(InLightComponent->IsUsedAsAtmosphereSunLight())
	, bAffectDynamicIndirectLighting(InLightComponent->bAffectDynamicIndirectLighting)
	, bHasReflectiveShadowMap(InLightComponent->bAffectDynamicIndirectLighting && InLightComponent->GetLightType() == LightType_Directional)
	, bUseRayTracedDistanceFieldShadows(InLightComponent->bUseRayTracedDistanceFieldShadows)
	, bCastModulatedShadows(false)
	, bUseWholeSceneCSMForMovableObjects(false)
	, RayStartOffsetDepthScale(InLightComponent->RayStartOffsetDepthScale)
	// NVCHANGE_BEGIN: Add VXGI
	// Disable VXGI for Static and Stationary lights because Lightmass is already baking their indirect lighting
#if WITH_GFSDK_VXGI
	, bCastVxgiIndirectLighting(InLightComponent->bCastVxgiIndirectLighting && !InLightComponent->HasStaticShadowing())
#endif
	// NVCHANGE_END: Add VXGI
	, LightType(InLightComponent->GetLightType())	
	, LightingChannelMask(GetLightingChannelMaskForStruct(InLightComponent->LightingChannels))
	, ComponentName(InLightComponent->GetOwner() ? InLightComponent->GetOwner()->GetFName() : InLightComponent->GetFName())
	, LevelName(InLightComponent->GetOutermost()->GetFName())
	, StatId(InLightComponent->GetStatID(true))
	, FarShadowDistance(0)
	, FarShadowCascadeCount(0)


	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	, bEnableNVVL(InLightComponent->bEnableVolumetricLighting)
	, TessQuality(InLightComponent->TessQuality)
	, TargetRayResolution(InLightComponent->TargetRayResolution)
	, DepthBias(InLightComponent->DepthBias)
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

// NvFlow begin
	, bFlowGridShadowEnabled(InLightComponent->bFlowGridShadowEnabled)
	, FlowGridShadowChannel(InLightComponent->FlowGridShadowChannel)
// NvFlow end
{
	check(SceneInterface);

	const FLightComponentMapBuildData* MapBuildData = InLightComponent->GetLightComponentMapBuildData();
	
	if (MapBuildData && bStaticShadowing && !bStaticLighting)
	{
		ShadowMapChannel = MapBuildData->ShadowMapChannel;
	}
	else
	{
		ShadowMapChannel = INDEX_NONE;
	}

	// Use the preview channel if valid, otherwise fallback to the lighting build channel
	PreviewShadowMapChannel = InLightComponent->PreviewShadowMapChannel != INDEX_NONE ? InLightComponent->PreviewShadowMapChannel : ShadowMapChannel;

	StaticShadowDepthMap = &LightComponent->StaticShadowDepthMap;

	// Brightness in Lumens
	float LightBrightness = InLightComponent->ComputeLightBrightness();

	if(LightComponent->IESTexture)
	{
		IESTexture = LightComponent->IESTexture;
	}

	Color = FLinearColor(InLightComponent->LightColor) * LightBrightness;
	if( InLightComponent->bUseTemperature )
	{
		Color *= FLinearColor::MakeFromColorTemperature(InLightComponent->Temperature);
	}

	if(LightComponent->LightFunctionMaterial &&
		LightComponent->LightFunctionMaterial->GetMaterial()->MaterialDomain == MD_LightFunction )
	{
		LightFunctionMaterial = LightComponent->LightFunctionMaterial->GetRenderProxy(false);
	}
	else
	{
		LightFunctionMaterial = NULL;
	}

	LightFunctionScale = LightComponent->LightFunctionScale;
	LightFunctionFadeDistance = LightComponent->LightFunctionFadeDistance;
	LightFunctionDisabledBrightness = LightComponent->DisabledBrightness;
	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	Intensity = LightComponent->bUseVolumetricLightingColor ? FLinearColor(InLightComponent->VolumetricLightingColor) * InLightComponent->VolumetricLightingIntensity : Color;
	InLightComponent->GetNvVlAttenuation(AttenuationMode, AttenuationFactors);
	InLightComponent->GetNvVlFalloff(FalloffMode, FalloffAngleAndPower);
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting
}



bool FLightSceneProxy::ShouldCreatePerObjectShadowsForDynamicObjects() const
{
	// Only create per-object shadows for Stationary lights, which use static shadowing from the world and therefore need a way to integrate dynamic objects
	return HasStaticShadowing() && !HasStaticLighting();
}

/** Whether this light should create CSM for dynamic objects only (mobile renderer) */
bool FLightSceneProxy::UseCSMForDynamicObjects() const
{
	return false;
}

void FLightSceneProxy::SetTransform(const FMatrix& InLightToWorld,const FVector4& InPosition)
{
	LightToWorld = InLightToWorld;
	WorldToLight = InLightToWorld.InverseFast();
	Position = InPosition;
}

void FLightSceneProxy::SetColor(const FLinearColor& InColor)
{
	Color = InColor;
}

void FLightSceneProxy::ApplyWorldOffset(FVector InOffset)
{
	FMatrix NewLightToWorld = LightToWorld.ConcatTranslation(InOffset);
	FVector4 NewPosition = Position + InOffset;
	SetTransform(NewLightToWorld, NewPosition);
}

ULightComponentBase::ULightComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Brightness_DEPRECATED = 3.1415926535897932f;
	Intensity = 3.1415926535897932f;
	LightColor = FColor::White;
	VolumetricScatteringIntensity = 1.0f;
	bAffectsWorld = true;
	CastShadows = true;
	CastStaticShadows = true;
	CastDynamicShadows = true;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}

/**
 * Updates/ resets light GUIDs.
 */
ULightComponent::ULightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Temperature = 6500.0f;
	bUseTemperature = false;
	PreviewShadowMapChannel = INDEX_NONE;
	IndirectLightingIntensity = 1.0f;
	ShadowResolutionScale = 1.0f;
	ShadowBias = 0.5f;
	ShadowSharpen = 0.0f;
	ContactShadowLength = 0.0f;
	bUseIESBrightness = false;
	IESBrightnessScale = 1.0f;
	IESTexture = NULL;

	bAffectTranslucentLighting = true;
	LightFunctionScale = FVector(1024.0f, 1024.0f, 1024.0f);

	LightFunctionFadeDistance = 100000.0f;
	DisabledBrightness = 0.5f;
	MinRoughness = 0.08f;

	bEnableLightShaftBloom = false;
	BloomScale = .2f;
	BloomThreshold = 0;
	BloomTint = FColor::White;

	RayStartOffsetDepthScale = .003f;

	MaxDrawDistance = 0.0f;
	MaxDistanceFadeRange = 0.0f;
	bAddedToSceneVisible = false;
	// NvFlow begin
	bFlowGridShadowEnabled = false;
	FlowGridShadowChannel = 0;
	// NvFlow end

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	bEnableVolumetricLighting = false;
	TessQuality = ETessellationQuality::HIGH;
	DepthBias = 0.0f;
	TargetRayResolution = 12.0f;

	bUseVolumetricLightingColor = false;
	VolumetricLightingIntensity = 10.0f;
	VolumetricLightingColor = FColor::White;
	// NVCHANGE_END: Nvidia Volumetric Lighting

	// NVCHANGE_BEGIN: Add VXGI
	bCastVxgiIndirectLighting = false;
	// NVCHANGE_END: Add VXGI
}

bool ULightComponent::AffectsPrimitive(const UPrimitiveComponent* Primitive) const
{
	// Check whether the light affects the primitive's bounding volume.
	return AffectsBounds(Primitive->Bounds);
}

bool ULightComponent::AffectsBounds(const FBoxSphereBounds& InBounds) const
{
	return true;
}

bool ULightComponent::IsShadowCast(UPrimitiveComponent* Primitive) const
{
	if(Primitive->HasStaticLighting())
	{
		return CastShadows && CastStaticShadows;
	}
	else
	{
		return CastShadows && CastDynamicShadows;
	}
}

float ULightComponent::ComputeLightBrightness() const
{
	float LightBrightness = Intensity;

	if(IESTexture)
	{
		if(bUseIESBrightness)
		{
			LightBrightness = IESTexture->Brightness * IESBrightnessScale;
		}

		LightBrightness *= IESTexture->TextureMultiplier;
	}

	return LightBrightness;
}

void ULightComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (Ar.UE4Ver() >= VER_UE4_STATIC_SHADOW_DEPTH_MAPS)
	{
		if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MapBuildDataSeparatePackage)
		{
			FLightComponentMapBuildData* LegacyData = new FLightComponentMapBuildData();
			Ar << LegacyData->DepthMap;
			LegacyData->ShadowMapChannel = ShadowMapChannel_DEPRECATED;

			FLightComponentLegacyMapBuildData LegacyLightData;
			LegacyLightData.Id = LightGuid;
			LegacyLightData.Data = LegacyData;
			GLightComponentsWithLegacyBuildData.AddAnnotation(this, LegacyLightData);
		}
	}
}

/**
 * Called after this UObject has been serialized
 */
void ULightComponent::PostLoad()
{
	Super::PostLoad();

	if (LightFunctionMaterial && HasStaticLighting())
	{
		// Light functions can only be used on dynamic lights
		LightFunctionMaterial = NULL;
	}

	PreviewShadowMapChannel = INDEX_NONE;
	Intensity = FMath::Max(0.0f, Intensity);

	if (GetLinkerUE4Version() < VER_UE4_LIGHTCOMPONENT_USE_IES_TEXTURE_MULTIPLIER_ON_NON_IES_BRIGHTNESS)
	{
		if(IESTexture)
		{
			Intensity /= IESTexture->TextureMultiplier; // Previous version didn't apply IES texture multiplier, so cancel out
			IESBrightnessScale = FMath::Pow(IESBrightnessScale, 2.2f); // Previous version applied 2.2 gamma to brightness scale
			IESBrightnessScale /= IESTexture->TextureMultiplier; // Previous version didn't apply IES texture multiplier, so cancel out
		}
	}
}

#if WITH_EDITOR
bool ULightComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bCastShadowsFromCinematicObjectsOnly))
		{
			return Mobility == EComponentMobility::Movable;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightingChannels))
		{
			return Mobility != EComponentMobility::Static;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightFunctionMaterial)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightFunctionScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightFunctionFadeDistance)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, DisabledBrightness)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, IESTexture)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bUseIESBrightness)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, IESBrightnessScale))
		{
			if (Mobility == EComponentMobility::Static)
			{
				return false;
			}
		}
		
		const bool bIsRayStartOffset = PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, RayStartOffsetDepthScale);

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bUseRayTracedDistanceFieldShadows)
			|| bIsRayStartOffset)
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
			bool bCanEdit = CastShadows && CastDynamicShadows && Mobility != EComponentMobility::Static && CVar->GetValueOnGameThread() != 0;

			if (bIsRayStartOffset)
			{
				bCanEdit = bCanEdit && bUseRayTracedDistanceFieldShadows;
			}

			return bCanEdit;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightFunctionScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightFunctionFadeDistance)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, DisabledBrightness))
		{
			return LightFunctionMaterial != NULL;
		}

		if (PropertyName == TEXT("LightmassSettings"))
		{
			return Mobility != EComponentMobility::Movable;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, BloomScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, BloomThreshold)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, BloomTint))
		{
			return bEnableLightShaftBloom;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, Temperature))
		{
			return bUseTemperature;
		}

		// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, VolumetricLightingIntensity)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, VolumetricLightingColor))
		{
			return bEnableVolumetricLighting && bUseVolumetricLightingColor;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bUseVolumetricLightingColor)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, TargetRayResolution)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, DepthBias)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, TessQuality))
		{
			return bEnableVolumetricLighting;
		}
		// NVCHANGE_END: Nvidia Volumetric Lighting

	}

	return Super::CanEditChange(InProperty);
}

void ULightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FString PropertyName = PropertyThatChanged ? PropertyThatChanged->GetName() : TEXT("");

	Intensity = FMath::Max(0.0f, Intensity);

	if (HasStaticLighting())
	{
		// Lightmapped lights must not have light functions
		LightFunctionMaterial = NULL;
	}

	// Unbuild lighting because a property changed
	// Exclude properties that don't affect built lighting
	//@todo - make this inclusive instead of exclusive?
	if( PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, CastTranslucentShadows) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bCastShadowsFromCinematicObjectsOnly) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, CastDynamicShadows) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bAffectTranslucentLighting) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, MinRoughness) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightFunctionMaterial) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightFunctionScale) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightFunctionFadeDistance) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, DisabledBrightness) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, ShadowResolutionScale) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, ShadowBias) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, ShadowSharpen) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, ContactShadowLength) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bEnableLightShaftBloom) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, BloomScale) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, BloomThreshold) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, BloomTint) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bUseRayTracedDistanceFieldShadows) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, RayStartOffsetDepthScale) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bVisible) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightingChannels) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, VolumetricScatteringIntensity) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bCastVolumetricShadow) &&
		// NVCHANGE_BEGIN: Add VXGI
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bCastVxgiIndirectLighting) &&
		// NVCHANGE_END: Add VXGI
		// Point light properties that shouldn't unbuild lighting
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UPointLightComponent, SourceRadius) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UPointLightComponent, SoftSourceRadius) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UPointLightComponent, SourceLength) &&
		// Directional light properties that shouldn't unbuild lighting
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DynamicShadowDistanceMovableLight) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DynamicShadowDistanceStationaryLight) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DynamicShadowCascades) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, FarShadowDistance) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, FarShadowCascadeCount) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, CascadeDistributionExponent) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, CascadeTransitionFraction) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, ShadowDistanceFadeoutFraction) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, bUseInsetShadowsForMovableObjects) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DistanceFieldShadowDistance) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, LightSourceAngle) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, bEnableLightShaftOcclusion) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, OcclusionMaskDarkness) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, OcclusionDepthRange) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, LightShaftOverrideDirection) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, bCastModulatedShadows) &&
		PropertyName != GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, ModulatedShadowColor) &&
		// Properties that should only unbuild lighting for a Static light (can be changed dynamically on a Stationary light)
		(PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, Intensity) || Mobility == EComponentMobility::Static) &&
		(PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, LightColor) || Mobility == EComponentMobility::Static) &&
		(PropertyName != GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, Temperature) || Mobility == EComponentMobility::Static) )
	{
		InvalidateLightingCache();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ULightComponent::UpdateLightSpriteTexture()
{
	if( SpriteComponent != NULL )
	{
		if (HasStaticShadowing() &&
			!HasStaticLighting() &&
			bAffectsWorld &&
			CastShadows &&
			CastStaticShadows &&
			PreviewShadowMapChannel == INDEX_NONE)
		{
			UTexture2D* SpriteTexture = NULL;
			SpriteTexture = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/LightIcons/S_LightError.S_LightError"));
			SpriteComponent->SetSprite(SpriteTexture);
			SpriteComponent->SetRelativeScale3D(FVector(0.5f));
		}
		else
		{
			Super::UpdateLightSpriteTexture();
		}
	}
}

#endif // WITH_EDITOR

void ULightComponent::BeginDestroy()
{
	Super::BeginDestroy();

	BeginReleaseResource(&StaticShadowDepthMap);

	// Use a fence to keep track of when the rendering thread executes the release command
	DestroyFence.BeginFence();
}

bool ULightComponent::IsReadyForFinishDestroy()
{
	// Don't allow the light component to be destroyed until its rendering resources have been released
	return Super::IsReadyForFinishDestroy() && DestroyFence.IsFenceComplete();
}

void ULightComponent::OnRegister()
{
	Super::OnRegister();

	// Update GUIDs on attachment if they are not valid.
	ValidateLightGUIDs();
}

void ULightComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	if (bAffectsWorld)
	{
		UWorld* World = GetWorld();
		const bool bHidden = !ShouldComponentAddToScene() || !ShouldRender() || Intensity <= 0.f;
		if (!bHidden)
		{
			InitializeStaticShadowDepthMap();

			// Add the light to the scene.
			World->Scene->AddLight(this);
			bAddedToSceneVisible = true;
		}
		// Add invisible stationary lights to the scene in the editor
		// Even invisible stationary lights consume a shadowmap channel so they must be included in the stationary light overlap preview
		else if (GIsEditor 
			&& !World->IsGameWorld()
			&& CastShadows 
			&& CastStaticShadows 
			&& HasStaticShadowing()
			&& !HasStaticLighting())
		{
			InitializeStaticShadowDepthMap();

			World->Scene->AddInvisibleLight(this);
		}
	}
}

void ULightComponent::SendRenderTransform_Concurrent()
{
	// Update the scene info's transform for this light.
	GetWorld()->Scene->UpdateLightTransform(this);
	Super::SendRenderTransform_Concurrent();
}

void ULightComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	GetWorld()->Scene->RemoveLight(this);
	bAddedToSceneVisible = false;
}

/** Set brightness of the light */
void ULightComponent::SetIntensity(float NewIntensity)
{
	// Can't set brightness on a static light
	if (AreDynamicDataChangesAllowed()
		&& Intensity != NewIntensity)
	{
		Intensity = NewIntensity;

		// Use lightweight color and brightness update if possible
		UpdateColorAndBrightness();
	}
}

void ULightComponent::SetIndirectLightingIntensity(float NewIntensity)
{
	// Can't set brightness on a static light
	if (AreDynamicDataChangesAllowed()
		&& IndirectLightingIntensity != NewIntensity)
	{
		IndirectLightingIntensity = NewIntensity;

		// Use lightweight color and brightness update 
		UWorld* World = GetWorld();
		if( World && World->Scene )
		{
			//@todo - remove from scene if brightness or color becomes 0
			World->Scene->UpdateLightColorAndBrightness( this );
		}
	}
}

void ULightComponent::SetVolumetricScatteringIntensity(float NewIntensity)
{
	// Can't set brightness on a static light
	if (AreDynamicDataChangesAllowed()
		&& VolumetricScatteringIntensity != NewIntensity)
	{
		VolumetricScatteringIntensity = NewIntensity;

		// Use lightweight color and brightness update 
		UWorld* World = GetWorld();
		if( World && World->Scene )
		{
			//@todo - remove from scene if brightness or color becomes 0
			World->Scene->UpdateLightColorAndBrightness( this );
		}
	}
}

/** Set color of the light */
void ULightComponent::SetLightColor(FLinearColor NewLightColor, bool bSRGB)
{
	FColor NewColor(NewLightColor.ToFColor(bSRGB));

	// Can't set color on a static light
	if (AreDynamicDataChangesAllowed()
		&& LightColor != NewColor)
	{
		LightColor	= NewColor;

		// Use lightweight color and brightness update 
		UWorld* World = GetWorld();
		if( World && World->Scene )
		{
			//@todo - remove from scene if brightness or color becomes 0
			World->Scene->UpdateLightColorAndBrightness( this );
		}
	}
}

/** Set color temperature of the light */
void ULightComponent::SetTemperature(float NewTemperature)
{
	// Can't set color on a static light
	if (AreDynamicDataChangesAllowed()
		&& Temperature != NewTemperature)
	{
		Temperature = NewTemperature;

		// Use lightweight color and brightness update 
		UWorld* World = GetWorld();
		if( World && World->Scene )
		{
			//@todo - remove from scene if brightness or color becomes 0
			World->Scene->UpdateLightColorAndBrightness( this );
		}
	}
}

void ULightComponent::SetLightFunctionMaterial(UMaterialInterface* NewLightFunctionMaterial)
{
	// Can't set light function on a static light
	if (AreDynamicDataChangesAllowed()
		&& NewLightFunctionMaterial != LightFunctionMaterial)
	{
		LightFunctionMaterial = NewLightFunctionMaterial;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetLightFunctionScale(FVector NewLightFunctionScale)
{
	if (AreDynamicDataChangesAllowed()
		&& NewLightFunctionScale != LightFunctionScale)
	{
		LightFunctionScale = NewLightFunctionScale;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetLightFunctionFadeDistance(float NewLightFunctionFadeDistance)
{
	if (AreDynamicDataChangesAllowed()
		&& NewLightFunctionFadeDistance != LightFunctionFadeDistance)
	{
		LightFunctionFadeDistance = NewLightFunctionFadeDistance;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetLightFunctionDisabledBrightness(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& NewValue != DisabledBrightness)
	{
		DisabledBrightness = NewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetAffectDynamicIndirectLighting(bool bNewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& bAffectDynamicIndirectLighting != bNewValue)
	{
		bAffectDynamicIndirectLighting = bNewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetAffectTranslucentLighting(bool bNewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& bAffectTranslucentLighting != bNewValue)
	{
		bAffectTranslucentLighting = bNewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetEnableLightShaftBloom(bool bNewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& bEnableLightShaftBloom != bNewValue)
	{
		bEnableLightShaftBloom = bNewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetBloomScale(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& BloomScale != NewValue)
	{
		BloomScale = NewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetBloomThreshold(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& BloomThreshold != NewValue)
	{
		BloomThreshold = NewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetBloomTint(FColor NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& BloomTint != NewValue)
	{
		BloomTint = NewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetIESTexture(UTextureLightProfile* NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& IESTexture != NewValue)
	{
		IESTexture = NewValue;
		MarkRenderStateDirty();
	}
}

void ULightComponent::SetShadowBias(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& ShadowBias != NewValue)
	{
		ShadowBias = NewValue;
		MarkRenderStateDirty();
	}
}

// GetDirection
FVector ULightComponent::GetDirection() const 
{ 
	return GetComponentTransform().GetUnitAxis( EAxis::X );
}

void ULightComponent::UpdateColorAndBrightness()
{
	UWorld* World = GetWorld();
	if( World && World->Scene )
	{
		const bool bNeedsToBeAddedToScene = (!bAddedToSceneVisible && Intensity > 0.f);
		const bool bNeedsToBeRemovedFromScene = (bAddedToSceneVisible && Intensity <= 0.f);
		if (bNeedsToBeAddedToScene || bNeedsToBeRemovedFromScene)
		{
			// We may have just been set to 0 intensity or we were previously 0 intensity.
			// Mark the render state dirty to add or remove this light from the scene as necessary.
			MarkRenderStateDirty();
		}
		else if (bAddedToSceneVisible && Intensity > 0.f)
		{
			// We are already in the scene. Just update with this fast path command
			World->Scene->UpdateLightColorAndBrightness(this);
		}
	}
}

//
//	ULightComponent::InvalidateLightingCache
//

void ULightComponent::InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly)
{
	InvalidateLightingCacheInner(true);

	UWorld* World = GetWorld();
	if (GIsEditor
		&& World != NULL
		&& HasStaticShadowing()
		&& !HasStaticLighting())
	{
		ReassignStationaryLightChannels(World, false, NULL);
	}
}

/** Invalidates the light's cached lighting with the option to recreate the light Guids. */
void ULightComponent::InvalidateLightingCacheInner(bool bRecreateLightGuids)
{
	if (HasStaticLighting() || HasStaticShadowing())
	{
		// Save the light state for transactions.
		Modify();

		BeginReleaseResource(&StaticShadowDepthMap);

		if (bRecreateLightGuids)
		{
			// Create new guids for light.
			UpdateLightGUIDs();
		}
		else
		{
			ValidateLightGUIDs();
		}

		MarkRenderStateDirty();
	}
}

/** Used to store lightmap data during RerunConstructionScripts */
class FPrecomputedLightInstanceData : public FSceneComponentInstanceData
{
public:
	FPrecomputedLightInstanceData(const ULightComponent* SourceComponent)
		: FSceneComponentInstanceData(SourceComponent)
		, Transform(SourceComponent->GetComponentTransform())
		, LightGuid(SourceComponent->LightGuid)
		, PreviewShadowMapChannel(SourceComponent->PreviewShadowMapChannel)
	{}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<ULightComponent>(Component)->ApplyComponentInstanceData(this);
	}

	FTransform Transform;
	FGuid LightGuid;
	int32 PreviewShadowMapChannel;
};

FActorComponentInstanceData* ULightComponent::GetComponentInstanceData() const
{
	// Allocate new struct for holding light map data
	return new FPrecomputedLightInstanceData(this);
}

void ULightComponent::ApplyComponentInstanceData(FPrecomputedLightInstanceData* LightMapData)
{
	check(LightMapData);

	if (!LightMapData->Transform.Equals(GetComponentTransform()))
	{
		return;
	}

	LightGuid = LightMapData->LightGuid;
	PreviewShadowMapChannel = LightMapData->PreviewShadowMapChannel;

	MarkRenderStateDirty();

#if WITH_EDITOR
	// Update the icon with the new state of PreviewShadowMapChannel
	UpdateLightSpriteTexture();
#endif
}

void ULightComponent::PropagateLightingScenarioChange()
{
	FComponentRecreateRenderStateContext Context(this);
	BeginReleaseResource(&StaticShadowDepthMap);
}

bool ULightComponent::IsPrecomputedLightingValid() const
{
	return GetLightComponentMapBuildData() != NULL && HasStaticShadowing();
}

int32 ULightComponent::GetNumMaterials() const
{
	return 1;
}

const FLightComponentMapBuildData* ULightComponent::GetLightComponentMapBuildData() const
{
	AActor* Owner = GetOwner();

	if (Owner)
	{
		ULevel* OwnerLevel = Owner->GetLevel();

		if (OwnerLevel && OwnerLevel->OwningWorld)
		{
			ULevel* ActiveLightingScenario = OwnerLevel->OwningWorld->GetActiveLightingScenario();
			UMapBuildDataRegistry* MapBuildData = NULL;

			if (ActiveLightingScenario && ActiveLightingScenario->MapBuildData)
			{
				MapBuildData = ActiveLightingScenario->MapBuildData;
			}
			else if (OwnerLevel->MapBuildData)
			{
				MapBuildData = OwnerLevel->MapBuildData;
			}

			if (MapBuildData)
			{
				return MapBuildData->GetLightBuildData(LightGuid);
			}
		}
	}

	return NULL;
}

void ULightComponent::InitializeStaticShadowDepthMap()
{
	if (HasStaticShadowing() && !HasStaticLighting())
	{
		const FStaticShadowDepthMapData* DepthMapData = NULL;
		const FLightComponentMapBuildData* MapBuildData = GetLightComponentMapBuildData();
	
		if (MapBuildData)
		{
			DepthMapData = &MapBuildData->DepthMap;
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			SetDepthMapData,
			FStaticShadowDepthMap*, DepthMap, &StaticShadowDepthMap,
			const FStaticShadowDepthMapData*, DepthMapData, DepthMapData,
			{
				DepthMap->Data = DepthMapData;
			}
		);

		BeginInitResource(&StaticShadowDepthMap);
	}
}

UMaterialInterface* ULightComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex == 0)
	{
		return LightFunctionMaterial;
	}
	else
	{
		return NULL;
	}
}

void ULightComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
	if (ElementIndex == 0)
	{
		LightFunctionMaterial = InMaterial;
		MarkRenderStateDirty();
	}
}

/** 
* This is called when property is modified by InterpPropertyTracks
*
* @param PropertyThatChanged	Property that changed
*/
void ULightComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	static FName LightColorName(TEXT("LightColor"));
	static FName IntensityName(TEXT("Intensity"));
	static FName BrightnessName(TEXT("Brightness"));
	static FName IndirectLightingIntensityName(TEXT("IndirectLightingIntensity"));
	static FName VolumetricScatteringIntensityName(TEXT("VolumetricScatteringIntensity"));
	static FName TemperatureName(TEXT("Temperature"));

	FName PropertyName = PropertyThatChanged->GetFName();
	if (PropertyName == LightColorName
		|| PropertyName == IntensityName
		|| PropertyName == BrightnessName
		|| PropertyName == IndirectLightingIntensityName
		|| PropertyName == TemperatureName
		|| PropertyName == VolumetricScatteringIntensityName)
	{
		// Old brightness tracks will animate the deprecated value
		if (PropertyName == BrightnessName)
		{
			Intensity = Brightness_DEPRECATED;
		}

		UpdateColorAndBrightness();
	}
	else
	{
		Super::PostInterpChange(PropertyThatChanged);
	}
}

/** Stores a light and a channel it has been assigned to. */
struct FLightAndChannel
{
	ULightComponent* Light;
	int32 Channel;

	FLightAndChannel(ULightComponent* InLight) :
		Light(InLight),
		Channel(INDEX_NONE)
	{}
};

struct FCompareLightsByArrayCount
{
	FORCEINLINE bool operator()( const TArray<FLightAndChannel*>& A, const TArray<FLightAndChannel*>& B ) const 
	{ 
		// Sort by descending array count
		return B.Num() < A.Num(); 
	}
};

void ULightComponent::ReassignStationaryLightChannels(UWorld* TargetWorld, bool bAssignForLightingBuild, ULevel* LightingScenario)
{
	TMap<FLightAndChannel*, TArray<FLightAndChannel*> > LightToOverlapMap;

	// Build an array of all static shadowing lights that need to be assigned
	for (TObjectIterator<ULightComponent> LightIt(RF_ClassDefaultObject, /** bIncludeDerivedClasses */ true, /** InternalExcludeFlags */ EInternalObjectFlags::PendingKill); LightIt; ++LightIt)
	{
		ULightComponent* const LightComponent = *LightIt;
		AActor* LightOwner = LightComponent->GetOwner();

		const bool bLightIsInWorld = LightOwner && TargetWorld->ContainsActor(LightOwner) && !LightOwner->IsPendingKill();

		if (bLightIsInWorld 
			// Only operate on stationary light components (static shadowing only)
			&& LightComponent->HasStaticShadowing()
			&& !LightComponent->HasStaticLighting())
		{
			ULevel* LightLevel = LightOwner->GetLevel();

			if (!LightingScenario || !LightLevel->bIsLightingScenario || LightLevel == LightingScenario)
			{				
				if (LightComponent->bAffectsWorld
					&& LightComponent->CastShadows 
					&& LightComponent->CastStaticShadows)
				{
					LightToOverlapMap.Add(new FLightAndChannel(LightComponent), TArray<FLightAndChannel*>());
				}
				else
				{
					// Reset the preview channel of stationary light components that shouldn't get a channel
					// This is necessary to handle a light being newly disabled
					LightComponent->PreviewShadowMapChannel = INDEX_NONE;

#if WITH_EDITOR
					LightComponent->UpdateLightSpriteTexture();
#endif
				}
			}
		}
	}

	// Build an array of overlapping lights
	for (TMap<FLightAndChannel*, TArray<FLightAndChannel*> >::TIterator It(LightToOverlapMap); It; ++It)
	{
		ULightComponent* CurrentLight = It.Key()->Light;
		TArray<FLightAndChannel*>& OverlappingLights = It.Value();

		if (bAssignForLightingBuild)
		{
			ULevel* StorageLevel = LightingScenario ? LightingScenario : CurrentLight->GetOwner()->GetLevel();
			UMapBuildDataRegistry* Registry = StorageLevel->GetOrCreateMapBuildData();
			FLightComponentMapBuildData& LightBuildData = Registry->FindOrAllocateLightBuildData(CurrentLight->LightGuid, true);
			LightBuildData.ShadowMapChannel = INDEX_NONE;
		}

		for (TMap<FLightAndChannel*, TArray<FLightAndChannel*> >::TIterator OtherIt(LightToOverlapMap); OtherIt; ++OtherIt)
		{
			ULightComponent* OtherLight = OtherIt.Key()->Light;

			if (CurrentLight != OtherLight 
				// Testing both directions because the spotlight <-> spotlight test is just cone vs bounding sphere
				//@todo - more accurate spotlight <-> spotlight intersection
				&& CurrentLight->AffectsBounds(FBoxSphereBounds(OtherLight->GetBoundingSphere()))
				&& OtherLight->AffectsBounds(FBoxSphereBounds(CurrentLight->GetBoundingSphere())))
			{
				OverlappingLights.Add(OtherIt.Key());
			}
		}
	}
		
	// Sort lights with the most overlapping lights first
	LightToOverlapMap.ValueSort(FCompareLightsByArrayCount());

	TMap<FLightAndChannel*, TArray<FLightAndChannel*> > SortedLightToOverlapMap;

	// Add directional lights to the beginning so they always get channels
	for (TMap<FLightAndChannel*, TArray<FLightAndChannel*> >::TIterator It(LightToOverlapMap); It; ++It)
	{
		FLightAndChannel* CurrentLight = It.Key();

		if (CurrentLight->Light->GetLightType() == LightType_Directional)
		{
			SortedLightToOverlapMap.Add(It.Key(), It.Value());
		}
	}

	// Add everything else, which has been sorted by descending overlaps
	for (TMap<FLightAndChannel*, TArray<FLightAndChannel*> >::TIterator It(LightToOverlapMap); It; ++It)
	{
		FLightAndChannel* CurrentLight = It.Key();

		if (CurrentLight->Light->GetLightType() != LightType_Directional)
		{
			SortedLightToOverlapMap.Add(It.Key(), It.Value());
		}
	}

	// Go through lights and assign shadowmap channels
	//@todo - retry with different ordering heuristics when it fails
	for (TMap<FLightAndChannel*, TArray<FLightAndChannel*> >::TIterator It(SortedLightToOverlapMap); It; ++It)
	{
		bool bChannelUsed[4] = {0};
		FLightAndChannel* CurrentLight = It.Key();
		const TArray<FLightAndChannel*>& OverlappingLights = It.Value();

		// Mark which channels have already been assigned to overlapping lights
		for (int32 OverlappingIndex = 0; OverlappingIndex < OverlappingLights.Num(); OverlappingIndex++)
		{
			FLightAndChannel* OverlappingLight = OverlappingLights[OverlappingIndex];

			if (OverlappingLight->Channel != INDEX_NONE)
			{
				bChannelUsed[OverlappingLight->Channel] = true;
			}
		}

		// Use the lowest free channel
		for (int32 ChannelIndex = 0; ChannelIndex < ARRAY_COUNT(bChannelUsed); ChannelIndex++)
		{
			if (!bChannelUsed[ChannelIndex])
			{
				CurrentLight->Channel = ChannelIndex;
				break;
			}
		}
	}

	// Go through the assigned lights and update their render state and icon
	for (TMap<FLightAndChannel*, TArray<FLightAndChannel*> >::TIterator It(SortedLightToOverlapMap); It; ++It)
	{
		FLightAndChannel* CurrentLight = It.Key();

		if (CurrentLight->Light->PreviewShadowMapChannel != CurrentLight->Channel)
		{
			CurrentLight->Light->PreviewShadowMapChannel = CurrentLight->Channel;
			CurrentLight->Light->MarkRenderStateDirty();
		}

#if WITH_EDITOR
		CurrentLight->Light->UpdateLightSpriteTexture();
#endif

		if (bAssignForLightingBuild)
		{
			ULevel* StorageLevel = LightingScenario ? LightingScenario : CurrentLight->Light->GetOwner()->GetLevel();
			UMapBuildDataRegistry* Registry = StorageLevel->GetOrCreateMapBuildData();
			FLightComponentMapBuildData& LightBuildData = Registry->FindOrAllocateLightBuildData(CurrentLight->Light->LightGuid, true);
			LightBuildData.ShadowMapChannel = CurrentLight->Channel;

			if (CurrentLight->Channel == INDEX_NONE)
			{
				FMessageLog("LightingResults").Error()
					->AddToken(FUObjectToken::Create(CurrentLight->Light->GetOwner()))
					->AddToken(FTextToken::Create( NSLOCTEXT("Lightmass", "LightmassError_FailedToAllocateShadowmapChannel", "Severe performance loss: Failed to allocate shadowmap channel for stationary light due to overlap - light will fall back to dynamic shadows!") ) );
			}
		}

		delete CurrentLight;
	}
}

static void ToggleLight(const TArray<FString>& Args)
{
	for (TObjectIterator<ULightComponent> It; It; ++It)
	{
		ULightComponent* Light = *It;
		if (Light->Mobility != EComponentMobility::Static)
		{
			FString LightName = (Light->GetOwner() ? Light->GetOwner()->GetFName() : Light->GetFName()).ToString();
			for (int32 ArgIndex = 0; ArgIndex < Args.Num(); ++ArgIndex)
			{
				const FString& ToggleName = Args[ArgIndex];
				if (LightName.Contains(ToggleName) )
				{
					Light->ToggleVisibility(/*bPropagateToChildren=*/ false);
					UE_LOG(LogConsoleResponse,Display,TEXT("Now%svisible: %s"),
						Light->IsVisible() ? TEXT("") : TEXT(" not "),
						*Light->GetFullName()
						);
					break;
				}
			}
		}
	}
}

static FAutoConsoleCommand ToggleLightCmd(
	TEXT("ToggleLight"),
	TEXT("Toggles all lights whose name contains the specified string"),
	FConsoleCommandWithArgsDelegate::CreateStatic(ToggleLight),
	ECVF_Cheat
	);
