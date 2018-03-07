// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "AnimNotifyState.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;
struct FAnimNotifyEvent;
struct FBranchingPointNotifyPayload;

UCLASS(abstract, editinlinenew, Blueprintable, const, hidecategories=Object, collapsecategories, meta=(ShowWorldContextPin))
class ENGINE_API UAnimNotifyState : public UObject
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Implementable event to get a custom name for the notify
	 */
	UFUNCTION(BlueprintNativeEvent)
	FString GetNotifyName() const;

	UFUNCTION(BlueprintImplementableEvent)
	bool Received_NotifyBegin(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float TotalDuration) const;
	
	UFUNCTION(BlueprintImplementableEvent)
	bool Received_NotifyTick(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float FrameDeltaTime) const;

	UFUNCTION(BlueprintImplementableEvent)
	bool Received_NotifyEnd(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation) const;

#if WITH_EDITORONLY_DATA
	/** Color of Notify in editor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimNotify)
	FColor NotifyColor;

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	virtual void OnAnimNotifyCreatedInEditor(FAnimNotifyEvent& ContainingAnimNotifyEvent) {};
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const { return true; }
#endif

	virtual void NotifyBegin(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float TotalDuration);
	virtual void NotifyTick(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float FrameDeltaTime);
	virtual void NotifyEnd(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation);

	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload);
	virtual void BranchingPointNotifyTick(FBranchingPointNotifyPayload& BranchingPointPayload, float FrameDeltaTime);
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload);

	// @todo document 
	virtual FString GetEditorComment() 
	{ 
		return TEXT(""); 
	}

	// @todo document 
	virtual FLinearColor GetEditorColor() 
	{ 
#if WITH_EDITORONLY_DATA
		return FLinearColor(NotifyColor); 
#else
		return FLinearColor::Black;
#endif // WITH_EDITORONLY_DATA
	}

	/** UObject Interface */
	virtual void PostLoad();
	/** End UObject Interface */

	/** This notify is always a branching point when used on Montages. */
	bool bIsNativeBranchingPoint;
};



