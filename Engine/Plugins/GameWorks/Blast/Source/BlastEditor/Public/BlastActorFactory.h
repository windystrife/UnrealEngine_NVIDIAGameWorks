#pragma once
#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "BlastActorFactory.generated.h"

class AActor;
struct FAssetData;

UCLASS(MinimalAPI, config = Editor)
class UActorFactoryBlastMesh : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
protected:
	//~ Begin UActorFactory Interface
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual void PostCreateBlueprint(UObject* Asset, AActor* CDO) override;
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	//~ End UActorFactory Interface

};
