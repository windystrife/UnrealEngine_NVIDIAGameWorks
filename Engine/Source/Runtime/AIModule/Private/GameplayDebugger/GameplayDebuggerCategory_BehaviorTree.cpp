// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebugger/GameplayDebuggerCategory_BehaviorTree.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameFramework/Pawn.h"
#include "BrainComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

FGameplayDebuggerCategory_BehaviorTree::FGameplayDebuggerCategory_BehaviorTree()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_BehaviorTree::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_BehaviorTree());
}

void FGameplayDebuggerCategory_BehaviorTree::FRepData::Serialize(FArchive& Ar)
{
	Ar << CompName;
	Ar << TreeDesc;
	Ar << BlackboardDesc;
}

void FGameplayDebuggerCategory_BehaviorTree::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	APawn* MyPawn = Cast<APawn>(DebugActor);
	AAIController* MyController = MyPawn ? Cast<AAIController>(MyPawn->Controller) : nullptr;
	UBrainComponent* BrainComp = MyController ? MyController->GetBrainComponent() : nullptr;
	
	if (BrainComp && !BrainComp->IsPendingKill())
	{
		DataPack.CompName = BrainComp->GetName();
		DataPack.TreeDesc = BrainComp->GetDebugInfoString();

		if (BrainComp->GetBlackboardComponent())
		{
			DataPack.BlackboardDesc = BrainComp->GetBlackboardComponent()->GetDebugInfoString(EBlackboardDescription::KeyWithValue);
		}
	}
}

void FGameplayDebuggerCategory_BehaviorTree::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (!DataPack.CompName.IsEmpty())
	{
		CanvasContext.Printf(TEXT("Brain Component: {yellow}%s"), *DataPack.CompName);
		CanvasContext.Print(DataPack.TreeDesc);

		TArray<FString> BlackboardLines;
		DataPack.BlackboardDesc.ParseIntoArrayLines(BlackboardLines, true);

		int32 SavedDefX = CanvasContext.DefaultX;
		int32 SavedPosY = CanvasContext.CursorY;
		CanvasContext.DefaultX = CanvasContext.CursorX = 600.0f;
		CanvasContext.CursorY = CanvasContext.DefaultY;

		for (int32 Idx = 0; Idx < BlackboardLines.Num(); Idx++)
		{
			int32 SeparatorIndex = INDEX_NONE;
			BlackboardLines[Idx].FindChar(TEXT(':'), SeparatorIndex);

			if (SeparatorIndex != INDEX_NONE && Idx)
			{
				FString ColoredLine = BlackboardLines[Idx].Left(SeparatorIndex + 1) + FString(TEXT("{yellow}")) + BlackboardLines[Idx].Mid(SeparatorIndex + 1);
				CanvasContext.Print(ColoredLine);
			}
			else
			{
				CanvasContext.Print(BlackboardLines[Idx]);
			}
		}

		CanvasContext.DefaultX = CanvasContext.CursorX = SavedDefX;
		CanvasContext.CursorY = SavedPosY;
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
