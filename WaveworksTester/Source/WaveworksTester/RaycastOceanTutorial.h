// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "RaycastOceanTutorial.generated.h"

UCLASS()
class WAVEWORKSTESTER_API ARaycastOceanTutorial : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARaycastOceanTutorial();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	// Called when recieve intersect points from rendering thread
	void OnRecievedWaveWorksIntersectPoints(FVector OutIntersectPoint, bool bSuc);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	AActor* WaveWorksActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	AActor* RaycastOriginActor;

private:
	FVector IntersectPoint;
	UWaveWorksComponent* WaveWorksComponent;		
	FWaveWorksRaycastResultDelegate WaveWorksRaycastResultDelegate;
};
