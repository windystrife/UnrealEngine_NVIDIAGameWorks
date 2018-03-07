// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AndroidPermissionFunctionLibrary.generated.h"

UCLASS()
class ANDROIDPERMISSION_API UAndroidPermissionFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** check if the permission is already granted */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Check Android Permission"), Category="AndroidPermission")
	static bool CheckPermission(const FString& permission);

	/** try to acquire permissions and return a singleton callback proxy object containing OnPermissionsGranted delegate */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Request Android Permissions"), Category="AndroidPermission")
	static UAndroidPermissionCallbackProxy* AcquirePermissions(const TArray<FString>& permissions);

public:
	/** initialize java objects and cache them for further usage. called when the module is loaded */
	static void Initialize();
};
