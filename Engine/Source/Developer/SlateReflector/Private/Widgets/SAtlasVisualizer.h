// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Rendering/RenderingCommon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

class ISlateAtlasProvider;
class SAtlasVisualizerPanel;

class SAtlasVisualizer : public ISlateViewport, public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAtlasVisualizer )
		: _AtlasProvider()
		{}

		SLATE_ARGUMENT( ISlateAtlasProvider*, AtlasProvider )

	SLATE_END_ARGS()

	// ISlateViewport
	virtual FIntPoint GetSize() const override;
	virtual bool RequiresVsync() const override;
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	bool IsViewportTextureAlphaOnly() const override;

	void Construct( const FArguments& InArgs );

protected:
	FText GetZoomLevelPercentText() const;
	void OnFitToWindowStateChanged( ECheckBoxState NewState );
	ECheckBoxState OnGetFitToWindowState() const;
	FReply OnActualSizeClicked();
	void OnDisplayCheckerboardStateChanged( ECheckBoxState NewState );
	ECheckBoxState OnGetCheckerboardState() const;
	EVisibility OnGetCheckerboardVisibility() const;
	void OnComboOpening();
	FText OnGetSelectedItemText() const;
	void OnAtlasPageChanged( TSharedPtr<int32> AtlasPage, ESelectInfo::Type SelectionType );
	TSharedRef<SWidget> OnGenerateWidgetForCombo( TSharedPtr<int32> AtlasPage );

	ISlateAtlasProvider* AtlasProvider;
	TSharedPtr<SComboBox<TSharedPtr<int32>>> AtlasPageCombo;
	TArray<TSharedPtr<int32>> AtlasPages;
	TSharedPtr<SAtlasVisualizerPanel> ScrollPanel;
	int32 SelectedAtlasPage;
	bool bDisplayCheckerboard;
};
