// Fill out your copyright notice in the Description page of Project Settings.

#include "WaveworksTester.h"
#include "WaveWorksFloatingComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values for this component's properties
UWaveWorksFloatingComponent::UWaveWorksFloatingComponent()
	: bShowVoxels(true)
	, VoxelRadius(0.0f)
	, VoxelImpulsedForce(0)
	, BuoyancyBodyComponent(nullptr)
	, WaveWorksComponent(nullptr)
	, WaveWorksStaticMeshComponent(nullptr)
	, InitialLinearDamping(0)
	, InitialAngularDamping(0)
{
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UWaveWorksFloatingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (nullptr != WaveWorksActor)
	{
		WaveWorksComponent = Cast<UWaveWorksComponent>(WaveWorksActor->GetComponentByClass(UWaveWorksComponent::StaticClass()));
		WaveWorksStaticMeshComponent = Cast<UWaveWorksStaticMeshComponent>(WaveWorksActor->GetComponentByClass(UWaveWorksStaticMeshComponent::StaticClass()));
		WaveWorksRecieveDisplacementDelegate = FWaveWorksSampleDisplacementsDelegate::CreateUObject(this, &UWaveWorksFloatingComponent::OnRecievedWaveWorksDisplacements);

		BuoyancyBodyComponent = Cast<UStaticMeshComponent>(GetOwner()->GetComponentByClass(UStaticMeshComponent::StaticClass()));

		// Calculate voxels
		CutIntoVoxels(VoxelCenterPoints);

		// Calculate Buoyancy
		float TotalBuoyancy = WaterDensity * BuoyancyBodyComponent->GetVolume() * -GetWorld()->GetWorldSettings()->WorldGravityZ / 1000.0f;
		VoxelBuoyancy = TotalBuoyancy / VoxelCenterPoints.Num();
		VoxelImpulsedForce = WaterImpulsedForce / VoxelCenterPoints.Num();

		InitialLinearDamping = BuoyancyBodyComponent->GetLinearDamping();
		InitialAngularDamping = BuoyancyBodyComponent->GetAngularDamping();
	}
}

// Called every frame
void UWaveWorksFloatingComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	AActor* pOwner = GetOwner();
	if ((nullptr != WaveWorksComponent || nullptr != WaveWorksStaticMeshComponent) && VoxelCenterPoints.Num() > 0)
	{
		// Sample Displacements
		TArray<FVector> SamplePoints;
		for (int i = 0; i < VoxelCenterPoints.Num(); i++)
		{
			FVector CenterPt = UKismetMathLibrary::TransformLocation(pOwner->GetActorTransform(), VoxelCenterPoints[i]) / 100.0f;
			SamplePoints.Add(CenterPt);
		}
		if(WaveWorksComponent)
			WaveWorksComponent->SampleDisplacements(SamplePoints, WaveWorksRecieveDisplacementDelegate);
		else if (WaveWorksStaticMeshComponent)
			WaveWorksStaticMeshComponent->SampleDisplacements(SamplePoints, WaveWorksRecieveDisplacementDelegate);

		// Calculate Buoyancy Force
		if (WaveWorksOutDisplacements.Num() != 0)
		{
			float SubmergedVolume = 0;
			for (int index = 0; index < WaveWorksOutDisplacements.Num(); index++)
			{
				FVector NewSamplePoint;
				NewSamplePoint = WaveWorksInDisplacementSamplers[index] * 100.0f;

				FVector NewDisplacementPoint;
				NewDisplacementPoint.X = WaveWorksOutDisplacements[index].X * 100.0f + NewSamplePoint.X;
				NewDisplacementPoint.Y = WaveWorksOutDisplacements[index].Y * 100.0f + NewSamplePoint.Y;

				if(WaveWorksComponent)
					NewDisplacementPoint.Z = WaveWorksOutDisplacements[index].Z * 100.0f + WaveWorksComponent->SeaLevel;
				else if (WaveWorksStaticMeshComponent)
				{
					NewDisplacementPoint.Z = WaveWorksOutDisplacements[index].Z * 100.0f + WaveWorksStaticMeshComponent->GetOwner()->GetActorLocation().Z;
				}
				//DrawDebugSphere(GetWorld(), NewDisplacementPoint, VoxelRadius * 0.5f, 6, FColor(255, 0, 0));

				float WaterLevel = NewDisplacementPoint.Z;
				float DeepLevel = WaterLevel - NewSamplePoint.Z + (VoxelRadius); // How deep is the voxel
				DeepLevel = FMath::Clamp<float>(DeepLevel, 0, 2.0f * VoxelRadius);
				float SubmergedFactor = ((3.0 * VoxelRadius - DeepLevel) * FMath::Pow(DeepLevel, 2)) / (4.0 * FMath::Pow(VoxelRadius, 3));
				SubmergedVolume += SubmergedFactor;

				FVector FinalForce = FVector(0, 0, VoxelBuoyancy * SubmergedFactor);
				BuoyancyBodyComponent->AddForceAtLocation(FinalForce, NewSamplePoint);

				FVector WaterImpusedForce = FVector::ZeroVector;
				if (WaveWorksComponent)
				{
					WaterImpusedForce = FVector(WaveWorksComponent->WaveWorksAsset->WindDirection.X,
						WaveWorksComponent->WaveWorksAsset->WindDirection.Y, 0) * WaveWorksComponent->WaveWorksAsset->BeaufortScale *
						WaveWorksComponent->WaveWorksAsset->WindDependency * VoxelImpulsedForce * SubmergedFactor;
				}				
				else
				{
					WaterImpusedForce = FVector(WaveWorksStaticMeshComponent->WaveWorksAsset->WindDirection.X,
						WaveWorksStaticMeshComponent->WaveWorksAsset->WindDirection.Y, 0) * WaveWorksStaticMeshComponent->WaveWorksAsset->BeaufortScale *
						WaveWorksStaticMeshComponent->WaveWorksAsset->WindDependency * VoxelImpulsedForce * SubmergedFactor;
				}

				BuoyancyBodyComponent->AddForceAtLocation(WaterImpusedForce, NewSamplePoint);

				//DrawDebugLine(GetWorld(), NewSamplePoint, NewSamplePoint + FVector(0, 0, 100), FColor(0, 255, 0));
			}

			SubmergedVolume /= VoxelCenterPoints.Num(); // 0 - object is fully out of the water, 1 - object is fully submerged
			BuoyancyBodyComponent->SetLinearDamping(FMath::Lerp(InitialLinearDamping, DragInWater, SubmergedVolume));
			BuoyancyBodyComponent->SetAngularDamping(FMath::Lerp(InitialAngularDamping, AngularDragInWater, SubmergedVolume));
		}
		
		// Helper -> show voxels
		if (bShowVoxels)
		{
			for (int i = 0; i < VoxelCenterPoints.Num(); i++)
			{
				FVector CenterPt = UKismetMathLibrary::TransformLocation(pOwner->GetActorTransform(), VoxelCenterPoints[i]);
				DrawDebugSphere(GetWorld(), CenterPt, VoxelRadius * 0.9f, 6, FColor(255, 0, 0));
			}
		}
	}
}

void UWaveWorksFloatingComponent::CutIntoVoxels(TArray<FVector>& OutVoxelCenterPoints)
{
	AActor* pOwner = GetOwner();

	FQuat InitialRotation = pOwner->GetActorQuat();
	pOwner->SetActorRotation(FQuat::Identity);

	FVector Orgin;
	FVector BoundsExtent;
	pOwner->GetActorBounds(false, Orgin, BoundsExtent);	

	float MinExtentLength = (BoundsExtent.X < BoundsExtent.Y) ? BoundsExtent.X : BoundsExtent.Y;
	MinExtentLength = (MinExtentLength < BoundsExtent.Z) ? MinExtentLength : BoundsExtent.Z;

	VoxelRadius = MinExtentLength * NormalizedVoxelSize;
	int VoxelsCountForAxisX = FMath::RoundToInt(BoundsExtent.X / VoxelRadius);
	int VoxelsCountForAxisY = FMath::RoundToInt(BoundsExtent.Y / VoxelRadius);
	int VoxelsCountForAxisZ = FMath::RoundToInt(BoundsExtent.Z / VoxelRadius);

	FVector BoundMin = Orgin - BoundsExtent;
	for (int i = 0; i < VoxelsCountForAxisX; i++)
	{
		for (int j = 0; j < VoxelsCountForAxisY; j++)
		{
			for (int k = 0; k < VoxelsCountForAxisZ; k++)
			{
				float pX = BoundMin.X + (VoxelRadius * 2.0f) * (0.5f + i);
				float pY = BoundMin.Y + (VoxelRadius * 2.0f) * (0.5f + j);
				float pZ = BoundMin.Z + (VoxelRadius * 2.0f) * (0.5f + k);

				FVector point(pX, pY, pZ);

				TArray<class AActor*> OutActors;
				TArray<TEnumAsByte<EObjectTypeQuery> > bjectTypes;
				const TArray<AActor*> ActorsToIgnore;
				if(UKismetSystemLibrary::SphereOverlapActors(GetWorld(), point, VoxelRadius, bjectTypes, AStaticMeshActor::StaticClass(), ActorsToIgnore, OutActors))
				{
					for (int32 index = 0; index <= OutActors.Num(); index++)
					{
						if (OutActors[index] == pOwner)
						{
							OutVoxelCenterPoints.Add(UKismetMathLibrary::InverseTransformLocation(pOwner->GetActorTransform(), point));
							break;
						}
					}
				}
			}
		}
	}

	pOwner->SetActorRotation(InitialRotation);
}

// displacements
void UWaveWorksFloatingComponent::OnRecievedWaveWorksDisplacements(TArray<FVector> InDisplacementSamplers, TArray<FVector4> OutDisplacements)
{
	WaveWorksInDisplacementSamplers = InDisplacementSamplers;
	WaveWorksOutDisplacements = OutDisplacements;
}