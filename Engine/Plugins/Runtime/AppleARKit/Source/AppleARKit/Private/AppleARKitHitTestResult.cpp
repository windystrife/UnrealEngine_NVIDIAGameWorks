// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// AppleARKit
#include "AppleARKitHitTestResult.h"
#include "AppleARKitTransform.h"

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

/** Conversion function from ARKit native ARHitTestResultType */
EAppleARKitHitTestResultType ToEAppleARKitHitTestResultType(ARHitTestResultType InTypes)
{
	EAppleARKitHitTestResultType Types = EAppleARKitHitTestResultType::None;

	if (!!(InTypes & ARHitTestResultTypeFeaturePoint))
	{
		Types |= EAppleARKitHitTestResultType::FeaturePoint;
	}

	if (!!(InTypes & ARHitTestResultTypeEstimatedHorizontalPlane))
	{
		Types |= EAppleARKitHitTestResultType::EstimatedHorizontalPlane;
	}

	if (!!(InTypes & ARHitTestResultTypeExistingPlane))
	{
		Types |= EAppleARKitHitTestResultType::ExistingPlane;
	}

    if (!!(InTypes & ARHitTestResultTypeExistingPlaneUsingExtent))
    {
        Types |= EAppleARKitHitTestResultType::ExistingPlaneUsingExtent;
    }
    
	return Types;
}

/** Conversion function to ARKit native ARHitTestResultType */
ARHitTestResultType ToARHitTestResultType(EAppleARKitHitTestResultType InTypes)
{
	ARHitTestResultType Types = 0;

	if (!!(InTypes & EAppleARKitHitTestResultType::FeaturePoint))
	{
		Types |= ARHitTestResultTypeFeaturePoint;
	}

	if (!!(InTypes & EAppleARKitHitTestResultType::EstimatedHorizontalPlane))
	{
		Types |= ARHitTestResultTypeEstimatedHorizontalPlane;
	}

	if (!!(InTypes & EAppleARKitHitTestResultType::ExistingPlane))
	{
		Types |= ARHitTestResultTypeExistingPlane;
	}

    if (!!(InTypes & EAppleARKitHitTestResultType::ExistingPlaneUsingExtent))
    {
        Types |= ARHitTestResultTypeExistingPlaneUsingExtent;
    }
    
	return Types;
}

FAppleARKitHitTestResult::FAppleARKitHitTestResult( ARHitTestResult* InARHitTestResult, class UAppleARKitAnchor* InAnchor /*= nullptr*/, float WorldToMetersScale /*= 100.0f*/ )
{
	// Sanity check
	check( InARHitTestResult );

	// Convert properties
	Type = ToEAppleARKitHitTestResultType( InARHitTestResult.type );
    Distance = InARHitTestResult.distance * WorldToMetersScale;
	Transform = FAppleARKitTransform::ToFTransform( InARHitTestResult.worldTransform, WorldToMetersScale );
	Anchor = InAnchor;
}

#endif // #ARKIT_SUPPORT
