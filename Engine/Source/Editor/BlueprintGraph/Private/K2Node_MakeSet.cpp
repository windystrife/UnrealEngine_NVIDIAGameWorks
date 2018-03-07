// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_MakeSet.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "ScopedTransaction.h"
#include "EdGraphUtilities.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompilerMisc.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

namespace MakeSetLiterals
{
	static const FString OutputPinName = FString(TEXT("Set"));
};

#define LOCTEXT_NAMESPACE "MakeSetNode"

/////////////////////////////////////////////////////
// FKCHandler_MakeSet
class FKCHandler_MakeSet : public FKCHandler_MakeContainer
{
public:
	FKCHandler_MakeSet(FKismetCompilerContext& InCompilerContext)
		: FKCHandler_MakeContainer(InCompilerContext)
	{
		CompiledStatementType = KCST_CreateSet;
	}
};

/////////////////////////////////////////////////////
// UK2Node_MakeSet

UK2Node_MakeSet::UK2Node_MakeSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ContainerType = EPinContainerType::Set;
}

FNodeHandlingFunctor* UK2Node_MakeSet::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_MakeSet(CompilerContext);
}

FText UK2Node_MakeSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Make Set");
}

FString UK2Node_MakeSet::GetOutputPinName() const
{
	return MakeSetLiterals::OutputPinName;
}

FText UK2Node_MakeSet::GetTooltipText() const
{
	return LOCTEXT("MakeSetTooltip", "Create a set from a series of items.");
}

FSlateIcon UK2Node_MakeSet::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.MakeSet_16x");
	return Icon;
}

void UK2Node_MakeSet::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);

	if (!Context.bIsDebugging)
	{
		Context.MenuBuilder->BeginSection("K2NodeMakeSet", NSLOCTEXT("K2Nodes", "MakeSetHeader", "MakeSet"));

		if (Context.Pin)
		{
			if (Context.Pin->Direction == EGPD_Input && Context.Pin->ParentPin == nullptr)
			{
				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("RemovePin", "Remove set element pin"),
					LOCTEXT("RemovePinTooltip", "Remove this set element pin"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateUObject(this, &UK2Node_MakeSet::RemoveInputPin, const_cast<UEdGraphPin*>(Context.Pin))
					)
				);
			}
		}
		else
		{
			Context.MenuBuilder->AddMenuEntry(
				LOCTEXT("AddPin", "Add set element pin"),
				LOCTEXT("AddPinTooltip", "Add another set element pin"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateUObject(this, &UK2Node_MakeSet::InteractiveAddInputPin)
				)
			);
		}

		Context.MenuBuilder->AddMenuEntry(
			LOCTEXT("ResetToWildcard", "Reset to wildcard"),
			LOCTEXT("ResetToWildcardTooltip", "Reset the node to have wildcard input/outputs. Requires no pins are connected."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateUObject(this, &UK2Node_MakeSet::ClearPinTypeToWildcard),
				FCanExecuteAction::CreateUObject(this, &UK2Node_MakeSet::CanResetToWildcard)
			)
		);

		Context.MenuBuilder->EndSection();
	}
}

void UK2Node_MakeSet::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
	UEdGraphPin* OutputPin = GetOutputPin();
	if (!ensure(Schema) || !ensure(OutputPin) || Schema->IsExecPin(*OutputPin))
	{
		MessageLog.Error(*NSLOCTEXT("K2Node", "MakeSet_OutputIsExec", "Unacceptable set type in @@").ToString(), this);
	}
}

FText UK2Node_MakeSet::GetMenuCategory() const
{
	static FNodeTextCache CachedCategory;
	if (CachedCategory.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedCategory.SetCachedText(FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Utilities, LOCTEXT("ActionMenuCategory", "Set")), this);
	}
	return CachedCategory;
}

#undef LOCTEXT_NAMESPACE
