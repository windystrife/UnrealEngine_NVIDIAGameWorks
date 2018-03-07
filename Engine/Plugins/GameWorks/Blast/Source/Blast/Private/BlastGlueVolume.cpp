#include "BlastGlueVolume.h"
#include "BlastMeshComponent.h"
#include "BlastGlobals.h"
#include "Engine/CollisionProfile.h"
#include "Components/BrushComponent.h"

ABlastGlueVolume::ABlastGlueVolume(const FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer),
	bEnabled(true),
	GlueVector(FVector::ForwardVector)
{
#if WITH_EDITORONLY_DATA
	GlueVectorComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(GET_MEMBER_NAME_CHECKED(ABlastGlueVolume, GlueVectorComponent));

	if (GlueVectorComponent)
	{
		GlueVectorComponent->ArrowColor = FColor(150, 200, 255);
		GlueVectorComponent->bTreatAsASprite = true;
		GlueVectorComponent->SpriteInfo.Category = FName("Blast");
		GlueVectorComponent->SpriteInfo.DisplayName = NSLOCTEXT("Blast", "GlueArrow", "GlueArrow");
		GlueVectorComponent->SetupAttachment(RootComponent);
		GlueVectorComponent->bIsScreenSizeScaled = true;
		GlueVectorComponent->bUseInEditorScaling = true;

		GlueVectorComponent->SetWorldRotation(GlueVector.Rotation());
	}
#endif
	bCanBeDamaged = false;
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void ABlastGlueVolume::PostActorCreated()
{
	Super::PostActorCreated();
#if WITH_EDITOR
	UBlastGlueWorldTag* WorldTag = UBlastGlueWorldTag::GetForWorld(GetWorld());
	if (WorldTag)
	{
		WorldTag->GlueVolumes.AddUnique(this);
	}
	//Creating a new one invalidates the data
	InvalidateGlueData();
#endif
}

void ABlastGlueVolume::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	UBlastGlueWorldTag* WorldTag = UBlastGlueWorldTag::GetForWorld(GetWorld());
	if (WorldTag)
	{
		WorldTag->GlueVolumes.AddUnique(this);
	}
#endif
}

void ABlastGlueVolume::Destroyed()
{
	Super::Destroyed();
#if WITH_EDITOR
	InvalidateGlueData();
	UBlastGlueWorldTag* WorldTag = UBlastGlueWorldTag::GetForWorld(GetWorld());
	if (WorldTag)
	{
		WorldTag->GlueVolumes.RemoveSwap(this);
		if (WorldTag->SupportStructures.Num() == 0 && WorldTag->GlueVolumes.Num() == 0)
		{
			WorldTag->bIsDirty = false;
		}
	}
#endif
}

#if WITH_EDITOR

void ABlastGlueVolume::UpdateArrowVector()
{
	GlueVectorComponent->SetWorldRotation(GlueVector.Rotation());
}

void ABlastGlueVolume::PostEditMove(bool bFinished)
{
	if (bFinished)
	{
		UpdateArrowVector();
		InvalidateGlueData();
	}

	Super::PostEditMove(bFinished);
}

void ABlastGlueVolume::InvalidateGlueData()
{
	UBlastGlueWorldTag::SetDirty(GetWorld());
	for (UBlastMeshComponent* Component : GluedComponents)
	{
		if (Component)
		{
			Component->SetModifiedAsset(nullptr);
		}
	}
	GluedComponents.Reset();
}


static FName NAME_GlueVector = GET_MEMBER_NAME_CHECKED(ABlastGlueVolume, GlueVector);
static FName NAME_RelativeLocation = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation);
static FName NAME_RelativeRotation = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation);
static FName NAME_RelativeScale3D = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D);

void ABlastGlueVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PropertyName == NAME_GlueVector)
	{
		// Make sure vector is normalized
		GlueVector.Normalize();

		if (GlueVector.IsNearlyZero())
		{
			UE_LOG(LogBlast, Warning, TEXT("GlueVector was set to 0, so corrected to a forward vector."));
			GlueVector = FVector::ForwardVector;
		}

		UpdateArrowVector();
		InvalidateGlueData();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ABlastGlueVolume, bEnabled) ||
			PropertyName == NAME_RelativeLocation ||
			PropertyName == NAME_RelativeRotation ||
			PropertyName == NAME_RelativeScale3D)
	{
		InvalidateGlueData();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

UBlastGlueWorldTag* UBlastGlueWorldTag::GetForWorld(UWorld* World)
{
	UBlastGlueWorldTag* Ret = nullptr;
	if (World)
	{
		if (!World->PerModuleDataObjects.FindItemByClass<UBlastGlueWorldTag>(&Ret))
		{
			Ret = NewObject<UBlastGlueWorldTag>(World);
			World->PerModuleDataObjects.Add(Ret);
		}
	}
	return Ret;
}

#endif


