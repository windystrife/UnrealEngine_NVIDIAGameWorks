// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorDockableWindow.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VREditorUISystem.h"
#include "VREditorMode.h"
#include "VREditorWidgetComponent.h"
#include "ViewportWorldInteraction.h"
#include "VREditorInteractor.h"
#include "VREditorAssetContainer.h"

namespace VREd
{
	static FAutoConsoleVariable DockWindowThickness( TEXT( "VREd.DockWindowTickness" ), 1.0f, TEXT( "How thick the window is" ) );
	static FAutoConsoleVariable DockUISelectionBarVerticalOffset( TEXT( "VREd.DockUISelectionBarVerticalOffset" ), 2.0f, TEXT( "Z Distance between the selectionbar and the UI" ) );
	static FAutoConsoleVariable DockUIFadeAnimationDuration( TEXT( "VREd.DockUIFadeAnimationDuration" ), 0.15f, TEXT( "How quick the fade animation should complete in" ) );
	static FAutoConsoleVariable DockUIHoverScale( TEXT( "VREd.DockUIHoverScale" ), 1.1f, TEXT( "How big the selection bar gets when you hover over it" ) );
	static FAutoConsoleVariable DockUIHoverAnimationDuration( TEXT( "VREd.DockUIHoverAnimationDuration" ), 0.15f, TEXT( "How quick the hover animation should complete in" ) );
	static FAutoConsoleVariable DockUIDragSmoothingAmount( TEXT( "VREd.DockUIDragSmoothingAmount" ), 0.85f, TEXT( "How much to smooth out motion when dragging UI (frame rate sensitive)" ) );
}


AVREditorDockableWindow::AVREditorDockableWindow() : 
	Super(),
	bIsLaserAimingTowardUI(false),
	AimingAtMeFadeAlpha(0.0f),
	bIsHoveringOverSelectionBar(false),
	SelectionBarHoverAlpha(0.0f),
	bIsHoveringOverCloseButton(false),
	CloseButtonHoverAlpha(0.0f),
	DockSelectDistance(0.0f)
{
	UVREditorAssetContainer* AssetContainer = LoadObject<UVREditorAssetContainer>(nullptr, *UVREditorMode::AssetContainerPath);

	{
		UStaticMesh* WindowMesh = AssetContainer->WindowMesh;
		WindowMeshComponent->SetStaticMesh(WindowMesh);
		check(WindowMeshComponent != nullptr);
	}

	UMaterialInterface* HoverMaterial = AssetContainer->WindowMaterial;
	UMaterialInterface* TranslucentHoverMaterial = AssetContainer->WindowMaterial;

	const FRotator RelativeRotation(30.f, 0.f, 0.f);
	{
		UStaticMesh* SelectionMesh = AssetContainer->WindowSelectionBarMesh;

		SelectionBarMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "SelectionBarMesh" ) );
		SelectionBarMeshComponent->SetStaticMesh( SelectionMesh );
		SelectionBarMeshComponent->SetMobility( EComponentMobility::Movable );
		SelectionBarMeshComponent->SetupAttachment( RootComponent );

		SelectionBarMeshComponent->bGenerateOverlapEvents = false;
		SelectionBarMeshComponent->SetCanEverAffectNavigation( false );
		SelectionBarMeshComponent->bCastDynamicShadow = false;
		SelectionBarMeshComponent->bCastStaticShadow = false;
		SelectionBarMeshComponent->bAffectDistanceFieldLighting = false;
		SelectionBarMeshComponent->SetRelativeRotation(RelativeRotation);


		SelectionBarMID = UMaterialInstanceDynamic::Create( HoverMaterial, GetTransientPackage() );
		check( SelectionBarMID != nullptr );
		SelectionBarMeshComponent->SetMaterial( 0, SelectionBarMID );
		SelectionBarTranslucentMID = UMaterialInstanceDynamic::Create( TranslucentHoverMaterial, GetTransientPackage() );
		check( SelectionBarTranslucentMID != nullptr );
		SelectionBarMeshComponent->SetMaterial( 1, SelectionBarTranslucentMID );
	}

	{
		UStaticMesh* CloseButtonMesh = AssetContainer->WindowCloseButtonMesh;

		CloseButtonMeshComponent= CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "CloseButtonMesh" ) );
		CloseButtonMeshComponent->SetStaticMesh( CloseButtonMesh );
		CloseButtonMeshComponent->SetMobility( EComponentMobility::Movable );
		CloseButtonMeshComponent->SetupAttachment( RootComponent );

		CloseButtonMeshComponent->bGenerateOverlapEvents = false;
		CloseButtonMeshComponent->SetCanEverAffectNavigation( false );
		CloseButtonMeshComponent->bCastDynamicShadow = false;
		CloseButtonMeshComponent->bCastStaticShadow = false;
		CloseButtonMeshComponent->bAffectDistanceFieldLighting = false;
		CloseButtonMeshComponent->SetRelativeRotation(RelativeRotation);

		CloseButtonMID = UMaterialInstanceDynamic::Create( HoverMaterial, GetTransientPackage() );
		check( CloseButtonMID != nullptr );
		CloseButtonMeshComponent->SetMaterial( 0, CloseButtonMID );
		CloseButtonTranslucentMID = UMaterialInstanceDynamic::Create( TranslucentHoverMaterial, GetTransientPackage() );
		check( CloseButtonTranslucentMID != nullptr );
		CloseButtonMeshComponent->SetMaterial( 1, CloseButtonTranslucentMID );
	}

	// The selection bar and close button will not be initially visible.  They'll appear when the user aims
	// their laser toward the UI
	SelectionBarMeshComponent->SetVisibility( false );
	CloseButtonMeshComponent->SetVisibility( false );

	// Create the drag operation
	DragOperationComponent = CreateDefaultSubobject<UViewportDragOperationComponent>( TEXT( "DragOperation" ) );
	DragOperationComponent->SetDragOperationClass( UDockableWindowDragOperation::StaticClass() );
}

void AVREditorDockableWindow::SetupWidgetComponent()
{
	Super::SetupWidgetComponent();

	SetSelectionBarColor( GetOwner().GetOwner().GetColor( UVREditorMode::EColors::UISelectionBarColor ) );
	SetCloseButtonColor( GetOwner().GetOwner().GetColor( UVREditorMode::EColors::UICloseButtonColor ) );
}


void AVREditorDockableWindow::SetCollision(const ECollisionEnabled::Type InCollisionType, const ECollisionResponse InCollisionResponse, const ECollisionChannel InCollisionChannel)
{
	AVREditorFloatingUI::SetCollision(InCollisionType, InCollisionResponse, InCollisionChannel);

	SelectionBarMeshComponent->SetCollisionEnabled(InCollisionType);
	SelectionBarMeshComponent->SetCollisionResponseToAllChannels(InCollisionResponse);
	SelectionBarMeshComponent->SetCollisionObjectType(InCollisionChannel);
	CloseButtonMeshComponent->SetCollisionEnabled(InCollisionType);
	CloseButtonMeshComponent->SetCollisionResponseToAllChannels(InCollisionResponse);
}


void AVREditorDockableWindow::TickManually( float DeltaTime )
{
	Super::TickManually( DeltaTime );

	if( WidgetComponent->IsVisible() )
	{
		const FVector2D Size = GetSize();
		const float WorldScaleFactor = GetOwner().GetOwner().GetWorldScaleFactor();
		const FVector AnimatedScale = CalculateAnimatedScale();

		// Update whether the user is aiming toward us or not
		bIsLaserAimingTowardUI = false;

		if (!GetOwner().IsDraggingDockUI())
		{
			const FTransform UICapsuleTransform = this->GetActorTransform();

			const FVector UICapsuleStart = FVector( 0.0f, 0.0f, -Size.Y * 0.4f ) * WorldScaleFactor * AnimatedScale;
			const FVector UICapsuleEnd = FVector( 0.0f, 0.0f, Size.Y * 0.4f ) * WorldScaleFactor * AnimatedScale;
			const float UICapsuleLocalRadius = Size.X * 0.5f * WorldScaleFactor * AnimatedScale.Y;
			const float MinDistanceToUICapsule = 10.0f * WorldScaleFactor * AnimatedScale.Y;	// @todo vreditor tweak
			const FVector UIForwardVector = FVector::ForwardVector;
			const float MinDotForAimingAtOtherHand = -1.1f;	// @todo vreditor tweak

			for( UViewportInteractor* Interactor : GetOwner().GetOwner().GetWorldInteraction().GetInteractors() )
			{
				if( GetOwner().GetOwner().IsHandAimingTowardsCapsule( Interactor, UICapsuleTransform, UICapsuleStart, UICapsuleEnd, UICapsuleLocalRadius, MinDistanceToUICapsule, UIForwardVector, MinDotForAimingAtOtherHand ) )
				{
					bIsLaserAimingTowardUI = true;
				}
			}

			if( bIsLaserAimingTowardUI )
			{
				AimingAtMeFadeAlpha += DeltaTime / VREd::DockUIFadeAnimationDuration->GetFloat();
			}
			else
			{
				AimingAtMeFadeAlpha -= DeltaTime / VREd::DockUIFadeAnimationDuration->GetFloat();;
			}
			AimingAtMeFadeAlpha = FMath::Clamp( AimingAtMeFadeAlpha, 0.0f, 1.0f );
		}

		const float AnimationOvershootAmount = 1.0f;	// @todo vreditor tweak
		float EasedAimingAtMeFadeAlpha = UVREditorMode::OvershootEaseOut( AimingAtMeFadeAlpha, AnimationOvershootAmount );

		// Only show our extra buttons and controls if the user is roughly aiming toward us.  This just reduces clutter.
		SelectionBarMeshComponent->SetVisibility( EasedAimingAtMeFadeAlpha > KINDA_SMALL_NUMBER ? true : false );
		CloseButtonMeshComponent->SetVisibility( EasedAimingAtMeFadeAlpha > KINDA_SMALL_NUMBER ? true : false );

		EasedAimingAtMeFadeAlpha = FMath::Max( 0.001f, EasedAimingAtMeFadeAlpha );

		// Update the selection bar
		{
			if( bIsHoveringOverSelectionBar )
			{
				SelectionBarHoverAlpha += DeltaTime / VREd::DockUIHoverAnimationDuration->GetFloat();
			}
			else
			{
				SelectionBarHoverAlpha -= DeltaTime / VREd::DockUIHoverAnimationDuration->GetFloat();;
			}
			SelectionBarHoverAlpha = FMath::Clamp( SelectionBarHoverAlpha, 0.0f, 1.0f );

			// How big the selection bar should be
			const FVector SelectionBarSize(20.0f, Size.X * 0.8f, Size.X * 0.1f);
			FVector SelectionBarScale = SelectionBarSize * AnimatedScale * WorldScaleFactor;
			SelectionBarScale *= FMath::Lerp( 1.0f, VREd::DockUIHoverScale->GetFloat(), SelectionBarHoverAlpha );

			// Scale vertically based on our fade alpha
			SelectionBarScale.Z *= EasedAimingAtMeFadeAlpha;

			SelectionBarMeshComponent->SetRelativeScale3D( SelectionBarScale );
			const FVector SelectionBarRelativeLocation = FVector(
				4.0f,
				((Size.X * 0.5f) - (SelectionBarSize.Y * 0.5f)),
				-(Size.Y * 0.5f + SelectionBarSize.Z + VREd::DockUISelectionBarVerticalOffset->GetFloat())) * AnimatedScale * WorldScaleFactor;
			SelectionBarMeshComponent->SetRelativeLocation( SelectionBarRelativeLocation );

			SetSelectionBarColor( GetOwner().GetOwner().GetColor( bIsHoveringOverSelectionBar ? UVREditorMode::EColors::UISelectionBarHoverColor : UVREditorMode::EColors::UISelectionBarColor ) );
		}

		// Update the close button
		{
			if( bIsHoveringOverCloseButton )
			{
				CloseButtonHoverAlpha += DeltaTime / VREd::DockUIHoverAnimationDuration->GetFloat();
			}
			else
			{
				CloseButtonHoverAlpha -= DeltaTime / VREd::DockUIHoverAnimationDuration->GetFloat();;
			}
			CloseButtonHoverAlpha = FMath::Clamp( CloseButtonHoverAlpha, 0.0f, 1.0f );

			// How big the close button should be
			const FVector CloseButtonSize(20.0f, Size.X * 0.1f, Size.X * 0.1f);
			FVector CloseButtonScale = CloseButtonSize * AnimatedScale * WorldScaleFactor * EasedAimingAtMeFadeAlpha;
			CloseButtonScale *= FMath::Lerp( 1.0f, VREd::DockUIHoverScale->GetFloat(), CloseButtonHoverAlpha );

			CloseButtonMeshComponent->SetRelativeScale3D( CloseButtonScale );
			const FVector CloseButtonRelativeLocation = FVector(
				4.0f,
				-((Size.X * 0.5f) - (CloseButtonSize.Y * 0.5f)),
				-(Size.Y * 0.5f + CloseButtonSize.Z + VREd::DockUISelectionBarVerticalOffset->GetFloat())) * AnimatedScale * WorldScaleFactor;
			CloseButtonMeshComponent->SetRelativeLocation(CloseButtonRelativeLocation);

			SetCloseButtonColor( GetOwner().GetOwner().GetColor( bIsHoveringOverCloseButton ? UVREditorMode::EColors::UICloseButtonHoverColor : UVREditorMode::EColors::UICloseButtonColor ) );
		}
	}
} 

void AVREditorDockableWindow::UpdateRelativeRoomTransform()
{
	const FTransform RoomToWorld = GetOwner().GetOwner().GetRoomTransform();
	const FTransform WorldToRoom = RoomToWorld.Inverse();

	const FTransform WindowToWorldTransform = GetActorTransform();
	const FTransform WindowToRoomTransform = WindowToWorldTransform * WorldToRoom;

	const FVector RoomSpaceWindowLocation = WindowToRoomTransform.GetLocation() / GetOwner().GetOwner().GetWorldScaleFactor();;
	const FQuat RoomSpaceWindowRotation = WindowToRoomTransform.GetRotation();

	SetRelativeOffset( RoomSpaceWindowLocation );
	SetLocalRotation( RoomSpaceWindowRotation.Rotator() );
}

UStaticMeshComponent* AVREditorDockableWindow::GetCloseButtonMeshComponent() const
{
	return CloseButtonMeshComponent;
}

UStaticMeshComponent* AVREditorDockableWindow::GetSelectionBarMeshComponent() const
{
	return SelectionBarMeshComponent;
}

float AVREditorDockableWindow::GetDockSelectDistance() const
{
	return DockSelectDistance;
}

void AVREditorDockableWindow::SetDockSelectDistance(const float InDockDistance)
{
	DockSelectDistance = InDockDistance;
}

void AVREditorDockableWindow::OnPressed( UViewportInteractor* Interactor, const FHitResult& InHitResult, bool& bOutResultedInDrag )
{
	bOutResultedInDrag = false;

	UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>( Interactor );
	if( VRInteractor != nullptr )
	{
		if( InHitResult.Component == GetCloseButtonMeshComponent() )
		{
			// Close the window
			const bool bShouldShow = false;
			const bool bSpawnInFront = false;
			GetOwner().ShowEditorUIPanel(this, VRInteractor, bShouldShow, bSpawnInFront);
		}
		else if(InHitResult.Component == GetSelectionBarMeshComponent() && !GetOwner().IsDraggingPanelFromOpen())
		{
			bOutResultedInDrag = true;
			SetDockSelectDistance((InHitResult.TraceStart - InHitResult.Location ).Size());
			GetOwner().StartDraggingDockUI(this, VRInteractor, DockSelectDistance);
		}
	}
}

void AVREditorDockableWindow::OnHover( UViewportInteractor* Interactor )
{

}

void AVREditorDockableWindow::OnHoverEnter( UViewportInteractor* Interactor, const FHitResult& InHitResult )
{
	if ( SelectionBarMeshComponent == InHitResult.GetComponent() )
	{
		bIsHoveringOverSelectionBar = true;
	}

	if ( CloseButtonMeshComponent == InHitResult.GetComponent() )
	{
		bIsHoveringOverCloseButton = true;
	}
}

void AVREditorDockableWindow::OnHoverLeave( UViewportInteractor* Interactor, const UActorComponent* NewComponent )
{
	UActorComponent* OtherInteractorHoveredComponent = nullptr;
	if( Interactor->GetOtherInteractor() != nullptr )
	{
		OtherInteractorHoveredComponent = Interactor->GetOtherInteractor()->GetLastHoverComponent();
	}

	if ( OtherInteractorHoveredComponent != SelectionBarMeshComponent && NewComponent != SelectionBarMeshComponent && !GetDragOperationComponent()->IsDragging() )
	{
		bIsHoveringOverSelectionBar = false;
	}

	if ( OtherInteractorHoveredComponent != CloseButtonMeshComponent && NewComponent != CloseButtonMeshComponent )
	{
		bIsHoveringOverCloseButton = false;
	}
}

void AVREditorDockableWindow::OnDragRelease( UViewportInteractor* Interactor )
{
	UVREditorInteractor* VREditorInteractor = Cast<UVREditorInteractor>( Interactor );
	if( VREditorInteractor != nullptr )
	{
		UVREditorUISystem& UISystem = GetOwner();
		UISystem.StopDraggingDockUI( VREditorInteractor );
	}
}

UViewportDragOperationComponent* AVREditorDockableWindow::GetDragOperationComponent()
{
	return DragOperationComponent;
}

void AVREditorDockableWindow::SetSelectionBarColor( const FLinearColor& LinearColor )
{		
	static FName StaticColorParameterName( "Color" );
	SelectionBarMID->SetVectorParameterValue( StaticColorParameterName, LinearColor );
	SelectionBarTranslucentMID->SetVectorParameterValue( StaticColorParameterName, LinearColor );
}

void AVREditorDockableWindow::SetCloseButtonColor( const FLinearColor& LinearColor )
{
	static FName StaticColorParameterName( "Color" );
	CloseButtonMID->SetVectorParameterValue( StaticColorParameterName, LinearColor );
	CloseButtonTranslucentMID->SetVectorParameterValue( StaticColorParameterName, LinearColor );
}

/************************************************************************/
/* Dockable window drag operation                                       */
/******************	******************************************************/
void UDockableWindowDragOperation::ExecuteDrag( UViewportInteractor* Interactor, IViewportInteractableInterface* Interactable  )
{
	UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>( Interactor );
	AVREditorDockableWindow* DockableWindow = Cast<AVREditorDockableWindow>( Interactable );
	if ( VRInteractor && DockableWindow )
	{
		UVREditorUISystem& UISystem = DockableWindow->GetOwner();

		if (UISystem.CanScalePanel())
		{
			float NewUIScale = DockableWindow->GetScale() + VRInteractor->GetSlideDelta();
			if (NewUIScale <= UISystem.GetMinDockWindowSize())
			{
				NewUIScale = UISystem.GetMinDockWindowSize();
			}
			else if (NewUIScale >= UISystem.GetMaxDockWindowSize())
			{
				NewUIScale = UISystem.GetMaxDockWindowSize();
			}
			DockableWindow->SetScale(NewUIScale);
		}

		const FTransform UIToWorld = UISystem.MakeDockableUITransform( DockableWindow, VRInteractor, DockableWindow->GetDockSelectDistance() );
		FTransform SmoothedUIToWorld = UIToWorld;
		if( LastUIToWorld.IsSet() )
		{
			SmoothedUIToWorld.Blend( UIToWorld, LastUIToWorld.GetValue(), VREd::DockUIDragSmoothingAmount->GetFloat() );
		}

		// Update interactor hover location while dragging the interactor
		const FTransform LaserImpactToWorld = UISystem.MakeDockableUITransformOnLaser( DockableWindow, VRInteractor, DockableWindow->GetDockSelectDistance() );
		FTransform SmoothedLaserImpactToWorld = LaserImpactToWorld;
		if( LastLaserImpactToWorld.IsSet() )
		{
			SmoothedLaserImpactToWorld.Blend( LaserImpactToWorld, LastLaserImpactToWorld.GetValue(), VREd::DockUIDragSmoothingAmount->GetFloat() );
		}

		DockableWindow->SetActorTransform( SmoothedUIToWorld );
		DockableWindow->UpdateRelativeRoomTransform();

		Interactor->SetHoverLocation(SmoothedLaserImpactToWorld.GetLocation());

		LastUIToWorld = SmoothedUIToWorld;
		LastLaserImpactToWorld = SmoothedLaserImpactToWorld;
	}
}
