// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * A sticky note.  Level designers can place these in the level and then
 * view them as a batch in the error/warnings window.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Note.generated.h"

UCLASS(MinimalAPI, hidecategories = (Input), showcategories=("Input|MouseInput", "Input|TouchInput"))
class ANote : public AActor
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Note, meta = (MultiLine = "true"))
	FString Text;

	// Reference to sprite visualization component
private:
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;

	// Reference to arrow visualization component
	UPROPERTY()
	class UArrowComponent* ArrowComponent;
public:

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	//~ Begin AActor Interface
	virtual void CheckForErrors() override;
	//~ End AActor Interface
#endif

#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API class UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
	/** Returns ArrowComponent subobject **/
	ENGINE_API class UArrowComponent* GetArrowComponent() const { return ArrowComponent; }
#endif
};



