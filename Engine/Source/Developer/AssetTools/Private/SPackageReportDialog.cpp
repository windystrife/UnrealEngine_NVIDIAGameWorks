// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SPackageReportDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "PackageReportDialog"

struct FCompareFPackageReportNodeByName
{
	FORCEINLINE bool operator()( TSharedPtr<FPackageReportNode> A, TSharedPtr<FPackageReportNode> B ) const
	{
		return A->NodeName < B->NodeName;
	}
};

FPackageReportNode::FPackageReportNode()
	: bIsFolder(false)
{}

FPackageReportNode::FPackageReportNode(const FString& InNodeName, bool InIsFolder)
	: NodeName(InNodeName)
	, bIsFolder(InIsFolder)
{}

void FPackageReportNode::AddPackage(const FString& PackageName)
{
	TArray<FString> PathElements;
	PackageName.ParseIntoArray(PathElements, TEXT("/"), /*InCullEmpty=*/true);

	return AddPackage_Recursive(PathElements);
}

void FPackageReportNode::ExpandChildrenRecursively(const TSharedRef<PackageReportTree>& Treeview)
{
	for ( auto ChildIt = Children.CreateConstIterator(); ChildIt; ++ChildIt )
	{
		Treeview->SetItemExpansion(*ChildIt, true);
		(*ChildIt)->ExpandChildrenRecursively(Treeview);
	}
}

void FPackageReportNode::AddPackage_Recursive(TArray<FString>& PathElements)
{
	if ( PathElements.Num() > 0 )
	{
		// Pop the bottom element
		FString ChildNodeName = PathElements[0];
		PathElements.RemoveAt(0);

		// Try to find a child which uses this folder name
		TSharedPtr<FPackageReportNode> Child;
		for ( auto ChildIt = Children.CreateConstIterator(); ChildIt; ++ChildIt )
		{
			if ( (*ChildIt)->NodeName == ChildNodeName )
			{
				Child = (*ChildIt);
				break;
			}
		}

		// If one was not found, create it
		if ( !Child.IsValid() )
		{
			const bool bIsAFolder = (PathElements.Num() > 0);
			int32 ChildIdx = Children.Add( MakeShareable(new FPackageReportNode(ChildNodeName, bIsAFolder)) );
			Child = Children[ChildIdx];
			Children.Sort( FCompareFPackageReportNodeByName() );
		}

		if ( ensure(Child.IsValid()) )
		{
			Child->AddPackage_Recursive(PathElements);
		}
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SPackageReportDialog::Construct( const FArguments& InArgs, const FText& InReportMessage, const TArray<FString>& InPackageNames, const FOnReportConfirmed& InOnReportConfirmed )
{
	OnReportConfirmed = InOnReportConfirmed;
	FolderOpenBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
	FolderClosedBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");
	PackageBrush = FEditorStyle::GetBrush("ContentBrowser.ColumnViewAssetIcon");

	ConstructNodeTree(InPackageNames);
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
		.Padding(FMargin(4, 8, 4, 4))
		[
			SNew(SVerticalBox)

			// Report Message
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(STextBlock)
				.Text(InReportMessage)
				.TextStyle( FEditorStyle::Get(), "PackageMigration.DialogTitle" )
			]

			// Tree of packages in the report
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SAssignNew( ReportTreeView, PackageReportTree )
					.TreeItemsSource(&PackageReportRootNode.Children)
					.ItemHeight(18)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow( this, &SPackageReportDialog::GenerateTreeRow )
					.OnGetChildren( this, &SPackageReportDialog::GetChildrenForTree )
				]
			]

			// Ok/Cancel buttons
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0,4,0,0)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding( FEditorStyle::GetMargin("StandardDialog.ContentPadding") )
					.OnClicked(this, &SPackageReportDialog::OkClicked)
					.Text(LOCTEXT("OkButton", "OK"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding( FEditorStyle::GetMargin("StandardDialog.ContentPadding") )
					.OnClicked(this, &SPackageReportDialog::CancelClicked)
					.Text(LOCTEXT("CancelButton", "Cancel"))
				]
			]
		]
	];

	if ( ensure(ReportTreeView.IsValid()) )
	{
		PackageReportRootNode.ExpandChildrenRecursively(ReportTreeView.ToSharedRef());
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SPackageReportDialog::OpenPackageReportDialog(const FText& ReportMessage, const TArray<FString>& PackageNames, const FOnReportConfirmed& InOnReportConfirmed)
{
	TSharedRef<SWindow> ReportWindow = SNew(SWindow)
		.Title(LOCTEXT("ReportWindowTitle", "Asset Report"))
		.ClientSize( FVector2D(600, 500) )
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SPackageReportDialog, ReportMessage, PackageNames, InOnReportConfirmed)
		];

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if ( MainFrameModule.GetParentWindow().IsValid() )
	{
		FSlateApplication::Get().AddWindowAsNativeChild(ReportWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(ReportWindow);
	}
}

void SPackageReportDialog::CloseDialog()
{
	FWidgetPath WidgetPath;
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared(), WidgetPath);

	if ( Window.IsValid() )
	{
		Window->RequestDestroyWindow();
	}
}

TSharedRef<ITableRow> SPackageReportDialog::GenerateTreeRow( TSharedPtr<FPackageReportNode> TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check(TreeItem.IsValid());

	const FSlateBrush* IconBrush = GetNodeIcon(TreeItem);

	return SNew( STableRow< TSharedPtr<FPackageReportNode> >, OwnerTable )
		[
			// Icon
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SImage).Image( IconBrush )
			]

			// Name
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(STextBlock).Text(FText::FromString(TreeItem->NodeName))
			]
		];
}

void SPackageReportDialog::GetChildrenForTree( TSharedPtr<FPackageReportNode> TreeItem, TArray< TSharedPtr<FPackageReportNode> >& OutChildren )
{
	OutChildren = TreeItem->Children;
}

void SPackageReportDialog::ConstructNodeTree(const TArray<FString>& PackageNames)
{
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		PackageReportRootNode.AddPackage(*PackageIt);
	}
}

const FSlateBrush* SPackageReportDialog::GetNodeIcon(const TSharedPtr<FPackageReportNode>& ReportNode) const
{
	if ( !ReportNode->bIsFolder )
	{
		return PackageBrush;
	}
	else if ( ReportTreeView->IsItemExpanded(ReportNode) )
	{
		return FolderOpenBrush;
	}
	else
	{
		return FolderClosedBrush;
	}
}

FReply SPackageReportDialog::OkClicked()
{
	CloseDialog();
	OnReportConfirmed.ExecuteIfBound();

	return FReply::Handled();
}

FReply SPackageReportDialog::CancelClicked()
{
	CloseDialog();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
