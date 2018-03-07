// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/Tests/EnvQueryTest_Trace.h"
#include "UObject/Package.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Engine/World.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryTest_Trace::UEnvQueryTest_Trace(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(false);
	
	Context = UEnvQueryContext_Querier::StaticClass();
	TraceData.SetGeometryOnly();
}

void UEnvQueryTest_Trace::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* DataOwner = QueryInstance.Owner.Get();
	BoolValue.BindData(DataOwner, QueryInstance.QueryID);
	TraceFromContext.BindData(DataOwner, QueryInstance.QueryID);
	ItemHeightOffset.BindData(DataOwner, QueryInstance.QueryID);
	ContextHeightOffset.BindData(DataOwner, QueryInstance.QueryID);

	bool bWantsHit = BoolValue.GetValue();
	bool bTraceToItem = TraceFromContext.GetValue();
	float ItemZ = ItemHeightOffset.GetValue();
	float ContextZ = ContextHeightOffset.GetValue();

	TArray<FVector> ContextLocations;
	if (!QueryInstance.PrepareContext(Context, ContextLocations))
	{
		return;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(EnvQueryTrace), TraceData.bTraceComplex);
	TraceParams.bTraceAsyncScene = true;

	TArray<AActor*> IgnoredActors;
	if (QueryInstance.PrepareContext(Context, IgnoredActors))
	{
		TraceParams.AddIgnoredActors(IgnoredActors);
	}
	
	ECollisionChannel TraceCollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceData.TraceChannel);	
	FVector TraceExtent(TraceData.ExtentX, TraceData.ExtentY, TraceData.ExtentZ);
	FRunTraceSignature TraceFunc;
	switch (TraceData.TraceShape)
	{
	case EEnvTraceShape::Line:
		TraceFunc.BindUObject(this, bTraceToItem ? &UEnvQueryTest_Trace::RunLineTraceTo : &UEnvQueryTest_Trace::RunLineTraceFrom);
		break;

	case EEnvTraceShape::Box:
		TraceFunc.BindUObject(this, bTraceToItem ? &UEnvQueryTest_Trace::RunBoxTraceTo : &UEnvQueryTest_Trace::RunBoxTraceFrom);
		break;

	case EEnvTraceShape::Sphere:
		TraceFunc.BindUObject(this, bTraceToItem ? &UEnvQueryTest_Trace::RunSphereTraceTo : &UEnvQueryTest_Trace::RunSphereTraceFrom);
		break;

	case EEnvTraceShape::Capsule:
		TraceFunc.BindUObject(this, bTraceToItem ? &UEnvQueryTest_Trace::RunCapsuleTraceTo : &UEnvQueryTest_Trace::RunCapsuleTraceFrom);
		break;

	default:
		return;
	}

	for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
	{
		ContextLocations[ContextIndex].Z += ContextZ;
	}

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex()) + FVector(0, 0, ItemZ);
		AActor* ItemActor = GetItemActor(QueryInstance, It.GetIndex());

		for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
		{
			const bool bHit = TraceFunc.Execute(ItemLocation, ContextLocations[ContextIndex], ItemActor, QueryInstance.World, TraceCollisionChannel, TraceParams, TraceExtent);
			It.SetScore(TestPurpose, FilterType, bHit, bWantsHit);
		}
	}
}

void UEnvQueryTest_Trace::PostLoad()
{
	Super::PostLoad();
	TraceData.OnPostLoad();
}

FText UEnvQueryTest_Trace::GetDescriptionTitle() const
{
	UEnum* ChannelEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETraceTypeQuery"), true);
	FString ChannelDesc = ChannelEnum->GetDisplayNameTextByValue(TraceData.TraceChannel).ToString();

	FString DirectionDesc = TraceFromContext.IsDynamic() ?
		FString::Printf(TEXT("%s, direction: %s"), *UEnvQueryTypes::DescribeContext(Context).ToString(), *TraceFromContext.ToString()) :
		FString::Printf(TEXT("%s %s"), TraceFromContext.DefaultValue ? TEXT("from") : TEXT("to"), *UEnvQueryTypes::DescribeContext(Context).ToString());

	return FText::FromString(FString::Printf(TEXT("%s: %s on %s"), 
		*Super::GetDescriptionTitle().ToString(), *DirectionDesc, *ChannelDesc));
}

FText UEnvQueryTest_Trace::GetDescriptionDetails() const
{
	return FText::Format(FText::FromString("{0}\n{1}"),
		TraceData.ToText(FEnvTraceData::Detailed), DescribeBoolTestParams("hit"));
}

bool UEnvQueryTest_Trace::RunLineTraceTo(const FVector& ItemPos, const FVector& ContextPos, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params, const FVector& Extent)
{
	FCollisionQueryParams TraceParams(Params);
	TraceParams.AddIgnoredActor(ItemActor);

	const bool bHit = World->LineTraceTestByChannel(ContextPos, ItemPos, Channel, TraceParams);
	return bHit;
}

bool UEnvQueryTest_Trace::RunLineTraceFrom(const FVector& ItemPos, const FVector& ContextPos, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params, const FVector& Extent)
{
	FCollisionQueryParams TraceParams(Params);
	TraceParams.AddIgnoredActor(ItemActor);

	const bool bHit = World->LineTraceTestByChannel(ItemPos, ContextPos, Channel, TraceParams);
	return bHit;
}

bool UEnvQueryTest_Trace::RunBoxTraceTo(const FVector& ItemPos, const FVector& ContextPos, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params, const FVector& Extent)
{
	FCollisionQueryParams TraceParams(Params);
	TraceParams.AddIgnoredActor(ItemActor);

	const bool bHit = World->SweepTestByChannel(ContextPos, ItemPos, FQuat((ItemPos - ContextPos).Rotation()), Channel, FCollisionShape::MakeBox(Extent), TraceParams);
	return bHit;
}

bool UEnvQueryTest_Trace::RunBoxTraceFrom(const FVector& ItemPos, const FVector& ContextPos, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params, const FVector& Extent)
{
	FCollisionQueryParams TraceParams(Params);
	TraceParams.AddIgnoredActor(ItemActor);

	const bool bHit = World->SweepTestByChannel(ItemPos, ContextPos, FQuat((ContextPos - ItemPos).Rotation()), Channel, FCollisionShape::MakeBox(Extent), TraceParams);
	return bHit;
}

bool UEnvQueryTest_Trace::RunSphereTraceTo(const FVector& ItemPos, const FVector& ContextPos, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params, const FVector& Extent)
{
	FCollisionQueryParams TraceParams(Params);
	TraceParams.AddIgnoredActor(ItemActor);

	const bool bHit = World->SweepTestByChannel(ContextPos, ItemPos, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Extent.X), TraceParams);
	return bHit;
}

bool UEnvQueryTest_Trace::RunSphereTraceFrom(const FVector& ItemPos, const FVector& ContextPos, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params, const FVector& Extent)
{
	FCollisionQueryParams TraceParams(Params);
	TraceParams.AddIgnoredActor(ItemActor);

	const bool bHit = World->SweepTestByChannel(ItemPos, ContextPos, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Extent.X), TraceParams);
	return bHit;
}

bool UEnvQueryTest_Trace::RunCapsuleTraceTo(const FVector& ItemPos, const FVector& ContextPos, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params, const FVector& Extent)
{
	FCollisionQueryParams TraceParams(Params);
	TraceParams.AddIgnoredActor(ItemActor);

	const bool bHit = World->SweepTestByChannel(ContextPos, ItemPos, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(Extent.X, Extent.Z), TraceParams);
	return bHit;
}

bool UEnvQueryTest_Trace::RunCapsuleTraceFrom(const FVector& ItemPos, const FVector& ContextPos, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params, const FVector& Extent)
{
	FCollisionQueryParams TraceParams(Params);
	TraceParams.AddIgnoredActor(ItemActor);

	const bool bHit = World->SweepTestByChannel(ItemPos, ContextPos, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(Extent.X, Extent.Z), TraceParams);
	return bHit;
}

#undef LOCTEXT_NAMESPACE
