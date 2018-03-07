// @third party code - BEGIN HairWorks
#include "Components/HairWorksPinTransformComponent.h"

UHairWorksPinTransformComponent::UHairWorksPinTransformComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseAttachParentBound = true;
}

void UHairWorksPinTransformComponent::OnChildAttached(USceneComponent* ChildComponent)
{
	Super::OnChildAttached(ChildComponent);

	ChildComponent->bUseAttachParentBound = true;
}
// @third party code - END HairWorks
