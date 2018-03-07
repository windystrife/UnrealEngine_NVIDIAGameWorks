// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitBlueprintLibrary.h"
#include "AppleARKitModule.h"


bool UAppleARKitBlueprintLibrary::GetCurrentFrame( UObject* WorldContextObject, FAppleARKitFrame& OutCurrentFrame )
{
	auto System = FAppleARKitModule::GetARKitSystem();
	if (System.IsValid())
	{
		return System->GetCurrentFrame(OutCurrentFrame);
	}
	return false;
}
	
bool UAppleARKitBlueprintLibrary::HitTestAtScreenPosition_TrackingSpace( UObject* WorldContextObject, const FVector2D ScreenPosition, EAppleARKitHitTestResultType Types, TArray< FAppleARKitHitTestResult >& OutResults )
{
	auto System = FAppleARKitModule::GetARKitSystem();
	if (System.IsValid())
	{
		return System->HitTestAtScreenPosition(ScreenPosition, Types, OutResults);
	}
	return false;
}

