// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Slate/SGameLayerManager.h"
#include "Widgets/SOverlay.h"
#include "Engine/LocalPlayer.h"
#include "Slate/SceneViewport.h"
#include "EngineGlobals.h"
#include "SceneView.h"
#include "Engine/Engine.h"
#include "Types/NavigationMetaData.h"


#include "Engine/GameEngine.h"
#include "Engine/UserInterfaceSettings.h"

#include "Widgets/LayerManager/STooltipPresenter.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SPopup.h"
#include "Widgets/Layout/SWindowTitleBarArea.h"

/* SGameLayerManager interface
 *****************************************************************************/

SGameLayerManager::SGameLayerManager()
{
}

void SGameLayerManager::Construct(const SGameLayerManager::FArguments& InArgs)
{
	SceneViewport = InArgs._SceneViewport;

	TSharedRef<SDPIScaler> DPIScaler =
		SNew(SDPIScaler)
		.DPIScale(this, &SGameLayerManager::GetGameViewportDPIScale)
		[
			// All user widgets live inside this vertical box.
			SAssignNew(WidgetHost, SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TitleBarAreaVerticalBox, SWindowTitleBarArea)
				[
					SAssignNew(WindowTitleBarVerticalBox, SBox)
				]
			]

			+ SVerticalBox::Slot()
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SAssignNew(PlayerCanvas, SCanvas)
				]

				+ SOverlay::Slot()
				[
					InArgs._Content.Widget
				]

				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(TitleBarAreaOverlay, SWindowTitleBarArea)
						[
							SAssignNew(WindowTitleBarOverlay, SBox)
						]
					]
				]

				+ SOverlay::Slot()
				[
					SNew(SPopup)
					[
						SAssignNew(TooltipPresenter, STooltipPresenter)
					]
				]
			]
		];

	ChildSlot
	[
		DPIScaler
	];

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine != nullptr)
	{
		TSharedPtr<SWindow> GameViewportWindow = GameEngine->GameViewportWindow.Pin();
		if (GameViewportWindow.IsValid())
		{
			TitleBarAreaOverlay->SetGameWindow(GameViewportWindow);
			TitleBarAreaVerticalBox->SetGameWindow(GameViewportWindow);
		}
	}

	bIsWindowTitleBarVisible = false;

	DefaultWindowTitleBarHeight = 64.0f;
	DefaultWindowTitleBarContent.Mode = EWindowTitleBarMode::Overlay;
	DefaultWindowTitleBarContent.ContentWidget =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox).HeightOverride(this, &SGameLayerManager::GetDefaultWindowTitleBarHeight)
		];

	SetDefaultWindowTitleBarContentAsCurrent();
}

const FGeometry& SGameLayerManager::GetViewportWidgetHostGeometry() const
{
	return WidgetHost->GetCachedGeometry();
}

const FGeometry& SGameLayerManager::GetPlayerWidgetHostGeometry(ULocalPlayer* Player) const
{
	TSharedPtr<FPlayerLayer> PlayerLayer = PlayerLayers.FindRef(Player);
	if ( PlayerLayer.IsValid() )
	{
		return PlayerLayer->Widget->GetCachedGeometry();
	}

	static FGeometry Identity;
	return Identity;
}

void SGameLayerManager::NotifyPlayerAdded(int32 PlayerIndex, ULocalPlayer* AddedPlayer)
{
	UpdateLayout();
}

void SGameLayerManager::NotifyPlayerRemoved(int32 PlayerIndex, ULocalPlayer* RemovedPlayer)
{
	UpdateLayout();
}

void SGameLayerManager::AddWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, const int32 ZOrder)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = FindOrCreatePlayerLayer(Player);
	
	// NOTE: Returns FSimpleSlot but we're ignoring here.  Could be used for alignment though.
	PlayerLayer->Widget->AddSlot(ZOrder)
	[
		ViewportContent
	];
}

void SGameLayerManager::RemoveWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent)
{
	TSharedPtr<FPlayerLayer>* PlayerLayerPtr = PlayerLayers.Find(Player);
	if ( PlayerLayerPtr )
	{
		TSharedPtr<FPlayerLayer> PlayerLayer = *PlayerLayerPtr;
		PlayerLayer->Widget->RemoveSlot(ViewportContent);
	}
}

void SGameLayerManager::ClearWidgetsForPlayer(ULocalPlayer* Player)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = PlayerLayers.FindRef(Player);
	if ( PlayerLayer.IsValid() )
	{
		PlayerLayer->Widget->ClearChildren();
	}
}

TSharedPtr<IGameLayer> SGameLayerManager::FindLayerForPlayer(ULocalPlayer* Player, const FName& LayerName)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = PlayerLayers.FindRef(Player);
	if ( PlayerLayer.IsValid() )
	{
		return PlayerLayer->Layers.FindRef(LayerName);
	}

	return TSharedPtr<IGameLayer>();
}

bool SGameLayerManager::AddLayerForPlayer(ULocalPlayer* Player, const FName& LayerName, TSharedRef<IGameLayer> Layer, int32 ZOrder)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = FindOrCreatePlayerLayer(Player);
	if ( PlayerLayer.IsValid() )
	{
		TSharedPtr<IGameLayer> ExistingLayer = PlayerLayer->Layers.FindRef(LayerName);
		if ( ExistingLayer.IsValid() )
		{
			return false;
		}

		PlayerLayer->Layers.Add(LayerName, Layer);

		PlayerLayer->Widget->AddSlot(ZOrder)
		[
			Layer->AsWidget()
		];

		return true;
	}

	return false;
}

void SGameLayerManager::ClearWidgets()
{
	PlayerCanvas->ClearChildren();

	// Potential for removed layers to impact the map, so need to
	// remove & delete as separate steps
	while (PlayerLayers.Num())
	{
		const auto LayerIt = PlayerLayers.CreateIterator();
		const TSharedPtr<FPlayerLayer> Layer = LayerIt.Value();

		if (Layer.IsValid())
		{
			Layer->Slot = nullptr;
		}

		PlayerLayers.Remove(LayerIt.Key());
	}

	WindowTitleBarContentStack.Empty();
	bIsWindowTitleBarVisible = false;
	SetDefaultWindowTitleBarContentAsCurrent();
}

void SGameLayerManager::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	CachedGeometry = AllottedGeometry;

	UpdateLayout();
}

int32 SGameLayerManager::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FPlatformMisc::BeginNamedEvent(FColor::Green, "Paint: Game UI");
	const int32 ResultLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	FPlatformMisc::EndNamedEvent();
	return ResultLayer;
}

bool SGameLayerManager::OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent)
{
	TooltipPresenter->SetContent(TooltipContent.IsValid() ? TooltipContent.ToSharedRef() : SNullWidget::NullWidget);

	return true;
}

float SGameLayerManager::GetGameViewportDPIScale() const
{
	if ( const FSceneViewport* Viewport = SceneViewport.Get() )
	{
		FIntPoint ViewportSize = Viewport->GetSize();
		const float GameUIScale = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(ViewportSize);

		// Remove the platform DPI scale from the incoming size.  Since the platform DPI is already
		// attempt to normalize the UI for a high DPI, and the DPI scale curve is based on raw resolution
		// for what a assumed platform scale of 1, extract that scale the calculated scale, since that will
		// already be applied by slate.
		const float FinalUIScale = GameUIScale / Viewport->GetCachedGeometry().Scale;

		return FinalUIScale;
	}

	return 1;
}

FOptionalSize SGameLayerManager::GetDefaultWindowTitleBarHeight() const
{
	return DefaultWindowTitleBarHeight;
}

void SGameLayerManager::UpdateLayout()
{
	if ( const FSceneViewport* Viewport = SceneViewport.Get() )
	{
		if ( UWorld* World = Viewport->GetClient()->GetWorld() )
		{
			if ( World->IsGameWorld() == false )
			{
				PlayerLayers.Reset();
				return;
			}

			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				const TArray<ULocalPlayer*>& GamePlayers = GEngine->GetGamePlayers(World);

				RemoveMissingPlayerLayers(GamePlayers);
				AddOrUpdatePlayerLayers(CachedGeometry, ViewportClient, GamePlayers);
			}
		}
	}
}

TSharedPtr<SGameLayerManager::FPlayerLayer> SGameLayerManager::FindOrCreatePlayerLayer(ULocalPlayer* LocalPlayer)
{
	TSharedPtr<FPlayerLayer>* PlayerLayerPtr = PlayerLayers.Find(LocalPlayer);
	if ( PlayerLayerPtr == nullptr )
	{
		// Prevent any navigation outside of a player's layer once focus has been placed there.
		TSharedRef<FNavigationMetaData> StopNavigation = MakeShareable(new FNavigationMetaData());
		StopNavigation->SetNavigationStop(EUINavigation::Up);
		StopNavigation->SetNavigationStop(EUINavigation::Down);
		StopNavigation->SetNavigationStop(EUINavigation::Left);
		StopNavigation->SetNavigationStop(EUINavigation::Right);
		StopNavigation->SetNavigationStop(EUINavigation::Previous);
		StopNavigation->SetNavigationStop(EUINavigation::Next);

		// Create a new entry for the player
		TSharedPtr<FPlayerLayer> NewLayer = MakeShareable(new FPlayerLayer());

		// Create a new overlay widget to house any widgets we want to display for the player.
		NewLayer->Widget = SNew(SOverlay)
			.AddMetaData(StopNavigation)
			.Clipping(EWidgetClipping::ClipToBoundsAlways);
		
		// Add the overlay to the player canvas, which we'll update every frame to match
		// the dimensions of the player's split screen rect.
		PlayerCanvas->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Expose(NewLayer->Slot)
			[
				NewLayer->Widget.ToSharedRef()
			];

		PlayerLayerPtr = &PlayerLayers.Add(LocalPlayer, NewLayer);
	}

	return *PlayerLayerPtr;
}

void SGameLayerManager::RemoveMissingPlayerLayers(const TArray<ULocalPlayer*>& GamePlayers)
{
	TArray<ULocalPlayer*> ToRemove;

	// Find the player layers for players that no longer exist
	for ( TMap< ULocalPlayer*, TSharedPtr<FPlayerLayer> >::TIterator It(PlayerLayers); It; ++It )
	{
		ULocalPlayer* Key = ( *It ).Key;
		if ( !GamePlayers.Contains(Key) )
		{
			ToRemove.Add(Key);
		}
	}

	// Remove the missing players
	for ( ULocalPlayer* Player : ToRemove )
	{
		RemovePlayerWidgets(Player);
	}
}

void SGameLayerManager::RemovePlayerWidgets(ULocalPlayer* LocalPlayer)
{
	TSharedPtr<FPlayerLayer> Layer = PlayerLayers.FindRef(LocalPlayer);
	PlayerCanvas->RemoveSlot(Layer->Widget.ToSharedRef());

	PlayerLayers.Remove(LocalPlayer);
}

void SGameLayerManager::AddOrUpdatePlayerLayers(const FGeometry& AllottedGeometry, UGameViewportClient* ViewportClient, const TArray<ULocalPlayer*>& GamePlayers)
{
	ESplitScreenType::Type SplitType = ViewportClient->GetCurrentSplitscreenConfiguration();
	TArray<struct FSplitscreenData>& SplitInfo = ViewportClient->SplitscreenInfo;

	float InverseDPIScale = 1.0f / GetGameViewportDPIScale();

	// Add and Update Player Layers
	for ( int32 PlayerIndex = 0; PlayerIndex < GamePlayers.Num(); PlayerIndex++ )
	{
		ULocalPlayer* Player = GamePlayers[PlayerIndex];

		if ( SplitType < SplitInfo.Num() && PlayerIndex < SplitInfo[SplitType].PlayerData.Num() )
		{
			TSharedPtr<FPlayerLayer> PlayerLayer = FindOrCreatePlayerLayer(Player);
			FPerPlayerSplitscreenData& SplitData = SplitInfo[SplitType].PlayerData[PlayerIndex];

			// Viewport Sizes
			FVector2D Size, Position;
			Size.X = SplitData.SizeX;
			Size.Y = SplitData.SizeY;
			Position.X = SplitData.OriginX;
			Position.Y = SplitData.OriginY;

			FVector2D AspectRatioInset = GetAspectRatioInset(Player);

			Position += AspectRatioInset;
			Size -= (AspectRatioInset * 2.0f);

			Size = Size * AllottedGeometry.GetLocalSize() * InverseDPIScale;
			Position = Position * AllottedGeometry.GetLocalSize() * InverseDPIScale;

			if (bIsWindowTitleBarVisible)
			{
				const FWindowTitleBarContent& WindowTitleBarContent = WindowTitleBarContentStack.Top();
				if (WindowTitleBarContent.Mode == EWindowTitleBarMode::VerticalBox && Size.Y > WindowTitleBarVerticalBox->GetDesiredSize().Y)
				{
					Size.Y -= WindowTitleBarVerticalBox->GetDesiredSize().Y;
				}
			}

			PlayerLayer->Slot->Size(Size);
			PlayerLayer->Slot->Position(Position);
		}
	}
}

FVector2D SGameLayerManager::GetAspectRatioInset(ULocalPlayer* LocalPlayer) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SGameLayerManager_GetAspectRatioInset);
	FVector2D Offset(0.f, 0.f);
	if ( LocalPlayer )
	{
		FSceneViewInitOptions ViewInitOptions;
		if (LocalPlayer->CalcSceneViewInitOptions(ViewInitOptions, LocalPlayer->ViewportClient->Viewport))
		{
			FIntRect ViewRect = ViewInitOptions.GetViewRect();
			FIntRect ConstrainedViewRect = ViewInitOptions.GetConstrainedViewRect();

			// Return normalized coordinates.
			Offset.X = ( ConstrainedViewRect.Min.X - ViewRect.Min.X ) / (float)ViewRect.Width();
			Offset.Y = ( ConstrainedViewRect.Min.Y - ViewRect.Min.Y ) / (float)ViewRect.Height();
		}
	}

	return Offset;
}

void SGameLayerManager::SetDefaultWindowTitleBarHeight(float Height)
{
	DefaultWindowTitleBarHeight = Height;
}

void SGameLayerManager::SetWindowTitleBarContent(const TSharedPtr<SWidget>& TitleBarContent, EWindowTitleBarMode Mode)
{
	WindowTitleBarContentStack.Push(FWindowTitleBarContent(TitleBarContent, Mode));
	UpdateWindowTitleBar();
}

void SGameLayerManager::RestorePreviousWindowTitleBarContent()
{
	WindowTitleBarContentStack.Pop();
	UpdateWindowTitleBar();
}

void SGameLayerManager::SetDefaultWindowTitleBarContentAsCurrent()
{
	WindowTitleBarContentStack.Push(DefaultWindowTitleBarContent);
	UpdateWindowTitleBar();
}

void SGameLayerManager::SetWindowTitleBarVisibility(bool bIsVisible)
{
	bIsWindowTitleBarVisible = bIsVisible;
	UpdateWindowTitleBarVisibility();
}

void SGameLayerManager::UpdateWindowTitleBar()
{
	const FWindowTitleBarContent& WindowTitleBarContent = WindowTitleBarContentStack.Top();
	if (WindowTitleBarContent.ContentWidget.IsValid())
	{
		if (WindowTitleBarContent.Mode == EWindowTitleBarMode::Overlay)
		{
			WindowTitleBarOverlay->SetContent(WindowTitleBarContent.ContentWidget.ToSharedRef());
		}
		else if (WindowTitleBarContent.Mode == EWindowTitleBarMode::VerticalBox)
		{
			WindowTitleBarVerticalBox->SetContent(WindowTitleBarContent.ContentWidget.ToSharedRef());
		}
	}

	UpdateWindowTitleBarVisibility();
}

void SGameLayerManager::UpdateWindowTitleBarVisibility()
{
	const FWindowTitleBarContent& WindowTitleBarContent = WindowTitleBarContentStack.Top();
	TitleBarAreaOverlay->SetVisibility(bIsWindowTitleBarVisible && WindowTitleBarContent.Mode == EWindowTitleBarMode::Overlay ? EVisibility::Visible : EVisibility::Collapsed);
	TitleBarAreaVerticalBox->SetVisibility(bIsWindowTitleBarVisible && WindowTitleBarContent.Mode == EWindowTitleBarMode::VerticalBox ? EVisibility::Visible : EVisibility::Collapsed);
}