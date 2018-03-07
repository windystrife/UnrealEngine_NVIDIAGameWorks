// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/NavigationModifier.h"
#include "UObject/UnrealType.h"
#include "EngineStats.h"
#include "GameFramework/Actor.h"
#include "Components/BrushComponent.h"
#include "AI/Navigation/RecastHelpers.h"
#include "AI/Navigation/NavLinkTrivial.h"
#include "AI/Navigation/NavAreas/NavAreaMeta.h"
#include "PhysicsEngine/BodySetup.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavAreas/NavArea_LowHeight.h"

// if square distance between two points is less than this the those points
// will be considered identical when calculating convex hull
// should be less than voxel size (recast navmesh)
static const float CONVEX_HULL_POINTS_MIN_DISTANCE_SQ = 4.0f * 4.0f;

//----------------------------------------------------------------------//
// FNavigationLinkBase
//----------------------------------------------------------------------//
FNavigationLinkBase::FNavigationLinkBase() 
	: LeftProjectHeight(0.0f), MaxFallDownLength(1000.0f), Direction(ENavLinkDirection::BothWays), UserId(0),
	  SnapRadius(30.f), SnapHeight(50.0f), bUseSnapHeight(false), bSnapToCheapestArea(true),
	  bCustomFlag0(false), bCustomFlag1(false), bCustomFlag2(false), bCustomFlag3(false), bCustomFlag4(false),
	  bCustomFlag5(false), bCustomFlag6(false), bCustomFlag7(false), bAreaClassInitialized(false)
{
	AreaClass = NULL;
	SupportedAgentsBits = 0xFFFFFFFF;
}

void FNavigationLinkBase::SetAreaClass(UClass* InAreaClass)
{
	if (InAreaClass)
	{
		AreaClassOb = InAreaClass;
		AreaClass = InAreaClass;
		bAreaClassInitialized = true;
	}
}

UClass* FNavigationLinkBase::GetAreaClass() const
{
	UClass* ClassOb = AreaClassOb.Get();
	return ClassOb ? ClassOb : (UClass*)UNavigationSystem::GetDefaultWalkableArea();
}

void FNavigationLinkBase::InitializeAreaClass(const bool bForceRefresh)
{
	// @hack fix for changes to AreaClass in the editor not taking effect
	// proper fix will get merged over from Dev-Gen
	if (!bAreaClassInitialized || bForceRefresh || GIsEditor)
	{
		AreaClassOb = (UClass*)AreaClass;
		bAreaClassInitialized = true;
	}
}

bool FNavigationLinkBase::HasMetaArea() const
{
	UClass* ClassOb = GetAreaClass();
	return ClassOb && ClassOb->IsChildOf(UNavAreaMeta::StaticClass());
}

void FNavigationLinkBase::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_NAVIGATION_AGENT_SELECTOR)
	{
		SupportedAgents.bSupportsAgent0 = bSupportsAgent0;
		SupportedAgents.bSupportsAgent1 = bSupportsAgent1;
		SupportedAgents.bSupportsAgent2 = bSupportsAgent2;
		SupportedAgents.bSupportsAgent3 = bSupportsAgent3;
		SupportedAgents.bSupportsAgent4 = bSupportsAgent4;
		SupportedAgents.bSupportsAgent5 = bSupportsAgent5;
		SupportedAgents.bSupportsAgent6 = bSupportsAgent6;
		SupportedAgents.bSupportsAgent7 = bSupportsAgent7;
		SupportedAgents.bSupportsAgent8 = bSupportsAgent8;
		SupportedAgents.bSupportsAgent9 = bSupportsAgent9;
		SupportedAgents.bSupportsAgent10 = bSupportsAgent10;
		SupportedAgents.bSupportsAgent11 = bSupportsAgent11;
		SupportedAgents.bSupportsAgent12 = bSupportsAgent12;
		SupportedAgents.bSupportsAgent13 = bSupportsAgent13;
		SupportedAgents.bSupportsAgent14 = bSupportsAgent14;
		SupportedAgents.bSupportsAgent15 = bSupportsAgent15;
		SupportedAgents.MarkInitialized();
	}

	if (Ar.IsLoading())
	{
		InitializeAreaClass();
	}
}

#if WITH_EDITOR

void FNavigationLinkBase::DescribeCustomFlags(const TArray<FString>& EditableFlagNames, UClass* NavLinkPropertiesOwnerClass)
{
	if (NavLinkPropertiesOwnerClass == nullptr)
	{
		NavLinkPropertiesOwnerClass = UNavLinkDefinition::StaticClass();
	}

	const int32 MaxFlags = FMath::Min(8, EditableFlagNames.Num());
	const FString CustomNameMeta = TEXT("DisplayName");

	for (TFieldIterator<UProperty> PropertyIt(NavLinkPropertiesOwnerClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Prop = *PropertyIt;

		UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop);
		UStructProperty* StructProp = Cast<UStructProperty>(ArrayProp ? ArrayProp->Inner : Prop);

		if (StructProp)
		{
			for (UStruct* StructIt = StructProp->Struct; StructIt; StructIt = StructIt->GetSuperStruct())
			{
				if (StructIt->GetFName() == TEXT("NavigationLinkBase"))
				{
					for (int32 Idx = 0; Idx < 8; Idx++)
					{
						FString PropName(TEXT("bCustomFlag"));
						PropName += TTypeToString<int32>::ToString(Idx);

						UProperty* FlagProp = FindField<UProperty>(StructIt, *PropName);
						if (FlagProp)
						{
							if (Idx < MaxFlags)
							{
								FlagProp->SetPropertyFlags(CPF_Edit);
								FlagProp->SetMetaData(*CustomNameMeta, *EditableFlagNames[Idx]);
							}
							else
							{
								FlagProp->ClearPropertyFlags(CPF_Edit);
							}
						}
					}

					break;
				}
			}
		}
	}
}

#endif // WITH_EDITOR

//----------------------------------------------------------------------//
// UNavLinkDefinition
//----------------------------------------------------------------------//
UNavLinkDefinition::UNavLinkDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bHasInitializedAreaClasses(false)
	, bHasDeterminedMetaAreaClass(false)
	, bHasMetaAreaClass(false)
	, bHasDeterminedAdjustableLinks(false)
	, bHasAdjustableLinks(false)
{
}

const TArray<FNavigationLink>& UNavLinkDefinition::GetLinksDefinition(UClass* LinkDefinitionClass)
{
	const UNavLinkDefinition* LinkDefCDO = LinkDefinitionClass ? LinkDefinitionClass->GetDefaultObject<UNavLinkDefinition>() : nullptr;
	if (LinkDefCDO)
	{
		LinkDefCDO->InitializeAreaClass();
		return LinkDefCDO->Links;
	}

	static const TArray<FNavigationLink> DummyDefinition;
	return DummyDefinition;
}

const TArray<FNavigationSegmentLink>& UNavLinkDefinition::GetSegmentLinksDefinition(UClass* LinkDefinitionClass)
{
	const UNavLinkDefinition* LinkDefCDO = LinkDefinitionClass ? LinkDefinitionClass->GetDefaultObject<UNavLinkDefinition>() : nullptr;
	if (LinkDefCDO)
	{
		LinkDefCDO->InitializeAreaClass();
		return LinkDefCDO->SegmentLinks;
	}

	static const TArray<FNavigationSegmentLink> DummyDefinition;
	return DummyDefinition;
}

#if WITH_EDITOR
void UNavLinkDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// In case relevant data has changed, clear the flag that says we've determined whether there's a meta area class
	// so it will be recalculated the next time it's needed.
	bHasDeterminedMetaAreaClass = false;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UNavLinkDefinition::InitializeAreaClass() const
{
	if (bHasInitializedAreaClasses)
	{
		return;
	}

	UNavLinkDefinition* MutableThis = (UNavLinkDefinition*)this;
	MutableThis->bHasInitializedAreaClasses = true;

	for (int32 Idx = 0; Idx < Links.Num(); Idx++)
	{
		FNavigationLink& LinkData = MutableThis->Links[Idx];
		LinkData.InitializeAreaClass();
	}

	for (int32 Idx = 0; Idx < SegmentLinks.Num(); Idx++)
	{
		FNavigationSegmentLink& LinkData = MutableThis->SegmentLinks[Idx];
		LinkData.InitializeAreaClass();
	}
}

bool UNavLinkDefinition::HasMetaAreaClass() const
{
	if (bHasDeterminedMetaAreaClass)
	{
		return bHasMetaAreaClass;
	}

	UNavLinkDefinition* MutableThis = (UNavLinkDefinition*)this;
	MutableThis->bHasDeterminedMetaAreaClass = true;

	for (int32 Idx = 0; Idx < Links.Num(); Idx++)
	{
		const FNavigationLink& LinkData = Links[Idx];
		if (!bHasMetaAreaClass && LinkData.HasMetaArea())
		{
			MutableThis->bHasMetaAreaClass = true;
			return true;
		}
	}

	for (int32 Idx = 0; Idx < SegmentLinks.Num(); Idx++)
	{
		const FNavigationSegmentLink& LinkData = SegmentLinks[Idx];
		if (!bHasMetaAreaClass && LinkData.HasMetaArea())
		{
			MutableThis->bHasMetaAreaClass = true;
			return true;
		}
	}

	return false;
}

bool UNavLinkDefinition::HasAdjustableLinks() const
{
	if (bHasDeterminedAdjustableLinks && !GIsEditor)
	{
		return bHasAdjustableLinks;
	}

	UNavLinkDefinition* MutableThis = (UNavLinkDefinition*)this;
	MutableThis->bHasDeterminedAdjustableLinks = true;

	for (int32 Idx = 0; Idx < Links.Num(); Idx++)
	{
		if (Links[Idx].MaxFallDownLength > 0)
		{
			MutableThis->bHasAdjustableLinks = true;
			return true;
		}
	}

	for (int32 Idx = 0; Idx < SegmentLinks.Num(); Idx++)
	{
		if (SegmentLinks[Idx].MaxFallDownLength > 0)
		{
			MutableThis->bHasAdjustableLinks = true;
			return true;
		}
	}

	return false;
}



//----------------------------------------------------------------------//
// FAreaNavModifier
//----------------------------------------------------------------------//

FAreaNavModifier::FAreaNavModifier(float Radius, float Height, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> InAreaClass)
{
	Init(InAreaClass);
	
	FVector Scale3D = LocalToWorld.GetScale3D().GetAbs();
	Radius *= FMath::Max(Scale3D.X, Scale3D.Y);
	Height *= Scale3D.Z;

	Points.SetNumUninitialized(2);
	Points[0] = LocalToWorld.GetLocation();
	Points[1].X = Radius;
	Points[1].Z = Height;
	ShapeType = ENavigationShapeType::Cylinder;

	Bounds = FBox::BuildAABB(LocalToWorld.GetLocation(), FVector(Radius, Radius, Height));
}

FAreaNavModifier::FAreaNavModifier(const FVector& Extent, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> InAreaClass)
{
	Init(InAreaClass);
	SetBox(FBox::BuildAABB(FVector::ZeroVector, Extent), LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const FBox& Box, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> InAreaClass)
{
	Init(InAreaClass);
	SetBox(Box, LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const TArray<FVector>& InPoints, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> InAreaClass)
{
	Init(InAreaClass);
	SetConvex(InPoints.GetData(), 0, InPoints.Num(), CoordType, LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const TArray<FVector>& InPoints, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> InAreaClass)
{
	check(InPoints.IsValidIndex(FirstIndex) && InPoints.IsValidIndex(LastIndex-1));

	Init(InAreaClass);
	SetConvex(InPoints.GetData(), FirstIndex, LastIndex, CoordType, LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const TNavStatArray<FVector>& InPoints, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> InAreaClass)
{
	check(InPoints.IsValidIndex(FirstIndex) && InPoints.IsValidIndex(LastIndex-1));

	Init(InAreaClass);
	SetConvex(InPoints.GetData(), FirstIndex, LastIndex, CoordType, LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const UBrushComponent* BrushComponent, const TSubclassOf<UNavArea> InAreaClass)
{
	check(BrushComponent != NULL);

	TArray<FVector> Verts;
	if(BrushComponent->BrushBodySetup)
	{
		for (int32 ElemIndex = 0; ElemIndex < BrushComponent->BrushBodySetup->AggGeom.ConvexElems.Num(); ElemIndex++)
		{
			const FKConvexElem& Convex = BrushComponent->BrushBodySetup->AggGeom.ConvexElems[ElemIndex];
			for (int32 VertexIndex = 0; VertexIndex < Convex.VertexData.Num(); VertexIndex++)
			{
				Verts.AddUnique(Convex.VertexData[VertexIndex]);
			}
		}
	}

	Init(InAreaClass);
	SetConvex(Verts.GetData(), 0, Verts.Num(), ENavigationCoordSystem::Unreal, BrushComponent->GetComponentTransform());
}

void FAreaNavModifier::GetCylinder(FCylinderNavAreaData& Data) const
{
	check(Points.Num() == 2 && ShapeType == ENavigationShapeType::Cylinder);
	Data.Origin = Points[0];
	Data.Radius = Points[1].X;
	Data.Height = Points[1].Z;
}

void FAreaNavModifier::GetBox(FBoxNavAreaData& Data) const
{
	check(Points.Num() == 2 && ShapeType == ENavigationShapeType::Box);
	Data.Origin = Points[0];
	Data.Extent = Points[1];
}

void FAreaNavModifier::GetConvex(FConvexNavAreaData& Data) const
{
	check(ShapeType == ENavigationShapeType::Convex);
	Data.Points.Append(Points);
	FVector LastPoint = Data.Points.Pop();
	Data.MinZ = LastPoint.X;
	Data.MaxZ = LastPoint.Y;
}

void FAreaNavModifier::Init(const TSubclassOf<UNavArea> InAreaClass)
{
	bIncludeAgentHeight = false;
	ApplyMode = ENavigationAreaMode::Apply;
	Cost = 0.0f;
	FixedCost = 0.0f;
	Bounds = FBox(ForceInitToZero);
	SetAreaClass(InAreaClass);
}

void FAreaNavModifier::SetAreaClass(const TSubclassOf<UNavArea> InAreaClass)
{
	AreaClassOb = (UClass*)InAreaClass;

	const UClass* AreaClass1 = AreaClassOb.Get();
	const UClass* AreaClass2 = ReplaceAreaClassOb.Get();
	bHasMetaAreas = (AreaClass1 && AreaClass1->IsChildOf(UNavAreaMeta::StaticClass())) || (AreaClass2 && AreaClass2->IsChildOf(UNavAreaMeta::StaticClass()));
}

void FAreaNavModifier::SetAreaClassToReplace(const TSubclassOf<UNavArea> InAreaClass)
{
	ReplaceAreaClassOb = (UClass*)InAreaClass;

	const UClass* AreaClass1 = AreaClassOb.Get();
	const UClass* AreaClass2 = ReplaceAreaClassOb.Get();
	bHasMetaAreas = (AreaClass1 && AreaClass1->IsChildOf(UNavAreaMeta::StaticClass())) || (AreaClass2 && AreaClass2->IsChildOf(UNavAreaMeta::StaticClass()));

	ApplyMode = (AreaClass2 == UNavArea_LowHeight::StaticClass()) ? ENavigationAreaMode::ReplaceInLowPass :
		AreaClass2 ? ENavigationAreaMode::Replace : ENavigationAreaMode::Apply;
}

void FAreaNavModifier::SetApplyMode(ENavigationAreaMode::Type InApplyMode)
{
	ApplyMode = InApplyMode;
}

bool IsAngleMatching(float Angle)
{
	const float AngleThreshold = 1.0f; // degrees
	return (Angle < AngleThreshold) || ((90.0f - Angle) < AngleThreshold);
}

void FAreaNavModifier::SetBox(const FBox& Box, const FTransform& LocalToWorld)
{
	const FVector BoxOrigin = Box.GetCenter();
	const FVector BoxExtent = Box.GetExtent();

	TArray<FVector> Corners;
	for (int32 i = 0; i < 8; i++)
	{
		const FVector Dir(((i / 4) % 2) ? 1 : -1, ((i / 2) % 2) ? 1 : -1, (i % 2) ? 1 : -1);
		Corners.Add(LocalToWorld.TransformPosition(BoxOrigin + BoxExtent * Dir));
	}

	// check if it can be used as AABB
	const FRotator Rotation = LocalToWorld.GetRotation().Rotator();
	const float PitchMod = FMath::Fmod(FMath::Abs(Rotation.Pitch), 90.0f);
	const float YawMod = FMath::Fmod(FMath::Abs(Rotation.Yaw), 90.0f);
	const float RollMod = FMath::Fmod(FMath::Abs(Rotation.Roll), 90.0f);
	if (IsAngleMatching(PitchMod) && IsAngleMatching(YawMod) && IsAngleMatching(RollMod))
	{
		Bounds = FBox(ForceInit);
		for (int32 i = 0; i < Corners.Num(); i++)
		{
			Bounds += Corners[i];
		}

		Points.SetNumUninitialized(2);
		Points[0] = Bounds.GetCenter();
		Points[1] = Bounds.GetExtent();
		ShapeType = ENavigationShapeType::Box;
	}
	else
	{
		SetConvex(Corners.GetData(), 0, Corners.Num(), ENavigationCoordSystem::Unreal, FTransform::Identity);
	}
}

void FAreaNavModifier::SetConvex(const FVector* InPoints, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld)
{
	FConvexNavAreaData ConvexData;
	ConvexData.MinZ = MAX_FLT;
	ConvexData.MaxZ = -MAX_FLT;

	const int MaxConvexPoints = 8;
	TArray<FVector, TInlineAllocator<MaxConvexPoints>> HullVertices;
	HullVertices.Empty(MaxConvexPoints);

	for (int32 i = FirstIndex; i < LastIndex; i++)
	{
		const FVector Point = (CoordType == ENavigationCoordSystem::Recast) ? Recast2UnrealPoint(InPoints[i]) : InPoints[i];

		FVector TransformedPoint = LocalToWorld.TransformPosition(Point);
		ConvexData.MinZ = FMath::Min( ConvexData.MinZ, TransformedPoint.Z );
		ConvexData.MaxZ = FMath::Max( ConvexData.MaxZ, TransformedPoint.Z );
		TransformedPoint.Z = 0.f;

		// check if there's a similar point already in HullVertices array
		bool bUnique = true;
		const FVector* RESTRICT Start = HullVertices.GetData();
		for (const FVector* RESTRICT Data = Start, * RESTRICT DataEnd = Data + HullVertices.Num(); Data != DataEnd; ++Data)
		{
			if (FVector::DistSquared(*Data, TransformedPoint) < CONVEX_HULL_POINTS_MIN_DISTANCE_SQ)
			{
				bUnique = false;
				break;
			}
		}
		
		if (bUnique)
		{
			HullVertices.Add(TransformedPoint);
		}
	}

	TArray<int32, TInlineAllocator<MaxConvexPoints>> HullIndices;
	HullIndices.Empty(MaxConvexPoints);

	ConvexHull2D::ComputeConvexHull(HullVertices, HullIndices);
	if (HullIndices.Num())
	{
		Bounds = FBox(ForceInit);
		for(int32 i = 0; i < HullIndices.Num(); ++i)
		{
			const FVector& HullVert = HullVertices[HullIndices[i]];
			ConvexData.Points.Add(HullVert);
			Bounds += HullVert;
		}

		Bounds.Min.Z = ConvexData.MinZ;
		Bounds.Max.Z = ConvexData.MaxZ;

		Points.Append(ConvexData.Points);
		Points.Add(FVector(ConvexData.MinZ, ConvexData.MaxZ, 0));
		ShapeType = ENavigationShapeType::Convex;
	}
	else
	{
		ShapeType = ENavigationShapeType::Unknown;
	}
}

//----------------------------------------------------------------------//
// FCustomLinkNavModifier
//----------------------------------------------------------------------//
void FCustomLinkNavModifier::Set(TSubclassOf<UNavLinkDefinition> InPresetLinkClass, const FTransform& InLocalToWorld)
{
	LinkDefinitionClassOb = (UClass*)InPresetLinkClass;
	LocalToWorld = InLocalToWorld;

	const UNavLinkDefinition* LinkDefOb = InPresetLinkClass->GetDefaultObject<UNavLinkDefinition>();
	if (LinkDefOb)
	{
		LinkDefOb->InitializeAreaClass();

		bHasMetaAreas = LinkDefOb->HasMetaAreaClass();
	}
	else
	{
		bHasMetaAreas = false;
	}
}

//----------------------------------------------------------------------//
// FSimpleLinkNavModifier
//----------------------------------------------------------------------//
void FSimpleLinkNavModifier::SetLinks(const TArray<FNavigationLink>& InLinks)
{
	Links = InLinks;
	bHasMetaAreasPoint = false;

	for (int32 Idx = 0; Idx < Links.Num(); Idx++)
	{
		FNavigationLink& LinkData = Links[Idx];

		bHasMetaAreasPoint |= LinkData.HasMetaArea();
		bHasFallDownLinks |= LinkData.MaxFallDownLength > 0.f;
	}

	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::SetSegmentLinks(const TArray<FNavigationSegmentLink>& InLinks)
{
	SegmentLinks = InLinks;

	bHasMetaAreasSegment = false;
	for (int32 Idx = 0; Idx < SegmentLinks.Num(); Idx++)
	{
		FNavigationSegmentLink& LinkData = SegmentLinks[Idx];
		LinkData.UserId = UserId;

		bHasMetaAreasSegment |= LinkData.HasMetaArea();
		bHasFallDownLinks |= LinkData.MaxFallDownLength > 0.f;
	}
	
	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::AppendLinks(const TArray<FNavigationLink>& InLinks)
{
	const int32 LinkBase = SegmentLinks.Num();
	Links.Append(InLinks);

	for (int32 Idx = 0; Idx < InLinks.Num(); Idx++)
	{
		FNavigationLink& LinkData = Links[LinkBase + Idx];

		bHasMetaAreasPoint |= LinkData.HasMetaArea();
		bHasFallDownLinks |= LinkData.MaxFallDownLength > 0.f;
	}

	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::AppendSegmentLinks(const TArray<FNavigationSegmentLink>& InLinks)
{
	const int32 LinkBase = SegmentLinks.Num();
	SegmentLinks.Append(InLinks);

	for (int32 Idx = 0; Idx < InLinks.Num(); Idx++)
	{
		FNavigationSegmentLink& LinkData = SegmentLinks[LinkBase + Idx];
		LinkData.UserId = UserId;

		bHasMetaAreasSegment |= LinkData.HasMetaArea();
		bHasFallDownLinks |= LinkData.MaxFallDownLength > 0.f;
	}

	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::AddLink(const FNavigationLink& InLink)
{
	const int32 LinkIdx = Links.Add(InLink);

	FNavigationLink& LinkData = Links[LinkIdx];

	bHasMetaAreasPoint |= LinkData.HasMetaArea();
	bHasFallDownLinks |= LinkData.MaxFallDownLength > 0.f;
	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::AddSegmentLink(const FNavigationSegmentLink& InLink)
{
	const int32 LinkIdx = SegmentLinks.Add(InLink);

	FNavigationSegmentLink& LinkData = SegmentLinks[LinkIdx];
	LinkData.UserId = UserId;

	bHasMetaAreasSegment |= LinkData.HasMetaArea();
	bHasFallDownLinks |= LinkData.MaxFallDownLength > 0.f;
	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::UpdateFlags()
{
	bHasMetaAreasPoint = false;
	bHasMetaAreasSegment = false;
	bHasFallDownLinks = false;

	for (int32 Idx = 0; Idx < Links.Num(); Idx++)
	{
		bHasMetaAreasPoint |= Links[Idx].HasMetaArea();
		bHasFallDownLinks |= Links[Idx].MaxFallDownLength > 0.f;
	}
	
	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}


//----------------------------------------------------------------------//
// FCompositeNavMeshModifier
//----------------------------------------------------------------------//
void FCompositeNavModifier::Shrink()
{
	Areas.Shrink();
	SimpleLinks.Shrink();
	CustomLinks.Shrink();
}

void FCompositeNavModifier::Reset()
{
	Areas.Reset();
	SimpleLinks.Reset();
	CustomLinks.Reset();
	bHasPotentialLinks = false;
	bAdjustHeight = false;
}

void FCompositeNavModifier::Empty()
{
	Areas.Empty();
	SimpleLinks.Empty();
	CustomLinks.Empty();
	bHasPotentialLinks = false;
	bAdjustHeight = false;
}

FCompositeNavModifier FCompositeNavModifier::GetInstantiatedMetaModifier(const FNavAgentProperties* NavAgent, TWeakObjectPtr<UObject> WeakOwnerPtr) const
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_MetaAreaTranslation);
	FCompositeNavModifier Result;

	check(NavAgent);
	// should not be called when HasMetaAreas == false since it's a waste of performance
	ensure(HasMetaAreas() == true);

	UObject* ObjectOwner = WeakOwnerPtr.Get();
	if (ObjectOwner == NULL)
	{
		return Result;
	}

	const AActor* ActorOwner = Cast<AActor>(ObjectOwner) ? (AActor*)ObjectOwner : Cast<AActor>(ObjectOwner->GetOuter());
	if (ActorOwner == NULL)
	{
		return Result;
	}

	// copy values
	Result = *this;

	{
		FAreaNavModifier* Area = Result.Areas.GetData();
		for (int32 Index = 0; Index < Result.Areas.Num(); ++Index, ++Area)
		{
			if (Area->HasMetaAreas())
			{
				Area->SetAreaClass(UNavAreaMeta::PickAreaClass(Area->GetAreaClass(), ActorOwner, *NavAgent));
				Area->SetAreaClassToReplace(UNavAreaMeta::PickAreaClass(Area->GetAreaClassToReplace(), ActorOwner, *NavAgent));
			}
		}
	}

	{
		FSimpleLinkNavModifier* SimpleLink = Result.SimpleLinks.GetData();
		for (int32 Index = 0; Index < Result.SimpleLinks.Num(); ++Index, ++SimpleLink)
		{
			if (SimpleLink->HasMetaAreas())
			{
				for (int32 LinkIndex = 0; LinkIndex < SimpleLink->Links.Num(); ++LinkIndex)
				{
					FNavigationLink& Link = SimpleLink->Links[LinkIndex];
					Link.SetAreaClass(UNavAreaMeta::PickAreaClass(Link.GetAreaClass(), ActorOwner, *NavAgent));
				}

				for (int32 LinkIndex = 0; LinkIndex < SimpleLink->SegmentLinks.Num(); ++LinkIndex)
				{
					FNavigationSegmentLink& Link = SimpleLink->SegmentLinks[LinkIndex];
					Link.SetAreaClass(UNavAreaMeta::PickAreaClass(Link.GetAreaClass(), ActorOwner, *NavAgent));
				}
			}
		}
	}

	{
		// create new entry in CustomLinks, and put there all regular links that have meta area class
		// making plain FNavigationLink from them first
		Result.SimpleLinks.Reserve(Result.CustomLinks.Num() + Result.SimpleLinks.Num());
		
		for (int32 Index = Result.CustomLinks.Num() - 1; Index >= 0; --Index)
		{
			FCustomLinkNavModifier* CustomLink = &Result.CustomLinks[Index];
			if (CustomLink->HasMetaAreas())
			{
				const TArray<FNavigationLink>& Links = UNavLinkDefinition::GetLinksDefinition(CustomLink->GetNavLinkClass());

				FSimpleLinkNavModifier& SimpleLink = Result.SimpleLinks[Result.SimpleLinks.AddZeroed(1)];
				SimpleLink.LocalToWorld = CustomLink->LocalToWorld;
				SimpleLink.Links.Reserve(Links.Num());

				// and copy all links to FCompositeNavMeshModifier::CustomLinks 
				// updating AreaClass if it's meta area
				for (int32 LinkIndex = 0; LinkIndex < Links.Num(); ++LinkIndex)
				{
					const int32 AddedIdx = SimpleLink.Links.Add(Links[LinkIndex]);
					FNavigationLink& NavLink = SimpleLink.Links[AddedIdx];
					NavLink.SetAreaClass(UNavAreaMeta::PickAreaClass(NavLink.GetAreaClass(), ActorOwner, *NavAgent));
				}				

				const TArray<FNavigationSegmentLink>& SegmentLinks = UNavLinkDefinition::GetSegmentLinksDefinition(CustomLink->GetNavLinkClass());

				FSimpleLinkNavModifier& SimpleSegLink = Result.SimpleLinks[Result.SimpleLinks.AddZeroed(1)];
				SimpleSegLink.LocalToWorld = CustomLink->LocalToWorld;
				SimpleSegLink.SegmentLinks.Reserve(SegmentLinks.Num());

				// and copy all links to FCompositeNavMeshModifier::CustomLinks 
				// updating AreaClass if it's meta area
				for (int32 LinkIndex = 0; LinkIndex < SegmentLinks.Num(); ++LinkIndex)
				{
					const int32 AddedIdx = SimpleSegLink.SegmentLinks.Add(SegmentLinks[LinkIndex]);
					FNavigationSegmentLink& NavLink = SimpleSegLink.SegmentLinks[AddedIdx];
					NavLink.SetAreaClass(UNavAreaMeta::PickAreaClass(NavLink.GetAreaClass(), ActorOwner, *NavAgent));
				}

				Result.CustomLinks.RemoveAtSwap(Index, 1, false);
			}
		}
	}

	return Result;
}

void FCompositeNavModifier::CreateAreaModifiers(const UPrimitiveComponent* PrimComp, const TSubclassOf<UNavArea> AreaClass)
{
	UBodySetup* BodySetup = PrimComp ? ((UPrimitiveComponent*)PrimComp)->GetBodySetup() : nullptr;
	if (BodySetup == nullptr)
	{
		return;
	}

	for (int32 Idx = 0; Idx < BodySetup->AggGeom.BoxElems.Num(); Idx++)
	{
		const FKBoxElem& BoxElem = BodySetup->AggGeom.BoxElems[Idx];
		const FBox BoxSize = BoxElem.CalcAABB(FTransform::Identity, 1.0f);

		FAreaNavModifier AreaMod(BoxSize, PrimComp->GetComponentTransform(), AreaClass);
		Add(AreaMod);
	}

	for (int32 Idx = 0; Idx < BodySetup->AggGeom.SphylElems.Num(); Idx++)
	{
		const FKSphylElem& SphylElem = BodySetup->AggGeom.SphylElems[Idx];
		const FTransform AreaOffset(FVector(0, 0, -SphylElem.Length));

		FAreaNavModifier AreaMod(SphylElem.Radius, SphylElem.Length * 2.0f, AreaOffset * PrimComp->GetComponentTransform(), AreaClass);
		Add(AreaMod);
	}

	for (int32 Idx = 0; Idx < BodySetup->AggGeom.ConvexElems.Num(); Idx++)
	{
		const FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems[Idx];
		
		FAreaNavModifier AreaMod(ConvexElem.VertexData, 0, ConvexElem.VertexData.Num(), ENavigationCoordSystem::Unreal, PrimComp->GetComponentTransform(), AreaClass);
		Add(AreaMod);
	}
	
	for (int32 Idx = 0; Idx < BodySetup->AggGeom.SphereElems.Num(); Idx++)
	{
		const FKSphereElem& SphereElem = BodySetup->AggGeom.SphereElems[Idx];
		const FTransform AreaOffset(FVector(0, 0, -SphereElem.Radius));

		FAreaNavModifier AreaMod(SphereElem.Radius, SphereElem.Radius * 2.0f, AreaOffset * PrimComp->GetComponentTransform(), AreaClass);
		Add(AreaMod);
	}
}

uint32 FCompositeNavModifier::GetAllocatedSize() const
{
	uint32 MemUsed = Areas.GetAllocatedSize() + SimpleLinks.GetAllocatedSize() + CustomLinks.GetAllocatedSize();

	const FSimpleLinkNavModifier* SimpleLink = SimpleLinks.GetData();
	for (int32 Index = 0; Index < SimpleLinks.Num(); ++Index, ++SimpleLink)
	{
		MemUsed += SimpleLink->Links.GetAllocatedSize();
	}

	return MemUsed;
}

bool FCompositeNavModifier::HasPerInstanceTransforms() const
{
	return NavDataPerInstanceTransformDelegate.IsBound();
}

//----------------------------------------------------------------------//
// UNavLinkTrivial
//----------------------------------------------------------------------//
UNavLinkTrivial::UNavLinkTrivial(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	FNavigationLink& Link = Links[Links.Add(FNavigationLink(FVector(0,100,0), FVector(0,-100,0)))];
}
