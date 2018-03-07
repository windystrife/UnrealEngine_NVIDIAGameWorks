// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Animation/CurveSequence.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * A widget that shows another widget as long as the mouse isn't hovering over it.
 */
class SDisappearingBar : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDisappearingBar)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}

		/** Slot for this designers content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;

private:
	FLinearColor GetFadeColorAndOpacity() const;

	FCurveSequence FadeCurve;
};
