//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "GameFramework/Actor.h"
THIRD_PARTY_INCLUDES_START
#include "phonon.h"
THIRD_PARTY_INCLUDES_END
#include "PhononScene.generated.h"

class AStaticMeshActor;

UCLASS(HideCategories = (Actor, Advanced, Input, Rendering))
class STEAMAUDIO_API APhononScene : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<uint8> SceneData;
};

namespace SteamAudio
{

#if WITH_EDITOR

	void STEAMAUDIO_API LoadScene(class UWorld* World, IPLhandle* PhononScene, TArray<IPLhandle>* PhononStaticMeshes);

#endif // WITH_EDITOR

	uint32 GetNumTrianglesForStaticMesh(AStaticMeshActor* StaticMeshActor);
	uint32 GetNumTrianglesAtRoot(AActor* RootActor);
}
