// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Settings/EditorProjectSettings.h"
#include "UObject/UnrealType.h"

extern COREUOBJECT_API bool GBlueprintUseCompilationManager;

EUnit ConvertDefaultInputUnits(EDefaultLocationUnit In)
{
	typedef EDefaultLocationUnit EDefaultLocationUnit;

	switch(In)
	{
	case EDefaultLocationUnit::Micrometers:		return EUnit::Micrometers;
	case EDefaultLocationUnit::Millimeters:		return EUnit::Millimeters;
	case EDefaultLocationUnit::Centimeters:		return EUnit::Centimeters;
	case EDefaultLocationUnit::Meters:			return EUnit::Meters;
	case EDefaultLocationUnit::Kilometers:		return EUnit::Kilometers;
	case EDefaultLocationUnit::Inches:			return EUnit::Inches;
	case EDefaultLocationUnit::Feet:			return EUnit::Feet;
	case EDefaultLocationUnit::Yards:			return EUnit::Yards;
	case EDefaultLocationUnit::Miles:			return EUnit::Miles;
	default:									return EUnit::Centimeters;
	}
}

UEditorProjectAppearanceSettings::UEditorProjectAppearanceSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, bDisplayUnitsOnComponentTransforms(false)
	, UnitDisplay_DEPRECATED(EUnitDisplay::Invalid)
	, DefaultInputUnits_DEPRECATED(EDefaultLocationUnit::Invalid)
{
}

void UEditorProjectAppearanceSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	auto& Settings = FUnitConversion::Settings();
	if (Name == GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, DistanceUnits))
	{
		Settings.SetDisplayUnits(EUnitType::Distance, DistanceUnits);
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, MassUnits))
	{
		Settings.SetDisplayUnits(EUnitType::Mass, MassUnits);
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, TimeUnits))
	{
		Settings.SetDisplayUnits(EUnitType::Time, TimeUnits);
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, AngleUnits))
	{
		Settings.SetDisplayUnits(EUnitType::Angle, AngleUnits);
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, SpeedUnits))
	{
		Settings.SetDisplayUnits(EUnitType::Speed, SpeedUnits);
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, TemperatureUnits))
	{
		Settings.SetDisplayUnits(EUnitType::Temperature, TemperatureUnits);
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, ForceUnits))
	{
		Settings.SetDisplayUnits(EUnitType::Force, ForceUnits);
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, bDisplayUnits))
	{
		Settings.SetShouldDisplayUnits(bDisplayUnits);
	}

	DefaultInputUnits_DEPRECATED = EDefaultLocationUnit::Invalid;
	UnitDisplay_DEPRECATED = EUnitDisplay::Invalid;

	SaveConfig();
}

void SetupEnumMetaData(UClass* Class, const FName& MemberName, const TCHAR* Values)
{
	UArrayProperty* Array = Cast<UArrayProperty>(Class->FindPropertyByName(MemberName));
	if (Array && Array->Inner)
	{
		Array->Inner->SetMetaData(TEXT("ValidEnumValues"), Values);
	}
}

void UEditorProjectAppearanceSettings::PostInitProperties()
{
	Super::PostInitProperties();

	/** Setup the meta data for the array properties */
	SetupEnumMetaData(GetClass(), GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, DistanceUnits), TEXT("Micrometers, Millimeters, Centimeters, Meters, Kilometers, Inches, Feet, Yards, Miles"));
	SetupEnumMetaData(GetClass(), GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, MassUnits), TEXT("Micrograms, Milligrams, Grams, Kilograms, MetricTons,	Ounces, Pounds, Stones"));
	SetupEnumMetaData(GetClass(), GET_MEMBER_NAME_CHECKED(UEditorProjectAppearanceSettings, TimeUnits), TEXT("Milliseconds, Seconds, Minutes, Hours, Days, Months, Years"));

	if (UnitDisplay_DEPRECATED != EUnitDisplay::Invalid)
	{
		bDisplayUnits = UnitDisplay_DEPRECATED != EUnitDisplay::None;
	}

	if (DefaultInputUnits_DEPRECATED != EDefaultLocationUnit::Invalid)
	{
		DistanceUnits.Empty();
		DistanceUnits.Add(ConvertDefaultInputUnits(DefaultInputUnits_DEPRECATED));
	}

	auto& Settings = FUnitConversion::Settings();

	Settings.SetDisplayUnits(EUnitType::Distance, DistanceUnits);
	Settings.SetDisplayUnits(EUnitType::Mass, MassUnits);
	Settings.SetDisplayUnits(EUnitType::Time, TimeUnits);
	Settings.SetDisplayUnits(EUnitType::Angle, AngleUnits);
	Settings.SetDisplayUnits(EUnitType::Speed, SpeedUnits);
	Settings.SetDisplayUnits(EUnitType::Temperature, TemperatureUnits);
	Settings.SetDisplayUnits(EUnitType::Force, ForceUnits);

	Settings.SetShouldDisplayUnits(bDisplayUnits);
}


/* ULevelEditor2DSettings
*****************************************************************************/

ULevelEditor2DSettings::ULevelEditor2DSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SnapAxis(ELevelEditor2DAxis::Y)
{
	SnapLayers.Emplace(TEXT("Foreground"), 100.0f);
	SnapLayers.Emplace(TEXT("Default"), 0.0f);
	SnapLayers.Emplace(TEXT("Background"), -100.0f);
}

void ULevelEditor2DSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	// Sort the snap layers
	SnapLayers.Sort([](const FMode2DLayer& LHS, const FMode2DLayer& RHS){ return LHS.Depth > RHS.Depth; });

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/* UBlueprintEditorProjectSettings */

UBlueprintEditorProjectSettings::UBlueprintEditorProjectSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBlueprintEditorProjectSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName Name = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (Name == GET_MEMBER_NAME_CHECKED(UBlueprintEditorProjectSettings, bUseCompilationManager))
	{
		GBlueprintUseCompilationManager = bUseCompilationManager;
	}
}
