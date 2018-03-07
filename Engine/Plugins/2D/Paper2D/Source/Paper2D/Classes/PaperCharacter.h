// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Character.h"
#include "PaperCharacter.generated.h"

// APaperCharacter behaves like ACharacter, but uses a UPaperFlipbookComponent instead of a USkeletalMeshComponent as a visual representation
// Note: The variable named Mesh will not be set up on this actor!
UCLASS()
class PAPER2D_API APaperCharacter : public ACharacter
{
	GENERATED_UCLASS_BODY()
	
	// Name of the Sprite component
	static FName SpriteComponentName;

private:
	/** The main skeletal mesh associated with this Character (optional sub-object). */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UPaperFlipbookComponent* Sprite;
public:

	virtual void PostInitializeComponents() override;

	/** Returns Sprite subobject **/
	FORCEINLINE class UPaperFlipbookComponent* GetSprite() const { return Sprite; }
};
