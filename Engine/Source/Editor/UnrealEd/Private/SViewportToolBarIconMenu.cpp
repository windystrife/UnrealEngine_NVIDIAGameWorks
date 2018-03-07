// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SViewportToolBarIconMenu.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"

void SViewportToolBarIconMenu::Construct( const FArguments& InArgs )
{
	ParentToolBar = InArgs._ParentToolBar;

	const FName ButtonStyle = FEditorStyle::Join(InArgs._Style.Get(), ".Button");

	const FSlateIcon& Icon = InArgs._Icon.Get();

	SAssignNew( MenuAnchor, SMenuAnchor )
	.Placement( MenuPlacement_BelowAnchor )
	.OnGetMenuContent( InArgs._OnGetMenuContent )
	[
		SNew(SButton)
		.ButtonStyle( FEditorStyle::Get(), ButtonStyle )
		.ContentPadding( FMargin( 5.0f, 0.0f ) )
		.OnClicked(this, &SViewportToolBarIconMenu::OnMenuClicked)
		[
			SNew(SHorizontalBox)

			// Icon
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride( 16 )
				.HeightOverride( 16 )
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(Icon.GetIcon())
				]
			]

			// Spacer
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( FMargin( 4.0f, 0.0f ) )

			// Menu dropdown
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.TextStyle( FEditorStyle::Get(), FEditorStyle::Join( InArgs._Style.Get(), ".Label" ) )
					.Text(InArgs._Label)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Bottom)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SBox )
						.WidthOverride( 4 )
						.HeightOverride( 4 )
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("ComboButton.Arrow"))
							.ColorAndOpacity(FLinearColor::Black)
						]
					]

					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
				]
			]
		]
	];

	ChildSlot
	[
		MenuAnchor.ToSharedRef()
	];
}

FReply SViewportToolBarIconMenu::OnMenuClicked()
{
	// If the menu button is clicked toggle the state of the menu anchor which will open or close the menu
	MenuAnchor->SetIsOpen( !MenuAnchor->IsOpen() );
	ParentToolBar.Pin()->SetOpenMenu( MenuAnchor );
	return FReply::Handled();
}
	
void SViewportToolBarIconMenu::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// See if there is another menu on the same tool bar already open
	TWeakPtr<SMenuAnchor> OpenedMenu = ParentToolBar.Pin()->GetOpenMenu();
	if( OpenedMenu.IsValid() && OpenedMenu.Pin()->IsOpen() && OpenedMenu.Pin() != MenuAnchor )
	{
		// There is another menu open so we open this menu and close the other
		ParentToolBar.Pin()->SetOpenMenu( MenuAnchor ); 
		MenuAnchor->SetIsOpen( true );
	}
}
