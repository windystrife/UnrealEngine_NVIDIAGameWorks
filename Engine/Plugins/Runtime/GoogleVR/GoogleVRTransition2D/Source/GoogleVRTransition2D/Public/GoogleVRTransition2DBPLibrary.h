// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
//#include "Engine/EngineTypes.h"
#include "GoogleVRTransition2DBPLibrary.generated.h"

UCLASS()
class UGoogleVRTransition2DBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//transition to 2D with the visual guidance. a black 2D screen will be displayed after transition
	//this function returns a singleton callback proxy object to handle OnTransition2D delegate
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transition to 2D"), Category = "Google VR")
	static UGoogleVRTransition2DCallbackProxy* TransitionTo2D();

	//transition back from 2D to VR. will see Back to VR button which make you resume to game by clicking it
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Transition back to VR"), Category = "Google VR")
	static void TransitionToVR();

public:
	//intitialize and caches java classes and methods when the module is loaded
	static void Initialize();
};
