// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "PaperGroupedSpriteActor.generated.h"

class UPaperGroupedSpriteComponent;

/**
 * A group of sprites that will be rendered and culled as a single unit
 *
 * This actor is created when you Merge several sprite components together.
 * it is just a thin wrapper around a UPaperGroupedSpriteComponent.
 */
UCLASS(ComponentWrapperClass)
class PAPER2D_API APaperGroupedSpriteActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category=Sprite, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories = "Sprite,Rendering,Physics,Components|Sprite", AllowPrivateAccess="true"))
	UPaperGroupedSpriteComponent* RenderComponent;

public:
	// AActor interface
#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
	// End of AActor interface

	/** Returns RenderComponent subobject **/
	FORCEINLINE UPaperGroupedSpriteComponent* GetRenderComponent() const { return RenderComponent; }
};
