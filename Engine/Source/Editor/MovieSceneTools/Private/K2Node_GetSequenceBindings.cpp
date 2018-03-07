// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "K2Node_GetSequenceBindings.h"
#include "BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyCustomizationHelpers.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "UDEPRECATED_K2Node_GetSequenceBindings"


class FKCHandler_GetSequenceBindings : public FNodeHandlingFunctor
{
public:
	FKCHandler_GetSequenceBindings(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UDEPRECATED_K2Node_GetSequenceBindings* BindingsNode = CastChecked<UDEPRECATED_K2Node_GetSequenceBindings>(Node);
		
		for (UEdGraphPin* Pin : BindingsNode->GetAllPins())
		{
			if (Pin->Direction == EGPD_Output && Pin->LinkedTo.Num())
			{
				FBPTerminal* Term = RegisterLiteral(Context, Pin);
				TOptional<FGuid> Guid = BindingsNode->GetGuidFromPin(Pin);
				if (!Guid.IsSet())
				{
					Context.MessageLog.Warning(TEXT("Invalid Object Binding ID (%%) for node %%."), Pin, Node);
				}
				else
				{
					FMovieSceneObjectBindingID Value(Guid.GetValue(), MovieSceneSequenceID::Root);
					FMovieSceneObjectBindingID::StaticStruct()->ExportText(Term->Name, &Value, nullptr, nullptr, 0, nullptr);
				}
			}
		}
	}
};

void UDEPRECATED_K2Node_GetSequenceBindings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdatePins();
}

TOptional<FGuid> UDEPRECATED_K2Node_GetSequenceBindings::GetGuidFromPin(UEdGraphPin* Pin) const
{
	const FGetSequenceBindingGuidMapping* Mapping = BindingGuids.FindByPredicate(
		[=](const FGetSequenceBindingGuidMapping& InMapping)
		{
			return InMapping.PinName == Pin->PinName;
		}
	);

	return Mapping ? Mapping->Guid : TOptional<FGuid>();
}

void UDEPRECATED_K2Node_GetSequenceBindings::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	// Ensure pins always have a default value
	TOptional<FGuid> FoundGuid = GetGuidFromPin(Pin);
	if (FoundGuid.IsSet())
	{
		FMovieSceneObjectBindingID Value(FoundGuid.GetValue(), MovieSceneSequenceID::Root);
		FMovieSceneObjectBindingID::StaticStruct()->ExportText(Pin->DefaultValue, &Value, nullptr, nullptr, 0, nullptr);
	}

	Super::NotifyPinConnectionListChanged(Pin);
}

void UDEPRECATED_K2Node_GetSequenceBindings::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (Pin && Pin->Direction == EGPD_Input)
	{
		UpdatePins();
	}

	Super::PinDefaultValueChanged(Pin);
}

FNodeHandlingFunctor* UDEPRECATED_K2Node_GetSequenceBindings::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_GetSequenceBindings(CompilerContext);
}

FText UDEPRECATED_K2Node_GetSequenceBindings::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::Format(LOCTEXT("NodeTitle", "Get Sequence Bindings ({0})"), Sequence ? FText::FromName(Sequence->GetFName()) : LOCTEXT("NoSequence", "No Sequence"));
}

void UDEPRECATED_K2Node_GetSequenceBindings::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	UpdatePins();
}

FText UDEPRECATED_K2Node_GetSequenceBindings::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Access all the binding IDs for the specified sequence");
}

void UDEPRECATED_K2Node_GetSequenceBindings::UpdatePins()
{
	PreloadObject(Sequence);

	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	PreloadObject(MovieScene);

	for (int32 Index = Pins.Num() - 1; Index >= 0; --Index)
	{
		UEdGraphPin* Pin = Pins[Index];
		RemovePin(Pin);
	}

	BindingGuids.Reset();

	// Generate all new pins
	if (MovieScene)
	{
		int32 PossessableCount = MovieScene->GetPossessableCount();
		for (int32 Index = 0; Index < PossessableCount; ++Index)
		{
			const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(Index);
			if (!Sequence->CanRebindPossessable(Possessable))
			{
				continue;
			}

			FString GuidString = Possessable.GetGuid().ToString();
			UEdGraphPin* NewPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FString(), FMovieSceneObjectBindingID::StaticStruct(), GuidString);
			NewPin->PinFriendlyName = MovieScene->GetObjectDisplayName(Possessable.GetGuid());
			NewPin->PersistentGuid = Possessable.GetGuid();

			FMovieSceneObjectBindingID Value(Possessable.GetGuid(), MovieSceneSequenceID::Root);
			FMovieSceneObjectBindingID::StaticStruct()->ExportText(NewPin->DefaultValue, &Value, nullptr, nullptr, 0, nullptr);

			BindingGuids.Add(FGetSequenceBindingGuidMapping{ NewPin->PinName, Possessable.GetGuid() });
		}
	}

	const bool bIsCompiling = GetBlueprint()->bBeingCompiled;
	if( !bIsCompiling )
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UDEPRECATED_K2Node_GetSequenceBindings::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);

	if (!Context.bIsDebugging)
	{
		Context.MenuBuilder->BeginSection("K2NodeGetSequenceBindings", LOCTEXT("ThisNodeHeader", "This Node"));
		if (!Context.Pin)
		{
			auto SubMenu = [=](FMenuBuilder& SubMenuBuilder)
				{
					TArray<const UClass*> AllowedClasses({ UMovieSceneSequence::StaticClass() });

					TSharedRef<SWidget> MenuContent = PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
						FAssetData(Sequence),
						true /* bAllowClear */,
						AllowedClasses,
						PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(AllowedClasses),
						FOnShouldFilterAsset(),
						FOnAssetSelected::CreateUObject(this, &UDEPRECATED_K2Node_GetSequenceBindings::SetSequence),
						FSimpleDelegate());
					
					SubMenuBuilder.AddWidget(MenuContent, FText::GetEmpty(), false);
				};

			Context.MenuBuilder->AddSubMenu(
				LOCTEXT("SetSequence_Text", "Sequence"),
				LOCTEXT("SetSequence_ToolTip", "Sets the sequence to extract bindings from"),
				FNewMenuDelegate::CreateLambda(SubMenu)
				);

			Context.MenuBuilder->AddMenuEntry(
				LOCTEXT("Refresh_Text", "Refresh"),
				LOCTEXT("Refresh_ToolTip", "Refresh this node's bindings"),
				FSlateIcon(),
				FUIAction(
						FExecuteAction::CreateUObject(this, &UDEPRECATED_K2Node_GetSequenceBindings::ReconstructNode)
					)
				);
		}

		Context.MenuBuilder->EndSection();
	}
}

void UDEPRECATED_K2Node_GetSequenceBindings::SetSequence(const FAssetData& InAssetData)
{
	FSlateApplication::Get().DismissAllMenus();
	Sequence = Cast<UMovieSceneSequence>(InAssetData.GetAsset());
	UpdatePins();
}

void UDEPRECATED_K2Node_GetSequenceBindings::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UDEPRECATED_K2Node_GetSequenceBindings::GetMenuCategory() const
{
	static FNodeTextCache CachedCategory;
	if (CachedCategory.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedCategory.SetCachedText(FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Utilities, LOCTEXT("ActionMenuCategory", "Sequence")), this);
	}
	return CachedCategory;
}

#undef LOCTEXT_NAMESPACE
