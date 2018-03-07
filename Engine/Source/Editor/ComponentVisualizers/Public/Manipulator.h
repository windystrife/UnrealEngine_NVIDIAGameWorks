// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "ViewportInteractableInterface.h"
#include "Manipulator.generated.h"

UCLASS()
class COMPONENTVISUALIZERS_API AManipulator : public AActor, public IViewportInteractableInterface
{
	GENERATED_BODY()

public:

	AManipulator();

	// Begin AActor
	virtual void PostEditMove(bool bFinished) override;
	virtual bool IsEditorOnly() const final;
	// End AActor

	// Begin IViewportInteractableInterface
	virtual void OnPressed(UViewportInteractor* Interactor, const FHitResult& InHitResult, bool& bOutResultedInDrag) override;
	virtual void OnHover(UViewportInteractor* Interactor) override;
	virtual void OnHoverEnter(UViewportInteractor* Interactor, const FHitResult& InHitResult) override;
	virtual void OnHoverLeave(UViewportInteractor* Interactor, const UActorComponent* NewComponent) override;
	virtual void OnDragRelease(UViewportInteractor* Interactor) override;
	virtual bool CanBeSelected() override { return true; };
	// End IViewportInteractableInterface

	/** Set the component that should be moved when the manipulator was moved. */
	void SetAssociatedComponent(USceneComponent* SceneComponent);

private:

	/** The component to transform when this manipulator was moved. */
	UPROPERTY()
	USceneComponent* AssociatedComponent;

	/** Visual representation of this manipulator. */
	UPROPERTY()
	class UStaticMeshComponent* StaticMeshComponent;

};