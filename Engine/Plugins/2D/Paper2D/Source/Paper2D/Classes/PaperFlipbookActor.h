// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "PaperFlipbookActor.generated.h"

/**
 * An instance of a UPaperFlipbook in a level.
 *
 * This actor is created when you drag a flipbook asset from the content browser into the level, and
 * it is just a thin wrapper around a UPaperFlipbookComponent that actually references the asset.
 */
UCLASS(ComponentWrapperClass)
class PAPER2D_API APaperFlipbookActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category=Sprite, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories="Sprite,Rendering,Physics,Components|Flipbook", AllowPrivateAccess="true"))
	class UPaperFlipbookComponent* RenderComponent;

public:

	// AActor interface
#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
	// End of AActor interface

	/** Returns RenderComponent subobject **/
	FORCEINLINE class UPaperFlipbookComponent* GetRenderComponent() const { return RenderComponent; }
};

