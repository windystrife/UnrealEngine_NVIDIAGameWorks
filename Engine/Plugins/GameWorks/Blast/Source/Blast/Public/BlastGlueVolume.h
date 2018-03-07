#pragma once
#include "CoreMinimal.h"

#include "GameFramework/Volume.h"
#include "Components/ArrowComponent.h"
#include "BlastGlueVolume.generated.h"

//This is a empty tag object added on UWorld::PerModuleDataObjects to mark the glue for the world as dirty.
//This could just be a flag on UWorld like NumLightingUnbuiltObjects, but we can't edit that class
UCLASS(Transient)
class BLAST_API UBlastGlueWorldTag : public UObject
{
	GENERATED_BODY()
public:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bIsDirty;

	UPROPERTY()
	TArray<class ABlastGlueVolume*> GlueVolumes;

	UPROPERTY()
	TArray<class ABlastExtendedSupportStructure*> SupportStructures;
#endif

#if WITH_EDITOR
	static UBlastGlueWorldTag* GetForWorld(UWorld* World);

	static void SetDirty(UWorld* World)
	{
		UBlastGlueWorldTag* Tag = GetForWorld(World);
		if (Tag && Tag->GlueVolumes.Num() > 0)
		{
			Tag->bIsDirty = true;
		}
	}

	static void SetExtendedSupportDirty(UWorld* World)
	{
		UBlastGlueWorldTag* Tag = GetForWorld(World);
		if (Tag && Tag->SupportStructures.Num() > 0)
		{
			Tag->bIsDirty = true;
		}
	}
#endif
};

/*
	This bounding volume causes overlapping Blast chunks to be bound to an invisible chunk in the direction of the GlueVector.

	Any Blast actors that are attached to this invisible chunk will be kinematic, thus gluing them to the "world"

	When the bond to the invisible chunk is broken, the actor will become simulated.
*/
UCLASS(ClassGroup=Blast, Blueprintable)
class BLAST_API ABlastGlueVolume : public AVolume
{
	GENERATED_BODY()
public:
	ABlastGlueVolume(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast")
	bool bEnabled;

	// This vector represents the direction of the invisible chunk that the glued Blast chunks are glued to.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast")
	FVector GlueVector;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UArrowComponent* GlueVectorComponent;

	//These are used to invalidate the components after we are edited
	UPROPERTY(DuplicateTransient)
	TSet<class UBlastMeshComponent*> GluedComponents;
#endif

	virtual void PostActorCreated() override;
	//PostActorCreated is only called when spawing a new actor, not loading one from disk
	virtual void PostLoad() override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;

	void InvalidateGlueData();

private:
	void UpdateArrowVector();
#endif
};
