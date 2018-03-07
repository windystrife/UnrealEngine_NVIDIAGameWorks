// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/MultiBox/SToolBarButtonBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SCheckBox.h"


FToolBarButtonBlock::FToolBarButtonBlock( const TSharedPtr< const FUICommandInfo > InCommand, TSharedPtr< const FUICommandList > InCommandList, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InToolTipOverride, const TAttribute<FSlateIcon>& InIconOverride )
	: FMultiBlock( InCommand, InCommandList, NAME_None, EMultiBlockType::ToolBarButton )
	, LabelOverride( InLabelOverride )
	, ToolTipOverride( InToolTipOverride )
	, IconOverride( InIconOverride )
	, LabelVisibility()
	, UserInterfaceActionType(EUserInterfaceActionType::Button)
	, bIsFocusable(false)
	, bForceSmallIcons(false)
{
}

FToolBarButtonBlock::FToolBarButtonBlock( const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const TAttribute<FSlateIcon>& InIcon, const FUIAction& InUIAction, const EUserInterfaceActionType::Type InUserInterfaceActionType )
	: FMultiBlock( InUIAction )
	, LabelOverride( InLabel )
	, ToolTipOverride( InToolTip )
	, IconOverride( InIcon )
	, LabelVisibility()
	, UserInterfaceActionType(InUserInterfaceActionType)
	, bIsFocusable(false)
	, bForceSmallIcons(false)
{
}

void FToolBarButtonBlock::CreateMenuEntry(FMenuBuilder& MenuBuilder) const
{
	TSharedPtr<const FUICommandInfo> MenuEntryAction = GetAction();
	TSharedPtr<const FUICommandList> MenuEntryActionList = GetActionList();
	if (MenuEntryAction.IsValid() && MenuEntryActionList.IsValid())
	{
		MenuBuilder.PushCommandList(MenuEntryActionList.ToSharedRef());
		MenuBuilder.AddMenuEntry(MenuEntryAction);
		MenuBuilder.PopCommandList();
	}
	else if ( LabelOverride.IsSet() )
	{
		const FUIAction& DirectAction = GetDirectActions();
		MenuBuilder.AddMenuEntry( LabelOverride.Get(), ToolTipOverride.Get(), IconOverride.Get(), DirectAction );
	}
}

bool FToolBarButtonBlock::HasIcon() const
{
	const FSlateIcon ActionIcon = GetAction().IsValid() ? GetAction()->GetIcon() : FSlateIcon();
	const FSlateIcon& ActualIcon = IconOverride.IsSet() ? IconOverride.Get() : ActionIcon;

	if (ActualIcon.IsSet())
	{
		return ActualIcon.GetIcon()->GetResourceName() != NAME_None;
	}

	return false;
}

/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FToolBarButtonBlock::ConstructWidget() const
{
	return SNew( SToolBarButtonBlock )
		.LabelVisibility( LabelVisibility.IsSet() ? LabelVisibility.GetValue() : TOptional< EVisibility >() )
		.IsFocusable( bIsFocusable )
		.ForceSmallIcons( bForceSmallIcons )
		.TutorialHighlightName(GetTutorialHighlightName())
		.Cursor( EMouseCursor::Default );
}


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SToolBarButtonBlock::Construct( const FArguments& InArgs )
{
	if ( InArgs._LabelVisibility.IsSet() )
	{
		LabelVisibility = InArgs._LabelVisibility.GetValue();
	}
	else
	{
		LabelVisibility = TAttribute< EVisibility >::Create( TAttribute< EVisibility >::FGetter::CreateSP( SharedThis( this ), &SToolBarButtonBlock::GetIconVisibility, false ) );
	}

	bIsFocusable = InArgs._IsFocusable;
	bForceSmallIcons = InArgs._ForceSmallIcons;
	TutorialHighlightName = InArgs._TutorialHighlightName;
}


/**
 * Builds this MultiBlock widget up from the MultiBlock associated with it
 */
void SToolBarButtonBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	struct Local
	{
		/** Appends the key binding to the end of the provided ToolTip */
		static FText AppendKeyBindingToToolTip( const TAttribute<FText> ToolTip, TWeakPtr< const FUICommandInfo> Command )
		{
			TSharedPtr<const FUICommandInfo> CommandPtr = Command.Pin();
			if( CommandPtr.IsValid() && (CommandPtr->GetFirstValidChord()->IsValidChord()) )
			{
				FFormatNamedArguments Args;
				Args.Add( TEXT("ToolTipDescription"), ToolTip.Get() );
				Args.Add( TEXT("Keybinding"), CommandPtr->GetInputText() );
				return FText::Format( NSLOCTEXT("ToolBar", "ToolTip + Keybinding", "{ToolTipDescription} ({Keybinding})"), Args );
			}
			else
			{
				return ToolTip.Get();
			}
		}
	};


	TSharedRef< const FMultiBox > MultiBox( OwnerMultiBoxWidget.Pin()->GetMultiBox() );
	
	TSharedRef< const FToolBarButtonBlock > ToolBarButtonBlock = StaticCastSharedRef< const FToolBarButtonBlock >( MultiBlock.ToSharedRef() );

	// Allow the block to override the action's label and tool tip string, if desired
	TAttribute<FText> ActualLabel;
	if (ToolBarButtonBlock->LabelOverride.IsSet())
	{
		ActualLabel = ToolBarButtonBlock->LabelOverride;
	}
	else
	{
		ActualLabel = ToolBarButtonBlock->GetAction()->GetLabel();
	}

	// Add this widget to the search list of the multibox
	if (MultiBlock->GetSearchable())
		OwnerMultiBoxWidget.Pin()->AddSearchElement(this->AsWidget(), ActualLabel.Get());

	TAttribute<FText> ActualToolTip;
	if (ToolBarButtonBlock->ToolTipOverride.IsSet())
	{
		ActualToolTip = ToolBarButtonBlock->ToolTipOverride;
	}
	else
	{
		ActualToolTip = ToolBarButtonBlock->GetAction()->GetDescription();
	}

	// If a key is bound to the command, append it to the tooltip text.
	TWeakPtr<const FUICommandInfo> Action = ToolBarButtonBlock->GetAction();
	ActualToolTip = TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateStatic( &Local::AppendKeyBindingToToolTip, ActualToolTip, Action ) );
	
	// If we were supplied an image than go ahead and use that, otherwise we use a null widget
	TSharedRef< SWidget > IconWidget =
		SNew( SImage )
		.Visibility( this, &SToolBarButtonBlock::GetIconVisibility, false )	
		.Image( this, &SToolBarButtonBlock::GetIconBrush );
	TSharedRef< SWidget > SmallIconWidget =
		SNew( SImage )
		.Visibility( this, &SToolBarButtonBlock::GetIconVisibility, true )	
		.Image( this, &SToolBarButtonBlock::GetSmallIconBrush );

	// Create the content for our button
	TSharedRef< SWidget > ButtonContent =

		SNew(SHorizontalBox)
		.AddMetaData<FTagMetaData>(FTagMetaData(TutorialHighlightName))

		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Center)
		[
			SNew( SVerticalBox )

			// Icon image
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign( HAlign_Center )	// Center the icon horizontally, so that large labels don't stretch out the artwork
			[
				IconWidget
			]
			+ SVerticalBox::Slot().AutoHeight()
			.HAlign( HAlign_Center )
			[
				SmallIconWidget
			]

			// Label text
			+ SVerticalBox::Slot().AutoHeight()
			.HAlign( HAlign_Center )	// Center the label text horizontally
			[
				SNew( STextBlock )
					.Visibility( LabelVisibility )
					.Text( ActualLabel )
					.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Label" ) )	// Smaller font for tool tip labels
					.ShadowOffset( FVector2D::UnitVector )
			]
		];

	EMultiBlockLocation::Type BlockLocation = GetMultiBlockLocation();
	
	// What type of UI should we create for this block?
	EUserInterfaceActionType::Type UserInterfaceType = ToolBarButtonBlock->UserInterfaceActionType;
	if ( Action.IsValid() )
	{
		// If we have a UICommand, then this is specified in the command.
		UserInterfaceType = Action.Pin()->GetUserInterfaceType();
	}
	
	if( UserInterfaceType == EUserInterfaceActionType::Button )
	{
		FName BlockStyle = EMultiBlockLocation::ToName(ISlateStyle::Join( StyleName, ".Button" ), BlockLocation);

		ChildSlot
		[
			// Create a button
			SNew( SButton )
				.ContentPadding(0 )
				// Use the tool bar item style for this button
				.ButtonStyle( StyleSet, BlockStyle )
				.ForegroundColor( FSlateColor::UseForeground() )
				.IsFocusable(bIsFocusable)
				[
					ButtonContent
				]
				// Bind the button's "on clicked" event to our object's method for this
				.OnClicked( this, &SToolBarButtonBlock::OnClicked )
				// Pass along the block's tool-tip string
				.ToolTip( FMultiBoxSettings::ToolTipConstructor.Execute( ActualToolTip, nullptr, Action.Pin() ) )
		];
	}
	else if( ensure( UserInterfaceType == EUserInterfaceActionType::ToggleButton || UserInterfaceType == EUserInterfaceActionType::RadioButton ) )
	{
		FName BlockStyle = EMultiBlockLocation::ToName(ISlateStyle::Join( StyleName, ".ToggleButton" ), BlockLocation);
		FName CheckboxStyle = ISlateStyle::Join( StyleName, ".SToolBarButtonBlock.CheckBox.Padding" );

		ChildSlot
		[
			// Create a check box
			SNew( SCheckBox )

				// Use the tool bar style for this check box
				.Style( StyleSet, BlockStyle )

				// User will have set the focusable attribute for the block, honor it
				.IsFocusable( bIsFocusable )

				// Pass along the block's tool-tip string
				.ToolTip( FMultiBoxSettings::ToolTipConstructor.Execute( ActualToolTip, nullptr, Action.Pin() ) )
				[
					ButtonContent
				]

				// Bind the button's "on checked" event to our object's method for this
				.OnCheckStateChanged( this, &SToolBarButtonBlock::OnCheckStateChanged )

				// Bind the check box's "checked" state to our user interface action
				.IsChecked( this, &SToolBarButtonBlock::OnIsChecked )

				.Padding( StyleSet->GetMargin(CheckboxStyle) )
		];
	}
	
	ChildSlot.Padding(StyleSet->GetMargin(ISlateStyle::Join( StyleName, ".SToolBarButtonBlock.Padding" )));

	// Bind our widget's enabled state to whether or not our action can execute
	SetEnabled( TAttribute< bool >( this, &SToolBarButtonBlock::IsEnabled ) );

	// Bind our widget's visible state to whether or not the button should be visible
	SetVisibility( TAttribute<EVisibility>(this, &SToolBarButtonBlock::GetBlockVisibility) );
}



/**
 * Called by Slate when this tool bar button's button is clicked
 */
FReply SToolBarButtonBlock::OnClicked()
{
	// Button was clicked, so trigger the action!
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	TSharedPtr< const FUICommandInfo > Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();
	
	if( ActionList.IsValid() && Action.IsValid() )
	{
		ActionList->ExecuteAction( Action.ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		MultiBlock->GetDirectActions().Execute();
	}

	TSharedRef< const FMultiBox > MultiBox( OwnerMultiBoxWidget.Pin()->GetMultiBox() );

	// If this is a context menu, then we'll also dismiss the window after the user clicked on the item
	const bool ClosingMenu = MultiBox->ShouldCloseWindowAfterMenuSelection();
	if( ClosingMenu )
	{
		FSlateApplication::Get().DismissMenuByWidget(AsShared());
	}

	return FReply::Handled();
}



/**
 * Called by Slate when this tool bar check box button is toggled
 */
void SToolBarButtonBlock::OnCheckStateChanged( const ECheckBoxState NewCheckedState )
{
	OnClicked();
}

/**
 * Called by slate to determine if this button should appear checked
 *
 * @return ECheckBoxState::Checked if it should be checked, ECheckBoxState::Unchecked if not.
 */
ECheckBoxState SToolBarButtonBlock::OnIsChecked() const
{
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	TSharedPtr< const FUICommandInfo > Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();

	ECheckBoxState CheckState = ECheckBoxState::Unchecked;
	if( ActionList.IsValid() && Action.IsValid() )
	{
		CheckState = ActionList->GetCheckState( Action.ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		CheckState = DirectActions.GetCheckState();
	}

	return CheckState;
}

/**
 * Called by Slate to determine if this button is enabled
 * 
 * @return True if the menu entry is enabled, false otherwise
 */
bool SToolBarButtonBlock::IsEnabled() const
{
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	TSharedPtr< const FUICommandInfo > Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();

	bool bEnabled = true;
	if( ActionList.IsValid() && Action.IsValid() )
	{
		bEnabled = ActionList->CanExecuteAction( Action.ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		bEnabled = DirectActions.CanExecute();
	}

	return bEnabled;
}


/**
 * Called by Slate to determine if this button is visible
 *
 * @return EVisibility::Visible or EVisibility::Collapsed, depending on if the button should be displayed
 */
EVisibility SToolBarButtonBlock::GetBlockVisibility() const
{
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();
	if( ActionList.IsValid() )
	{
		return ActionList->GetVisibility( MultiBlock->GetAction().ToSharedRef() );
	}
	else if(DirectActions.IsActionVisibleDelegate.IsBound())
	{
		return DirectActions.IsActionVisibleDelegate.Execute() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SToolBarButtonBlock::GetIconVisibility(bool bIsASmallIcon) const
{
	return ((bForceSmallIcons || FMultiBoxSettings::UseSmallToolBarIcons.Get()) ^ bIsASmallIcon) ? EVisibility::Collapsed : EVisibility::Visible;
}

const FSlateBrush* SToolBarButtonBlock::GetIconBrush() const
{
	TSharedRef< const FToolBarButtonBlock > ToolBarButtonBlock = StaticCastSharedRef< const FToolBarButtonBlock >( MultiBlock.ToSharedRef() );

	const FSlateIcon ActionIcon = ToolBarButtonBlock->GetAction().IsValid() ? ToolBarButtonBlock->GetAction()->GetIcon() : FSlateIcon();
	const FSlateIcon& ActualIcon = ToolBarButtonBlock->IconOverride.IsSet() ? ToolBarButtonBlock->IconOverride.Get() : ActionIcon;
	
	if( ActualIcon.IsSet() )
	{
		return ActualIcon.GetIcon();
	}
	else
	{
		check( OwnerMultiBoxWidget.IsValid() );

		TSharedPtr<SMultiBoxWidget> MultiBoxWidget = OwnerMultiBoxWidget.Pin();
		const ISlateStyle* const StyleSet = MultiBoxWidget->GetStyleSet();
		return StyleSet->GetBrush( "MultiBox.GenericToolBarIcon" );
	}
}

const FSlateBrush* SToolBarButtonBlock::GetSmallIconBrush() const
{
	TSharedRef< const FToolBarButtonBlock > ToolBarButtonBlock = StaticCastSharedRef< const FToolBarButtonBlock >( MultiBlock.ToSharedRef() );
	
	const FSlateIcon ActionIcon = ToolBarButtonBlock->GetAction().IsValid() ? ToolBarButtonBlock->GetAction()->GetIcon() : FSlateIcon();
	const FSlateIcon& ActualIcon = ToolBarButtonBlock->IconOverride.IsSet() ? ToolBarButtonBlock->IconOverride.Get() : ActionIcon;
	
	if( ActualIcon.IsSet() )
	{
		return ActualIcon.GetSmallIcon();
	}
	else
	{
		check( OwnerMultiBoxWidget.IsValid() );

		TSharedPtr<SMultiBoxWidget> MultiBoxWidget = OwnerMultiBoxWidget.Pin();
		const ISlateStyle* const StyleSet = MultiBoxWidget->GetStyleSet();
		return StyleSet->GetBrush( "MultiBox.GenericToolBarIcon.Small" );
	}
}
