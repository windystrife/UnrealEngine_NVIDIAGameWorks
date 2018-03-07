// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SInputBindingEditorPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SChordEditBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "SInputBindingEditorPanel"



void FInputBindingEditorPanel::Initialize(class IDetailLayoutBuilder& InDetailBuilder)
{
	DetailBuilder = &InDetailBuilder;

	UpdateContextMasterList();

	FBindingContext::CommandsChanged.AddSP( SharedThis( this ), &FInputBindingEditorPanel::OnCommandsChanged );

	UpdateUI();
}

void FInputBindingEditorPanel::Tick(float DeltaSeconds)
{
	if(bUpdateRequested)
	{
		UpdateContextMasterList();

		if (DetailBuilder)
		{
			DetailBuilder->ForceRefreshDetails();
			// Force refreshing is going to invalidate the detail builder anyway
			DetailBuilder = nullptr;
			FBindingContext::CommandsChanged.RemoveAll(this);
		}

		bUpdateRequested = false;
	}

	
}

void FInputBindingEditorPanel::UpdateContextMasterList()
{
	TArray< TSharedPtr<FBindingContext> > Contexts;

	FInputBindingManager::Get().GetKnownInputContexts( Contexts );
	
	ContextMasterList.Empty(Contexts.Num());
	struct FContextNameSort
	{
		bool operator()( const TSharedPtr<FBindingContext>& A, const TSharedPtr<FBindingContext>& B ) const
		{
			return A->GetContextDesc().CompareTo( B->GetContextDesc() ) == -1;
		}
	};

	Contexts.Sort( FContextNameSort() );

	for( int32 ListIndex = 0; ListIndex < Contexts.Num(); ++ListIndex )
	{
		const auto& Context = Contexts[ListIndex];

		TSharedRef<FChordTreeItem> TreeItem( new FChordTreeItem );
		TreeItem->BindingContext = Context;
		
		ContextMasterList.Add( TreeItem );
	}
}


void FInputBindingEditorPanel::OnCommandsChanged(const FBindingContext& ContextThatChanged)
{
	bUpdateRequested = true;
}

void FInputBindingEditorPanel::UpdateUI()
{
	for(TSharedPtr<FChordTreeItem>& TreeItem : ContextMasterList)
	{
		check(TreeItem->IsContext());

		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder->EditCategory(TreeItem->GetBindingContext()->GetContextName(), TreeItem->GetBindingContext()->GetContextDesc());

		TArray<TSharedPtr<FUICommandInfo>> Commands;
		GetCommandsForContext(TreeItem, Commands);

		for(TSharedPtr<FUICommandInfo>& CommandInfo : Commands)
		{
			FDetailWidgetRow& Row = CategoryBuilder.AddCustomRow(CommandInfo->GetLabel());
			Row.NameContent()
			.MaxDesiredWidth(0)
			.MinDesiredWidth(500)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(CommandInfo->GetLabel())
					.ToolTipText(CommandInfo->GetDescription())
				]
				+ SVerticalBox::Slot()
				.Padding(0.0f, 3.0f, 0.0f, 3.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FLinearColor::Gray)
					.Text(CommandInfo->GetDescription())
				]
			];

			Row.ValueContent()
			.MaxDesiredWidth(200)
			.MinDesiredWidth(200)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1.0f, 0.0f, 9.0f, 0.0f)
				[
					SNew(SChordEditBox, CommandInfo, EMultipleKeyBindingIndex::Primary)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SChordEditBox, CommandInfo, EMultipleKeyBindingIndex::Secondary)
				]
			];
		}
	}
}

void FInputBindingEditorPanel::GetCommandsForContext(TSharedPtr<FChordTreeItem> InTreeItem, TArray< TSharedPtr< FUICommandInfo > >& OutChildren)
{
	if( InTreeItem->IsContext() )
	{
		FInputBindingManager::Get().GetCommandInfosFromContext( InTreeItem->GetBindingContext()->GetContextName(), OutChildren);

		OutChildren.Sort(FChordSort(true,false));
	}
}


#undef LOCTEXT_NAMESPACE
