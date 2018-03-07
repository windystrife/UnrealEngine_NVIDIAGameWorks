// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CodeProject.h"
#include "Misc/Paths.h"


UCodeProject::UCodeProject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Path = FPaths::GameSourceDir();
}
