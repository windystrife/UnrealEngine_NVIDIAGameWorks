// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_MakeArray.h"
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

namespace MakeArrayLiterals
{
	static const FString OutputPinName = FString(TEXT("Array"));
};

#define LOCTEXT_NAMESPACE "MakeArrayNode"

/////////////////////////////////////////////////////
// FKCHandler_MakeArray
class FKCHandler_MakeArray : public FKCHandler_MakeContainer
{
public:
	FKCHandler_MakeArray(FKismetCompilerContext& InCompilerContext)
		: FKCHandler_MakeContainer(InCompilerContext)
	{
		CompiledStatementType = KCST_CreateArray;
	}
};

/////////////////////////////////////////////////////
// UK2Node_MakeArray

UK2Node_MakeArray::UK2Node_MakeArray(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ContainerType = EPinContainerType::Array;
}

FNodeHandlingFunctor* UK2Node_MakeArray::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_MakeArray(CompilerContext);
}

FText UK2Node_MakeArray::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Make Array");
}

FString UK2Node_MakeArray::GetOutputPinName() const
{
	return MakeArrayLiterals::OutputPinName;
}

FText UK2Node_MakeArray::GetTooltipText() const
{
	return LOCTEXT("MakeArrayTooltip", "Create an array from a series of items.");
}

FSlateIcon UK2Node_MakeArray::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.MakeArray_16x");
	return Icon;
}

void UK2Node_MakeArray::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);

	if (!Context.bIsDebugging)
	{
		Context.MenuBuilder->BeginSection("K2NodeMakeArray", NSLOCTEXT("K2Nodes", "MakeArrayHeader", "MakeArray"));

		if (Context.Pin != NULL)
		{
			if (Context.Pin->Direction == EGPD_Input && Context.Pin->ParentPin == nullptr)
			{
				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("RemovePin", "Remove array element pin"),
					LOCTEXT("RemovePinTooltip", "Remove this array element pin"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateUObject(this, &UK2Node_MakeArray::RemoveInputPin, const_cast<UEdGraphPin*>(Context.Pin))
					)
				);
			}
		}
		else
		{
			Context.MenuBuilder->AddMenuEntry(
				LOCTEXT("AddPin", "Add array element pin"),
				LOCTEXT("AddPinTooltip", "Add another array element pin"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateUObject(this, &UK2Node_MakeArray::InteractiveAddInputPin)
				)
			);
		}

		Context.MenuBuilder->AddMenuEntry(
			LOCTEXT("ResetToWildcard", "Reset to wildcard"),
			LOCTEXT("ResetToWildcardTooltip", "Reset the node to have wildcard input/outputs. Requires no pins are connected."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateUObject(this, &UK2Node_MakeArray::ClearPinTypeToWildcard),
				FCanExecuteAction::CreateUObject(this, &UK2Node_MakeArray::CanResetToWildcard)
			)
		);

		Context.MenuBuilder->EndSection();
	}
}

void UK2Node_MakeArray::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
	UEdGraphPin* OutputPin = GetOutputPin();
	if (!ensure(Schema) || !ensure(OutputPin) || Schema->IsExecPin(*OutputPin))
	{
		MessageLog.Error(*NSLOCTEXT("K2Node", "MakeArray_OutputIsExec", "Unacceptable array type in @@").ToString(), this);
	}
}

FText UK2Node_MakeArray::GetMenuCategory() const
{
	static FNodeTextCache CachedCategory;
	if (CachedCategory.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedCategory.SetCachedText(FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Utilities, LOCTEXT("ActionMenuCategory", "Array")), this);
	}
	return CachedCategory;
}

#undef LOCTEXT_NAMESPACE
