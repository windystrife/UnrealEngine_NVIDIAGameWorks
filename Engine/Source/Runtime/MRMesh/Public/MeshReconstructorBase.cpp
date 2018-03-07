// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshReconstructorBase.h"


void UMeshReconstructorBase::StartReconstruction()
{
}

void UMeshReconstructorBase::StopReconstruction()
{
}

void UMeshReconstructorBase::PauseReconstruction()
{
}

bool UMeshReconstructorBase::IsReconstructionStarted() const
{
	return false;
}

bool UMeshReconstructorBase::IsReconstructionPaused() const
{
	return false;
}

FMRMeshConfiguration UMeshReconstructorBase::ConnectMRMesh(class UMRMeshComponent* Mesh)
{
	return FMRMeshConfiguration();
}

void UMeshReconstructorBase::DisconnectMRMesh()
{
}