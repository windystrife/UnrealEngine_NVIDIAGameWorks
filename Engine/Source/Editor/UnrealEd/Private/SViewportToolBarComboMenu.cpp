// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SViewportToolBarComboMenu.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"

void SViewportToolBarComboMenu::Construct( const FArguments& InArgs )
{
	FName ButtonStyle = FEditorStyle::Join(InArgs._Style.Get(), ".Button");
	FName CheckboxStyle = FEditorStyle::Join(InArgs._Style.Get(), ".ToggleButton");

	const FSlateIcon& Icon = InArgs._Icon.Get();

	ParentToolBar = InArgs._ParentToolBar;

	TSharedPtr<SCheckBox> ToggleControl;
	{
		ToggleControl = SNew(SCheckBox)
		.Cursor( EMouseCursor::Default )
		.Padding(FMargin( 4.0f ))
		.Style(FEditorStyle::Get(), EMultiBlockLocation::ToName(CheckboxStyle, InArgs._BlockLocation))
		.OnCheckStateChanged(InArgs._OnCheckStateChanged)
		.ToolTipText(InArgs._ToggleButtonToolTip)
		.IsChecked(InArgs._IsChecked)
		.Content()
		[
			SNew( SBox )
			.WidthOverride( 16 )
			.HeightOverride( 16 )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(Icon.GetIcon())
			]
		];
	}

	{
		TSharedRef<SWidget> ButtonContents =
			SNew(SButton)
			.ButtonStyle( FEditorStyle::Get(), EMultiBlockLocation::ToName(ButtonStyle,EMultiBlockLocation::End) )
			.ContentPadding( FMargin( 5.0f, 0.0f ) )
			.ToolTipText(InArgs._MenuButtonToolTip)
			.OnClicked(this, &SViewportToolBarComboMenu::OnMenuClicked)
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
			];
		
		if (InArgs._MinDesiredButtonWidth > 0.0f)
		{
			ButtonContents =
				SNew(SBox)
				.MinDesiredWidth(InArgs._MinDesiredButtonWidth)
				[
					ButtonContents
				];
		}

		MenuAnchor = SNew(SMenuAnchor)
		.Placement( MenuPlacement_BelowAnchor )
		[
			ButtonContents
		]
		.OnGetMenuContent( InArgs._OnGetMenuContent );
	}


	ChildSlot
	[
		SNew(SHorizontalBox)

		//Checkbox concept
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			ToggleControl->AsShared()
		]

		// Black Separator line
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(FMargin(1.0f, 0.0f, 0.0f, 0.0f))
			.BorderImage(FEditorStyle::GetDefaultBrush())
			.BorderBackgroundColor(FLinearColor::Black)
		]

		// Menu dropdown concept
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			MenuAnchor->AsShared()
		]
	];
}

FReply SViewportToolBarComboMenu::OnMenuClicked()
{
	// If the menu button is clicked toggle the state of the menu anchor which will open or close the menu
	MenuAnchor->SetIsOpen( !MenuAnchor->IsOpen() );
	ParentToolBar.Pin()->SetOpenMenu( MenuAnchor );
	return FReply::Handled();
}

void SViewportToolBarComboMenu::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
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
