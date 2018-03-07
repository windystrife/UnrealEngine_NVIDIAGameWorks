// Copyright 2017 Google Inc.

#pragma once
#include "GoogleARCorePrimitives.h"
#include "GoogleARCorePlane.h"
#include "CoreMinimal.h"

#include "GoogleARCorePlaneManager.generated.h"

UCLASS()
class GOOGLEARCOREBASE_API UGoogleARCorePlaneManager : public UObject
{
	friend class FGoogleARCoreDevice;
	GENERATED_BODY()
public:
	UGoogleARCorePlaneManager();

	void GetAllPlanes(TArray<UGoogleARCorePlane*>& ARCorePlaneList) const;
	bool PerformLineTraceOnPlanes(FVector StartPoint, FVector EndPoint, FVector& ImpactPoint, FVector& ImpactNormal, UGoogleARCorePlane*& HitPlane, bool bCheckBoundingBoxOnly = false);
private:
	UPROPERTY()
	TMap<int, UGoogleARCorePlane*> ARCorePlaneMap;
	float TimeLeftToUpdatePlanes;

	void UpdatePlanes(float DeltaTime);
	void EmptyPlanes();
};
