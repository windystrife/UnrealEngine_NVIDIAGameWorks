// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"

class FPaintArgs;
class FSlateWindowElementList;
struct FSlateBrush;

/** Abstract base class that defines how an overlay should be drawn over the viewport */
struct IFilmOverlay
{
	IFilmOverlay() : Tint(FLinearColor::White), bEnabled(false) {}

	virtual ~IFilmOverlay() {};

	/** Get a localized display name that is representative of this overlay */
	virtual FText GetDisplayName() const = 0;

	/** Get a slate thumbnail brush that is representative of this overlay. 36x24 recommended */
	virtual const FSlateBrush* GetThumbnail() const = 0;

	/** Construct a widget that controls this overlay's arbitrary settings. Only used for toggleable overlays. */
	virtual TSharedPtr<SWidget> ConstructSettingsWidget() { return nullptr; }

	/** Paint the overlay */
	virtual void Paint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const = 0;

public:

	/** Get/Set a custom tint for this overlay */
	const FLinearColor& GetTint() const { return Tint; }
	void SetTint(const FLinearColor& InTint) { Tint = InTint; }

	/** Get/Set whether this overlay should be drawn */
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

protected:

	/** Tint to apply to this overlay */
	FLinearColor Tint;

	/** Whether this overlay is enabled or not */
	bool bEnabled;
};

/** A widget that sits on top of a viewport, and draws custom content */
class SFilmOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFilmOverlay){}

		/** User provided array of overlays to draw */
		SLATE_ATTRIBUTE(TArray<IFilmOverlay*>, FilmOverlays)

	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs);

	/** Paint this widget */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** Attribute used once per frame to retrieve the film overlays to paint */
	TAttribute<TArray<IFilmOverlay*>> FilmOverlays;
};

/** A custom widget that comprises a combo box displaying all available overlay options */
class SFilmOverlayOptions : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFilmOverlayOptions){}
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs);

	/** Retrieve the actual overlay widget that this widget controls. Can be positioned in any other widget hierarchy. */
	TSharedRef<SFilmOverlay> GetFilmOverlayWidget() const;

private:

	/** Generate menu content for the combo button */
	TSharedRef<SWidget> GetMenuContent();

	/** Construct the part of the menu that defines the set of film overlays */
	TSharedRef<SWidget> ConstructMasterOverlaysMenu();

	/** Construct the part of the menu that defines the set of toggleable overlays (currently just safe-frames) */
	TSharedRef<SWidget> ConstructToggleableOverlaysMenu();

private:

	/** Get the thumbnail to be displayed on the combo box button */
	const FSlateBrush* GetCurrentThumbnail() const;

	/** Get the master film overlay ptr (may be nullptr) */
	IFilmOverlay* GetMasterFilmOverlay() const;

	/** Get an array of all enabled film overlays */
	TArray<IFilmOverlay*> GetActiveFilmOverlays() const;

	/** Set the current master overlay to the specified name */
	FReply SetMasterFilmOverlay(FName InName);

	/** Get/Set the color tint override for the current master overlay */
	FLinearColor GetMasterColorTint() const;
	void OnMasterColorTintChanged(const FLinearColor& Tint);

private:

	/** Set of master film overlays (only one can be active at a time) */
	TMap<FName, TUniquePtr<IFilmOverlay>> MasterFilmOverlays;

	/** The name of the current master overlay */
	FName CurrentMasterOverlay;

	/** Color tint to apply to master overlays */
	FLinearColor MasterColorTint;

	/** A map of toggleable overlays (any number can be active at a time) */
	TMap<FName, TUniquePtr<IFilmOverlay>> ToggleableOverlays;

	/** The overlay widget we control */
	TSharedPtr<SFilmOverlay> OverlayWidget;
};
