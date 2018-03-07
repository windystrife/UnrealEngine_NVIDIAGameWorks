// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;

class STutorialLoading : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STutorialLoading) {}
	
	SLATE_ARGUMENT(TWeakPtr<SWindow>, ContextWindow)
	
	SLATE_END_ARGS()

public:

	/** Widget constructor */
	void Construct(const FArguments& InArgs);

private:
	/** Window where loading visual will be displayed */
	TWeakPtr<SWindow> ContextWindow;
};
