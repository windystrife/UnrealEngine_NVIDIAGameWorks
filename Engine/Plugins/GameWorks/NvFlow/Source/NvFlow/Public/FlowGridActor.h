#pragma once

// NvFlow begin

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "FlowGridActor.generated.h"

UCLASS(MinimalAPI, hidecategories=(Input))
class AFlowGridActor : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = FlowGrid)
	class UFlowGridComponent* FlowGridComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
#endif
};

// NvFlow end