// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Commands/InputBindingManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SlateGlobals.h"
#include "Misc/RemoteConfigIni.h"


/* FUserDefinedChords helper class
 *****************************************************************************/

/** An identifier for a user defined chord */
struct FUserDefinedChordKey
{
	FUserDefinedChordKey()
	{
	}

	FUserDefinedChordKey(const FName InBindingContext, const FName InCommandName, const EMultipleKeyBindingIndex InChordIndex)
		: BindingContext(InBindingContext)
		, CommandName(InCommandName)
		, ChordIndex(InChordIndex)
	{
	}

	bool operator==(const FUserDefinedChordKey& Other) const
	{
		return BindingContext == Other.BindingContext && CommandName == Other.CommandName && ChordIndex == Other.ChordIndex;
	}

	bool operator!=(const FUserDefinedChordKey& Other) const
	{
		return !(*this == Other);
	}

	FName BindingContext;
	FName CommandName;
	EMultipleKeyBindingIndex ChordIndex;
};

uint32 GetTypeHash( const FUserDefinedChordKey& Key )
{
	return GetTypeHash(Key.BindingContext) ^ GetTypeHash(Key.CommandName);
}

class FUserDefinedChords
{
public:
	void LoadChords();
	void SaveChords() const;
	bool GetUserDefinedChord( const FName BindingContext, const FName CommandName, const EMultipleKeyBindingIndex ChordIndex, FInputChord& OutUserDefinedChord ) const;
	void SetUserDefinedChords( const FUICommandInfo& CommandInfo );
	/** Remove all user defined chords */
	void RemoveAll();
private:

	void LoadChord(const TSharedPtr<FJsonObject>& ChordInfoObject, const FName& BindingContextName, const EMultipleKeyBindingIndex ChordIndex, const FName& CommandName);

	/* Mapping from a chord key to the user defined chord */
	typedef TMap<FUserDefinedChordKey, FInputChord> FChordsMap;
	TSharedPtr<FChordsMap> Chords;
};

void FUserDefinedChords::LoadChord(const TSharedPtr<FJsonObject>& ChordInfoObject, const FName& BindingContextName, const EMultipleKeyBindingIndex ChordIndex, const FName& CommandName)
{
	const TSharedPtr<FJsonValue> CtrlObj = ChordInfoObject->Values.FindRef(TEXT("Control"));
	const TSharedPtr<FJsonValue> AltObj = ChordInfoObject->Values.FindRef( TEXT("Alt") );
	const TSharedPtr<FJsonValue> ShiftObj = ChordInfoObject->Values.FindRef( TEXT("Shift") );
	const TSharedPtr<FJsonValue> CmdObj = ChordInfoObject->Values.FindRef(TEXT("Command"));
	const TSharedPtr<FJsonValue> KeyObj = ChordInfoObject->Values.FindRef( TEXT("Key") );

	const FUserDefinedChordKey ChordKey(BindingContextName, CommandName, ChordIndex);
	FInputChord& UserDefinedChord = Chords->FindOrAdd(ChordKey);

#if PLATFORM_MAC
	// Command is treated like "Control" on Mac and vice-versa, so swap them.
	EModifierKey::Type Modifiers = EModifierKey::FromBools(
										CmdObj.IsValid() ? CmdObj->AsBool() : false,
										AltObj->AsBool(),
										ShiftObj->AsBool(),
										CtrlObj->AsBool()
									);
#else
	EModifierKey::Type Modifiers = EModifierKey::FromBools(
										CtrlObj->AsBool(),
										AltObj->AsBool(),
										ShiftObj->AsBool(),
										CmdObj.IsValid() ? CmdObj->AsBool() : false
									);
#endif	//#if PLATFORM_MAC

	UserDefinedChord = FInputChord(*KeyObj->AsString(), Modifiers);
}

void FUserDefinedChords::LoadChords()
{
	if( !Chords.IsValid() )
	{
		Chords = MakeShareable( new FChordsMap );

		// First, try and load the chords from their new location in the ini file
		// Failing that, try and load them from the older txt file
		TArray<FString> ChordJsonArray;
		bool bFoundChords = (GConfig->GetArray(TEXT("UserDefinedChords"), TEXT("UserDefinedChords"), ChordJsonArray, GEditorKeyBindingsIni) > 0);
		if (!bFoundChords)
		{
			// Backwards compat for when it was named gestures
			bFoundChords = (GConfig->GetArray(TEXT("UserDefinedGestures"), TEXT("UserDefinedGestures"), ChordJsonArray, GEditorKeyBindingsIni) > 0);
		}

		if(bFoundChords)
		{
			// This loads an array of JSON strings representing the FUserDefinedChordKey and FInputChord in a single JSON object
			for(const FString& ChordJson : ChordJsonArray)
			{
				const FString UnescapedContent = FRemoteConfig::ReplaceIniSpecialCharWithChar(ChordJson).ReplaceEscapedCharWithChar();

				TSharedPtr<FJsonObject> ChordInfoObj;
				auto JsonReader = TJsonReaderFactory<>::Create( UnescapedContent );
				if( FJsonSerializer::Deserialize( JsonReader, ChordInfoObj ) )
				{
					const TSharedPtr<FJsonValue> BindingContextObj = ChordInfoObj->Values.FindRef( TEXT("BindingContext") );
					const TSharedPtr<FJsonValue> CommandNameObj = ChordInfoObj->Values.FindRef( TEXT("CommandName") );
					const TSharedPtr<FJsonValue> ChordIndexObj = ChordInfoObj->Values.FindRef(TEXT("ChordIndex") );

					const FName BindingContext = *BindingContextObj->AsString();
					const FName CommandName = *CommandNameObj->AsString(); 
					const EMultipleKeyBindingIndex ChordIndex = ChordIndexObj.IsValid() ? static_cast<EMultipleKeyBindingIndex>(static_cast<uint32>(ChordIndexObj->AsNumber())) : EMultipleKeyBindingIndex::Primary;

					LoadChord(ChordInfoObj, BindingContext, ChordIndex, CommandName);
				}
			}
		}
		else
		{
			TSharedPtr<FJsonObject> ChordsObj;

			// This loads a JSON object containing BindingContexts, containing objects of CommandNames, containing the FInputChord information
			FString Content;
			bool bFoundContent = (GConfig->GetArray(TEXT("UserDefinedChords"), TEXT("Content"), ChordJsonArray, GEditorKeyBindingsIni) > 0);
			if (!bFoundContent)
			{
				// Backwards compat for when it was named gestures
				bFoundChords = (GConfig->GetArray(TEXT("UserDefinedGestures"), TEXT("Content"), ChordJsonArray, GEditorKeyBindingsIni) > 0);
			}
			if (bFoundContent)
			{
				const FString UnescapedContent = FRemoteConfig::ReplaceIniSpecialCharWithChar(Content).ReplaceEscapedCharWithChar();

				auto JsonReader = TJsonReaderFactory<>::Create( UnescapedContent );
				FJsonSerializer::Deserialize( JsonReader, ChordsObj );
			}

			if (!ChordsObj.IsValid())
			{
				// Chords have not been loaded from the ini file, try reading them from the txt file now
				TSharedPtr<FArchive> Ar = MakeShareable( IFileManager::Get().CreateFileReader( *( FPaths::ProjectSavedDir() / TEXT("Preferences/EditorKeyBindings.txt") ) ) );
				if( Ar.IsValid() )
				{
					auto TextReader = TJsonReaderFactory<ANSICHAR>::Create( Ar.Get() );
					FJsonSerializer::Deserialize( TextReader, ChordsObj );
				}
			}

			if (ChordsObj.IsValid())
			{
				for(const auto& BindingContextInfo : ChordsObj->Values)
				{
					const FName BindingContext = *BindingContextInfo.Key;
					TSharedPtr<FJsonObject> BindingContextObj = BindingContextInfo.Value->AsObject();
					for (const auto& CommandInfo : BindingContextObj->Values)
					{
						const FName CommandName = *CommandInfo.Key;
						TSharedPtr<FJsonObject> CommandObj = CommandInfo.Value->AsObject();
						for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
						{
							LoadChord(CommandObj, BindingContext, static_cast<EMultipleKeyBindingIndex>(i), CommandName);
						}
					}
				}
			}
		}
	}
}

void FUserDefinedChords::SaveChords() const
{
	if( Chords.IsValid() )
	{
		FString ChordRawJsonContent;
		TArray<FString> ChordJsonArray;
		for(const auto& ChordInfo : *Chords)
		{
			TSharedPtr<FJsonValueObject> ChordInfoValueObj = MakeShareable( new FJsonValueObject( MakeShareable( new FJsonObject ) ) );
			TSharedPtr<FJsonObject> ChordInfoObj = ChordInfoValueObj->AsObject();

			// Set the chord values for the command
			ChordInfoObj->Values.Add( TEXT("BindingContext"), MakeShareable( new FJsonValueString( ChordInfo.Key.BindingContext.ToString() ) ) );
			ChordInfoObj->Values.Add( TEXT("CommandName"), MakeShareable( new FJsonValueString( ChordInfo.Key.CommandName.ToString() ) ) );
			ChordInfoObj->Values.Add(TEXT("ChordIndex"), MakeShareable(new FJsonValueNumber(static_cast<uint8>(ChordInfo.Key.ChordIndex))));
			ChordInfoObj->Values.Add( TEXT("Control"), MakeShareable( new FJsonValueBoolean( ChordInfo.Value.NeedsControl() ) ) );
			ChordInfoObj->Values.Add( TEXT("Alt"), MakeShareable( new FJsonValueBoolean( ChordInfo.Value.NeedsAlt() ) ) );
			ChordInfoObj->Values.Add( TEXT("Shift"), MakeShareable( new FJsonValueBoolean( ChordInfo.Value.NeedsShift() ) ) );
			ChordInfoObj->Values.Add( TEXT("Command"), MakeShareable( new FJsonValueBoolean( ChordInfo.Value.NeedsCommand() ) ) );
			ChordInfoObj->Values.Add( TEXT("Key"), MakeShareable( new FJsonValueString( ChordInfo.Value.Key.ToString() ) ) );

			auto JsonWriter = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create( &ChordRawJsonContent );
			FJsonSerializer::Serialize( ChordInfoObj.ToSharedRef(), JsonWriter );

			const FString EscapedContent = FRemoteConfig::ReplaceIniCharWithSpecialChar(ChordRawJsonContent).ReplaceCharWithEscapedChar();
			ChordJsonArray.Add(EscapedContent);
		}

		GConfig->SetArray(TEXT("UserDefinedChords"), TEXT("UserDefinedChords"), ChordJsonArray, GEditorKeyBindingsIni);

		// Clean up old keys (if they still exist)
		GConfig->RemoveKey(TEXT("UserDefinedGestures"), TEXT("UserDefinedGestures"), GEditorKeyBindingsIni);
		GConfig->RemoveKey(TEXT("UserDefinedChords"), TEXT("Content"), GEditorKeyBindingsIni);
		GConfig->RemoveKey(TEXT("UserDefinedChords"), TEXT("Content"), GEditorKeyBindingsIni);
	}
}

bool FUserDefinedChords::GetUserDefinedChord( const FName BindingContext, const FName CommandName, const EMultipleKeyBindingIndex ChordIndex, FInputChord& OutUserDefinedChord ) const
{
	bool bResult = false;

	if( Chords.IsValid() )
	{
		const FUserDefinedChordKey ChordKey(BindingContext, CommandName, ChordIndex);
		const FInputChord* const UserDefinedChordPtr = Chords->Find(ChordKey);
		if( UserDefinedChordPtr )
		{
			OutUserDefinedChord = *UserDefinedChordPtr;
			bResult = true;
		}
	}

	return bResult;
}

void FUserDefinedChords::SetUserDefinedChords( const FUICommandInfo& CommandInfo)
{
	if( Chords.IsValid() )
	{
		const FName BindingContext = CommandInfo.GetBindingContext();
		const FName CommandName = CommandInfo.GetCommandName();

		for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
		{
			EMultipleKeyBindingIndex ChordIndex = static_cast<EMultipleKeyBindingIndex>(i);
			// Find or create the command context
			const FUserDefinedChordKey ChordKey(BindingContext, CommandName, ChordIndex);
			FInputChord& UserDefinedChord = Chords->FindOrAdd(ChordKey);

			// Save an empty invalid chord if one was not set
			// This is an indication that the user doesn't want this bound and not to use the default chord
			const TSharedPtr<const FInputChord> InputChord = CommandInfo.GetActiveChord(ChordIndex);
			UserDefinedChord = (InputChord.IsValid()) ? *InputChord : FInputChord();
		}
	}
}

void FUserDefinedChords::RemoveAll()
{
	Chords = MakeShareable( new FChordsMap );
}


/* FInputBindingManager structors
 *****************************************************************************/

FInputBindingManager& FInputBindingManager::Get()
{
	static FInputBindingManager* Instance= NULL;
	if( Instance == NULL )
	{
		Instance = new FInputBindingManager();
	}

	return *Instance;
}


/* FInputBindingManager interface
 *****************************************************************************/

/**
 * Gets the user defined chord (if any) from the provided command name
 * 
 * @param InBindingContext	The context in which the command is active
 * @param InCommandName		The name of the command to get the chord from
 */
bool FInputBindingManager::GetUserDefinedChord( const FName InBindingContext, const FName InCommandName, const EMultipleKeyBindingIndex InChordIndex, FInputChord& OutUserDefinedChord )
{
	if( !UserDefinedChords.IsValid() )
	{
		UserDefinedChords = MakeShareable( new FUserDefinedChords );
		UserDefinedChords->LoadChords();
	}

	return UserDefinedChords->GetUserDefinedChord( InBindingContext, InCommandName, InChordIndex, OutUserDefinedChord );
}

void FInputBindingManager::CheckForDuplicateDefaultChords( const FBindingContext& InBindingContext, TSharedPtr<FUICommandInfo> InCommandInfo ) const
{
	const bool bCheckDefault = true;
	for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
	{
		if (!InCommandInfo->DefaultChords[i].IsValidChord())
		{
			continue;
		}
		TSharedPtr<FUICommandInfo> ExistingInfo = GetCommandInfoFromInputChord(InBindingContext.GetContextName(), InCommandInfo->DefaultChords[i], bCheckDefault);
		if (ExistingInfo.IsValid())
		{
			if (ExistingInfo->CommandName != InCommandInfo->CommandName)
			{
				// Two different commands with the same name in the same context or parent context
				UE_LOG(LogSlate, Fatal, TEXT("The command '%s.%s' has the same default chord as '%s.%s' [%s]"),
					*InCommandInfo->BindingContext.ToString(),
					*InCommandInfo->CommandName.ToString(),
					*ExistingInfo->BindingContext.ToString(),
					*ExistingInfo->CommandName.ToString(),
					*InCommandInfo->DefaultChords[i].GetInputText().ToString());
			}
		}
	}
}


void FInputBindingManager::NotifyActiveChordChanged( const FUICommandInfo& CommandInfo, const EMultipleKeyBindingIndex InChordIndex )
{
	FContextEntry& ContextEntry = ContextMap.FindChecked( CommandInfo.GetBindingContext() );
	// Slow but doesn't happen frequently
	FChordMap& ActualChordMap = ContextEntry.ChordToCommandInfoMaps[static_cast<uint8>(InChordIndex)];
	for( FChordMap::TIterator It(ActualChordMap); It; ++It )
	{
		// Remove the currently active chord from the map if one exists
		if( It.Value() == CommandInfo.GetCommandName() )
		{
			It.RemoveCurrent();
			// There should only be one active chord for each map
			break;
		}
	}

	if( CommandInfo.GetActiveChord(InChordIndex)->IsValidChord() )
	{
		checkSlow( !ActualChordMap.Contains( *CommandInfo.GetActiveChord(InChordIndex) ) )
			ActualChordMap.Add( *CommandInfo.GetActiveChord(InChordIndex), CommandInfo.GetCommandName() );
	}
	//else if (!bIsAlternateChord) 
	// This is optional, keep it off for now
	//// We removed the primary chord, so swap them and remove the alternate; this is so that it's easier for the tooltip system 
	//{
	//	for (FChordMap::TIterator It(ContextEntry.AlternateChordToCommandInfoMap); It; ++It)
	//	{
	//		if (It.Value() == CommandInfo.GetCommandName())
	//		{
	//			It.RemoveCurrent();
	//			ContextEntry.ChordToCommandInfoMap.Add(It.Key(), It.Value());
	//			break;
	//		}
	//	}
	//}

	// The user defined chords should have already been created
	check( UserDefinedChords.IsValid() );

	UserDefinedChords->SetUserDefinedChords( CommandInfo );

	// Broadcast the chord event when a new one is added
	OnUserDefinedChordChanged.Broadcast(CommandInfo);
}

void FInputBindingManager::SaveInputBindings()
{
	if( UserDefinedChords.IsValid() )
	{
		UserDefinedChords->SaveChords();
	}
}

void FInputBindingManager::RemoveUserDefinedChords()
{
	if( UserDefinedChords.IsValid() )
	{
		UserDefinedChords->RemoveAll();
		UserDefinedChords->SaveChords();
	}
}

void FInputBindingManager::GetCommandInfosFromContext( const FName InBindingContext, TArray< TSharedPtr<FUICommandInfo> >& OutCommandInfos ) const
{
	ContextMap.FindRef( InBindingContext ).CommandInfoMap.GenerateValueArray( OutCommandInfos );
}

void FInputBindingManager::CreateInputCommand( const TSharedRef<FBindingContext>& InBindingContext, TSharedRef<FUICommandInfo> InCommandInfo )
{
	check( InCommandInfo->BindingContext == InBindingContext->GetContextName() );

	// The command name should be valid
	check( InCommandInfo->CommandName != NAME_None );

	// Should not have already created a chord for this command
	for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
	{
		check(!InCommandInfo->ActiveChords[i]->IsValidChord());
	}
	
	const FName ContextName = InBindingContext->GetContextName();

	FContextEntry& ContextEntry = ContextMap.FindOrAdd( ContextName );
	
	// Our parent context must exist.
	check( InBindingContext->GetContextParent() == NAME_None || ContextMap.Find( InBindingContext->GetContextParent() ) != NULL );

	FCommandInfoMap& CommandInfoMap = ContextEntry.CommandInfoMap;

	if( !ContextEntry.BindingContext.IsValid() )
	{
		ContextEntry.BindingContext = InBindingContext;
	}

	if( InBindingContext->GetContextParent() != NAME_None  )
	{
		check( InBindingContext->GetContextName() != InBindingContext->GetContextParent() );
		// Set a mapping from the parent of the current context to the current context
		ParentToChildMap.AddUnique( InBindingContext->GetContextParent(), InBindingContext->GetContextName() );
	}

	CheckForDuplicateDefaultChords( *InBindingContext, InCommandInfo );

	TSharedPtr<FUICommandInfo> ExistingInfo = CommandInfoMap.FindRef( InCommandInfo->CommandName );
	ensureMsgf( !ExistingInfo.IsValid(), TEXT("A command with name %s already exists in context %s"), *InCommandInfo->CommandName.ToString(), *InBindingContext->GetContextName().ToString() );
	
	// Add the command info to the list of known infos.  It can only exist once.
	CommandInfoMap.Add( InCommandInfo->CommandName, InCommandInfo );

	// See if there are user defined chords for this command

	for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
	{
		EMultipleKeyBindingIndex ChordIndex = static_cast<EMultipleKeyBindingIndex> (i);
		FInputChord UserDefinedChord;
		bool bFoundUserDefinedChord = GetUserDefinedChord(ContextName, InCommandInfo->CommandName, ChordIndex, UserDefinedChord);


		if (!bFoundUserDefinedChord && InCommandInfo->DefaultChords[i].IsValidChord())
		{
			// Find any existing command with the same chord 
			// This is for inconsistency between default and user defined chord.  We need to make sure that if default chords are changed to a chord that a user set to a different command, that the default chord doesn't replace
			// the existing commands chord. Note: Duplicate default chords are found above in CheckForDuplicateDefaultChords
			// This needs to check through the maps for the primary and alternate key bindings.
			FName ExisingCommand = NAME_None;  
			for (uint32 j = 0; j < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords) && ExisingCommand == NAME_None; ++j)
			{
				ExisingCommand = ContextEntry.ChordToCommandInfoMaps[j].FindRef(InCommandInfo->DefaultChords[i]);
			}
			if (ExisingCommand == NAME_None)
			{
				// No existing command has a user defined chord and no user defined chord is available for this command 
				TSharedRef<FInputChord> NewChord = MakeShareable(new FInputChord(InCommandInfo->DefaultChords[i]));
				InCommandInfo->ActiveChords[i] = NewChord;
			}

		}
		else if (bFoundUserDefinedChord)
		{
			// Find any existing command with the same chord 
			// This is for inconsistency between default and user defined chord.  We need to make sure that if default chords are changed to a chord that a user set to a different command, that the default chord doesn't replace
			// the existing commands chord. This needs to check through the maps for the primary and alternate key bindings.
			FName ExisingCommandName = NAME_None;
			for (uint32 j = 0; j < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords) && ExisingCommandName == NAME_None; ++j)
			{
				ExisingCommandName = ContextEntry.ChordToCommandInfoMaps[j].FindRef(UserDefinedChord);
			}

			if (ExisingCommandName != NAME_None)
			{
				// Get the command with using the same chord
				TSharedPtr<FUICommandInfo> PreviousInfo = CommandInfoMap.FindRef(ExisingCommandName);
				if (*PreviousInfo->ActiveChords[i] != PreviousInfo->DefaultChords[i])
				{
					// two user defined chords are the same within a context.  If the keybinding editor was used this wont happen so this must have been directly a modified user setting file
					UE_LOG(LogSlate, Error, TEXT("Duplicate user defined chords found: [%s,%s].  Chord for %s being removed"), *InCommandInfo->GetLabel().ToString(), *PreviousInfo->GetLabel().ToString(), *ExistingInfo->GetLabel().ToString());
				}
				ContextEntry.ChordToCommandInfoMaps[i].Remove(*PreviousInfo->ActiveChords[i]);
				// Remove the existing chord. 
				PreviousInfo->ActiveChords[i] = MakeShareable(new FInputChord());
			}

			TSharedRef<FInputChord> NewChord = MakeShareable(new FInputChord(UserDefinedChord));
			// Set the active chord on the command info
			InCommandInfo->ActiveChords[i] = NewChord;
		}

		// If the active chord is valid, map the chord to the map for fast lookup when processing bindings
		if (InCommandInfo->ActiveChords[i]->IsValidChord())
		{
			checkSlow(!ContextEntry.ChordToCommandInfoMaps[i].Contains(*InCommandInfo->GetActiveChord(ChordIndex)));
			ContextEntry.ChordToCommandInfoMaps[i].Add(*InCommandInfo->GetActiveChord(ChordIndex), InCommandInfo->GetCommandName());
		}
	}
}

void FInputBindingManager::RemoveInputCommand(const TSharedRef<FBindingContext>& InBindingContext, TSharedRef<FUICommandInfo> InUICommandInfo)
{
	check(InUICommandInfo->BindingContext == InBindingContext->GetContextName());

	// The command name should be valid
	check(InUICommandInfo->CommandName != NAME_None);

	const FName ContextName = InBindingContext->GetContextName();

	FContextEntry& ContextEntry = ContextMap.FindOrAdd(ContextName);

	// Our parent context must exist.
	check(InBindingContext->GetContextParent() == NAME_None || ContextMap.Find(InBindingContext->GetContextParent()) != NULL);

	// Remove the command and its associated chord if it's valid
	ContextEntry.CommandInfoMap.Remove(InUICommandInfo->CommandName);
	for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
	{
		if (InUICommandInfo->ActiveChords[i]->IsValidChord())
		{
			ContextEntry.ChordToCommandInfoMaps[i].Remove(*InUICommandInfo->GetActiveChord(static_cast<EMultipleKeyBindingIndex> (i)));
		}
	}
}

const TSharedPtr<FUICommandInfo> FInputBindingManager::FindCommandInContext( const FName InBindingContext, const FInputChord& InChord, bool bCheckDefault ) const
{
	const FContextEntry& ContextEntry = ContextMap.FindRef( InBindingContext );
	
	TSharedPtr<FUICommandInfo> FoundCommand = NULL;

	if( bCheckDefault )
	{
		const FCommandInfoMap& InfoMap = ContextEntry.CommandInfoMap;
		for( FCommandInfoMap::TConstIterator It(InfoMap); It && !FoundCommand.IsValid(); ++It )
		{
			const FUICommandInfo& CommandInfo = *It.Value();
			if( CommandInfo.HasDefaultChord(InChord))
			{
				FoundCommand = It.Value();
			}	
		}
	}
	else
	{
		// faster lookup for active chords, using the mapped values
		FName CommandName = NAME_None;
		for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords) && CommandName == NAME_None; ++i)
		{
			CommandName = ContextEntry.ChordToCommandInfoMaps[i].FindRef(InChord);
		}
		if (CommandName != NAME_None)
		{
			FoundCommand = ContextEntry.CommandInfoMap.FindChecked(CommandName);
		}
	}

	return FoundCommand;
}

const TSharedPtr<FUICommandInfo> FInputBindingManager::FindCommandInContext( const FName InBindingContext, const FName CommandName ) const 
{
	const FContextEntry& ContextEntry = ContextMap.FindRef( InBindingContext );
	
	return ContextEntry.CommandInfoMap.FindRef( CommandName );
}


void FInputBindingManager::GetAllChildContexts( const FName InBindingContext, TArray<FName>& AllChildren ) const
{
	AllChildren.Add( InBindingContext );

	TArray<FName> TempChildren;
	ParentToChildMap.MultiFind( InBindingContext, TempChildren );
	for( int32 ChildIndex = 0; ChildIndex < TempChildren.Num(); ++ChildIndex )
	{
		GetAllChildContexts( TempChildren[ChildIndex], AllChildren );
	}
}

const TSharedPtr<FUICommandInfo> FInputBindingManager::GetCommandInfoFromInputChord( const FName InBindingContext, const FInputChord& InChord, bool bCheckDefault ) const
{
	TSharedPtr<FUICommandInfo> FoundCommand = NULL;

	FName CurrentContext = InBindingContext;
	while( CurrentContext != NAME_None && !FoundCommand.IsValid() )
	{
		const FContextEntry& ContextEntry = ContextMap.FindRef( CurrentContext );

		FoundCommand = FindCommandInContext( CurrentContext, InChord, bCheckDefault );

		CurrentContext = ContextEntry.BindingContext->GetContextParent();
	}
	
	if( !FoundCommand.IsValid() )
	{
		TArray<FName> Children;
		GetAllChildContexts( InBindingContext, Children );

		for( int32 ContextIndex = 0; ContextIndex < Children.Num() && !FoundCommand.IsValid(); ++ContextIndex )
		{
			FoundCommand = FindCommandInContext( Children[ContextIndex], InChord, bCheckDefault );
		}
	}
	

	return FoundCommand;
}

/**
 * Returns a list of all known input contexts
 *
 * @param OutInputContexts	The generated list of contexts
 * @return A list of all known input contexts                   
 */
void FInputBindingManager::GetKnownInputContexts( TArray< TSharedPtr<FBindingContext> >& OutInputContexts ) const
{
	for( TMap< FName, FContextEntry >::TConstIterator It(ContextMap); It; ++It )
	{
		OutInputContexts.Add( It.Value().BindingContext );
	}
}

TSharedPtr<FBindingContext> FInputBindingManager::GetContextByName( const FName& InContextName )
{
	FContextEntry* FindResult = ContextMap.Find( InContextName );
	if ( FindResult == NULL )
	{
		return TSharedPtr<FBindingContext>();
	}
	else
	{
		return FindResult->BindingContext;
	}
}

void FInputBindingManager::RemoveContextByName( const FName& InContextName )
{
	ContextMap.Remove(InContextName);
}
