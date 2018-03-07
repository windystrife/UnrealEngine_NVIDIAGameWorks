// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SPluginCategory.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "PluginStyle.h"


#define LOCTEXT_NAMESPACE "PluginCategoryTreeItem"


void SPluginCategory::Construct(const FArguments& Args, const TSharedRef<FPluginCategory>& InCategory)
{
	Category = InCategory;

	const float CategoryIconSize = FPluginStyle::Get()->GetFloat("CategoryTreeItem.IconSize");
	const float PaddingAmount = FPluginStyle::Get()->GetFloat("CategoryTreeItem.PaddingAmount");;

	// Figure out which font size to use
	const auto bIsRootItem = !Category->ParentCategory.IsValid();

	auto PluginCountLambda = [&]() -> FText {
		return FText::Format(LOCTEXT("NumberOfPluginsWrapper", "({0})"), FText::AsNumber(Category->Plugins.Num()));
	};

	ChildSlot
	[ 
		SNew( SBorder )
		.BorderImage( FPluginStyle::Get()->GetBrush(bIsRootItem ? "CategoryTreeItem.Root.BackgroundBrush" : "CategoryTreeItem.BackgroundBrush") )
		.Padding(FPluginStyle::Get()->GetMargin(bIsRootItem ? "CategoryTreeItem.Root.BackgroundPadding" : "CategoryTreeItem.BackgroundPadding") )
		[
			SNew( SHorizontalBox )

			// Icon image
			+SHorizontalBox::Slot()
			.Padding( PaddingAmount )
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew( SBox )
				.WidthOverride( CategoryIconSize )
				.HeightOverride( CategoryIconSize )
				[
					SNew( SImage )
					.Image( this, &SPluginCategory::GetIconBrush )
				]
			]

			// Category name
			+SHorizontalBox::Slot()
			.Padding( PaddingAmount )
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Text( Category->DisplayName )
				.TextStyle( FPluginStyle::Get(), bIsRootItem ? "CategoryTreeItem.Root.Text" : "CategoryTreeItem.Text" )
			]
			
			// Plugin count
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( PaddingAmount )
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )

				// Only display if at there is least one plugin is in this category
				.Visibility( Category->Plugins.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed )

				.Text_Lambda( PluginCountLambda )
				.TextStyle( FPluginStyle::Get(), bIsRootItem ? "CategoryTreeItem.Root.PluginCountText" : "CategoryTreeItem.PluginCountText" )
			]
		]
	];
}


const FSlateBrush* SPluginCategory::GetIconBrush() const
{
	if(Category->ParentCategory.IsValid())
	{
		return FPluginStyle::Get()->GetBrush( "CategoryTreeItem.LeafItemWithPlugin" );
	}
	else if (Category->Name == TEXT("Installed"))
	{
		return FPluginStyle::Get()->GetBrush( "CategoryTreeItem.Installed");
	}
	else
	{
		return FPluginStyle::Get()->GetBrush( "CategoryTreeItem.BuiltIn" );
	}
}


#undef LOCTEXT_NAMESPACE
