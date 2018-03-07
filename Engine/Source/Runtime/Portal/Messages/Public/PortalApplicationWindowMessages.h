// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RpcMessage.h"
#include "PortalApplicationWindowMessages.generated.h"

USTRUCT()
struct FPortalApplicationWindowNavigateToRequest
	: public FRpcMessage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category="Message")
	FString Url;

	FPortalApplicationWindowNavigateToRequest() { }
	FPortalApplicationWindowNavigateToRequest(const FString& InUrl)
		: Url(InUrl)
	{ }
};


USTRUCT()
struct FPortalApplicationWindowNavigateToResponse
	: public FRpcMessage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category="Message")
	bool Result;

	FPortalApplicationWindowNavigateToResponse() { }
	FPortalApplicationWindowNavigateToResponse(bool InResult)
		: Result(InResult)
	{ }
};


DECLARE_RPC(FPortalApplicationWindowNavigateTo, bool)
