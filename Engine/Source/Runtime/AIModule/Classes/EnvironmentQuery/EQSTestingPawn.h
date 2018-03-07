// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EQSQueryResultSourceInterface.h"
#include "GameFramework/Character.h"
#include "EQSTestingPawn.generated.h"

class UEnvQuery;
class UEQSRenderingComponent;

UENUM()
enum class EEnvQueryHightlightMode : uint8
{
	All,
	Best5Pct UMETA(DisplayName = "Best 5%"),
	Best25Pct UMETA(DisplayName = "Best 25%"),
};

/** this class is abstract even though it's perfectly functional on its own.
 *	The reason is to stop it from showing as valid player pawn type when configuring 
 *	project's game mode. */
UCLASS(abstract, hidecategories=(Advanced, Attachment, Mesh, Animation, Clothing, Physics, Rendering, Lighting, Activation, CharacterMovement, AgentPhysics, Avoidance, MovementComponent, Velocity, Shape, Camera, Input, Layers, SkeletalMesh, Optimization, Pawn, Replication, Actor))
class AIMODULE_API AEQSTestingPawn : public ACharacter, public IEQSQueryResultSourceInterface
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(Category=EQS, EditAnywhere)
	UEnvQuery* QueryTemplate;

	/** optional parameters for query */
	UPROPERTY(Category=EQS, VisibleAnywhere, meta=(DisplayName="QueryParams_DEPRECATED"))
	TArray<FEnvNamedValue> QueryParams;

	UPROPERTY(Category=EQS, EditAnywhere)
	TArray<FAIDynamicParam> QueryConfig;

	UPROPERTY(Category=EQS, EditAnywhere)
	float TimeLimitPerStep;

	UPROPERTY(Category=EQS, EditAnywhere)
	int32 StepToDebugDraw;

	UPROPERTY(Category = EQS, EditAnywhere)
	EEnvQueryHightlightMode HighlightMode;

	UPROPERTY(Category = EQS, EditAnywhere)
	uint32 bDrawLabels:1;

	UPROPERTY(Category=EQS, EditAnywhere)
	uint32 bDrawFailedItems:1;

	UPROPERTY(Category=EQS, EditAnywhere)
	uint32 bReRunQueryOnlyOnFinishedMove:1;

	UPROPERTY(Category=EQS, EditAnywhere)
	uint32 bShouldBeVisibleInGame:1;

	UPROPERTY(Category = EQS, EditAnywhere)
	uint32 bTickDuringGame : 1;

	UPROPERTY(Category=EQS, EditAnywhere)
	TEnumAsByte<EEnvQueryRunMode::Type> QueryingMode;
	

#if WITH_EDITORONLY_DATA
private:
	/** Editor Preview */
	UPROPERTY(Transient)
	UEQSRenderingComponent* EdRenderComp;
#endif // WITH_EDITORONLY_DATA

protected:
	TSharedPtr<FEnvQueryInstance> QueryInstance;

	TArray<FEnvQueryInstance> StepResults;

public:
	/** This pawn class spawns its controller in PostInitProperties to have it available in editor mode*/
	virtual void TickActor( float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction ) override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
#endif // WITH_EDITOR

	// IEQSQueryResultSourceInterface start
	virtual const FEnvQueryResult* GetQueryResult() const override;
	virtual const FEnvQueryInstance* GetQueryInstance() const  override;

	virtual bool GetShouldDebugDrawLabels() const override { return bDrawLabels; }
	virtual bool GetShouldDrawFailedItems() const override{ return bDrawFailedItems; }
	virtual float GetHighlightRangePct() const override;
	// IEQSQueryResultSourceInterface end

	void RunEQSQuery();

protected:	
	void Reset() override;
	void MakeOneStep();

	void UpdateDrawing();

	static void OnEditorSelectionChanged(UObject* NewSelection);

public:
#if WITH_EDITORONLY_DATA
	/** Returns EdRenderComp subobject **/
	UEQSRenderingComponent* GetEdRenderComp() { return EdRenderComp; }
#endif
};
