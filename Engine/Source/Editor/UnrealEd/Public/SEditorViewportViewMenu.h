// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/SWidget.h"
#include "SEditorViewport.h"
#include "SEditorViewportToolBarMenu.h"

struct FSlateBrush;

class UNREALED_API SEditorViewportViewMenu : public SEditorViewportToolbarMenu
{
public:
	SLATE_BEGIN_ARGS( SEditorViewportViewMenu ){}
		SLATE_ARGUMENT( TSharedPtr<class FExtender>, MenuExtenders )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<class SViewportToolBar> InParentToolBar );

private:
	FText GetViewMenuLabel() const;
	const FSlateBrush* GetViewMenuLabelIcon() const;

protected:
	virtual TSharedRef<SWidget> GenerateViewMenuContent() const;
	TWeakPtr<SEditorViewport> Viewport;

private:
	TSharedPtr<class FExtender> MenuExtenders;
};
