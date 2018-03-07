// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DirectionalLightComponent.cpp: DirectionalLightComponent implementation.
=============================================================================*/

#include "Components/DirectionalLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "RenderingThread.h"
#include "ConvexVolume.h"
#include "SceneInterface.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "Engine/DirectionalLight.h"

static float GMaxCSMRadiusToAllowPerObjectShadows = 8000;
static FAutoConsoleVariableRef CVarMaxCSMRadiusToAllowPerObjectShadows(
	TEXT("r.MaxCSMRadiusToAllowPerObjectShadows"),
	GMaxCSMRadiusToAllowPerObjectShadows,
	TEXT("Only stationary lights with a CSM radius smaller than this will create per object shadows for dynamic objects.")
	);

static TAutoConsoleVariable<float> CVarUnbuiltWholeSceneDynamicShadowRadius(
	TEXT("r.Shadow.UnbuiltWholeSceneDynamicShadowRadius"),
	200000.0f,
	TEXT("WholeSceneDynamicShadowRadius to use when using CSM to preview unbuilt lighting from a directional light")
	);

static TAutoConsoleVariable<int32> CVarUnbuiltNumWholeSceneDynamicShadowCascades(
	TEXT("r.Shadow.UnbuiltNumWholeSceneDynamicShadowCascades"),
	4,
	TEXT("DynamicShadowCascades to use when using CSM to preview unbuilt lighting from a directional light"),
	ECVF_RenderThreadSafe);

/**
 * The directional light policy for TMeshLightingDrawingPolicy.
 */
class FDirectionalLightPolicy
{
public:
	typedef FLightSceneInfo SceneInfoType;
};

/**
 * The scene info for a directional light.
 */
class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:

	/** Whether to occlude fog and atmosphere inscattering with screenspace blurred occlusion from this light. */
	bool bEnableLightShaftOcclusion;

	/** 
	 * Controls how dark the occlusion masking is, a value of 1 results in no darkening term.
	 */
	float OcclusionMaskDarkness;

	/** Everything closer to the camera than this distance will occlude light shafts. */
	float OcclusionDepthRange;

	/** 
	 * Can be used to make light shafts come from somewhere other than the light's actual direction. 
	 * Will only be used when non-zero.
	 */
	FVector LightShaftOverrideDirection;

	/** 
	 * Radius of the whole scene dynamic shadow centered on the viewer, which replaces the precomputed shadows based on distance from the camera.  
	 * A Radius of 0 disables the dynamic shadow.
	 */
	float WholeSceneDynamicShadowRadius;

	/** 
	 * Number of cascades to split the view frustum into for the whole scene dynamic shadow.  
	 * More cascades result in better shadow resolution and allow WholeSceneDynamicShadowRadius to be further, but add rendering cost.
	 */
	uint32 DynamicShadowCascades;

	/** 
	 * Exponent that is applied to the cascade transition distances as a fraction of WholeSceneDynamicShadowRadius.
	 * An exponent of 1 means that cascade transitions will happen at a distance proportional to their resolution.
	 * A value greater than 1 brings transitions closer to the camera.
	 */
	float CascadeDistributionExponent;

	/** see UDirectionalLightComponent::CascadeTransitionFraction */
	float CascadeTransitionFraction;

	/** see UDirectionalLightComponent::ShadowDistanceFadeoutFraction */
	float ShadowDistanceFadeoutFraction;

	bool bUseInsetShadowsForMovableObjects;

	/** If greater than WholeSceneDynamicShadowRadius, a cascade will be created to support ray traced distance field shadows covering up to this distance. */
	float DistanceFieldShadowDistance;

	/** Light source angle in degrees. */
	float LightSourceAngle;

	/** Determines how far shadows can be cast, in world units.  Larger values increase the shadowing cost. */
	float TraceDistance;

	/** Initialization constructor. */
	FDirectionalLightSceneProxy(const UDirectionalLightComponent* Component):
		FLightSceneProxy(Component),
		bEnableLightShaftOcclusion(Component->bEnableLightShaftOcclusion),
		OcclusionMaskDarkness(Component->OcclusionMaskDarkness),
		OcclusionDepthRange(Component->OcclusionDepthRange),
		LightShaftOverrideDirection(Component->LightShaftOverrideDirection),
		DynamicShadowCascades(Component->DynamicShadowCascades > 0 ? Component->DynamicShadowCascades : 0),
		CascadeDistributionExponent(Component->CascadeDistributionExponent),
		CascadeTransitionFraction(Component->CascadeTransitionFraction),
		ShadowDistanceFadeoutFraction(Component->ShadowDistanceFadeoutFraction),
		bUseInsetShadowsForMovableObjects(Component->bUseInsetShadowsForMovableObjects),
		DistanceFieldShadowDistance(Component->bUseRayTracedDistanceFieldShadows ? Component->DistanceFieldShadowDistance : 0),
		LightSourceAngle(Component->LightSourceAngle),
		TraceDistance(FMath::Clamp(Component->TraceDistance, 1000.0f, 1000000.0f))
	{
		LightShaftOverrideDirection.Normalize();

		if(Component->Mobility == EComponentMobility::Movable)
		{
			WholeSceneDynamicShadowRadius = Component->DynamicShadowDistanceMovableLight;
		}
		else
		{
			WholeSceneDynamicShadowRadius = Component->DynamicShadowDistanceStationaryLight;
		}

		const float FarCascadeSize = Component->FarShadowDistance - WholeSceneDynamicShadowRadius;

		// 100 is just some number to avoid cascades of 0 size, the user still can still create many tiny cascades (very inefficient)
		if(Component->FarShadowCascadeCount && FarCascadeSize > 100.0f)
		{
			FarShadowDistance = Component->FarShadowDistance;
			FarShadowCascadeCount = Component->FarShadowCascadeCount;
		}

		{
			const FSceneInterface* Scene = Component->GetScene();
			// ensure bUseWholeSceneCSMForMovableObjects is only be used with the forward renderer.
			const bool bUsingDeferredRenderer = Scene == nullptr ? true : Scene->GetShadingPath() == EShadingPath::Deferred;
			bUseWholeSceneCSMForMovableObjects = Component->Mobility == EComponentMobility::Stationary && !Component->bUseInsetShadowsForMovableObjects && !bUsingDeferredRenderer;
		}
		bCastModulatedShadows = Component->bCastModulatedShadows;
		ModulatedShadowColor = FLinearColor(Component->ModulatedShadowColor);
	}

	void UpdateLightShaftOverrideDirection_GameThread(const UDirectionalLightComponent* Component)
	{
		FVector NewLightShaftOverrideDirection = Component->LightShaftOverrideDirection;
		NewLightShaftOverrideDirection.Normalize();
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FUpdateLightShaftOverrideDirectionCommand,
			FDirectionalLightSceneProxy*,Proxy,this,
			FVector,NewLightShaftOverrideDirection,NewLightShaftOverrideDirection,
		{
			Proxy->UpdateLightShaftOverrideDirection_RenderThread(NewLightShaftOverrideDirection);
		});
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FLightParameters& LightParameters) const override
	{
		LightParameters.LightPositionAndInvRadius = FVector4(0, 0, 0, 0);

		LightParameters.LightColorAndFalloffExponent = FVector4(
			GetColor().R,
			GetColor().G,
			GetColor().B,
			0);

		LightParameters.NormalizedLightDirection = -GetDirection();

		LightParameters.NormalizedLightTangent = -GetDirection();

		LightParameters.SpotAngles = FVector2D(0, 0);
		LightParameters.LightSourceRadius = 0.0f;
		LightParameters.LightSoftSourceRadius = 0.0f;
		LightParameters.LightSourceLength = 0.0f;
		// Prevent 0 Roughness which causes NaNs in Vis_SmithJointApprox
		LightParameters.LightMinRoughness = FMath::Max(MinRoughness, .02f);
	}

	virtual float GetLightSourceAngle() const override
	{
		return LightSourceAngle;
	}

	virtual float GetTraceDistance() const override
	{
		return TraceDistance;
	}

	virtual bool GetLightShaftOcclusionParameters(float& OutOcclusionMaskDarkness, float& OutOcclusionDepthRange) const override
	{
		OutOcclusionMaskDarkness = OcclusionMaskDarkness;
		OutOcclusionDepthRange = OcclusionDepthRange;
		return bEnableLightShaftOcclusion;
	}

	virtual FVector GetLightPositionForLightShafts(FVector ViewOrigin) const override
	{
		const FVector EffectiveDirection = LightShaftOverrideDirection.SizeSquared() > 0 ? LightShaftOverrideDirection : GetDirection();
		return ViewOrigin - EffectiveDirection * WORLD_MAX;
	}

	// FLightSceneInfo interface.

	virtual bool ShouldCreatePerObjectShadowsForDynamicObjects() const override
	{
		return FLightSceneProxy::ShouldCreatePerObjectShadowsForDynamicObjects()
			// Only create per-object shadows for dynamic objects if the CSM range is under some threshold
			// When the CSM range is very small, CSM is just being used to provide high resolution / animating shadows near the player,
			// But dynamic objects outside the CSM range would not have a shadow (or ones inside the range that cast a shadow out of the CSM area of influence).
			&& WholeSceneDynamicShadowRadius < GMaxCSMRadiusToAllowPerObjectShadows
			&& bUseInsetShadowsForMovableObjects;
	}

	/** Whether this light should create CSM for dynamic objects only (mobile renderer) */
	virtual bool UseCSMForDynamicObjects() const override
	{
		return	FLightSceneProxy::ShouldCreatePerObjectShadowsForDynamicObjects()
				&& bUseWholeSceneCSMForMovableObjects
				&& WholeSceneDynamicShadowRadius > 0;
	}

	/** Returns the number of view dependent shadows this light will create, not counting distance field shadow cascades. */
	virtual uint32 GetNumViewDependentWholeSceneShadows(const FSceneView& View, bool bPrecomputedLightingIsValid) const override
	{ 
		uint32 TotalCascades = GetNumShadowMappedCascades(View.MaxShadowCascades, bPrecomputedLightingIsValid) + FarShadowCascadeCount;

		return TotalCascades;
	}

	/**
	 * Sets up a projected shadow initializer that's dependent on the current view for shadows from the entire scene.
	 * @param InCascadeIndex ShadowSplitIndex or INDEX_NONE for the the DistanceFieldCascade
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetViewDependentWholeSceneProjectedShadowInitializer(const FSceneView& View, int32 InCascadeIndex, bool bPrecomputedLightingIsValid, FWholeSceneProjectedShadowInitializer& OutInitializer) const override
	{
		const bool bRayTracedCascade = (InCascadeIndex == INDEX_NONE);

		FSphere Bounds = FDirectionalLightSceneProxy::GetShadowSplitBounds(View, InCascadeIndex, bPrecomputedLightingIsValid, &OutInitializer.CascadeSettings);

		uint32 NumNearCascades = GetNumShadowMappedCascades(View.MaxShadowCascades, bPrecomputedLightingIsValid);

		// Last cascade is the ray traced shadow, if enabled
		OutInitializer.CascadeSettings.ShadowSplitIndex = bRayTracedCascade ? NumNearCascades : InCascadeIndex;

		const float ShadowExtent = Bounds.W / FMath::Sqrt(3.0f);
		const FBoxSphereBounds SubjectBounds(Bounds.Center, FVector(ShadowExtent, ShadowExtent, ShadowExtent), Bounds.W);
		OutInitializer.PreShadowTranslation = -Bounds.Center;
		OutInitializer.WorldToLight = FInverseRotationMatrix(GetDirection().GetSafeNormal().Rotation());
		OutInitializer.Scales = FVector(1.0f,1.0f / Bounds.W,1.0f / Bounds.W);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector,SubjectBounds.BoxExtent,SubjectBounds.SphereRadius);
		OutInitializer.WAxis = FVector4(0,0,0,1);
		OutInitializer.MinLightW = -HALF_WORLD_MAX;
		// Reduce casting distance on a directional light
		// This is necessary to improve floating point precision in several places, especially when deriving frustum verts from InvReceiverMatrix
		OutInitializer.MaxDistanceToCastInLightW = HALF_WORLD_MAX / 32.0f;
		OutInitializer.bRayTracedDistanceField = bRayTracedCascade;
		OutInitializer.CascadeSettings.bFarShadowCascade = !bRayTracedCascade && OutInitializer.CascadeSettings.ShadowSplitIndex >= (int32)NumNearCascades;
		return true;
	}

	// reflective shadow map for Light Propagation Volume
	virtual bool GetViewDependentRsmWholeSceneProjectedShadowInitializer(
		const class FSceneView& View, 
		const FBox& LightPropagationVolumeBounds,
	    class FWholeSceneProjectedShadowInitializer& OutInitializer ) const override
	{
		float LpvExtent = LightPropagationVolumeBounds.GetExtent().X; // LPV is a cube, so this should be valid

		OutInitializer.PreShadowTranslation = -LightPropagationVolumeBounds.GetCenter();
		OutInitializer.WorldToLight = FInverseRotationMatrix(GetDirection().GetSafeNormal().Rotation());
		OutInitializer.Scales = FVector(1.0f,1.0f / LpvExtent,1.0f / LpvExtent);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds( FVector::ZeroVector, LightPropagationVolumeBounds.GetExtent(), FMath::Sqrt( LpvExtent * LpvExtent * 3.0f ) );
		OutInitializer.WAxis = FVector4(0,0,0,1);
		OutInitializer.MinLightW = -HALF_WORLD_MAX;
		// Reduce casting distance on a directional light
		// This is necessary to improve floating point precision in several places, especially when deriving frustum verts from InvReceiverMatrix
		OutInitializer.MaxDistanceToCastInLightW = HALF_WORLD_MAX / 32.0f;

		// Compute the RSM bounds
		{
			FVector Centre = LightPropagationVolumeBounds.GetCenter();
			FVector Extent = LightPropagationVolumeBounds.GetExtent();
			FVector CascadeFrustumVerts[8];
			CascadeFrustumVerts[0] = Centre + Extent * FVector( -1.0f,-1.0f, 1.0f ); // 0 Near Top    Right
			CascadeFrustumVerts[1] = Centre + Extent * FVector( -1.0f,-1.0f,-1.0f ); // 1 Near Bottom Right
			CascadeFrustumVerts[2] = Centre + Extent * FVector(  1.0f,-1.0f, 1.0f ); // 2 Near Top    Left
			CascadeFrustumVerts[3] = Centre + Extent * FVector(  1.0f,-1.0f,-1.0f ); // 3 Near Bottom Left
			CascadeFrustumVerts[4] = Centre + Extent * FVector( -1.0f, 1.0f, 1.0f ); // 4 Far  Top    Right
			CascadeFrustumVerts[5] = Centre + Extent * FVector( -1.0f, 1.0f,-1.0f ); // 5 Far  Bottom Right
			CascadeFrustumVerts[6] = Centre + Extent * FVector(  1.0f, 1.0f, 1.0f ); // 6 Far  Top    Left
			CascadeFrustumVerts[7] = Centre + Extent * FVector(  1.0f, 1.0f,-1.0f ); // 7 Far  Bottom Left

			FPlane Far;
			FPlane Near;
			const FVector LightDirection = -GetDirection();
			ComputeShadowCullingVolume(View.bReverseCulling, CascadeFrustumVerts, LightDirection, OutInitializer.CascadeSettings.ShadowBoundsAccurate, Near, Far);
		}
		return true;
	}

	virtual FVector2D GetDirectionalLightDistanceFadeParameters(ERHIFeatureLevel::Type InFeatureLevel, bool bPrecomputedLightingIsValid, int32 MaxNearCascades) const override
	{
		float FarDistance = GetCSMMaxDistance(bPrecomputedLightingIsValid, MaxNearCascades);
		{
			if (ShouldCreateRayTracedCascade(InFeatureLevel, bPrecomputedLightingIsValid, MaxNearCascades))
			{
				FarDistance = GetDistanceFieldShadowDistance();
			}
			FarDistance = FMath::Max(FarDistance, FarShadowDistance);
		}
	    
		// The far distance for the dynamic to static fade is the range of the directional light.
		// The near distance is placed at a depth of 90% of the light's range.
		const float NearDistance = FarDistance - FarDistance * ShadowDistanceFadeoutFraction;
		return FVector2D(NearDistance, 1.0f / FMath::Max<float>(FarDistance - NearDistance, KINDA_SMALL_NUMBER));
	}

	virtual bool GetPerObjectProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,FPerObjectProjectedShadowInitializer& OutInitializer) const override
	{
		OutInitializer.PreShadowTranslation = -SubjectBounds.Origin;
		OutInitializer.WorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
		OutInitializer.Scales = FVector(1.0f,1.0f / SubjectBounds.SphereRadius,1.0f / SubjectBounds.SphereRadius);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector,SubjectBounds.BoxExtent,SubjectBounds.SphereRadius);
		OutInitializer.WAxis = FVector4(0,0,0,1);
		OutInitializer.MinLightW = -HALF_WORLD_MAX;
		// Reduce casting distance on a directional light
		// This is necessary to improve floating point precision in several places, especially when deriving frustum verts from InvReceiverMatrix
		OutInitializer.MaxDistanceToCastInLightW = HALF_WORLD_MAX / 32.0f;
		return true;
	}

	virtual bool ShouldCreateRayTracedCascade(ERHIFeatureLevel::Type InFeatureLevel, bool bPrecomputedLightingIsValid, int32 MaxNearCascades) const override
	{
		const uint32 NumCascades = GetNumShadowMappedCascades(MaxNearCascades, bPrecomputedLightingIsValid);
		const float RaytracedShadowDistance = GetDistanceFieldShadowDistance();
		const bool bCreateWithCSM = NumCascades > 0 && RaytracedShadowDistance > GetCSMMaxDistance(bPrecomputedLightingIsValid, MaxNearCascades);
		const bool bCreateWithoutCSM = NumCascades == 0 && RaytracedShadowDistance > 0;
		return DoesPlatformSupportDistanceFieldShadowing(GShaderPlatformForFeatureLevel[InFeatureLevel]) && (bCreateWithCSM || bCreateWithoutCSM);
	}

private:

	float GetEffectiveWholeSceneDynamicShadowRadius(bool bPrecomputedLightingIsValid) const
	{
		return bPrecomputedLightingIsValid ? WholeSceneDynamicShadowRadius : CVarUnbuiltWholeSceneDynamicShadowRadius.GetValueOnAnyThread();
	}

	uint32 GetNumShadowMappedCascades(uint32 MaxShadowCascades, bool bPrecomputedLightingIsValid) const
	{
		int32 EffectiveNumDynamicShadowCascades = DynamicShadowCascades;
		
		if (!bPrecomputedLightingIsValid)
		{
			EffectiveNumDynamicShadowCascades = FMath::Max(0, CVarUnbuiltNumWholeSceneDynamicShadowCascades.GetValueOnAnyThread());

			static const auto CVarUnbuiltPreviewShadowsInGame = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.UnbuiltPreviewInGame"));
			bool bUnbuiltPreviewShadowsInGame = CVarUnbuiltPreviewShadowsInGame->GetInt() != 0;

			if (!bUnbuiltPreviewShadowsInGame && !GetSceneInterface()->IsEditorScene())
			{
				EffectiveNumDynamicShadowCascades = 0;
			}
		}

		const int32 NumCascades = GetCSMMaxDistance(bPrecomputedLightingIsValid, MaxShadowCascades) > 0.0f ? EffectiveNumDynamicShadowCascades : 0;
		return FMath::Min<int32>(NumCascades, MaxShadowCascades);
	}

	float GetCSMMaxDistance(bool bPrecomputedLightingIsValid, int32 MaxShadowCascades) const
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.DistanceScale"));
		
		if (MaxShadowCascades <= 0)
		{
			return 0.0f;
		}

		float Scale = FMath::Clamp(CVar->GetValueOnRenderThread(), 0.0f, 2.0f);
		float Distance = GetEffectiveWholeSceneDynamicShadowRadius(bPrecomputedLightingIsValid) * Scale;
		return Distance;
	}

	float GetDistanceFieldShadowDistance() const
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));

		if (CVar->GetValueOnRenderThread() == 0)
		{
			// Meshes must have distance fields generated
			return 0;
		}

		return DistanceFieldShadowDistance;
	}

	float GetShadowTransitionScale() const
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.CSM.TransitionScale"));
		float Scale = FMath::Clamp(CVar->GetValueOnRenderThread(), 0.0f, 2.0f);
		return Scale;
	}

	void UpdateLightShaftOverrideDirection_RenderThread(FVector NewLightShaftOverrideDirection)
	{
		LightShaftOverrideDirection = NewLightShaftOverrideDirection;
	}

	// Computes a shadow culling volume (convex hull) based on a set of 8 vertices and a light direction
	void ComputeShadowCullingVolume(bool bReverseCulling, const FVector* CascadeFrustumVerts, const FVector& LightDirection, FConvexVolume& ConvexVolumeOut, FPlane& NearPlaneOut, FPlane& FarPlaneOut) const
	{
		// For mobile platforms that switch vertical axis and MobileHDR == false the sense of bReverseCulling is inverted.
		bReverseCulling = XOR(bReverseCulling, (RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]) && !IsMobileHDR()));

		// Pairs of plane indices from SubFrustumPlanes whose intersections
		// form the edges of the frustum.
		static const int32 AdjacentPlanePairs[12][2] =
		{
			{0,2}, {0,4}, {0,1}, {0,3},
			{2,3}, {4,2}, {1,4}, {3,1},
			{2,5}, {4,5}, {1,5}, {3,5}
		};
		// Maps a plane pair index to the index of the two frustum corners
		// which form the end points of the plane intersection.
		static const int32 LineVertexIndices[12][2] =
		{
			{0,1}, {1,3}, {3,2}, {2,0},
			{0,4}, {1,5}, {3,7}, {2,6},
			{4,5}, {5,7}, {7,6}, {6,4}
		};

		TArray<FPlane, TInlineAllocator<6>> Planes;

		// Find the view frustum subsection planes which face away from the light and add them to the bounding volume
		FPlane SubFrustumPlanes[6];
		if (!bReverseCulling)
		{
			SubFrustumPlanes[0] = FPlane(CascadeFrustumVerts[3], CascadeFrustumVerts[2], CascadeFrustumVerts[0]); // Near
			SubFrustumPlanes[1] = FPlane(CascadeFrustumVerts[7], CascadeFrustumVerts[6], CascadeFrustumVerts[2]); // Left
			SubFrustumPlanes[2] = FPlane(CascadeFrustumVerts[0], CascadeFrustumVerts[4], CascadeFrustumVerts[5]); // Right
			SubFrustumPlanes[3] = FPlane(CascadeFrustumVerts[2], CascadeFrustumVerts[6], CascadeFrustumVerts[4]); // Top
			SubFrustumPlanes[4] = FPlane(CascadeFrustumVerts[5], CascadeFrustumVerts[7], CascadeFrustumVerts[3]); // Bottom
			SubFrustumPlanes[5] = FPlane(CascadeFrustumVerts[4], CascadeFrustumVerts[6], CascadeFrustumVerts[7]); // Far
		}
		else
		{
			SubFrustumPlanes[0] = FPlane(CascadeFrustumVerts[0], CascadeFrustumVerts[2], CascadeFrustumVerts[3]); // Near
			SubFrustumPlanes[1] = FPlane(CascadeFrustumVerts[2], CascadeFrustumVerts[6], CascadeFrustumVerts[7]); // Left
			SubFrustumPlanes[2] = FPlane(CascadeFrustumVerts[5], CascadeFrustumVerts[4], CascadeFrustumVerts[0]); // Right
			SubFrustumPlanes[3] = FPlane(CascadeFrustumVerts[4], CascadeFrustumVerts[6], CascadeFrustumVerts[2]); // Top
			SubFrustumPlanes[4] = FPlane(CascadeFrustumVerts[3], CascadeFrustumVerts[7], CascadeFrustumVerts[5]); // Bottom
			SubFrustumPlanes[5] = FPlane(CascadeFrustumVerts[7], CascadeFrustumVerts[6], CascadeFrustumVerts[4]); // Far
		}

		NearPlaneOut = SubFrustumPlanes[0];
		FarPlaneOut = SubFrustumPlanes[5];

		// Add the planes from the camera's frustum which form the back face of the frustum when in light space.
		for (int32 i = 0; i < 6; i++)
		{
			FVector Normal(SubFrustumPlanes[i]);
			float d = Normal | LightDirection;
			if ( d < 0.0f )
			{
				Planes.Add(SubFrustumPlanes[i]);
			}
		}

		// Now add the planes which form the silhouette edges of the camera frustum in light space.
		for (int32 i = 0; i < 12; i++)
		{
			FVector NormalA(SubFrustumPlanes[AdjacentPlanePairs[i][0]]);
			FVector NormalB(SubFrustumPlanes[AdjacentPlanePairs[i][1]]);

			float DotA = NormalA | LightDirection;
			float DotB = NormalB | LightDirection;

			// If the signs of the dot product are different
			if ( DotA * DotB < 0.0f )
			{
				// Planes are opposing, so this is an edge. 
				// Extrude the plane along the light direction, and add it to the array.

				FVector A = CascadeFrustumVerts[LineVertexIndices[i][0]];
				FVector B = CascadeFrustumVerts[LineVertexIndices[i][1]];
				// Scale the 3rd vector by the length of AB for precision
				FVector C = A + LightDirection * (A - B).Size(); 

				// Account for winding
				if (XOR(DotA >= 0.0f, bReverseCulling)) 
				{
					Planes.Add(FPlane(A, B, C));
				}
				else
				{
					Planes.Add(FPlane(B, A, C));
				}
			}
		}
		ConvexVolumeOut = FConvexVolume(Planes);

		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		if (bCastVxgiIndirectLighting)
		{
			ConvexVolumeOut = FConvexVolume();
		}
#endif
		// NVCHANGE_END: Add VXGI
	}

	// Useful helper function to compute shadow map cascade distribution
	// @param Exponent >=1, 1:linear, 2:each cascade gets 2x in size, ...
	// @param CascadeIndex 0..CascadeCount
	// @param CascadeCount >0
	static float ComputeAccumulatedScale(float Exponent, int32 CascadeIndex, int32 CascadeCount)
	{
		if(CascadeIndex <= 0)
		{
			return 0.0f;
		}

		float CurrentScale = 1;
		float TotalScale = 0.0f;
		float Ret = 0.0f;

		// lame implementation for simplicity, CascadeIndex is small anyway
		for(int i = 0; i < CascadeCount; ++i)
		{
			if(i < CascadeIndex)
			{
				Ret += CurrentScale;
			}
			TotalScale += CurrentScale;
			CurrentScale *= Exponent;
		}

		return Ret / TotalScale;
	}
	
	float GetEffectiveCascadeDistributionExponent(bool bPrecomputedLightingIsValid) const
	{
		return bPrecomputedLightingIsValid ? CascadeDistributionExponent : 4;
	}

	// @param SplitIndex 0: near, 1:end of 1st cascade, ...
	inline float GetSplitDistance(const FSceneView& View, uint32 SplitIndex, bool bPrecomputedLightingIsValid, bool bDistanceFieldShadows) const
	{
		// near cascade means non far and non distance field cascade
		uint32 NumNearCascades = GetNumShadowMappedCascades(View.MaxShadowCascades, bPrecomputedLightingIsValid);

		float CascadeDistanceWithoutFar = GetCSMMaxDistance(bPrecomputedLightingIsValid, View.MaxShadowCascades);
		float ShadowNear = View.NearClippingDistance;
		const float EffectiveCascadeDistributionExponent = GetEffectiveCascadeDistributionExponent(bPrecomputedLightingIsValid);

		// non near cascades are split differently for distance field shadow
		if(SplitIndex > NumNearCascades)
		{
			if(bDistanceFieldShadows)
			{
				// there is only one distance field shadow cascade
				check(SplitIndex == NumNearCascades + 1);
				return DistanceFieldShadowDistance;
			}
			else
			{
				// the far cascades start at the after the near cascades
				return CascadeDistanceWithoutFar + ComputeAccumulatedScale(EffectiveCascadeDistributionExponent, SplitIndex - NumNearCascades, FarShadowCascadeCount) * (FarShadowDistance - CascadeDistanceWithoutFar);
			}
		}
		else
		{
			return ShadowNear + ComputeAccumulatedScale(EffectiveCascadeDistributionExponent, SplitIndex, NumNearCascades) * (CascadeDistanceWithoutFar - ShadowNear);
		}
	}

	virtual FSphere GetShadowSplitBoundsDepthRange(const FSceneView& View, FVector ViewOrigin, float SplitNear, float SplitFar, FShadowCascadeSettings* OutCascadeSettings) const override
	{
		const FMatrix& ViewMatrix = View.ShadowViewMatrices.GetViewMatrix();
		const FMatrix& ProjectionMatrix = View.ShadowViewMatrices.GetProjectionMatrix();

		const FVector CameraDirection = ViewMatrix.GetColumn(2);
		const FVector LightDirection = -GetDirection();

		// Get FOV and AspectRatio from the view's projection matrix.
		float AspectRatio = ProjectionMatrix.M[1][1] / ProjectionMatrix.M[0][0];
		float HalfFOV = View.ShadowViewMatrices.IsPerspectiveProjection() ? FMath::Atan(1.0f / ProjectionMatrix.M[0][0]) : PI/4.0f;

		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		if (bCastVxgiIndirectLighting)
		{
			// Force this hard-coded FOV to avoid numerical instability when ViewOrigin.W is close to 0.0f
			HalfFOV = PI / 4.0f;
		}
#endif
		// NVCHANGE_END: Add VXGI

		// Build the camera frustum for this cascade
		const float StartHorizontalLength = SplitNear * FMath::Tan(HalfFOV);
		const FVector StartCameraRightOffset = ViewMatrix.GetColumn(0) * StartHorizontalLength;
		const float StartVerticalLength = StartHorizontalLength / AspectRatio;
		const FVector StartCameraUpOffset = ViewMatrix.GetColumn(1) * StartVerticalLength;

		const float EndHorizontalLength = SplitFar * FMath::Tan(HalfFOV);
		const FVector EndCameraRightOffset = ViewMatrix.GetColumn(0) * EndHorizontalLength;
		const float EndVerticalLength = EndHorizontalLength / AspectRatio;
		const FVector EndCameraUpOffset = ViewMatrix.GetColumn(1) * EndVerticalLength;

		// Get the 8 corners of the cascade's camera frustum, in world space
		FVector CascadeFrustumVerts[8];
		CascadeFrustumVerts[0] = ViewOrigin + CameraDirection * SplitNear + StartCameraRightOffset + StartCameraUpOffset; // 0 Near Top    Right
		CascadeFrustumVerts[1] = ViewOrigin + CameraDirection * SplitNear + StartCameraRightOffset - StartCameraUpOffset; // 1 Near Bottom Right
		CascadeFrustumVerts[2] = ViewOrigin + CameraDirection * SplitNear - StartCameraRightOffset + StartCameraUpOffset; // 2 Near Top    Left
		CascadeFrustumVerts[3] = ViewOrigin + CameraDirection * SplitNear - StartCameraRightOffset - StartCameraUpOffset; // 3 Near Bottom Left
		CascadeFrustumVerts[4] = ViewOrigin + CameraDirection * SplitFar  + EndCameraRightOffset   + EndCameraUpOffset;   // 4 Far  Top    Right
		CascadeFrustumVerts[5] = ViewOrigin + CameraDirection * SplitFar  + EndCameraRightOffset   - EndCameraUpOffset;   // 5 Far  Bottom Right
		CascadeFrustumVerts[6] = ViewOrigin + CameraDirection * SplitFar  - EndCameraRightOffset   + EndCameraUpOffset;   // 6 Far  Top    Left
		CascadeFrustumVerts[7] = ViewOrigin + CameraDirection * SplitFar  - EndCameraRightOffset   - EndCameraUpOffset;   // 7 Far  Bottom Left

		// Fit a bounding sphere around the world space camera cascade frustum.
		// Compute the sphere ideal centre point given the FOV and near/far.
		float TanHalfFOVx = FMath::Tan(HalfFOV);
		float TanHalfFOVy = TanHalfFOVx / AspectRatio;
		float FrustumLength = SplitFar - SplitNear;

		float FarX = TanHalfFOVx * SplitFar;
		float FarY = TanHalfFOVy * SplitFar;
		float DiagonalASq = FarX * FarX + FarY * FarY;

		float NearX = TanHalfFOVx * SplitNear;
		float NearY = TanHalfFOVy * SplitNear;
		float DiagonalBSq = NearX * NearX + NearY * NearY;

		// Calculate the ideal bounding sphere for the subfrustum.
		// Fx  = (Db^2 - da^2) / 2Fl + Fl / 2 
		// (where Da is the far diagonal, and Db is the near, and Fl is the frustum length)
		float OptimalOffset = (DiagonalBSq - DiagonalASq) / (2.0f * FrustumLength) + FrustumLength * 0.5f;
		float CentreZ = SplitFar - OptimalOffset;
		CentreZ = FMath::Clamp( CentreZ, SplitNear, SplitFar );
		FSphere CascadeSphere(ViewOrigin + CameraDirection * CentreZ, 0);
		for (int32 Index = 0; Index < 8; Index++)
		{
			CascadeSphere.W = FMath::Max(CascadeSphere.W, FVector::DistSquared(CascadeFrustumVerts[Index], CascadeSphere.Center));
		}

		// Don't allow the bounds to reach 0 (INF)
		CascadeSphere.W = FMath::Max(FMath::Sqrt(CascadeSphere.W), 1.0f); 

		if (OutCascadeSettings)
		{
			// this function is needed, since it's also called by the LPV code.
			ComputeShadowCullingVolume(View.bReverseCulling, CascadeFrustumVerts, LightDirection, OutCascadeSettings->ShadowBoundsAccurate, OutCascadeSettings->NearFrustumPlane, OutCascadeSettings->FarFrustumPlane);
		}

		return CascadeSphere;
	}

	// @param InShadowSplitIndex cascade index or InShadowSplitIndex == INDEX_NONE for the distance field cascade
	virtual FSphere GetShadowSplitBounds(const FSceneView& View, int32 InCascadeIndex, bool bPrecomputedLightingIsValid, FShadowCascadeSettings* OutCascadeSettings) const override
	{
		uint32 NumNearCascades = GetNumShadowMappedCascades(View.MaxShadowCascades, bPrecomputedLightingIsValid);

		const bool bHasRayTracedCascade = ShouldCreateRayTracedCascade(View.GetFeatureLevel(), bPrecomputedLightingIsValid, View.MaxShadowCascades);

		// this checks for WholeSceneDynamicShadowRadius and DynamicShadowCascades
		uint32 NumNearAndFarCascades = GetNumViewDependentWholeSceneShadows(View, bPrecomputedLightingIsValid);

		uint32 NumTotalCascades = FMath::Max(NumNearAndFarCascades, NumNearCascades + (bHasRayTracedCascade ? 1 : 0));

		const bool bIsRayTracedCascade = InCascadeIndex == INDEX_NONE;
		const uint32 ShadowSplitIndex = bIsRayTracedCascade ? NumNearCascades : InCascadeIndex;		// todo -1?

		// Determine start and end distances to the current cascade's split planes
		// Presence of the ray traced cascade does not change depth ranges for the shadow-mapped cascades
		float SplitNear = GetSplitDistance(View, ShadowSplitIndex, bPrecomputedLightingIsValid, bIsRayTracedCascade);
		float SplitFar = GetSplitDistance(View, ShadowSplitIndex + 1, bPrecomputedLightingIsValid, bIsRayTracedCascade);
		float FadePlane = SplitFar;

		float LocalCascadeTransitionFraction = CascadeTransitionFraction * GetShadowTransitionScale();

		float FadeExtension = (SplitFar - SplitNear) * LocalCascadeTransitionFraction;

		// Add the fade region to the end of the subfrustum, if this is not the last cascade.
		if ((int32)ShadowSplitIndex < (int32)NumTotalCascades - 1)
		{
			SplitFar += FadeExtension;			
		}

		if(OutCascadeSettings)
		{
			OutCascadeSettings->SplitFarFadeRegion = FadeExtension;

			OutCascadeSettings->SplitNearFadeRegion = 0.0f;

			if(ShadowSplitIndex >= 1)
			{
				// only used to fade the translucency lighting volume

				float BeforeSplitNear = GetSplitDistance(View, ShadowSplitIndex - 1, bPrecomputedLightingIsValid, bIsRayTracedCascade);
				float BeforeSplitFar = GetSplitDistance(View, ShadowSplitIndex, bPrecomputedLightingIsValid, bIsRayTracedCascade);

				OutCascadeSettings->SplitNearFadeRegion = (BeforeSplitFar - BeforeSplitNear) * LocalCascadeTransitionFraction;
			}

			// Pass out the split settings
			OutCascadeSettings->SplitFar = SplitFar;
			OutCascadeSettings->SplitNear = SplitNear;
			OutCascadeSettings->FadePlaneOffset = FadePlane;
			OutCascadeSettings->FadePlaneLength = SplitFar - FadePlane;
		}

		const FSphere CascadeSphere = FDirectionalLightSceneProxy::GetShadowSplitBoundsDepthRange(View, View.ViewMatrices.GetViewOrigin(), SplitNear, SplitFar, OutCascadeSettings);

		return CascadeSphere;
	}
};

UDirectionalLightComponent::UDirectionalLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightDirectional"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightDirectionalMove"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	Intensity = 10;

	bEnableLightShaftOcclusion = false;
	OcclusionDepthRange = 100000.f;
	OcclusionMaskDarkness = 0.05f;

	WholeSceneDynamicShadowRadius_DEPRECATED = 20000.0f;
	DynamicShadowDistanceMovableLight = 20000.0f;
	DynamicShadowDistanceStationaryLight = 0.f;

	DistanceFieldShadowDistance = 30000.0f;
	TraceDistance = 10000.0f;
	FarShadowDistance = 300000.0f;
	LightSourceAngle = 1;

	DynamicShadowCascades = 3;
	CascadeDistributionExponent = 3.0f;
	CascadeTransitionFraction = 0.1f;
	ShadowDistanceFadeoutFraction = 0.1f;
	IndirectLightingIntensity = 1.0f;
	CastTranslucentShadows = true;
	bUseInsetShadowsForMovableObjects = true;
	bCastVolumetricShadow = true;

	ModulatedShadowColor = FColor(128, 128, 128);
}

#if WITH_EDITOR
/**
 * Called after property has changed via e.g. property window or set command.
 *
 * @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
 */
void UDirectionalLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	LightmassSettings.LightSourceAngle = FMath::Max(LightmassSettings.LightSourceAngle, 0.0f);
	LightmassSettings.IndirectLightingSaturation = FMath::Max(LightmassSettings.IndirectLightingSaturation, 0.0f);
	LightmassSettings.ShadowExponent = FMath::Clamp(LightmassSettings.ShadowExponent, .5f, 8.0f);

	DynamicShadowDistanceMovableLight = FMath::Max(DynamicShadowDistanceMovableLight, 0.0f);
	DynamicShadowDistanceStationaryLight = FMath::Max(DynamicShadowDistanceStationaryLight, 0.0f);

	DynamicShadowCascades = FMath::Clamp(DynamicShadowCascades, 0, 10);
	FarShadowCascadeCount = FMath::Clamp(FarShadowCascadeCount, 0, 10);
	CascadeDistributionExponent = FMath::Clamp(CascadeDistributionExponent, .1f, 10.0f);
	CascadeTransitionFraction = FMath::Clamp(CascadeTransitionFraction, 0.0f, 0.3f);
	ShadowDistanceFadeoutFraction = FMath::Clamp(ShadowDistanceFadeoutFraction, 0.0f, 1.0f);
	// max range is larger than UI
	ShadowBias = FMath::Clamp(ShadowBias, 0.0f, 10.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UDirectionalLightComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();
		
		bool bShadowCascades = CastShadows
			&& CastDynamicShadows 
			&& ((DynamicShadowDistanceMovableLight > 0 && Mobility == EComponentMobility::Movable)
			|| (DynamicShadowDistanceStationaryLight > 0 && Mobility == EComponentMobility::Stationary));

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, bUseInsetShadowsForMovableObjects))
		{
			return CastShadows && CastDynamicShadows && DynamicShadowDistanceStationaryLight > 0 && Mobility == EComponentMobility::Stationary;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DynamicShadowDistanceMovableLight))
		{
			return CastShadows && CastDynamicShadows;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DynamicShadowCascades)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, CascadeDistributionExponent)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, CascadeTransitionFraction)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, ShadowDistanceFadeoutFraction)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, bUseInsetShadowsForMovableObjects)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, FarShadowCascadeCount))
		{
			return bShadowCascades;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, FarShadowDistance))
		{
			return bShadowCascades && FarShadowCascadeCount > 0;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DistanceFieldShadowDistance)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, TraceDistance))
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
			bool bCanEdit = CastShadows && CastDynamicShadows && bUseRayTracedDistanceFieldShadows && Mobility != EComponentMobility::Static && CVar->GetValueOnGameThread() != 0;
			return bCanEdit;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, OcclusionMaskDarkness)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, OcclusionDepthRange))
		{
			return bEnableLightShaftOcclusion;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, bCastModulatedShadows))
		{
			return bUseInsetShadowsForMovableObjects;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, ModulatedShadowColor))
		{
			return bUseInsetShadowsForMovableObjects && bCastModulatedShadows;
		}

	}

	return Super::CanEditChange(InProperty);
}

#endif // WITH_EDITOR

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy() const
{
	return new FDirectionalLightSceneProxy(this);
}

FVector4 UDirectionalLightComponent::GetLightPosition() const
{
	return FVector4(-GetDirection() * WORLD_MAX, 0 );
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType UDirectionalLightComponent::GetLightType() const
{
	return LightType_Directional;
}

float UDirectionalLightComponent::GetUniformPenumbraSize() const
{
	if (LightmassSettings.bUseAreaShadowsForStationaryLight)
	{
		// Interpret distance as shadow factor directly
		return 1.0f;
	}
	else
	{
		// Heuristic to derive uniform penumbra size from light source angle
		return FMath::Clamp(LightmassSettings.LightSourceAngle * .05f, .0001f, 1.0f);
	}
}

void UDirectionalLightComponent::SetDynamicShadowDistanceMovableLight(float NewValue)
{
	if (DynamicShadowDistanceMovableLight != NewValue)
	{
		DynamicShadowDistanceMovableLight = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetDynamicShadowDistanceStationaryLight(float NewValue)
{
	if (DynamicShadowDistanceStationaryLight != NewValue)
	{
		DynamicShadowDistanceStationaryLight = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetDynamicShadowCascades(int32 NewValue)
{
	if (DynamicShadowCascades != NewValue)
	{
		DynamicShadowCascades = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetCascadeDistributionExponent(float NewValue)
{
	if (CascadeDistributionExponent != NewValue)
	{
		CascadeDistributionExponent = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetCascadeTransitionFraction(float NewValue)
{
	if (CascadeTransitionFraction != NewValue)
	{
		CascadeTransitionFraction = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetShadowDistanceFadeoutFraction(float NewValue)
{
	if (ShadowDistanceFadeoutFraction != NewValue)
	{
		ShadowDistanceFadeoutFraction = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetEnableLightShaftOcclusion(bool bNewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& bEnableLightShaftOcclusion != bNewValue)
	{
		bEnableLightShaftOcclusion = bNewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetOcclusionMaskDarkness(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& OcclusionMaskDarkness != NewValue)
	{
		OcclusionMaskDarkness = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetLightShaftOverrideDirection(FVector NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& LightShaftOverrideDirection != NewValue)
	{
		LightShaftOverrideDirection = NewValue;
		if (SceneProxy)
		{
			FDirectionalLightSceneProxy* DirectionalLightSceneProxy = (FDirectionalLightSceneProxy*)SceneProxy;
			DirectionalLightSceneProxy->UpdateLightShaftOverrideDirection_GameThread(this);
		}
	}
}

void UDirectionalLightComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.UE4Ver() < VER_UE4_REMOVE_LIGHT_MOBILITY_CLASSES)
	{
		// If outer is a DirectionalLight, we use the ADirectionalLight::LoadedFromAnotherClass path
		if( GetOuter() != NULL && !GetOuter()->IsA(ADirectionalLight::StaticClass()) )
		{
			if(Mobility == EComponentMobility::Movable)
			{
				DynamicShadowDistanceMovableLight = WholeSceneDynamicShadowRadius_DEPRECATED; 
			}
			else if(Mobility == EComponentMobility::Stationary)
			{
				DynamicShadowDistanceStationaryLight = WholeSceneDynamicShadowRadius_DEPRECATED;
			}
		}
	}
}

void UDirectionalLightComponent::InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly)
{
	// Directional lights don't care about translation
	if (!bTranslationOnly)
	{
		Super::InvalidateLightingCacheDetailed(bInvalidateBuildEnqueuedLighting, bTranslationOnly);
	}
}
