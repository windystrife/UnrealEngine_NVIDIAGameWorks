// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelViewportLayout.h"
#include "SLevelViewport.h"
#include "CinematicViewport/SCinematicLevelViewport.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"

class FCinematicViewportLayoutEntity : public IViewportLayoutEntity
{
public:
	FCinematicViewportLayoutEntity(const FViewportConstructionArgs& Args)
		: Widget(
			SNew(SCinematicLevelViewport)
			.ParentLayout(Args.ParentLayout)
			.ParentLevelEditor(Args.ParentLevelEditor)
			.LayoutName(*Args.ConfigKey)
			)
	{}

	virtual TSharedRef<SWidget> AsWidget() const override { return Widget; }
	virtual TSharedPtr<SLevelViewport> AsLevelViewport() const { return Widget->GetLevelViewport(); }

	FName GetType() const
	{
		static FName CinematicName("Cinematic");
		return CinematicName;
	}

	FLevelEditorViewportClient& GetLevelViewportClient() const { return Widget->GetLevelViewport()->GetLevelViewportClient(); }
	bool IsPlayInEditorViewportActive() const { return Widget->GetLevelViewport()->IsPlayInEditorViewportActive(); }
	void RegisterGameViewportIfPIE(){ return Widget->GetLevelViewport()->RegisterGameViewportIfPIE(); }
	void SetKeyboardFocus(){ FSlateApplication::Get().SetKeyboardFocus(Widget->GetLevelViewport()); }
	void OnLayoutDestroyed()
	{
		if (IsPlayInEditorViewportActive() || GetLevelViewportClient().IsSimulateInEditorViewport() )
		{
			GUnrealEd->EndPlayMap();
		}
	}
	void SaveConfig(const FString& ConfigString){ return Widget->GetLevelViewport()->SaveConfig(ConfigString); }

protected:

	/** This entity's widget */
	TSharedRef<SCinematicLevelViewport> Widget;
};
