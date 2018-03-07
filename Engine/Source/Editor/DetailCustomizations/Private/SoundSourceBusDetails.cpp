// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundSourceBusDetails.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Sound/SoundSourceBus.h"
#include "DetailLayoutBuilder.h"
#include "ScopedTransaction.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Engine/CurveTable.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FSoundSourceBusDetails"

TSharedRef<IDetailCustomization> FSoundSourceBusDetails::MakeInstance()
{
	return MakeShareable(new FSoundSourceBusDetails);
}

void FSoundSourceBusDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide details of sound base that aren't relevant for buses


}

#undef LOCTEXT_NAMESPACE
