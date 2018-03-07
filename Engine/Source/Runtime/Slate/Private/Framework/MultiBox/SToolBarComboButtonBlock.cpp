// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/MultiBox/SToolBarComboButtonBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"


FToolBarComboButtonBlock::FToolBarComboButtonBlock( const FUIAction& InAction, const FOnGetContent& InMenuContentGenerator, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const TAttribute<FSlateIcon>& InIcon, bool bInSimpleComboBox )
	: FMultiBlock( InAction, NAME_None, EMultiBlockType::ToolBarComboButton )
	, MenuContentGenerator( InMenuContentGenerator )
	, Label( InLabel )
	, ToolTip( InToolTip )
	, Icon( InIcon )
	, LabelVisibility()
	, bSimpleComboBox( bInSimpleComboBox )
	, bForceSmallIcons( false )
{
}

void FToolBarComboButtonBlock::CreateMenuEntry(FMenuBuilder& MenuBuilder) const
{
	FName IconName;
	FText EntryLabel = Label.Get();
	if ( EntryLabel.IsEmpty() )
	{
		EntryLabel = NSLOCTEXT("ToolBar", "CustomControlLabel", "Custom Control");
	}

	MenuBuilder.AddWrapperSubMenu(EntryLabel, FText::GetEmpty(), MenuContentGenerator, Icon.Get());
}

bool FToolBarComboButtonBlock::HasIcon() const
{
	const FSlateIcon& ActualIcon = Icon.Get();
	return ActualIcon.GetIcon()->GetResourceName() != NAME_None;
}

TSharedRef< class IMultiBlockBaseWidget > FToolBarComboButtonBlock::ConstructWidget() const
{
	return SNew( SToolBarComboButtonBlock )
		.LabelVisibility( LabelVisibility.IsSet() ? LabelVisibility.GetValue() : TOptional< EVisibility >() )
		.Icon(Icon)
		.ForceSmallIcons( bForceSmallIcons )
		.Cursor( EMouseCursor::Default );
}

void SToolBarComboButtonBlock::Construct( const FArguments& InArgs )
{
	if ( InArgs._LabelVisibility.IsSet() )
	{
		LabelVisibility = InArgs._LabelVisibility.GetValue();
	}
	else
	{
		LabelVisibility = TAttribute< EVisibility >::Create( TAttribute< EVisibility >::FGetter::CreateSP( SharedThis( this ), &SToolBarComboButtonBlock::GetIconVisibility, false ) );
	}

	Icon = InArgs._Icon;
	bForceSmallIcons = InArgs._ForceSmallIcons;
}

void SToolBarComboButtonBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	TSharedRef< const FMultiBox > MultiBox( OwnerMultiBoxWidget.Pin()->GetMultiBox() );
	
	TSharedRef< const FToolBarComboButtonBlock > ToolBarComboButtonBlock = StaticCastSharedRef< const FToolBarComboButtonBlock >( MultiBlock.ToSharedRef() );

	TAttribute<FText> Label;

	// If we were supplied an image than go ahead and use that, otherwise we use a null widget
	TSharedRef< SWidget > IconWidget = SNullWidget::NullWidget;
	TSharedRef< SWidget > SmallIconWidget = SNullWidget::NullWidget;
	if (!ToolBarComboButtonBlock->bSimpleComboBox)
	{
		if( !HasDynamicIcon() )
		{
			// Not dynamic, so resolve the icons now
			const FSlateBrush* IconBrush = GetIcon();
			const FSlateBrush* SmallIconBrush = GetSmallIcon();

			if( IconBrush->GetResourceName() != NAME_None )
			{
				IconWidget =
					SNew( SImage )
					.Visibility( this, &SToolBarComboButtonBlock::GetIconVisibility, false )
					.Image( IconBrush );
			}
			if( SmallIconBrush->GetResourceName() != NAME_None )
			{
				SmallIconWidget =
					SNew( SImage )
					.Visibility( this, &SToolBarComboButtonBlock::GetIconVisibility, true )
					.Image( SmallIconBrush );
			}
		}
		else 
		{
			// Dynamic, so we need to preserve the callback used
			IconWidget =
				SNew( SImage )
				.Visibility( this, &SToolBarComboButtonBlock::GetIconVisibility, false )
				.Image( this, &SToolBarComboButtonBlock::GetIcon );

			SmallIconWidget =
				SNew( SImage )
				.Visibility( this, &SToolBarComboButtonBlock::GetIconVisibility, true )
				.Image( this, &SToolBarComboButtonBlock::GetSmallIcon );
		}

		Label = ToolBarComboButtonBlock->Label;
	}

	// Add this widget to the search list of the multibox
	if (MultiBlock->GetSearchable())
		OwnerMultiBoxWidget.Pin()->AddSearchElement(this->AsWidget(), Label.Get());

	// Setup the string for the metatag
	FName TagName;
	if (ToolBarComboButtonBlock->GetTutorialHighlightName() == NAME_None)
	{
		TagName = *FString::Printf(TEXT("ToolbarComboButton,%s,0"), *Label.Get().ToString());
	}
	else
	{
		TagName = ToolBarComboButtonBlock->GetTutorialHighlightName();
	}
	
	// Create the content for our button
	TSharedRef< SWidget > ButtonContent =
		SNew( SVerticalBox )
		.AddMetaData<FTagMetaData>(FTagMetaData(TagName))
		// Icon image
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		// Center the icon horizontally, so that large labels don't stretch out the artwork
		[
			IconWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SmallIconWidget
		]

		// Label text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign( HAlign_Center )	// Center the label text horizontally
		[
			SNew( STextBlock )
				.Visibility( LabelVisibility )
				.Text( Label )

				// Smaller font for tool tip labels
				.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Label" ) )
				.ShadowOffset(FVector2D::UnitVector)
		]
		;
		
	EMultiBlockLocation::Type BlockLocation = GetMultiBlockLocation();
	FName BlockStyle = EMultiBlockLocation::ToName(ISlateStyle::Join( StyleName, ".Button" ), BlockLocation);
	FName ColorStyle = ISlateStyle::Join( StyleName, ".SToolBarComboButtonBlock.ComboButton.Color" );

	ChildSlot
	[
		SNew( SComboButton )
			.ContentPadding(0)

			// Use the tool bar item style for this button
			.ButtonStyle( StyleSet, BlockStyle )

			// Pass along the block's tool-tip string
			.ToolTipText( ToolBarComboButtonBlock->ToolTip )

			.ForegroundColor( StyleSet->GetSlateColor( ColorStyle ) )

			.ButtonContent()
			[
				ButtonContent
			]

			// Route the content generator event
			.OnGetMenuContent( this, &SToolBarComboButtonBlock::OnGetMenuContent )
	];

	ChildSlot.Padding(StyleSet->GetMargin(ISlateStyle::Join( StyleName, ".SToolBarComboButtonBlock.Padding" )));
	// Bind our widget's enabled state to whether or not our action can execute
	SetEnabled( TAttribute< bool >( this, &SToolBarComboButtonBlock::IsEnabled ) );

	// Bind our widget's visible state to whether or not the button should be visible
	SetVisibility( TAttribute<EVisibility>(this, &SToolBarComboButtonBlock::GetVisibility) );
}

TSharedRef<SWidget> SToolBarComboButtonBlock::OnGetMenuContent()
{
	TSharedRef< const FToolBarComboButtonBlock > ToolBarButtonComboBlock = StaticCastSharedRef< const FToolBarComboButtonBlock >( MultiBlock.ToSharedRef() );
	return ToolBarButtonComboBlock->MenuContentGenerator.Execute();
}

bool SToolBarComboButtonBlock::IsEnabled() const
{
	const FUIAction& UIAction = MultiBlock->GetDirectActions();
	if( UIAction.CanExecuteAction.IsBound() )
	{
		return UIAction.CanExecuteAction.Execute();
	}

	return true;
}

EVisibility SToolBarComboButtonBlock::GetVisibility() const
{
	const FUIAction& UIAction = MultiBlock->GetDirectActions();
	if (UIAction.IsActionVisibleDelegate.IsBound())
	{
		return UIAction.IsActionVisibleDelegate.Execute() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

bool SToolBarComboButtonBlock::HasDynamicIcon() const
{
	return Icon.IsBound();
}

const FSlateBrush* SToolBarComboButtonBlock::GetIcon() const
{
	const FSlateIcon& ActualIcon = Icon.Get();
	return ActualIcon.GetIcon();
}

const FSlateBrush* SToolBarComboButtonBlock::GetSmallIcon() const
{
	const FSlateIcon& ActualIcon = Icon.Get();
	return ActualIcon.GetSmallIcon();
}

EVisibility SToolBarComboButtonBlock::GetIconVisibility(bool bIsASmallIcon) const
{
	return ((bForceSmallIcons || FMultiBoxSettings::UseSmallToolBarIcons.Get()) ^ bIsASmallIcon) ? EVisibility::Collapsed : EVisibility::Visible;
}
