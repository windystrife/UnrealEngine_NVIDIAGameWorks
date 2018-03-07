// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundWaveDetails.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Sound/SoundWave.h"
#include "DetailLayoutBuilder.h"
#include "ScopedTransaction.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Engine/CurveTable.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FSoundWaveDetails"

static const FName InternalCurveTableName("InternalCurveTable");

TSharedRef<IDetailCustomization> FSoundWaveDetails::MakeInstance()
{
	return MakeShareable(new FSoundWaveDetails);
}

void FSoundWaveDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	CustomizeCurveDetails(DetailBuilder);
}

void FSoundWaveDetails::CustomizeCurveDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() == 1 && Objects[0].IsValid())
	{
		USoundWave* SoundWave = CastChecked<USoundWave>(Objects[0].Get());

		TSharedRef<IPropertyHandle> CurvePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USoundWave, Curves));
		if(CurvePropertyHandle->IsValidHandle())
		{
			IDetailPropertyRow& CurvePropertyRow = DetailBuilder.EditCategory(TEXT("Curves")).AddProperty(CurvePropertyHandle);
			TSharedPtr<SWidget> DefaultNameWidget;
			TSharedPtr<SWidget> DefaultValueWidget;
			CurvePropertyRow.GetDefaultWidgets(DefaultNameWidget, DefaultValueWidget);

			CurvePropertyRow.CustomWidget()
			.NameContent()
			[
				DefaultNameWidget.ToSharedRef()
			]
			.ValueContent()
			.MaxDesiredWidth(TOptional<float>())
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					DefaultValueWidget.ToSharedRef()
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Visibility(this, &FSoundWaveDetails::GetMakeInternalCurvesVisibility, SoundWave, CurvePropertyHandle)
					.OnClicked(this, &FSoundWaveDetails::HandleMakeInternalCurves, SoundWave)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MakeInternal", "Copy To Internal"))
						.ToolTipText(LOCTEXT("MakeInternalTooltip", "Convert the currently selected curve table to an internal curve table."))
						.Font(DetailBuilder.GetDetailFont())
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Visibility(this, &FSoundWaveDetails::GetUseInternalCurvesVisibility, SoundWave, CurvePropertyHandle)
					.OnClicked(this, &FSoundWaveDetails::HandleUseInternalCurves, SoundWave, CurvePropertyHandle)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UseInternal", "Use Internal"))
						.ToolTipText(LOCTEXT("UseInternalTooltip", "Use the curve table internal to this sound wave."))
						.Font(DetailBuilder.GetDetailFont())
					]
				]
			];
		}
	}
}

EVisibility FSoundWaveDetails::GetMakeInternalCurvesVisibility(USoundWave* SoundWave, TSharedRef<IPropertyHandle> CurvePropertyHandle) const
{
	UObject* CurrentCurveTable = nullptr;
	if (CurvePropertyHandle->GetValue(CurrentCurveTable) == FPropertyAccess::Success)
	{
		if (CurrentCurveTable != nullptr && CurrentCurveTable->HasAnyFlags(RF_Public) && CurrentCurveTable->GetOutermost() != SoundWave->GetOutermost())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

EVisibility FSoundWaveDetails::GetUseInternalCurvesVisibility(USoundWave* SoundWave, TSharedRef<IPropertyHandle> CurvePropertyHandle) const
{
	UCurveTable* InternalCurveTable = SoundWave->InternalCurves;

	UObject* CurrentCurveTable = nullptr;
	if (CurvePropertyHandle->GetValue(CurrentCurveTable) == FPropertyAccess::Success)
	{
		if (InternalCurveTable != nullptr && InternalCurveTable->HasAnyFlags(RF_Standalone) && CurrentCurveTable != InternalCurveTable)
		{
			return EVisibility::Visible;
		}
}

	return EVisibility::Collapsed;
}

FReply FSoundWaveDetails::HandleMakeInternalCurves(USoundWave* SoundWave)
{
	if (SoundWave->Curves != nullptr)
	{
		FScopedTransaction Transaction(LOCTEXT("MakeInternalCurve", "Copy Curve to Internal"));
		SoundWave->Modify();

		SoundWave->Curves = DuplicateObject<UCurveTable>(SoundWave->Curves, SoundWave, InternalCurveTableName);
		SoundWave->Curves->ClearFlags(RF_Public);
		SoundWave->Curves->SetFlags(SoundWave->Curves->GetFlags() | RF_Standalone | RF_Transactional);
		SoundWave->InternalCurves = SoundWave->Curves;
	}

	return FReply::Handled();
}

FReply FSoundWaveDetails::HandleUseInternalCurves(USoundWave* SoundWave, TSharedRef<IPropertyHandle> CurvePropertyHandle)
{
	UCurveTable* InternalCurveTable = SoundWave->InternalCurves;
	if (InternalCurveTable != nullptr)
	{
		CurvePropertyHandle->SetValue(InternalCurveTable);
	}
	
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
