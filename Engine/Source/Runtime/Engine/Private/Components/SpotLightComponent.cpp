// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SpotLightComponent.cpp: LightComponent implementation.
=============================================================================*/

#include "Components/SpotLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "PointLightSceneProxy.h"


/**
 * The spot light policy for TMeshLightingDrawingPolicy.
 */
class FSpotLightPolicy
{
public:
	typedef class FSpotLightSceneProxy SceneInfoType;
};

/**
 * The scene info for a spot light.
 */
class FSpotLightSceneProxy : public TPointLightSceneProxy<FSpotLightPolicy>
{
public:

	/** Outer cone angle in radians, clamped to a valid range. */
	float OuterConeAngle;

	/** Cosine of the spot light's inner cone angle. */
	float CosInnerCone;

	/** Cosine of the spot light's outer cone angle. */
	float CosOuterCone;

	/** 1 / (CosInnerCone - CosOuterCone) */
	float InvCosConeDifference;

	/** Sine of the spot light's outer cone angle. */
	float SinOuterCone;

	/** 1 / Tangent of the spot light's outer cone angle. */
	float InvTanOuterCone;

	/** Cosine of the spot light's outer light shaft cone angle. */
	float CosLightShaftConeAngle;

	/** 1 / (appCos(ClampedInnerLightShaftConeAngle) - CosLightShaftConeAngle) */
	float InvCosLightShaftConeDifference;

	/** Initialization constructor. */
	FSpotLightSceneProxy(const USpotLightComponent* Component):
		TPointLightSceneProxy<FSpotLightPolicy>(Component)
	{
		const float ClampedInnerConeAngle = FMath::Clamp(Component->InnerConeAngle,0.0f,89.0f) * (float)PI / 180.0f;
		const float ClampedOuterConeAngle = FMath::Clamp(Component->OuterConeAngle * (float)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)PI / 180.0f + 0.001f);
		OuterConeAngle = ClampedOuterConeAngle;
		CosOuterCone = FMath::Cos(ClampedOuterConeAngle);
		SinOuterCone = FMath::Sin(ClampedOuterConeAngle);
		CosInnerCone = FMath::Cos(ClampedInnerConeAngle);
		InvCosConeDifference = 1.0f / (CosInnerCone - CosOuterCone);
		InvTanOuterCone = 1.0f / FMath::Tan(ClampedOuterConeAngle);
		const float ClampedOuterLightShaftConeAngle = FMath::Clamp(Component->LightShaftConeAngle * (float)PI / 180.0f, 0.001f, 89.0f * (float)PI / 180.0f + 0.001f);
		// Use half the outer light shaft cone angle as the inner angle to provide a nice falloff
		// Not exposing the inner light shaft cone angle as it is probably not needed
		const float ClampedInnerLightShaftConeAngle = .5f * ClampedOuterLightShaftConeAngle;
		CosLightShaftConeAngle = FMath::Cos(ClampedOuterLightShaftConeAngle);
		InvCosLightShaftConeDifference = 1.0f / (FMath::Cos(ClampedInnerLightShaftConeAngle) - CosLightShaftConeAngle);
	}

	virtual FVector GetPerObjectProjectedShadowProjectionPoint(const FBoxSphereBounds& SubjectBounds) const
	{
		const FVector ZAxis(WorldToLight.M[0][2], WorldToLight.M[1][2], WorldToLight.M[2][2]);
		return FMath::ClosestPointOnSegment(SubjectBounds.Origin, GetOrigin() - ZAxis * SourceLength / 2, GetOrigin() + ZAxis * SourceLength / 2);
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FLightParameters& LightParameters) const override
	{
		LightParameters.LightPositionAndInvRadius = FVector4(
			GetOrigin(),
			InvRadius);

		LightParameters.LightColorAndFalloffExponent = FVector4(
			GetColor().R,
			GetColor().G,
			GetColor().B,
			FalloffExponent);

		const FVector ZAxis(WorldToLight.M[0][2], WorldToLight.M[1][2], WorldToLight.M[2][2]);

		LightParameters.NormalizedLightDirection = -GetDirection();
		LightParameters.NormalizedLightTangent = ZAxis;
		LightParameters.SpotAngles = FVector2D(CosOuterCone, InvCosConeDifference);
		LightParameters.LightSourceRadius = SourceRadius;
		LightParameters.LightSoftSourceRadius = SoftSourceRadius;
		LightParameters.LightSourceLength = SourceLength;
		// Prevent 0 Roughness which causes NaNs in Vis_SmithJointApprox
		LightParameters.LightMinRoughness = FMath::Max(MinRoughness, .04f);
	}

	// FLightSceneInfo interface.
	virtual bool AffectsBounds(const FBoxSphereBounds& Bounds) const override
	{
		if(!TPointLightSceneProxy<FSpotLightPolicy>::AffectsBounds(Bounds))
		{
			return false;
		}

		FVector	U = GetOrigin() - (Bounds.SphereRadius / SinOuterCone) * GetDirection(),
				D = Bounds.Origin - U;
		float	dsqr = D | D,
				E = GetDirection() | D;
		if(E > 0.0f && E * E >= dsqr * FMath::Square(CosOuterCone))
		{
			D = Bounds.Origin - GetOrigin();
			dsqr = D | D;
			E = -(GetDirection() | D);
			if(E > 0.0f && E * E >= dsqr * FMath::Square(SinOuterCone))
				return dsqr <= FMath::Square(Bounds.SphereRadius);
			else
				return true;
		}

		return false;
	}
	
	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const override
	{
		FWholeSceneProjectedShadowInitializer& OutInitializer = *new(OutInitializers) FWholeSceneProjectedShadowInitializer;
		OutInitializer.PreShadowTranslation = -GetLightToWorld().GetOrigin();
		OutInitializer.WorldToLight = GetWorldToLight().RemoveTranslation();
		OutInitializer.Scales = FVector(1.0f,InvTanOuterCone,InvTanOuterCone);
		OutInitializer.FaceDirection = FVector(1,0,0);

		const FSphere AbsoluteBoundingSphere = FSpotLightSceneProxy::GetBoundingSphere();
		OutInitializer.SubjectBounds = FBoxSphereBounds(
			AbsoluteBoundingSphere.Center - GetOrigin(),
			FVector(AbsoluteBoundingSphere.W, AbsoluteBoundingSphere.W, AbsoluteBoundingSphere.W),
			AbsoluteBoundingSphere.W
			);

		OutInitializer.WAxis = FVector4(0,0,1,0);
		OutInitializer.MinLightW = 0.1f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		OutInitializer.bRayTracedDistanceField = UseRayTracedDistanceFieldShadows() && DoesPlatformSupportDistanceFieldShadowing(ViewFamily.GetShaderPlatform());
		return true;
	}

	virtual float GetOuterConeAngle() const override { return OuterConeAngle; }

	virtual FVector2D GetLightShaftConeParams() const override { return FVector2D(CosLightShaftConeAngle, InvCosLightShaftConeDifference); }

	virtual FSphere GetBoundingSphere() const override
	{
		// Use the law of cosines to find the distance to the furthest edge of the spotlight cone from a position that is halfway down the spotlight direction
		const float BoundsRadius = FMath::Sqrt(1.25f * Radius * Radius - Radius * Radius * CosOuterCone);
		return FSphere(GetOrigin() + .5f * GetDirection() * Radius, BoundsRadius);
	}

	virtual float GetEffectiveScreenRadius(const FViewMatrices& ShadowViewMatrices) const override
	{
		// Heuristic: use the radius of the inscribed sphere at the cone's end as the light's effective screen radius
		// We do so because we do not want to use the light's radius directly, which will make us overestimate the shadow map resolution greatly for a spot light

		const FVector InscribedSpherePosition = GetOrigin() + GetDirection() * GetRadius();
		const float InscribedSphereRadius = GetRadius() / InvTanOuterCone;

 		const float SphereDistanceFromViewOrigin = (InscribedSpherePosition - ShadowViewMatrices.GetViewOrigin()).Size();

		return ShadowViewMatrices.GetScreenScale() * InscribedSphereRadius / FMath::Max(SphereDistanceFromViewOrigin, 1.0f);
	}
};

USpotLightComponent::USpotLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightSpot"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightSpotMove"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	InnerConeAngle = 0.0f;
	OuterConeAngle = 44.0f;

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	FalloffMode = EFalloffMode::NONE;
	FalloffAngle = 45.0f;
	FalloffPower = 1.0f;
	// NVCHANGE_END: Nvidia Volumetric Lighting

}

void USpotLightComponent::SetInnerConeAngle(float NewInnerConeAngle)
{
	if (AreDynamicDataChangesAllowed(false)
		&& NewInnerConeAngle != InnerConeAngle)
	{
		InnerConeAngle = NewInnerConeAngle;
		MarkRenderStateDirty();
	}
}

void USpotLightComponent::SetOuterConeAngle(float NewOuterConeAngle)
{
	if (AreDynamicDataChangesAllowed(false)
		&& NewOuterConeAngle != OuterConeAngle)
	{
		OuterConeAngle = NewOuterConeAngle;
		MarkRenderStateDirty();
	}
}

// Disable for now
//void USpotLightComponent::SetLightShaftConeAngle(float NewLightShaftConeAngle)
//{
//	if (NewLightShaftConeAngle != LightShaftConeAngle)
//	{
//		LightShaftConeAngle = NewLightShaftConeAngle;
//		MarkRenderStateDirty();
//	}
//}

FLightSceneProxy* USpotLightComponent::CreateSceneProxy() const
{
	return new FSpotLightSceneProxy(this);
}

FSphere USpotLightComponent::GetBoundingSphere() const
{
	float ClampedInnerConeAngle = FMath::Clamp(InnerConeAngle,0.0f,89.0f) * (float)PI / 180.0f;
	float ClampedOuterConeAngle = FMath::Clamp(OuterConeAngle * (float)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)PI / 180.0f + 0.001f);

	float CosOuterCone = FMath::Cos(ClampedOuterConeAngle);

	// Use the law of cosines to find the distance to the furthest edge of the spotlight cone from a position that is halfway down the spotlight direction
	const float BoundsRadius = FMath::Sqrt(1.25f * AttenuationRadius * AttenuationRadius - AttenuationRadius * AttenuationRadius * CosOuterCone);
	return FSphere(GetComponentTransform().GetLocation() + .5f * GetDirection() * AttenuationRadius, BoundsRadius);
}

bool USpotLightComponent::AffectsBounds(const FBoxSphereBounds& InBounds) const
{
	if(!Super::AffectsBounds(InBounds))
	{
		return false;
	}

	float	ClampedInnerConeAngle = FMath::Clamp(InnerConeAngle,0.0f,89.0f) * (float)PI / 180.0f,
			ClampedOuterConeAngle = FMath::Clamp(OuterConeAngle * (float)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)PI / 180.0f + 0.001f);

	float	Sin = FMath::Sin(ClampedOuterConeAngle),
			Cos = FMath::Cos(ClampedOuterConeAngle);

	FVector	U = GetComponentLocation() - (InBounds.SphereRadius / Sin) * GetDirection(),
			D = InBounds.Origin - U;
	float	dsqr = D | D,
			E = GetDirection() | D;
	if(E > 0.0f && E * E >= dsqr * FMath::Square(Cos))
	{
		D = InBounds.Origin - GetComponentLocation();
		dsqr = D | D;
		E = -(GetDirection() | D);
		if(E > 0.0f && E * E >= dsqr * FMath::Square(Sin))
			return dsqr <= FMath::Square(InBounds.SphereRadius);
		else
			return true;
	}

	return false;
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType USpotLightComponent::GetLightType() const
{
	return LightType_Spot;
}

#if WITH_EDITOR

void USpotLightComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == TEXT("InnerConeAngle"))
		{
			OuterConeAngle = FMath::Max(InnerConeAngle, OuterConeAngle);
		}
		else if (PropertyChangedEvent.Property->GetFName() == TEXT("OuterConeAngle"))
		{
			InnerConeAngle = FMath::Min(InnerConeAngle, OuterConeAngle);
		}
	}

	UPointLightComponent::PostEditChangeProperty(PropertyChangedEvent);
}

// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
bool USpotLightComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USpotLightComponent, FalloffMode)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USpotLightComponent, FalloffAngle)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USpotLightComponent, FalloffPower))
		{
			return bEnableVolumetricLighting;
		}
	}

	return UPointLightComponent::CanEditChange(InProperty);
}
// NVCHANGE_END: Nvidia Volumetric Lighting

#endif	// WITH_EDITOR
