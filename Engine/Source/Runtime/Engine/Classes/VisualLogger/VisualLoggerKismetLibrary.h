// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VisualLoggerKismetLibrary.generated.h"

UCLASS(MinimalAPI)
class UVisualLoggerKismetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Logs simple text string with Visual Logger - recording for Visual Logs has to be enabled to record this data */
	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", DisplayName = "LogText"))
	static void LogText(UObject* WorldContextObject, FString Text, FName LogCategory = TEXT("Blueprint Log"));

	/** Logs location as sphere with given radius - recording for Visual Logs has to be enabled to record this data */
	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", DisplayName = "LogLocation"))
	static void LogLocation(UObject* WorldContextObject, FVector Location, FString Text, FLinearColor ObjectColor = FLinearColor::White, float Radius = 10, FName LogCategory = TEXT("Blueprint Log"));

	/** Logs box shape - recording for Visual Logs has to be enabled to record this data */
	UFUNCTION(BlueprintCallable, Category = "Debug|VisualLogger", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", DisplayName = "Log Box Shape"))
	static void LogBox(UObject* WorldContextObject, FBox BoxShape, FString Text, FLinearColor ObjectColor = FLinearColor::White, FName LogCategory = TEXT("Blueprint Log"));
};
