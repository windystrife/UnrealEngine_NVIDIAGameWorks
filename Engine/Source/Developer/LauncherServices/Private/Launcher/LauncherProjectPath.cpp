// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Launcher/LauncherProjectPath.h"
#include "Misc/Paths.h"

FString FLauncherProjectPath::GetProjectName(const FString& ProjectPath)
{
	if (!ProjectPath.IsEmpty())
	{
		return FPaths::GetBaseFilename(ProjectPath);
	}
	return FString();
}

FString FLauncherProjectPath::GetProjectBasePath(const FString& ProjectPath)
{
	if (!ProjectPath.IsEmpty())
	{
		FString OutPath = ProjectPath;
		FPaths::MakePathRelativeTo(OutPath, *FPaths::RootDir());
		return FPaths::GetPath(OutPath);
	}
	return FString();
}
