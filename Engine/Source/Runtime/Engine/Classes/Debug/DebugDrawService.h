// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ShowFlags.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugDrawService.generated.h"

class FCanvas;
class FSceneView;

/** 
 * 
 */
DECLARE_DELEGATE_TwoParams(FDebugDrawDelegate, class UCanvas*, class APlayerController*);

UCLASS(config=Engine)
class ENGINE_API UDebugDrawService : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//void Register

	static FDelegateHandle Register(const TCHAR* Name, const FDebugDrawDelegate& NewDelegate);
	static void Unregister(FDelegateHandle HandleToRemove);
	static void Draw(const FEngineShowFlags Flags, class UCanvas* Canvas);
	static void Draw(const FEngineShowFlags Flags, class FViewport* Viewport, FSceneView* View, FCanvas* Canvas);

private:
	static TArray<TArray<FDebugDrawDelegate> > Delegates;
	static FEngineShowFlags ObservedFlags;
};
