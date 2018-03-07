// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FFontEditorViewportClient;
class FSceneViewport;
class IFontEditor;
class SScrollBar;
class SViewport;

/*-----------------------------------------------------------------------------
   SFontEditorViewport
-----------------------------------------------------------------------------*/

class SFontEditorViewport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SFontEditorViewport )
		: _IsPreview(false)
		{}

		SLATE_ARGUMENT(TWeakPtr<IFontEditor>, FontEditor)
		SLATE_ARGUMENT(bool, IsPreview)
	SLATE_END_ARGS()

	/** Destructor */
	~SFontEditorViewport();

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs);
	
	/** Refreshes the viewport */
	void RefreshViewport();

	/** Accessors */
	int32 GetCurrentSelectedPage() const;
	void SetPreviewText(const FText& PreviewText);
	void SetPreviewBackgroundColor(const FColor& BackgroundColor);
	const FColor& GetPreviewBackgroundColor() const;
	void SetPreviewForegroundColor(const FColor& ForgroundColor);
	const FColor& GetPreviewForegroundColor() const;
	void SetPreviewFontMetrics(const bool InDrawFontMetrics);
	bool GetPreviewFontMetrics() const;
	TWeakPtr<IFontEditor> GetFontEditor() const;
	bool IsPreviewViewport() const;
	TSharedPtr<FSceneViewport> GetViewport() const;
	TSharedPtr<SViewport> GetViewportWidget() const;
	TSharedPtr<SScrollBar> GetVerticalScrollBar() const;
	TSharedPtr<SScrollBar> GetHorizontalScrollBar() const;

protected:
	/** Returns the visibility of the viewport scrollbars */
	EVisibility GetViewportVerticalScrollBarVisibility() const;
	EVisibility GetViewportHorizontalScrollBarVisibility() const;

	/** Called when the viewport scrollbars are scrolled */
	void OnViewportVerticalScrollBarScrolled(float InScrollOffsetFraction);
	void OnViewportHorizontalScrollBarScrolled(float InScrollOffsetFraction);

private:
	/** Pointer back to the Font editor tool that owns us */
	TWeakPtr<IFontEditor> FontEditorPtr;

	/** If true, this is a viewport for the font editor's preview tab */
	bool bIsPreview;
	
	/** Level viewport client */
	TSharedPtr<FFontEditorViewportClient> ViewportClient;

	/** Slate viewport for rendering and I/O */
	TSharedPtr<FSceneViewport> Viewport;

	/** Viewport widget*/
	TSharedPtr<SViewport> ViewportWidget;

	/** Vertical scrollbar */
	TSharedPtr<SScrollBar> FontViewportVerticalScrollBar;

	/** Horizontal scrollbar */
	TSharedPtr<SScrollBar> FontViewportHorizontalScrollBar;
};
