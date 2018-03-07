// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Geometry.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SBox.h"

class FPaintArgs;
class FSceneViewport;
class FSlateWindowElementList;
class SOverlay;
class STooltipPresenter;
class UGameViewportClient;
class ULocalPlayer;

/**
 * Allows you to provide a custom layer that multiple sources can contribute to.  Unlike
 * adding widgets directly to the layer manager.  First registering a layer with a name
 * allows multiple widgets to be added.
 */
class IGameLayer : public TSharedFromThis<IGameLayer>
{
public:
	/** Get the layer as a widget. */
	virtual TSharedRef<SWidget> AsWidget() = 0;

public:

	/** Virtual destructor. */
	virtual ~IGameLayer() { }
};

UENUM()
enum class EWindowTitleBarMode : uint8
{
	Overlay,
	VerticalBox,
};

/**
 * Allows widgets to be managed for different users.
 */
class IGameLayerManager
{
public:
	virtual const FGeometry& GetViewportWidgetHostGeometry() const = 0;
	virtual const FGeometry& GetPlayerWidgetHostGeometry(ULocalPlayer* Player) const = 0;

	virtual void NotifyPlayerAdded(int32 PlayerIndex, ULocalPlayer* AddedPlayer) = 0;
	virtual void NotifyPlayerRemoved(int32 PlayerIndex, ULocalPlayer* RemovedPlayer) = 0;

	virtual void AddWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, int32 ZOrder) = 0;
	virtual void RemoveWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent) = 0;
	virtual void ClearWidgetsForPlayer(ULocalPlayer* Player) = 0;

	virtual TSharedPtr<IGameLayer> FindLayerForPlayer(ULocalPlayer* Player, const FName& LayerName) = 0;
	virtual bool AddLayerForPlayer(ULocalPlayer* Player, const FName& LayerName, TSharedRef<IGameLayer> Layer, int32 ZOrder) = 0;

	virtual void ClearWidgets() = 0;

	virtual void SetDefaultWindowTitleBarHeight(float Height) = 0;
	virtual void SetWindowTitleBarContent(const TSharedPtr<SWidget>& TitleBarContent, EWindowTitleBarMode Mode) = 0;
	virtual void RestorePreviousWindowTitleBarContent() = 0;
	virtual void SetDefaultWindowTitleBarContentAsCurrent() = 0;
	virtual void SetWindowTitleBarVisibility(bool bIsVisible) = 0;
};

/**
 * 
 */
class ENGINE_API SGameLayerManager : public SCompoundWidget, public IGameLayerManager
{
public:

	SLATE_BEGIN_ARGS(SGameLayerManager)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
		_Clipping = EWidgetClipping::ClipToBoundsAlways;
	}

		/** Slot for this content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_ATTRIBUTE(const FSceneViewport*, SceneViewport)

	SLATE_END_ARGS()

	SGameLayerManager();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	// Begin IGameLayerManager
	virtual const FGeometry& GetViewportWidgetHostGeometry() const override;
	virtual const FGeometry& GetPlayerWidgetHostGeometry(ULocalPlayer* Player) const override;

	virtual void NotifyPlayerAdded(int32 PlayerIndex, ULocalPlayer* AddedPlayer) override;
	virtual void NotifyPlayerRemoved(int32 PlayerIndex, ULocalPlayer* RemovedPlayer) override;

	virtual void AddWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, int32 ZOrder) override;
	virtual void RemoveWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent) override;
	virtual void ClearWidgetsForPlayer(ULocalPlayer* Player) override;

	virtual TSharedPtr<IGameLayer> FindLayerForPlayer(ULocalPlayer* Player, const FName& LayerName) override;
	virtual bool AddLayerForPlayer(ULocalPlayer* Player, const FName& LayerName, TSharedRef<IGameLayer> Layer, int32 ZOrder) override;

	virtual void ClearWidgets() override;

	virtual void SetDefaultWindowTitleBarHeight(float Height);
	virtual void SetWindowTitleBarContent(const TSharedPtr<SWidget>& TitleBarContent, EWindowTitleBarMode Mode);
	virtual void RestorePreviousWindowTitleBarContent();
	virtual void SetDefaultWindowTitleBarContentAsCurrent();
	virtual void SetWindowTitleBarVisibility(bool bIsVisible);
	// End IGameLayerManager

public:

	// Begin SWidget overrides
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent) override;
	// End SWidget overrides

private:
	float GetGameViewportDPIScale() const;
	FOptionalSize GetDefaultWindowTitleBarHeight() const;

private:

	struct FPlayerLayer : TSharedFromThis<FPlayerLayer>
	{
		TSharedPtr<SOverlay> Widget;
		SCanvas::FSlot* Slot;

		TMap< FName, TSharedPtr<IGameLayer> > Layers;

		FPlayerLayer()
			: Slot(nullptr)
		{
		}
	};

	void UpdateLayout();

	TSharedPtr<FPlayerLayer> FindOrCreatePlayerLayer(ULocalPlayer* LocalPlayer);
	void RemoveMissingPlayerLayers(const TArray<ULocalPlayer*>& GamePlayers);
	void RemovePlayerWidgets(ULocalPlayer* LocalPlayer);
	void AddOrUpdatePlayerLayers(const FGeometry& AllottedGeometry, UGameViewportClient* ViewportClient, const TArray<ULocalPlayer*>& GamePlayers);
	FVector2D GetAspectRatioInset(ULocalPlayer* LocalPlayer) const;

	void UpdateWindowTitleBar();
	void UpdateWindowTitleBarVisibility();

private:
	FGeometry CachedGeometry;

	TMap < ULocalPlayer*, TSharedPtr<FPlayerLayer> > PlayerLayers;

	TAttribute<const FSceneViewport*> SceneViewport;
	TSharedPtr<class SVerticalBox> WidgetHost;
	TSharedPtr<SCanvas> PlayerCanvas;
	TSharedPtr<class STooltipPresenter> TooltipPresenter;

	TSharedPtr<class SWindowTitleBarArea> TitleBarAreaOverlay;
	TSharedPtr<class SWindowTitleBarArea> TitleBarAreaVerticalBox;
	TSharedPtr<SBox> WindowTitleBarVerticalBox;
	TSharedPtr<SBox> WindowTitleBarOverlay;

	struct FWindowTitleBarContent
	{
		TSharedPtr<SWidget> ContentWidget;
		EWindowTitleBarMode Mode;

		FWindowTitleBarContent() : ContentWidget(), Mode(EWindowTitleBarMode::Overlay) {}
		FWindowTitleBarContent(const TSharedPtr<SWidget>& TitleBarContent, EWindowTitleBarMode InMode) : ContentWidget(TitleBarContent), Mode(InMode) {}
	};

	TArray<FWindowTitleBarContent> WindowTitleBarContentStack;
	FWindowTitleBarContent DefaultWindowTitleBarContent;
	float DefaultWindowTitleBarHeight;

	bool bIsWindowTitleBarVisible;
};
