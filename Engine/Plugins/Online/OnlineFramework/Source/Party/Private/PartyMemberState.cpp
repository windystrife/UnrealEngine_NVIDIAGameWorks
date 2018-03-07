// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PartyMemberState.h"

UPartyMemberState::UPartyMemberState(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	MemberStateRefDef(nullptr),
	MemberStateRef(nullptr),
	MemberStateRefScratch(nullptr),
	bHasAnnouncedJoin(false)	
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
	}
}

void UPartyMemberState::BeginDestroy()
{
	Super::BeginDestroy();

	if (MemberStateRefDef)
	{
		if (MemberStateRefScratch)
		{
			MemberStateRefDef->DestroyStruct(MemberStateRefScratch);
			FMemory::Free(MemberStateRefScratch);
			MemberStateRefScratch = nullptr;
		}
		MemberStateRefDef = nullptr;
	}

	MemberStateRef = nullptr;
}

UPartyGameState* UPartyMemberState::GetParty() const
{
	return GetTypedOuter<UPartyGameState>();
}

bool UPartyMemberState::IsPartyLeader() const
{
	UPartyGameState* Party = GetParty();
	check(Party);

	TSharedPtr<const FUniqueNetId> PartyLeader = Party->GetPartyLeader();
	if (PartyLeader.IsValid() && PartyLeader->IsValid())
	{
		return *PartyLeader == *UniqueId;
	}

	return false;
}

bool UPartyMemberState::IsLocalPlayer() const
{
	UPartyGameState* Party = GetParty();
	check(Party);

	return UniqueId == Party->GetOwningUserId();
}

void UPartyMemberState::UpdatePartyMemberState()
{
	UPartyGameState* Party = GetParty();
	check(Party);
	Party->UpdatePartyMemberState(UniqueId, this);
}

void UPartyMemberState::ComparePartyMemberData(const FPartyMemberRepState& OldPartyMemberState)
{
	UPartyGameState* Party = GetParty();
	check(Party);
}

void UPartyMemberState::Reset()
{
	if (MemberStateRef)
	{
		MemberStateRef->Reset();
	}
}
