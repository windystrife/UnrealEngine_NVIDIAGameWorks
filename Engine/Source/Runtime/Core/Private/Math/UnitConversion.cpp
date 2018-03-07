// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Math/UnitConversion.h"
#include "Internationalization/Internationalization.h"
#include "Misc/ExpressionParserTypes.h"
#include "Misc/ExpressionParser.h"
#include "Math/BasicMathExpressionEvaluator.h"

#define LOCTEXT_NAMESPACE "UnitConversion"

/** Structure used to match units when parsing */
struct FParseCandidate
{
	const TCHAR* String;
	EUnit Unit;
};

FParseCandidate ParseCandidates[] = {
	
	{ TEXT("Micrometers"),			EUnit::Micrometers },			{ TEXT("um"),		EUnit::Micrometers }, 			{ TEXT("\u00B5m"),	EUnit::Micrometers },
	{ TEXT("Millimeters"),			EUnit::Millimeters },			{ TEXT("mm"),		EUnit::Millimeters },
	{ TEXT("Centimeters"),			EUnit::Centimeters },			{ TEXT("cm"),		EUnit::Centimeters },
	{ TEXT("Meters"),				EUnit::Meters },				{ TEXT("m"),		EUnit::Meters },
	{ TEXT("Kilometers"),			EUnit::Kilometers },			{ TEXT("km"),		EUnit::Kilometers },
	{ TEXT("Inches"),				EUnit::Inches },				{ TEXT("in"),		EUnit::Inches },
	{ TEXT("Feet"),					EUnit::Feet },					{ TEXT("ft"),		EUnit::Feet },
	{ TEXT("Yards"),				EUnit::Yards },					{ TEXT("yd"),		EUnit::Yards },
	{ TEXT("Miles"),				EUnit::Miles },					{ TEXT("mi"),		EUnit::Miles },
	{ TEXT("Lightyears"),			EUnit::Lightyears },			{ TEXT("ly"),		EUnit::Lightyears },

	{ TEXT("Degrees"),				EUnit::Degrees },				{ TEXT("deg"),		EUnit::Degrees },				{ TEXT("\u00B0"),	EUnit::Degrees },
	{ TEXT("Radians"),				EUnit::Radians },				{ TEXT("rad"),		EUnit::Radians },
		
	{ TEXT("MetersPerSecond"),		EUnit::MetersPerSecond },		{ TEXT("m/s"),		EUnit::MetersPerSecond },
	{ TEXT("KilometersPerHour"),	EUnit::KilometersPerHour },		{ TEXT("km/h"),		EUnit::KilometersPerHour },		{ TEXT("kmph"),		EUnit::KilometersPerHour },
	{ TEXT("MilesPerHour"),			EUnit::MilesPerHour },			{ TEXT("mi/h"),		EUnit::MilesPerHour },			{ TEXT("mph"),		EUnit::MilesPerHour },
		
	{ TEXT("Celsius"),				EUnit::Celsius },				{ TEXT("C"),		EUnit::Celsius },				{ TEXT("degC"),		EUnit::Celsius },			{ TEXT("\u00B0C"),		EUnit::Celsius },
	{ TEXT("Farenheit"),			EUnit::Farenheit },				{ TEXT("F"),		EUnit::Farenheit },				{ TEXT("degF"),		EUnit::Farenheit },			{ TEXT("\u00B0F"),		EUnit::Farenheit },
	{ TEXT("Kelvin"),				EUnit::Kelvin },				{ TEXT("K"),		EUnit::Kelvin },
					
	{ TEXT("Micrograms"),			EUnit::Micrograms },			{ TEXT("ug"),		EUnit::Micrograms }, 			{ TEXT("\u00B5g"),		EUnit::Micrograms },
	{ TEXT("Milligrams"),			EUnit::Milligrams },			{ TEXT("mg"),		EUnit::Milligrams },
	{ TEXT("Grams"),				EUnit::Grams },					{ TEXT("g"),		EUnit::Grams },
	{ TEXT("Kilograms"),			EUnit::Kilograms },				{ TEXT("kg"),		EUnit::Kilograms },
	{ TEXT("MetricTons"),			EUnit::MetricTons },			{ TEXT("t"),		EUnit::MetricTons },
	{ TEXT("Ounces"),				EUnit::Ounces },				{ TEXT("oz"),		EUnit::Ounces },
	{ TEXT("Pounds"),				EUnit::Pounds },				{ TEXT("lb"),		EUnit::Pounds },
	{ TEXT("Stones"),				EUnit::Stones },				{ TEXT("st"),		EUnit::Stones },

	{ TEXT("Newtons"),				EUnit::Newtons },				{ TEXT("N"),		EUnit::Newtons },
	{ TEXT("PoundsForce"),			EUnit::PoundsForce },			{ TEXT("lbf"),		EUnit::PoundsForce },
	{ TEXT("KilogramsForce"),		EUnit::KilogramsForce },		{ TEXT("kgf"),		EUnit::KilogramsForce },

	{ TEXT("Hertz"),				EUnit::Hertz },					{ TEXT("Hz"),		EUnit::Hertz },
	{ TEXT("Kilohertz"),			EUnit::Kilohertz },				{ TEXT("KHz"),		EUnit::Kilohertz },
	{ TEXT("Megahertz"),			EUnit::Megahertz },				{ TEXT("MHz"),		EUnit::Megahertz },
	{ TEXT("Gigahertz"),			EUnit::Gigahertz },				{ TEXT("GHz"),		EUnit::Gigahertz },
	{ TEXT("RevolutionsPerMinute"),	EUnit::RevolutionsPerMinute },	{ TEXT("rpm"),		EUnit::RevolutionsPerMinute },

	{ TEXT("Bytes"),				EUnit::Bytes },					{ TEXT("B"),		EUnit::Bytes },
	{ TEXT("Kilobytes"),			EUnit::Kilobytes },				{ TEXT("KB"),		EUnit::Kilobytes },
	{ TEXT("Megabytes"),			EUnit::Megabytes },				{ TEXT("MB"),		EUnit::Megabytes },
	{ TEXT("Gigabytes"),			EUnit::Gigabytes },				{ TEXT("GB"),		EUnit::Gigabytes },
	{ TEXT("Terabytes"),			EUnit::Terabytes },				{ TEXT("TB"),		EUnit::Terabytes },

	{ TEXT("Lumens"),				EUnit::Lumens },				{ TEXT("lm"),		EUnit::Lumens },

	{ TEXT("Milliseconds"),			EUnit::Milliseconds },			{ TEXT("ms"),		EUnit::Milliseconds },
	{ TEXT("Seconds"),				EUnit::Seconds },				{ TEXT("s"),		EUnit::Seconds },
	{ TEXT("Minutes"),				EUnit::Minutes },				{ TEXT("min"),		EUnit::Minutes },
	{ TEXT("Hours"),				EUnit::Hours },					{ TEXT("hrs"),		EUnit::Hours },
	{ TEXT("Days"),					EUnit::Days },					{ TEXT("dy"),		EUnit::Days },
	{ TEXT("Months"),				EUnit::Months },				{ TEXT("mth"),		EUnit::Months },
	{ TEXT("Years"),				EUnit::Years },					{ TEXT("yr"),		EUnit::Years },

	{ TEXT("ppi"),					EUnit::PixelsPerInch },			{ TEXT("dpi"),		EUnit::PixelsPerInch },

	{ TEXT("Percent"),				EUnit::Percentage },			{ TEXT("%"),	EUnit::Percentage },

	{ TEXT("times"),				EUnit::Multiplier },			{ TEXT("x"),	EUnit::Multiplier },			{ TEXT("multiplier"),		EUnit::Multiplier },
};

/** Static array of display strings that directly map to EUnit enumerations */
const TCHAR* const DisplayStrings[] = {
	TEXT("\u00B5m"),			TEXT("mm"),					TEXT("cm"),					TEXT("m"),					TEXT("km"),
	TEXT("in"),					TEXT("ft"),					TEXT("yd"),					TEXT("mi"),
	TEXT("ly"),

	TEXT("\u00B0"), TEXT("rad"),

	TEXT("m/s"), TEXT("km/h"), TEXT("mi/h"),

	TEXT("\u00B0C"), TEXT("\u00B0F"), TEXT("K"),

	TEXT("\u00B5g"), TEXT("mg"), TEXT("g"), TEXT("kg"), TEXT("t"),
	TEXT("oz"), TEXT("lb"), TEXT("st"),

	TEXT("N"), TEXT("lbf"), TEXT("kgf"),

	TEXT("Hz"), TEXT("KHz"), TEXT("MHz"), TEXT("GHz"), TEXT("rpm"),

	TEXT("B"), TEXT("KB"), TEXT("MB"), TEXT("GB"), TEXT("TB"),

	TEXT("lm"),

	TEXT("ms"), TEXT("s"), TEXT("min"), TEXT("hr"), TEXT("dy"), TEXT("mth"), TEXT("yr"),

	TEXT("ppi"),

	TEXT("%"),

	TEXT("x"),
};

const EUnitType UnitTypes[] = {
	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,
	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,	EUnitType::Distance,
	EUnitType::Distance,

	EUnitType::Angle,		EUnitType::Angle,

	EUnitType::Speed,		EUnitType::Speed, 		EUnitType::Speed,

	EUnitType::Temperature,	EUnitType::Temperature,	EUnitType::Temperature,

	EUnitType::Mass,		EUnitType::Mass,		EUnitType::Mass,		EUnitType::Mass,		EUnitType::Mass,
	EUnitType::Mass,		EUnitType::Mass,		EUnitType::Mass,

	EUnitType::Force,		EUnitType::Force,		EUnitType::Force,

	EUnitType::Frequency,	EUnitType::Frequency,	EUnitType::Frequency,	EUnitType::Frequency,	EUnitType::Frequency,

	EUnitType::DataSize,	EUnitType::DataSize,	EUnitType::DataSize,	EUnitType::DataSize,	EUnitType::DataSize,

	EUnitType::LuminousFlux,

	EUnitType::Time,		EUnitType::Time,		EUnitType::Time,		EUnitType::Time,		EUnitType::Time,		EUnitType::Time,		EUnitType::Time,

	EUnitType::PixelDensity,

	EUnitType::Multipliers,

	EUnitType::Arbitrary,
};



DEFINE_EXPRESSION_NODE_TYPE(FNumericUnit<double>, 0x3C138BC9, 0x71314F0B, 0xBB469BF7, 0xED47D147)

struct FUnitExpressionParser
{
	FUnitExpressionParser(EUnit InDefaultUnit)
	{
		using namespace ExpressionParser;

		TokenDefinitions.IgnoreWhitespace();
			
		// Defined in order of importance
		TokenDefinitions.DefineToken(&ConsumeSymbol<FPlusEquals>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FMinusEquals>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FStarEquals>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FForwardSlashEquals>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FPlus>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FMinus>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FStar>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FForwardSlash>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FSubExpressionStart>);
		TokenDefinitions.DefineToken(&ConsumeSymbol<FSubExpressionEnd>);

		TokenDefinitions.DefineToken([&](FExpressionTokenConsumer& Consumer){ return ConsumeNumberWithUnits(Consumer); });

		Grammar.DefineGrouping<FSubExpressionStart, FSubExpressionEnd>();
		
		Grammar.DefinePreUnaryOperator<FPlus>();
		Grammar.DefinePreUnaryOperator<FMinus>();

		Grammar.DefineBinaryOperator<FPlus>(5);
		Grammar.DefineBinaryOperator<FMinus>(5);
		Grammar.DefineBinaryOperator<FStar>(4);
		Grammar.DefineBinaryOperator<FForwardSlash>(4);

		// Unary operators for numeric units
		JumpTable.MapPreUnary<FPlus>(	[](const FNumericUnit<double>& N) {	return FNumericUnit<double>(N.Value, N.Units);		});
		JumpTable.MapPreUnary<FMinus>(	[](const FNumericUnit<double>& N) {	return FNumericUnit<double>(-N.Value, N.Units);		});

		/** Addition for numeric units */
		JumpTable.MapBinary<FPlus>([InDefaultUnit](const FNumericUnit<double>& A, const FNumericUnit<double>& B) -> FExpressionResult {

			// Have to ensure we're adding the correct units here

			EUnit UnitsLHS = A.Units, UnitsRHS = B.Units;

			if (UnitsLHS == EUnit::Unspecified && UnitsRHS != EUnit::Unspecified)
			{
				UnitsLHS = InDefaultUnit;
			}
			else if (UnitsLHS != EUnit::Unspecified && UnitsRHS == EUnit::Unspecified)
			{
				UnitsRHS = InDefaultUnit;
			}

			if (FUnitConversion::AreUnitsCompatible(UnitsLHS, UnitsRHS))
			{
				if (UnitsLHS != EUnit::Unspecified)
				{
					return MakeValue(FNumericUnit<double>(A.Value + FUnitConversion::Convert(B.Value, UnitsRHS, UnitsLHS), UnitsLHS));
				}
				else
				{
					return MakeValue(FNumericUnit<double>(FUnitConversion::Convert(A.Value, UnitsLHS, UnitsRHS) + B.Value, UnitsRHS));
				}
			}

			FFormatOrderedArguments Args;
			Args.Add(FText::FromString(FUnitConversion::GetUnitDisplayString(B.Units)));
			Args.Add(FText::FromString(FUnitConversion::GetUnitDisplayString(A.Units)));
			return MakeError(FText::Format(LOCTEXT("CannotAddErr", "Cannot add {0} to {1}"), Args));
		});

		/** Subtraction for numeric units */
		JumpTable.MapBinary<FMinus>([InDefaultUnit](const FNumericUnit<double>& A, const FNumericUnit<double>& B) -> FExpressionResult {
			
			// Have to ensure we're adding the correct units here

			EUnit UnitsLHS = A.Units, UnitsRHS = B.Units;

			if (UnitsLHS == EUnit::Unspecified && UnitsRHS != EUnit::Unspecified)
			{
				UnitsLHS = InDefaultUnit;
			}
			else if (UnitsLHS != EUnit::Unspecified && UnitsRHS == EUnit::Unspecified)
			{
				UnitsRHS = InDefaultUnit;
			}

			if (FUnitConversion::AreUnitsCompatible(UnitsLHS, UnitsRHS))
			{
				if (UnitsLHS != EUnit::Unspecified)
				{
					return MakeValue(FNumericUnit<double>(A.Value - FUnitConversion::Convert(B.Value, UnitsRHS, UnitsLHS), UnitsLHS));
				}
				else
				{
					return MakeValue(FNumericUnit<double>(FUnitConversion::Convert(A.Value, UnitsLHS, UnitsRHS) - B.Value, UnitsRHS));
				}
			}

			FFormatOrderedArguments Args;
			Args.Add(FText::FromString(FUnitConversion::GetUnitDisplayString(B.Units)));
			Args.Add(FText::FromString(FUnitConversion::GetUnitDisplayString(A.Units)));
			return MakeError(FText::Format(LOCTEXT("CannotSubtractErr", "Cannot subtract {1} from {0}"), Args));
		});

		/** Multiplication */
		JumpTable.MapBinary<FStar>([](const FNumericUnit<double>& A, const FNumericUnit<double>& B) -> FExpressionResult {
			if (A.Units != EUnit::Unspecified && B.Units != EUnit::Unspecified)
			{
				return MakeError(LOCTEXT("InvalidMultiplication", "Cannot multiply by numbers with units"));
			}
			return MakeValue(FNumericUnit<double>(B.Value*A.Value, A.Units == EUnit::Unspecified ? B.Units : A.Units));
		});

		/** Division */
		JumpTable.MapBinary<FForwardSlash>([](const FNumericUnit<double>& A, const FNumericUnit<double>& B) -> FExpressionResult {
			if (B.Units != EUnit::Unspecified)
			{
				return MakeError(LOCTEXT("InvalidDivision", "Cannot divide by numbers with units"));
			}
			else if (B.Value == 0)
			{
				return MakeError(LOCTEXT("DivideByZero", "DivideByZero"));	
			}
			return MakeValue(FNumericUnit<double>(A.Value/B.Value, A.Units));
		});
	}

	/** Consume a number from the stream, optionally including units */
	TOptional<FExpressionError> ConsumeNumberWithUnits(FExpressionTokenConsumer& Consumer)
	{
		auto& Stream = Consumer.GetStream();

		if (!FChar::IsDigit(Stream.PeekChar()))
		{
			return TOptional<FExpressionError>();
		}

		TOptional<FStringToken> NumberToken = ExpressionParser::ParseNumber(Stream);
		
		if (NumberToken.IsSet())
		{
			// Skip over whitespace
			Stream.ParseToken([](TCHAR InC){ return FChar::IsWhitespace(InC) ? EParseState::Continue : EParseState::StopBefore; }, &NumberToken.GetValue());

			TOptional<EUnit> Unit;
			TOptional<FStringToken> UnitToken;
			for (int32 Index = 0; Index < ARRAY_COUNT(ParseCandidates); ++Index)
			{
				UnitToken = Stream.ParseTokenIgnoreCase(ParseCandidates[Index].String, &NumberToken.GetValue());
				if (UnitToken.IsSet())
				{
					Unit = ParseCandidates[Index].Unit;
					break;
				}
			}

			const double Value = FCString::Atod(*NumberToken.GetValue().GetString());
			if (Unit.IsSet())
			{
				Consumer.Add(NumberToken.GetValue(), FNumericUnit<double>(Value, Unit.GetValue()));
			}
			else
			{
				Consumer.Add(NumberToken.GetValue(), FNumericUnit<double>(Value));
			}
		}

		return TOptional<FExpressionError>();
	}

	TValueOrError<FNumericUnit<double>, FExpressionError> Evaluate(const TCHAR* InExpression, const FNumericUnit<double>& InExistingValue) const
	{
		using namespace ExpressionParser;

		TValueOrError<TArray<FExpressionToken>, FExpressionError> LexResult = ExpressionParser::Lex(InExpression, TokenDefinitions);
		if (!LexResult.IsValid())
		{
			return MakeError(LexResult.StealError());
		}

		// Handle the += and -= tokens.
		TArray<FExpressionToken> Tokens = LexResult.StealValue();
		if (Tokens.Num())
		{
			FStringToken Context = Tokens[0].Context;
			const FExpressionNode& FirstNode = Tokens[0].Node;
			bool WasOpAssign = true;

			if (FirstNode.Cast<FPlusEquals>())
			{			
				Tokens.Insert(FExpressionToken(Context, FPlus()), 0);
			}
			else if (FirstNode.Cast<FMinusEquals>())
			{
				Tokens.Insert(FExpressionToken(Context, FMinus()), 0);
			}
			else if (FirstNode.Cast<FStarEquals>())
			{
				Tokens.Insert(FExpressionToken(Context, FStar()), 0);
			}
			else if (FirstNode.Cast<FForwardSlashEquals>())
			{
				Tokens.Insert(FExpressionToken(Context, FForwardSlash()), 0);
			}
			else
			{
				WasOpAssign = false;
			}

			if (WasOpAssign)
			{
				Tokens.Insert(FExpressionToken(Context, InExistingValue), 0);
				Tokens.RemoveAt(2, 1, false);
			}
		}

		TValueOrError<TArray<FCompiledToken>, FExpressionError> CompilationResult = ExpressionParser::Compile(MoveTemp(Tokens), Grammar);
		if (!CompilationResult.IsValid())
		{
			return MakeError(CompilationResult.StealError());
		}

		TOperatorEvaluationEnvironment<> Env(JumpTable, nullptr);
		TValueOrError<FExpressionNode, FExpressionError> Result = ExpressionParser::Evaluate(CompilationResult.GetValue(), Env);
		if (!Result.IsValid())
		{
			return MakeError(Result.GetError());
		}

		auto& Node = Result.GetValue();
		if (const auto* Numeric = Node.Cast<double>())
		{
			return MakeValue(FNumericUnit<double>(*Numeric, EUnit::Unspecified));
		}
		else if (const auto* NumericUnit = Node.Cast<FNumericUnit<double>>())
		{
			return MakeValue(*NumericUnit);
		}
		else
		{
			return MakeError(LOCTEXT("UnrecognizedResult", "Unrecognized result returned from expression"));
		}
	}
	
private:
	FTokenDefinitions TokenDefinitions;
	FExpressionGrammar Grammar;
	FOperatorJumpTable JumpTable;
};

FUnitSettings::FUnitSettings()
	: bGlobalUnitDisplay(true)
{
	DisplayUnits[(uint8)EUnitType::Distance].Add(EUnit::Centimeters);
	DisplayUnits[(uint8)EUnitType::Angle].Add(EUnit::Degrees);
	DisplayUnits[(uint8)EUnitType::Speed].Add(EUnit::MetersPerSecond);
	DisplayUnits[(uint8)EUnitType::Temperature].Add(EUnit::Celsius);
	DisplayUnits[(uint8)EUnitType::Mass].Add(EUnit::Kilograms);
	DisplayUnits[(uint8)EUnitType::Force].Add(EUnit::Newtons);
	DisplayUnits[(uint8)EUnitType::Frequency].Add(EUnit::Hertz);
	DisplayUnits[(uint8)EUnitType::DataSize].Add(EUnit::Megabytes);
	DisplayUnits[(uint8)EUnitType::LuminousFlux].Add(EUnit::Lumens);
	DisplayUnits[(uint8)EUnitType::Time].Add(EUnit::Seconds);
}

bool FUnitSettings::ShouldDisplayUnits() const
{
	return bGlobalUnitDisplay;
}

void FUnitSettings::SetShouldDisplayUnits(bool bInGlobalUnitDisplay)
{
	bGlobalUnitDisplay = bInGlobalUnitDisplay;
	SettingChangedEvent.Broadcast();
}

const TArray<EUnit>& FUnitSettings::GetDisplayUnits(EUnitType InType) const
{
	return DisplayUnits[(uint8)InType];
}

void FUnitSettings::SetDisplayUnits(EUnitType InType, EUnit Unit)
{
	if (InType != EUnitType::NumberOf)
	{
		DisplayUnits[(uint8)InType].Empty();
		if (FUnitConversion::IsUnitOfType(Unit, InType))
		{
			DisplayUnits[(uint8)InType].Add(Unit);
		}

		SettingChangedEvent.Broadcast();
	}	
}

void FUnitSettings::SetDisplayUnits(EUnitType InType, const TArray<EUnit>& Units)
{
	if (InType != EUnitType::NumberOf)
	{
		DisplayUnits[(uint8)InType].Reset();
		for (EUnit Unit : Units)
		{
			if (FUnitConversion::IsUnitOfType(Unit, InType))
			{
				DisplayUnits[(uint8)InType].Add(Unit);
			}
		}

		SettingChangedEvent.Broadcast();
	}
}

/** Get the global settings for unit conversion/display */
FUnitSettings& FUnitConversion::Settings()
{
	static TUniquePtr<FUnitSettings> Settings(new FUnitSettings);
	return *Settings;
}

/** Check whether it is possible to convert a number between the two specified units */
bool FUnitConversion::AreUnitsCompatible(EUnit From, EUnit To)
{
	return From == EUnit::Unspecified || To == EUnit::Unspecified || GetUnitType(From) == GetUnitType(To);
}

/** Check whether a unit is of the specified type */
bool FUnitConversion::IsUnitOfType(EUnit Unit, EUnitType Type)
{
	return Unit != EUnit::Unspecified && GetUnitType(Unit) == Type;
}

/** Get the type of the specified unit */
EUnitType FUnitConversion::GetUnitType(EUnit InUnit)
{
	if (ensure(InUnit != EUnit::Unspecified))
	{
		return UnitTypes[(uint8)InUnit];
	}
	return EUnitType::NumberOf;
}

/** Get the unit abbreviation the specified unit type */
const TCHAR* FUnitConversion::GetUnitDisplayString(EUnit Unit)
{
	static_assert(ARRAY_COUNT(UnitTypes) == (uint8)EUnit::Unspecified, "Type array does not match size of unit enum");
	static_assert(ARRAY_COUNT(DisplayStrings) == (uint8)EUnit::Unspecified, "Display String array does not match size of unit enum");
	
	if (Unit != EUnit::Unspecified)
	{
		return DisplayStrings[(uint8)Unit];
	}
	return nullptr;
}

/** Helper function to find a unit from a string (name or abbreviation) */
TOptional<EUnit> FUnitConversion::UnitFromString(const TCHAR* UnitString)
{
	if (!UnitString || *UnitString == '\0')
	{
		return TOptional<EUnit>();
	}

	for (int32 Index = 0; Index < ARRAY_COUNT(ParseCandidates); ++Index)
	{
		if (FCString::Stricmp(UnitString, ParseCandidates[Index].String) == 0)
		{
			return ParseCandidates[Index].Unit;
		}
	}

	return TOptional<EUnit>();
}

namespace UnitConversion
{

	double DistanceUnificationFactor(EUnit From)
	{
		// Convert to meters
		double Factor = 1;
		switch (From)
		{
			case EUnit::Micrometers:		return 0.000001;
			case EUnit::Millimeters:		return 0.001;
			case EUnit::Centimeters:		return 0.01;
			case EUnit::Kilometers:			return 1000;

			case EUnit::Lightyears:			return 9.4605284e15;

			case EUnit::Miles:				Factor *= 1760;				// fallthrough
			case EUnit::Yards:				Factor *= 3;				// fallthrough
			case EUnit::Feet:				Factor *= 12;				// fallthrough
			case EUnit::Inches:				Factor /= 39.3700787;		// fallthrough
			default: 						return Factor;				// return
		}
	}

	double AngleUnificationFactor(EUnit From)
	{
		// Convert to degrees
		switch (From)
		{
			case EUnit::Radians:			return (180 / PI);
			default: 						return 1;
		}
	}

	double SpeedUnificationFactor(EUnit From)
	{
		// Convert to km/h
		switch (From)
		{
			case EUnit::MetersPerSecond:	return 3.6;
			case EUnit::MilesPerHour:		return DistanceUnificationFactor(EUnit::Miles) / 1000;
			default: 						return 1;
		}
	}

	double MassUnificationFactor(EUnit From)
	{
		double Factor = 1;
		// Convert to grams
		switch (From)
		{
			case EUnit::Micrograms:			return 0.000001;
			case EUnit::Milligrams:			return 0.001;
			case EUnit::Kilograms:			return 1000;
			case EUnit::MetricTons:			return 1000000;

			case EUnit::Stones:				Factor *= 14;		// fallthrough
			case EUnit::Pounds:				Factor *= 16;		// fallthrough
			case EUnit::Ounces:				Factor *= 28.3495;	// fallthrough
			default: 						return Factor;		// return
		}
	}

	double ForceUnificationFactor(EUnit From)
	{
		// Convert to Newtons
		switch (From)
		{
			case EUnit::PoundsForce:		return 4.44822162;
			case EUnit::KilogramsForce:		return 9.80665;
			default: 						return 1;
		}
	}

	double FrequencyUnificationFactor(EUnit From)
	{
		// Convert to KHz
		switch (From)
		{
			case EUnit::Hertz:				return 0.001;
			case EUnit::Megahertz:			return 1000;
			case EUnit::Gigahertz:			return 1000000;

			case EUnit::RevolutionsPerMinute:	return 0.001/60;

			default: 						return 1;
		}
	}

	double DataSizeUnificationFactor(EUnit From)
	{
		// Convert to MB
		switch (From)
		{
			case EUnit::Bytes:				return 1.0/(1024*1024);
			case EUnit::Kilobytes:			return 1.0/1024;
			case EUnit::Gigabytes:			return 1024;
			case EUnit::Terabytes:			return 1024*1024;

			default: 						return 1;
		}
	}

	double TimeUnificationFactor(EUnit From)
	{
		// Convert to hours
		double Factor = 1;
		switch (From)
		{
			case EUnit::Months:				return (365.242 * 24) / 12;

			case EUnit::Years:				Factor *= 365.242;	// fallthrough
			case EUnit::Days:				Factor *= 24;		// fallthrough
											return Factor;

			case EUnit::Milliseconds:		Factor /= 1000;		// fallthrough
			case EUnit::Seconds:			Factor /= 60;		// fallthrough
			case EUnit::Minutes:			Factor /= 60;		// fallthrough
											return Factor;

			default: 						return 1;
		}
	}

	double MultiplierUnificationFactor(EUnit From)
	{
		switch (From)
		{
			case EUnit::Percentage:			return 0.01;
			case EUnit::Multiplier:						// fallthrough
			default: 						return 1.0;
		}
	}

	TValueOrError<FNumericUnit<double>, FText> TryParseExpression(const TCHAR* InExpression, EUnit From, const FNumericUnit<double>& InExistingValue)
	{
		const FUnitExpressionParser Parser(From);
		auto Result = Parser.Evaluate(InExpression, InExistingValue);
		if (Result.IsValid())
		{
			if (Result.GetValue().Units == EUnit::Unspecified)
			{
				return MakeValue(FNumericUnit<double>(Result.GetValue().Value, From));
			}
			else
			{
				return MakeValue(Result.GetValue());
			}
		}
		else
		{
			return MakeError(Result.GetError().Text);
		}
	}

	TOptional<const TArray<FQuantizationInfo>*> GetQuantizationBounds(EUnit Unit)
	{
		struct FStaticBounds
		{
			TArray<FQuantizationInfo> MetricDistance;
			TArray<FQuantizationInfo> ImperialDistance;

			TArray<FQuantizationInfo> MetricMass;
			TArray<FQuantizationInfo> ImperialMass;

			TArray<FQuantizationInfo> Frequency;
			TArray<FQuantizationInfo> DataSize;

			TArray<FQuantizationInfo> Time;

			FStaticBounds()
			{
				MetricDistance.Emplace(EUnit::Micrometers,	1000);
				MetricDistance.Emplace(EUnit::Millimeters,	10);
				MetricDistance.Emplace(EUnit::Centimeters,	100);
				MetricDistance.Emplace(EUnit::Meters,			1000);
				MetricDistance.Emplace(EUnit::Kilometers,		0);

				ImperialDistance.Emplace(EUnit::Inches,	12);
				ImperialDistance.Emplace(EUnit::Feet,		3);
				ImperialDistance.Emplace(EUnit::Yards,	1760);
				ImperialDistance.Emplace(EUnit::Miles,	0);

				MetricMass.Emplace(EUnit::Micrograms,	1000);
				MetricMass.Emplace(EUnit::Milligrams,	1000);
				MetricMass.Emplace(EUnit::Grams,		1000);
				MetricMass.Emplace(EUnit::Kilograms,	1000);
				MetricMass.Emplace(EUnit::MetricTons,	0);

				ImperialMass.Emplace(EUnit::Ounces,	16);
				ImperialMass.Emplace(EUnit::Pounds,	14);
				ImperialMass.Emplace(EUnit::Stones,	0);

				Frequency.Emplace(EUnit::Hertz,		1000);
				Frequency.Emplace(EUnit::Kilohertz,	1000);
				Frequency.Emplace(EUnit::Megahertz,	1000);
				Frequency.Emplace(EUnit::Gigahertz,	0);

				DataSize.Emplace(EUnit::Bytes,		1000);
				DataSize.Emplace(EUnit::Kilobytes,	1000);
				DataSize.Emplace(EUnit::Megabytes,	1000);
				DataSize.Emplace(EUnit::Gigabytes,	1000);
				DataSize.Emplace(EUnit::Terabytes,	0);

				Time.Emplace(EUnit::Milliseconds,		1000);
				Time.Emplace(EUnit::Seconds,			60);
				Time.Emplace(EUnit::Minutes,			60);
				Time.Emplace(EUnit::Hours,			24);
				Time.Emplace(EUnit::Days,				365.242f / 12);
				Time.Emplace(EUnit::Months,			12);
				Time.Emplace(EUnit::Years,			0);
			}
		};

		static FStaticBounds Bounds;

		switch (Unit)
		{
		case EUnit::Micrometers: case EUnit::Millimeters: case EUnit::Centimeters: case EUnit::Meters: case EUnit::Kilometers:
			return &Bounds.MetricDistance;

		case EUnit::Inches: case EUnit::Feet: case EUnit::Yards: case EUnit::Miles:
			return &Bounds.ImperialDistance;

		case EUnit::Micrograms: case EUnit::Milligrams: case EUnit::Grams: case EUnit::Kilograms: case EUnit::MetricTons:
			return &Bounds.MetricMass;

		case EUnit::Ounces: case EUnit::Pounds: case EUnit::Stones:
			return &Bounds.ImperialMass;

		case EUnit::Hertz: case EUnit::Kilohertz: case EUnit::Megahertz: case EUnit::Gigahertz: case EUnit::RevolutionsPerMinute:
			return &Bounds.Frequency;

		case EUnit::Bytes: case EUnit::Kilobytes: case EUnit::Megabytes: case EUnit::Gigabytes: case EUnit::Terabytes:
			return &Bounds.DataSize;

		case EUnit::Milliseconds: case EUnit::Seconds: case EUnit::Minutes: case EUnit::Hours: case EUnit::Days: case EUnit::Months: case EUnit::Years:
			return &Bounds.Time;

		default:
			return TOptional<const TArray<FQuantizationInfo>*>();
		}
	}

}	// namespace UnitConversion

#undef LOCTEXT_NAMESPACE
