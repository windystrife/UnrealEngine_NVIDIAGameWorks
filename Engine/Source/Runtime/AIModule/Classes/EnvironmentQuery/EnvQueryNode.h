// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvQueryNode.generated.h"

struct FPropertyChangedEvent;

UCLASS(Abstract)
class AIMODULE_API UEnvQueryNode : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Versioning for updating deprecated properties */
	UPROPERTY()
	int32 VerNum;

	virtual void UpdateNodeVersion();

	virtual FText GetDescriptionTitle() const;
	virtual FText GetDescriptionDetails() const;

#if WITH_EDITOR && USE_EQS_DEBUGGER
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR && USE_EQS_DEBUGGER
};
