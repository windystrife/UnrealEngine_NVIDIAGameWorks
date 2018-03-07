// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MorphMesh.cpp: Unreal morph target mesh and blending implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "EngineUtils.h"
#include "Animation/MorphTarget.h"
#include "HAL/LowLevelMemTracker.h"

//////////////////////////////////////////////////////////////////////////

UMorphTarget::UMorphTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UMorphTarget::Serialize( FArchive& Ar )
{
	LLM_SCOPE(ELLMTag::Animation);
	
	Super::Serialize( Ar );
	FStripDataFlags StripFlags( Ar );
	if( !StripFlags.IsDataStrippedForServer() )
	{
		Ar << MorphLODModels;
	}
}


void UMorphTarget::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	for (const auto& LODModel : MorphLODModels)
	{
		LODModel.GetResourceSizeEx(CumulativeResourceSize);
	}
}


//////////////////////////////////////////////////////////////////////
SIZE_T FMorphTargetLODModel::GetResourceSize() const
{
	return GetResourceSizeBytes();
}

void FMorphTargetLODModel::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
	CumulativeResourceSize.AddUnknownMemoryBytes(Vertices.GetAllocatedSize() + sizeof(int32));
}

SIZE_T FMorphTargetLODModel::GetResourceSizeBytes() const
{
	FResourceSizeEx ResSize;
	GetResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}
