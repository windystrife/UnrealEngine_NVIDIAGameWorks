// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class FEdModeLandscape;
class IDetailLayoutBuilder;

class FLandscapeSplineDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FEdModeLandscape* GetEditorMode() const;
	FReply OnSelectConnectedControlPointsButtonClicked();
	FReply OnSelectConnectedSegmentsButtonClicked();
	FReply OnMoveToCurrentLevelButtonClicked();
	bool IsMoveToCurrentLevelButtonEnabled() const;
};
