// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBehaviorTreeEditor;
class FExtender;
class FToolBarBuilder;

class FBehaviorTreeEditorToolbar : public TSharedFromThis<FBehaviorTreeEditorToolbar>
{
public:
	FBehaviorTreeEditorToolbar(TSharedPtr<FBehaviorTreeEditor> InBehaviorTreeEditor)
		: BehaviorTreeEditor(InBehaviorTreeEditor) {}

	void AddModesToolbar(TSharedPtr<FExtender> Extender);
	void AddDebuggerToolbar(TSharedPtr<FExtender> Extender);
	void AddBehaviorTreeToolbar(TSharedPtr<FExtender> Extender);

private:
	void FillModesToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillDebuggerToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillBehaviorTreeToolbar(FToolBarBuilder& ToolbarBuilder);

protected:
	/** Pointer back to the blueprint editor tool that owns us */
	TWeakPtr<FBehaviorTreeEditor> BehaviorTreeEditor;
};
