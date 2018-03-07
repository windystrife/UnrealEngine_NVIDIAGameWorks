#pragma once

#include "CoreMinimal.h"
#include "BlastMeshComponent.h"
#include "GameFramework/Actor.h"
#include "BlastMeshActor.generated.h"

//ABlastMeshActor is used to support GetReferencedContentObjects functionality. Code should not assume that UBlastMeshComponents are in ABlastMeshActors
UCLASS(Blueprintable, ComponentWrapperClass, ConversionRoot, meta = (ChildCanTick))
class BLAST_API ABlastMeshActor : public AActor
{
	GENERATED_BODY()
public:
	ABlastMeshActor();

	inline UBlastMeshComponent* GetBlastMeshComponent() const { return BlastMeshComponent; }

#if WITH_EDITOR
	//~ Begin AActor Interface
	virtual void Destroyed() override;

	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	//~ End AActor Interface
#endif

protected:
	UPROPERTY(Category = BlastMeshActor, VisibleAnywhere, BlueprintReadOnly)
	UBlastMeshComponent*	BlastMeshComponent;
};
