// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorFloatingText.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/WorldSettings.h"
#include "Components/TextRenderComponent.h"


AFloatingText::AFloatingText()
{
	if (UNLIKELY(IsRunningDedicatedServer()))
	{
		return;
	}

	// Create root default scene component
	{
		SceneComponent = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneComponent" ) );
		check( SceneComponent != nullptr );

		RootComponent = SceneComponent;
	}

	UStaticMesh* LineSegmentCylinderMesh = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ObjectFinder( TEXT( "/Engine/VREditor/FloatingText/LineSegmentCylinder" ) );
		LineSegmentCylinderMesh = ObjectFinder.Object;
		check( LineSegmentCylinderMesh != nullptr );
	}

	UStaticMesh* JointSphereMesh = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ObjectFinder( TEXT( "/Engine/VREditor/FloatingText/JointSphere" ) );
		JointSphereMesh = ObjectFinder.Object;
		check( JointSphereMesh != nullptr );
	}

	{
		static ConstructorHelpers::FObjectFinder<UMaterial> ObjectFinder( TEXT( "/Engine/VREditor/FloatingText/LineMaterial" ) );
		LineMaterial = ObjectFinder.Object;
		check( LineMaterial != nullptr );
	}



	// @todo vreditor: Tweak
	const bool bAllowTextLighting = false;
	const float TextSize = 1.5f;

	{
		FirstLineComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "FirstLine" ) );
		check( FirstLineComponent != nullptr );

		FirstLineComponent->SetStaticMesh( LineSegmentCylinderMesh );
		FirstLineComponent->SetMobility( EComponentMobility::Movable );
		FirstLineComponent->SetupAttachment( SceneComponent );
		
		FirstLineComponent->SetCollisionEnabled( ECollisionEnabled::NoCollision );

		FirstLineComponent->bGenerateOverlapEvents = false;
		FirstLineComponent->SetCanEverAffectNavigation( false );
		FirstLineComponent->bCastDynamicShadow = bAllowTextLighting;
		FirstLineComponent->bCastStaticShadow = false;
		FirstLineComponent->bAffectDistanceFieldLighting = bAllowTextLighting;
		FirstLineComponent->bAffectDynamicIndirectLighting = bAllowTextLighting;
	}

	{
		JointSphereComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "JointSphere" ) );
		check( JointSphereComponent != nullptr );

		JointSphereComponent->SetStaticMesh( JointSphereMesh );
		JointSphereComponent->SetMobility( EComponentMobility::Movable );
		JointSphereComponent->SetupAttachment( SceneComponent );

		JointSphereComponent->SetCollisionEnabled( ECollisionEnabled::NoCollision );

		JointSphereComponent->bGenerateOverlapEvents = false;
		JointSphereComponent->SetCanEverAffectNavigation( false );
		JointSphereComponent->bCastDynamicShadow = bAllowTextLighting;
		JointSphereComponent->bCastStaticShadow = false;
		JointSphereComponent->bAffectDistanceFieldLighting = bAllowTextLighting;
		JointSphereComponent->bAffectDynamicIndirectLighting = bAllowTextLighting;
		
	}

	{
		SecondLineComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "SecondLine" ) );
		check( SecondLineComponent != nullptr );

		SecondLineComponent->SetStaticMesh( LineSegmentCylinderMesh );
		SecondLineComponent->SetMobility( EComponentMobility::Movable );
		SecondLineComponent->SetupAttachment( SceneComponent );

		SecondLineComponent->SetCollisionEnabled( ECollisionEnabled::NoCollision );

		SecondLineComponent->bGenerateOverlapEvents = false;
		SecondLineComponent->SetCanEverAffectNavigation( false );
		SecondLineComponent->bCastDynamicShadow = bAllowTextLighting;
		SecondLineComponent->bCastStaticShadow = false;
		SecondLineComponent->bAffectDistanceFieldLighting = bAllowTextLighting;
		SecondLineComponent->bAffectDynamicIndirectLighting = bAllowTextLighting;

	}

	{
		static ConstructorHelpers::FObjectFinder<UMaterial> ObjectFinder( TEXT( "/Engine/VREditor/Fonts/VRTextMaterial" ) );
		MaskedTextMaterial = ObjectFinder.Object;
		check( MaskedTextMaterial != nullptr );
	}

	{
		static ConstructorHelpers::FObjectFinder<UMaterialInstance> ObjectFinder( TEXT( "/Engine/VREditor/Fonts/TranslucentVRTextMaterial" ) );
		TranslucentTextMaterial = ObjectFinder.Object;
		check( TranslucentTextMaterial != nullptr );
	}

	UFont* TextFont = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UFont> ObjectFinder( TEXT( "/Engine/VREditor/Fonts/VRText_RobotoLarge" ) );
		TextFont = ObjectFinder.Object;
		check( TextFont != nullptr );
	}

	{
		TextComponent = CreateDefaultSubobject<UTextRenderComponent>( TEXT( "Text" ) );
		check( TextComponent != nullptr );

		TextComponent->SetMobility( EComponentMobility::Movable );
		TextComponent->SetupAttachment( SceneComponent );

		TextComponent->SetCollisionProfileName( UCollisionProfile::NoCollision_ProfileName );

		TextComponent->bGenerateOverlapEvents = false;
		TextComponent->SetCanEverAffectNavigation( false );
		TextComponent->bCastDynamicShadow = bAllowTextLighting;
		TextComponent->bCastStaticShadow = false;
		TextComponent->bAffectDistanceFieldLighting = bAllowTextLighting;
		TextComponent->bAffectDynamicIndirectLighting = bAllowTextLighting;


		TextComponent->SetWorldSize( TextSize );

		// Use a custom font.  The text will be visible up close.	   
		TextComponent->SetFont( TextFont );

		// Assign our custom text rendering material.
		TextComponent->SetTextMaterial( MaskedTextMaterial );

		TextComponent->SetTextRenderColor( FLinearColor::White.ToFColor( false ) );

		// Left justify the text
		TextComponent->SetHorizontalAlignment( EHTA_Left );

	}
}


void AFloatingText::PostActorCreated()
{
	Super::PostActorCreated();

	// Create an MID so that we can change parameters on the fly (fading)
	check( LineMaterial != nullptr );
	this->LineMaterialMID = UMaterialInstanceDynamic::Create( LineMaterial, this );

	FirstLineComponent->SetMaterial( 0, LineMaterialMID );
	JointSphereComponent->SetMaterial( 0, LineMaterialMID );
	SecondLineComponent->SetMaterial( 0, LineMaterialMID );
}


void AFloatingText::SetText( const FText& NewText )
{
	check( TextComponent != nullptr );
	TextComponent->SetText( NewText );
}


void AFloatingText::SetOpacity( const float NewOpacity )
{
	const FLinearColor NewColor = FLinearColor( 0.6f, 0.6f, 0.6f ).CopyWithNewOpacity( NewOpacity );	// @todo vreditor: Tweak brightness
	const FColor NewFColor = NewColor.ToFColor( false );

	check( TextComponent != nullptr );
// 	if( NewOpacity >= 1.0f - KINDA_SMALL_NUMBER )	// @todo vreditor ui: get fading/translucency working again!
// 	{
		if( TextComponent->GetMaterial( 0 ) != MaskedTextMaterial )
		{
			TextComponent->SetTextMaterial( MaskedTextMaterial );
		}
// 	}
// 	else
// 	{
// 		if( TextComponent->GetMaterial( 0 ) != TranslucentTextMaterial )
// 		{
// 			TextComponent->SetTextMaterial( TranslucentTextMaterial );
// 		}
// 	}
	
	if( NewFColor != TextComponent->TextRenderColor )
	{
		TextComponent->SetTextRenderColor( NewFColor );
	}

	check( LineMaterialMID != nullptr );
	static FName ColorAndOpacityParameterName( "ColorAndOpacity" );
	LineMaterialMID->SetVectorParameterValue( ColorAndOpacityParameterName, NewColor );
}


void AFloatingText::Update( const FVector OrientateToward )
{
	// Orientate it toward the viewer
	const FVector DirectionToward = ( OrientateToward - GetActorLocation() ).GetSafeNormal();

	const FQuat TowardRotation = DirectionToward.ToOrientationQuat();

	// @todo vreditor tweak
	const float LineRadius = 0.1f;
	const float FirstLineLength = 4.0f;	   // Default line length (note that socket scale can affect this!)
	const float SecondLineLength = TextComponent->GetTextLocalSize().Y;	// The second line "underlines" the text


	// NOTE: The origin of the actor will be the designated target of the text
	const FVector FirstLineLocation = FVector::ZeroVector;
	const FQuat FirstLineRotation = FVector::ForwardVector.ToOrientationQuat();
	const FVector FirstLineScale = FVector( FirstLineLength, LineRadius, LineRadius );
	FirstLineComponent->SetRelativeLocation( FirstLineLocation );
	FirstLineComponent->SetRelativeRotation( FirstLineRotation );
	FirstLineComponent->SetRelativeScale3D( FirstLineScale );

	// NOTE: The joint sphere draws at the connection point between the lines
	const FVector JointLocation = FirstLineLocation + FirstLineRotation * FVector::ForwardVector * FirstLineLength;
	const FVector JointScale = FVector( LineRadius );
	JointSphereComponent->SetRelativeLocation( JointLocation );
	JointSphereComponent->SetRelativeScale3D( JointScale );

	// NOTE: The second line starts at the joint location
	SecondLineComponent->SetWorldLocation( JointSphereComponent->GetComponentLocation() );
	SecondLineComponent->SetWorldRotation( ( TowardRotation * -FVector::RightVector ).ToOrientationQuat() );
	SecondLineComponent->SetRelativeScale3D( FVector( ( SecondLineLength / GetActorScale().X ) * GetWorld()->GetWorldSettings()->WorldToMeters / 100.0f, LineRadius, LineRadius ) );

	TextComponent->SetWorldLocation( JointSphereComponent->GetComponentLocation() );
	TextComponent->SetWorldRotation( ( TowardRotation * FVector::ForwardVector ).ToOrientationQuat() );

}
