// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Interface.h"
#include "PreviewCollectionInterface.generated.h"

class USkeletalMesh;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class ENGINE_API UPreviewCollectionInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/** Preview Collection options. If you have native UDataAsset class that implements this, you can preview all in the animation editor using Additional Mesh section  */
class ENGINE_API IPreviewCollectionInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Returns nodes that needs for them to map */
	virtual void GetPreviewSkeletalMeshes(TArray<USkeletalMesh*>& OutList) const = 0;
};