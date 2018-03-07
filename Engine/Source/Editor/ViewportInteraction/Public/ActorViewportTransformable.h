// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportTransformable.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/WeakObjectPtr.h"

/**
 * A transformable actor
 */
class VIEWPORTINTERACTION_API FActorViewportTransformable : public FViewportTransformable
{

public:

	/** Sets up safe defaults */
	FActorViewportTransformable()
		: ActorWeakPtr()
	{
	}

	// FViewportTransformable overrides
	virtual const FTransform GetTransform() const override;
	virtual void ApplyTransform( const FTransform& NewTransform, const bool bSweep ) override;
	virtual FBox BuildBoundingBox( const FTransform& BoundingBoxToWorld ) const override;
	virtual bool IsPhysicallySimulated() const override;
	virtual void SetLinearVelocity( const FVector& NewVelocity ) override;
	virtual FVector GetLinearVelocity() const override;
	virtual void UpdateIgnoredActorList( TArray<class AActor*>& IgnoredActors ) override;

	/** The actual actor object */
	TWeakObjectPtr<class AActor> ActorWeakPtr;
};


