// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTaskResource.h"
#include "AIResources.generated.h"

UCLASS(meta = (DisplayName = "AI Movement"))
class AIMODULE_API UAIResource_Movement : public UGameplayTaskResource
{
	GENERATED_BODY()

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual FString GenerateDebugDescription() const override;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
};

UCLASS(meta = (DisplayName = "AI Logic"))
class AIMODULE_API UAIResource_Logic : public UGameplayTaskResource
{
	GENERATED_BODY()
};
