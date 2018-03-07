// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "CodeProjectItem.generated.h"

/** Types of project items. Note that the enum ordering determines the tree sorting */
UENUM()
namespace ECodeProjectItemType
{
	enum Type
	{
		Project,
		Folder,
		File
	};
}

UCLASS()
class UCodeProjectItem : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void RescanChildren();

	void HandleDirectoryScanned(const FString& InPathName, ECodeProjectItemType::Type InType);

	/** Handle directory changing */
	void HandleDirectoryChanged(const TArray<struct FFileChangeData>& FileChanges);

public:
	UPROPERTY(Transient)
	TEnumAsByte<ECodeProjectItemType::Type> Type;

	UPROPERTY(Transient)
	FString Name;

	UPROPERTY(Transient)
	FString Extension;

	UPROPERTY(Transient)
	FString Path;

	UPROPERTY(Transient)
	TArray<UCodeProjectItem*> Children;

	/** Delegate handle for directory watcher */
	FDelegateHandle OnDirectoryChangedHandle;
};
