// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Widgets/SOverlay.h"
#include "Engine/GameViewportClient.h"
#include "SGraphNodeK2Default.h"

class SGraphPin;

class GRAPHEDITOR_API SGraphNodeK2Event : public SGraphNodeK2Default
{
public:
	SGraphNodeK2Event() : SGraphNodeK2Default(), bHasDelegateOutputPin(false) {}

protected:
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle) override;
	virtual bool UseLowDetailNodeTitles() const override;
	virtual void AddPin( const TSharedRef<SGraphPin>& PinToAdd ) override;


	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override
	{
		TitleAreaWidget = DefaultTitleAreaWidget;
	}

private:
	bool ParentUseLowDetailNodeTitles() const
	{
		return SGraphNodeK2Default::UseLowDetailNodeTitles();
	}

	EVisibility GetTitleVisibility() const;

	TSharedPtr<SOverlay> TitleAreaWidget;
	bool bHasDelegateOutputPin;
};
