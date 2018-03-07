// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IBuildPatchServicesModule.h"

namespace BuildPatchTool
{
	enum class EReturnCode : int32;

	class IToolMode
	{
	public:
		TCHAR const * const EqualsStr = TEXT("=");
		TCHAR const * const QuoteStr = TEXT("\"");

	public:
		virtual EReturnCode Execute() = 0;

		/**
		 * Helper for parsing a switch from an array of switches, usually produced using FCommandLine::Parse(..)
		 *
		 * @param InSwitch      The switch name, ending with =. E.g. option=, foo=. It would usually be a compile time const.
		 * @param Value         FString to receive the value for the switch
		 * @param Switches      The array of switches to search through
		 * @return true if the switch was found
		**/
		template <typename TValueType>
		bool ParseSwitch(const TCHAR* InSwitch, TValueType& Value, const TArray<FString>& Switches)
		{
			// Debug check requirements for InSwitch
			checkSlow(InSwitch != nullptr);
			checkSlow(InSwitch[FCString::Strlen(InSwitch)-1] == TEXT('='));

			for (const FString& Switch : Switches)
			{
				if (Switch.StartsWith(InSwitch))
				{
					FString StringValue;
					Switch.Split(EqualsStr, nullptr, &StringValue);
					return ParseValue(StringValue, Value);
				}
			}
			return false;
		}

		bool ParseOption(const TCHAR* InSwitch, const TArray<FString>& Switches)
		{
			return Switches.Contains(InSwitch);
		}

		bool ParseValue(const FString& ValueIn, FString& ValueOut)
		{
			ValueOut = ValueIn.TrimQuotes();
			return true;
		}

		bool ParseValue(const FString& ValueIn, uint64& ValueOut)
		{
			if (FCString::IsNumeric(*ValueIn) && !ValueIn.Contains(TEXT("-"), ESearchCase::CaseSensitive))
			{
				ValueOut = FCString::Strtoui64(*ValueIn, nullptr, 10);
				return true;
			}
			return false;
		}
	};

	typedef TSharedRef<IToolMode> IToolModeRef;
	typedef TSharedPtr<IToolMode> IToolModePtr;

	class FToolModeFactory
	{
	public:
		static IToolModeRef Create(IBuildPatchServicesModule& BpsInterface);
	};
}
