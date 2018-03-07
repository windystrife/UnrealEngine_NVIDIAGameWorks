// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "Object.h"
#include "UObject/ObjectMacros.h" 
#include "UObject/ScriptMacros.h"
#include "Delegates/Delegate.h"
#include "AndroidPermissionCallbackProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAndroidPermissionDynamicDelegate, const TArray<FString>&, Permissions, const TArray<bool>&, GrantResults);
DECLARE_DELEGATE_TwoParams(FAndroidPermissionDelegate, const TArray<FString>& /*Permissions*/, const TArray<bool>& /*GrantResults*/);


UCLASS()
class ANDROIDPERMISSION_API UAndroidPermissionCallbackProxy : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable, Category="AndroidPermission")
	FAndroidPermissionDynamicDelegate OnPermissionsGrantedDynamicDelegate;

	FAndroidPermissionDelegate OnPermissionsGrantedDelegate;
	
	static UAndroidPermissionCallbackProxy *GetInstance();
};
