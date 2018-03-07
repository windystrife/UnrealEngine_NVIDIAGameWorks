// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditorModeActorPicker.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SToolTip.h"
#include "EngineUtils.h"
#include "LevelEditorViewport.h"
#include "EditorModes.h"

#define LOCTEXT_NAMESPACE "PropertyPicker"

FEdModeActorPicker::FEdModeActorPicker()
{
	PickState = EPickState::NotOverViewport;
	HoveredActor.Reset();
}

void FEdModeActorPicker::Initialize()
{
	CursorDecoratorWindow = SWindow::MakeCursorDecorator();
	FSlateApplication::Get().AddWindow(CursorDecoratorWindow.ToSharedRef(), true);
	CursorDecoratorWindow->SetContent(
		SNew(SToolTip)
		.Text(this, &FEdModeActorPicker::GetCursorDecoratorText)
		);
}

void FEdModeActorPicker::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	if(CursorDecoratorWindow.IsValid())
	{
		CursorDecoratorWindow->MoveWindowTo(FSlateApplication::Get().GetCursorPos() + FSlateApplication::Get().GetCursorSize());
	}

	FEdMode::Tick(ViewportClient, DeltaTime);
}

bool FEdModeActorPicker::MouseEnter(FEditorViewportClient* ViewportClient, FViewport* Viewport,int32 x, int32 y)
{
	PickState = EPickState::OverViewport;
	HoveredActor.Reset();
	return FEdMode::MouseEnter(ViewportClient, Viewport, x, y);
}

bool FEdModeActorPicker::MouseLeave(FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	PickState = EPickState::NotOverViewport;
	HoveredActor.Reset();
	return FEdMode::MouseLeave(ViewportClient, Viewport);
}

bool FEdModeActorPicker::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y)
{
	if (ViewportClient == GCurrentLevelEditingViewportClient)
	{
		PickState = EPickState::OverViewport;
		HoveredActor.Reset();

		int32 HitX = Viewport->GetMouseX();
		int32 HitY = Viewport->GetMouseY();
		HHitProxy* HitProxy = Viewport->GetHitProxy(HitX, HitY);
		if (HitProxy != NULL && HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorHit = static_cast<HActor*>(HitProxy);
			if(ActorHit->Actor != NULL)
			{
				AActor* Actor = ActorHit->Actor;
				HoveredActor = Actor;
				PickState =  IsActorValid(Actor) ? EPickState::OverActor : EPickState::OverIncompatibleActor;
			}
		}
	}
	else
	{
		PickState = EPickState::NotOverViewport;
		HoveredActor.Reset();
	}

	return true;
}

bool FEdModeActorPicker::LostFocus(FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	if (ViewportClient == GCurrentLevelEditingViewportClient)
	{
		// Make sure actor picking mode is disabled once the active viewport loses focus
		RequestDeletion();
		return true;
	}

	return false;
}

bool FEdModeActorPicker::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (ViewportClient == GCurrentLevelEditingViewportClient)
	{
		if (Key == EKeys::LeftMouseButton && Event == IE_Pressed)
		{
			// See if we clicked on an actor
			int32 HitX = Viewport->GetMouseX();
			int32 HitY = Viewport->GetMouseY();
			HHitProxy*	HitProxy = Viewport->GetHitProxy(HitX, HitY);
			if (HitProxy != NULL && HitProxy->IsA(HActor::StaticGetType()))
			{
				HActor* ActorHit = static_cast<HActor*>(HitProxy);
				if(IsActorValid(ActorHit->Actor))
				{
					OnActorSelected.ExecuteIfBound(ActorHit->Actor);
					RequestDeletion();
				}
			}
			return true;
		}
		else if(Key == EKeys::Escape && Event == IE_Pressed)
		{
			RequestDeletion();
			return true;
		}
	}
	else
	{
		RequestDeletion();
	}

	return false;
}

bool FEdModeActorPicker::GetCursor(EMouseCursor::Type& OutCursor) const
{
	if(HoveredActor.IsValid() && PickState == EPickState::OverActor)
	{
		OutCursor = EMouseCursor::EyeDropper;
	}
	else
	{
		OutCursor = EMouseCursor::SlashedCircle;
	}
	
	return true;
}

bool FEdModeActorPicker::UsesToolkits() const
{
	return false;
}

bool FEdModeActorPicker::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	// We want to be able to perform this action with all the built-in editor modes
	return OtherModeID != FBuiltinEditorModes::EM_None;
}

void FEdModeActorPicker::Exit()
{
	OnActorSelected = FOnActorSelected();
	OnGetAllowedClasses = FOnGetAllowedClasses();
	OnShouldFilterActor = FOnShouldFilterActor();

	if (CursorDecoratorWindow.IsValid())
	{
		CursorDecoratorWindow->RequestDestroyWindow();
		CursorDecoratorWindow.Reset();
	}

	HoveredActor.Reset();
	PickState = EPickState::NotOverViewport;

	FEdMode::Exit();
}

FText FEdModeActorPicker::GetCursorDecoratorText() const
{
	switch(PickState)
	{
	default:
	case EPickState::NotOverViewport:
		return LOCTEXT("PickActor_NotOverViewport", "Pick an actor by clicking on it in the active level viewport");
	case EPickState::OverViewport:
		return LOCTEXT("PickActor_NotOverActor", "Pick an actor by clicking on it");
	case EPickState::OverIncompatibleActor:
		{
			if(HoveredActor.IsValid())
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Actor"), FText::FromString(HoveredActor.Get()->GetName()));
				return FText::Format(LOCTEXT("PickActor_OverIncompatibleActor", "{Actor} is incompatible"), Arguments);
			}
			else
			{
				return LOCTEXT("PickActor_NotOverActor", "Pick an actor by clicking on it");
			}
		}
	case EPickState::OverActor:
		{
			if(HoveredActor.IsValid())
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Actor"), FText::FromString(HoveredActor.Get()->GetName()));
				return FText::Format(LOCTEXT("PickActor_OverActor", "Pick {Actor}"), Arguments);
			}
			else
			{
				return LOCTEXT("PickActor_NotOverActor", "Pick an actor by clicking on it");
			}
		}
	}
}

bool FEdModeActorPicker::IsActorValid(const AActor *const Actor) const
{
	bool bIsValid = false;

	if(Actor)
	{
		bool bHasValidClass = true;
		if(OnGetAllowedClasses.IsBound())
		{
			bHasValidClass = false;

			TArray<const UClass*> AllowedClasses;
			OnGetAllowedClasses.Execute(AllowedClasses);
			for(const UClass* AllowedClass : AllowedClasses)
			{
				if(Actor->IsA(AllowedClass))
				{
					bHasValidClass = true;
					break;
				}
			}
		}

		bool bHasValidActor = true;
		if(OnShouldFilterActor.IsBound())
		{
			bHasValidActor = OnShouldFilterActor.Execute(Actor);
		}

		bIsValid = bHasValidClass && bHasValidActor;
	}

	return bIsValid;
}

#undef LOCTEXT_NAMESPACE
