// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizations.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "StaticMeshComponentDetails.h"
#include "LightComponentDetails.h"
#include "PointLightComponentDetails.h"
#include "DirectionalLightComponentDetails.h"
#include "SceneComponentDetails.h"
#include "BodyInstanceCustomization.h"
#include "PrimitiveComponentDetails.h"
#include "StaticMeshActorDetails.h"
#include "SkinnedMeshComponentDetails.h"
#include "SkeletalMeshComponentDetails.h"
#include "SplineComponentDetails.h"
#include "MeshComponentDetails.h"
#include "MatineeActorDetails.h"
#include "LevelSequenceActorDetails.h"
#include "ReflectionCaptureDetails.h"
#include "SkyLightComponentDetails.h"
#include "BrushDetails.h"
#include "ObjectDetails.h"
#include "ActorDetails.h"
#include "SkeletalControlNodeDetails.h"
#include "AnimMontageSegmentDetails.h"
#include "AnimSequenceDetails.h"
#include "AnimTransitionNodeDetails.h"
#include "AnimStateNodeDetails.h"
#include "PoseAssetDetails.h"
#include "AnimationAssetDetails.h"
#include "AmbientSoundDetails.h"
#include "MathStructCustomizations.h"
#include "MathStructProxyCustomizations.h"
#include "RangeStructCustomization.h"
#include "IntervalStructCustomization.h"
#include "SoftObjectPathCustomization.h"
#include "SoftClassPathCustomization.h"
#include "AttenuationSettingsCustomizations.h"
#include "WorldSettingsDetails.h"
#include "DialogueStructsCustomizations.h"
#include "DataTableCustomization.h"
#include "DataTableCategoryCustomization.h"
#include "CurveTableCustomization.h"
#include "DialogueWaveDetails.h"
#include "BodySetupDetails.h"
#include "Customizations/SlateBrushCustomization.h"
#include "SlateSoundCustomization.h"
#include "Customizations/SlateFontInfoCustomization.h"
#include "MarginCustomization.h"
#include "PhysicsConstraintComponentDetails.h"
#include "GuidStructCustomization.h"
#include "ParticleModuleDetails.h"
#include "CameraDetails.h"
#include "BlackboardEntryDetails.h"
#include "AIDataProviderValueDetails.h"
#include "EnvQueryParamInstanceCustomization.h"
#include "SkeletonNotifyDetails.h"
#include "ColorStructCustomization.h"
#include "SlateColorCustomization.h"
#include "CurveStructCustomization.h"
#include "NavLinkStructCustomization.h"
#include "NavAgentSelectorCustomization.h"
#include "DirectoryPathStructCustomization.h"
#include "FilePathStructCustomization.h"
#include "DeviceProfileDetails.h"
#include "KeyStructCustomization.h"
#include "InternationalizationSettingsModelDetails.h"
#include "InputSettingsDetails.h"
#include "InputStructCustomization.h"
#include "CollisionProfileDetails.h"
#include "PhysicsSettingsDetails.h"
#include "GeneralProjectSettingsDetails.h"
#include "HardwareTargetingSettingsDetails.h"
#include "LinuxTargetSettingsDetails.h"
#include "WindowsTargetSettingsDetails.h"
#include "MacTargetSettingsDetails.h"
#include "MoviePlayerSettingsDetails.h"
#include "SourceCodeAccessSettingsDetails.h"
#include "ParticleSystemComponentDetails.h"
#include "ParticleSysParamStructCustomization.h"
#include "RawDistributionVectorStructCustomization.h"
#include "CollisionProfileNameCustomization.h"
#include "DocumentationActorDetails.h"
#include "SoundBaseDetails.h"
#include "SoundSourceBusDetails.h"
#include "SoundWaveDetails.h"
#include "AudioSettingsDetails.h"
#include "DateTimeStructCustomization.h"
#include "TimespanStructCustomization.h"
#include "FbxImportUIDetails.h"
#include "FbxSceneImportDataDetails.h"
#include "RigDetails.h"
#include "SceneCaptureDetails.h"
// WaveWorks Start
#include "WaveWorksShorelineCaptureDetails.h"
// WaveWorks End
#include "CurveColorCustomization.h"
#include "ActorComponentDetails.h"
#include "AutoReimportDirectoryCustomization.h"
#include "DistanceDatumStructCustomization.h"
#include "HierarchicalSimplificationCustomizations.h"
#include "PostProcessSettingsCustomization.h"
#include "ConfigEditorPropertyDetails.h"
#include "AssetImportDataCustomization.h"
#include "CaptureResolutionCustomization.h"
#include "CaptureTypeCustomization.h"
#include "RenderPassesCustomization.h"
#include "MovieSceneCaptureCustomization.h"
#include "MovieSceneEvalOptionsCustomization.h"
#include "MovieSceneEventParametersCustomization.h"
#include "MovieSceneSequencePlaybackSettingsCustomization.h"
#include "MovieSceneCurveInterfaceKeyEditStructCustomization.h"
#include "LevelSequenceBurnInOptionsCustomization.h"
#include "MovieSceneBindingOverrideDataCustomization.h"
#include "TextCustomization.h"
#include "AnimTrailNodeDetails.h"
#include "MaterialProxySettingsCustomizations.h"
#include "ImportantToggleSettingCustomization.h"
#include "CameraFilmbackSettingsCustomization.h"
#include "CameraLensSettingsCustomization.h"
#include "CameraFocusSettingsCustomization.h"
#include "RotatorStructCustomization.h"
#include "VectorStructCustomization.h"
#include "Vector4StructCustomization.h"
#include "AssetViewerSettingsCustomization.h"
#include "MeshMergingSettingsCustomization.h"
#include "MaterialAttributePropertyDetails.h"
#include "CollectionReferenceStructCustomization.h"
// @third party code - BEGIN HairWorks
#include "HairWorksDetails.h"
// @third party code - END HairWorks

IMPLEMENT_MODULE( FDetailCustomizationsModule, DetailCustomizations );


void FDetailCustomizationsModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	RegisterPropertyTypeCustomizations();
	RegisterObjectCustomizations();

	PropertyModule.NotifyCustomizationModuleChanged();
}


void FDetailCustomizationsModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		// Unregister all classes customized by name
		for (auto It = RegisteredClassNames.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomClassLayout(*It);
			}
		}

		// Unregister all structures
		for (auto It = RegisteredPropertyTypes.CreateConstIterator(); It; ++It)
		{
			if(It->IsValid())
			{
				PropertyModule.UnregisterCustomPropertyTypeLayout(*It);
			}
		}
	
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}


void FDetailCustomizationsModule::RegisterPropertyTypeCustomizations()
{
	RegisterCustomPropertyTypeLayout("SoftObjectPath", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSoftObjectPathCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("SoftClassPath", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSoftClassPathCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("DataTableRowHandle", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTableCustomizationLayout::MakeInstance));
	RegisterCustomPropertyTypeLayout("DataTableCategoryHandle", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTableCategoryCustomizationLayout::MakeInstance));
	RegisterCustomPropertyTypeLayout("CurveTableRowHandle", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCurveTableCustomizationLayout::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_Vector, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVectorStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_Vector4, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVector4StructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_Vector2D, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMathStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_IntPoint, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMathStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_Rotator, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRotatorStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_LinearColor, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FColorStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_Color, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FColorStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_Matrix, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMatrixStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_Transform, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTransformStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout(NAME_Quat, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FQuatStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("SlateColor", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSlateColorCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("ForceFeedbackAttenuationSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FForceFeedbackAttenuationSettingsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("SoundAttenuationSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSoundAttenuationSettingsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("DialogueContext", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDialogueContextStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("DialogueWaveParameter", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDialogueWaveParameterStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("BodyInstance", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBodyInstanceCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("SlateBrush", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSlateBrushStructCustomization::MakeInstance, true));
	RegisterCustomPropertyTypeLayout("SlateSound", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSlateSoundStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("SlateFontInfo", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSlateFontInfoStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("Guid", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGuidStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("Key", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FKeyStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("FloatRange", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRangeStructCustomization<float>::MakeInstance));
	RegisterCustomPropertyTypeLayout("Int32Range", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRangeStructCustomization<int32>::MakeInstance));
	RegisterCustomPropertyTypeLayout("FloatInterval", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FIntervalStructCustomization<float>::MakeInstance));
	RegisterCustomPropertyTypeLayout("Int32Interval", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FIntervalStructCustomization<int32>::MakeInstance));
	RegisterCustomPropertyTypeLayout("DateTime", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDateTimeStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("Timespan", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTimespanStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("BlackboardEntry", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlackboardEntryDetails::MakeInstance));
	RegisterCustomPropertyTypeLayout("AIDataProviderIntValue", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAIDataProviderValueDetails::MakeInstance));
	RegisterCustomPropertyTypeLayout("AIDataProviderFloatValue", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAIDataProviderValueDetails::MakeInstance));
	RegisterCustomPropertyTypeLayout("AIDataProviderBoolValue", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAIDataProviderValueDetails::MakeInstance));
	RegisterCustomPropertyTypeLayout("RuntimeFloatCurve", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCurveStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("EnvNamedValue", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEnvQueryParamInstanceCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("NavigationLink", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNavLinkStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("NavigationSegmentLink", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNavLinkStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("NavAgentSelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNavAgentSelectorCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("Margin", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMarginStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("TextProperty", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTextCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("DirectoryPath", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDirectoryPathStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("FilePath", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FFilePathStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("IOSBuildResourceDirectory", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDirectoryPathStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("IOSBuildResourceFilePath", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FFilePathStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("InputAxisConfigEntry", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInputAxisConfigCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("InputActionKeyMapping", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInputActionMappingCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("InputAxisKeyMapping", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInputAxisMappingCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("RuntimeCurveLinearColor", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCurveColorCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("ParticleSysParam", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FParticleSysParamStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("RawDistributionVector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRawDistributionVectorStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("CollisionProfileName", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCollisionProfileNameCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("AutoReimportDirectoryConfig", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAutoReimportDirectoryCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("AutoReimportWildcard", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAutoReimportWildcardCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("DistanceDatum", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDistanceDatumStructCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("HierarchicalSimplification", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FHierarchicalSimplificationCustomizations::MakeInstance));
	RegisterCustomPropertyTypeLayout("PostProcessSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPostProcessSettingsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("AssetImportInfo", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAssetImportDataCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("CaptureResolution", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCaptureResolutionCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("CaptureProtocolID", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCaptureTypeCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("CompositionGraphCapturePasses", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRenderPassesCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("WeightedBlendable", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FWeightedBlendableCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("MaterialProxySettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMaterialProxySettingsCustomizations::MakeInstance));
	RegisterCustomPropertyTypeLayout("CameraFilmbackSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCameraFilmbackSettingsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("CameraLensSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCameraLensSettingsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("CameraFocusSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCameraFocusSettingsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("MovieSceneSequencePlaybackSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMovieSceneSequencePlaybackSettingsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("MovieSceneBindingOverrideData", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMovieSceneBindingOverrideDataCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("MovieSceneTrackEvalOptions", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMovieSceneTrackEvalOptionsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("MovieSceneSectionEvalOptions", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMovieSceneSectionEvalOptionsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("MovieSceneEventParameters", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMovieSceneEventParametersCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("LevelSequenceBurnInOptions", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLevelSequenceBurnInOptionsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("LevelSequenceBurnInInitSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLevelSequenceBurnInInitSettingsCustomization::MakeInstance));
	RegisterCustomPropertyTypeLayout("CollectionReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCollectionReferenceStructCustomization::MakeInstance));
}

void FDetailCustomizationsModule::RegisterObjectCustomizations()
{
	// Note: By default properties are displayed in script defined order (i.e the order in the header).  These layout detail classes are called in the order seen here which will display properties
	// in the order they are customized.  This is only relevant for inheritance where both a child and a parent have properties that are customized.
	// In the order below, Actor will get a chance to display details first, followed by USceneComponent.

	RegisterCustomClassLayout("Object", FOnGetDetailCustomizationInstance::CreateStatic(&FObjectDetails::MakeInstance));
	RegisterCustomClassLayout("Actor", FOnGetDetailCustomizationInstance::CreateStatic(&FActorDetails::MakeInstance));
	RegisterCustomClassLayout("ActorComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FActorComponentDetails::MakeInstance));
	RegisterCustomClassLayout("SceneComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSceneComponentDetails::MakeInstance));
	RegisterCustomClassLayout("PrimitiveComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FPrimitiveComponentDetails::MakeInstance));
	RegisterCustomClassLayout("StaticMeshComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FStaticMeshComponentDetails::MakeInstance));
	RegisterCustomClassLayout("SkeletalMeshComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSkeletalMeshComponentDetails::MakeInstance));
	RegisterCustomClassLayout("SkinnedMeshComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSkinnedMeshComponentDetails::MakeInstance));
	RegisterCustomClassLayout("SplineComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSplineComponentDetails::MakeInstance));
	RegisterCustomClassLayout("LightComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FLightComponentDetails::MakeInstance));
	RegisterCustomClassLayout("PointLightComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FPointLightComponentDetails::MakeInstance));
	RegisterCustomClassLayout("DirectionalLightComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FDirectionalLightComponentDetails::MakeInstance));
	RegisterCustomClassLayout("StaticMeshActor", FOnGetDetailCustomizationInstance::CreateStatic(&FStaticMeshActorDetails::MakeInstance));
	RegisterCustomClassLayout("MeshComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FMeshComponentDetails::MakeInstance));
	RegisterCustomClassLayout("MatineeActor", FOnGetDetailCustomizationInstance::CreateStatic(&FMatineeActorDetails::MakeInstance));
	RegisterCustomClassLayout("LevelSequenceActor", FOnGetDetailCustomizationInstance::CreateStatic(&FLevelSequenceActorDetails::MakeInstance));
	RegisterCustomClassLayout("ReflectionCapture", FOnGetDetailCustomizationInstance::CreateStatic(&FReflectionCaptureDetails::MakeInstance));
	RegisterCustomClassLayout("SceneCaptureComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSceneCaptureDetails::MakeInstance));
	// WaveWorks Start
	RegisterCustomClassLayout("WaveWorksShorelineCapture", FOnGetDetailCustomizationInstance::CreateStatic(&FWaveWorksShorelineCaptureDetails::MakeInstance));
	// WaveWorks End
	RegisterCustomClassLayout("SkyLight", FOnGetDetailCustomizationInstance::CreateStatic(&FSkyLightComponentDetails::MakeInstance));
	RegisterCustomClassLayout("Brush", FOnGetDetailCustomizationInstance::CreateStatic(&FBrushDetails::MakeInstance));
	RegisterCustomClassLayout("AmbientSound", FOnGetDetailCustomizationInstance::CreateStatic(&FAmbientSoundDetails::MakeInstance));
	RegisterCustomClassLayout("WorldSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FWorldSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("GeneralProjectSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FGeneralProjectSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("HardwareTargetingSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FHardwareTargetingSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("DocumentationActor", FOnGetDetailCustomizationInstance::CreateStatic(&FDocumentationActorDetails::MakeInstance));

	//@TODO: A2REMOVAL: Rename FSkeletalControlNodeDetails to something more generic
	RegisterCustomClassLayout("K2Node_StructMemberGet", FOnGetDetailCustomizationInstance::CreateStatic(&FSkeletalControlNodeDetails::MakeInstance));
	RegisterCustomClassLayout("K2Node_StructMemberSet", FOnGetDetailCustomizationInstance::CreateStatic(&FSkeletalControlNodeDetails::MakeInstance));
	RegisterCustomClassLayout("K2Node_GetClassDefaults", FOnGetDetailCustomizationInstance::CreateStatic(&FSkeletalControlNodeDetails::MakeInstance));

	RegisterCustomClassLayout("AnimSequence", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimSequenceDetails::MakeInstance));
	RegisterCustomClassLayout("Rig", FOnGetDetailCustomizationInstance::CreateStatic(&FRigDetails::MakeInstance));

	RegisterCustomClassLayout("EditorAnimSegment", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimMontageSegmentDetails::MakeInstance));
	RegisterCustomClassLayout("EditorAnimCompositeSegment", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimMontageSegmentDetails::MakeInstance));
	RegisterCustomClassLayout("EditorSkeletonNotifyObj", FOnGetDetailCustomizationInstance::CreateStatic(&FSkeletonNotifyDetails::MakeInstance));
	RegisterCustomClassLayout("AnimStateNode", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimStateNodeDetails::MakeInstance));
	RegisterCustomClassLayout("AnimStateTransitionNode", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimTransitionNodeDetails::MakeInstance));
	RegisterCustomClassLayout("AnimGraphNode_Trail", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimTrailNodeDetails::MakeInstance));
	RegisterCustomClassLayout("PoseAsset", FOnGetDetailCustomizationInstance::CreateStatic(&FPoseAssetDetails::MakeInstance));
	RegisterCustomClassLayout("AnimationAsset", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimationAssetDetails::MakeInstance));

	RegisterCustomClassLayout("SoundBase", FOnGetDetailCustomizationInstance::CreateStatic(&FSoundBaseDetails::MakeInstance));
	RegisterCustomClassLayout("SoundSourceBus", FOnGetDetailCustomizationInstance::CreateStatic(&FSoundSourceBusDetails::MakeInstance));
	RegisterCustomClassLayout("SoundWave", FOnGetDetailCustomizationInstance::CreateStatic(&FSoundWaveDetails::MakeInstance));
	RegisterCustomClassLayout("DialogueWave", FOnGetDetailCustomizationInstance::CreateStatic(&FDialogueWaveDetails::MakeInstance));
	RegisterCustomClassLayout("BodySetup", FOnGetDetailCustomizationInstance::CreateStatic(&FBodySetupDetails::MakeInstance));
	RegisterCustomClassLayout("SkeletalBodySetup", FOnGetDetailCustomizationInstance::CreateStatic(&FSkeletalBodySetupDetails::MakeInstance));
	RegisterCustomClassLayout("PhysicsConstraintTemplate", FOnGetDetailCustomizationInstance::CreateStatic(&FPhysicsConstraintComponentDetails::MakeInstance));
	RegisterCustomClassLayout("PhysicsConstraintComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FPhysicsConstraintComponentDetails::MakeInstance));
	RegisterCustomClassLayout("CollisionProfile", FOnGetDetailCustomizationInstance::CreateStatic(&FCollisionProfileDetails::MakeInstance));
	RegisterCustomClassLayout("PhysicsSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FPhysicsSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("AudioSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FAudioSettingsDetails::MakeInstance));

	RegisterCustomClassLayout("ParticleModuleRequired", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleRequiredDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleSubUV", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleSubUVDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleAccelerationDrag", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleAccelerationDragDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleAcceleration", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleAccelerationDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleAccelerationDragScaleOverLife", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleAccelerationDragScaleOverLifeDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleCollisionGPU", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleCollisionGPUDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleOrbit", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleOrbitDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleSizeMultiplyLife", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleSizeMultiplyLifeDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleSizeScale", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleSizeScaleDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleVectorFieldScale", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleVectorFieldScaleDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleModuleVectorFieldScaleOverLife", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleModuleVectorFieldScaleOverLifeDetails::MakeInstance));

	RegisterCustomClassLayout("CameraComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FCameraDetails::MakeInstance));
	RegisterCustomClassLayout("DeviceProfile", FOnGetDetailCustomizationInstance::CreateStatic(&FDeviceProfileDetails::MakeInstance));
	RegisterCustomClassLayout("InternationalizationSettingsModel", FOnGetDetailCustomizationInstance::CreateStatic(&FInternationalizationSettingsModelDetails::MakeInstance));
	RegisterCustomClassLayout("InputSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FInputSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("WindowsTargetSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FWindowsTargetSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("MacTargetSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FMacTargetSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("LinuxTargetSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FLinuxTargetSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("MoviePlayerSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FMoviePlayerSettingsDetails::MakeInstance));

	RegisterCustomClassLayout("SourceCodeAccessSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FSourceCodeAccessSettingsDetails::MakeInstance));
	RegisterCustomClassLayout("ParticleSystemComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FParticleSystemComponentDetails::MakeInstance));

	RegisterCustomClassLayout("FbxImportUI", FOnGetDetailCustomizationInstance::CreateStatic(&FFbxImportUIDetails::MakeInstance));
	RegisterCustomClassLayout("FbxSceneImportData", FOnGetDetailCustomizationInstance::CreateStatic(&FFbxSceneImportDataDetails::MakeInstance));

	RegisterCustomClassLayout("ConfigHierarchyPropertyView", FOnGetDetailCustomizationInstance::CreateStatic(&FConfigPropertyHelperDetails::MakeInstance));

	RegisterCustomClassLayout("MovieSceneCapture", FOnGetDetailCustomizationInstance::CreateStatic(&FMovieSceneCaptureCustomization::MakeInstance));
	RegisterCustomClassLayout("MovieSceneCurveInterfaceKeyEditStruct", FOnGetDetailCustomizationInstance::CreateStatic(&FMovieSceneCurveInterfaceKeyEditStructCustomization::MakeInstance));

	RegisterCustomClassLayout("AnalyticsPrivacySettings", FOnGetDetailCustomizationInstance::CreateStatic(&FImportantToggleSettingCustomization::MakeInstance));
	RegisterCustomClassLayout("EndUserSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FImportantToggleSettingCustomization::MakeInstance));

	RegisterCustomClassLayout("AssetViewerSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FAssetViewerSettingsCustomization::MakeInstance));

	RegisterCustomClassLayout("MeshMergingSettingsObject", FOnGetDetailCustomizationInstance::CreateStatic(&FMeshMergingSettingsObjectCustomization::MakeInstance));

	RegisterCustomClassLayout("MaterialExpressionGetMaterialAttributes", FOnGetDetailCustomizationInstance::CreateStatic(&FMaterialAttributePropertyDetails::MakeInstance));
	RegisterCustomClassLayout("MaterialExpressionSetMaterialAttributes", FOnGetDetailCustomizationInstance::CreateStatic(&FMaterialAttributePropertyDetails::MakeInstance));

	// @third party code - BEGIN HairWorks
	RegisterCustomClassLayout("HairWorksMaterial", FOnGetDetailCustomizationInstance::CreateStatic(&FHairWorksMaterialDetails::MakeInstance));
	RegisterCustomClassLayout("HairWorksComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FHairWorksComponentDetails::MakeInstance));
	// @third party code - END HairWorks
}


void FDetailCustomizationsModule::RegisterCustomClassLayout(FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate )
{
	check(ClassName != NAME_None);

	RegisteredClassNames.Add(ClassName);

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomClassLayout( ClassName, DetailLayoutDelegate );
}


void FDetailCustomizationsModule::RegisterCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate)
{
	check(PropertyTypeName != NAME_None);

	RegisteredPropertyTypes.Add(PropertyTypeName);

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomPropertyTypeLayout(PropertyTypeName, PropertyTypeLayoutDelegate);
}
