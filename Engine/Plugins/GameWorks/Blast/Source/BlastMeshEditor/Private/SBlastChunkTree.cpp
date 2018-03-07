// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SBlastChunkTree.h"
#include "SBlastDepthFilter.h"
#include "IBlastMeshEditor.h"
#include "BlastMeshEditorModule.h"
#include "BlastMesh.h"
#include "BlastMeshEditorStyle.h"
#include "NvBlastTypes.h"

#include "SButton.h"
#include "SCheckBox.h"
#include "EditorStyleSet.h"
#include "Widgets/Images/SImage.h"
#include "SlateOptMacros.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "BlastMeshEditor"

static const FName ColumnID_Visibility("Visibility");
static const FName ColumnID_Chunk("Chunk");

/////////////////////////////////////////////////////////////////////////
// SBlastChunkTreeItem

class SBlastChunkTreeItem : public SMultiColumnTableRow<FBlastChunkEditorModelPtr>
{
public:
	SLATE_BEGIN_ARGS(SBlastChunkTreeItem)
	{
	}
		/** Item model this widget represents */
		SLATE_ARGUMENT(TWeakPtr<IBlastMeshEditor>, InBlastMeshEditorPtr)
		SLATE_ARGUMENT(FBlastChunkEditorModelPtr, InChunkEditorModel)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView);

	TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	bool IsVisibilityEnabled() const;

	/**
	*	Called when the user clicks on the visibility icon for a Level's item widget
	*
	*	@return	A reply that indicated whether this event was handled.
	*/
	FReply OnToggleVisibility();

	const FSlateBrush* GetVisibilityBrush() const;
	const FSlateBrush* GetChunkBrush() const;

private:
	/**	The visibility button for the Level */
	TSharedPtr<SButton>				VisibilityButton;
	TWeakPtr<IBlastMeshEditor>		BlastMeshEditorPtr;
	FBlastChunkEditorModelPtr		ChunkEditorModel;
};

void SBlastChunkTreeItem::Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView)
{
	ChunkEditorModel = InArgs._InChunkEditorModel;
	BlastMeshEditorPtr = InArgs._InBlastMeshEditorPtr;

	SMultiColumnTableRow<FBlastChunkEditorModelPtr>::Construct(
		FSuperRowType::FArguments(),
		OwnerTableView
	);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef< SWidget > SBlastChunkTreeItem::GenerateWidgetForColumn(const FName& ColumnID)
{
	TSharedPtr< SWidget > TableRowContent = SNullWidget::NullWidget;
	if (ColumnID == ColumnID_Visibility)
	{
		TableRowContent =
			SAssignNew(VisibilityButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.IsEnabled(this, &SBlastChunkTreeItem::IsVisibilityEnabled)
			.OnClicked(this, &SBlastChunkTreeItem::OnToggleVisibility)
			.ToolTipText(LOCTEXT("VisibilityButtonToolTip", "Toggle Chunk Visibility"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SBlastChunkTreeItem::GetVisibilityBrush)
			]
		;
	}
	else if (ColumnID == ColumnID_Chunk)
	{
		TableRowContent = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				[
					SNew(SImage)
					.Image(this, &SBlastChunkTreeItem::GetChunkBrush)
				]
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font((*ChunkEditorModel).bBold ? FEditorStyle::GetFontStyle("BoldFont") : FEditorStyle::GetFontStyle("NormalFont"))
				.Text(FText::FromName((*ChunkEditorModel).Name))
			];
	}
	return TableRowContent.ToSharedRef();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool SBlastChunkTreeItem::IsVisibilityEnabled() const
{
	return ChunkEditorModel.IsValid();
}

FReply SBlastChunkTreeItem::OnToggleVisibility()
{
	if (ChunkEditorModel.IsValid() && BlastMeshEditorPtr.IsValid())
	{
		ChunkEditorModel->bVisible = !ChunkEditorModel->bVisible;
		BlastMeshEditorPtr.Pin()->RefreshViewport();
	}
	return FReply::Handled();
}

const FSlateBrush* SBlastChunkTreeItem::GetVisibilityBrush() const
{
	if (ChunkEditorModel.IsValid())
	{
		if (ChunkEditorModel->bVisible)
		{
			return VisibilityButton->IsHovered() ? FEditorStyle::GetBrush("Level.VisibleHighlightIcon16x") :
				FEditorStyle::GetBrush("Level.VisibleIcon16x");
		}
		else
		{
			return VisibilityButton->IsHovered() ? FEditorStyle::GetBrush("Level.NotVisibleHighlightIcon16x") :
				FEditorStyle::GetBrush("Level.NotVisibleIcon16x");
		}
	}
	
	return FEditorStyle::GetBrush("Level.EmptyIcon16x");
	
}

const FSlateBrush* SBlastChunkTreeItem::GetChunkBrush() const
{
	if (ChunkEditorModel.IsValid())
	{
		if (ChunkEditorModel->bSupport)
		{
			if (ChunkEditorModel->bStatic)
			{
				return FBlastMeshEditorStyle::Get()->GetBrush("BlastMeshEditor.SupportStaticChunk");
			}
			else
			{
				return FBlastMeshEditorStyle::Get()->GetBrush("BlastMeshEditor.SupportChunk");
			}
		}
		else
		{
			if (ChunkEditorModel->bStatic)
			{
				return FBlastMeshEditorStyle::Get()->GetBrush("BlastMeshEditor.StaticChunk");
			}
			else
			{
				return FBlastMeshEditorStyle::Get()->GetBrush("BlastMeshEditor.Chunk");
			}
		}
	}

	return FEditorStyle::GetBrush("Level.EmptyIcon16x");

}

/////////////////////////////////////////////////////////////////////////
// SBlastChunkTree

void SBlastChunkTree::Construct(const FArguments& InArgs, TWeakPtr <IBlastMeshEditor> InBlastMeshEditor)
{
	BlastMeshEditorPtr = InBlastMeshEditor;

	ChunkHierarchy = SNew(STreeView<FBlastChunkEditorModelPtr>)
		.SelectionMode(ESelectionMode::Multi)
		.TreeItemsSource(&RootChunks)
		.OnGetChildren(this, &SBlastChunkTree::OnGetChildrenForTree)
		.OnGenerateRow(this, &SBlastChunkTree::OnGenerateRowForTree)
		.OnSelectionChanged(this, &SBlastChunkTree::OnTreeSelectionChanged)
		.OnContextMenuOpening(this, &SBlastChunkTree::ConstructContextMenu)
		.HeaderRow
		(
			SNew(SHeaderRow)
			.Visibility(EVisibility::Collapsed)
			+ SHeaderRow::Column(ColumnID_Chunk)
			.FillWidth(1)
			.DefaultLabel(LOCTEXT("BlastChunkTree_Hierarchy", "Chunk Hierarchy"))
			+ SHeaderRow::Column(ColumnID_Visibility)
			.FixedWidth(25.0f)
			.DefaultLabel(LOCTEXT("BlastChunkTree_visibility", "Chunk Visibility"))
			
		);

	ChildSlot
	[
		SNew(SVerticalBox)
			
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BlastChunkTree_SelectionFilter", "Selection filter"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					[
						SAssignNew(DepthFilter, SBlastDepthFilter)
						.Text(LOCTEXT("BlastChunkTree_DepthFilter", "Depth:"))
						.IsMultipleSelection(true)
						.OnDepthFilterChanged(this, &SBlastChunkTree::OnDepthFilterChanged)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					[
						SAssignNew(SupportChunkFilter, SCheckBox)
						.ToolTipText(LOCTEXT("BlastChunkTree_SupportChunkTT", "Filter support chunks"))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BlastChunkTree_SupportChunk", "Support chunks"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					[
						SAssignNew(StaticChunkFilter, SCheckBox)
						.ToolTipText(LOCTEXT("BlastChunkTree_StaticChunkTT", "Filter static chunks"))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BlastChunkTree_StaticChunk", "Static chunks"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Dark")
						.OnClicked(this, &SBlastChunkTree::ApplySelectionFilter)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FLinearColor::White)
							.Text(LOCTEXT("BlastChunkTree_Apply", "Apply"))
						]
					]
						
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Dark")
						.OnClicked(this, &SBlastChunkTree::ClearSelectionFilter)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FLinearColor::White)
							.Text(LOCTEXT("BlastChunkTree_Clear", "Clear"))
						]
					]
				]
			]
		]

		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SBorder)
			.Padding(8)
			[
				ChunkHierarchy.ToSharedRef()
			]
		]
	];
}

void SBlastChunkTree::Refresh()
{
	if (ChunkHierarchy.IsValid())
	{
		if (DepthFilter.IsValid() && BlastMeshEditorPtr.IsValid())
		{
			DepthFilter->SetBlastMesh(BlastMeshEditorPtr.Pin()->GetBlastMesh());
		}
		UpdateExpansion();

		ChunkHierarchy->RequestTreeRefresh();

		// Force the tree to refresh now instead of next tick
		const FGeometry Stub;
		ChunkHierarchy->Tick(Stub, 0.0f, 0.0f);

		if (!InsideSelectionChanged && (ChunkHierarchy->GetNumItemsSelected() > 0))
		{
			ChunkHierarchy->RequestScrollIntoView(ChunkHierarchy->GetSelectedItems()[0]);
		}
	}
}

void SBlastChunkTree::UpdateSelection()
{
	if (InsideSelectionChanged)	//we only want to update if the change came from viewport
	{
		return;
	}

	InsideSelectionChanged = true;

	if (ChunkHierarchy.Get())
	{
		TArray<FBlastChunkEditorModelPtr>& ChunkEditorModels = BlastMeshEditorPtr.Pin()->GetChunkEditorModels();
		TSet <int32>& SelectedChunkIndices = BlastMeshEditorPtr.Pin()->GetSelectedChunkIndices();
		for (int32 i = 0; i < ChunkEditorModels.Num(); ++i)
		{
			int32 ChunkIndex = (*ChunkEditorModels[i]).ChunkIndex;
			ChunkHierarchy->SetItemSelection(ChunkEditorModels[i], SelectedChunkIndices.Contains(ChunkIndex), ESelectInfo::Direct);
		}
	}

	InsideSelectionChanged = false;
}

void SBlastChunkTree::UpdateExpansion()
{
	auto BME = BlastMeshEditorPtr.Pin();
	UBlastMesh* BlastMesh = BME->GetBlastMesh();
	uint32 CurrentPreviewDepth = BME->GetCurrentPreviewDepth();
	TArray<FBlastChunkEditorModelPtr>& ChunkEditorModels = BME->GetChunkEditorModels();

	//show chunks with current PreviewDepth
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkEditorModels.Num(); ++ChunkIndex)
	{
		ChunkHierarchy->SetItemExpansion(ChunkEditorModels[ChunkIndex], BlastMesh->GetChunkDepth(ChunkIndex) < CurrentPreviewDepth);
	}

	//show selected chunks
	for (int32 ChunkIndex : BME->GetSelectedChunkIndices())
	{
		if (ChunkIndex >= ChunkEditorModels.Num())
		{
			continue;
		}
		auto elem = ChunkEditorModels[ChunkIndex];
		while ((*elem).Parent.IsValid())
		{
			elem = (*elem).Parent;
			ChunkHierarchy->SetItemExpansion(elem, true);
		}
	}
}

void SBlastChunkTree::OnDepthFilterChanged(int32 /*NewDepth*/)
{

}

FReply SBlastChunkTree::ApplySelectionFilter()
{
	auto BME = BlastMeshEditorPtr.Pin();
	auto& Depths = DepthFilter->GetSelectedDepths();
	auto& ChunkModels = BME->GetChunkEditorModels();
	bool ShowLeaves = Depths.Find(FBlastMeshEditorModule::MaxChunkDepth) != INDEX_NONE;
	InsideSelectionChanged = true;
	for (auto& ChunkModel : ChunkModels)
	{
		auto& chunkInfo = BME->GetBlastMesh()->GetChunkInfo(ChunkModel->ChunkIndex);
		bool bSupportFilter = !SupportChunkFilter->IsChecked() //pass if empty
			|| (SupportChunkFilter->IsChecked() && BME->GetBlastMesh()->IsSupportChunk(ChunkModel->ChunkIndex));
		bool bStaticFilter = !StaticChunkFilter->IsChecked() //pass if empty
			|| (StaticChunkFilter->IsChecked() && BME->GetBlastMesh()->IsChunkStatic(ChunkModel->ChunkIndex));
		bool bDepthFilter = !(Depths.Num() || ShowLeaves) //pass if empty
			|| Depths.Find(BME->GetBlastMesh()->GetChunkDepth(ChunkModel->ChunkIndex)) != INDEX_NONE
			|| (ShowLeaves && (chunkInfo.childIndexStop - chunkInfo.firstChildIndex == 0));
		
		ChunkHierarchy->SetItemSelection(ChunkModel, bSupportFilter && bStaticFilter && bDepthFilter, ESelectInfo::Direct);
	}
	InsideSelectionChanged = false;
	if (RootChunks.Num())
	{
		OnTreeSelectionChanged(RootChunks[0], ESelectInfo::Direct);
	}
	return FReply::Handled();
}

FReply SBlastChunkTree::ClearSelectionFilter()
{
	DepthFilter->SetSelectedDepths(TArray<int32>());
	SupportChunkFilter->SetIsChecked(ECheckBoxState::Unchecked);
	StaticChunkFilter->SetIsChecked(ECheckBoxState::Unchecked);
	return FReply::Handled();
}

TSharedRef<ITableRow> SBlastChunkTree::OnGenerateRowForTree(FBlastChunkEditorModelPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SBlastChunkTreeItem, OwnerTable)
		.InBlastMeshEditorPtr(BlastMeshEditorPtr)
		.InChunkEditorModel(Item);
}

void SBlastChunkTree::OnGetChildrenForTree(FBlastChunkEditorModelPtr Parent, TArray<FBlastChunkEditorModelPtr>& OutChildren)
{
	auto& chunkInfo = BlastMeshEditorPtr.Pin()->GetBlastMesh()->GetChunkInfo((*Parent).ChunkIndex);
	TArray<FBlastChunkEditorModelPtr>& ChunkEditorModels = BlastMeshEditorPtr.Pin()->GetChunkEditorModels();
	for (uint32 ChildIndex = chunkInfo.firstChildIndex; ChildIndex < chunkInfo.childIndexStop && ChildIndex < (uint32)ChunkEditorModels.Num(); ChildIndex++)
	{
		(*ChunkEditorModels[ChildIndex]).Parent = Parent;
		OutChildren.Add(ChunkEditorModels[ChildIndex]);
	}
}

void SBlastChunkTree::OnTreeSelectionChanged(FBlastChunkEditorModelPtr TreeElem, ESelectInfo::Type SelectInfo)
{
	// prevent re-entrancy
	if (InsideSelectionChanged)
	{
		return;
	}

	if (!TreeElem.IsValid())
	{
		return;
	}

	InsideSelectionChanged = true;

	TArray<FBlastChunkEditorModelPtr> SelectedElems = ChunkHierarchy->GetSelectedItems();
	TSet <int32>& SelectedChunkIndices = BlastMeshEditorPtr.Pin()->GetSelectedChunkIndices();
	SelectedChunkIndices.Empty(SelectedElems.Num());

	for (FBlastChunkEditorModelPtr SelectedElem : SelectedElems)
	{
		SelectedChunkIndices.Add((*SelectedElem).ChunkIndex);
	}
	if (SelectedChunkIndices.Num())
	{
		BlastMeshEditorPtr.Pin()->UpdateChunkSelection();
	}

	InsideSelectionChanged = false;
}

TSharedPtr<SWidget> SBlastChunkTree::ConstructContextMenu() const
{
	if (ChunkHierarchy.IsValid() && ChunkHierarchy->GetSelectedItems().Num())
	{
		TSharedPtr<FUICommandList> CommandList(new FUICommandList);
		FMenuBuilder MenuBuilder(true, CommandList);

		MenuBuilder.AddMenuEntry(	LOCTEXT("BlastChunkTree_show", "Show"), 
									LOCTEXT("BlastChunkTree_showSelected", "Show selected"), 
									FSlateIcon(), 
									FExecuteAction::CreateSP(this, &SBlastChunkTree::Visibility, true));

		MenuBuilder.AddMenuEntry(	LOCTEXT("BlastChunkTree_hide", "Hide"),
									LOCTEXT("BlastChunkTree_hideSelected", "Hide selected"),
									FSlateIcon(),
									FExecuteAction::CreateSP(this, &SBlastChunkTree::Visibility, false));

		MenuBuilder.AddMenuEntry(	LOCTEXT("BlastChunkTree_setStatic", "Set static flag"),
									LOCTEXT("BlastChunkTree_setStaticTT", "Selected chunks and its parents will fracture as static actors"),
									FSlateIcon(),
									FExecuteAction::CreateSP(this, &SBlastChunkTree::SetStatic, true));

		MenuBuilder.AddMenuEntry(	LOCTEXT("BlastChunkTree_clearStatic", "Clear static flag"),
									LOCTEXT("BlastChunkTree_clearStaticTT", "Selected chunks and its children will fracture as dynamic actors (default)"),
									FSlateIcon(),
									FExecuteAction::CreateSP(this, &SBlastChunkTree::SetStatic, false));

		MenuBuilder.AddMenuEntry(	LOCTEXT("BlastChunkTree_removeChildren", "Remove children"),
									LOCTEXT("BlastChunkTree_removeChildrenTT", "All children of selected chunks will be removed"),
									FSlateIcon(),
									FExecuteAction::CreateSP(this, &SBlastChunkTree::RemoveChildren));
		
		return MenuBuilder.MakeWidget();
	}
	return SNullWidget::NullWidget;
}

void SBlastChunkTree::Visibility(bool isShow)
{
	for (auto& item : ChunkHierarchy->GetSelectedItems())
	{
		item->bVisible = isShow;
	}
	BlastMeshEditorPtr.Pin()->RefreshViewport();
}

void SBlastChunkTree::SetStatic(bool isStatic)
{
	auto BME = BlastMeshEditorPtr.Pin();
	bool isDirty = false;
	for (auto& item : ChunkHierarchy->GetSelectedItems())
	{
		if (BME->GetBlastMesh()->IsChunkStatic(item->ChunkIndex) != isStatic)
		{
			BME->GetBlastMesh()->SetChunkStatic(item->ChunkIndex, isStatic);
			isDirty = true;
		}
	}
	if (isDirty)
	{
		BME->RefreshTool(); //need to update item models, may be we should special method for it
		BME->GetBlastMesh()->MarkPackageDirty();
	}
}

void SBlastChunkTree::RemoveChildren()
{
	auto BME = BlastMeshEditorPtr.Pin();
	BME->RemoveChildren();
}

#undef LOCTEXT_NAMESPACE
