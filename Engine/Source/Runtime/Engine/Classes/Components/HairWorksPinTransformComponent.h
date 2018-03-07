// @third party code - BEGIN HairWorks
#pragma once

#include "SceneComponent.h"
#include "HairWorksPinTransformComponent.generated.h"

/**
* HairWorksPinTransformComponent propagates pin transforms to child components.
*/
UCLASS(ClassGroup = Rendering, meta = (BlueprintSpawnableComponent), HideCategories = (Collision, Base, Object, PhysicsVolume))
class ENGINE_API UHairWorksPinTransformComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pin", meta = (ClampMin = "0"))
	int32 PinIndex;

protected:
	//~ Begin USceneComponent Interface
	virtual void OnChildAttached(USceneComponent* ChildComponent) override;
	//~ End USceneComponent Interface
};
// @third party code - END HairWorks