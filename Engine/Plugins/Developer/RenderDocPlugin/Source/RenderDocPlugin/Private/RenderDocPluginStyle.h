// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "ISlateStyle.h"

class FRenderDocPluginStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static TSharedPtr<class ISlateStyle> Get();

private:
	static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);

private:
	static TSharedPtr<class FSlateStyleSet> StyleSet;
};

#endif
