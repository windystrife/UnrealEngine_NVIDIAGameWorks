// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Commands/Commands.h"
#include "Styling/ISlateStyle.h"

#define LOC_DEFINE_REGION

void UI_COMMAND_Function( FBindingContext* This, TSharedPtr< FUICommandInfo >& OutCommand, const TCHAR* OutSubNamespace, const TCHAR* OutCommandName, const TCHAR* OutCommandNameUnderscoreTooltip, const ANSICHAR* DotOutCommandName, const TCHAR* FriendlyName, const TCHAR* InDescription, const EUserInterfaceActionType::Type CommandType, const FInputChord& InDefaultChord, const FInputChord& InAlternateDefaultChord)
{
	static const FString UICommandsStr(TEXT("UICommands"));
	const FString Namespace = OutSubNamespace && FCString::Strlen(OutSubNamespace) > 0 ?  UICommandsStr + TEXT(".") + OutSubNamespace : UICommandsStr;

	FUICommandInfo::MakeCommandInfo(
		This->AsShared(),
		OutCommand,
		OutCommandName,
		FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText( FriendlyName, *Namespace, OutCommandName ),
		FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText( InDescription, *Namespace, OutCommandNameUnderscoreTooltip ),
		FSlateIcon( This->GetStyleSetName(), ISlateStyle::Join( This->GetContextName(), DotOutCommandName ) ),
		CommandType,
		InDefaultChord,
		InAlternateDefaultChord
	);
}

#undef LOC_DEFINE_REGION
