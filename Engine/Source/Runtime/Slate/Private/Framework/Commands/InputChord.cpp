// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Commands/InputChord.h"

#define LOCTEXT_NAMESPACE "FInputChord"

/* FInputGesture interface
 *****************************************************************************/

/**
 * Returns the friendly, localized string name of this key binding
 * @todo Slate: Got to be a better way to do this
 */
FText FInputChord::GetInputText( ) const
{
#if PLATFORM_MAC
    const FText CommandText = LOCTEXT("KeyName_Control", "Ctrl");
    const FText ControlText = LOCTEXT("KeyName_Command", "Cmd");
#else
    const FText ControlText = LOCTEXT("KeyName_Control", "Ctrl");
    const FText CommandText = LOCTEXT("KeyName_Command", "Cmd"); 
#endif
    const FText AltText = LOCTEXT("KeyName_Alt", "Alt");
    const FText ShiftText = LOCTEXT("KeyName_Shift", "Shift");
    
	const FText AppenderText = LOCTEXT("ModAppender", "+");

	FFormatNamedArguments Args;
	int32 ModCount = 0;

    if (bCtrl)
    {
		Args.Add(FString::Printf(TEXT("Mod%d"),++ModCount), ControlText);
    }
    if (bCmd)
    {
		Args.Add(FString::Printf(TEXT("Mod%d"),++ModCount), CommandText);
    }
    if (bAlt)
    {
		Args.Add(FString::Printf(TEXT("Mod%d"),++ModCount), AltText);
    }
    if (bShift)
    {
		Args.Add(FString::Printf(TEXT("Mod%d"),++ModCount), ShiftText);
    }

	for (int32 i = 1; i <= 4; ++i)
	{
		if (i > ModCount)
		{
			Args.Add(FString::Printf(TEXT("Mod%d"), i), FText::GetEmpty());
			Args.Add(FString::Printf(TEXT("Appender%d"), i), FText::GetEmpty());
		}
		else
		{
			Args.Add(FString::Printf(TEXT("Appender%d"), i), AppenderText);
		}

	}

	Args.Add(TEXT("Key"), GetKeyText());

	return FText::Format(LOCTEXT("FourModifiers", "{Mod1}{Appender1}{Mod2}{Appender2}{Mod3}{Appender3}{Mod4}{Appender4}{Key}"), Args);
}


FText FInputChord::GetKeyText( ) const
{
	FText OutString; // = KeyGetDisplayName(Key);

	if (Key.IsValid() && !Key.IsModifierKey())
	{
		OutString = Key.GetDisplayName();
	}

	return OutString;
}

FInputChord::ERelationshipType FInputChord::GetRelationship( const FInputChord& OtherChord ) const
{
	ERelationshipType Relationship = None;

	if (Key == OtherChord.Key)
	{
		if ((bAlt == OtherChord.bAlt) &&
			(bCtrl == OtherChord.bCtrl) &&
			(bShift == OtherChord.bShift) &&
			(bCmd == OtherChord.bCmd))
		{
			Relationship = Same;
		}
		else if ((bAlt || !OtherChord.bAlt) &&
				(bCtrl || !OtherChord.bCtrl) &&
				(bShift || !OtherChord.bShift) &&
				(bCmd || !OtherChord.bCmd))
		{
			Relationship = Masks;
		}
		else if ((!bAlt || OtherChord.bAlt) &&
				(!bCtrl || OtherChord.bCtrl) &&
				(!bShift || OtherChord.bShift) &&
				(!bCmd || OtherChord.bCmd))
		{
			Relationship = Masked;
		}
	}

	return Relationship;
}

#undef LOCTEXT_NAMESPACE
