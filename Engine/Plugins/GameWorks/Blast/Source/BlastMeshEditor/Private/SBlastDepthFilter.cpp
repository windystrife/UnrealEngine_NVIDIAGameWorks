// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SBlastDepthFilter.h"
#include "BlastMesh.h"
#include "BlastMeshEditorModule.h"
#include "BlastMeshEditorStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "SUniformGridPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "ISlateStyle.h"
#include "SButton.h"
#include "SToolTip.h"

#define LOCTEXT_NAMESPACE "BlastMeshEditor"

#define MAX_DEPTH_LEVEL_BUTTONS 6
#define ALL_DEPTHS_COLUMNS 5
#define LEAVES_BUTTON_ID -1
#define ALL_DEPTHS_BUTTON_ID -2

#define ADD_BUTTON(container, i) \
	SAssignNew(container[i], SButton) \
	.ButtonStyle(FEditorStyle::Get(), "FlatButton.Dark") \
	.Text(FText::FromString(FString::FormatAsNumber(i))) \
	.IsEnabled(true) \
	.OnClicked(this, &SBlastDepthFilter::OnButtonClicked, i) \
	.VAlign(VAlign_Center) \
	.HAlign(HAlign_Center) \
	.ForegroundColor( FSlateColor::UseForeground() )

#define ADD_BUTTON_TO_GRID(container, i) \
	+ SUniformGridPanel::Slot(i, 0) \
	[ \
		ADD_BUTTON(container, i) \
	]

/////////////////////////////////////////////////////////////////////////
// SBlastDepthFilter

void SBlastDepthFilter::Construct(const FArguments& InArgs)
{
	Text = InArgs._Text;
	IsMultipleSelection = InArgs._IsMultipleSelection;
	OnDepthFilterChanged = InArgs._OnDepthFilterChanged;
	BlastMesh = nullptr;

	FixedDepthsButtons.SetNum(MAX_DEPTH_LEVEL_BUTTONS);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(0.f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.FillWidth(1)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(LeavesButton, SButton)
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Dark")
				.Text(LOCTEXT("BlastDepthFilter_Leaves", "Leaves"))
				.IsEnabled(true)
				.OnClicked(this, &SBlastDepthFilter::OnButtonClicked, LEAVES_BUTTON_ID)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ForegroundColor(FSlateColor::UseForeground())
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(ShowAllDepthsButton, SButton)
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Dark")
				//.Text(LOCTEXT("BlastDepthFilter_A", "A"))
				.IsEnabled(true)
				.OnClicked(this, &SBlastDepthFilter::OnButtonClicked, ALL_DEPTHS_BUTTON_ID)
				.Visibility(EVisibility::Collapsed)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ForegroundColor(FSlateColor::UseForeground())
				[
					TSharedRef<SWidget>(SNew(SImage).Image(
						FBlastMeshEditorStyle::Get()->GetBrush("BlastMeshEditor.ExpandArrow")
					))
				]
				
			]
		]

		+ SVerticalBox::Slot()
		.Padding(.0f)
		.FillHeight(1)
		[
			//SNew(SHorizontalBox)
			SNew(SUniformGridPanel)
			.SlotPadding(1)
			.MinDesiredSlotWidth(27)
			.MinDesiredSlotHeight(27)
				
			ADD_BUTTON_TO_GRID(FixedDepthsButtons, 0)
			ADD_BUTTON_TO_GRID(FixedDepthsButtons, 1)
			ADD_BUTTON_TO_GRID(FixedDepthsButtons, 2)
			ADD_BUTTON_TO_GRID(FixedDepthsButtons, 3)
			ADD_BUTTON_TO_GRID(FixedDepthsButtons, 4)
			ADD_BUTTON_TO_GRID(FixedDepthsButtons, 5)
		]
	];
}

void SBlastDepthFilter::SetBlastMesh(UBlastMesh* InBlastMesh)
{
	BlastMesh = InBlastMesh;
	Refresh();
}

const TArray<int32>& SBlastDepthFilter::GetSelectedDepths() const
{
	return SelectedDepths;
}

void SBlastDepthFilter::SetSelectedDepths(const TArray<int32>& InSelectedDepths)
{
	//Clear current depths
	if (IsMultipleSelection.Get())
	{
		while (SelectedDepths.Num())
		{
			OnButtonClicked(SelectedDepths.Last());
		}
	}
	//Setup new depths
	for (int32 Depth : InSelectedDepths)
	{
		OnButtonClicked(Depth);
	}
}

void SBlastDepthFilter::Refresh()
{
	if (BlastMesh != nullptr)
	{
		const int32 DepthCount = BlastMesh->GetMaxChunkDepth() + 1;
		if (DepthCount > 0)
		{
			// Build depths buttons
			FilterDepths.Empty();
			for (int32 Depth = 0; Depth < DepthCount; ++Depth)
			{
				FilterDepths.Add(MakeShareable(new FString(FString::FromInt(Depth))));
			}
			for (int32 b = 0; b < FixedDepthsButtons.Num(); b++)
			{
				if (FixedDepthsButtons[b].IsValid())
				{
					FixedDepthsButtons[b]->SetVisibility(b < DepthCount ? EVisibility::Visible : EVisibility::Collapsed);
					FixedDepthsButtons[b]->SetCursor(EMouseCursor::Hand);
					FixedDepthsButtons[b]->SetToolTip(
						SNew(SToolTip)
						[
							SNew(STextBlock)
							.Text(FText::Format(LOCTEXT("BlastDepthFilter_ShowDepth", "Show chunks with depth {0}"), FText::AsNumber(b)))
						]
					);
				}
			}

			//build all depths widget
			auto w = SNew(SUniformGridPanel)
				.SlotPadding(1)
				.MinDesiredSlotWidth(25)
				.MinDesiredSlotHeight(25);
			AllDepthsButtons.SetNum(DepthCount);
			for (int32 b = 0; b < AllDepthsButtons.Num(); b++)
			{
				ADD_BUTTON(AllDepthsButtons, b);
				w->AddSlot(b % ALL_DEPTHS_COLUMNS, b / ALL_DEPTHS_COLUMNS).AttachWidget(AllDepthsButtons[b]->AsShared());
			}
			AllDepthsWidget = w->AsShared();

			LeavesButton->SetVisibility(EVisibility::Visible);
			LeavesButton->SetToolTip(
				SNew(SToolTip)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BlastDepthFilter_ShowLeaf", "Show leaves - all unfracturable chunks (chunks without children)"))
				]
			);
			
			ShowAllDepthsButton->SetVisibility(DepthCount > MAX_DEPTH_LEVEL_BUTTONS ? EVisibility::Visible : EVisibility::Collapsed);
			if (DepthCount > MAX_DEPTH_LEVEL_BUTTONS)
			{
				ShowAllDepthsButton->SetToolTip(
					SNew(SToolTip)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BlastDepthFilter_ShowAdditional", "Show all depth buttons"))
					]
				);
			}

			//Update selection
			auto OldSelectedFilterDepths = SelectedDepths;
			SelectedDepths.Reset();
			for (auto s : OldSelectedFilterDepths)
			{		
				OnButtonClicked(s);
			}
		}
	}
}

FReply SBlastDepthFilter::OnButtonClicked(int32 i)
{
	int32 Depth = i;
	const int32 DepthCount = BlastMesh->GetMaxChunkDepth() + 1;
	if (i == LEAVES_BUTTON_ID) //Leaves button clicked
	{
		Depth = FBlastMeshEditorModule::MaxChunkDepth;
	}
	else if (i == ALL_DEPTHS_BUTTON_ID) //All depths button clicked
	{
		FWidgetPath WidgetPath;
		FSlateApplication::Get().FindPathToWidget(ShowAllDepthsButton.ToSharedRef(), WidgetPath);
		auto& g = ShowAllDepthsButton->GetCachedGeometry();
		auto pos = g.GetAccumulatedLayoutTransform().GetTranslation();
		FSlateApplication::Get().PushMenu(ShowAllDepthsButton.ToSharedRef(), WidgetPath, AllDepthsWidget.ToSharedRef(), pos, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}
	if (Depth >= 0)
	{
		//unmark previous selection
		for (int32 s : SelectedDepths)
		{
			if (s == FBlastMeshEditorModule::MaxChunkDepth)
			{
				LeavesButton->SetColorAndOpacity(FLinearColor::White);
			}
			else
			{
				if (s < FixedDepthsButtons.Num())
				{
					FixedDepthsButtons[s]->SetColorAndOpacity(FLinearColor::White);
				}
				if (s < AllDepthsButtons.Num())
				{
					AllDepthsButtons[s]->SetColorAndOpacity(FLinearColor::White);
				}
			}
		}
		ShowAllDepthsButton->SetColorAndOpacity(FLinearColor::White);
		
		//change selection
		if (IsMultipleSelection.Get())
		{
			if (SelectedDepths.Find(Depth) == INDEX_NONE)
			{
				SelectedDepths.Add(Depth);
			}
			else
			{
				SelectedDepths.Remove(Depth);
			}
		}
		else
		{
			SelectedDepths.Empty();
			SelectedDepths.Add(Depth);
		}

		//mark new selection
		for (int32 s : SelectedDepths)
		{
			if (s == FBlastMeshEditorModule::MaxChunkDepth)
			{
				LeavesButton->SetColorAndOpacity(FLinearColor::Green);
			}
			else
			{
				if (s >= MAX_DEPTH_LEVEL_BUTTONS)
				{
					ShowAllDepthsButton->SetColorAndOpacity(FLinearColor::Green);
				}
				if (s < AllDepthsButtons.Num())
				{
					AllDepthsButtons[s]->SetColorAndOpacity(FLinearColor::Green);
				}
				if (s < FixedDepthsButtons.Num())
				{
					FixedDepthsButtons[s]->SetColorAndOpacity(FLinearColor::Green);
				}
			}
		}

		//emit delegate
		OnDepthFilterChanged.ExecuteIfBound(Depth);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
