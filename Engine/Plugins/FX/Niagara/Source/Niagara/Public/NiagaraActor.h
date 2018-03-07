// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "NiagaraActor.generated.h"

UCLASS(MinimalAPI)
class ANiagaraActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** Pointer to System component */
	UPROPERTY(VisibleAnywhere, Category=NiagaraActor)
	class UNiagaraComponent* NiagaraComponent;

#if WITH_EDITORONLY_DATA
	// Reference to sprite visualization component
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;

	// Reference to arrow visualization component
	UPROPERTY()
	class UArrowComponent* ArrowComponent;

#endif

public:
	/** Returns NiagaraComponent subobject **/
	NIAGARA_API class UNiagaraComponent* GetNiagaraComponent() const { return NiagaraComponent; }
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	class UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
	/** Returns ArrowComponent subobject **/
	class UArrowComponent* GetArrowComponent() const { return ArrowComponent; }
#endif

#if WITH_EDITOR
	// AActor interface
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	// End of AActor interface
#endif // WITH_EDITOR

};
