// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQueryGraphNode.h"
#include "EnvironmentQueryGraphNode_Root.generated.h"

UCLASS()
class UEnvironmentQueryGraphNode_Root : public UEnvironmentQueryGraphNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Debug)
	TArray<FString> DebugMessages;

	UPROPERTY()
	bool bHasDebugError;

	void LogDebugMessage(const FString& Message);
	void LogDebugError(const FString& Message);

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool HasErrors() const override { return false; }
};
