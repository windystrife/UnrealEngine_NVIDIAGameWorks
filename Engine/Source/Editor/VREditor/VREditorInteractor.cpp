// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorInteractor.h"
#include "EngineUtils.h"
#include "ViewportWorldInteraction.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "VREditorMode.h"
#include "VREditorFloatingText.h"
#include "VREditorFloatingUI.h"
#include "VREditorDockableWindow.h"
#include "ActorTransformer.h"

#define LOCTEXT_NAMESPACE "VREditor"

UVREditorInteractor::UVREditorInteractor() :
	Super(),
	VRMode( nullptr ),
	bIsModifierPressed( false ),
	SelectAndMoveTriggerValue( 0.0f ),
	bHasUIOnForearm( false ),
	bIsClickingOnUI( false ),
	bIsRightClickingOnUI( false ),
	bIsHoveringOverUI( false ),
	UIScrollVelocity( 0.0f ),
	LastUIPressTime( 0.0f ),
	bWantHelpLabels( false ),
	HelpLabelShowOrHideStartTime( FTimespan::MinValue() )
{

}

void UVREditorInteractor::Init( UVREditorMode* InVRMode )
{
	VRMode = InVRMode;
	KeyToActionMap.Reset();
}

void UVREditorInteractor::Shutdown()
{
	for ( auto& KeyAndValue : HelpLabels )
	{
		AFloatingText* FloatingText = KeyAndValue.Value;
		GetVRMode().DestroyTransientActor(FloatingText);
	}

	HelpLabels.Empty();	

	VRMode = nullptr;

	Super::Shutdown();
}

void UVREditorInteractor::SetControllerType( const EControllerType& InControllerType )
{
	this->ControllerType = InControllerType;
}

void UVREditorInteractor::Tick( const float DeltaTime )
{
	Super::Tick( DeltaTime );

	// If the other controller is dragging freely, the UI controller can assist
	if( ControllerType == EControllerType::UI )
	{
		if( GetOtherInteractor() &&
			( GetOtherInteractor()->GetDraggingMode() == EViewportInteractionDraggingMode::TransformablesFreely ) )
		{
			ControllerType = EControllerType::AssistingLaser;
		}
	}
	// Otherwise the UI controller resets to a UI controller
	// Allow for "trading off" during an assisted drag
	else if( ControllerType == EControllerType::AssistingLaser )
	{
		if( GetOtherInteractor() &&
			!( GetOtherInteractor()->GetDraggingMode() == EViewportInteractionDraggingMode::TransformablesFreely ||
				GetOtherInteractor()->GetInteractorData().bWasAssistingDrag ) )
		{
			ControllerType = EControllerType::UI;
		}
	}
}


FHitResult UVREditorInteractor::GetHitResultFromLaserPointer( TArray<AActor*>* OptionalListOfIgnoredActors /*= nullptr*/, const bool bIgnoreGizmos /*= false*/,
	TArray<UClass*>* ObjectsInFrontOfGizmo /*= nullptr */, const bool bEvenIfBlocked /*= false */, const float LaserLengthOverride /*= 0.0f */ )
{
	static TArray<AActor*> IgnoredActors;
	IgnoredActors.Reset();
	if ( OptionalListOfIgnoredActors == nullptr )
	{
		OptionalListOfIgnoredActors = &IgnoredActors;
	}

	// Ignore UI widgets too
	if ( GetDraggingMode() == EViewportInteractionDraggingMode::TransformablesAtLaserImpact )
	{
		for( TActorIterator<AVREditorFloatingUI> UIActorIt( WorldInteraction->GetWorld() ); UIActorIt; ++UIActorIt )
		{
			OptionalListOfIgnoredActors->Add( *UIActorIt );
		}
	}

	static TArray<UClass*> PriorityOverGizmoObjects;
	PriorityOverGizmoObjects.Reset();
	if ( ObjectsInFrontOfGizmo == nullptr )
	{
		ObjectsInFrontOfGizmo = &PriorityOverGizmoObjects;
	}

	ObjectsInFrontOfGizmo->Add( AVREditorDockableWindow::StaticClass() );
	ObjectsInFrontOfGizmo->Add( AVREditorFloatingUI::StaticClass() );

	return UViewportInteractor::GetHitResultFromLaserPointer( OptionalListOfIgnoredActors, bIgnoreGizmos, ObjectsInFrontOfGizmo, bEvenIfBlocked, LaserLengthOverride );
}

void UVREditorInteractor::PreviewInputKey( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const EInputEvent Event, bool& bOutWasHandled )
{
	// Update modifier state
	if( Action.ActionType == VRActionTypes::Modifier )
	{
		if( Event == IE_Pressed )
		{
			bIsModifierPressed = true;
		}
		else if( Event == IE_Released )
		{
			bIsModifierPressed = false;
		}
	}

	if( !bOutWasHandled )
	{
		Super::PreviewInputKey( ViewportClient, Action, Key, Event, bOutWasHandled );
	}
}

void UVREditorInteractor::ResetHoverState()
{
	Super::ResetHoverState();
	bIsHoveringOverUI = false;
}	

float UVREditorInteractor::GetSlideDelta()
{
	return 0.0f;
}

bool UVREditorInteractor::IsHoveringOverUI() const
{
	return bIsHoveringOverUI;
}

void UVREditorInteractor::SetHasUIOnForearm( const bool bInHasUIOnForearm )
{
	bHasUIOnForearm = bInHasUIOnForearm;
}

bool UVREditorInteractor::HasUIOnForearm() const
{
	return bHasUIOnForearm;
}

UWidgetComponent* UVREditorInteractor::GetLastHoveredWidgetComponent() const
{
	return InteractorData.LastHoveredWidgetComponent.Get();
}

void UVREditorInteractor::SetLastHoveredWidgetComponent( UWidgetComponent* NewHoveringOverWidgetComponent )
{
	InteractorData.LastHoveredWidgetComponent = NewHoveringOverWidgetComponent;
}

bool UVREditorInteractor::IsModifierPressed() const
{
	return bIsModifierPressed;
}

void UVREditorInteractor::SetIsClickingOnUI( const bool bInIsClickingOnUI )
{
	bIsClickingOnUI = bInIsClickingOnUI;
}

bool UVREditorInteractor::IsClickingOnUI() const
{
	return bIsClickingOnUI;
}

void UVREditorInteractor::SetIsHoveringOverUI( const bool bInIsHoveringOverUI )
{
	bIsHoveringOverUI = bInIsHoveringOverUI;
}

void UVREditorInteractor::SetIsRightClickingOnUI( const bool bInIsRightClickingOnUI )
{
	bIsRightClickingOnUI = bInIsRightClickingOnUI;
}

bool UVREditorInteractor::IsRightClickingOnUI() const
{
	return bIsRightClickingOnUI;
}

void UVREditorInteractor::SetLastUIPressTime( const double InLastUIPressTime )
{
	LastUIPressTime = InLastUIPressTime;
}

double UVREditorInteractor::GetLastUIPressTime() const
{
	return LastUIPressTime;
}

void UVREditorInteractor::SetUIScrollVelocity( const float InUIScrollVelocity )
{
	UIScrollVelocity = InUIScrollVelocity;
}

float UVREditorInteractor::GetUIScrollVelocity() const
{
	return UIScrollVelocity;
}

float UVREditorInteractor::GetSelectAndMoveTriggerValue() const
{
	return SelectAndMoveTriggerValue;
}

bool UVREditorInteractor::GetIsLaserBlocked() const
{
	return Super::GetIsLaserBlocked() || bHasUIInFront;
}

#undef LOCTEXT_NAMESPACE
