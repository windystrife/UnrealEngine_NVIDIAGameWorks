// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/LevelVisibilitySection.h"
#include "Sections/MovieSceneLevelVisibilitySection.h"
#include "Misc/PackageName.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "SequencerSectionPainter.h"
#include "SDropTarget.h"
#include "DragAndDrop/LevelDragDropOp.h"
#include "ScopedTransaction.h"

FLevelVisibilitySection::FLevelVisibilitySection( UMovieSceneLevelVisibilitySection& InSectionObject )
	: SectionObject( InSectionObject )
{
	VisibleText = NSLOCTEXT( "LevelVisibilitySection", "VisibleHeader", "Visible" );
	HiddenText = NSLOCTEXT( "LevelVisibilitySection", "HiddenHeader", "Hidden" );
}

UMovieSceneSection* FLevelVisibilitySection::GetSectionObject()
{
	return &SectionObject;
}

TSharedRef<SWidget> FLevelVisibilitySection::GenerateSectionWidget()
{
	return
		SNew( SDropTarget )
		.OnAllowDrop( this, &FLevelVisibilitySection::OnAllowDrop )
		.OnDrop( this, &FLevelVisibilitySection::OnDrop )
		.Content()
		[
			SNew( SBorder )
			.BorderBackgroundColor( this, &FLevelVisibilitySection::GetBackgroundColor )
			.BorderImage( FCoreStyle::Get().GetBrush( "WhiteBrush" ) )
			[
				SNew( STextBlock )
				.Text( this, &FLevelVisibilitySection::GetVisibilityText )
				.ToolTipText( this, &FLevelVisibilitySection::GetVisibilityToolTip )
			]
		];
}


int32 FLevelVisibilitySection::OnPaintSection( FSequencerSectionPainter& InPainter ) const
{
	return InPainter.PaintSectionBackground();
}


FSlateColor FLevelVisibilitySection::GetBackgroundColor() const
{
	return SectionObject.GetVisibility() == ELevelVisibility::Visible
		? FSlateColor( FLinearColor::Green.Desaturate( .5f ) )
		: FSlateColor( FLinearColor::Red.Desaturate( .5f ) );
}


FText FLevelVisibilitySection::GetVisibilityText() const
{
	FText VisibilityText = SectionObject.GetVisibility() == ELevelVisibility::Visible ? VisibleText : HiddenText;
	return FText::Format( NSLOCTEXT( "LevelVisibilitySection", "SectionTextFormat", "{0} ({1})" ), VisibilityText, FText::AsNumber( SectionObject.GetLevelNames()->Num() ) );
}

FText FLevelVisibilitySection::GetVisibilityToolTip() const
{
	TArray<FString> LevelNameStrings;
	for ( const FName& LevelName : *SectionObject.GetLevelNames() )
	{
		LevelNameStrings.Add( LevelName.ToString() );
	}

	FText VisibilityText = SectionObject.GetVisibility() == ELevelVisibility::Visible ? VisibleText : HiddenText;
	return FText::Format( NSLOCTEXT( "LevelVisibilitySection", "ToolTipFormat", "{0}\r\n{1}" ), VisibilityText, FText::FromString( FString::Join( LevelNameStrings, TEXT( "\r\n" ) ) ) );
}


bool FLevelVisibilitySection::OnAllowDrop( TSharedPtr<FDragDropOperation> DragDropOperation )
{
	return DragDropOperation->IsOfType<FLevelDragDropOp>() && StaticCastSharedPtr<FLevelDragDropOp>( DragDropOperation )->StreamingLevelsToDrop.Num() > 0;
}


FReply FLevelVisibilitySection::OnDrop( TSharedPtr<FDragDropOperation> DragDropOperation )
{
	if ( DragDropOperation->IsOfType<FLevelDragDropOp>() )
	{
		TSharedPtr<FLevelDragDropOp> LevelDragDropOperation = StaticCastSharedPtr<FLevelDragDropOp>( DragDropOperation );
		if ( LevelDragDropOperation->StreamingLevelsToDrop.Num() > 0 )
		{
			FScopedTransaction Transaction(NSLOCTEXT("LevelVisibilitySection", "TransactionText", "Add Level(s) to Level Visibility Section"));
			SectionObject.Modify();

			for ( TWeakObjectPtr<ULevelStreaming> Level : LevelDragDropOperation->StreamingLevelsToDrop )
			{
				if ( Level.IsValid() )
				{
					FName ShortLevelName = FPackageName::GetShortFName( Level->GetWorldAssetPackageFName() );
					SectionObject.GetLevelNames()->AddUnique( ShortLevelName );
				}
			}
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

