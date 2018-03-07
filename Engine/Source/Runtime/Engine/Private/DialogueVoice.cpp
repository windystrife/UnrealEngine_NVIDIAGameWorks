// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sound/DialogueVoice.h"
#include "UObject/UnrealType.h"

UDialogueVoice::UDialogueVoice(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LocalizationGUID( FGuid::NewGuid() )
{
}

// Begin UObject interface. 
bool UDialogueVoice::IsReadyForFinishDestroy()
{
	return true;
}

FName UDialogueVoice::GetExporterName()
{
	return NAME_None;
}

FString UDialogueVoice::GetDesc()
{
	FString SummaryString;
	{
		UByteProperty* GenderProperty = CastChecked<UByteProperty>(FindFieldChecked<UProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(UDialogueVoice, Gender)));
		SummaryString += GenderProperty->Enum->GetDisplayNameTextByValue(Gender).ToString();

		if( Plurality != EGrammaticalNumber::Singular )
		{
			UByteProperty* PluralityProperty = CastChecked<UByteProperty>( FindFieldChecked<UProperty>( GetClass(), GET_MEMBER_NAME_CHECKED(UDialogueVoice, Plurality)) );

			SummaryString += ", ";
			SummaryString += PluralityProperty->Enum->GetDisplayNameTextByValue(Plurality).ToString();
		}
	}

	return FString::Printf( TEXT( "%s (%s)" ), *( GetName() ), *(SummaryString) );
}

void UDialogueVoice::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}

void UDialogueVoice::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if( !bDuplicateForPIE )
	{
		LocalizationGUID = FGuid::NewGuid();
	}
}
// End UObject interface. 
