// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityUtilLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Components/MaterialBillboardComponent.h"
#include "Misc/ConfigCacheIni.h" // for GetFloat()

/* MixedRealityUtilLibrary_Impl 
 *****************************************************************************/

namespace MixedRealityUtilLibrary_Impl
{
	static USceneComponent* GetHMDRootComponent(const APawn* PlayerPawn);
	static FTransform GetVRToWorldTransform(const APawn* PlayerPawn);
	static bool IsActorOwnedByPlayer(AActor* ActorInst, ULocalPlayer* Player);

	typedef TPairInitializer<int32, float> FDivergenceVal;
	typedef TPair<int32, float> FDivergenceItem;

	static FVector FindAvgVector(const TArray<FVector>& VectorSet);
	static void ComputeDivergenceField(const TArray<FVector>& VectorSet, TArray<FDivergenceItem>& Out);
}

static USceneComponent* MixedRealityUtilLibrary_Impl::GetHMDRootComponent(const APawn* PlayerPawn)
{
	USceneComponent* VROrigin = nullptr;
	if (UCameraComponent* HMDCamera = UMixedRealityUtilLibrary::GetHMDCameraComponent(PlayerPawn))
	{
		VROrigin = HMDCamera->GetAttachParent();
	}
	return VROrigin;
}

static FTransform MixedRealityUtilLibrary_Impl::GetVRToWorldTransform(const APawn* PlayerPawn)
{
	if (USceneComponent* VROrigin = GetHMDRootComponent(PlayerPawn))
	{
		return VROrigin->GetComponentTransform();
	}
	else
	{
		return FTransform::Identity;
	}
}

static bool MixedRealityUtilLibrary_Impl::IsActorOwnedByPlayer(AActor* ActorInst, ULocalPlayer* Player)
{
	bool bIsOwned = false;
	if (ActorInst)
	{
		if (UWorld* ActorWorld = ActorInst->GetWorld())
		{
			APlayerController* Controller = Player->GetPlayerController(ActorWorld);
			if (Controller && ActorInst->IsOwnedBy(Controller))
			{
				bIsOwned = true;
			}
			else if (Controller)
			{
				APawn* PlayerPawn = Controller->GetPawnOrSpectator();
				if (PlayerPawn && ActorInst->IsOwnedBy(PlayerPawn))
				{
					bIsOwned = true;
				}
				else if (USceneComponent* ActorRoot = ActorInst->GetRootComponent())
				{
					USceneComponent* AttachParent = ActorRoot->GetAttachParent();
					if (AttachParent)
					{
						bIsOwned = IsActorOwnedByPlayer(AttachParent->GetOwner(), Player);
					}
				}
			}
		}
	}
	return bIsOwned;
}

static FVector MixedRealityUtilLibrary_Impl::FindAvgVector(const TArray<FVector>& VectorSet)
{
	FVector AvgVec = FVector::ZeroVector;
	for (const FVector& Vec : VectorSet)
	{
		AvgVec += Vec;
	}
	if (VectorSet.Num() > 0)
	{
		AvgVec /= VectorSet.Num();
	}
	return AvgVec;
}

static void MixedRealityUtilLibrary_Impl::ComputeDivergenceField(const TArray<FVector>& VectorSet, TArray<MixedRealityUtilLibrary_Impl::FDivergenceItem>& Out)
{
	Out.Empty(VectorSet.Num());
	FVector AvgVec = FindAvgVector(VectorSet);

	int32 Index = 0;
	for (const FVector& Vec : VectorSet)
	{		
		Out.Add(FDivergenceVal(Index, FVector::Distance(Vec, AvgVec)));
		++Index;
	}

	Out.Sort([](const FDivergenceItem& A, const FDivergenceItem& B) {
		return A.Value < B.Value; // sort by divergence (smallest to largest)
	});
}

/* UMixedRealityUtilLibrary 
 *****************************************************************************/

UMixedRealityUtilLibrary::UMixedRealityUtilLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

APawn* UMixedRealityUtilLibrary::FindAssociatedPlayerPawn(AActor* ActorInst)
{
	APawn* PlayerPawn = nullptr;

	if (UWorld* TargetWorld = ActorInst->GetWorld())
	{
		const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(TargetWorld);
		for (ULocalPlayer* Player : LocalPlayers)
		{
			if (MixedRealityUtilLibrary_Impl::IsActorOwnedByPlayer(ActorInst, Player))
			{
				PlayerPawn = Player->GetPlayerController(TargetWorld)->GetPawnOrSpectator();
				break;
			}
		}
	}
	return PlayerPawn;
}

USceneComponent* UMixedRealityUtilLibrary::FindAssociatedHMDRoot(AActor* ActorInst)
{
	APawn* PlayerPawn = FindAssociatedPlayerPawn(ActorInst);
	return MixedRealityUtilLibrary_Impl::GetHMDRootComponent(PlayerPawn);
}

USceneComponent* UMixedRealityUtilLibrary::GetHMDRootComponent(const UObject* WorldContextObject, int32 PlayerIndex)
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex);
	return MixedRealityUtilLibrary_Impl::GetHMDRootComponent(PlayerPawn);
}

USceneComponent* UMixedRealityUtilLibrary::GetHMDRootComponentFromPlayer(const APlayerController* Player)
{
	if (Player)
	{
		return MixedRealityUtilLibrary_Impl::GetHMDRootComponent(Player->GetPawnOrSpectator());
	}
	return nullptr;
}

UCameraComponent* UMixedRealityUtilLibrary::GetHMDCameraComponent(const APawn* PlayerPawn)
{
	UCameraComponent* HMDCam = nullptr;
	if (PlayerPawn)
	{
		TArray<UCameraComponent*> CameraComponents;
		PlayerPawn->GetComponents(CameraComponents);

		for (UCameraComponent* Camera : CameraComponents)
		{
			if (Camera->bLockToHmd)
			{
				HMDCam = Camera;
				break;
			}
		}

		if (HMDCam == nullptr && CameraComponents.Num() > 0)
		{
			HMDCam = CameraComponents[0];
		}
	}
	return HMDCam;
}

FTransform UMixedRealityUtilLibrary::GetVRDeviceToWorldTransform(const UObject* WorldContextObject, int32 PlayerIndex)
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, PlayerIndex);
	return MixedRealityUtilLibrary_Impl::GetVRToWorldTransform(PlayerPawn);
}

FTransform UMixedRealityUtilLibrary::GetVRDeviceToWorldTransformFromPlayer(const APlayerController* Player)
{
	if (Player)
	{
		return MixedRealityUtilLibrary_Impl::GetVRToWorldTransform(Player->GetPawnOrSpectator());
	}
	return FTransform::Identity;
}

void UMixedRealityUtilLibrary::SetMaterialBillboardSize(UMaterialBillboardComponent* Target, float NewSizeX, float NewSizeY)
{
	bool bMarkRenderStateDirty = false;
	for (FMaterialSpriteElement& Sprite : Target->Elements)
	{
		if (Sprite.BaseSizeX != NewSizeX || Sprite.BaseSizeY != NewSizeY)
		{
			Sprite.BaseSizeX = NewSizeX;
			Sprite.BaseSizeY = NewSizeY;
			bMarkRenderStateDirty = true;
		}
	}

	if (bMarkRenderStateDirty)
	{
		Target->MarkRenderStateDirty();
	}
}

void UMixedRealityUtilLibrary::SanitizeVectorDataSet(TArray<FVector>& VectorArray, const float TolerableDeviation, const int32 MinSamplCount, const int32 MaxSampleCount, const bool bRecursive)
{
	using namespace MixedRealityUtilLibrary_Impl;

	if (VectorArray.Num() >= MinSamplCount)
	{
		TArray<FDivergenceItem> SortedDivergenceVals;
		ComputeDivergenceField(VectorArray, SortedDivergenceVals);

		if (SortedDivergenceVals.Num() > 1 && (SortedDivergenceVals.Last().Value > TolerableDeviation || SortedDivergenceVals.Num() > MaxSampleCount))
		{
			const int32 IsEven = 1 - (SortedDivergenceVals.Num() % 2);
			const int32 FirstHalfEnd = (SortedDivergenceVals.Num() / 2) - IsEven;
			const int32 SecndHalfStart = FirstHalfEnd + IsEven;
			const int32 SecndHalfEnd = SortedDivergenceVals.Num() - 1;

			auto ComputeMedian = [&SortedDivergenceVals](const int32 StartIndx, const int32 LastIndx)->float
			{
				const int32 ValCount = (LastIndx - StartIndx) + 1;
				const int32 MedianIndex = StartIndx + (ValCount / 2);

				float MedianVal = SortedDivergenceVals[MedianIndex].Value;
				if (ValCount % 2 == 0)
				{
					MedianVal = (MedianVal + SortedDivergenceVals[MedianIndex - 1].Value) / 2.f;
				}
				return MedianVal;
			};

			// compute the 1st and 3rd quartile
			const float Q1 = ComputeMedian(0, FirstHalfEnd);
			const float Q3 = ComputeMedian(SecndHalfStart, SecndHalfEnd);
			// compute the interquartile range
			const float IQR = Q3 - Q1;

			const float UpperLimit = Q3 + 1.5 * IQR;
			const float LowerLimit = Q1 - 1.5 * IQR;

			// sort  in an order so we can remove from VectorArray without worries of indices becoming stale
			auto ReverseIndexOrder = [](const FDivergenceItem& A, const FDivergenceItem& B)
			{
				return A.Key > B.Key;
			};

			const bool bHasOutliers = (SortedDivergenceVals.Last().Value > UpperLimit) || (SortedDivergenceVals[0].Value < LowerLimit);
			if (bHasOutliers)
			{
				SortedDivergenceVals.Sort(ReverseIndexOrder);
				for (const FDivergenceItem& DivergenceVal : SortedDivergenceVals)
				{
					if (DivergenceVal.Value < LowerLimit || DivergenceVal.Value > UpperLimit)
					{
						VectorArray.RemoveAtSwap(DivergenceVal.Key);
					}
				}

				if (bRecursive)
				{
					// run another pass, the divergence will change now that we've rejected some points contributing to the avg
					SanitizeVectorDataSet(VectorArray, TolerableDeviation, MinSamplCount, MaxSampleCount, bRecursive);
				}
			}
			else if (SortedDivergenceVals.Num() > MaxSampleCount)
			{
				const int32 ExcessCount = (SortedDivergenceVals.Num() - MaxSampleCount);
				if (bRecursive)
				{
					float IterationPercent = 0.33f;
					GConfig->GetFloat(
						TEXT("/MixedRealityFramework/Calibration/Alignment/BP_AlignmentController.BP_AlignmentController_C"),
						TEXT("DeviationSanitizationIterationPercent"),
						IterationPercent,
						GEngineIni
					);

					// remove the top X percent and recalculate
					const int32 NumToRemove = FMath::CeilToInt(ExcessCount * IterationPercent);

					TArray<FDivergenceItem> MostDivergent(SortedDivergenceVals.GetData() + SortedDivergenceVals.Num() - NumToRemove, NumToRemove);
					MostDivergent.Sort(ReverseIndexOrder);

					for (const FDivergenceItem& DivergenceVal : MostDivergent)
					{
						VectorArray.RemoveAtSwap(DivergenceVal.Key);
					}

					SanitizeVectorDataSet(VectorArray, TolerableDeviation, MinSamplCount, MaxSampleCount, bRecursive);
				}
				else
				{
					TArray<FDivergenceItem> MostDivergent(SortedDivergenceVals.GetData() + MaxSampleCount, ExcessCount);
					MostDivergent.Sort(ReverseIndexOrder);

					for (const FDivergenceItem& DivergenceVal : MostDivergent)
					{
						VectorArray.RemoveAtSwap(DivergenceVal.Key);
					}
				}
			}
		}
	}

	
}
