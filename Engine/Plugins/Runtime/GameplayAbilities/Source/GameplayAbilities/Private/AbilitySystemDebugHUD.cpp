// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemDebugHUD.h"
#include "GameFramework/PlayerController.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Debug/DebugDrawService.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"

AAbilitySystemDebugHUD::AAbilitySystemDebugHUD(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void AAbilitySystemDebugHUD::DrawWithBackground(UFont* InFont, const FString& Text, const FColor& TextColor, EAlignHorizontal::Type HAlign, float& OffsetX, EAlignVertical::Type VAlign, float& OffsetY, float Alpha)
{
	float SizeX, SizeY;
	Canvas->StrLen(InFont, Text, SizeX, SizeY);

	const float PosX = (HAlign == EAlignHorizontal::Center) ? OffsetX + (Canvas->ClipX - SizeX) * 0.5f :
		(HAlign == EAlignHorizontal::Left) ? Canvas->OrgX + OffsetX :
		Canvas->ClipX - SizeX - OffsetX;

	const float PosY = (VAlign == EAlignVertical::Center) ? OffsetY + (Canvas->ClipY - SizeY) * 0.5f :
		(VAlign == EAlignVertical::Top) ? Canvas->OrgY + OffsetY :
		Canvas->ClipY - SizeY - OffsetY;

	const float BoxPadding = 5.0f;

	const float X = PosX - BoxPadding;
	const float Y = PosY - BoxPadding;
	const float Z = 0.1f;
	FCanvasTileItem TileItem(FVector2D(X, Y), FVector2D(SizeX + BoxPadding * 2.0f, SizeY + BoxPadding * 2.0f), FLinearColor(0.75f, 0.75f, 0.75f, Alpha));
	Canvas->DrawItem(TileItem);

	FLinearColor TextCol(TextColor);
	TextCol.A = Alpha;
	FCanvasTextItem TextItem(FVector2D(PosX, PosY), FText::FromString(Text), GEngine->GetSmallFont(), TextCol);
	Canvas->DrawItem(TextItem);

	OffsetY += 25;
}

void AAbilitySystemDebugHUD::DrawDebugHUD(UCanvas* InCanvas, APlayerController*)
{
	Canvas = InCanvas;
	if (!Canvas)
	{
		return;
	}

	UPlayer * LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		return;
	}

	APlayerController *PC = LocalPlayer->PlayerController;
	if (!PC)
	{
		return;
	}

	if (PC->GetPawn())
	{
		UAbilitySystemComponent * AbilitySystemComponent = PC->GetPawn()->FindComponentByClass<UAbilitySystemComponent>();
		if (AbilitySystemComponent)
		{
			DrawDebugAbilitySystemComponent(AbilitySystemComponent);
		}
	}
}

void AAbilitySystemDebugHUD::DrawDebugAbilitySystemComponent(UAbilitySystemComponent *Component)
{
	UWorld *World = GetWorld();
	float GameWorldTime = World->GetTimeSeconds();

	UFont* Font = GEngine->GetSmallFont();
	FColor Color(38, 128, 0);
	float X = 20.f;
	float Y = 20.f;

	FString String = FString::Printf(TEXT("%.2f"), Component->GetWorld()->GetTimeSeconds());
	DrawWithBackground(Font, String, Color, EAlignHorizontal::Left, X, EAlignVertical::Top, Y);

	String = FString::Printf(TEXT("%s (%d)"), *Component->GetPathName(), Component->IsDefaultSubobject());
	DrawWithBackground(Font, String, Color, EAlignHorizontal::Left, X, EAlignVertical::Top, Y);

	
	String = FString::Printf(TEXT("%s == %s"), *Component->GetArchetype()->GetPathName(), *Component->GetClass()->GetDefaultObject()->GetPathName());
	DrawWithBackground(Font, String, Color, EAlignHorizontal::Left, X, EAlignVertical::Top, Y);


	for (const UAttributeSet* Set : Component->SpawnedAttributes)
	{
		if (!Set)
			continue;
		check(Set);

		// Draw Attribute Set
		DrawWithBackground(Font, FString::Printf(TEXT("%s (%d)"), *Set->GetName(), Set->IsDefaultSubobject()), Color, EAlignHorizontal::Left, X, EAlignVertical::Top, Y);

		String = FString::Printf(TEXT("%s == %s"), *Set->GetArchetype()->GetPathName(), *Set->GetClass()->GetDefaultObject()->GetPathName());
		DrawWithBackground(Font, String, Color, EAlignHorizontal::Left, X, EAlignVertical::Top, Y);

		for (TFieldIterator<UProperty> PropertyIt(Set->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty *Prop = *PropertyIt;			

			FString ValueString;
			const void *PropertyValue = Prop->ContainerPtrToValuePtr<void>(Set);
			Prop->ExportTextItem(ValueString, PropertyValue, NULL, NULL, 0);

			String = FString::Printf(TEXT("%s: %s"), *Prop->GetName(), *ValueString);
			DrawWithBackground(Font, String, Color, EAlignHorizontal::Left, X, EAlignVertical::Top, Y);
		}

		Y+= 25;
		// Draw Active GameplayEffect
		for (FActiveGameplayEffect& Effect : &Component->ActiveGameplayEffects)
		{
			String = FString::Printf(TEXT("%s. [%d, %d] %.2f"), *Effect.Spec.ToSimpleString(), Effect.PredictionKey.Current, Effect.PredictionKey.Base, Effect.GetTimeRemaining(GameWorldTime));
			DrawWithBackground(Font, String, Color, EAlignHorizontal::Left, X, EAlignVertical::Top, Y);	
		}
	}
	
}

#if !UE_BUILD_SHIPPING
static void	ToggleDebugHUD(const TArray<FString>& Args, UWorld* InWorld)
{

	if (!InWorld)
		return;

	AAbilitySystemDebugHUD *HUD = NULL;

	for (TActorIterator<AAbilitySystemDebugHUD> It(InWorld); It; ++It)
	{
		HUD = *It;
		break;
	}

	static FDelegateHandle DrawDebugDelegateHandle;

	if (!HUD)
	{
		HUD = InWorld->SpawnActor<AAbilitySystemDebugHUD>();
		
		FDebugDrawDelegate DrawDebugDelegate = FDebugDrawDelegate::CreateUObject(HUD, &AAbilitySystemDebugHUD::DrawDebugHUD);
		DrawDebugDelegateHandle = UDebugDrawService::Register(TEXT("GameplayDebug"), DrawDebugDelegate);
	}
	else
	{
		FDebugDrawDelegate DrawDebugDelegate = FDebugDrawDelegate::CreateUObject(HUD, &AAbilitySystemDebugHUD::DrawDebugHUD);
		UDebugDrawService::Unregister(DrawDebugDelegateHandle);
		HUD->Destroy();
	}
}

FAutoConsoleCommandWithWorldAndArgs AbilitySystemToggleDebugHUDCommand(
	TEXT("AbilitySystem.ToggleDebugHUD"),
	TEXT("ToggleDebugHUD Drawing"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(ToggleDebugHUD)
	);
#endif // !UE_BUILD_SHIPPING
