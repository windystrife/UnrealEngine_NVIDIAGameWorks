// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"
#include "STableRow.h"
#include "STableViewBase.h"
#include "STreeView.h"
#include "NiagaraSystemViewModel.h"
#include "TickableEditorObject.h"
#include "WeakObjectPtrTemplates.h"
#include "NiagaraDataSet.h"
#include "SharedPointer.h"
#include "Map.h"
#include "SCheckBox.h"

class SNiagaraSpreadsheetView : public SCompoundWidget, public FTickableEditorObject
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSpreadsheetView)
	{}

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);
	virtual ~SNiagaraSpreadsheetView();

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	struct FieldInfo
	{
		uint32 FloatStartOffset;
		uint32 IntStartOffset;
		bool bFloat;
		bool bBoolean;
		TWeakObjectPtr<const UEnum> Enum;
	};

protected:

	enum EUITab : uint8
	{
		UIPerParticleUpdate = 0,
		UISystemUpdate,
		UIMax
	};

	void OnTabChanged(ECheckBoxState State,	EUITab Tab);
	ECheckBoxState GetTabCheckedState(EUITab Tab) const;
	EVisibility GetViewVisibility(EUITab Tab) const;

	void GenerateLayoutInfo(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct, const UEnum* Enum, FName BaseName, TArray<FName>& PropertyNames, TArray<FieldInfo>& FieldInfo);

	EUITab TabState;

	struct CapturedUIData
	{
		CapturedUIData() : TargetUsage(ENiagaraScriptUsage::ParticleUpdateScript), LastCaptureTime( -FLT_MAX) , bAwaitingFrame(false), LastReadWriteId(-1),	DataSet( nullptr), bColumnsAreAttributes(true)
		{ }

		TSharedPtr<SHeaderRow> HeaderRow;
		TSharedPtr < STreeView<TSharedPtr<int32> > > ListView;
		TSharedPtr < SCheckBox > CheckBox;
		TArray< TSharedPtr<int32> > SupportedIndices;
		int32 LastReadWriteId;
		FNiagaraDataSet* DataSet;
		TSharedPtr<TArray<FName> > SupportedFields;
		TSharedPtr<TMap<FName, FieldInfo> > FieldInfoMap;
		ENiagaraScriptUsage TargetUsage;
		bool bAwaitingFrame;
		float LastCaptureTime;
		float TargetCaptureTime;
		FGuid LastCaptureHandleId;
		TWeakObjectPtr<UNiagaraEmitter> DataSource;
		TSharedPtr<SScrollBar> HorizontalScrollBar;
		TSharedPtr<SScrollBar> VerticalScrollBar;
		TSharedPtr<SVerticalBox> Container;
		bool bColumnsAreAttributes;
		FText ColumnName;
	};

	TArray<CapturedUIData> CaptureData;

	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;

	UEnum* ScriptEnum;

	void HandleTimeChange();
	
	void OnSequencerTimeChanged();

	FText LastCapturedInfoText() const;

	FReply OnCaptureRequestPressed();
	FReply OnCSVOutputPressed();

	bool CanCapture() const;
	bool IsPausedAtRightTimeOnRightHandle() const;

	void SelectedEmitterHandlesChanged();

	void ResetColumns(EUITab Tab);
	void ResetEntries(EUITab Tab);

	/**
	 * Function called when the currently selected event in the list of thread events changes
	 *
	 * @param Selection Currently selected event
	 * @param SelectInfo Provides context on how the selection changed
	 */
	void OnEventSelectionChanged(TSharedPtr<int32> Selection, ESelectInfo::Type SelectInfo, EUITab Tab);

	/** 
	 * Generates SEventItem widgets for the events tree
	 *
	 * @param InItem Event to generate SEventItem for
	 * @param OwnerTable Owner Table
	 */
	TSharedRef< ITableRow > OnGenerateWidgetForList(TSharedPtr<int32> InItem, const TSharedRef< STableViewBase >& OwnerTable , EUITab Tab);

	/** 
	 * Given a profiler event, generates children for it
	 *
	 * @param InItem Event to generate children for
	 * @param OutChildren Generated children
	 */
	void OnGetChildrenForList(TSharedPtr<int32> InItem, TArray<TSharedPtr<int32>>& OutChildren, EUITab Tab);
};


class SNiagaraSpreadsheetRow : public SMultiColumnTableRow<TSharedPtr<int32> >
{
public:
	typedef TSharedPtr<TArray<FName> > NamesArray;
	typedef TSharedPtr<TMap <FName, SNiagaraSpreadsheetView::FieldInfo> > FieldsMap;

	SLATE_BEGIN_ARGS(SNiagaraSpreadsheetRow)
		: _RowIndex(0), _DataSet(nullptr), _ColumnsAreAttributes(true)
	{}
	SLATE_ARGUMENT(int32, RowIndex)
	SLATE_ARGUMENT(bool, ColumnsAreAttributes)
	SLATE_ARGUMENT(FNiagaraDataSet*, DataSet)
	SLATE_ARGUMENT(NamesArray, SupportedFields)
	SLATE_ARGUMENT(FieldsMap, FieldInfoMap)
	SLATE_END_ARGS()

		/**
		* Generates a widget for debug attribute list column
		*
		* @param InArgs   A declaration from which to construct the widget
		*/
		virtual TSharedRef< SWidget > GenerateWidgetForColumn(const FName& ColumnName) override;


	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

private:
	int32 RowIndex;
	FNiagaraDataSet* DataSet;
	TSharedPtr<TArray<FName> > SupportedFields;
	TSharedPtr<TMap<FName, SNiagaraSpreadsheetView::FieldInfo> > FieldInfoMap;
	bool ColumnsAreAttributes;
};

