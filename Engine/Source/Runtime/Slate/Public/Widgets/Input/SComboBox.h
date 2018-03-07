// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Sound/SlateSound.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Framework/Views/TableViewTypeTraits.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

DECLARE_DELEGATE( FOnComboBoxOpening )


template<typename OptionType>
class SComboRow : public STableRow< OptionType >
{
public:

	SLATE_BEGIN_ARGS( SComboRow )
		: _Style(&FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
		, _Content()
		{}
		SLATE_STYLE_ARGUMENT(FTableRowStyle, Style)
		SLATE_DEFAULT_SLOT( FArguments, Content )
	SLATE_END_ARGS()

public:

	/**
	 * Constructs this widget.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable )
	{
		STableRow< OptionType >::Construct(
			typename STableRow<OptionType>::FArguments()
			.Style(InArgs._Style)
			.Content()
			[
				InArgs._Content.Widget
			]
			, InOwnerTable
		);
	}

	// handle case where user clicks on an existing selected item
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			TSharedPtr< ITypedTableView<OptionType> > OwnerWidget = this->OwnerTablePtr.Pin();

			const OptionType* MyItem = OwnerWidget->Private_ItemFromWidget( this );
			const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );
				
			if (bIsSelected)
			{
				// Reselect content to ensure selection is taken
				OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::Direct);
				return FReply::Handled();
			}
		}
		return STableRow<OptionType>::OnMouseButtonDown(MyGeometry, MouseEvent);
	}
};


/**
 * A combo box that shows arbitrary content.
 */
template< typename OptionType >
class SComboBox : public SComboButton
{
public:
	/** Type of list used for showing menu options. */
	typedef SListView< OptionType > SComboListType;
	/** Delegate type used to generate widgets that represent Options */
	typedef typename TSlateDelegates< OptionType >::FOnGenerateWidget FOnGenerateWidget;
	typedef typename TSlateDelegates< OptionType >::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS( SComboBox )
		: _Content()
		, _ComboBoxStyle( &FCoreStyle::Get().GetWidgetStyle< FComboBoxStyle >( "ComboBox" ) )
		, _ButtonStyle(nullptr)
		, _ItemStyle( &FCoreStyle::Get().GetWidgetStyle< FTableRowStyle >( "TableView.Row" ) )
		, _ContentPadding(FMargin(4.0, 2.0))
		, _ForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
		, _OptionsSource()
		, _OnSelectionChanged()
		, _OnGenerateWidget()
		, _InitiallySelectedItem( nullptr)
		, _Method()
		, _MaxListHeight(450.0f)
		, _HasDownArrow( true )
		, _EnableGamepadNavigationMode(false)
		, _IsFocusable( true )
		{}
		
		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT( FArguments, Content )

		SLATE_STYLE_ARGUMENT( FComboBoxStyle, ComboBoxStyle )

		/** The visual style of the button part of the combo box (overrides ComboBoxStyle) */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )

		SLATE_STYLE_ARGUMENT(FTableRowStyle, ItemStyle)

		SLATE_ATTRIBUTE( FMargin, ContentPadding )
		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )

		SLATE_ARGUMENT( const TArray< OptionType >*, OptionsSource )
		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )
		SLATE_EVENT( FOnGenerateWidget, OnGenerateWidget )

		/** Called when combo box is opened, before list is actually created */
		SLATE_EVENT( FOnComboBoxOpening, OnComboBoxOpening )

		/** The custom scrollbar to use in the ListView */
		SLATE_ARGUMENT(TSharedPtr<SScrollBar>, CustomScrollbar)

		/** The option that should be selected when the combo box is first created */
		SLATE_ARGUMENT( OptionType, InitiallySelectedItem )

		SLATE_ARGUMENT( TOptional<EPopupMethod>, Method )

		/** The max height of the combo box menu */
		SLATE_ARGUMENT(float, MaxListHeight)

		/** The sound to play when the button is pressed (overrides ComboBoxStyle) */
		SLATE_ARGUMENT( TOptional<FSlateSound>, PressedSoundOverride )

		/** The sound to play when the selection changes (overrides ComboBoxStyle) */
		SLATE_ARGUMENT( TOptional<FSlateSound>, SelectionChangeSoundOverride )

		/**
		 * When false, the down arrow is not generated and it is up to the API consumer
		 * to make their own visual hint that this is a drop down.
		 */
		SLATE_ARGUMENT( bool, HasDownArrow )

		/**
		 *  When false, directional keys will change the selection. When true, ComboBox
		 *	must be activated and will only capture arrow input while activated.
		*/
		SLATE_ARGUMENT(bool, EnableGamepadNavigationMode)

		/** When true, allows the combo box to receive keyboard focus */
		SLATE_ARGUMENT( bool, IsFocusable )
				
	SLATE_END_ARGS()

	/**
	 * Construct the widget from a declaration
	 *
	 * @param InArgs   Declaration from which to construct the combo box
	 */
	void Construct( const FArguments& InArgs )
	{
		check(InArgs._ComboBoxStyle);

		ItemStyle = InArgs._ItemStyle;

		// Work out which values we should use based on whether we were given an override, or should use the style's version
		const FComboButtonStyle& OurComboButtonStyle = InArgs._ComboBoxStyle->ComboButtonStyle;
		const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &OurComboButtonStyle.ButtonStyle;
		PressedSound = InArgs._PressedSoundOverride.Get(InArgs._ComboBoxStyle->PressedSlateSound);
		SelectionChangeSound = InArgs._SelectionChangeSoundOverride.Get(InArgs._ComboBoxStyle->SelectionChangeSlateSound);

		this->OnComboBoxOpening = InArgs._OnComboBoxOpening;
		this->OnSelectionChanged = InArgs._OnSelectionChanged;
		this->OnGenerateWidget = InArgs._OnGenerateWidget;
		this->EnableGamepadNavigationMode = InArgs._EnableGamepadNavigationMode;

		OptionsSource = InArgs._OptionsSource;
		CustomScrollbar = InArgs._CustomScrollbar;

		TSharedRef<SWidget> ComboBoxMenuContent =
			SNew(SBox)
			.MaxDesiredHeight(InArgs._MaxListHeight)
			[
				SAssignNew(this->ComboListView, SComboListType)
				.ListItemsSource(InArgs._OptionsSource)
				.OnGenerateRow(this, &SComboBox< OptionType >::GenerateMenuItemRow)
				.OnSelectionChanged(this, &SComboBox< OptionType >::OnSelectionChanged_Internal)
				.SelectionMode(ESelectionMode::Single)
				.ExternalScrollbar(InArgs._CustomScrollbar)
			];

		// Set up content
		TSharedPtr<SWidget> ButtonContent = InArgs._Content.Widget;
		if (InArgs._Content.Widget == SNullWidget::NullWidget)
		{
			 SAssignNew(ButtonContent, STextBlock)
			.Text(NSLOCTEXT("SComboBox", "ContentWarning", "No Content Provided"))
			.ColorAndOpacity( FLinearColor::Red);
		}


		SComboButton::Construct( SComboButton::FArguments()
			.ComboButtonStyle(&OurComboButtonStyle)
			.ButtonStyle(OurButtonStyle)
			.Method( InArgs._Method )
			.ButtonContent()
			[
				ButtonContent.ToSharedRef()
			]
			.MenuContent()
			[
				ComboBoxMenuContent
			]
			.HasDownArrow( InArgs._HasDownArrow )
			.ContentPadding( InArgs._ContentPadding )
			.ForegroundColor( InArgs._ForegroundColor )
			.OnMenuOpenChanged(this, &SComboBox< OptionType >::OnMenuOpenChanged)
			.IsFocusable(InArgs._IsFocusable)
		);
		SetMenuContentWidgetToFocus(ComboListView);

		// Need to establish the selected item at point of construction so its available for querying
		// NB: If you need a selection to fire use SetItemSelection rather than setting an IntiallySelectedItem
		SelectedItem = InArgs._InitiallySelectedItem;
		if( TListTypeTraits<OptionType>::IsPtrValid( SelectedItem ) )
		{
			ComboListView->Private_SetItemSelection( SelectedItem, true);
		}

	}

	void ClearSelection( )
	{
		ComboListView->ClearSelection();
	}

	void SetSelectedItem( OptionType InSelectedItem )
	{
		if (TListTypeTraits<OptionType>::IsPtrValid(InSelectedItem))
		{
			ComboListView->SetSelection(InSelectedItem);
		}
		else
		{
			ComboListView->ClearSelection();
		}
	}

	/** @return the item currently selected by the combo box. */
	OptionType GetSelectedItem()
	{
		return SelectedItem;
	}

	/** 
	 * Requests a list refresh after updating options 
	 * Call SetSelectedItem to update the selected item if required
	 * @see SetSelectedItem
	 */
	void RefreshOptions()
	{
		ComboListView->RequestListRefresh();
	}

protected:
	/** Handle key presses that SListView ignores */
	virtual FReply OnHandleKeyPressed(FKey KeyPressed)
	{
		if (KeyPressed == EKeys::Enter || KeyPressed == EKeys::SpaceBar || KeyPressed == EKeys::Virtual_Accept)
		{
			TArray<OptionType> SelectedItems = ComboListView->GetSelectedItems();
			if (SelectedItems.Num() > 0)
			{
				ComboListView->SetSelection(SelectedItems[0]);
			}
			return FReply::Handled();
		}
		else if (KeyPressed == EKeys::Escape)
		{
			this->SetIsOpen(false);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{

		const FKey KeyPressed = InKeyEvent.GetKey();

		if (IsInteractable())
		{

			if (EnableGamepadNavigationMode)
			{
				// The controller's bottom face button must be pressed once to begin manipulating the combobox's value.
				// Navigation away from the widget is prevented until the button has been pressed again or focus is lost.
				if (KeyPressed == EKeys::Enter || KeyPressed == EKeys::SpaceBar || KeyPressed == EKeys::Virtual_Accept)
				{
					if (bControllerInputCaptured == false)
					{
						// Begin capturing controller input and open the ListView
						bControllerInputCaptured = true;
						PlayPressedSound();
						OnComboBoxOpening.ExecuteIfBound();
						return SComboButton::OnButtonClicked();
					}
					else
					{
						// Set selection to the selected item on the list and close
						bControllerInputCaptured = false;

						// Re-select first selected item, just in case it was selected by navigation previously
						TArray<OptionType> SelectedItems = ComboListView->GetSelectedItems();
						if (SelectedItems.Num() > 0)
						{
							OnSelectionChanged_Internal(SelectedItems[0], ESelectInfo::Direct);
						}

						// Set focus back to ComboBox
						FReply Reply = FReply::Handled();
						Reply.SetUserFocus(this->AsShared(), EFocusCause::SetDirectly);
						return Reply;
					}

				}
				else if (KeyPressed == EKeys::Escape || KeyPressed == EKeys::Virtual_Back || KeyPressed == EKeys::BackSpace)
				{
					OnMenuOpenChanged(false);
				}
				else
				{
					if (bControllerInputCaptured)
					{
						return FReply::Handled();
					}
				}
			}
			else
			{
				if (KeyPressed == EKeys::Up || KeyPressed == EKeys::Gamepad_DPad_Up || KeyPressed == EKeys::Gamepad_LeftStick_Up)
				{
					const int32 SelectionIndex = OptionsSource->Find(GetSelectedItem());
					if (SelectionIndex >= 1)
					{
						// Select an item on the prev row
						SetSelectedItem((*OptionsSource)[SelectionIndex - 1]);
					}

					return FReply::Handled();
				}
				else if (KeyPressed == EKeys::Down || KeyPressed == EKeys::Gamepad_DPad_Down || KeyPressed == EKeys::Gamepad_LeftStick_Down)
				{
					const int32 SelectionIndex = OptionsSource->Find(GetSelectedItem());
					if (SelectionIndex < OptionsSource->Num() - 1)
					{
						// Select an item on the next row
						SetSelectedItem((*OptionsSource)[SelectionIndex + 1]);
					}
					return FReply::Handled();
				}

				return SComboButton::OnKeyDown(MyGeometry, InKeyEvent);
			}
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return bIsFocusable;
	}

	virtual bool IsInteractable() const
	{
		return IsEnabled();
	}

private:

	/** Generate a row for the InItem in the combo box's list (passed in as OwnerTable). Do this by calling the user-specified OnGenerateWidget */
	TSharedRef<ITableRow> GenerateMenuItemRow( OptionType InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		if (OnGenerateWidget.IsBound())
		{
			return SNew(SComboRow<OptionType>, OwnerTable)
				.Style(ItemStyle)
				[
					OnGenerateWidget.Execute(InItem)
				];
		}
		else
		{
			return SNew(SComboRow<OptionType>, OwnerTable)
				[
					SNew(STextBlock).Text(NSLOCTEXT("SlateCore", "ComboBoxMissingOnGenerateWidgetMethod", "Please provide a .OnGenerateWidget() handler."))
				];

		}
	}

	//** Called if the menu is closed
	void OnMenuOpenChanged(bool bOpen)
	{
		if (bOpen == false)
		{
			bControllerInputCaptured = false;

			if (TListTypeTraits<OptionType>::IsPtrValid(SelectedItem))
			{
				// Ensure the ListView selection is set back to the last committed selection
				ComboListView->SetSelection(SelectedItem, ESelectInfo::OnNavigation);
				ComboListView->RequestScrollIntoView(SelectedItem, 0);
			}

			// Set focus back to ComboBox for users focusing the ListView that just closed
			FSlateApplication::Get().ForEachUser([&](FSlateUser* User) {
				if (FSlateApplication::Get().HasUserFocusedDescendants(AsShared(), User->GetUserIndex()))
				{
					FSlateApplication::Get().SetUserFocus(User->GetUserIndex(), AsShared(), EFocusCause::SetDirectly);
				}
			});

		}	
	}


	/** Invoked when the selection in the list changes */
	void OnSelectionChanged_Internal( OptionType ProposedSelection, ESelectInfo::Type SelectInfo )
	{
		// Ensure that the proposed selection is different
		if(SelectInfo != ESelectInfo::OnNavigation)
		{
			// Ensure that the proposed selection is different from selected
			if ( ProposedSelection != SelectedItem )
			{
				PlaySelectionChangeSound();
				SelectedItem = ProposedSelection;
				OnSelectionChanged.ExecuteIfBound( ProposedSelection, SelectInfo );
			}
			// close combo even if user reselected item
			this->SetIsOpen( false );
		}
	}

	/** Handle clicking on the content menu */
	virtual FReply OnButtonClicked() override
	{
		// if user clicked to close the combo menu
		if (this->IsOpen())
		{
			// Re-select first selected item, just in case it was selected by navigation previously
			TArray<OptionType> SelectedItems = ComboListView->GetSelectedItems();
			if (SelectedItems.Num() > 0)
			{
				OnSelectionChanged_Internal(SelectedItems[0], ESelectInfo::Direct);
			}
		}
		else
		{
			PlayPressedSound();
			OnComboBoxOpening.ExecuteIfBound();
		}

		return SComboButton::OnButtonClicked();
	}

	/** Play the pressed sound */
	void PlayPressedSound() const
	{
		FSlateApplication::Get().PlaySound( PressedSound );
	}

	/** Play the selection changed sound */
	void PlaySelectionChangeSound() const
	{
		FSlateApplication::Get().PlaySound( SelectionChangeSound );
	}

	/** The Sound to play when the button is pressed */
	FSlateSound PressedSound;

	/** The Sound to play when the selection is changed */
	FSlateSound SelectionChangeSound;

	/** The item style to use. */
	const FTableRowStyle* ItemStyle;

private:
	/** Delegate that is invoked when the selected item in the combo box changes */
	FOnSelectionChanged OnSelectionChanged;
	/** The item currently selected in the combo box */
	OptionType SelectedItem;
	/** The ListView that we pop up; visualized the available options. */
	TSharedPtr< SComboListType > ComboListView;
	/** The Scrollbar used in the ListView. */
	TSharedPtr< SScrollBar > CustomScrollbar;
	/** Delegate to invoke before the combo box is opening. */
	FOnComboBoxOpening OnComboBoxOpening;
	/** Delegate to invoke when we need to visualize an option as a widget. */
	FOnGenerateWidget OnGenerateWidget;
	// Use activate button to toggle ListView when enabled
	bool EnableGamepadNavigationMode;
	// Holds a flag indicating whether a controller/keyboard is manipulating the combobox's value. 
	// When true, navigation away from the widget is prevented until a new value has been accepted or canceled. 
	bool bControllerInputCaptured;

		

	const TArray< OptionType >* OptionsSource;
};

