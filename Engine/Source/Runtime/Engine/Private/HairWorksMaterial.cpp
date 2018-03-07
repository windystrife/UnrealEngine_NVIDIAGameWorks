// @third party code - BEGIN HairWorks
#include "Engine/HairWorksMaterial.h"
#include "RenderingThread.h"
#include "HairWorksSDK.h"

UHairWorksMaterial::UHairWorksMaterial(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHairWorksMaterial::PostLoad()
{
	Super::PostLoad();

	// Compile shader
	if(::HairWorks::GetSDK() == nullptr)
		return;

	NvHair::InstanceDescriptor HairDesc;
	TArray<UTexture2D*> HairTextures;
	SyncHairDescriptor(HairDesc, HairTextures, false);

	NvHair::ShaderCacheSettings ShaderCacheSettings;
	ShaderCacheSettings.setFromInstanceDescriptor(HairDesc);

	for(int Index = 0; Index < HairTextures.Num(); ++Index)
	{
		const auto* Texture = HairTextures[Index];
		ShaderCacheSettings.setTextureUsed(Index, Texture != nullptr);
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		HairUpdateDynamicData,
		const NvHair::ShaderCacheSettings, ShaderCacheSettings, ShaderCacheSettings,
		{
			::HairWorks::GetSDK()->addToShaderCache(ShaderCacheSettings);
		}
	);
}

void UHairWorksMaterial::GetHairInstanceParameters(nvidia::HairWorks::InstanceDescriptor & HairDescriptor, TArray<UTexture2D*>& HairTexture) const
{
	const_cast<UHairWorksMaterial*>(this)->SyncHairDescriptor(HairDescriptor, HairTexture, false);
}

void UHairWorksMaterial::SetHairInstanceParameters(const nvidia::HairWorks::InstanceDescriptor & HairDescriptor, const TArray<UTexture2D*>& HairTexture)
{
	SyncHairDescriptor(const_cast<nvidia::HairWorks::InstanceDescriptor&>(HairDescriptor), const_cast<TArray<UTexture2D*>&>(HairTexture), true);
}

void UHairWorksMaterial::SyncHairDescriptor(NvHair::InstanceDescriptor& HairDescriptor, TArray<UTexture2D*>& HairTextures, bool bFromDescriptor)
{
	HairTextures.SetNum(NvHair::ETextureType::COUNT_OF, false);

#pragma region Visualization
	SyncHairParameter(HairDescriptor.m_visualizeBones, bBones, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeBoundingBox, bBoundingBox, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeCapsules, bCollisionCapsules, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeControlVertices, bControlPoints, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeGrowthMesh, bGrowthMesh, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeGuideHairs, bGuideCurves, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeHairInteractions, bHairInteraction, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizePinConstraints, bPinConstraints, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeShadingNormals, bShadingNormal, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeShadingNormalBone, bShadingNormalCenter, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_visualizeSkinnedGuideHairs, bSkinnedGuideCurves, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_drawRenderHairs, bHair, bFromDescriptor);

	if(bFromDescriptor)
		ColorizeOptions = static_cast<EHairWorksColorizeMode>(HairDescriptor.m_colorizeMode);
	else
		HairDescriptor.m_colorizeMode = static_cast<unsigned>(ColorizeOptions);
#pragma endregion

#pragma region General
	SyncHairParameter(HairDescriptor.m_enable, bEnable, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_splineMultiplier, SplineMultiplier, bFromDescriptor);
#pragma endregion

#pragma region Physical
	SyncHairParameter(HairDescriptor.m_simulate, bSimulate, bFromDescriptor);
	FVector GravityDir = FVector(0, 0, -1);
	SyncHairParameter(HairDescriptor.m_gravityDir, (NvHair::Vec3&)GravityDir, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_massScale, MassScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_damping, Damping, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_inertiaScale, InertiaScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_inertiaLimit, InertiaLimit, bFromDescriptor);
#pragma endregion

#pragma region Wind
	FVector WindVector = WindDirection.Vector() * Wind;
	SyncHairParameter(HairDescriptor.m_wind, (NvHair::Vec3&)WindVector, bFromDescriptor);
	if(bFromDescriptor)
	{
		Wind = WindVector.Size();
		WindDirection = FRotator(FQuat(FRotationMatrix::MakeFromX(WindVector)));
	}
	SyncHairParameter(HairDescriptor.m_windNoise, WindNoise, bFromDescriptor);
#pragma endregion

#pragma region Stiffness
	SyncHairParameter(HairDescriptor.m_stiffness, StiffnessGlobal, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::STIFFNESS], StiffnessGlobalMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_stiffnessCurve, (NvHair::Vec4&)StiffnessGlobalCurve, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_stiffnessStrength, StiffnessStrength, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_stiffnessStrengthCurve, (NvHair::Vec4&)StiffnessStrengthCurve, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_stiffnessDamping, StiffnessDamping, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_stiffnessDampingCurve, (NvHair::Vec4&)StiffnessDampingCurve, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_rootStiffness, StiffnessRoot, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::ROOT_STIFFNESS], StiffnessRootMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_tipStiffness, StiffnessTip, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_bendStiffness, StiffnessBend, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_bendStiffnessCurve, (NvHair::Vec4&)StiffnessBendCurve, bFromDescriptor);
#pragma endregion

#pragma region Collision
	SyncHairParameter(HairDescriptor.m_backStopRadius, Backstop, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_friction, Friction, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_useCollision, bCapsuleCollision, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_interactionStiffness, StiffnessInteraction, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_interactionStiffnessCurve, (NvHair::Vec4&)StiffnessInteractionCurve, bFromDescriptor);
#pragma endregion

//#pragma region Pin
//	SyncHairParameter(HairDescriptor.m_pinStiffness, PinStiffness, bFromDescriptor);
//#pragma endregion

#pragma region Volume
	SyncHairParameter(HairDescriptor.m_density, Density, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::DENSITY], DensityMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_usePixelDensity, bUsePixelDensity, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_lengthScale, LengthScale, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::LENGTH], LengthScaleMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_lengthNoise, LengthNoise, bFromDescriptor);
#pragma endregion

#pragma region Strand Width
	SyncHairParameter(HairDescriptor.m_width, WidthScale, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::WIDTH], WidthScaleMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_widthRootScale, WidthRootScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_widthTipScale, WidthTipScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_widthNoise, WidthNoise, bFromDescriptor);
#pragma endregion

#pragma region Clumping
	SyncHairParameter(HairDescriptor.m_clumpScale, ClumpingScale, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::CLUMP_SCALE], ClumpingScaleMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_clumpRoundness, ClumpingRoundness, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::CLUMP_ROUNDNESS], ClumpingRoundnessMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_clumpNoise, ClumpingNoise, bFromDescriptor);
#pragma endregion

#pragma region Waveness
	SyncHairParameter(HairDescriptor.m_waveScale, WavinessScale, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::WAVE_SCALE], WavinessScaleMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_waveScaleNoise, WavinessScaleNoise, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_waveScaleStrand, WavinessScaleStrand, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_waveScaleClump, WavinessScaleClump, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_waveFreq, WavinessFreq, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::WAVE_FREQ], WavinessFreqMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_waveFreqNoise, WavinessFreqNoise, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_waveRootStraighten, WavinessRootStraigthen, bFromDescriptor);
#pragma endregion

#pragma region Color
	SyncHairParameter(HairDescriptor.m_rootColor, (NvHair::Vec4&)RootColor, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::ROOT_COLOR], RootColorMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_tipColor, (NvHair::Vec4&)TipColor, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::TIP_COLOR], TipColorMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_rootTipColorWeight, RootTipColorWeight, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_rootTipColorFalloff, RootTipColorFalloff, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_rootAlphaFalloff, RootAlphaFalloff, bFromDescriptor);
#pragma endregion

#pragma region Strand
	SyncHairParameter(HairTextures[NvHair::ETextureType::STRAND], PerStrandTexture, bFromDescriptor);

	switch(StrandBlendMode)
	{
	case EHairWorksStrandBlendMode::Overwrite:
		HairDescriptor.m_strandBlendMode = NvHair::EStrandBlendMode::OVERWRITE;
		break;
	case EHairWorksStrandBlendMode::Multiply:
		HairDescriptor.m_strandBlendMode = NvHair::EStrandBlendMode::MULTIPLY;
		break;
	case EHairWorksStrandBlendMode::Add:
		HairDescriptor.m_strandBlendMode = NvHair::EStrandBlendMode::ADD;
		break;
	case EHairWorksStrandBlendMode::Modulate:
		HairDescriptor.m_strandBlendMode = NvHair::EStrandBlendMode::MODULATE;
		break;
	}

	SyncHairParameter(HairDescriptor.m_strandBlendScale, StrandBlendScale, bFromDescriptor);
#pragma endregion

#pragma region Diffuse
	SyncHairParameter(HairDescriptor.m_diffuseBlend, DiffuseBlend, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_hairNormalWeight, HairNormalWeight, bFromDescriptor);
#pragma endregion

#pragma region Specular
	SyncHairParameter(HairDescriptor.m_specularColor, (NvHair::Vec4&)SpecularColor, bFromDescriptor);
	SyncHairParameter(HairTextures[NvHair::ETextureType::SPECULAR], SpecularColorMap, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_specularPrimary, PrimaryScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_specularPowerPrimary, PrimaryShininess, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_specularPrimaryBreakup, PrimaryBreakup, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_specularSecondary, SecondaryScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_specularPowerSecondary, SecondaryShininess, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_specularSecondaryOffset, SecondaryOffset, bFromDescriptor);
#pragma endregion

#pragma region Glint
	SyncHairParameter(HairDescriptor.m_glintStrength, GlintStrength, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_glintCount, GlintSize, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_glintExponent, GlintPowerExponent, bFromDescriptor);
#pragma endregion

#pragma region Shadow
	SyncHairParameter(HairDescriptor.m_shadowSigma, ShadowAttenuation, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_shadowDensityScale, ShadowDensityScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_castShadows, bCastShadows, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_receiveShadows, bReceiveShadows, bFromDescriptor);
#pragma endregion

#pragma region Culling
	SyncHairParameter(HairDescriptor.m_useViewfrustrumCulling, bViewFrustumCulling, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_useBackfaceCulling, bBackfaceCulling, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_backfaceCullingThreshold, BackfaceCullingThreshold, bFromDescriptor);
#pragma endregion

#pragma region LOD
	if(!bFromDescriptor)
		HairDescriptor.m_enableLod = true;
#pragma endregion

#pragma region Distance LOD
	SyncHairParameter(HairDescriptor.m_enableDistanceLod, bDistanceLodEnable, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_distanceLodStart, DistanceLodStart, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_distanceLodEnd, DistanceLodEnd, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_distanceLodFadeStart, FadeStartDistance, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_distanceLodWidth, DistanceLodBaseWidthScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_distanceLodDensity, DistanceLodBaseDensityScale, bFromDescriptor);
#pragma endregion

#pragma region Detail LOD
	SyncHairParameter(HairDescriptor.m_enableDetailLod, bDetailLodEnable, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_detailLodStart, DetailLodStart, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_detailLodEnd, DetailLodEnd, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_detailLodWidth, DetailLodBaseWidthScale, bFromDescriptor);
	SyncHairParameter(HairDescriptor.m_detailLodDensity, DetailLodBaseDensityScale, bFromDescriptor);
#pragma endregion
}
// @third party code - END HairWorks
