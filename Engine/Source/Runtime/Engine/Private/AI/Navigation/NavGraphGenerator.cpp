// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavGraphGenerator.h"

//----------------------------------------------------------------------//
// FNavGraphGenerator
//----------------------------------------------------------------------//
FNavGraphGenerator::FNavGraphGenerator(ANavigationGraph* InDestNavGraph)
{

}

FNavGraphGenerator::~FNavGraphGenerator()
{

}

//----------------------------------------------------------------------//
// FNavDataGenerator overrides
//----------------------------------------------------------------------//
bool FNavGraphGenerator::IsBuildInProgress(bool bCheckDirtyToo) const
{
	return false;
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
void FNavGraphGenerator::Init()
{

}

void FNavGraphGenerator::CleanUpIntermediateData()
{

}

void FNavGraphGenerator::UpdateBuilding()
{

}

