// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "ReflectionCapture.generated.h"

class UBillboardComponent;

UCLASS(abstract, hidecategories=(Collision, Attachment, Actor), MinimalAPI)
class AReflectionCapture : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** Reflection capture component. */
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UReflectionCaptureComponent* CaptureComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* SpriteComponent;

	UPROPERTY()
	UBillboardComponent* CaptureOffsetComponent;
#endif // WITH_EDITORONLY_DATA

public:	

	virtual bool IsLevelBoundsRelevant() const override { return false; }

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif // WITH_EDITOR

	/** Returns CaptureComponent subobject **/
	ENGINE_API class UReflectionCaptureComponent* GetCaptureComponent() const { return CaptureComponent; }
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
	ENGINE_API UBillboardComponent* GetCaptureOffsetComponent() const { return CaptureOffsetComponent; }
#endif
};



