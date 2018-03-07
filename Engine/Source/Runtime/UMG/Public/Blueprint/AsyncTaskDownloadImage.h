// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Interfaces/IHttpRequest.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "AsyncTaskDownloadImage.generated.h"

class UTexture2DDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDownloadImageDelegate, UTexture2DDynamic*, Texture);

UCLASS()
class UMG_API UAsyncTaskDownloadImage : public UBlueprintAsyncActionBase
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=( BlueprintInternalUseOnly="true" ))
	static UAsyncTaskDownloadImage* DownloadImage(FString URL);

public:

	UPROPERTY(BlueprintAssignable)
	FDownloadImageDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FDownloadImageDelegate OnFail;

public:

	void Start(FString URL);

private:

	/** Handles image requests coming from the web */
	void HandleImageRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
};
