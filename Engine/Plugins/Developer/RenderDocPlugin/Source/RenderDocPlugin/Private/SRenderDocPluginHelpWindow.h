// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SWindow.h"

#if WITH_EDITOR

class SRenderDocPluginHelpWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SRenderDocPluginHelpWindow)
	{
	}
	SLATE_END_ARGS()

	SRenderDocPluginHelpWindow() {}

	/** Widget constructor */
	void Construct(const FArguments& Args);

private:
	FReply Close();
};

#endif
