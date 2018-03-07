// Copyright 2017 Google Inc.

#pragma once

#include "Components/ActorComponent.h"
#include "GoogleARCoreAnchor.h"
#include "GameFramework/Actor.h"

#include "GoogleARCoreAnchorActor.generated.h"

/**
 * An Actor that updates its transform using a GoogleARAnchor object.
 */
UCLASS(Blueprintable, BlueprintType)
class GOOGLEARCOREBASE_API AGoogleARCoreAnchorActor : public AActor
{
	GENERATED_BODY()
public:

	/**
	 * If set to true, this Actor will use the ARAnchor object's latest
	 * pose to update its transform.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARAnchor")
	bool bTrackingEnabled = true;

	/** If set to true, the Actor will be hidden when the ARAnchor isn't currently tracked.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARAnchor")
	bool bHideWhenNotCurrentlyTracking = true;

	/** If set to true, the Actor will be destroyed when the ARAnchor stops tracking completely.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARAnchor")
	bool bDestroyWhenStoppedTracking = true;

	/**
	 * If set to true, this Actor will remove the ARCoreAnchor object from the current tracking
	 * session when the Actor gets destroyed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARAnchor")
	bool bRemoveAnchorObjectWhenDestroyed = true;

	/**
	 * When set to true, if SetARAnchor() is called and a previous anchor had already been set, the
	 * previous anchor will be removed from the current tracking session.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARAnchor")
	bool bRemoveAnchorObjectWhenAnchorChanged = true;

	/** Connects an ARCoreAnchor object to this Actor. */
	UFUNCTION(BlueprintCallable, Category = "GoogleARAnchor")
	void SetARAnchor(UGoogleARCoreAnchorBase* InARAnchorObject);

	/** Returns the ARCoreAnchor object that is connected with this Actor. */
	UFUNCTION(BlueprintPure, Category = "GoogleARAnchor")
	UGoogleARCoreAnchorBase* GetARAnchor();

	/** Tick function on this Actor.*/
	virtual void Tick(float DeltaSeconds) override;

	/** Called before destroying the object.*/
	virtual void BeginDestroy() override;

private:
	/** Pointer to the ARAnchorObject. */
	UPROPERTY()
	UGoogleARCoreAnchorBase* ARAnchorObject;
};
