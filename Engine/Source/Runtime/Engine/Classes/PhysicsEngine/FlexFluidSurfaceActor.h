// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "FlexFluidSurfaceActor.generated.h"

UCLASS(notplaceable, transient, MinimalAPI)
class AFlexFluidSurfaceActor : public AInfo
{
	GENERATED_UCLASS_BODY()

private:
	/** @todo document */
	DEPRECATED_FORGAME(4.6, "Component should not be accessed directly, please use GetComponent() function instead. Component will soon be private and your code will not compile.")
	UPROPERTY(Category = FlexFluidSurface, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UFlexFluidSurfaceComponent* Component;
public:

	/** replicated copy of FlexFluidSurfaceComponent's bEnabled property */
	UPROPERTY(replicatedUsing=OnRep_bEnabled)
	uint32 bEnabled:1;

	/** Replication Notification Callbacks */
	UFUNCTION()
	virtual void OnRep_bEnabled();


	//Begin AActor Interface
	virtual void PostInitializeComponents() override;
	virtual void PostActorCreated() override;
	//End AActor Interface

	/** Returns Component subobject **/
	ENGINE_API class UFlexFluidSurfaceComponent* GetComponent() const;
};



