// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavLinkProxy.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Navigation/NavLinkCustomComponent.h"
#include "AI/Navigation/NavLinkRenderingComponent.h"
#include "AI/Navigation/NavAreas/NavArea_Default.h"
#include "AI/NavigationSystemHelpers.h"
#include "VisualLogger/VisualLogger.h"
#include "AI/NavigationOctree.h"
#include "ObjectEditorUtils.h"

ANavLinkProxy::ANavLinkProxy(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PositionComponent"));
	RootComponent = SceneComponent;

	bHidden = true;

#if WITH_EDITORONLY_DATA
	EdRenderComp = CreateDefaultSubobject<UNavLinkRenderingComponent>(TEXT("EdRenderComp"));
	EdRenderComp->PostPhysicsComponentTick.bCanEverTick = false;
	EdRenderComp->SetupAttachment(RootComponent);
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != NULL))
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture;
			FName ID_Decals;
			FText NAME_Decals;
			FConstructorStatics()
				: SpriteTexture(TEXT("/Engine/EditorResources/AI/S_NavLink"))
				, ID_Decals(TEXT("Navigation"))
				, NAME_Decals(NSLOCTEXT("SpriteCategory", "Navigation", "Navigation"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.SpriteTexture.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->bVisible = true;
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Decals;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Decals;
		SpriteComponent->SetupAttachment(RootComponent);
		SpriteComponent->SetAbsolute(false, false, true);
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif

	SmartLinkComp = CreateDefaultSubobject<UNavLinkCustomComponent>(TEXT("SmartLinkComp"));
	SmartLinkComp->SetNavigationRelevancy(false);
	SmartLinkComp->SetMoveReachedLink(this, &ANavLinkProxy::NotifySmartLinkReached);
	bSmartLinkIsRelevant = false;

	FNavigationLink DefLink;
	DefLink.SetAreaClass(UNavArea_Default::StaticClass());

	PointLinks.Add(DefLink);

	SetActorEnableCollision(false);
	bCanBeDamaged = false;
}

#if WITH_EDITOR
void ANavLinkProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	static const FName NAME_SmartLinkIsRelevant = GET_MEMBER_NAME_CHECKED(ANavLinkProxy, bSmartLinkIsRelevant);
	static const FName NAME_PointLinks = GET_MEMBER_NAME_CHECKED(ANavLinkProxy, PointLinks);
	static const FName NAME_AreaClass = TEXT("AreaClass");
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	bool bUpdateInNavOctree = false;
	if (PropertyName == NAME_SmartLinkIsRelevant)
	{
		SmartLinkComp->SetNavigationRelevancy(bSmartLinkIsRelevant);
		bUpdateInNavOctree = true;
	}

	const FName CategoryName = FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property);
	const FName MemberCategoryName = FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.MemberProperty);
	if (CategoryName == TEXT("SimpleLink") || MemberCategoryName == TEXT("SimpleLink"))
	{
		bUpdateInNavOctree = true;
		if (PropertyName == NAME_AreaClass && MemberPropertyName == NAME_PointLinks)
		{
			for (FNavigationLink& Link : PointLinks)
			{
				Link.InitializeAreaClass(/*bForceRefresh=*/true);
			}
		}
	}

	if (bUpdateInNavOctree)
	{
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		if (NavSys)
		{
			NavSys->UpdateActorInNavOctree(*this);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ANavLinkProxy::PostEditUndo()
{
	Super::PostEditUndo();

	for (FNavigationLink& Link : PointLinks)
	{
		Link.InitializeAreaClass(/*bForceRefresh=*/true);
	}
}

void ANavLinkProxy::PostEditImport()
{
	Super::PostEditImport();

	for (FNavigationLink& Link : PointLinks)
	{
		Link.InitializeAreaClass(/*bForceRefresh=*/true);
	}
}

#endif // WITH_EDITOR

void ANavLinkProxy::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if (SmartLinkComp)
	{
		SmartLinkComp->SetNavigationRelevancy(bSmartLinkIsRelevant);
	}
}

void ANavLinkProxy::PostLoad()
{
	Super::PostLoad();

	if (SmartLinkComp)
	{
		SmartLinkComp->SetNavigationRelevancy(bSmartLinkIsRelevant);
	}
}

#if ENABLE_VISUAL_LOG
void ANavLinkProxy::BeginPlay()
{
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys)
	{
		REDIRECT_OBJECT_TO_VLOG(this, NavSys);
	}

	Super::BeginPlay();
}
#endif // ENABLE_VISUAL_LOG

void ANavLinkProxy::GetNavigationData(FNavigationRelevantData& Data) const
{
	NavigationHelper::ProcessNavLinkAndAppend(&Data.Modifiers, this, PointLinks);
	NavigationHelper::ProcessNavLinkSegmentAndAppend(&Data.Modifiers, this, SegmentLinks);
}

FBox ANavLinkProxy::GetNavigationBounds() const
{
	return GetComponentsBoundingBox();
}

bool ANavLinkProxy::IsNavigationRelevant() const
{
	return (PointLinks.Num() > 0) || (SegmentLinks.Num() > 0) || bSmartLinkIsRelevant;
}

bool ANavLinkProxy::GetNavigationLinksClasses(TArray<TSubclassOf<UNavLinkDefinition> >& OutClasses) const
{
	return false;
}

bool ANavLinkProxy::GetNavigationLinksArray(TArray<FNavigationLink>& OutLink, TArray<FNavigationSegmentLink>& OutSegments) const
{
	OutLink.Append(PointLinks);
	OutSegments.Append(SegmentLinks);

	return (PointLinks.Num() > 0) || (SegmentLinks.Num() > 0);
}

FBox ANavLinkProxy::GetComponentsBoundingBox(bool bNonColliding) const
{
	FBox LinksBB(FVector(0.f, 0.f, -10.f), FVector(0.f,0.f,10.f));

	for (int32 i = 0; i < PointLinks.Num(); ++i)
	{
		const FNavigationLink& Link = PointLinks[i];
		LinksBB += Link.Left;
		LinksBB += Link.Right;
	}

	for (int32 i = 0; i < SegmentLinks.Num(); ++i)
	{
		const FNavigationSegmentLink& SegmentLink = SegmentLinks[i];
		LinksBB += SegmentLink.LeftStart;
		LinksBB += SegmentLink.LeftEnd;
		LinksBB += SegmentLink.RightStart;
		LinksBB += SegmentLink.RightEnd;
	}

	LinksBB = LinksBB.TransformBy(RootComponent->GetComponentTransform());

	if (SmartLinkComp && SmartLinkComp->IsNavigationRelevant())
	{
		LinksBB += SmartLinkComp->GetStartPoint();
		LinksBB += SmartLinkComp->GetEndPoint();
	}

	return LinksBB;
}

void ANavLinkProxy::NotifySmartLinkReached(UNavLinkCustomComponent* LinkComp, UPathFollowingComponent* PathComp, const FVector& DestPoint)
{
	AActor* PathOwner = PathComp->GetOwner();
	AController* ControllerOwner = Cast<AController>(PathOwner);
	if (ControllerOwner)
	{
		PathOwner = ControllerOwner->GetPawn();
	}

	ReceiveSmartLinkReached(PathOwner, DestPoint);
	OnSmartLinkReached.Broadcast(PathOwner, DestPoint);
}

void ANavLinkProxy::ResumePathFollowing(AActor* Agent)
{
	if (Agent)
	{
		UPathFollowingComponent* PathComp = Agent->FindComponentByClass<UPathFollowingComponent>();
		if (PathComp == NULL)
		{
			APawn* PawnOwner = Cast<APawn>(Agent);
			if (PawnOwner && PawnOwner->GetController())
			{
				PathComp = PawnOwner->GetController()->FindComponentByClass<UPathFollowingComponent>();
			}
		}

		if (PathComp)
		{
			PathComp->FinishUsingCustomLink(SmartLinkComp);
		}
	}
}

bool ANavLinkProxy::IsSmartLinkEnabled() const
{
	return SmartLinkComp->IsEnabled();
}

void ANavLinkProxy::SetSmartLinkEnabled(bool bEnabled)
{
	SmartLinkComp->SetEnabled(bEnabled);
}

bool ANavLinkProxy::HasMovingAgents() const
{
	return SmartLinkComp->HasMovingAgents();
}
