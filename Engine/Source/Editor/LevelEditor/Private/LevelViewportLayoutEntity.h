// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "LevelViewportLayout.h"

class FLevelEditorViewportClient;
class SLevelViewport;

class LEVELEDITOR_API FLevelViewportLayoutEntity : public IViewportLayoutEntity
{
public:

	FLevelViewportLayoutEntity(const FViewportConstructionArgs& ConstructionArgs);
	virtual TSharedRef<SWidget> AsWidget() const override;
	virtual TSharedPtr<SLevelViewport> AsLevelViewport() const override;

	bool IsPlayInEditorViewportActive() const;
	void RegisterGameViewportIfPIE();
	void SetKeyboardFocus();
	void OnLayoutDestroyed();
	void SaveConfig(const FString& ConfigSection);
	FLevelEditorViewportClient& GetLevelViewportClient() const;
	FName GetType() const;

protected:

	/** This entity's level viewport */
	TSharedRef<SLevelViewport> LevelViewport;
};
