// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Proxy class for display skeleton notifies in the details panel
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EditorSkeletonNotifyObj.generated.h"

class UAnimSequenceBase;

DECLARE_DELEGATE_TwoParams( FOnAnimObjectChange, class UObject*, bool)

UCLASS(MinimalAPI)
class UEditorSkeletonNotifyObj : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	
	UObject* EditedObject;

	UPROPERTY(VisibleAnywhere, Category=SkeletonNotifies)
	FName Name;

	TArray< TSharedPtr<FString> > AnimationNames;
};
