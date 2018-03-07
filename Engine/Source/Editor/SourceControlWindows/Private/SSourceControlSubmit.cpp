// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSourceControlSubmit.h"
#include "Misc/MessageDialog.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "SourceControlWindows.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "ISourceControlModule.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "FileHelpers.h"


IMPLEMENT_MODULE( FDefaultModuleImpl, SourceControlWindows );

#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SSourceControlSubmit"


namespace SSourceControlSubmitWidgetDefs
{
	const FName ColumnID_CheckBoxLabel("CheckBox");
	const FName ColumnID_IconLabel("Icon");
	const FName ColumnID_FileLabel("File");

	const float CheckBoxColumnWidth = 23.0f;
	const float IconColumnWidth = 21.0f;
}


FSubmitItem::FSubmitItem(const FSourceControlStateRef& InItem)
	: Item(InItem)
{
	CheckBoxState = ECheckBoxState::Checked;
	DisplayName = FText::FromString(Item->GetFilename());
}


void SSourceControlSubmitListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	SourceControlSubmitWidgetPtr = InArgs._SourceControlSubmitWidget;
	Item = InArgs._Item;

	SMultiColumnTableRow<TSharedPtr<FSubmitItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}


TSharedRef<SWidget> SSourceControlSubmitListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	// Create the widget for this item
	TSharedPtr<SSourceControlSubmitWidget> SourceControlSubmitWidget = SourceControlSubmitWidgetPtr.Pin();
	if (SourceControlSubmitWidget.IsValid())
	{
		return SourceControlSubmitWidget->GenerateWidgetForItemAndColumn(Item, ColumnName);
	}

	// Packages dialog no longer valid; return a valid, null widget.
	return SNullWidget::NullWidget;
}


void SSourceControlSubmitWidget::Construct(const FArguments& InArgs)
{
	ParentFrame = InArgs._ParentWindow.Get();
	SortByColumn = SSourceControlSubmitWidgetDefs::ColumnID_FileLabel;
	SortMode = EColumnSortMode::Ascending;

	for (const auto& Item : InArgs._Items.Get())
	{
		ListViewItems.Add(MakeShareable(new FSubmitItem(Item)));
	}

	TSharedRef<SHeaderRow> HeaderRowWidget = SNew(SHeaderRow);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_CheckBoxLabel)
		[
			SNew(SCheckBox)
			.IsChecked(this, &SSourceControlSubmitWidget::GetToggleSelectedState)
			.OnCheckStateChanged(this, &SSourceControlSubmitWidget::OnToggleSelectedCheckBox)
		]
		.FixedWidth(SSourceControlSubmitWidgetDefs::CheckBoxColumnWidth)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
		[
			SNew(SSpacer)
		]
		.SortMode(this, &SSourceControlSubmitWidget::GetColumnSortMode, SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
		.OnSort(this, &SSourceControlSubmitWidget::OnColumnSortModeChanged)
		.FixedWidth(SSourceControlSubmitWidgetDefs::IconColumnWidth)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
		.DefaultLabel(LOCTEXT("FileColumnLabel", "File"))
		.SortMode(this, &SSourceControlSubmitWidget::GetColumnSortMode, SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
		.OnSort(this, &SSourceControlSubmitWidget::OnColumnSortModeChanged)
		.FillWidth(7.0f)
	);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew( STextBlock )
				.Text( NSLOCTEXT("SourceControl.SubmitPanel", "ChangeListDesc", "Changelist Description") )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(5, 0, 5, 5))
			[
				SNew(SBox)
				.WidthOverride(520)
				[
					SAssignNew( ChangeListDescriptionTextCtrl, SMultiLineEditableTextBox )
					.SelectAllTextWhenFocused( true )
					.AutoWrapText( true )
				]
			]
			+SVerticalBox::Slot()
			.Padding(FMargin(5, 0))
			[
				SNew(SBorder)
				[
					SAssignNew(ListView, SListView<TSharedPtr<FSubmitItem>>)
					.ItemHeight(20)
					.ListItemsSource(&ListViewItems)
					.OnGenerateRow(this, &SSourceControlSubmitWidget::OnGenerateRowForList)
					.HeaderRow(HeaderRowWidget)
					.SelectionMode(ESelectionMode::None)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(5, 5, 5, 0))
			[
				SNew( SBorder)
				.Visibility(this, &SSourceControlSubmitWidget::IsWarningPanelVisible)
				.Padding(5)
				[
					SNew( SErrorText )
					.ErrorText( NSLOCTEXT("SourceControl.SubmitPanel", "ChangeListDescWarning", "Changelist description is required to submit") )
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SWrapBox)
				.UseAllottedWidth(true)
				+SWrapBox::Slot()
				.Padding(0.0f, 0.0f, 16.0f, 0.0f)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged( this, &SSourceControlSubmitWidget::OnCheckStateChanged_KeepCheckedOut)
					.IsChecked( this, &SSourceControlSubmitWidget::GetKeepCheckedOut )
					.IsEnabled( this, &SSourceControlSubmitWidget::CanCheckOut )
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SourceControl.SubmitPanel", "KeepCheckedOut", "Keep Files Checked Out") )
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(0.0f,0.0f,0.0f,5.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.IsEnabled(this, &SSourceControlSubmitWidget::IsOKEnabled)
					.Text( NSLOCTEXT("SourceControl.SubmitPanel", "OKButton", "OK") )
					.OnClicked(this, &SSourceControlSubmitWidget::OKClicked)
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text( NSLOCTEXT("SourceControl.SubmitPanel", "CancelButton", "Cancel") )
					.OnClicked(this, &SSourceControlSubmitWidget::CancelClicked)
				]
			]
		]
	];

	RequestSort();

	DialogResult = ESubmitResults::SUBMIT_CANCELED;
	KeepCheckedOut = ECheckBoxState::Unchecked;

	ParentFrame.Pin()->SetWidgetToFocusOnActivate(ChangeListDescriptionTextCtrl);
}


TSharedRef<SWidget> SSourceControlSubmitWidget::GenerateWidgetForItemAndColumn(TSharedPtr<FSubmitItem> Item, const FName ColumnID) const
{
	check(Item.IsValid());

	const FMargin RowPadding(3, 0, 0, 0);

	TSharedPtr<SWidget> ItemContentWidget;

	if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_CheckBoxLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(SCheckBox)
				.IsChecked(Item.Get(), &FSubmitItem::GetCheckBoxState)
				.OnCheckStateChanged(Item.Get(), &FSubmitItem::SetCheckBoxState)
			];
	}
	else if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(Item->GetIconName()))
				.ToolTipText(Item->GetIconTooltip())
			];
	}
	else if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(STextBlock)
				.Text(Item->GetDisplayName())
			];
	}

	return ItemContentWidget.ToSharedRef();
}


ECheckBoxState SSourceControlSubmitWidget::GetToggleSelectedState() const
{
	// Default to a Checked state
	ECheckBoxState PendingState = ECheckBoxState::Checked;

	// Iterate through the list of selected items
	for (const auto& Item : ListViewItems)
	{
		if (Item->GetCheckBoxState() == ECheckBoxState::Unchecked)
		{
			// If any item in the list is Unchecked, then represent the entire set of highlighted items as Unchecked,
			// so that the first (user) toggle of ToggleSelectedCheckBox consistently Checks all items
			PendingState = ECheckBoxState::Unchecked;
			break;
		}
	}

	return PendingState;
}


void SSourceControlSubmitWidget::OnToggleSelectedCheckBox(ECheckBoxState InNewState)
{
	for (const auto& Item : ListViewItems)
	{
		Item->SetCheckBoxState(InNewState);
	}

	ListView->RequestListRefresh();
}


void SSourceControlSubmitWidget::FillChangeListDescription(FChangeListDescription& OutDesc)
{
	OutDesc.Description = ChangeListDescriptionTextCtrl->GetText();
	OutDesc.FilesForAdd.Empty();
	OutDesc.FilesForSubmit.Empty();

	for (const auto& Item : ListViewItems)
	{
		if (Item->GetCheckBoxState() == ECheckBoxState::Checked)
		{
			if (Item->CanCheckIn())
			{
				OutDesc.FilesForSubmit.Add(Item->GetFilename());
			}
			else if (Item->NeedsAdding())
			{
				OutDesc.FilesForAdd.Add(Item->GetFilename());
			}
		}
	}
}


bool SSourceControlSubmitWidget::WantToKeepCheckedOut()
{
	return KeepCheckedOut == ECheckBoxState::Checked ? true : false;
}


FReply SSourceControlSubmitWidget::OKClicked()
{
	DialogResult = ESubmitResults::SUBMIT_ACCEPTED;
	ParentFrame.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}


FReply SSourceControlSubmitWidget::CancelClicked()
{
	DialogResult = ESubmitResults::SUBMIT_CANCELED;
	ParentFrame.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}


bool SSourceControlSubmitWidget::IsOKEnabled() const
{
	return !ChangeListDescriptionTextCtrl->GetText().IsEmpty();
}


EVisibility SSourceControlSubmitWidget::IsWarningPanelVisible() const
{
	return IsOKEnabled()? EVisibility::Hidden : EVisibility::Visible;
}


void SSourceControlSubmitWidget::OnCheckStateChanged_KeepCheckedOut(ECheckBoxState InState)
{
	KeepCheckedOut = InState;
}


ECheckBoxState SSourceControlSubmitWidget::GetKeepCheckedOut() const
{
	return KeepCheckedOut;
}


bool SSourceControlSubmitWidget::CanCheckOut() const
{
	const ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	return SourceControlProvider.UsesCheckout();
}


TSharedRef<ITableRow> SSourceControlSubmitWidget::OnGenerateRowForList(TSharedPtr<FSubmitItem> SubmitItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> Row =
	SNew(SSourceControlSubmitListRow, OwnerTable)
		.SourceControlSubmitWidget(SharedThis(this))
		.Item(SubmitItem)
		.IsEnabled(SubmitItem->IsEnabled());

	return Row;
}


EColumnSortMode::Type SSourceControlSubmitWidget::GetColumnSortMode(const FName ColumnId) const
{
	if (SortByColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}


void SSourceControlSubmitWidget::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	SortByColumn = ColumnId;
	SortMode = InSortMode;

	RequestSort();
}


void SSourceControlSubmitWidget::RequestSort()
{
	// Sort the list of root items
	SortTree();

	ListView->RequestListRefresh();
}


void SSourceControlSubmitWidget::SortTree()
{
	if (SortByColumn == SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			ListViewItems.Sort([](const TSharedPtr<FSubmitItem>& A, const TSharedPtr<FSubmitItem>& B) {
				return A->GetDisplayName().ToString() < B->GetDisplayName().ToString(); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			ListViewItems.Sort([](const TSharedPtr<FSubmitItem>& A, const TSharedPtr<FSubmitItem>& B) {
				return A->GetDisplayName().ToString() >= B->GetDisplayName().ToString(); });
		}
	}
	else if (SortByColumn == SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			ListViewItems.Sort([](const TSharedPtr<FSubmitItem>& A, const TSharedPtr<FSubmitItem>& B) {
				return A->GetIconName().ToString() < B->GetIconName().ToString(); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			ListViewItems.Sort([](const TSharedPtr<FSubmitItem>& A, const TSharedPtr<FSubmitItem>& B) {
				return A->GetIconName().ToString() >= B->GetIconName().ToString(); });
		}
	}
}


TWeakPtr<SNotificationItem> FSourceControlWindows::ChoosePackagesToCheckInNotification;

void FSourceControlWindows::ChoosePackagesToCheckInCompleted(const TArray<UPackage*>& LoadedPackages, const TArray<FString>& PackageNames, const TArray<FString>& ConfigFiles)
{
	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();

	// Prompt the user to ask if they would like to first save any dirty packages they are trying to check-in
	const FEditorFileUtils::EPromptReturnCode UserResponse = FEditorFileUtils::PromptForCheckoutAndSave(LoadedPackages, true, true);

	// If the user elected to save dirty packages, but one or more of the packages failed to save properly OR if the user
	// canceled out of the prompt, don't follow through on the check-in process
	const bool bShouldProceed = (UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Success || UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Declined);
	if (bShouldProceed)
	{
		TArray<FString> PendingDeletePaths;
		PendingDeletePaths.Add(FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir()));
		PendingDeletePaths.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
		PendingDeletePaths.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()));
		PendingDeletePaths.Add(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));

		const bool bUseSourceControlStateCache = true;
		PromptForCheckin(bUseSourceControlStateCache, PackageNames, PendingDeletePaths, ConfigFiles);
	}
	else
	{
		// If a failure occurred, alert the user that the check-in was aborted. This warning shouldn't be necessary if the user cancelled
		// from the dialog, because they obviously intended to cancel the whole operation.
		if (UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Failure)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SCC_Checkin_Aborted", "Check-in aborted as a result of save failure."));
		}
	}
}

void FSourceControlWindows::ChoosePackagesToCheckInCancelled(FSourceControlOperationRef InOperation)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	SourceControlProvider.CancelOperation(InOperation);

	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();
}

void FSourceControlWindows::ChoosePackagesToCheckInCallback(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();

	if (InResult == ECommandResult::Succeeded)
	{
		// Get a list of all the checked out packages
		TArray<FString> PackageNames;
		TArray<UPackage*> LoadedPackages;
		TMap<FString, FSourceControlStatePtr> PackageStates;
		FEditorFileUtils::FindAllSubmittablePackageFiles(PackageStates, true);

		for (TMap<FString, FSourceControlStatePtr>::TConstIterator PackageIter(PackageStates); PackageIter; ++PackageIter)
		{
			const FString PackageName = *PackageIter.Key();
			const FSourceControlStatePtr CurPackageSCCState = PackageIter.Value();

			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (Package != nullptr)
			{
				LoadedPackages.Add(Package);
			}
			
			PackageNames.Add(PackageName);
		}

		// Get a list of all the checked out config files
		TMap<FString, FSourceControlStatePtr> ConfigFileStates;
		TArray<FString> ConfigFilesToSubmit;
		FEditorFileUtils::FindAllSubmittableConfigFiles(ConfigFileStates);
		for (TMap<FString, FSourceControlStatePtr>::TConstIterator It(ConfigFileStates); It; ++It)
		{
			ConfigFilesToSubmit.Add(It.Key());
		}

		ChoosePackagesToCheckInCompleted(LoadedPackages, PackageNames, ConfigFilesToSubmit);
	}
	else if (InResult == ECommandResult::Failed)
	{
		FMessageLog EditorErrors("EditorErrors");
		EditorErrors.Warning(LOCTEXT("CheckInOperationFailed", "Failed checking source control status!"));
		EditorErrors.Notify();
	}
}

void FSourceControlWindows::ChoosePackagesToCheckIn()
{
	if (ISourceControlModule::Get().IsEnabled())
	{
		if (ISourceControlModule::Get().GetProvider().IsAvailable())
		{
			// make sure we update the SCC status of all packages (this could take a long time, so we will run it as a background task)
			TArray<FString> Filenames;
			Filenames.Add(FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir()));
			Filenames.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
			Filenames.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()));
			Filenames.Add(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));

			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			FSourceControlOperationRef Operation = ISourceControlOperation::Create<FUpdateStatus>();
			StaticCastSharedRef<FUpdateStatus>(Operation)->SetCheckingAllFiles(false);
			SourceControlProvider.Execute(Operation, Filenames, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateStatic(&FSourceControlWindows::ChoosePackagesToCheckInCallback));

			if (ChoosePackagesToCheckInNotification.IsValid())
			{
				ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
			}

			FNotificationInfo Info(LOCTEXT("ChooseAssetsToCheckInIndicator", "Checking for assets to check in..."));
			Info.bFireAndForget = false;
			Info.ExpireDuration = 0.0f;
			Info.FadeOutDuration = 1.0f;

			if (SourceControlProvider.CanCancelOperation(Operation))
			{
				Info.ButtonDetails.Add(FNotificationButtonInfo(
					LOCTEXT("ChoosePackagesToCheckIn_CancelButton", "Cancel"),
					LOCTEXT("ChoosePackagesToCheckIn_CancelButtonTooltip", "Cancel the check in operation."),
					FSimpleDelegate::CreateStatic(&FSourceControlWindows::ChoosePackagesToCheckInCancelled, Operation)
					));
			}

			ChoosePackagesToCheckInNotification = FSlateNotificationManager::Get().AddNotification(Info);

			if (ChoosePackagesToCheckInNotification.IsValid())
			{
				ChoosePackagesToCheckInNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
			}
		}
		else
		{
			FMessageLog EditorErrors("EditorErrors");
			EditorErrors.Warning(LOCTEXT("NoSCCConnection", "No connection to source control available!"))
				->AddToken(FDocumentationToken::Create(TEXT("Engine/UI/SourceControl")));
			EditorErrors.Notify();
		}
	}
}

bool FSourceControlWindows::CanChoosePackagesToCheckIn()
{
	return !ChoosePackagesToCheckInNotification.IsValid();
}


bool FSourceControlWindows::PromptForCheckin(bool bUseSourceControlStateCache, const TArray<FString>& InPackageNames, const TArray<FString>& InPendingDeletePaths, const TArray<FString>& InConfigFiles)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	bool bCheckInSuccess = true;

	// Get filenames for packages and config to be checked in
	TArray<FString> AllFiles = SourceControlHelpers::PackageFilenames(InPackageNames);
	AllFiles.Append(InConfigFiles);

	// Prepare a list of files to have their states updated
	if (!bUseSourceControlStateCache)
	{
		TArray<FString> UpdateRequest;
		UpdateRequest.Append(AllFiles);

		// If there are pending delete paths to update, add them here.
		UpdateRequest.Append(InPendingDeletePaths);

		// Force an update on everything that's been requested
		if (UpdateRequest.Num() > 0)
		{
			SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), UpdateRequest);
		}
	}

	// Get file status of packages and config
	TArray<FSourceControlStateRef> States;
	SourceControlProvider.GetState(AllFiles, States, EStateCacheUsage::Use);

	if (InPendingDeletePaths.Num() > 0)
	{
		// Get any files pending delete
		TArray<FSourceControlStateRef> PendingDeleteItems = SourceControlProvider.GetCachedStateByPredicate(
			[](const FSourceControlStateRef& State) { return State->IsDeleted(); }
		);

		// And append them to the list
		States.Append(PendingDeleteItems);
	}

	if (States.Num())
	{
		TSharedRef<SWindow> NewWindow = SNew(SWindow)
			.Title(NSLOCTEXT("SourceControl.SubmitWindow", "Title", "Submit Files"))
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(600, 400))
			.SupportsMaximize(true)
			.SupportsMinimize(false);

		TSharedRef<SSourceControlSubmitWidget> SourceControlWidget = 
			SNew(SSourceControlSubmitWidget)
			.ParentWindow(NewWindow)
			.Items(States);

		NewWindow->SetContent(
			SourceControlWidget
			);

		FSlateApplication::Get().AddModalWindow(NewWindow, NULL);

		if (SourceControlWidget->GetResult() == ESubmitResults::SUBMIT_ACCEPTED)
		{
			//Get description from the dialog
			FChangeListDescription Description;
			SourceControlWidget->FillChangeListDescription(Description);

			//revert all unchanged files that were submitted
			if ( Description.FilesForSubmit.Num() > 0 )
			{
				SourceControlHelpers::RevertUnchangedFiles(SourceControlProvider, Description.FilesForSubmit);

				//make sure all files are still checked out
				for (int32 VerifyIndex = Description.FilesForSubmit.Num()-1; VerifyIndex >= 0; --VerifyIndex)
				{
					FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Description.FilesForSubmit[VerifyIndex], EStateCacheUsage::Use);
					if( SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded() && !SourceControlState->IsDeleted() )
					{
						Description.FilesForSubmit.RemoveAt(VerifyIndex);
					}
				}
			}

			TArray<FString> CombinedFileList = Description.FilesForAdd;
			CombinedFileList.Append(Description.FilesForSubmit);

			if(Description.FilesForAdd.Num() > 0)
			{
				bCheckInSuccess &= (SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), Description.FilesForAdd) == ECommandResult::Succeeded);
			}

			if(CombinedFileList.Num() > 0)
			{
				TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
				CheckInOperation->SetDescription(Description.Description);
				bCheckInSuccess &= (SourceControlProvider.Execute(CheckInOperation, CombinedFileList) == ECommandResult::Succeeded);

				if(bCheckInSuccess)
				{
					// report success with a notification
					FNotificationInfo Info(CheckInOperation->GetSuccessMessage());
					Info.ExpireDuration = 8.0f;
					Info.HyperlinkText = LOCTEXT("SCC_Checkin_ShowLog", "Show Message Log");
					Info.Hyperlink = FSimpleDelegate::CreateStatic([](){ FMessageLog("SourceControl").Open(EMessageSeverity::Info, true); });
					FSlateNotificationManager::Get().AddNotification(Info);

					// also add to the log
					FMessageLog("SourceControl").Info(CheckInOperation->GetSuccessMessage());
				}
			}

			if(!bCheckInSuccess)
			{
				FMessageLog("SourceControl").Notify(LOCTEXT("SCC_Checkin_Failed", "Failed to check in files!"));
			}
			// If we checked in ok, do we want to to re-check out the files we just checked in?
			else
			{
				if( SourceControlWidget->WantToKeepCheckedOut() == true )
				{
					// Re-check out files
					if(SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), CombinedFileList) != ECommandResult::Succeeded)
					{
						FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("SCC_Checkin_ReCheckOutFailed", "Failed to re-check out files.") );
					}
				}
			}
		}
	}
	else
	{
		FMessageLog EditorErrors("EditorErrors");
		EditorErrors.Warning(LOCTEXT("NoAssetsToCheckIn", "No assets to check in!"));
		EditorErrors.Notify();
	}

	return bCheckInSuccess;
}


#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
