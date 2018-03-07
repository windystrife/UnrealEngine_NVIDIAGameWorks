// Copyright 2017 Google Inc.

#pragma once

#include "GoogleARCorePrimitives.h"
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GoogleARCorePointCloudRendererComponent.generated.h"

/** A helper component that renders the latest point cloud from the ARCore tracking session. */
UCLASS(Experimental, ClassGroup = (GoogleARCore), meta = (BlueprintSpawnableComponent))
class GOOGLEARCOREBASE_API UGoogleARCorePointCloudRendererComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	/** The color of the point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCorePointCloudRenderer")
	FColor PointColor = FColor::Red;

	/** The size of the point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCorePointCloudRenderer")
	float PointSize = 1.f;

	UGoogleARCorePointCloudRendererComponent();

	/** Function called on every frame on this Component. */
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction) override;

private:
	TArray<FVector> PointCloudInWorldSpace;
	double PreviousPointCloudTimestamp;

	void DrawPointCloud();
};
