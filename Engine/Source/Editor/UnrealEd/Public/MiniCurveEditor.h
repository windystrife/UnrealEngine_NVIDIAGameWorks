// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/AssetEditorManager.h"

class FCurveOwnerInterface;
class SCurveEditor;

class UNREALED_API SMiniCurveEditor :  public SCompoundWidget,public IAssetEditorInstance
{
public:
	SLATE_BEGIN_ARGS( SMiniCurveEditor )
		: _CurveOwner(nullptr)
		, _OwnerObject(nullptr)
		{}

	SLATE_ARGUMENT( FCurveOwnerInterface*, CurveOwner )
	SLATE_ARGUMENT( UObject*, OwnerObject )
	SLATE_ARGUMENT( TWeakPtr<SWindow>, ParentWindow )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMiniCurveEditor();

	// IAssetEditorInstance interface
	virtual FName GetEditorName() const override;
	virtual void FocusWindow(UObject* ObjectToFocusOn) override;
	virtual bool CloseWindow() override;
	virtual bool IsPrimaryEditor() const override { return true; };
	virtual void InvokeTab(const struct FTabId& TabId) override {}
	virtual TSharedPtr<class FTabManager> GetAssociatedTabManager() override;
	virtual double GetLastActivationTime() override;
	virtual void RemoveEditingAsset(UObject* Asset) override;

private:

	float ViewMinInput;
	float ViewMaxInput;

	TSharedPtr<class SCurveEditor> TrackWidget;

	float GetViewMinInput() const { return ViewMinInput; }
	float GetViewMaxInput() const { return ViewMaxInput; }
	/** Return length of timeline */
	float GetTimelineLength() const;

	void SetInputViewRange(float InViewMinInput, float InViewMaxInput);


protected:
	TWeakPtr<SWindow> WidgetWindow;
};
