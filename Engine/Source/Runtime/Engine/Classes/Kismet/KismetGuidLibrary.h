// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KismetGuidLibrary.generated.h"


UCLASS()
class ENGINE_API UKismetGuidLibrary
	: public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (Guid)", CompactNodeTitle="=="), Category="Guid")
	static bool EqualEqual_GuidGuid( const FGuid& A, const FGuid& B );
	
	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="NotEqual (Guid)", CompactNodeTitle="!="), Category="Guid")
	static bool NotEqual_GuidGuid( const FGuid& A, const FGuid& B );

	/** Checks whether the given GUID is valid */
	UFUNCTION(BlueprintPure, meta=(DisplayName="IsValid (Guid)", CompactNodeTitle="isValid?"), Category="Guid")
	static bool IsValid_Guid( const FGuid& InGuid );

	/** Invalidates the given GUID */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Invalidate_Guid (Guid)"), Category="Guid")
	static void Invalidate_Guid( UPARAM(ref) FGuid& InGuid );

	/** Returns a new unique GUID */
	UFUNCTION(BlueprintPure, Category="Guid")
	static FGuid NewGuid();
	
	/** Converts a GUID value to a string, in the form 'A-B-C-D' */
	UFUNCTION(BlueprintPure, meta=(DisplayName="ToString (Guid)", CompactNodeTitle="->"), Category="Guid")
	static FString Conv_GuidToString( const FGuid& InGuid );

	/** Converts a String of format EGuidFormats to a Guid. Returns Guid OutGuid, Returns bool Success */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Parse String to Guid"), Category="Guid")
	static void Parse_StringToGuid( const FString& GuidString, FGuid& OutGuid, bool& Success );
};
