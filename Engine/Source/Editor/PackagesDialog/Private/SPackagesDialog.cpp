// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SPackagesDialog.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "UObject/UObjectHash.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "SPackagesDialog"

namespace SPackagesDialogDefs
{
	const FName ColumnID_CheckBoxLabel( "CheckBox" );
	const FName ColumnID_IconLabel( "Icon" );
	const FName ColumnID_FileLabel( "File" );
	const FName ColumnID_TypeLabel( "Type" );
	const FName ColumnID_CheckedOutByLabel( "CheckedOutBy" );

	const float CheckBoxColumnWidth = 23.0f;
	const float IconColumnWidth = 21.0f;
}


UObject* FPackageItem::GetPackageObject() const
{
	if ( !EntryName.StartsWith(TEXT("/Temp/Untitled")) )
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, false);
		for (UObject* Obj : ObjectsInPackage)
		{
			if (Obj->IsAsset())
			{
				return Obj;
			}
		}
	}
	return nullptr;
}

bool FPackageItem::GetTypeNameAndColor(FString& OutName, FColor& OutColor) const
{
	// Resolve the object belonging to the package and cache.
	if (!Object.IsValid())
	{
		Object = GetPackageObject();
	}

	if (Object.IsValid())
	{
		// Load the asset tools module to get access to the class color
		const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Object->GetClass()).Pin();
		if ( AssetTypeActions.IsValid() )
		{
			const FColor EngineBorderColor = AssetTypeActions->GetTypeColor();
			OutColor = FColor(						// Copied from ContentBrowserCLR.cpp
				127 + EngineBorderColor.R / 2,		// Desaturate the colors a bit (GB colors were too.. much)
				127 + EngineBorderColor.G / 2,
				127 + EngineBorderColor.B / 2, 
				200);		// Opacity
			OutName = AssetTypeActions->GetName().ToString();

			return true;
		}
	}

	return false;
}

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SPackagesDialog::Construct(const FArguments& InArgs)
{
	bReadOnly = InArgs._ReadOnly.Get();
	bAllowSourceControlConnection = InArgs._AllowSourceControlConnection.Get();
	Message = InArgs._Message;
	Warning = InArgs._Warning;
	OnSourceControlStateChanged = InArgs._OnSourceControlStateChanged;
	SortByColumn = SPackagesDialogDefs::ColumnID_FileLabel;
	SortMode = EColumnSortMode::Ascending;

	ButtonsBox = SNew( SHorizontalBox );

	if(bAllowSourceControlConnection)
	{
		ButtonsBox->AddSlot()
		.AutoWidth()
		.Padding( 2 )
		[
			SNew(SButton) 
				.Text(LOCTEXT("ConnectToSourceControl", "Connect To Source Control"))
				.ToolTipText(LOCTEXT("ConnectToSourceControl_Tooltip", "Connect to source control to allow source control operations to be performed on content and levels."))
				.ContentPadding(FMargin(10, 3))
				.HAlign(HAlign_Right)
				.Visibility(this, &SPackagesDialog::GetConnectToSourceControlVisibility)
				.OnClicked(this, &SPackagesDialog::OnConnectToSourceControlClicked)
		];
	}

	TSharedRef< SHeaderRow > HeaderRowWidget = SNew( SHeaderRow );

	if (!bReadOnly)
	{
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column( SPackagesDialogDefs::ColumnID_CheckBoxLabel )
			[
				SAssignNew( ToggleSelectedCheckBox, SCheckBox )
				.IsChecked(this, &SPackagesDialog::GetToggleSelectedState)
				.OnCheckStateChanged(this, &SPackagesDialog::OnToggleSelectedCheckBox)
			]
			.FixedWidth( SPackagesDialogDefs::CheckBoxColumnWidth )
		);
	}

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column( SPackagesDialogDefs::ColumnID_IconLabel )
		[
			SNew(SSpacer)
		]
		.SortMode( this, &SPackagesDialog::GetColumnSortMode, SPackagesDialogDefs::ColumnID_IconLabel )
		.OnSort( this, &SPackagesDialog::OnColumnSortModeChanged )
		.FixedWidth( SPackagesDialogDefs::IconColumnWidth )
		);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column( SPackagesDialogDefs::ColumnID_FileLabel )
		.DefaultLabel( LOCTEXT("FileColumnLabel", "File" ) )
		.SortMode( this, &SPackagesDialog::GetColumnSortMode, SPackagesDialogDefs::ColumnID_FileLabel )
		.OnSort( this, &SPackagesDialog::OnColumnSortModeChanged )
		.FillWidth( 7.0f )
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column( SPackagesDialogDefs::ColumnID_TypeLabel )
		.DefaultLabel( LOCTEXT("TypeColumnLabel", "Type" ) )
		.SortMode( this, &SPackagesDialog::GetColumnSortMode, SPackagesDialogDefs::ColumnID_TypeLabel )
		.OnSort( this, &SPackagesDialog::OnColumnSortModeChanged )
		.FillWidth( 2.0f )
		);

	if (bAllowSourceControlConnection)
	{
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SPackagesDialogDefs::ColumnID_CheckedOutByLabel)
			.DefaultLabel(LOCTEXT("CheckedOutByColumnLabel", "Checked Out By"))
			.SortMode(this, &SPackagesDialog::GetColumnSortMode, SPackagesDialogDefs::ColumnID_CheckedOutByLabel)
			.OnSort(this, &SPackagesDialog::OnColumnSortModeChanged)
			.FillWidth(4.0f)
			);
	}

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot() .Padding(10) .AutoHeight()
			[
					SNew( STextBlock )
					.Text( this, &SPackagesDialog::GetMessage )
				.AutoWrapText( true )
				]
			+SVerticalBox::Slot() .Padding(FMargin(10, 0, 10, 10)) .AutoHeight()
			[
				SNew( STextBlock )
				.Text( this, &SPackagesDialog::GetWarning )
				.ColorAndOpacity( FLinearColor::Yellow )
				.AutoWrapText( true )
				.Visibility( this, &SPackagesDialog::GetWarningVisibility )
			]
			+SVerticalBox::Slot()  .FillHeight(0.8)
			[
				SAssignNew(ItemListView, SListView< TSharedPtr<FPackageItem> >)
					.ListItemsSource(&Items)
					.OnGenerateRow(this, &SPackagesDialog::MakePackageListItemWidget)
					.OnContextMenuOpening(this, &SPackagesDialog::MakePackageListContextMenu)
					.ItemHeight(20)
					.HeaderRow( HeaderRowWidget )
					.SelectionMode( ESelectionMode::None )
			]
			+SVerticalBox::Slot() .AutoHeight() .Padding(2) .HAlign(HAlign_Right) .VAlign(VAlign_Bottom)
			[
				ButtonsBox.ToSharedRef()
			]
		]
	];
}

/**
 * Removes all checkbox items from the dialog
 */
void SPackagesDialog::RemoveAll()
{
	Items.Reset();
}

/**
 * Adds a new checkbox item to the dialog
 *
 * @param	Item	The item to be added
 */
void SPackagesDialog::Add(TSharedPtr<FPackageItem> Item)
{
	FSimpleDelegate RefreshCallback = FSimpleDelegate::CreateSP(this, &SPackagesDialog::RefreshButtons);
	Item->SetRefreshCallback(RefreshCallback);
	Items.Add(Item);
	RequestSort();
}

/**
 * Adds a new button to the dialog
 *
 * @param	Button	The button to be added
 */
void SPackagesDialog::AddButton(TSharedPtr<FPackageButton> Button)
{
	Buttons.Add(Button);

	ButtonsBox->AddSlot()
	.AutoWidth()
	.Padding( 2 )
	[
		SNew(SButton) 
			.Text(Button->GetName())
			.ContentPadding(FMargin(10, 3))
			.ToolTipText(Button->GetToolTip())
			.IsEnabled(Button.Get(), &FPackageButton::IsEnabled )
			.HAlign(HAlign_Right)
			.OnClicked(Button.Get(), &FPackageButton::OnButtonClicked)
	];
}

/**
 * Sets the message of the widget
 *
 * @param	InMessage	The string that the message should be set to
 */
void SPackagesDialog::SetMessage(const FText& InMessage)
{
	Message = InMessage;
}

/**
* Sets the warning message of the widget
*
* @param	InMessage	The string that the warning message should be set to
*/
void SPackagesDialog::SetWarning(const FText& InWarning)
{
	Warning = InWarning;
}

/**
 * Gets the return type of the dialog and it populates the package array results
 *
 * @param	OutCheckedPackages		Will be populated with the checked packages
 * @param	OutUncheckedPackages	Will be populated with the unchecked packages
 * @param	OutUndeterminedPackages	Will be populated with the undetermined packages
 *
 * @return	returns the button that was pressed to remove the dialog
 */
EDialogReturnType SPackagesDialog::GetReturnType(OUT TArray<UPackage*>& OutCheckedPackages, OUT TArray<UPackage*>& OutUncheckedPackages, OUT TArray<UPackage*>& OutUndeterminedPackages)
{
	/** Set the return type to which button was pressed */
	EDialogReturnType ReturnType = DRT_None;
	for( int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex)
	{
		FPackageButton& Button = *Buttons[ButtonIndex];
		if(Button.IsClicked())
		{
			ReturnType = Button.GetType();
			break;
		}
	}

	/** Populate the results */
	if(ReturnType != DRT_Cancel && ReturnType != DRT_None)
	{
		for( int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
		{
			FPackageItem& item = *Items[ItemIndex];
			if(item.GetState() == ECheckBoxState::Checked)
			{
				OutCheckedPackages.Add(item.GetPackage());
			}
			else if(item.GetState() == ECheckBoxState::Unchecked)
			{
				OutUncheckedPackages.Add(item.GetPackage());
			}
			else
			{
				OutUndeterminedPackages.Add(item.GetPackage());
			}
		}
	}

	return ReturnType;
}

/**
 * Gets the widget which is to have keyboard focus on activating the dialog
 *
 * @return	returns the widget
 */
TSharedPtr< SWidget > SPackagesDialog::GetWidgetToFocusOnActivate() const
{
	// Find the first visible button.  That will be our widget to focus
	FChildren* ButtonBoxChildren = ButtonsBox->GetChildren();
	for( int ButtonIndex = 0; ButtonIndex < ButtonBoxChildren->Num(); ++ButtonIndex )
	{
		TSharedPtr<SWidget> ButtonWidget = ButtonBoxChildren->GetChildAt(0);
		if(ButtonWidget.IsValid() && ButtonWidget->GetVisibility() == EVisibility::Visible)
		{
			return ButtonWidget;
		}
	}

	return  TSharedPtr< SWidget >();
}

/**
 * Called when the checkbox items have changed state
 */
void SPackagesDialog::RefreshButtons()
{
	int32 CheckedItems = 0;
	int32 UncheckedItems = 0;
	int32 UndeterminedItems = 0;

	/** Count the number of checkboxes that we have for each state */
	for( int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
	{
		FPackageItem& item = *Items[ItemIndex];
		if(item.GetState() == ECheckBoxState::Checked)
		{
			CheckedItems++;
		}
		else if(item.GetState() == ECheckBoxState::Unchecked)
		{
			UncheckedItems++;
		}
		else
		{
			UndeterminedItems++;
		}
	}

	/** Change the button state based on our selection */
	for( int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex)
	{
		FPackageButton& Button = *Buttons[ButtonIndex];
		if(Button.GetType() == DRT_MakeWritable)
		{
			if(UndeterminedItems > 0 || CheckedItems > 0)
				Button.SetDisabled(false);
			else
				Button.SetDisabled(true);
		}
		else if(Button.GetType() == DRT_CheckOut)
		{
			if(CheckedItems > 0)
				Button.SetDisabled(false);
			else
				Button.SetDisabled(true);
		}
	}
}

/** 
 * Makes the widget for the checkbox items in the list view 
 */
TSharedRef<ITableRow> SPackagesDialog::MakePackageListItemWidget( TSharedPtr<FPackageItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( SPackageItemsListRow, OwnerTable )
			.PackagesDialog( SharedThis( this ) )
			.Item( Item );
}


TSharedRef<SWidget> SPackagesDialog::GenerateWidgetForItemAndColumn( TSharedPtr<FPackageItem> Item, const FName ColumnID ) const
{
	check(Item.IsValid());

	// Choose the icon based on the severity
	const FSlateBrush* IconBrush = FEditorStyle::GetBrush( *( Item->GetIconName() ) );

	const FMargin RowPadding(3, 0, 0, 0);

	// Extract the type and color for the package
	FColor PackageColor;
	FString PackageType;
	if (Item->GetTypeNameAndColor(PackageType, PackageColor))
	{
		PackageType = FString(TEXT("(")) + PackageType + FString(TEXT(")"));
	}

	const FString PackageName = Item->GetName();

	TSharedPtr<SWidget> ItemContentWidget;

	if (ColumnID == SPackagesDialogDefs::ColumnID_CheckBoxLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(SCheckBox)
				.IsChecked(Item.Get(), &FPackageItem::OnGetDisplayCheckState)
				.OnCheckStateChanged(Item.Get(), &FPackageItem::OnDisplayCheckStateChanged)
			];
	}
	else if (ColumnID == SPackagesDialogDefs::ColumnID_IconLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image( IconBrush )
				.ToolTipText(FText::FromString(Item->GetToolTip()))
				.IsEnabled(!Item->IsDisabled())
			];
	}
	else if (ColumnID == SPackagesDialogDefs::ColumnID_FileLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(STextBlock)
				.Text(FText::FromString(PackageName))
				.ToolTipText(FText::FromString(PackageName))
				.IsEnabled(!Item->IsDisabled())
			];
	}
	else if (ColumnID == SPackagesDialogDefs::ColumnID_TypeLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(STextBlock)
				.Text(FText::FromString(PackageType))
				.ToolTipText(FText::FromString(PackageType))
				.IsEnabled(!Item->IsDisabled())
				.ColorAndOpacity(PackageColor)
			];
	}
	else if (ColumnID == SPackagesDialogDefs::ColumnID_CheckedOutByLabel)
	{
		check(bAllowSourceControlConnection);

		FString CheckedOutByString = Item->GetCheckedOutByString();

		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CheckedOutByString))
				.ToolTipText(FText::FromString(CheckedOutByString))
				.IsEnabled(!Item->IsDisabled())
				.ColorAndOpacity(PackageColor)
			];
	}

	return ItemContentWidget.ToSharedRef();
}

TSharedPtr<SWidget> SPackagesDialog::MakePackageListContextMenu() const
{
	FMenuBuilder MenuBuilder( true, NULL );

	const TArray< TSharedPtr<FPackageItem> > SelectedItems = GetSelectedItems( false );
	if( SelectedItems.Num() > 0 )
	{	
		MenuBuilder.BeginSection("FilePackage", LOCTEXT("PackageHeading", "Asset"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SCCDiffAgainstDepot", "Diff Against Depot"),
				LOCTEXT("SCCDiffAgainstDepotTooltip", "Look at differences between your version of the asset and that in source control."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP( this, &SPackagesDialog::ExecuteSCCDiffAgainstDepot ),
					FCanExecuteAction::CreateSP( this, &SPackagesDialog::CanExecuteSCCDiffAgainstDepot )
				)
			);	
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

bool SPackagesDialog::CanExecuteSCCDiffAgainstDepot() const
{
	return ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable();
}

void SPackagesDialog::ExecuteSCCDiffAgainstDepot() const
{
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const TArray< TSharedPtr<FPackageItem> > SelectedItems = GetSelectedItems( false );
	for(int32 ItemIdx=0; ItemIdx<SelectedItems.Num(); ItemIdx++)
	{
		const TSharedPtr<FPackageItem> SelectedItem = SelectedItems[ItemIdx];
		check( SelectedItem.IsValid() );

		UObject* Object = SelectedItem->GetPackageObject();
		if( Object )
		{
			const FString PackagePath = SelectedItem->GetName();
			const FString PackageName = FPaths::GetBaseFilename( PackagePath );
			AssetToolsModule.Get().DiffAgainstDepot( Object, PackagePath, PackageName );
		}
	}
}

TArray< TSharedPtr<FPackageItem> > SPackagesDialog::GetSelectedItems( bool bAllIfNone ) const
{
	//get the list of highlighted packages
	TArray< TSharedPtr<FPackageItem> > SelectedItems = ItemListView->GetSelectedItems();
	if ( SelectedItems.Num() == 0 && bAllIfNone )
	{
		//If no packages are explicitly highlighted, return all packages in the list.
		SelectedItems = Items;
	}

	return SelectedItems;
}

ECheckBoxState SPackagesDialog::GetToggleSelectedState() const
{
	//default to a Checked state
	ECheckBoxState PendingState = ECheckBoxState::Checked;

	TArray< TSharedPtr<FPackageItem> > SelectedItems = GetSelectedItems( true );

	//Iterate through the list of selected packages
	for ( auto SelectedItem = SelectedItems.CreateConstIterator(); SelectedItem; ++SelectedItem )
	{
		if ( SelectedItem->Get()->GetState() == ECheckBoxState::Unchecked )
		{
			//if any package in the selection is Unchecked, then represent the entire set of highlighted packages as Unchecked,
			//so that the first (user) toggle of ToggleSelectedCheckBox consistently Checks all highlighted packages
			PendingState = ECheckBoxState::Unchecked;
		}
	}

	return PendingState;
}

void SPackagesDialog::OnToggleSelectedCheckBox(ECheckBoxState InNewState)
{
	TArray< TSharedPtr<FPackageItem> > SelectedItems = GetSelectedItems( true );

	for ( auto SelectedItem = SelectedItems.CreateConstIterator(); SelectedItem; ++SelectedItem )
	{
		FPackageItem *item = (*SelectedItem).Get();
		if (InNewState == ECheckBoxState::Checked)
		{
			if(item->IsDisabled())
			{
				item->SetState(ECheckBoxState::Undetermined);
			}
			else
			{
				item->SetState(ECheckBoxState::Checked);
			}
		}
		else
		{
			item->SetState(ECheckBoxState::Unchecked);
		}
	}

	ItemListView->RequestListRefresh();
}

FReply SPackagesDialog::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if( InKeyEvent.GetKey() == EKeys::Escape )
	{
		for( int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex )
		{
			FPackageButton& Button = *Buttons[ ButtonIndex ];
			if( Button.GetType() == DRT_Cancel )
			{
				return Button.OnButtonClicked();
			}
		}
	}

	return SCompoundWidget::OnKeyDown( MyGeometry, InKeyEvent );
}

EVisibility SPackagesDialog::GetConnectToSourceControlVisibility() const
{
	if(bAllowSourceControlConnection)
	{
		if(!ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FReply SPackagesDialog::OnConnectToSourceControlClicked() const
{
	ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modal);
	OnSourceControlStateChanged.ExecuteIfBound();
	return FReply::Handled();
}

void SPackagesDialog::PopulateIgnoreForSaveItems( const TSet<FString>& InIgnorePackages )
{
	for( auto ItItem=Items.CreateIterator(); ItItem; ++ItItem )
	{
		const FString& ItemName = (*ItItem)->GetName();

		const ECheckBoxState CheckedStatus = (InIgnorePackages.Find(ItemName) != NULL) ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;

		if (!(*ItItem)->IsDisabled())
		{
			(*ItItem)->SetState(CheckedStatus);
		}
	}
}

void SPackagesDialog::PopulateIgnoreForSaveArray( OUT TSet<FString>& InOutIgnorePackages ) const
{
	for( auto ItItem=Items.CreateConstIterator(); ItItem; ++ItItem )
	{
		if((*ItItem)->GetState()==ECheckBoxState::Unchecked)
		{
			InOutIgnorePackages.Add( (*ItItem)->GetName() );
		}
		else
		{
			InOutIgnorePackages.Remove( (*ItItem)->GetName() );
		}
	}
}

void SPackagesDialog::Reset()
{
	for( int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex)
	{
		Buttons[ButtonIndex]->Reset();
	}
}

FText SPackagesDialog::GetMessage() const
{
	return Message;
}

FText SPackagesDialog::GetWarning() const
{
	return Warning;
}

EVisibility SPackagesDialog::GetWarningVisibility() const
{
	return (Warning.IsEmpty()) ? EVisibility::Collapsed : EVisibility::Visible;
}

EColumnSortMode::Type SPackagesDialog::GetColumnSortMode( const FName ColumnId ) const
{
	if ( SortByColumn != ColumnId )
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}

void SPackagesDialog::OnColumnSortModeChanged( const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode )
{
	SortByColumn = ColumnId;
	SortMode = InSortMode;

	RequestSort();
}

void SPackagesDialog::RequestSort()
{
	// Sort the list of root items
	SortTree();

	ItemListView->RequestListRefresh();
}

void SPackagesDialog::SortTree()
{
	if (SortByColumn == SPackagesDialogDefs::ColumnID_FileLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			Items.Sort([](const TSharedPtr<FPackageItem>& A, const TSharedPtr<FPackageItem>& B) {
				return A->GetName() < B->GetName(); } );
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			Items.Sort([](const TSharedPtr<FPackageItem>& A, const TSharedPtr<FPackageItem>& B) {
				return A->GetName() >= B->GetName(); } );
		}
	}
	else if (SortByColumn == SPackagesDialogDefs::ColumnID_TypeLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			Items.Sort([](const TSharedPtr<FPackageItem>& A, const TSharedPtr<FPackageItem>& B) {
				return A->GetTypeName() < B->GetTypeName(); } );
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			Items.Sort([](const TSharedPtr<FPackageItem>& A, const TSharedPtr<FPackageItem>& B) {
				return A->GetTypeName() >= B->GetTypeName(); } );
		}
	}
	else if (SortByColumn == SPackagesDialogDefs::ColumnID_IconLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			Items.Sort([](const TSharedPtr<FPackageItem>& A, const TSharedPtr<FPackageItem>& B) {
				return A->GetIconName() < B->GetIconName(); } );
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			Items.Sort([](const TSharedPtr<FPackageItem>& A, const TSharedPtr<FPackageItem>& B) {
				return A->GetIconName() >= B->GetIconName(); } );
		}
	}
	else if (SortByColumn == SPackagesDialogDefs::ColumnID_CheckedOutByLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			Items.Sort([](const TSharedPtr<FPackageItem>& A, const TSharedPtr<FPackageItem>& B) {
				return A->GetCheckedOutByString() < B->GetCheckedOutByString(); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			Items.Sort([](const TSharedPtr<FPackageItem>& A, const TSharedPtr<FPackageItem>& B) {
				return A->GetCheckedOutByString() >= B->GetCheckedOutByString(); });
		}
	}
}

void SPackageItemsListRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	PackagesDialogWeak = InArgs._PackagesDialog;
	Item = InArgs._Item;

	SMultiColumnTableRow< TSharedPtr<FPackageItem> >::Construct(
		FSuperRowType::FArguments()
		,InOwnerTableView );
}

TSharedRef<SWidget> SPackageItemsListRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	// Create the widget for this item
	auto PackagesDialogShared = PackagesDialogWeak.Pin();
	if (PackagesDialogShared.IsValid())
	{
		return PackagesDialogShared->GenerateWidgetForItemAndColumn( Item, ColumnName );
	}

	// Packages dialog no longer valid; return a valid, null widget.
	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
