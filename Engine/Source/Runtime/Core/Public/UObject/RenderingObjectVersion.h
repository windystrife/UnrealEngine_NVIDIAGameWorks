// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-Rendering stream
struct CORE_API FRenderingObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Added support for 3 band SH in the ILC
		IndirectLightingCache3BandSupport,

		// Allows specifying resolution for reflection capture probes
		CustomReflectionCaptureResolutionSupport,

		RemovedTextureStreamingLevelData,

		// translucency is now a property which matters for materials with the decal domain
		IntroducedMeshDecals,

		// Reflection captures are no longer prenormalized
		ReflectionCapturesStoreAverageBrightness,

		ChangedPlanarReflectionFadeDefaults,

		RemovedRenderTargetSize,

		// Particle Cutout (SubUVAnimation) data is now stored in the ParticleRequired Module
		MovedParticleCutoutsToRequiredModule,

		MapBuildDataSeparatePackage,

		// StaticMesh and SkeletalMesh texcoord size data.
		TextureStreamingMeshUVChannelData,
		
		// Added type handling to material normalize and length (sqrt) nodes
		TypeHandlingForMaterialSqrtNodes,

		FixedBSPLightmaps,

		DistanceFieldSelfShadowBias,

		FixedLegacyMaterialAttributeNodeTypes,

		ShaderResourceCodeSharing,

		MotionBlurAndTAASupportInSceneCapture2d,

		AddedTextureRenderTargetFormats,

		// Triggers a rebuild of the mesh UV density while also adding an update in the postedit
		FixedMeshUVDensity,

		AddedbUseShowOnlyList,

		VolumetricLightmaps,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FRenderingObjectVersion() {}
};
