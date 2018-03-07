// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSpreadsheetView.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "ISequencer.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "HAL/PlatformApplicationMisc.h"
#include "EditorStyleSet.h"
#include "SScrollBox.h"
#include "UObjectGlobals.h"
#include "Class.h"
#include "Package.h"
#include "SequencerSettings.h"
#include "NiagaraSystem.h"
#include "NiagaraEditorStyle.h"
#include "PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SNiagaraSpreadsheetView"
#define ARRAY_INDEX_COLUMN_NAME TEXT("Array Index")
#define KEY_COLUMN_NAME TEXT("Property")
#define VALUE_COLUMN_NAME TEXT("Value")

void SNiagaraSpreadsheetRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	RowIndex = InArgs._RowIndex;
	DataSet = InArgs._DataSet;
	ColumnsAreAttributes = InArgs._ColumnsAreAttributes;

	SupportedFields = InArgs._SupportedFields;
	FieldInfoMap = InArgs._FieldInfoMap;

	SMultiColumnTableRow< TSharedPtr<int32> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}


TSharedRef< SWidget > SNiagaraSpreadsheetRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	TSharedPtr<SWidget> EntryWidget;
	const SNiagaraSpreadsheetView::FieldInfo* FieldInfo = nullptr;
	int32 RealRowIdx = 0;
	if (ColumnsAreAttributes && ColumnName == ARRAY_INDEX_COLUMN_NAME)
	{
		EntryWidget = SNew(STextBlock)
			.Text(FText::AsNumber(RowIndex));
	}
	else if (!ColumnsAreAttributes && ColumnName == KEY_COLUMN_NAME)
	{
		EntryWidget = SNew(STextBlock)
			.Text(FText::FromName((*SupportedFields.Get())[RowIndex]));
	}
	else if (ColumnsAreAttributes)
	{
		FieldInfo = FieldInfoMap->Find(ColumnName);
		RealRowIdx = RowIndex;
	}
	else if (!ColumnsAreAttributes && ColumnName == VALUE_COLUMN_NAME)
	{
		FieldInfo = FieldInfoMap->Find((*SupportedFields.Get())[RowIndex]);
	}

	if (FieldInfo != nullptr && DataSet != nullptr && !EntryWidget.IsValid())
	{
		
		if (FieldInfo->bFloat)
		{
			uint32 CompBufferOffset = FieldInfo->FloatStartOffset;
			float* Src = DataSet->PrevData().GetInstancePtrFloat(CompBufferOffset, RealRowIdx);

			EntryWidget = SNew(STextBlock)
				.Text(FText::AsNumber(Src[0]));

		}
		else if (FieldInfo->bBoolean)
		{
			uint32 CompBufferOffset = FieldInfo->IntStartOffset;
			int32* Src = DataSet->PrevData().GetInstancePtrInt32(CompBufferOffset, RealRowIdx);
			FText ValueText;
			if (Src[0] == 0)
			{
				ValueText = LOCTEXT("NiagaraFalse", "False(0)");
			}
			else if (Src[0] == -1)
			{
				ValueText = LOCTEXT("NiagaraTrue", "True(-1)");
			}
			else
			{
				ValueText = FText::Format(LOCTEXT("NiagaraUnknown", "Invalid({0}"), FText::AsNumber(Src[0]));
			}
			EntryWidget = SNew(STextBlock)
				.Text(ValueText);

		}
		else if (FieldInfo->Enum.IsValid())
		{
			uint32 CompBufferOffset = FieldInfo->IntStartOffset;
			int32* Src = DataSet->PrevData().GetInstancePtrInt32(CompBufferOffset, RealRowIdx);
			EntryWidget = SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("EnumValue","{0}({1})"), FieldInfo->Enum->GetDisplayNameTextByValue(Src[0]), FText::AsNumber(Src[0])));

		}
		else
		{
			uint32 CompBufferOffset = FieldInfo->IntStartOffset;
			int32* Src = DataSet->PrevData().GetInstancePtrInt32(CompBufferOffset, RealRowIdx);
			EntryWidget = SNew(STextBlock)
				.Text(FText::AsNumber(Src[0]));

		}
	}
	else if (!EntryWidget.IsValid())
	{
		EntryWidget = SNew(STextBlock)
			.Text(LOCTEXT("UnsupportedColumn", "n/a"));
	}

	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		.Padding(3)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		[
			EntryWidget.ToSharedRef()
		];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SNiagaraSpreadsheetView::Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	TabState = UIPerParticleUpdate;
	ScriptEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENiagaraScriptUsage"));
	ensure(ScriptEnum);
	
	CaptureData.SetNum(2);
	CaptureData[(int)UIPerParticleUpdate].TargetUsage = ENiagaraScriptUsage::ParticleUpdateScript;
	CaptureData[(int)UISystemUpdate].TargetUsage = ENiagaraScriptUsage::SystemUpdateScript;
	CaptureData[(int)UIPerParticleUpdate].ColumnName = LOCTEXT("PerParticleUpdate", "Per-Particle Update");
	CaptureData[(int)UISystemUpdate].ColumnName = LOCTEXT("SystemUpdate", "System Update");
	CaptureData[(int)UIPerParticleUpdate].bColumnsAreAttributes = true;
	CaptureData[(int)UISystemUpdate].bColumnsAreAttributes = false;

	SystemViewModel = InSystemViewModel;
	SystemViewModel->OnSelectedEmitterHandlesChanged().AddRaw(this, &SNiagaraSpreadsheetView::SelectedEmitterHandlesChanged);
	SystemViewModel->OnPostSequencerTimeChanged().AddRaw(this, &SNiagaraSpreadsheetView::OnSequencerTimeChanged);

	for (int32 i = 0; i < (int32)UIMax; i++)
	{
		CaptureData[i].HorizontalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Horizontal)
			.Thickness(FVector2D(8.0f, 8.0f));

		CaptureData[i].VerticalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Vertical)
			.Thickness(FVector2D(8.0f, 8.0f));


		SAssignNew(CaptureData[i].ListView, STreeView< TSharedPtr<int32> >)
			.IsEnabled(this, &SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle)
			// List view items are this tall
			.ItemHeight(12)
			// Tell the list view where to get its source data
			.TreeItemsSource(&CaptureData[i].SupportedIndices)
			// When the list view needs to generate a widget for some data item, use this method
			.OnGenerateRow(this, &SNiagaraSpreadsheetView::OnGenerateWidgetForList, (EUITab)i)
			// Given some DataItem, this is how we find out if it has any children and what they are.
			.OnGetChildren(this, &SNiagaraSpreadsheetView::OnGetChildrenForList, (EUITab)i)
			// Selection mode
			.SelectionMode(ESelectionMode::Single)
			.ExternalScrollbar(CaptureData[i].VerticalScrollBar)
			.ConsumeMouseWheel(EConsumeMouseWheel::Always)
			.AllowOverscroll(EAllowOverscroll::No)
			// Selection callback
			.OnSelectionChanged(this, &SNiagaraSpreadsheetView::OnEventSelectionChanged, (EUITab)i)
			.HeaderRow
			(
				SAssignNew(CaptureData[i].HeaderRow, SHeaderRow)
			);

		SAssignNew(CaptureData[i].CheckBox, SCheckBox)
			//.Style(FEditorStyle::Get(), "PlacementBrowser.Tab")
			.Style(FEditorStyle::Get(), i == 0 ? "Property.ToggleButton.Start" : (i < CaptureData.Num() - 1 ? "Property.ToggleButton.Middle" : "Property.ToggleButton.End"))
			.OnCheckStateChanged(this, &SNiagaraSpreadsheetView::OnTabChanged, (EUITab)i)
			.IsChecked(this, &SNiagaraSpreadsheetView::GetTabCheckedState, (EUITab)i)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.Padding(FMargin(6, 0, 15, 0))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					//.TextStyle(FEditorStyle::Get(), "PlacementBrowser.Tab.Text")
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.AttributeSpreadsheetTabText")
					.Text(CaptureData[i].ColumnName)
				]
			];

		SAssignNew(CaptureData[i].Container, SVerticalBox)
			.Visibility(this, &SNiagaraSpreadsheetView::GetViewVisibility, (EUITab)i)
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					.ExternalScrollbar(CaptureData[i].HorizontalScrollBar)
					+ SScrollBox::Slot()
					[
						CaptureData[i].ListView.ToSharedRef()
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CaptureData[i].VerticalScrollBar.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CaptureData[i].HorizontalScrollBar.ToSharedRef()
			];
		}

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SNew(SHorizontalBox)

					// Toolbar
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.OnClicked(this, &SNiagaraSpreadsheetView::OnCaptureRequestPressed)
						.Text(LOCTEXT("CaptureLabel", "Capture"))
						.ToolTipText(LOCTEXT("CaptureToolitp", "Press this button to capture one frame's contents. Can only capture CPU systems."))
						.IsEnabled(this, &SNiagaraSpreadsheetView::CanCapture)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.OnClicked(this, &SNiagaraSpreadsheetView::OnCSVOutputPressed)
						.Text(LOCTEXT("CSVOutput", "Copy For Excel"))
						.ToolTipText(LOCTEXT("CSVOutputToolitp", "Press this button to put the contents of this spreadsheet in the clipboard in an Excel-friendly format."))
						.IsEnabled(this, &SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle)
					]
				]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoDataText", "Please press capture to examine data from a particular frame."))
					.Visibility_Lambda([&]() {
						if (IsPausedAtRightTimeOnRightHandle())
							return EVisibility::Collapsed;
						return EVisibility::Visible;
					})
				]
				+ SVerticalBox::Slot()
				[
					SNew(STextBlock)
					.Text(this, &SNiagaraSpreadsheetView::LastCapturedInfoText)
					.Visibility_Lambda([&]() {
						if (IsPausedAtRightTimeOnRightHandle())
							return EVisibility::Visible;
						return EVisibility::Collapsed;
					})
				]
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UIPerParticleUpdate].CheckBox.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UISystemUpdate].CheckBox.ToSharedRef()
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UIPerParticleUpdate].Container.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UISystemUpdate].Container.ToSharedRef()
		]
	];
}

SNiagaraSpreadsheetView::~SNiagaraSpreadsheetView()
{
	if (SystemViewModel.IsValid())
	{
		SystemViewModel->OnSelectedEmitterHandlesChanged().RemoveAll(this);
		SystemViewModel->OnPostSequencerTimeChanged().RemoveAll(this);
	}
}

void SNiagaraSpreadsheetView::OnTabChanged(ECheckBoxState State, EUITab Tab)
{
	if (State == ECheckBoxState::Checked)
	{
		TabState = Tab;
	}
}

ECheckBoxState SNiagaraSpreadsheetView::GetTabCheckedState(EUITab Tab) const
{
	return TabState == Tab ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SNiagaraSpreadsheetView::GetViewVisibility(EUITab Tab) const
{
	return TabState == Tab ? EVisibility::Visible : EVisibility::Collapsed;
}


TSharedRef< ITableRow > SNiagaraSpreadsheetView::OnGenerateWidgetForList(TSharedPtr<int32> InItem, const TSharedRef<STableViewBase>& OwnerTable, EUITab Tab)
{
	return SNew(SNiagaraSpreadsheetRow, OwnerTable)
		.RowIndex(*InItem)
		.ColumnsAreAttributes(CaptureData[(int32)Tab].bColumnsAreAttributes)
		.DataSet(CaptureData[(int32)Tab].DataSet)
		.SupportedFields(CaptureData[(int32)Tab].SupportedFields)
		.FieldInfoMap(CaptureData[(int32)Tab].FieldInfoMap);
}

FText SNiagaraSpreadsheetView::LastCapturedInfoText() const
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
	if (SelectedEmitterHandles.Num() == 1 && SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle() && CaptureData[(int32)TabState].DataSet != nullptr)
	{
		return FText::Format(LOCTEXT("LastCapturedInfoName", "Captured Emitter: \"{0}\"     # Particles: {1}     Local Time: {2}     Script Type: {3}"),
			SelectedEmitterHandles[0]->GetNameText(),
			FText::AsNumber(CaptureData[(int32)TabState].DataSet->PrevData().GetNumInstances()),
			FText::AsNumber(CaptureData[(int32)TabState].LastCaptureTime),
			ScriptEnum->GetDisplayNameTextByValue((int64)CaptureData[(int32)TabState].TargetUsage));
	}

	return LOCTEXT("LastCapturedHandleNameStale", "Captured Info: Out-of-date");
}

void SNiagaraSpreadsheetView::OnGetChildrenForList(TSharedPtr<int32> InItem, TArray<TSharedPtr<int32>>& OutChildren, EUITab Tab)
{
	OutChildren.Empty();
}

void SNiagaraSpreadsheetView::SelectedEmitterHandlesChanged()
{
	// Need to reset the attributes list...
	for (int32 i = 0; i < (int32)UIMax; i++)
	{
		CaptureData[i].LastReadWriteId = -1;
		CaptureData[i].DataSet = nullptr;
		CaptureData[i].SupportedIndices.SetNum(0);
		CaptureData[i].ListView->RequestTreeRefresh();
	}
	
}

FReply SNiagaraSpreadsheetView::OnCSVOutputPressed()
{
	if (CaptureData[(int32)TabState].SupportedFields.IsValid() && CaptureData[(int32)TabState].FieldInfoMap.IsValid() && IsPausedAtRightTimeOnRightHandle())
	{
		FString CSVOutput;
		int32 SkipIdx = -1;
		int32 NumWritten = 0;
		TArray<const SNiagaraSpreadsheetView::FieldInfo*> FieldInfos;
		FieldInfos.SetNum(CaptureData[(int32)TabState].SupportedFields->Num());
		FString DelimiterString = TEXT("\t");
		for (int32 i = 0; i < CaptureData[(int32)TabState].SupportedFields->Num(); i++)
		{
			FName Field = (*CaptureData[(int32)TabState].SupportedFields)[i];
			if (Field == ARRAY_INDEX_COLUMN_NAME)
			{
				SkipIdx = i;
				continue;
			}

			if (NumWritten != 0)
			{
				CSVOutput += DelimiterString;
			}

			FieldInfos[i] = CaptureData[(int32)TabState].FieldInfoMap->Find(Field);

			CSVOutput += Field.ToString();
			NumWritten++;
		}

		CSVOutput += "\r\n";

		for (uint32 RowIndex = 0; RowIndex < CaptureData[(int32)TabState].DataSet->PrevData().GetNumInstances(); RowIndex++)
		{
			NumWritten = 0;
			for (int32 i = 0; i < CaptureData[(int32)TabState].SupportedFields->Num(); i++)
			{
				if (i == SkipIdx)
				{
					continue;
				}

				if (NumWritten != 0)
				{
					CSVOutput += DelimiterString;
				}
				const SNiagaraSpreadsheetView::FieldInfo* FieldInfo = FieldInfos[i];
				
				if (FieldInfo != nullptr && CaptureData[(int32)TabState].DataSet != nullptr)
				{
					if (FieldInfo->bFloat)
					{
						uint32 CompBufferOffset = FieldInfo->FloatStartOffset;
						float* Src = CaptureData[(int32)TabState].DataSet->PrevData().GetInstancePtrFloat(CompBufferOffset, RowIndex);
						CSVOutput += FString::Printf(TEXT("%3.3f"), Src[0]);
					}
					else
					{
						uint32 CompBufferOffset = FieldInfo->IntStartOffset;
						int32* Src = CaptureData[(int32)TabState].DataSet->PrevData().GetInstancePtrInt32(CompBufferOffset, RowIndex);
						CSVOutput += FString::Printf(TEXT("%d"), Src[0]);
					}
				}
				NumWritten++;
			}

			CSVOutput += "\r\n";
		}

		FPlatformApplicationMisc::ClipboardCopy(*CSVOutput);
	}

	return FReply::Handled();
}
void SNiagaraSpreadsheetView::OnSequencerTimeChanged()
{
	HandleTimeChange();
}

void SNiagaraSpreadsheetView::Tick(float DeltaTime)
{
	HandleTimeChange();
}

void SNiagaraSpreadsheetView::HandleTimeChange()
{
	for (int32 i = 0; i < (int32)UIMax; i++)
	{
		if (!CaptureData[i].DataSource.IsValid())
		{
			CaptureData[i].bAwaitingFrame = false;
		}

		if (CaptureData[i].bAwaitingFrame)
		{

			TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
			SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
			if (SelectedEmitterHandles.Num() == 1)
			{
				TArray<UNiagaraScript*> Scripts;
				CaptureData[i].DataSource->GetScripts(Scripts);
				Scripts.Add(SystemViewModel->GetSystem().GetSystemSpawnScript(true));
				Scripts.Add(SystemViewModel->GetSystem().GetSystemUpdateScript(true));

				UNiagaraScript* FoundScript = nullptr;
				for (UNiagaraScript* Script : Scripts)
				{
					if (Script->IsEquivalentUsage(CaptureData[i].TargetUsage))
					{
						FoundScript = Script;
						break;
					}
				}

				float LocalCaptureTime = SystemViewModel->GetSequencer()->GetLocalTime();

				if (FoundScript && FoundScript->GetDebuggerInfo().bRequestDebugFrame == false && CaptureData[i].LastReadWriteId != FoundScript->GetDebuggerInfo().DebugFrameLastWriteId)
				{
					CaptureData[i].LastReadWriteId = FoundScript->GetDebuggerInfo().DebugFrameLastWriteId;
					CaptureData[i].DataSet = &FoundScript->GetDebuggerInfo().DebugFrame;
					CaptureData[i].DataSet->Tick(ENiagaraSimTarget::CPUSim); // Force a buffer swap, from here out we read from PrevData.

					CaptureData[i].LastCaptureTime = LocalCaptureTime;
					ensure(CaptureData[i].LastCaptureTime == CaptureData[i].TargetCaptureTime);
					CaptureData[i].LastCaptureHandleId = SelectedEmitterHandles[0]->GetId();

					ResetColumns((EUITab)i);
					ResetEntries((EUITab)i);

					CaptureData[i].bAwaitingFrame = false;
				}
			}
		}
	}
}

bool SNiagaraSpreadsheetView::IsTickable() const
{
	return CaptureData[(int32)TabState].bAwaitingFrame;
}

TStatId SNiagaraSpreadsheetView::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(SNiagaraSpreadsheetView, STATGROUP_Tickables);
}

bool SNiagaraSpreadsheetView::CanCapture() const
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
	if (SelectedEmitterHandles.Num() == 1)
	{
		FNiagaraEmitterHandle* Handle = SelectedEmitterHandles[0]->GetEmitterHandle();
		if (Handle && Handle->GetInstance()->SimTarget == ENiagaraSimTarget::CPUSim)
		{
			return true;
		}
	}
	return false;
}


bool SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle() const
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
	if (SelectedEmitterHandles.Num() == 1)
	{
		return SystemViewModel->GetSequencer()->GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped &&
			CaptureData[(int32)TabState].LastCaptureTime == SystemViewModel->GetSequencer()->GetLocalTime() &&
			CaptureData[(int32)TabState].LastCaptureHandleId == SelectedEmitterHandles[0]->GetId();
	}
	return false;
}

void SNiagaraSpreadsheetView::ResetEntries(EUITab Tab)
{
	if (CaptureData[(int32)Tab].DataSet)
	{
		int32 NumInstances = CaptureData[(int32)Tab].DataSet->GetPrevNumInstances();
		if (!CaptureData[(int32)Tab].bColumnsAreAttributes)
		{
			NumInstances = CaptureData[(int32)Tab].SupportedFields->Num();
		}

		CaptureData[(int32)Tab].SupportedIndices.SetNum(NumInstances);

		for (int32 i = 0; i < NumInstances; i++)
		{
			CaptureData[(int32)Tab].SupportedIndices[i] = MakeShared<int32>(i);
		}

		CaptureData[(int32)Tab].ListView->RequestTreeRefresh();
	}
}

void SNiagaraSpreadsheetView::GenerateLayoutInfo(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct, const UEnum* Enum,  FName BaseName, TArray<FName>& PropertyNames, TArray<SNiagaraSpreadsheetView::FieldInfo>& FieldInfo)
{
	TFieldIterator<UProperty> PropertyCountIt(Struct, EFieldIteratorFlags::IncludeSuper);
	int32 NumProperties = 0;
	for (; PropertyCountIt; ++PropertyCountIt)
	{
		NumProperties++;
	}
	
	for (TFieldIterator<UProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		FName PropertyName = (NumProperties == 1) ? *(BaseName.ToString()) : *(BaseName.ToString() + "." + Property->GetName());
		if (Property->IsA(UFloatProperty::StaticClass()))
		{
			SNiagaraSpreadsheetView::FieldInfo Info;
			Info.bFloat = true;
			Info.FloatStartOffset = Layout.FloatComponentRegisterOffsets.Num();
			Info.IntStartOffset = UINT_MAX;
			Info.Enum = nullptr;
			FieldInfo.Add(Info);
				
			Layout.FloatComponentRegisterOffsets.Add(Layout.GetNumComponents());
			Layout.FloatComponentByteOffsets.Add(Property->GetOffset_ForInternal());
			PropertyNames.Add(PropertyName);
		}
		else if (Property->IsA(UIntProperty::StaticClass()) || Property->IsA(UBoolProperty::StaticClass()))
		{
			SNiagaraSpreadsheetView::FieldInfo Info;
			Info.bFloat = false;
			Info.bBoolean = Property->IsA(UBoolProperty::StaticClass());
			Info.FloatStartOffset = -1.0f;
			Info.IntStartOffset = Layout.Int32ComponentRegisterOffsets.Num();
			Info.Enum = Enum;
			FieldInfo.Add(Info);

			Layout.Int32ComponentRegisterOffsets.Add(Layout.GetNumComponents());
			Layout.Int32ComponentByteOffsets.Add(Property->GetOffset_ForInternal());
			PropertyNames.Add(PropertyName);
		}
		else if (Property->IsA(UEnumProperty::StaticClass()))
		{
			UEnumProperty* EnumProp = CastChecked<UEnumProperty>(Property);
			GenerateLayoutInfo(Layout, FNiagaraTypeDefinition::GetIntStruct(), EnumProp->GetEnum(), PropertyName, PropertyNames, FieldInfo);
		}
		else if (Property->IsA(UStructProperty::StaticClass()))
		{
			UStructProperty* StructProp = CastChecked<UStructProperty>(Property);
			GenerateLayoutInfo(Layout, StructProp->Struct, nullptr, PropertyName, PropertyNames, FieldInfo);
		}
		else
		{
			check(false);
		}
	}
}

void SNiagaraSpreadsheetView::ResetColumns(EUITab Tab)
{
	int32 i = (int32)Tab;

	if (CaptureData[(int32)i].DataSet)
	{
		CaptureData[(int32)i].HeaderRow->ClearColumns();


		CaptureData[(int32)i].SupportedFields = MakeShared<TArray<FName> >();
		CaptureData[(int32)i].FieldInfoMap = MakeShared<TMap<FName, FieldInfo> >();
		uint32 TotalFloatComponents = 0;
		uint32 TotalInt32Components = 0;

		TArray<FNiagaraVariable> Variables = CaptureData[(int32)i].DataSet->GetVariables();


		float ManualWidth = 75.0f;
		TArray<FName> ColumnNames;

		if (CaptureData[(int32)i].bColumnsAreAttributes)
		{
			ColumnNames.Add(ARRAY_INDEX_COLUMN_NAME);
		}
		else
		{
			ManualWidth = 300.0f;
			ColumnNames.Add(KEY_COLUMN_NAME);
			ColumnNames.Add(VALUE_COLUMN_NAME);
		}

		for (const FNiagaraVariable& Var : Variables)
		{
			FNiagaraTypeDefinition TypeDef = Var.GetType();
			const UScriptStruct* Struct = TypeDef.GetScriptStruct();
			const UEnum* Enum = TypeDef.GetEnum();

			FNiagaraTypeLayoutInfo Layout;
			TArray<FName> PropertyNames;
			TArray<SNiagaraSpreadsheetView::FieldInfo> FieldInfos;

			uint32 TotalFloatComponentsBeforeStruct = TotalFloatComponents;
			uint32 TotalInt32ComponentsBeforeStruct = TotalInt32Components;

			GenerateLayoutInfo(Layout, Struct, Enum, Var.GetName(), PropertyNames, FieldInfos);

			for (int32 VarIdx = 0; VarIdx < PropertyNames.Num(); VarIdx++)
			{
				
				{
					if (FieldInfos[VarIdx].bFloat)
					{
						FieldInfos[VarIdx].FloatStartOffset += TotalFloatComponentsBeforeStruct;
						TotalFloatComponents++;
					}
					else
					{
						FieldInfos[VarIdx].IntStartOffset += TotalInt32ComponentsBeforeStruct;
						TotalInt32Components++;
					}

					CaptureData[(int32)i].SupportedFields->Add(PropertyNames[VarIdx]);
					CaptureData[(int32)i].FieldInfoMap->Add(PropertyNames[VarIdx], FieldInfos[VarIdx]);
				}

				if (CaptureData[(int32)i].bColumnsAreAttributes)
				{
					ColumnNames.Add(PropertyNames[VarIdx]);
				}
			}
		}


		for (FName ColumnName : ColumnNames)
		{
			SHeaderRow::FColumn::FArguments ColumnArgs;

			ColumnArgs
				.ColumnId(ColumnName)
				.DefaultLabel(FText::FromName(ColumnName))
				.SortMode(EColumnSortMode::None)
				.HAlignHeader(HAlign_Center)
				.VAlignHeader(VAlign_Fill)
				.HeaderContentPadding(TOptional<FMargin>(2.0f))
				.HAlignCell(HAlign_Fill)
				.VAlignCell(VAlign_Fill)
				.ManualWidth(ManualWidth);
			CaptureData[(int32)i].HeaderRow->AddColumn(ColumnArgs);
		}

		CaptureData[(int32)i].HeaderRow->ResetColumnWidths();
		CaptureData[(int32)i].HeaderRow->RefreshColumns();
		CaptureData[(int32)i].ListView->RequestTreeRefresh();
	}
}

FReply SNiagaraSpreadsheetView::OnCaptureRequestPressed()
{
	float LocalTime = SystemViewModel->GetSequencer()->GetLocalTime();
	float SnapInterval = SystemViewModel->GetSequencer()->GetSequencerSettings()->GetTimeSnapInterval();
	float TargetCaptureTime = LocalTime + SnapInterval;

	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
	ensure(SelectedEmitterHandles.Num() == 1);

	for (int32 i = 0; i < CaptureData.Num(); i++)
	{
		CaptureData[(int32)i].DataSource = SelectedEmitterHandles[0]->GetEmitterHandle()->GetInstance();
		
		TArray<UNiagaraScript*> Scripts;
		CaptureData[(int32)i].DataSource->GetScripts(Scripts);
		Scripts.Add(SystemViewModel->GetSystem().GetSystemSpawnScript(true));
		Scripts.Add(SystemViewModel->GetSystem().GetSystemUpdateScript(true));

		UNiagaraScript* FoundScript = nullptr;
		for (UNiagaraScript* Script : Scripts)
		{
			if (Script->IsEquivalentUsage(CaptureData[(int32)i].TargetUsage))
			{
				FoundScript = Script;
				break;
			}
		}

		if (FoundScript)
		{
			FoundScript->GetDebuggerInfo().bRequestDebugFrame = true;
			CaptureData[(int32)i].bAwaitingFrame = true;

			CaptureData[(int32)i].TargetCaptureTime = TargetCaptureTime;			
		}
	}

	if (SystemViewModel->GetSequencer()->GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped)
	{
		SystemViewModel->GetSequencer()->SetLocalTime(TargetCaptureTime, STM_None);
	}
	else
	{
		SystemViewModel->GetSequencer()->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
		SystemViewModel->GetSequencer()->SetLocalTime(TargetCaptureTime, STM_None);
	}

	return FReply::Handled();
}

void SNiagaraSpreadsheetView::OnEventSelectionChanged(TSharedPtr<int32> Selection, ESelectInfo::Type /*SelectInfo*/, EUITab /*Tab*/)
{
	if (Selection.IsValid())
	{
		// Do nothing for now
	}
}

#undef LOCTEXT_NAMESPACE