// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/MapErrors.h"
	
FName FMapErrors::MatchingLightGUID(TEXT("MatchingLightGUID"));
FName FMapErrors::ActorLargeShadowCaster(TEXT("ActorLargeShadowCaster"));
FName FMapErrors::NoDamageType(TEXT("NoDamageType"));
FName FMapErrors::NonCoPlanarPolys(TEXT("NonCoPlanarPolys"));
FName FMapErrors::SameLocation(TEXT("SameLocation"));
FName FMapErrors::InvalidDrawscale(TEXT("InvalidDrawscale"));
FName FMapErrors::ActorIsObselete(TEXT("ActorIsObselete"));
FName FMapErrors::StaticPhysNone(TEXT("StaticPhysNone"));
FName FMapErrors::VolumeActorCollisionComponentNULL(TEXT("VolumeActorCollisionComponentNULL"));
FName FMapErrors::VolumeActorZeroRadius(TEXT("VolumeActorZeroRadius"));
FName FMapErrors::VertexColorsNotMatchOriginalMesh(TEXT("VertexColorsNotMatchOriginalMesh"));
FName FMapErrors::CollisionEnabledNoCollisionGeom(TEXT("CollisionEnabledNoCollisionGeom"));
FName FMapErrors::ShadowCasterUsingBoundsScale(TEXT("ShadowCasterUsingBoundsScale"));
FName FMapErrors::MultipleSkyLights(TEXT("MultipleSkyLights"));
FName FMapErrors::InvalidTrace(TEXT("InvalidTrace"));
FName FMapErrors::BrushZeroPolygons(TEXT("BrushZeroPolygons"));
FName FMapErrors::CleanBSPMaterials(TEXT("CleanBSPMaterials"));
FName FMapErrors::BrushComponentNull(TEXT("BrushComponentNull"));
FName FMapErrors::PlanarBrush(TEXT("PlanarBrush"));
FName FMapErrors::CameraAspectRatioIsZero(TEXT("CameraAspectRatioIsZero"));
FName FMapErrors::AbstractClass(TEXT("AbstractClass"));
FName FMapErrors::DeprecatedClass(TEXT("DeprecatedClass"));
FName FMapErrors::FoliageMissingStaticMesh(TEXT("FoliageMissingStaticMesh"));
FName FMapErrors::FoliageMissingClusterComponent(TEXT("FoliageMissingStaticMesh"));
FName FMapErrors::FixedUpDeletedLayerWeightmap(TEXT("FixedUpDeletedLayerWeightmap"));
FName FMapErrors::FixedUpIncorrectLayerWeightmap(TEXT("FixedUpIncorrectLayerWeightmap"));
FName FMapErrors::FixedUpSharedLayerWeightmap(TEXT("FixedUpSharedLayerWeightmap"));
FName FMapErrors::LandscapeComponentPostLoad_Warning(TEXT("LandscapeComponentPostLoad_Warning"));
FName FMapErrors::DuplicateLevelInfo(TEXT("DuplicateLevelInfo"));
FName FMapErrors::NoKillZ(TEXT("NoKillZ"));
FName FMapErrors::LightComponentNull(TEXT("LightComponentNull"));
FName FMapErrors::RebuildLighting(TEXT("RebuildLighting"));
FName FMapErrors::StaticComponentHasInvalidLightmapSettings(TEXT("StaticComponentHasInvalidLightmapSettings"));
FName FMapErrors::RebuildPaths(TEXT("RebuildPaths"));
FName FMapErrors::ParticleSystemComponentNull(TEXT("ParticleSystemComponentNull"));
FName FMapErrors::PSysCompErrorEmptyActorRef(TEXT("PSysCompErrorEmptyActorRef"));
FName FMapErrors::PSysCompErrorEmptyMaterialRef(TEXT("PSysCompErrorEmptyMaterialRef"));
FName FMapErrors::SkelMeshActorNoPhysAsset(TEXT("SkelMeshActorNoPhysAsset"));
FName FMapErrors::SkeletalMeshComponent(TEXT("SkeletalMeshComponent"));
FName FMapErrors::SkeletalMeshNull(TEXT("SkeletalMeshNull"));
FName FMapErrors::AudioComponentNull(TEXT("AudioComponentNull"));
FName FMapErrors::SoundCueNull(TEXT("SoundCueNull"));
FName FMapErrors::StaticMeshNull(TEXT("StaticMeshNull"));
FName FMapErrors::StaticMeshComponent(TEXT("StaticMeshComponent"));
FName FMapErrors::SimpleCollisionButNonUniformScale(TEXT("SimpleCollisionButNonUniformScale"));
FName FMapErrors::MoreMaterialsThanReferenced(TEXT("MoreMaterialsThanReferenced"));
FName FMapErrors::ElementsWithZeroTriangles(TEXT("ElementsWithZeroTriangles"));
FName FMapErrors::LevelStreamingVolume(TEXT("LevelStreamingVolume"));
FName FMapErrors::NoLevelsAssociated(TEXT("NoLevelsAssociated"));
FName FMapErrors::FilenameIsTooLongForCooking(TEXT("FilenameIsTooLongForCooking"));
FName FMapErrors::UsingExternalObject(TEXT("UsingExternalObject"));
FName FMapErrors::RepairedPaintedVertexColors(TEXT("RepairedPaintedVertexColors"));
FName FMapErrors::LODActorMissingStaticMesh(TEXT("LODActorMissingStaticMesh"));
FName FMapErrors::LODActorMissingActor(TEXT("LODActorMissingActor"));
FName FMapErrors::LODActorNoActorFound(TEXT("LODActorNoActor"));
FName FMapErrors::HLODSystemNotEnabled(TEXT("HLODSystemNotEnabled"));

static const FString MapErrorsPath = TEXT("Shared/Editor/MapErrors");

FMapErrorToken::FMapErrorToken(const FName& InErrorName)
	: FDocumentationToken(MapErrorsPath, MapErrorsPath, InErrorName.ToString())
{
}

TSharedRef<FMapErrorToken> FMapErrorToken::Create(const FName& InErrorName)
{
	return MakeShareable(new FMapErrorToken(InErrorName));
}
