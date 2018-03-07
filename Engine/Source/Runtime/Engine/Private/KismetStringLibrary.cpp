// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetStringLibrary.h"

//////////////////////////////////////////////////////////////////////////
// UKismetStringLibrary

UKismetStringLibrary::UKismetStringLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UKismetStringLibrary::Concat_StrStr(const FString& A, const FString& B)
{
	// faster, preallocating method
	FString StringResult;
	StringResult.Empty(A.Len() + B.Len() + 1); // adding one for the string terminator
	StringResult += A;
	StringResult += B;

	return StringResult;
}

bool UKismetStringLibrary::EqualEqual_StriStri(const FString& A, const FString& B)
{
	return FCString::Stricmp(*A, *B) == 0;
}

bool UKismetStringLibrary::EqualEqual_StrStr(const FString& A, const FString& B)
{
	return FCString::Strcmp(*A, *B) == 0;
}

bool UKismetStringLibrary::NotEqual_StriStri(const FString& A, const FString& B)
{
	return FCString::Stricmp(*A, *B) != 0;
}

bool UKismetStringLibrary::NotEqual_StrStr(const FString& A, const FString& B)
{
	return FCString::Strcmp(*A, *B) != 0;
}

int32 UKismetStringLibrary::Len(const FString& S)
{
	return S.Len();
}

FString UKismetStringLibrary::Conv_FloatToString(float InFloat)
{
	return FString::SanitizeFloat(InFloat);
}

FString UKismetStringLibrary::Conv_IntToString(int32 InInt)
{
	return FString::Printf(TEXT("%d"), InInt);	
}

FString UKismetStringLibrary::Conv_ByteToString(uint8 InByte)
{
	return FString::Printf(TEXT("%d"), InByte);	
}

FString UKismetStringLibrary::Conv_BoolToString(bool InBool)
{
	return InBool ? TEXT("true") : TEXT("false");	
}

FString UKismetStringLibrary::Conv_VectorToString(FVector InVec)
{
	return InVec.ToString();	
}

FString UKismetStringLibrary::Conv_IntVectorToString(FIntVector InIntVec)
{
	return InIntVec.ToString();
}

FString UKismetStringLibrary::Conv_Vector2dToString(FVector2D InVec)
{
	return InVec.ToString();	
}

FString UKismetStringLibrary::Conv_RotatorToString(FRotator InRot)
{
	return InRot.ToString();	
}

FString UKismetStringLibrary::Conv_TransformToString(const FTransform& InTrans)
{
	return FString::Printf(TEXT("Translation: %s Rotation: %s Scale %s"), *InTrans.GetTranslation().ToString(), *InTrans.Rotator().ToString(), *InTrans.GetScale3D().ToString());
}

FString UKismetStringLibrary::Conv_ObjectToString(class UObject* InObj)
{
	return (InObj != NULL) ? InObj->GetName() : FString(TEXT("None"));
}

FString UKismetStringLibrary::Conv_ColorToString(FLinearColor C)
{
	return C.ToString();
}

FString UKismetStringLibrary::Conv_NameToString(FName InName)
{
	return InName.ToString();
}

FName UKismetStringLibrary::Conv_StringToName(const FString& InString)
{
	return FName(*InString);
}

int32 UKismetStringLibrary::Conv_StringToInt(const FString& InString)
{
	return FCString::Atoi(*InString);
}

float UKismetStringLibrary::Conv_StringToFloat(const FString& InString)
{
	return FCString::Atof(*InString);
}

void UKismetStringLibrary::Conv_StringToVector(const FString& InString, FVector& OutConvertedVector, bool& OutIsValid)
{
	OutIsValid = OutConvertedVector.InitFromString(InString);
}

void UKismetStringLibrary::Conv_StringToVector2D(const FString& InString, FVector2D& OutConvertedVector2D, bool& OutIsValid)
{
	OutIsValid = OutConvertedVector2D.InitFromString(InString);
}

void UKismetStringLibrary::Conv_StringToRotator(const FString& InString, FRotator& OutConvertedRotator, bool& OutIsValid)
{
	OutIsValid = OutConvertedRotator.InitFromString(InString);
}

void UKismetStringLibrary::Conv_StringToColor(const FString& InString, FLinearColor& OutConvertedColor, bool& OutIsValid)
{
	OutIsValid = OutConvertedColor.InitFromString(InString);
}

FString UKismetStringLibrary::BuildString_Float(const FString& AppendTo, const FString& Prefix, float InFloat, const FString& Suffix)
{
	// faster, preallocating method
	FString const FloatStr = FString::SanitizeFloat(InFloat);

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+FloatStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += FloatStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_Int(const FString& AppendTo, const FString& Prefix, int32 InInt, const FString& Suffix)
{
	// faster, preallocating method
	FString const IntStr = FString::Printf(TEXT("%d"), InInt);

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+IntStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += IntStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_Bool(const FString& AppendTo, const FString& Prefix, bool InBool, const FString& Suffix)
{
	// faster, preallocating method
	FString const BoolStr = InBool ? TEXT("true") : TEXT("false");	

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+BoolStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += BoolStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_Vector(const FString& AppendTo, const FString& Prefix, FVector InVector, const FString& Suffix)
{
	// faster, preallocating method
	FString const VecStr = InVector.ToString();

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+VecStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += VecStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_IntVector(const FString& AppendTo, const FString& Prefix, FIntVector InIntVector, const FString& Suffix)
{
	// faster, preallocating method
	FString const VecStr = InIntVector.ToString();

	FString StringResult;
	StringResult.Empty(AppendTo.Len() + Prefix.Len() + VecStr.Len() + Suffix.Len() + 1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += VecStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_Vector2d(const FString& AppendTo, const FString& Prefix, FVector2D InVector2d, const FString& Suffix)
{
	// faster, preallocating method
	FString const VecStr = InVector2d.ToString();

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+VecStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += VecStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_Rotator(const FString& AppendTo, const FString& Prefix, FRotator InRot, const FString& Suffix)
{
	// faster, preallocating method
	FString const RotStr = InRot.ToString();

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+RotStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += RotStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_Object(const FString& AppendTo, const FString& Prefix, class UObject* InObj, const FString& Suffix)
{
	// faster, preallocating method
	FString const ObjStr = (InObj != NULL) ? InObj->GetName() : FString(TEXT("None"));

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+ObjStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += ObjStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_Color(const FString& AppendTo, const FString& Prefix, FLinearColor InColor, const FString& Suffix)
{
	// faster, preallocating method
	FString const ColorStr = InColor.ToString();

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+ColorStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += ColorStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::BuildString_Name(const FString& AppendTo, const FString& Prefix, FName InName, const FString& Suffix)
{
	// faster, preallocating method
	FString const NameStr = InName.ToString();

	FString StringResult;
	StringResult.Empty(AppendTo.Len()+Prefix.Len()+NameStr.Len()+Suffix.Len()+1); // adding one for the string terminator
	StringResult += AppendTo;
	StringResult += Prefix;
	StringResult += NameStr;
	StringResult += Suffix;

	return StringResult;
}

FString UKismetStringLibrary::GetSubstring(const FString& SourceString, int32 StartIndex, int32 Length)
{
	return (Length >= 0 ? SourceString.Mid(StartIndex, Length) : FString());
}

int32 UKismetStringLibrary::FindSubstring(const FString& SearchIn, const FString& Substring, bool bUseCase, bool bSearchFromEnd, int32 StartPosition)
{
	ESearchCase::Type Case = bUseCase ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;
	ESearchDir::Type Dir = bSearchFromEnd ? ESearchDir::FromEnd : ESearchDir::FromStart;

	return SearchIn.Find(Substring, Case, Dir, StartPosition);
}

bool UKismetStringLibrary::Contains(const FString& SearchIn, const FString& Substring, bool bUseCase, bool bSearchFromEnd)
{
	return FindSubstring(SearchIn, Substring, bUseCase, bSearchFromEnd) != -1;
}

int32 UKismetStringLibrary::GetCharacterAsNumber(const FString& SourceString, int32 Index)
{
	if ((Index >= 0) && (Index < SourceString.Len()))
	{
		return (int32)(SourceString.GetCharArray()[Index]);
	}
	else
	{
		//@TODO: Script error
		return 0;
	}
}

TArray<FString> UKismetStringLibrary::ParseIntoArray(const FString& SourceString, const FString& Delimiter, const bool CullEmptyStrings)
{
	TArray<FString> SeparatedStrings;
	const int32 nArraySize = SourceString.ParseIntoArray(SeparatedStrings, *Delimiter, CullEmptyStrings);
	return SeparatedStrings;
}

FString UKismetStringLibrary::JoinStringArray(const TArray<FString>& SourceArray, const FString&  Separator)
{
	return FString::Join(SourceArray, *Separator);
}

TArray<FString> UKismetStringLibrary::GetCharacterArrayFromString(const FString& SourceString)
{
	TArray<FString> SeparatedChars;

	if (!SourceString.IsEmpty())
	{
		for (auto CharIt(SourceString.CreateConstIterator()); CharIt; ++CharIt)
		{
			TCHAR Char = *CharIt;
			SeparatedChars.Add(FString(1, &Char));
		}

		// Remove the null terminator on the end
		SeparatedChars.RemoveAt(SeparatedChars.Num() - 1, 1);
	}

	return SeparatedChars;
}

FString UKismetStringLibrary::ToUpper(const FString& SourceString)
{
	return SourceString.ToUpper();
}

FString UKismetStringLibrary::ToLower(const FString& SourceString)
{
	return SourceString.ToLower();
}

FString UKismetStringLibrary::LeftPad(const FString& SourceString, int32 ChCount)
{
	return SourceString.LeftPad(ChCount);
}

FString UKismetStringLibrary::RightPad(const FString& SourceString, int32 ChCount)
{
	return SourceString.RightPad(ChCount);
}

bool UKismetStringLibrary::IsNumeric(const FString& SourceString)
{
	return SourceString.IsNumeric();
}

bool UKismetStringLibrary::StartsWith(const FString& SourceString, const FString& InPrefix, ESearchCase::Type SearchCase)
{
	return SourceString.StartsWith(InPrefix,SearchCase);
}

bool UKismetStringLibrary::EndsWith(const FString& SourceString, const FString& InSuffix, ESearchCase::Type SearchCase)
{
	return SourceString.EndsWith(InSuffix,SearchCase);
}

bool UKismetStringLibrary::MatchesWildcard(const FString& SourceString, const FString& Wildcard, ESearchCase::Type SearchCase)
{
	return SourceString.MatchesWildcard(Wildcard, SearchCase);
}

FString UKismetStringLibrary::Trim(const FString& SourceString)
{
	FString Trimmed = SourceString.TrimStart();
	return Trimmed;
}

FString UKismetStringLibrary::TrimTrailing(const FString& SourceString)
{
	FString Trimmed = SourceString.TrimEnd();
	return Trimmed;
}

int32 UKismetStringLibrary::CullArray(const FString& SourceString,TArray<FString>& InArray)
{
	return SourceString.CullArray(&InArray);
}

FString UKismetStringLibrary::Reverse(const FString& SourceString)
{
//	return SourceString.Reverse();
	FString Reversed = SourceString;
	Reversed.ReverseString();
	return Reversed;
}

FString UKismetStringLibrary::Replace(const FString& SourceString, const FString& From, const FString& To, ESearchCase::Type SearchCase)
{
	return SourceString.Replace(*From, *To, SearchCase);
}

int32 UKismetStringLibrary::ReplaceInline(FString& SourceString, const FString& SearchText, const FString& ReplacementText, ESearchCase::Type SearchCase)
{
	return SourceString.ReplaceInline(*SearchText, *ReplacementText, SearchCase);
}

bool UKismetStringLibrary::Split(const FString& SourceString, const FString& InStr, FString& LeftS, FString& RightS, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir)
{
	return SourceString.Split(InStr, &LeftS, &RightS, SearchCase, SearchDir);
}

FString UKismetStringLibrary::Left(const FString& SourceString, int32 Count)
{
	return SourceString.Left(Count);
}

FString UKismetStringLibrary::LeftChop(const FString& SourceString, int32 Count)
{
	return SourceString.LeftChop(Count);
}

FString UKismetStringLibrary::Right(const FString& SourceString, int32 Count)
{
	return SourceString.Right(Count);
}

FString UKismetStringLibrary::RightChop(const FString& SourceString, int32 Count)
{
	return SourceString.RightChop(Count);
}

FString UKismetStringLibrary::Mid(const FString& SourceString, int32 Start, int32 Count = MAX_int32)
{
	return (Count >= 0 ? SourceString.Mid(Start, Count) : FString());
}

FString UKismetStringLibrary::TimeSecondsToString(float InSeconds)
{
	// Get whole minutes
	const int32 NumMinutes = FMath::FloorToInt(InSeconds/60.f);
	// Get seconds not part of whole minutes
	const int32 NumSeconds = FMath::FloorToInt(InSeconds-(NumMinutes*60.f));
	// Get fraction of non-whole seconds, convert to 100th of a second, then floor to get whole 100ths
	const int32 NumCentiseconds = FMath::FloorToInt((InSeconds - FMath::FloorToFloat(InSeconds)) * 100.f);

	// Create string, including leading zeroes
	return FString::Printf(TEXT("%02d:%02d.%02d"), NumMinutes, NumSeconds, NumCentiseconds);
}
