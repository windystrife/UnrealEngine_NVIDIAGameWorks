// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLoggerExtension.generated.h"

class AActor;
class UCanvas;
class UEQSRenderingComponent;
struct FLogEntryItem;

namespace EVisLogTags
{
	const FString TAG_EQS = TEXT("LogEQS");
}

#if ENABLE_VISUAL_LOG
struct FVisualLogDataBlock;
struct FLogEntryItem;
class UCanvas;

class FVisualLoggerExtension : public FVisualLogExtensionInterface
{
public:
	FVisualLoggerExtension();

	virtual void ResetData(IVisualLoggerEditorInterface* EdInterface) override;
	virtual void DrawData(IVisualLoggerEditorInterface* EdInterface, UCanvas* Canvas) override;
	virtual void OnItemsSelectionChanged(IVisualLoggerEditorInterface* EdInterface) override;
	virtual void OnLogLineSelectionChanged(IVisualLoggerEditorInterface* EdInterface, TSharedPtr<struct FLogEntryItem> SelectedItem, int64 UserData) override;

private:
	void DrawData(UWorld* InWorld, class UEQSRenderingComponent* EQSRenderingComponent, UCanvas* Canvas, AActor* HelperActor, const FName& TagName, const FVisualLogDataBlock& DataBlock, float Timestamp);
	void DisableEQSRendering(AActor* HelperActor);

protected:
	uint32 SelectedEQSId;
	float CurrentTimestamp;
	TArray<TWeakObjectPtr<class UEQSRenderingComponent> >	EQSRenderingComponents;
};
#endif //ENABLE_VISUAL_LOG

UCLASS(Abstract)
class AIMODULE_API UVisualLoggerExtension : public UObject
{
	GENERATED_BODY()
};
