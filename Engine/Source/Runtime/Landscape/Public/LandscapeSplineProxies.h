// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "HitProxies.h"

class ULandscapeSplineSegment;

//////////////////////////////////////////////////////////////////////////
// LANDSCAPE SPLINES HIT PROXY

struct HLandscapeSplineProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( LANDSCAPE_API );

	HLandscapeSplineProxy(EHitProxyPriority InPriority = HPP_Wireframe) :
		HHitProxy(InPriority)
	{
	}
	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

struct HLandscapeSplineProxy_Segment : public HLandscapeSplineProxy
{
	DECLARE_HIT_PROXY( LANDSCAPE_API );

	class ULandscapeSplineSegment* SplineSegment;

	HLandscapeSplineProxy_Segment(class ULandscapeSplineSegment* InSplineSegment) :
		HLandscapeSplineProxy(),
		SplineSegment(InSplineSegment)
	{
	}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( SplineSegment );
	}
};

struct HLandscapeSplineProxy_ControlPoint : public HLandscapeSplineProxy
{
	DECLARE_HIT_PROXY( LANDSCAPE_API );

	class ULandscapeSplineControlPoint* ControlPoint;

	HLandscapeSplineProxy_ControlPoint(class ULandscapeSplineControlPoint* InControlPoint) :
		HLandscapeSplineProxy(HPP_Foreground),
		ControlPoint(InControlPoint)
	{
	}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( ControlPoint );
	}
};

struct HLandscapeSplineProxy_Tangent : public HLandscapeSplineProxy
{
	DECLARE_HIT_PROXY( LANDSCAPE_API );

	ULandscapeSplineSegment* SplineSegment;
	uint32 End:1;

	HLandscapeSplineProxy_Tangent(class ULandscapeSplineSegment* InSplineSegment, bool InEnd) :
		HLandscapeSplineProxy(HPP_UI),
		SplineSegment(InSplineSegment),
		End(InEnd)
	{
	}
	LANDSCAPE_API virtual void Serialize(FArchive& Ar);

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( SplineSegment );
	}
};
