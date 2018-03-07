// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_RunEQS.generated.h"

class UBehaviorTree;

struct FBTEQSServiceMemory
{
	/** Query request ID */
	int32 RequestID;
};

UCLASS()
class AIMODULE_API UBTService_RunEQS : public UBTService_BlackboardBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(Category = EQS, EditAnywhere)
	FEQSParametrizedQueryExecutionRequest EQSRequest;

	FQueryFinishedSignature QueryFinishedDelegate;

public:
	UBTService_RunEQS(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTEQSServiceMemory); }

	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	/** We need this only for verification, no need to have it in shipped builds */
	virtual void CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const override;
	/** prepare query params */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);
};
