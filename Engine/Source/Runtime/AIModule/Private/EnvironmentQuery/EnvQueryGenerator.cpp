// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/EnvQueryGenerator.h"

UEnvQueryGenerator::UEnvQueryGenerator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bAutoSortTests = true;
}

void UEnvQueryGenerator::UpdateNodeVersion()
{
	VerNum = EnvQueryGeneratorVersion::Latest;
}

void UEnvQueryGenerator::PostLoad()
{
	Super::PostLoad();
	UpdateNodeVersion();
}
