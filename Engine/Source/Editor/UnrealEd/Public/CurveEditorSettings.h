// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "CurveEditorSettings.generated.h"

/** Defines visibility states for the curves in the curve editor. */
UENUM()
namespace ECurveEditorCurveVisibility
{
	enum Type
	{
		/** All curves should be visible. */
		AllCurves,
		/** Only curves from selected nodes should be visible. */
		SelectedCurves,
		/** Only curves which have keyframes should be visible. */
		AnimatedCurves
	};
}

/** Defines visibility states for the tangents in the curve editor. */
UENUM()
namespace ECurveEditorTangentVisibility
{
	enum Type
	{
		/** All tangents should be visible. */
		AllTangents,
		/** Only tangents from selected keys should be visible. */
		SelectedKeys,
		/** Don't display tangents. */
		NoTangents
	};
}

/** Serializable options for curve editor. */
UCLASS(config=EditorPerProjectUserSettings)
class UNREALED_API UCurveEditorSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	DECLARE_MULTICAST_DELEGATE( FOnCurveEditorCurveVisibilityChanged );

	/** Gets whether or not the curve editor auto frames the selected curves. */
	bool GetAutoFrameCurveEditor() const;
	/** Sets whether or not the curve editor auto frames the selected curves. */
	void SetAutoFrameCurveEditor(bool InbAutoFrameCurveEditor);

	/** Gets whether or not to show curve tool tips in the curve editor. */
	bool GetShowCurveEditorCurveToolTips() const;
	/** Sets whether or not to show curve tool tips in the curve editor. */
	void SetShowCurveEditorCurveToolTips(bool InbShowCurveEditorCurveToolTips);

	/** Gets the current curve visibility. */
	ECurveEditorCurveVisibility::Type GetCurveVisibility() const;
	/** Sets the current curve visibility. */
	void SetCurveVisibility(ECurveEditorCurveVisibility::Type InCurveVisibility);

	/** Gets the current tangent visibility. */
	ECurveEditorTangentVisibility::Type GetTangentVisibility() const;
	/** Sets the current tangent visibility. */
	void SetTangentVisibility(ECurveEditorTangentVisibility::Type InTangentVisibility);

	/** Gets the multicast delegate which is run whenever the curve editor curve visibility changes. */
	FOnCurveEditorCurveVisibilityChanged& GetOnCurveEditorCurveVisibilityChanged();

protected:
	UPROPERTY( config, EditAnywhere, Category="Curve Editor" )
	bool bAutoFrameCurveEditor;

	UPROPERTY( config, EditAnywhere, Category="Curve Editor" )
	bool bShowCurveEditorCurveToolTips;

	UPROPERTY( config, EditAnywhere, Category="Curve Editor" )
	TEnumAsByte<ECurveEditorCurveVisibility::Type> CurveVisibility;

	UPROPERTY( config, EditAnywhere, Category="Curve Editor" )
	TEnumAsByte<ECurveEditorTangentVisibility::Type> TangentVisibility;

	FOnCurveEditorCurveVisibilityChanged OnCurveEditorCurveVisibilityChanged;
};
