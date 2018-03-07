// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "SourceControlWindows.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWindow.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "ISourceControlModule.h"
#include "PackageTools.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Linker.h"

#define LOCTEXT_NAMESPACE "SSourceControlRevert"

//-------------------------------------
//Source Control Window Constants
//-------------------------------------
namespace ERevertResults
{
	enum Type
	{
		REVERT_ACCEPTED,
		REVERT_CANCELED
	};
}


struct FRevertCheckBoxListViewItem
{
	/**
	 * Constructor
	 *
	 * @param	InText	String that should appear for the item in the list view
	 */
	FRevertCheckBoxListViewItem( FString InText )
	{
		Text = InText;
		IsSelected = false;
		IsModified = false;
	}

	void OnCheckStateChanged( const ECheckBoxState NewCheckedState )
	{
		IsSelected = (NewCheckedState == ECheckBoxState::Checked);
	}

	ECheckBoxState OnIsChecked() const
	{
		return ( IsSelected ) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	EVisibility OnGetModifiedStateVisibility() const
	{
		return (IsModified) ? EVisibility::Visible : EVisibility::Hidden;
	}

	bool IsSelected;
	bool IsModified;
	FString Text;
};

/**
 * Source control panel for reverting files. Allows the user to select which files should be reverted, as well as
 * provides the option to only allow unmodified files to be reverted.
 */
class SSourceControlRevertWidget : public SCompoundWidget
{
public:

	//* @param	InXamlName		Name of the XAML file defining this panel
	//* @param	InPackageNames	Names of the packages to be potentially reverted
	SLATE_BEGIN_ARGS( SSourceControlRevertWidget )
		: _ParentWindow()
		, _CheckedOutPackages()
	{}

		SLATE_ATTRIBUTE( TSharedPtr<SWindow>, ParentWindow )	
		SLATE_ATTRIBUTE( TArray<FString>, CheckedOutPackages )

	SLATE_END_ARGS()

	/**
	 * Constructor.
	 */
	SSourceControlRevertWidget()
	{
	}

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		ParentFrame = InArgs._ParentWindow.Get();

		for ( TArray<FString>::TConstIterator PackageIter( InArgs._CheckedOutPackages.Get() ); PackageIter; ++PackageIter )
		{
			ListViewItemSource.Add( MakeShareable(new FRevertCheckBoxListViewItem(*PackageIter) ));
		}


		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[

				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SourceControl.Revert", "SelectFiles", "Select the files that should be reverted below"))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10,0)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(5)
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &SSourceControlRevertWidget::ColumnHeaderClicked)
						.IsEnabled(this, &SSourceControlRevertWidget::OnGetItemsEnabled)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SourceControl.Revert", "ListHeader", "File Name"))
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10,0)
				.MaxHeight(300)
				[
					SNew(SBorder)
					.Padding(5)
					[
						SAssignNew(RevertListView, SListViewType)
						.ItemHeight(24)
						.ListItemsSource(&ListViewItemSource)
						.OnGenerateRow(this, &SSourceControlRevertWidget::OnGenerateRowForList)
					]
				]
				+SVerticalBox::Slot()
				.Padding(0, 10, 0, 0)
				.FillHeight(1)
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(15,5)
					.HAlign(HAlign_Left)
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &SSourceControlRevertWidget::RevertUnchangedToggled)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SourceControl.Revert", "RevertUnchanged", "Revert Unchanged Only"))
						]
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.FillWidth(1)
					.Padding(5)
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
							.OnClicked(this, &SSourceControlRevertWidget::OKClicked)
							.IsEnabled(this, &SSourceControlRevertWidget::IsOKEnabled)
							.Text(LOCTEXT("RevertButton", "Revert"))
						]
						+SUniformGridPanel::Slot(1,0)
						[
							SNew(SButton) 
							.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.OnClicked(this, &SSourceControlRevertWidget::CancelClicked)
							.Text(LOCTEXT("CancelButton", "Cancel"))
						]
					]
				]
			]
		];

		// update the modified state of all the files. 
		UpdateSCCStatus();

		DialogResult = ERevertResults::REVERT_CANCELED;
		bRevertUnchangedFilesOnly = false;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Populates the provided array with the names of the packages the user elected to revert, if any.
	 *
	 * @param	OutPackagesToRevert	Array of package names to revert, as specified by the user in the dialog
	 */
	void GetPackagesToRevert( TArray<FString>& OutPackagesToRevert )
	{
		for ( const auto& ListViewItem : ListViewItemSource )
		{
			if ((bRevertUnchangedFilesOnly && !ListViewItem->IsModified) || 
				(!bRevertUnchangedFilesOnly && ListViewItem->IsSelected))
			{
				OutPackagesToRevert.Add(ListViewItem->Text);
			}
		}
	}


	ERevertResults::Type GetResult()
	{
		return DialogResult;
	}

private:

	TSharedRef<ITableRow> OnGenerateRowForList( TSharedPtr<FRevertCheckBoxListViewItem> ListItemPtr, const TSharedRef<STableViewBase>& OwnerTable )
	{
		TSharedPtr<SCheckBox> CheckBox;
		TSharedRef<ITableRow> Row =
			SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
			.IsEnabled(this, &SSourceControlRevertWidget::OnGetItemsEnabled)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SAssignNew(CheckBox, SCheckBox)
					.OnCheckStateChanged(ListItemPtr.ToSharedRef(), &FRevertCheckBoxListViewItem::OnCheckStateChanged)
					.IsChecked(ListItemPtr.ToSharedRef(), &FRevertCheckBoxListViewItem::OnIsChecked)
					[
						SNew(STextBlock)
						.Text(FText::FromString(ListItemPtr->Text))
					]
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush(TEXT("ContentBrowser.ContentDirty")))
					.Visibility(ListItemPtr.ToSharedRef(), &FRevertCheckBoxListViewItem::OnGetModifiedStateVisibility)
					.ToolTipText(LOCTEXT("ModifiedFileToolTip","This file has been modified from the source version"))
				]
			];

		return Row;
	}

	/** Called when the settings of the dialog are to be accepted*/
	FReply OKClicked()
	{
		DialogResult = ERevertResults::REVERT_ACCEPTED;
		ParentFrame.Pin()->RequestDestroyWindow();

		return FReply::Handled();
	}

	bool IsOKEnabled() const
	{
		if (bRevertUnchangedFilesOnly)
		{
			return true;
		}

		for (int32 i=0; i<ListViewItemSource.Num(); i++)
		{
			if (ListViewItemSource[i]->IsSelected)
			{
				return true;
			}
		}
		return false;
	}

	/** Called when the settings of the dialog are to be ignored*/
	FReply CancelClicked()
	{
		DialogResult = ERevertResults::REVERT_CANCELED;
		ParentFrame.Pin()->RequestDestroyWindow();

		return FReply::Handled();
	}

	/** Called when the user checks or unchecks the revert unchanged checkbox; updates the list view accordingly */
	void RevertUnchangedToggled( const ECheckBoxState NewCheckedState )
	{
		bRevertUnchangedFilesOnly = (NewCheckedState == ECheckBoxState::Checked);
	}

	/**
	 * Called whenever a column header is clicked, or in the case of the dialog, also when the "Check/Uncheck All" column header
	 * checkbox is called, because its event bubbles to the column header. 
	 */
	void ColumnHeaderClicked( const ECheckBoxState NewCheckedState )
	{
		for (int32 i=0; i<ListViewItemSource.Num(); i++)
		{
			TSharedPtr<FRevertCheckBoxListViewItem> CurListViewItem = ListViewItemSource[i];

			if (OnGetItemsEnabled())
			{
				CurListViewItem->IsSelected = (NewCheckedState == ECheckBoxState::Checked);
			}
		}
	}

	/** Caches the current state of the files, */
	void UpdateSCCStatus()
	{
		TArray<FString> PackagesToCheck;
		for ( const auto& CurItem : ListViewItemSource )
		{
			PackagesToCheck.Add(SourceControlHelpers::PackageFilename(CurItem->Text));
		}

		// Make sure we update the modified state of the files
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
		UpdateStatusOperation->SetUpdateModifiedState(true);
		ISourceControlModule::Get().GetProvider().Execute(UpdateStatusOperation, PackagesToCheck);

		// Find the files modified from the server version
		TArray< FSourceControlStateRef > SourceControlStates;
		ISourceControlModule::Get().GetProvider().GetState( PackagesToCheck, SourceControlStates, EStateCacheUsage::Use );

		ModifiedPackages.Empty();

		for( const auto& ControlState : SourceControlStates )
		{
			FString PackageName;
			FPackageName::TryConvertFilenameToLongPackageName(ControlState->GetFilename(), PackageName);
			for ( const auto& CurItem : ListViewItemSource )
			{
				if (CurItem->Text == PackageName)
				{
					CurItem->IsModified = ControlState->IsModified();
				}
			}
		}
	}

	/** Check for whether the list items are enabled or not */
	bool OnGetItemsEnabled() const
	{
		return !bRevertUnchangedFilesOnly;
	}

	TWeakPtr<SWindow> ParentFrame;
	ERevertResults::Type DialogResult;

	/** ListView for the packages the user can revert */
	typedef SListView<TSharedPtr<FRevertCheckBoxListViewItem>> SListViewType;
	TSharedPtr<SListViewType> RevertListView;

	/** Collection of items serving as the data source for the list view */
	TArray<TSharedPtr<FRevertCheckBoxListViewItem>> ListViewItemSource;

	/** List of package names that are modified from the versions stored in source control; Used as an optimization */
	TArray<FString> ModifiedPackages;

	/** Flag set by the user to only revert non modified files */
	bool bRevertUnchangedFilesOnly;
};

bool FSourceControlWindows::PromptForRevert( const TArray<FString>& InPackageNames )
{
	bool bReverted = false;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// Only add packages that are actually already checked out to the prompt
	TArray<FString> CheckedOutPackages;
	for ( TArray<FString>::TConstIterator PackageIter( InPackageNames ); PackageIter; ++PackageIter )
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(*PackageIter), EStateCacheUsage::Use);
		if( SourceControlState.IsValid() && SourceControlState->CanCheckIn() )
		{
			CheckedOutPackages.Add( *PackageIter );
		}
	}

	// If any of the packages are checked out, provide the revert prompt
	if ( CheckedOutPackages.Num() > 0 )
	{
		TSharedRef<SWindow> NewWindow = SNew(SWindow)
			.Title( NSLOCTEXT("SourceControl.RevertWindow", "Title", "Revert Files") )
			.SizingRule( ESizingRule::Autosized )
			.SupportsMinimize(false) 
			.SupportsMaximize(false);

		TSharedRef<SSourceControlRevertWidget> SourceControlWidget = 
			SNew(SSourceControlRevertWidget)
			.ParentWindow(NewWindow)
			.CheckedOutPackages(CheckedOutPackages);

		NewWindow->SetContent(SourceControlWidget);

		FSlateApplication::Get().AddModalWindow(NewWindow, NULL);

		// If the user decided to revert some packages, go ahead and do revert the ones they selected
		if ( SourceControlWidget->GetResult() == ERevertResults::REVERT_ACCEPTED )
		{
			TArray<FString> PackagesToRevert;
			SourceControlWidget->GetPackagesToRevert( PackagesToRevert );
			if (PackagesToRevert.Num() > 0)
			{
				// attempt to unload the packages we are about to revert
				TArray<UPackage*> LoadedPackages;
				for (TArray<FString>::TConstIterator PackageIter(InPackageNames); PackageIter; ++PackageIter)
				{
					UPackage* Package = FindPackage(NULL, **PackageIter);
					if (Package != NULL)
					{
						LoadedPackages.Add(Package);
					}
				}

				const TArray<FString> RevertPackageFilenames = SourceControlHelpers::PackageFilenames(PackagesToRevert);

				// Prepare the packages to be reverted...
				for (UPackage* Package : LoadedPackages)
				{
					// Detach the linkers of any loaded packages so that SCC can overwrite the files...
					if (!Package->IsFullyLoaded())
					{
						FlushAsyncLoading();
						Package->FullyLoad();
					}
					ResetLoaders(Package);
				}

				// Revert everything...
				SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), RevertPackageFilenames);

				// Reverting may have deleted some packages, so we need to unload those rather than re-load them...
				TArray<UPackage*> PackagesToUnload;
				LoadedPackages.RemoveAll([&](UPackage* InPackage) -> bool
				{
					const FString PackageExtension = InPackage->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
					const FString PackageFilename = FPackageName::LongPackageNameToFilename(InPackage->GetName(), PackageExtension);
					if (!FPaths::FileExists(PackageFilename))
					{
						PackagesToUnload.Emplace(InPackage);
						return true; // remove package
					}
					return false; // keep package
				});

				// Hot-reload the new packages...
				PackageTools::ReloadPackages(LoadedPackages);

				// Unload any deleted packages...
				PackageTools::UnloadPackages(PackagesToUnload);

				// Re-cache the SCC state...
				SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), RevertPackageFilenames, EConcurrency::Asynchronous);

				bReverted = true;
			}
		}
	}

	return bReverted;
}

#undef LOCTEXT_NAMESPACE
