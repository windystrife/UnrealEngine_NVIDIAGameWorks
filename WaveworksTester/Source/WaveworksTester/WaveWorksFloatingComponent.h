// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "WaveWorksFloatingComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WAVEWORKSTESTER_API UWaveWorksFloatingComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWaveWorksFloatingComponent();

	// Called when the game starts
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	// RecieveWaveWorksDisplacements Handler
	void OnRecievedWaveWorksDisplacements(TArray<FVector> InDisplacementSamplers, TArray<FVector4> OutDisplacements);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Buoyancy)
	AActor* WaveWorksActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Buoyancy)
	bool bShowVoxels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Buoyancy, meta = (ClampMin = "0"))
	float WaterDensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Buoyancy, meta = (ClampMin = "0"))
	float NormalizedVoxelSize = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Buoyancy, meta = (ClampMin = "0"))
	float DragInWater = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Buoyancy, meta = (ClampMin = "0"))
	float AngularDragInWater = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Buoyancy, meta = (ClampMin = "0"))
	float WaterImpulsedForce = 0.0f;

protected:
	void CutIntoVoxels(TArray<FVector>& OutVoxelCenterPoints);

private:
	class UWaveWorksComponent* WaveWorksComponent;
	class UWaveWorksStaticMeshComponent* WaveWorksStaticMeshComponent;
	class UStaticMeshComponent* BuoyancyBodyComponent;

	// displacements
	TArray<FVector>  WaveWorksInDisplacementSamplers;
	TArray<FVector4> WaveWorksOutDisplacements;
	FWaveWorksSampleDisplacementsDelegate WaveWorksRecieveDisplacementDelegate;

	// voxel infos
	float VoxelRadius;
	float VoxelBuoyancy;
	float VoxelImpulsedForce;
	TArray<FVector> VoxelCenterPoints;	

	// dampings
	float InitialLinearDamping;
	float InitialAngularDamping;
};
