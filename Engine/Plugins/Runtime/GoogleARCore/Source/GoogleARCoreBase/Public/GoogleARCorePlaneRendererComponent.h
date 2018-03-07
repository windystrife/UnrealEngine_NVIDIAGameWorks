// Copyright 2017 Google Inc.

#pragma once

#include "GoogleARCorePrimitives.h"
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GoogleARCorePlaneRendererComponent.generated.h"

/** A helper component that renders all the ARCore planes in the current tracking session. */
UCLASS(Experimental, ClassGroup = (GoogleARCore), meta = (BlueprintSpawnableComponent))
class GOOGLEARCOREBASE_API UGoogleARCorePlaneRendererComponent : public USceneComponent
{
	GENERATED_BODY()
public:

	/** Render the plane quad when set to true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCorePlaneRendererComponent")
	bool bRenderPlane = true;

	/** Render the plane boundary polygon lines when set to true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCorePlaneRendererComponent")
	bool bRenderBoundaryPolygon = true;

	/** The color of the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCorePlaneRendererComponent")
	FColor PlaneColor = FColor::Green;

	/** The color of the boundary polygon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCorePlaneRendererComponent")
	FColor BoundaryPolygonColor = FColor::Blue;

	/** The line thickness for the plan boundary polygon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCorePlaneRendererComponent")
	float BoundaryPolygonThickness = 0.5f;

	UGoogleARCorePlaneRendererComponent();

	/** Function called every frame on this Component. */
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction) override;

private:
	TArray<int> PlaneIndices;
	TArray<FVector> PlaneVertices;

	void DrawPlanes();
};
