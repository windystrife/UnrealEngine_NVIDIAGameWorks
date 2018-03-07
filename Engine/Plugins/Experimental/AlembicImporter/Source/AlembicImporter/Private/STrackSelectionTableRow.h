// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "STrackSelectionTableRow.h"

#include "AbcImporter.h"
#include "AbcImportData.h"

#include "SlateOptMacros.h"

struct FAbcTrackInformation;

typedef TSharedPtr<struct FAbcPolyMeshObject> FAbcPolyMeshObjectPtr;

/**
* Implements a row widget for the dispatch state list.
*/
class STrackSelectionTableRow
	: public SMultiColumnTableRow<FAbcPolyMeshObjectPtr>
{
public:

	SLATE_BEGIN_ARGS(STrackSelectionTableRow) { }
	SLATE_ARGUMENT(FAbcPolyMeshObjectPtr, PolyMesh)
	SLATE_END_ARGS()

public:

	/**
	* Constructs the widget.
	*
	* @param InArgs The construction arguments.
	* @param InOwnerTableView The table view that owns this row.
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		check(InArgs._PolyMesh.IsValid());

		PolyMesh = InArgs._PolyMesh;

		SMultiColumnTableRow<FAbcPolyMeshObjectPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);		
	}

public:

	// SMultiColumnTableRow interface

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == "ShouldImport")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(this, &STrackSelectionTableRow::ShouldImportEnabled)
					.OnCheckStateChanged(this, &STrackSelectionTableRow::OnChangeShouldImport)					
				];
		}
		else if (ColumnName == "TrackName")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(PolyMesh->Name))
				];
		}
		else if (ColumnName == "TrackFrameStart")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::FromInt(PolyMesh->StartFrameIndex)))
				];
		}
		else if (ColumnName == "TrackFrameEnd")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::FromInt(PolyMesh->StartFrameIndex + PolyMesh->NumSamples)))
				];
		}
		else if (ColumnName == "TrackFrameNum")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::FromInt(PolyMesh->NumSamples)))
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:
	ECheckBoxState ShouldImportEnabled() const
	{
		return PolyMesh->bShouldImport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnChangeShouldImport(ECheckBoxState NewState)
	{
		PolyMesh->bShouldImport = (NewState == ECheckBoxState::Checked);
	}

private:
	FAbcPolyMeshObjectPtr PolyMesh;
	FText InformationText;
};
