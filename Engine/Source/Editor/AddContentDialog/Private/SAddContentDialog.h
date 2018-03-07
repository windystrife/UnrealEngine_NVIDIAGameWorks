// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"

class SAddContentWidget;

/** A window which allows the user to select additional content to add to the currently loaded project. */
class SAddContentDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SWindow)
	{}

	SLATE_END_ARGS()

	~SAddContentDialog();

	void Construct(const FArguments& InArgs);

private:
	/** The widget representing available content and which content the user has selected. */
	TSharedPtr<SAddContentWidget> AddContentWidget;
};
