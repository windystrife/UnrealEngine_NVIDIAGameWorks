// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PointLightSceneProxy.h: Point light scene info definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Components/PointLightComponent.h"
#include "SceneManagement.h"

/** The parts of the point light scene info that aren't dependent on the light policy type. */
class FPointLightSceneProxyBase : public FLightSceneProxy
{
public:

	/** The light radius. */
	float Radius;

	/** One over the light's radius. */
	float InvRadius;

	/** The light falloff exponent. */
	float FalloffExponent;

	/** Radius of light source shape */
	float SourceRadius;

	/** Soft radius of light source shape */
	float SoftSourceRadius;

	/** Length of light source shape */
	float SourceLength;

	/** Whether light uses inverse squared falloff. */
	const uint32 bInverseSquared : 1;

	/** Initialization constructor. */
	FPointLightSceneProxyBase(const UPointLightComponent* Component)
	:	FLightSceneProxy(Component)
	,	FalloffExponent(Component->LightFalloffExponent)
	,	SourceRadius(Component->SourceRadius)
	,	SoftSourceRadius(Component->SoftSourceRadius)
	,	SourceLength(Component->SourceLength)
	,	bInverseSquared(Component->bUseInverseSquaredFalloff)
	,	MaxDrawDistance(Component->MaxDrawDistance)
	,	FadeRange(Component->MaxDistanceFadeRange)
	{
		UpdateRadius(Component->AttenuationRadius);
	}

	/**
	* Called on the light scene info after it has been passed to the rendering thread to update the rendering thread's cached info when
	* the light's radius changes.
	*/
	void UpdateRadius_GameThread(UPointLightComponent* Component);

	// FLightSceneInfo interface.
	virtual float GetMaxDrawDistance() const final override 
	{ 
		return MaxDrawDistance; 
	}

	virtual float GetFadeRange() const final override 
	{ 
		return FadeRange; 
	}

	/** @return radius of the light or 0 if no radius */
	virtual float GetRadius() const override
	{ 
		return Radius; 
	}

	virtual float GetSourceRadius() const override
	{ 
		return SourceRadius; 
	}

	virtual bool IsInverseSquared() const override
	{
		return bInverseSquared;
	}

	virtual bool AffectsBounds(const FBoxSphereBounds& Bounds) const override
	{
		if((Bounds.Origin - GetLightToWorld().GetOrigin()).SizeSquared() > FMath::Square(Radius + Bounds.SphereRadius))
		{
			return false;
		}

		if(!FLightSceneProxy::AffectsBounds(Bounds))
		{
			return false;
		}

		return true;
	}

	virtual bool GetScissorRect(FIntRect& ScissorRect, const FSceneView& View) const override
	{
		ScissorRect = View.ViewRect;
		return FMath::ComputeProjectedSphereScissorRect(ScissorRect, GetLightToWorld().GetOrigin(), Radius, View.ViewMatrices.GetViewOrigin(), View.ViewMatrices.GetViewMatrix(), View.ViewMatrices.GetProjectionMatrix()) == 1;
	}

	virtual void SetScissorRect(FRHICommandList& RHICmdList, const FSceneView& View) const override
	{
		FIntRect ScissorRect;

		if (FPointLightSceneProxyBase::GetScissorRect(ScissorRect, View))
		{
			RHICmdList.SetScissorRect(true, ScissorRect.Min.X, ScissorRect.Min.Y, ScissorRect.Max.X, ScissorRect.Max.Y);
		}
		else
		{
			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		}
	}

	virtual FVector GetPerObjectProjectedShadowProjectionPoint(const FBoxSphereBounds& SubjectBounds) const
	{
		return FMath::ClosestPointOnSegment(SubjectBounds.Origin, GetOrigin() - GetDirection() * SourceLength / 2, GetOrigin() + GetDirection() * SourceLength / 2);
	}

	virtual bool GetPerObjectProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,class FPerObjectProjectedShadowInitializer& OutInitializer) const override
	{
		// Use a perspective projection looking at the primitive from the light position.
		FVector LightPosition = GetPerObjectProjectedShadowProjectionPoint(SubjectBounds);
		FVector LightVector = SubjectBounds.Origin - LightPosition;
		float LightDistance = LightVector.Size();
		float SilhouetteRadius = 1.0f;
		const float SubjectRadius = SubjectBounds.BoxExtent.Size();
		const float ShadowRadiusMultiplier = 1.1f;

		if (LightDistance > SubjectRadius)
		{
			SilhouetteRadius = FMath::Min(SubjectRadius * FMath::InvSqrt((LightDistance - SubjectRadius) * (LightDistance + SubjectRadius)), 1.0f);
		}

		if (LightDistance <= SubjectRadius * ShadowRadiusMultiplier)
		{
			// Make the primitive fit in a single < 90 degree FOV projection.
			LightVector = SubjectRadius * LightVector.GetSafeNormal() * ShadowRadiusMultiplier;
			LightPosition = (SubjectBounds.Origin - LightVector );
			LightDistance = SubjectRadius * ShadowRadiusMultiplier;
			SilhouetteRadius = 1.0f;
		}

		OutInitializer.PreShadowTranslation = -LightPosition;
		OutInitializer.WorldToLight = FInverseRotationMatrix((LightVector / LightDistance).Rotation());
		OutInitializer.Scales = FVector(1.0f,1.0f / SilhouetteRadius,1.0f / SilhouetteRadius);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds(SubjectBounds.Origin - LightPosition,SubjectBounds.BoxExtent,SubjectBounds.SphereRadius);
		OutInitializer.WAxis = FVector4(0,0,1,0);
		OutInitializer.MinLightW = 0.1f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		return true;
	}

private:

	/** Updates the light scene info's radius from the component. */
	void UpdateRadius(float ComponentRadius)
	{
		Radius = ComponentRadius;

		// Min to avoid div by 0 (NaN in InvRadius)
		InvRadius = 1.0f / FMath::Max(0.00001f, ComponentRadius);
	}

	float MaxDrawDistance;
	float FadeRange;
};

/**
 * The scene info for a point light.
 */
template<typename LightPolicyType>
class TPointLightSceneProxy : public FPointLightSceneProxyBase
{
public:

	/** Initialization constructor. */
	TPointLightSceneProxy(const UPointLightComponent* Component)
		:	FPointLightSceneProxyBase(Component)
	{
	}
};

