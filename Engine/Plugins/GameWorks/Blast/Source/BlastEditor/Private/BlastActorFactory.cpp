#include "BlastActorFactory.h"
#include "BlastMesh.h"
#include "BlastMeshActor.h"
#include "BlastMeshComponent.h"

#define LOCTEXT_NAMESPACE "Blast"

/*-----------------------------------------------------------------------------
UActorFactoryBlastMesh
-----------------------------------------------------------------------------*/
UActorFactoryBlastMesh::UActorFactoryBlastMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("BlastMeshDisplayName", "Blast Mesh");
	NewActorClass = ABlastMeshActor::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UActorFactoryBlastMesh::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() ||
		(!AssetData.GetClass()->IsChildOf(UBlastMesh::StaticClass()))
		)
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoBlastMesh", "A valid blast mesh must be specified.");
		return false;
	}

	return true;
}

UObject* UActorFactoryBlastMesh::GetAssetFromActorInstance(AActor* ActorInstance)
{
	if (ABlastMeshActor* BlastActorInstance = Cast<ABlastMeshActor>(ActorInstance))
	{
		return BlastActorInstance->GetBlastMeshComponent()->GetBlastMesh();
	}
	return nullptr;
}

void UActorFactoryBlastMesh::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UBlastMesh* BlastMesh = Cast<UBlastMesh>(Asset);
	UBlastMeshComponent* BlastComponent = Cast<ABlastMeshActor>(NewActor)->GetBlastMeshComponent();
	// Change properties
	BlastComponent->SetBlastMesh(BlastMesh);
}

void UActorFactoryBlastMesh::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	Super::PostCreateBlueprint(Asset, CDO);

	if (Asset != NULL && CDO != NULL)
	{
		UBlastMesh* BlastMesh = Cast<UBlastMesh>(Asset);
		UBlastMeshComponent* BlastComponent = Cast<ABlastMeshActor>(CDO)->GetBlastMeshComponent();
		// Change properties
		BlastComponent->SetBlastMesh(BlastMesh);
	}
}

#undef LOCTEXT_NAMESPACE