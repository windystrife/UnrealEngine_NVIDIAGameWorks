// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


/*-----------------------------------------------------------------------------
   SCascadePreviewViewportToolBar
-----------------------------------------------------------------------------*/

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Editor/UnrealEd/Public/SViewportToolBar.h"

class FCascade;

class SCascadePreviewViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SCascadePreviewViewportToolBar){}
		SLATE_ARGUMENT(TWeakPtr<FCascade>, CascadePtr)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Generates the toolbar view menu content */
	TSharedRef<SWidget> GenerateViewMenu() const;

	/** Generates the toolbar time menu content */
	TSharedRef<SWidget> GenerateTimeMenu() const;

private:
	/** The viewport that we are in */
	TWeakPtr<FCascade> CascadePtr;
};
