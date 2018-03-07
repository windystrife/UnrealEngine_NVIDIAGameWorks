// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HUD.cpp: Heads up Display related functionality
=============================================================================*/

#include "GameFramework/HUD.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/App.h"
#include "EngineGlobals.h"
#include "Layout/Margin.h"
#include "CollisionQueryParams.h"
#include "Materials/MaterialInterface.h"
#include "SceneView.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameModeBase.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/LineBatchComponent.h"
#include "Engine/Canvas.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "DisplayDebugHelpers.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogHUD, Log, All);

#define LOCTEXT_NAMESPACE "HUD"

FOnShowDebugInfo AHUD::OnShowDebugInfo;

// Should we visualize the safe zone? (and if so, title or action?)
TAutoConsoleVariable<int32> GSafeZoneVisualizationModeCVar(
	TEXT("r.DebugSafeZone.Mode"),
	0,
	TEXT("The safe zone visualization mode (0..2)\n")
	TEXT(" 0: Disabled (default)\n")
	TEXT(" 1: Show Title Safe Zone\n")
	TEXT(" 2: Show Action Safe Zone"));

// How opaque should the safe zone visualization be?
TAutoConsoleVariable<float> GSafeZoneVisualizationAlphaCVar(
	TEXT("r.DebugSafeZone.OverlayAlpha"),
	0.2f,
	TEXT("The alpha value of the safe zone overlay (0..1)\n")
	TEXT(" default: 0.2"));

const FColor AHUD::WhiteColor(255, 255, 255, 255);
const FColor AHUD::GreenColor(0, 255, 0, 255);
const FColor AHUD::RedColor(255, 0, 0, 255);

AHUD::AHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.TickGroup = TG_DuringPhysics;
	PrimaryActorTick.bCanEverTick = true;
	bHidden = true;
	bReplicates = false;

	bLostFocusPaused = false;
	bShowHUD = true;

	bCanBeDamaged = false;
	bEnableDebugTextShadow = false;
}

void AHUD::SetCanvas(class UCanvas* InCanvas, class UCanvas* InDebugCanvas)
{
	Canvas = InCanvas;
	DebugCanvas = InDebugCanvas;
}


void AHUD::Draw3DLine(FVector Start, FVector End, FColor LineColor)
{
	GetWorld()->LineBatcher->DrawLine(Start, End, LineColor, SDPG_World);
}

void AHUD::Draw2DLine(int32 X1, int32 Y1, int32 X2, int32 Y2, FColor LineColor)
{
	check(Canvas);

	FCanvasLineItem LineItem(FVector2D(X1, Y1), FVector2D(X2, Y2));
	LineItem.SetColor(LineColor);
	LineItem.Draw(Canvas->Canvas);
}

void AHUD::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	PlayerOwner = Cast<APlayerController>(GetOwner());

	// e.g. getting material pointers to control effects for gameplay
	NotifyBindPostProcessEffects();
}



void AHUD::NotifyBindPostProcessEffects()
{
	// overload with custom code e.g. getting material pointers to control effects for gameplay.
}

FVector2D AHUD::GetCoordinateOffset() const
{
	FVector2D Offset(0.f, 0.f);

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(GetOwningPlayerController()->Player);

	if (LocalPlayer)
	{
		// Create a view family for the game viewport
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			LocalPlayer->ViewportClient->Viewport,
			GetWorld()->Scene,
			LocalPlayer->ViewportClient->EngineShowFlags)
			.SetRealtimeUpdate(true));

		// Calculate a view where the player is to update the streaming from the players start location
		FVector ViewLocation;
		FRotator ViewRotation;
		FSceneView* SceneView = LocalPlayer->CalcSceneView(&ViewFamily, /*out*/ ViewLocation, /*out*/ ViewRotation, LocalPlayer->ViewportClient->Viewport);

		if (SceneView)
		{
			Offset.X = (SceneView->ViewRect.Min.X - SceneView->UnscaledViewRect.Min.X) // This accounts for the borders when the aspect ratio is locked
				- SceneView->UnscaledViewRect.Min.X;						// And this will deal with the viewport offset if its a split screen

			Offset.Y = (SceneView->ViewRect.Min.Y - SceneView->UnscaledViewRect.Min.Y)
				- SceneView->UnscaledViewRect.Min.Y;
		}
	}

	return Offset;
}

void AHUD::PostRender()
{
	// Theres nothing we can really do without a canvas or a world - so leave now in that case
	if ( (GetWorld() == nullptr) || (Canvas == nullptr))
	{
		return;
	}
	// Set up delta time
	RenderDelta = GetWorld()->TimeSeconds - LastHUDRenderTime;

	if ( PlayerOwner != NULL )
	{
		// draw any debug text in real-time
		DrawDebugTextList();
	}

	if ( bShowDebugInfo )
	{
		if (DebugCanvas)
		{
			DebugCanvas->DisplayDebugManager.Initialize(DebugCanvas, GEngine->GetTinyFont(), FVector2D(4.f, 50.f));
			ShowDebugInfo(DebugCanvas->DisplayDebugManager.GetMaxCharHeightRef(), DebugCanvas->DisplayDebugManager.GetYPosRef());
		}
	}
	else if ( bShowHUD && FApp::CanEverRender() )
	{
		DrawHUD();
		
		// No need to do work to determine hit box candidates if there will never be any
		if (HitBoxMap.Num() > 0)
		{
			ULocalPlayer* LocalPlayer = GetOwningPlayerController() ? Cast<ULocalPlayer>(GetOwningPlayerController()->Player) : NULL;

			if (LocalPlayer && LocalPlayer->ViewportClient)
			{
				TArray<FVector2D> ContactPoints;

				if (!FSlateApplication::Get().IsFakingTouchEvents())
				{
					FVector2D MousePosition;
					if (LocalPlayer->ViewportClient->GetMousePosition(MousePosition))
					{
						ContactPoints.Add(MousePosition);
					}
				}

				for (int32 FingerIndex = 0; FingerIndex < EKeys::NUM_TOUCH_KEYS; ++FingerIndex)
				{
					FVector2D TouchLocation;
					bool bPressed = false;

					GetOwningPlayerController()->GetInputTouchState((ETouchIndex::Type)FingerIndex, TouchLocation.X, TouchLocation.Y, bPressed);

					if (bPressed)
					{
						ContactPoints.Add(TouchLocation);
					}
				}

				const FVector2D ContactPointOffset = GetCoordinateOffset();

				if (!ContactPointOffset.IsZero())
				{
					for (FVector2D& ContactPoint : ContactPoints)
					{
						ContactPoint += ContactPointOffset;
					}
				}
				UpdateHitBoxCandidates( MoveTemp(ContactPoints) );
			}
		}
		else if (HitBoxesOver.Num() > 0)
		{
			// We still need to dispatch any end cursor over messages even if we don't have any hitboxes anymore
			for (const FName HitBoxName : HitBoxesOver)
			{
				NotifyHitBoxEndCursorOver(HitBoxName);
			}
			HitBoxesOver.Reset();
		}
	}
	
	if( bShowHitBoxDebugInfo )
	{
		RenderHitBoxes( Canvas->Canvas );
	}

	DrawSafeZoneOverlay();

	LastHUDRenderTime = GetWorld()->TimeSeconds;
}

void AHUD::DrawActorOverlays(FVector Viewpoint, FRotator ViewRotation)
{
	// determine rendered camera position
	FVector ViewDir = ViewRotation.Vector();
	int32 i = 0;
	while (i < PostRenderedActors.Num())
	{
		if ( PostRenderedActors[i] != NULL )
		{
			PostRenderedActors[i]->PostRenderFor(PlayerOwner,Canvas,Viewpoint,ViewDir);
			i++;
		}
		else
		{
			PostRenderedActors.RemoveAt(i,1);
		}
	}
}

void AHUD::DrawSafeZoneOverlay()
{
#if ENABLE_DRAW_DEBUG
	const int32 DebugSafeZoneMode = GSafeZoneVisualizationModeCVar.GetValueOnGameThread();

	if ((DebugSafeZoneMode > 0) && (DebugCanvas != nullptr))
	{
		FDisplayMetrics Metrics;
		FSlateApplication::Get().GetDisplayMetrics(Metrics);

		const FMargin SafeMargin =
#if PLATFORM_IOS
			// Hack: This is a temp solution to support iPhoneX safeArea. TitleSafePaddingSize and ActionSafePaddingSize should be FVector4 and use them separately. 
			FMargin(Metrics.TitleSafePaddingSize.X, Metrics.ActionSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y, Metrics.ActionSafePaddingSize.Y);
#else
			(DebugSafeZoneMode == 1) ?
			FMargin(Metrics.TitleSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y) :
			FMargin(Metrics.ActionSafePaddingSize.X, Metrics.ActionSafePaddingSize.Y);
#endif

		const float UnsafeZoneAlpha = GSafeZoneVisualizationAlphaCVar.GetValueOnGameThread();
		const FLinearColor UnsafeZoneColor(1.0f, 0.5f, 0.5f, UnsafeZoneAlpha);

		const float Width = DebugCanvas->SizeX;
		const float Height = DebugCanvas->SizeY;

		const float HeightOfSides = Height - SafeMargin.GetTotalSpaceAlong<Orient_Vertical>();

		FCanvasTileItem TileItem(FVector2D::ZeroVector, GWhiteTexture, UnsafeZoneColor);
		TileItem.BlendMode = SE_BLEND_Translucent;

		// Top bar
		TileItem.Position = FVector2D::ZeroVector;
		TileItem.Size = FVector2D(Width, SafeMargin.Top);
		DebugCanvas->DrawItem(TileItem);

		// Bottom bar
		TileItem.Position = FVector2D(0.0f, Height - SafeMargin.Bottom);
		TileItem.Size = FVector2D(Width, SafeMargin.Bottom);
		DebugCanvas->DrawItem(TileItem);

		// Left bar
		TileItem.Position = FVector2D(0.0f, SafeMargin.Top);
		TileItem.Size = FVector2D(SafeMargin.Left, HeightOfSides);
		DebugCanvas->DrawItem(TileItem);

		// Right bar
		TileItem.Position = FVector2D(Width - SafeMargin.Right, SafeMargin.Top);
		TileItem.Size = FVector2D(SafeMargin.Right, HeightOfSides);
		DebugCanvas->DrawItem(TileItem);
	}
#endif
}

void AHUD::RemovePostRenderedActor(AActor* A)
{
	for ( int32 i=0; i<PostRenderedActors.Num(); i++ )
	{
		if ( PostRenderedActors[i] == A )
		{
			PostRenderedActors[i] = NULL;
			return;
		}
	}
}

void AHUD::AddPostRenderedActor(AActor* A)
{
	// make sure that A is not already in list
	for ( int32 i=0; i<PostRenderedActors.Num(); i++ )
	{
		if ( PostRenderedActors[i] == A )
		{
			return;
		}
	}

	// add A at first empty slot
	for ( int32 i=0; i<PostRenderedActors.Num(); i++ )
	{
		if ( PostRenderedActors[i] == NULL )
		{
			PostRenderedActors[i] = A;
			return;
		}
	}

	// no empty slot found, so grow array
	PostRenderedActors.Add(A);
}

void AHUD::ShowHUD()
{
	bShowHUD = !bShowHUD;
}

static FName NAME_Reset = FName(TEXT("Reset"));
void AHUD::ShowDebug(FName DebugType)
{
	if (DebugType == NAME_None)
	{
		bShowDebugInfo = !bShowDebugInfo;
	}
	else if ( DebugType == FName("HitBox") )
	{
		bShowHitBoxDebugInfo = !bShowHitBoxDebugInfo;
	}
	else if( DebugType == NAME_Reset )
	{
		DebugDisplay.Reset();
		bShowDebugInfo = false;
		SaveConfig();
	}
	else
	{
		bool bRemoved = false;
		if (bShowDebugInfo)
		{
			// remove debugtype if already in array
			if (0 != DebugDisplay.Remove(DebugType))
			{
				bRemoved = true;
			}
		}
		if (!bRemoved)
		{
			DebugDisplay.Add(DebugType);
		}

		bShowDebugInfo = true;

		SaveConfig();
	}
}

void AHUD::ShowDebugToggleSubCategory(FName Category)
{
	if( Category == NAME_Reset )
	{
		ToggledDebugCategories.Reset();
		SaveConfig();
	}
	else
	{
		if (0 == ToggledDebugCategories.Remove(Category))
		{
			ToggledDebugCategories.Add(Category);
		}
		SaveConfig();
	}
}

void AHUD::ShowDebugForReticleTargetToggle(TSubclassOf<AActor> DesiredClass)
{
	bShowDebugForReticleTarget = !bShowDebugForReticleTarget;
	ShowDebugTargetDesiredClass = DesiredClass;
}

bool AHUD::ShouldDisplayDebug(const FName& DebugType) const
{
	return bShowDebugInfo && DebugDisplay.Contains(DebugType);
}

void AHUD::ShowDebugInfo(float& YL, float& YPos)
{
	if (DebugCanvas != nullptr )
	{
		FLinearColor BackgroundColor(0.f, 0.f, 0.f, 0.2f);
		DebugCanvas->Canvas->DrawTile(0, 0, DebugCanvas->ClipX, DebugCanvas->ClipY, 0.f, 0.f, 0.f, 0.f, BackgroundColor);

		FDebugDisplayInfo DisplayInfo(DebugDisplay, ToggledDebugCategories);

		if (bShowDebugForReticleTarget)
		{
			FRotator CamRot; FVector CamLoc; PlayerOwner->GetPlayerViewPoint(CamLoc, CamRot);

			FCollisionQueryParams TraceParams(NAME_None, FCollisionQueryParams::GetUnknownStatId(), true, PlayerOwner->PlayerCameraManager->ViewTarget.Target);
			FHitResult Hit;
			bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamRot.Vector() * 100000.f + CamLoc, ECC_WorldDynamic, TraceParams);
			if (bHit)
			{
				AActor* HitActor = Hit.Actor.Get();
				if (HitActor && (ShowDebugTargetDesiredClass == NULL || HitActor->IsA(ShowDebugTargetDesiredClass)))
				{
					ShowDebugTargetActor = HitActor;
				}
			}
		}
		else
		{
			ShowDebugTargetActor = PlayerOwner->PlayerCameraManager->ViewTarget.Target;
		}

		if (ShowDebugTargetActor && !ShowDebugTargetActor->IsPendingKill())
		{
			ShowDebugTargetActor->DisplayDebug(DebugCanvas, DisplayInfo, YL, YPos);
		}

		if (ShouldDisplayDebug(NAME_Game))
		{
			AGameModeBase* AuthGameMode = GetWorld()->GetAuthGameMode();
			if (AuthGameMode)
			{
				AuthGameMode->DisplayDebug(DebugCanvas, DisplayInfo, YL, YPos);
			}
		}

		if (bShowDebugInfo)
		{
			OnShowDebugInfo.Broadcast(this, DebugCanvas, DisplayInfo, YL, YPos);
		}
	}
}

void AHUD::DrawHUD()
{
	HitBoxMap.Reset();
	HitBoxHits.Reset();
	if ( bShowOverlays && (PlayerOwner != NULL) )
	{
		FVector ViewPoint;
		FRotator ViewRotation;
		PlayerOwner->GetPlayerViewPoint(ViewPoint, ViewRotation);
		DrawActorOverlays(ViewPoint, ViewRotation);
	}

	// Blueprint draw
	ReceiveDrawHUD(Canvas->SizeX, Canvas->SizeY);
}

UFont* AHUD::GetFontFromSizeIndex(int32 FontSizeIndex) const
{
	switch (FontSizeIndex)
	{
	case 0: return GEngine->GetTinyFont();
	case 1: return GEngine->GetSmallFont();
	case 2: return GEngine->GetMediumFont();
	case 3: return GEngine->GetLargeFont();
	}

	return GEngine->GetLargeFont();
}



void AHUD::OnLostFocusPause(bool bEnable)
{
	if ( bLostFocusPaused == bEnable )
		return;

	if ( GetNetMode() != NM_Client )
	{
		bLostFocusPaused = bEnable;
		PlayerOwner->SetPause(bEnable);
	}
}

void AHUD::DrawDebugTextList()
{
	if( (DebugTextList.Num() > 0) && (DebugCanvas != nullptr ) )
	{
		FRotator CameraRot;
		FVector CameraLoc;
		PlayerOwner->GetPlayerViewPoint(CameraLoc, CameraRot);

		FCanvasTextItem TextItem( FVector2D::ZeroVector, FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White );
		for (int32 Idx = 0; Idx < DebugTextList.Num(); Idx++)
		{
			if (DebugTextList[Idx].SrcActor == NULL)
			{
				DebugTextList.RemoveAt(Idx--,1);
				continue;
			}

			if( DebugTextList[Idx].Font != NULL )
			{
				TextItem.Font = DebugTextList[Idx].Font;
			}
			else
			{
				TextItem.Font = GEngine->GetSmallFont();
			}

			float const Alpha = FMath::IsNearlyZero(DebugTextList[Idx].Duration) ? 0.f : (1.f - (DebugTextList[Idx].TimeRemaining / DebugTextList[Idx].Duration));
			FVector WorldTextLoc;
			if (DebugTextList[Idx].bAbsoluteLocation)
			{
				WorldTextLoc = FMath::Lerp(DebugTextList[Idx].SrcActorOffset, DebugTextList[Idx].SrcActorDesiredOffset, Alpha);
			}
			else
			{
				FVector Offset = FMath::Lerp(DebugTextList[Idx].SrcActorOffset, DebugTextList[Idx].SrcActorDesiredOffset, Alpha);

				if( DebugTextList[Idx].bKeepAttachedToActor )
				{
					WorldTextLoc = DebugTextList[Idx].SrcActor->GetActorLocation() + Offset;
				}
				else
				{
					WorldTextLoc = DebugTextList[Idx].OrigActorLocation + Offset;
				}
			}

			if (bEnableDebugTextShadow || DebugTextList[Idx].bDrawShadow)
			{
				TextItem.EnableShadow(FLinearColor::Black);
			}
			else
			{
				TextItem.DisableShadow();
			}
				
			// don't draw text behind the camera
			if ( ((WorldTextLoc - CameraLoc) | CameraRot.Vector()) > 0.f )
			{
				FVector ScreenLoc = Canvas->Project(WorldTextLoc);
				TextItem.SetColor( DebugTextList[Idx].TextColor );
				TextItem.Text = FText::FromString( DebugTextList[Idx].DebugText );
				TextItem.Scale = FVector2D( DebugTextList[Idx].FontScale, DebugTextList[Idx].FontScale);
				DebugCanvas->DrawItem( TextItem, FVector2D( FMath::CeilToFloat(ScreenLoc.X), FMath::CeilToFloat(ScreenLoc.Y) ) );
			}

			// do this at the end so even small durations get at least one frame
			if (DebugTextList[Idx].TimeRemaining != -1.f)
			{
				DebugTextList[Idx].TimeRemaining -= RenderDelta;
			}
		}

		// Clear out the list of expired ones (going from the back to reduce copying the remaining portion of the list as it is shrunk, since order matters)
		for (int32 Idx = DebugTextList.Num() - 1; Idx >= 0; --Idx)
		{
			if (DebugTextList[Idx].TimeRemaining != -1.f)
			{
				if (DebugTextList[Idx].TimeRemaining <= 0.f)
				{
					DebugTextList.RemoveAt(Idx, 1);
				}
			}
		}
	}
}

/// @cond DOXYGEN_WARNINGS

void AHUD::AddDebugText_Implementation(const FString& DebugText,
										 AActor* SrcActor,
										 float Duration,
										 FVector Offset,
										 FVector DesiredOffset,
										 FColor TextColor,
										 bool bSkipOverwriteCheck,
										 bool bAbsoluteLocation,
										 bool bKeepAttachedToActor,
										 UFont* InFont,
										 float FontScale,
										 bool bDrawShadow
										 )
{
	// set a default color
	if (TextColor == FColor::Transparent)
	{
		TextColor = FColor::White;
	}
	
	// and a default source actor of our pawn
	if (SrcActor != NULL)
	{
		if (DebugText.Len() == 0)
		{
			RemoveDebugText(SrcActor);
		}
		else
		{
			// search for an existing entry
			int32 Idx = 0;
			if (!bSkipOverwriteCheck)
			{
				Idx = INDEX_NONE;
				for (int32 i = 0; i < DebugTextList.Num() && Idx == INDEX_NONE; ++i)
				{
					if (DebugTextList[i].SrcActor == SrcActor)
					{
						Idx = i;
					}
				}
				if (Idx == INDEX_NONE)
				{
					// manually grow the array one struct element
					Idx = DebugTextList.Add(FDebugTextInfo());
				}
			}
			else
			{
				Idx = DebugTextList.Add(FDebugTextInfo());
			}
			// assign the new text and actor
			DebugTextList[Idx].SrcActor = SrcActor;
			DebugTextList[Idx].SrcActorOffset = Offset;
			DebugTextList[Idx].SrcActorDesiredOffset = DesiredOffset;
			DebugTextList[Idx].DebugText = DebugText;
			DebugTextList[Idx].TimeRemaining = Duration;
			DebugTextList[Idx].Duration = Duration;
			DebugTextList[Idx].TextColor = TextColor;
			DebugTextList[Idx].bAbsoluteLocation = bAbsoluteLocation;
			DebugTextList[Idx].bKeepAttachedToActor = bKeepAttachedToActor;
			DebugTextList[Idx].OrigActorLocation = SrcActor->GetActorLocation();
			DebugTextList[Idx].Font = InFont;
			DebugTextList[Idx].FontScale = FontScale;
			DebugTextList[Idx].bDrawShadow = bDrawShadow;
		}
	}
}

/** Remove debug text for the specific actor. */
void AHUD::RemoveDebugText_Implementation(AActor* SrcActor, bool bLeaveDurationText)
{
	int32 Idx = INDEX_NONE;
	for (int32 i = 0; i < DebugTextList.Num() && Idx == INDEX_NONE; ++i)
	{
		if (DebugTextList[i].SrcActor == SrcActor && (!bLeaveDurationText || DebugTextList[i].TimeRemaining == -1.f))
		{
			Idx = i;
		}
	}
	if (Idx != INDEX_NONE)
	{
		DebugTextList.RemoveAt(Idx,1);
	}
}

/** Remove all debug text */
void AHUD::RemoveAllDebugStrings_Implementation()
{
	DebugTextList.Reset();
}

/// @endcond

void AHUD::NotifyHitBoxClick(FName BoxName)
{
	// dispatch BP event
	ReceiveHitBoxClick(BoxName);
}

void AHUD::NotifyHitBoxRelease(FName BoxName)
{
	// dispatch BP event
	ReceiveHitBoxRelease(BoxName);
}

void AHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
	// dispatch BP event
	ReceiveHitBoxBeginCursorOver(BoxName);
}

void AHUD::NotifyHitBoxEndCursorOver(FName BoxName)
{
	// dispatch BP event
	ReceiveHitBoxEndCursorOver(BoxName);
}

void AHUD::GetTextSize(const FString& Text, float& OutWidth, float& OutHeight, class UFont* Font, float Scale) const
{
	if (IsCanvasValid_WarnIfNot())
	{
		Canvas->TextSize(Font ? Font : GEngine->GetMediumFont(), Text, OutWidth, OutHeight, Scale, Scale);
	}
}

void AHUD::DrawText(FString const& Text, FLinearColor Color, float ScreenX, float ScreenY, UFont* Font, float Scale, bool bScalePosition)
{
	if (IsCanvasValid_WarnIfNot())
	{
		if (bScalePosition)
		{
			ScreenX *= Scale;
			ScreenY *= Scale;
		}
		FCanvasTextItem TextItem( FVector2D( ScreenX, ScreenY ), FText::FromString(Text), Font ? Font : GEngine->GetMediumFont(), Color );
		TextItem.Scale = FVector2D( Scale, Scale );
		Canvas->DrawItem( TextItem );
	}
}

void AHUD::DrawMaterial(UMaterialInterface* Material, float ScreenX, float ScreenY, float ScreenW, float ScreenH, float MaterialU, float MaterialV, float MaterialUWidth, float MaterialVHeight, float Scale, bool bScalePosition, float Rotation, FVector2D RotPivot)
{
	if (IsCanvasValid_WarnIfNot() && Material)
	{
		FCanvasTileItem TileItem( FVector2D( ScreenX, ScreenY ), Material->GetRenderProxy(0), FVector2D( ScreenW, ScreenH ) * Scale, FVector2D( MaterialU, MaterialV ), FVector2D( MaterialU+MaterialUWidth, MaterialV +MaterialVHeight) );
		TileItem.Rotation = FRotator(0, Rotation, 0);
		TileItem.PivotPoint = RotPivot;
		if (bScalePosition)
		{
			TileItem.Position *= Scale;
		}
		Canvas->DrawItem( TileItem );
	}
}

void AHUD::DrawMaterialSimple(UMaterialInterface* Material, float ScreenX, float ScreenY, float ScreenW, float ScreenH, float Scale, bool bScalePosition)
{
	if (IsCanvasValid_WarnIfNot() && Material)
	{
		FCanvasTileItem TileItem( FVector2D( ScreenX, ScreenY ), Material->GetRenderProxy(0), FVector2D( ScreenW, ScreenH ) * Scale );
		if (bScalePosition)
		{
			TileItem.Position *= Scale;
		}
		Canvas->DrawItem( TileItem );
	}
}

void AHUD::DrawTexture(UTexture* Texture, float ScreenX, float ScreenY, float ScreenW, float ScreenH, float TextureU, float TextureV, float TextureUWidth, float TextureVHeight, FLinearColor Color, EBlendMode BlendMode, float Scale, bool bScalePosition, float Rotation, FVector2D RotPivot)
{
	if (IsCanvasValid_WarnIfNot() && Texture)
	{
		FCanvasTileItem TileItem( FVector2D( ScreenX, ScreenY ), Texture->Resource, FVector2D( ScreenW, ScreenH ) * Scale, FVector2D( TextureU, TextureV ), FVector2D( TextureU + TextureUWidth, TextureV + TextureVHeight ), Color );
		TileItem.Rotation = FRotator(0, Rotation, 0);
		TileItem.PivotPoint = RotPivot;
		if (bScalePosition)
		{
			TileItem.Position *= Scale;
		}
		TileItem.BlendMode = FCanvas::BlendToSimpleElementBlend( BlendMode );
		Canvas->DrawItem( TileItem );
	}
}

void AHUD::DrawTextureSimple(UTexture* Texture, float ScreenX, float ScreenY, float Scale, bool bScalePosition)
{
	if (IsCanvasValid_WarnIfNot() && Texture)
	{
		FCanvasTileItem TileItem( FVector2D( ScreenX, ScreenY ), Texture->Resource, FLinearColor::White );
		if (bScalePosition)
		{
			TileItem.Position *= Scale;
		}
		// Apply the scale to the size (which will have been setup from the texture in the constructor).
		TileItem.Size *= Scale;
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem( TileItem );
	}
}

void AHUD::DrawMaterialTriangle(UMaterialInterface* Material, FVector2D V0_Pos, FVector2D V1_Pos, FVector2D V2_Pos, FVector2D V0_UV, FVector2D V1_UV, FVector2D V2_UV, FLinearColor V0_Color, FLinearColor V1_Color, FLinearColor V2_Color)
{
	if (IsCanvasValid_WarnIfNot() && Material)
	{
		FCanvasTriangleItem TriangleItem(V0_Pos, V1_Pos, V2_Pos, V0_UV, V1_UV, V2_UV, NULL);
		TriangleItem.TriangleList[0].V0_Color = V0_Color;
		TriangleItem.TriangleList[0].V1_Color = V1_Color;
		TriangleItem.TriangleList[0].V2_Color = V2_Color;
		TriangleItem.MaterialRenderProxy = Material->GetRenderProxy(0);
		Canvas->DrawItem(TriangleItem);
	}
}
FVector AHUD::Project(FVector Location) const
{
	if (IsCanvasValid_WarnIfNot())
	{
		return Canvas->Project(Location);
	}
	return FVector(0,0,0);
}

void AHUD::Deproject(float ScreenX, float ScreenY, FVector& WorldPosition, FVector& WorldDirection) const
{
	WorldPosition = WorldDirection = FVector(0,0,0);
	if (IsCanvasValid_WarnIfNot())
	{
		Canvas->Deproject(FVector2D(ScreenX, ScreenY), WorldPosition, WorldDirection);
	}
}


void AHUD::GetActorsInSelectionRectangle(TSubclassOf<class AActor> ClassFilter, const FVector2D& FirstPoint, const FVector2D& SecondPoint, TArray<AActor*>& OutActors, bool bIncludeNonCollidingComponents, bool bActorMustBeFullyEnclosed)
{
	// Because this is a HUD function it is likely to get called each tick,
	// so make sure any previous contents of the out actor array have been cleared!
	OutActors.Reset();

	//Create Selection Rectangle from Points
	FBox2D SelectionRectangle(ForceInit);

	//This method ensures that an appropriate rectangle is generated, 
	//		no matter what the coordinates of first and second point actually are.
	SelectionRectangle += FirstPoint;
	SelectionRectangle += SecondPoint;


	//The Actor Bounds Point Mapping
	const FVector BoundsPointMapping[8] =
	{
		FVector(1.f, 1.f, 1.f),
		FVector(1.f, 1.f, -1.f),
		FVector(1.f, -1.f, 1.f),
		FVector(1.f, -1.f, -1.f),
		FVector(-1.f, 1.f, 1.f),
		FVector(-1.f, 1.f, -1.f),
		FVector(-1.f, -1.f, 1.f),
		FVector(-1.f, -1.f, -1.f)	};

	//~~~

	//For Each Actor of the Class Filter Type
	for (TActorIterator<AActor> Itr(GetWorld(), ClassFilter); Itr; ++Itr)
	{
		AActor* EachActor = *Itr;

		//Get Actor Bounds				//casting to base class, checked by template in the .h
		const FBox EachActorBounds = EachActor->GetComponentsBoundingBox(bIncludeNonCollidingComponents); /* All Components? */

		//Center
		const FVector BoxCenter = EachActorBounds.GetCenter();

		//Extents
		const FVector BoxExtents = EachActorBounds.GetExtent();

		// Build 2D bounding box of actor in screen space
		FBox2D ActorBox2D(ForceInit);
		for (uint8 BoundsPointItr = 0; BoundsPointItr < 8; BoundsPointItr++)
		{
			// Project vert into screen space.
			const FVector ProjectedWorldLocation = Project(BoxCenter + (BoundsPointMapping[BoundsPointItr] * BoxExtents));
			// Add to 2D bounding box
			ActorBox2D += FVector2D(ProjectedWorldLocation.X, ProjectedWorldLocation.Y);
		}

		//Selection Box must fully enclose the Projected Actor Bounds
		if (bActorMustBeFullyEnclosed)
		{
			if(SelectionRectangle.IsInside(ActorBox2D))
			{
				OutActors.Add(EachActor);
			}
		}
		//Partial Intersection with Projected Actor Bounds
		else
		{
			if (SelectionRectangle.Intersect(ActorBox2D))
			{
				OutActors.Add(EachActor);
			}
		}
	}
}

void AHUD::DrawRect(FLinearColor Color, float ScreenX, float ScreenY, float Width, float Height)
{
	if (IsCanvasValid_WarnIfNot())	
	{
		FCanvasTileItem TileItem( FVector2D(ScreenX, ScreenY), GWhiteTexture, FVector2D( Width, Height ), Color );
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem( TileItem );		
	}
}

void AHUD::DrawLine(float StartScreenX, float StartScreenY, float EndScreenX, float EndScreenY, FLinearColor LineColor, float LineThickness)
{
	if (IsCanvasValid_WarnIfNot())
	{
		FCanvasLineItem LineItem( FVector2D(StartScreenX, StartScreenY), FVector2D(EndScreenX, EndScreenY) );
		LineItem.SetColor( LineColor );
		LineItem.LineThickness = LineThickness;
		Canvas->DrawItem( LineItem );
	}
}


APlayerController* AHUD::GetOwningPlayerController() const
{
	return PlayerOwner;
}

APawn* AHUD::GetOwningPawn() const
{
	return PlayerOwner ? PlayerOwner->GetPawn() : NULL;
}

void AHUD::RenderHitBoxes( FCanvas* InCanvas )
{
	for (const FHUDHitBox& HitBox : HitBoxMap)
	{
		FLinearColor BoxColor = FLinearColor::White;
		if( HitBoxHits.Contains(const_cast<FHUDHitBox*>(&HitBox)))
		{
			BoxColor = FLinearColor::Red;
		}
		HitBox.Draw( InCanvas, BoxColor );
	}
}

void AHUD::UpdateHitBoxCandidates( TArray<FVector2D> InContactPoints )
{
	HitBoxHits.Reset();
	for (FHUDHitBox& HitBox : HitBoxMap)
	{
		bool bAdded = false;
		for (int32 ContactPointIndex = InContactPoints.Num() - 1; ContactPointIndex >= 0; --ContactPointIndex)
		{
			if (HitBox.Contains(InContactPoints[ContactPointIndex]))
			{
				if (!bAdded)
				{
					HitBoxHits.Add(&HitBox);
					bAdded = true;
				}
				if (HitBox.ConsumesInput())
				{
					InContactPoints.RemoveAtSwap(ContactPointIndex);
				}
				else
				{
					break;
				}
			}
		}
		if (InContactPoints.Num() == 0)
		{
			break;
		}
	}

	TSet<FName> NotOverHitBoxes = HitBoxesOver;
	TArray<FName> NewlyOverHitBoxes;

	// Now figure out which boxes we are over and deal with begin/end cursor over messages 
	for (FHUDHitBox* HitBox : HitBoxHits)
	{
		const FName HitBoxName = HitBox->GetName();
		if (HitBoxesOver.Contains(HitBoxName))
		{
			NotOverHitBoxes.Remove(HitBoxName);
		}
		else
		{
			NewlyOverHitBoxes.AddUnique(HitBoxName);
		}
	}

	// Dispatch the end cursor over messages
	for (const FName HitBoxName : NotOverHitBoxes)
	{
		NotifyHitBoxEndCursorOver(HitBoxName);
		HitBoxesOver.Remove(HitBoxName);
	}

	// Dispatch the newly over hitbox messages
	for (const FName HitBoxName : NewlyOverHitBoxes)
	{
		NotifyHitBoxBeginCursorOver(HitBoxName);
		HitBoxesOver.Add(HitBoxName);
	}
}

const FHUDHitBox* AHUD::GetHitBoxAtCoordinates( FVector2D InHitLocation, const bool bIsConsumingInput ) const
{
	if (HitBoxMap.Num() > 0)
	{
		InHitLocation -= GetCoordinateOffset();

		for (const FHUDHitBox& HitBox : HitBoxMap)
		{
			if( (!bIsConsumingInput || HitBox.ConsumesInput()) && HitBox.Contains( InHitLocation ) )
			{
				return &HitBox;
			}
		}
	}
	return nullptr;
}

void AHUD::GetHitBoxesAtCoordinates(FVector2D InHitLocation, TArray<const FHUDHitBox*>& OutHitBoxes) const
{
	OutHitBoxes.Reset();

	if (HitBoxMap.Num() > 0)
	{
		InHitLocation -= GetCoordinateOffset();

		for (const FHUDHitBox& HitBox : HitBoxMap)
		{
			if (HitBox.Contains(InHitLocation))
			{
				OutHitBoxes.Add(&HitBox);
			}
		}
	}
}

const FHUDHitBox* AHUD::GetHitBoxWithName( const FName InName ) const
{
	for (const FHUDHitBox& HitBox : HitBoxMap)
	{
		if( HitBox.GetName() == InName )
		{
			return &HitBox;
		}
	}
	return nullptr;
}

bool AHUD::AnyCurrentHitBoxHits() const
{
	return HitBoxHits.Num() != 0;
}

bool AHUD::UpdateAndDispatchHitBoxClickEvents(FVector2D ClickLocation, const EInputEvent InEventType)
{
	const bool bIsClickEvent = (InEventType == IE_Pressed || InEventType == IE_DoubleClick);

	// Early out to avoid unnecessary expense of calling GetCoordinateOffset
	if ((bIsClickEvent && HitBoxMap.Num() == 0) || (!bIsClickEvent && HitBoxHits.Num() == 0))
	{
		return false;
	}

	ClickLocation += GetCoordinateOffset();

	bool bHit = false;

	// If this is a click event we may not have the hit box in the hit list yet (particularly for touch events) so we need to check all HitBoxes
	if (bIsClickEvent)
	{
		for (FHUDHitBox& HitBox : HitBoxMap)
		{
			if (HitBox.Contains(ClickLocation))
			{
				bHit = true;

				NotifyHitBoxClick(HitBox.GetName());

				if (HitBox.ConsumesInput())
				{
					break;	//Early out if this box consumed the click
				}
			}
		}
	}
	else
	{
		for (FHUDHitBox* HitBoxHit : HitBoxHits)
		{
			if (HitBoxHit->Contains(ClickLocation))
			{
				bHit = true;

				if (InEventType == IE_Released)
				{
					NotifyHitBoxRelease(HitBoxHit->GetName());
				}

				if (HitBoxHit->ConsumesInput() == true)
				{
					break;	//Early out if this box consumed the click
				}
			}
		}
	}
	return bHit;
}

void AHUD::AddHitBox(FVector2D Position, FVector2D Size, FName Name, bool bConsumesInput, int32 Priority)
{	
	if( GetHitBoxWithName(Name) == nullptr )
	{
		bool bAdded = false;
		for (int32 Index = 0; Index < HitBoxMap.Num(); ++Index)
		{
			if (HitBoxMap[Index].GetPriority() < Priority)
			{
				HitBoxMap.Insert(FHUDHitBox(Position, Size, Name, bConsumesInput, Priority), Index);
				bAdded = true;
				break;
			}
		}
		if (!bAdded)
		{
			HitBoxMap.Add(FHUDHitBox(Position, Size, Name, bConsumesInput, Priority));
		}
	}
	else
	{
		UE_LOG(LogHUD, Warning, TEXT("Failed to add hitbox named %s as a hitbox with this name already exists"), *Name.ToString());
	}
}

bool AHUD::IsCanvasValid_WarnIfNot() const
{
	const bool bIsValid = Canvas != NULL;
	if (!bIsValid)
	{
		FMessageLog("PIE").Warning()
			->AddToken(FUObjectToken::Create(const_cast<AHUD*>(this)))
			->AddToken(FTextToken::Create(LOCTEXT( "PIE_Warning_Message_CanvasCallOutsideOfDrawCanvas", "Canvas Draw functions may only be called during the handling of the DrawHUD event" )));
	}

	return bIsValid;
}


#undef LOCTEXT_NAMESPACE
