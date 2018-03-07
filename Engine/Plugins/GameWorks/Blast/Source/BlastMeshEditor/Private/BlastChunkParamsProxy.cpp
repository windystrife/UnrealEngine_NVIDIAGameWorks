// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlastChunkParamsProxy.h"
#include "IBlastMeshEditor.h"
//#include "ApexBlastAssetImport.h"
#include "BlastMesh.h"

UBlastChunkParamsProxy::UBlastChunkParamsProxy(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

void UBlastChunkParamsProxy::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	/*check(BlastMesh != NULL && BlastMesh->FractureSettings != NULL);

	if (BlastMesh->FractureSettings->ChunkParameters.Num() > ChunkIndex)
	{
		BlastMesh->FractureSettings->ChunkParameters[ChunkIndex] = ChunkParams;
	}

	BuildBlastMeshFromFractureSettings(*BlastMesh, NULL);
	BlastMeshEditorPtr.Pin()->RefreshViewport();
	*/
}
