// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetTextLibrary.h"
#include "Internationalization/TextFormatter.h"
#include "Internationalization/StringTableRegistry.h"

#define LOCTEXT_NAMESPACE "Kismet"

UKismetTextLibrary::UKismetTextLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }

FText UKismetTextLibrary::Conv_VectorToText(FVector InVec)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("X"), InVec.X);
	Args.Add(TEXT("Y"), InVec.Y);
	Args.Add(TEXT("Z"), InVec.Z);

	return FText::Format(NSLOCTEXT("Core", "Vector3", "X={X} Y={Y} Z={Z}"), Args);
}

FText UKismetTextLibrary::Conv_Vector2dToText(FVector2D InVec)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("X"), InVec.X);
	Args.Add(TEXT("Y"), InVec.Y);

	return FText::Format(NSLOCTEXT("Core", "Vector2", "X={X} Y={Y}"), Args);
}

FText UKismetTextLibrary::Conv_RotatorToText(FRotator InRot)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("P"), InRot.Pitch);
	Args.Add(TEXT("Y"), InRot.Yaw);
	Args.Add(TEXT("R"), InRot.Roll);

	return FText::Format(NSLOCTEXT("Core", "Rotator", "P={P} Y={Y} R={R}"), Args);
}

FText UKismetTextLibrary::Conv_TransformToText(const FTransform& InTrans)
{
	const FVector T(InTrans.GetTranslation());
	const FRotator R(InTrans.Rotator());
	const FVector S(InTrans.GetScale3D());

	FFormatNamedArguments Args;
	Args.Add(TEXT("T"), Conv_VectorToText(T));
	Args.Add(TEXT("R"), Conv_RotatorToText(R));
	Args.Add(TEXT("S"), Conv_VectorToText(S));

	return FText::Format(NSLOCTEXT("Core", "Transform", "Translation: {T} Rotation: {R} Scale: {S}"), Args);
}

FText UKismetTextLibrary::Conv_ObjectToText(class UObject* InObj)
{
	if (InObj)
	{
		return FText::AsCultureInvariant(InObj->GetName());
	}

	return GNone;
}

FText UKismetTextLibrary::Conv_ColorToText(FLinearColor InColor)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("R"), InColor.R);
	Args.Add(TEXT("G"), InColor.G);
	Args.Add(TEXT("B"), InColor.B);
	Args.Add(TEXT("A"), InColor.A);

	return FText::Format(NSLOCTEXT("Core", "LinearColor", "R={R} G={G} B={B} A={A}"), Args);
}

FString UKismetTextLibrary::Conv_TextToString(const FText& InText)
{
	return InText.ToString();
}

FText UKismetTextLibrary::Conv_StringToText(const FString& InString)
{	
	return FText::AsCultureInvariant(InString);
}

FText UKismetTextLibrary::Conv_NameToText(FName InName)
{
	return FText::AsCultureInvariant(InName.ToString());
}

bool UKismetTextLibrary::TextIsEmpty(const FText& InText)
{
	return InText.IsEmpty();
}

bool UKismetTextLibrary::TextIsTransient(const FText& InText)
{
	return InText.IsTransient();
}

bool UKismetTextLibrary::TextIsCultureInvariant(const FText& InText)
{
	return InText.IsCultureInvariant();
}

FText UKismetTextLibrary::TextToLower(const FText& InText)
{
	return InText.ToLower();
}

FText UKismetTextLibrary::TextToUpper(const FText& InText)
{
	return InText.ToUpper();
}

FText UKismetTextLibrary::TextTrimPreceding(const FText& InText)
{
	return FText::TrimPreceding(InText);
}

FText UKismetTextLibrary::TextTrimTrailing(const FText& InText)
{
	return FText::TrimTrailing(InText);
}

FText UKismetTextLibrary::TextTrimPrecedingAndTrailing(const FText& InText)
{
	return FText::TrimPrecedingAndTrailing(InText);
}

FText UKismetTextLibrary::GetEmptyText()
{
	return FText::GetEmpty();
}

bool UKismetTextLibrary::FindTextInLocalizationTable(const FString& Namespace, const FString& Key, FText& OutText)
{
	return FText::FindText(Namespace, Key, OutText);
}

bool UKismetTextLibrary::EqualEqual_IgnoreCase_TextText(const FText& A, const FText& B)
{
	return A.EqualToCaseIgnored( B );
}

bool UKismetTextLibrary::EqualEqual_TextText(const FText& A, const FText& B)
{
	return A.EqualTo( B );
}

bool UKismetTextLibrary::NotEqual_IgnoreCase_TextText(const FText& A, const FText& B)
{
	return !A.EqualToCaseIgnored( B );
}

bool UKismetTextLibrary::NotEqual_TextText(const FText& A, const FText& B)
{
	return !A.EqualTo( B );
}

FText UKismetTextLibrary::Conv_BoolToText(bool InBool)
{
	return InBool ? LOCTEXT("True", "true") : LOCTEXT("False", "false");
}

FText UKismetTextLibrary::Conv_ByteToText(uint8 Value)
{
	return FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping());
}

FText UKismetTextLibrary::Conv_IntToText(int32 Value, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/)
{
	// Only update the values that need to be changed from the default FNumberFormattingOptions, 
	// as this lets us use the default formatter if possible (which is a performance win!)
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;

	return FText::AsNumber(Value, &NumberFormatOptions);
}

FText UKismetTextLibrary::Conv_FloatToText(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;

	return FText::AsNumber(Value, &NumberFormatOptions);
}

FText UKismetTextLibrary::AsCurrencyBase(int32 BaseValue, const FString& CurrencyCode)
{
	return FText::AsCurrencyBase(BaseValue, CurrencyCode);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
// FIXME: we need to deprecate this kismet api too
FText UKismetTextLibrary::AsCurrency_Integer(int32 Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/, const FString& CurrencyCode)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;
	return FText::AsCurrency(Value, CurrencyCode, &NumberFormatOptions);
}
FText UKismetTextLibrary::AsCurrency_Float(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/, const FString& CurrencyCode)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;
	return FText::AsCurrency(Value, CurrencyCode, &NumberFormatOptions);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FText UKismetTextLibrary::AsPercent_Float(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;

	return FText::AsPercent(Value, &NumberFormatOptions);
}

FText UKismetTextLibrary::AsDate_DateTime(const FDateTime& InDateTime)
{
	return FText::AsDate(InDateTime, EDateTimeStyle::Default, FText::GetInvariantTimeZone());
}

FText UKismetTextLibrary::AsTimeZoneDate_DateTime(const FDateTime& InDateTime, const FString& InTimeZone)
{
	return FText::AsDate(InDateTime, EDateTimeStyle::Default, InTimeZone);
}

FText UKismetTextLibrary::AsDateTime_DateTime(const FDateTime& InDateTime)
{
	return FText::AsDateTime(InDateTime, EDateTimeStyle::Default, EDateTimeStyle::Default, FText::GetInvariantTimeZone());
}

FText UKismetTextLibrary::AsTimeZoneDateTime_DateTime(const FDateTime& InDateTime, const FString& InTimeZone)
{
	return FText::AsDateTime(InDateTime, EDateTimeStyle::Default, EDateTimeStyle::Default, InTimeZone);
}

FText UKismetTextLibrary::AsTime_DateTime(const FDateTime& InDateTime)
{
	return FText::AsTime(InDateTime, EDateTimeStyle::Default, FText::GetInvariantTimeZone());
}

FText UKismetTextLibrary::AsTimeZoneTime_DateTime(const FDateTime& InDateTime, const FString& InTimeZone)
{
	return FText::AsTime(InDateTime, EDateTimeStyle::Default, InTimeZone);
}

FText UKismetTextLibrary::AsTimespan_Timespan(const FTimespan& InTimespan)
{
	return FText::AsTimespan(InTimespan);
}

FText UKismetTextLibrary::Format(FText InPattern, TArray<FFormatArgumentData> InArgs)
{
	return FTextFormatter::Format(MoveTemp(InPattern), MoveTemp(InArgs), false, false);
}

bool UKismetTextLibrary::TextIsFromStringTable(const FText& Text)
{
	return Text.IsFromStringTable();
}

FText UKismetTextLibrary::TextFromStringTable(const FName TableId, const FString& Key)
{
	return FText::FromStringTable(TableId, Key);
}

bool UKismetTextLibrary::StringTableIdAndKeyFromText(FText Text, FName& OutTableId, FString& OutKey)
{
	return FStringTableRegistry::Get().FindTableIdAndKey(Text, OutTableId, OutKey);
}

#undef LOCTEXT_NAMESPACE
