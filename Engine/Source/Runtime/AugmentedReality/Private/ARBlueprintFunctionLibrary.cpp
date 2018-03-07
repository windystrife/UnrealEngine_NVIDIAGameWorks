// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ARBlueprintFunctionLibrary.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "Engine/Engine.h"

//bool UARBlueprintFunctionLibrary::ARLineTraceFromScreenPoint(UObject* WorldContextObject, const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults)
//{
//	TArray<IARHitTestingSupport*> Providers = IModularFeatures::Get().GetModularFeatureImplementations<IARHitTestingSupport>(IARHitTestingSupport::GetModularFeatureName());
//	const int32 NumProviders = Providers.Num();
//	ensureMsgf(NumProviders <= 1, TEXT("Expected at most one ARHitTestingSupport provider, but there are %d registered. Using the first."), NumProviders);
//	if ( ensureMsgf(NumProviders > 0, TEXT("Expected at least one ARHitTestingSupport provider.")) )
//	{
//		if (const bool bHit = Providers[0]->ARLineTraceFromScreenPoint(ScreenPosition, OutHitResults))
//		{
//			// Update transform from ARKit (camera) space to UE World Space
//			
//			UWorld* MyWorld = WorldContextObject->GetWorld();
//			APlayerController* MyPC = MyWorld != nullptr ? MyWorld->GetFirstPlayerController() : nullptr;
//			APawn* MyPawn = MyPC != nullptr ? MyPC->GetPawn() : nullptr;
//			
//			if (MyPawn != nullptr)
//			{
//				const FTransform PawnTransform = MyPawn->GetActorTransform();
//				for ( FARHitTestResult& HitResult : OutHitResults )
//				{
//					HitResult.Transform *= PawnTransform;
//				}
//				return true;
//			}
//		}
//	}
//
//	return false;
//}


EARTrackingQuality UARBlueprintFunctionLibrary::GetTrackingQuality( UObject* WorldContextObject )
{
	TArray<IARTrackingQuality*> Providers =
		IModularFeatures::Get().GetModularFeatureImplementations<IARTrackingQuality>(IARTrackingQuality::GetModularFeatureName());
	const int32 NumProviders = Providers.Num();
	ensureMsgf(NumProviders <= 1, TEXT("Expected at most one ARHitTestingSupport provider, but there are %d registered. Using the first."), NumProviders);
	if ( ensureMsgf(NumProviders > 0, TEXT("Expected at least one TrackingQuality provider.")) )
	{
		return Providers[0]->ARGetTrackingQuality();
	}
	
	return EARTrackingQuality::NotAvailable;
}
