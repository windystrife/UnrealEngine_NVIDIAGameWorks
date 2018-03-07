// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.generated.h"

UENUM()
namespace ETeamAttitude
{
	enum Type
	{
		Friendly,
		Neutral,
		Hostile,
	};
}

USTRUCT(BlueprintType)
struct AIMODULE_API FGenericTeamId
{
	GENERATED_USTRUCT_BODY()

private:
	enum EPredefinedId
	{
		// if you want to change NoTeam's ID update FGenericTeamId::NoTeam

		NoTeamId = 255
	};

protected:
	UPROPERTY(Category = "TeamID", EditAnywhere, BlueprintReadWrite)
	uint8 TeamID;

public:
	FGenericTeamId(uint8 InTeamID = NoTeamId) : TeamID(InTeamID)
	{}

	FORCEINLINE operator uint8() const { return TeamID; }

	FORCEINLINE uint8 GetId() const { return TeamID; }
	//FORCEINLINE void SetId(uint8 NewId) { TeamID = NewId; }
	
	static FGenericTeamId GetTeamIdentifier(const AActor* TeamMember);
	static ETeamAttitude::Type GetAttitude(const AActor* A, const AActor* B);
	static ETeamAttitude::Type GetAttitude(FGenericTeamId TeamA, FGenericTeamId TeamB)
	{
		return AttitudeSolverImpl ? (*AttitudeSolverImpl)(TeamA, TeamB) : ETeamAttitude::Neutral;
	}

	typedef ETeamAttitude::Type FAttitudeSolverFunction(FGenericTeamId, FGenericTeamId);
	
	static void SetAttitudeSolver(FAttitudeSolverFunction* Solver);

protected:
	// the default implementation makes all teams hostile
	// @note that for consistency IGenericTeamAgentInterface should be using the same function 
	//	(by default it does)
	static FAttitudeSolverFunction* AttitudeSolverImpl;

public:
	static const FGenericTeamId NoTeam;
};

UINTERFACE()
class AIMODULE_API UGenericTeamAgentInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class AIMODULE_API IGenericTeamAgentInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Assigns Team Agent to given TeamID */
	virtual void SetGenericTeamId(const FGenericTeamId& TeamID) {}
	
	/** Retrieve team identifier in form of FGenericTeamId */
	virtual FGenericTeamId GetGenericTeamId() const { return FGenericTeamId::NoTeam; }

	/** Retrieved owner attitude toward given Other object */
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const
	{ 
		const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(&Other);
		return OtherTeamAgent ? FGenericTeamId::GetAttitude(GetGenericTeamId(), OtherTeamAgent->GetGenericTeamId())
			: ETeamAttitude::Neutral;
	}
};
