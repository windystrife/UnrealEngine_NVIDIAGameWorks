// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "GameFramework/OnlineReplStructs.h"
#include "PartyGameState.h"
#include "PartyMemberState.generated.h"

/**
 * Simple struct for replication and copying of party member data on updates
 */
USTRUCT()
struct FPartyMemberRepState
{
	GENERATED_USTRUCT_BODY()

	/** Reset the variables of this party member state */
	virtual void Reset()
	{}

	virtual ~FPartyMemberRepState()
	{}
};

/**
 * Main representation of a party member
 */
UCLASS(config = Game, notplaceable)
class PARTY_API UPartyMemberState : public UObject
{
	GENERATED_UCLASS_BODY()

	// Begin UObject Interface
	virtual void BeginDestroy() override;
	// End UObject Interface

	/** Unique id of this party member */
	UPROPERTY(Transient)
	FUniqueNetIdRepl UniqueId;

	/** Display name of this party member */
	UPROPERTY(BlueprintReadOnly, Transient, Category = PartyMemberState)
	FText DisplayName;

	/** @return the party this member is associated with */
	UPartyGameState* GetParty() const;

	/** @return True if this party member is the party leader */
	UFUNCTION(BlueprintCallable, Category = PartyMemberState)
	bool IsPartyLeader() const;

	/** @return True if this party member state corresponds to the local player */
	UFUNCTION(BlueprintCallable, Category = PartyMemberState)
	bool IsLocalPlayer() const;

protected:

	/** Reflection data for child USTRUCT */
	UPROPERTY()
	const UScriptStruct* MemberStateRefDef;

	/** Pointer to child USTRUCT that holds the current state of party member (set via InitPartyMemberState) */
	FPartyMemberRepState* MemberStateRef;

	template<typename T>
	void InitPartyMemberState(T* InMemberState)
	{
		MemberStateRef = InMemberState;
		if (MemberStateRefDef == nullptr)
		{
			MemberStateRefDef = T::StaticStruct();
		}

		MemberStateRefScratch = (FPartyMemberRepState*)FMemory::Malloc(MemberStateRefDef->GetCppStructOps()->GetSize());
		MemberStateRefDef->InitializeStruct(MemberStateRefScratch);
	}

	void UpdatePartyMemberState();

	/**
	 * Compare current data to old data, triggering delegates
	 *
	 * @param OldPartyMemberState old view of data to compare against
	 */
	virtual void ComparePartyMemberData(const FPartyMemberRepState& OldPartyMemberState);

	/** Reset to default state */
	virtual void Reset();

private:
	

	/** Scratch copy of child USTRUCT for handling replication comparisons */
	FPartyMemberRepState* MemberStateRefScratch;

	/** Have we announced this player joining the game locally */
	UPROPERTY(Transient)
	bool bHasAnnouncedJoin;

	friend UPartyGameState;
};
