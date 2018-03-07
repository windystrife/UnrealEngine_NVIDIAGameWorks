// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DummyMeshReconstructorModule.h"
#include "BaseMeshReconstructorModule.h"
#include "Modules/ModuleManager.h"
#include "Runnable.h"
#include "PlatformProcess.h"
#include "RunnableThread.h"
#include "ThreadSafeBool.h"
#include "MRMeshComponent.h"
#include "DynamicMeshBuilder.h"
#include "Queue.h"

// Thin wrapper around the running thread that does all the reconstruction.
class FDummyMeshReconstructorModule : public FBaseMeshReconstructorModule
{
};

IMPLEMENT_MODULE(FDummyMeshReconstructorModule, DummyMeshReconstructor);