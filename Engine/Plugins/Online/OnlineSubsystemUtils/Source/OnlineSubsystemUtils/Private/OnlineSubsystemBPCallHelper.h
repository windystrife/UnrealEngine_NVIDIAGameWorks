// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"

class APlayerController;
class IOnlineSubsystem;

// Helper class for various methods to reduce the call hierarchy
struct FOnlineSubsystemBPCallHelper
{
public:
	FOnlineSubsystemBPCallHelper(const TCHAR* CallFunctionContext, UObject* WorldContextObject, FName SystemName = NAME_None);

	void QueryIDFromPlayerController(APlayerController* PlayerController);

	bool IsValid() const
	{
		return UserID.IsValid() && (OnlineSub != nullptr);
	}

public:
	TSharedPtr<const FUniqueNetId> UserID;
	IOnlineSubsystem* const OnlineSub;
	const TCHAR* FunctionContext;
};

#define INVALID_INDEX -1



