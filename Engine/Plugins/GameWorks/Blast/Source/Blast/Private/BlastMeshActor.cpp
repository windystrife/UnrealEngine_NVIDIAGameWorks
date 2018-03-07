#include "BlastMeshActor.h"
#include "BlastExtendedSupport.h"

#define LOCTEXT_NAMESPACE "Blast"

ABlastMeshActor::ABlastMeshActor()
{
	BlastMeshComponent = CreateDefaultSubobject<UBlastMeshComponent>(GET_MEMBER_NAME_CHECKED(ABlastMeshActor, BlastMeshComponent));
}

#if WITH_EDITOR

void ABlastMeshActor::Destroyed()
{
	// See if we're part of an extended support structures, and if so, remove ourselves from it
	UBlastMeshComponent* C = GetBlastMeshComponent();

	if (C != nullptr)
	{
		ABlastExtendedSupportStructure* OSS = C->GetOwningSupportStructure();
		if (OSS != nullptr)
		{
			C->SetOwningSuppportStructure(nullptr, INDEX_NONE);
			OSS->RemoveStructureActor(this);
		}
	}

	Super::Destroyed();
}


bool ABlastMeshActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (BlastMeshComponent && BlastMeshComponent->GetBlastMesh())
	{
		Objects.Add(BlastMeshComponent->GetBlastMesh());
	}
	return true;
}

#endif

#undef LOCTEXT_NAMESPACE