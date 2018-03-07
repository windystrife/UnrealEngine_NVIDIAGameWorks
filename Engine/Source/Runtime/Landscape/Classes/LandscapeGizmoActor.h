// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "LandscapeGizmoActor.generated.h"

class UBillboardComponent;

UCLASS(notplaceable, MinimalAPI, NotBlueprintable)
class ALandscapeGizmoActor : public AActor
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Gizmo)
	float Width;

	UPROPERTY(EditAnywhere, Category=Gizmo)
	float Height;

	UPROPERTY(EditAnywhere, Category=Gizmo)
	float LengthZ;

	UPROPERTY(EditAnywhere, Category=Gizmo)
	float MarginZ;

	UPROPERTY(EditAnywhere, Category=Gizmo)
	float MinRelativeZ;

	UPROPERTY(EditAnywhere, Category=Gizmo)
	float RelativeScaleZ;

	UPROPERTY(EditAnywhere, transient, Category=Gizmo)
	class ULandscapeInfo* TargetLandscapeInfo;

private:
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
#endif // WITH_EDITORONLY_DATA
public:


#if WITH_EDITOR
	virtual void Duplicate(ALandscapeGizmoActor* Gizmo); 
	//virtual void EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown);

	bool EditorCanAttachTo(const AActor* InParent, FText& OutReason) const override
	{
		return false;
	}
#endif

	/** 
	 * Indicates whether this actor should participate in level bounds calculations
	 */
	virtual bool IsLevelBoundsRelevant() const override { return false; }

public:
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	LANDSCAPE_API UBillboardComponent* GetSpriteComponent() const;
#endif
};



