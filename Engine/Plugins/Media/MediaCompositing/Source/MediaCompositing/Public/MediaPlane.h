// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "MediaPlane.generated.h"

class UMediaPlaneComponent;


UCLASS(ClassGroup=Rendering, hidecategories=(Object, Activation, Physics, Collision, Input, PhysicsVolume))
class MEDIACOMPOSITING_API AMediaPlane
	: public AActor
{
public:

	GENERATED_BODY()

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ObjectInitializer Object initializer.
	 */
	AMediaPlane(const FObjectInitializer& ObjectInitializer);

public:

	/**
	 * Get the media plane component of this actor.
	 *
	 * @return The media plane component.
	 */
	UMediaPlaneComponent* GetMediaPlaneComponent() const
	{
		return MediaPlane;
	}

protected:

	/** The media plane component used by this actor. */
	UPROPERTY(Category=MediaPlane, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories="Mesh,Rendering,Physics,Components|StaticMesh"))
	UMediaPlaneComponent* MediaPlane;
};
