// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialEditorUtilities.h"
#include "UObject/UObjectHash.h"
#include "EdGraph/EdGraph.h"
#include "Materials/Material.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "IMaterialEditor.h"

#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionReroute.h"

#include "Toolkits/ToolkitManager.h"
#include "MaterialEditor.h"
#include "MaterialExpressionClasses.h"
#include "Materials/MaterialInstance.h"
#include "MaterialUtilities.h"
#include "Misc/ScopedSlowTask.h"
#include "UniquePtr.h"

#define LOCTEXT_NAMESPACE "MaterialEditorUtilities"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialEditorUtilities, Log, All);

UMaterialExpression* FMaterialEditorUtilities::CreateNewMaterialExpression(const class UEdGraph* Graph, UClass* NewExpressionClass, const FVector2D& NodePos, bool bAutoSelect, bool bAutoAssignResource)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if (MaterialEditor.IsValid())
	{
		return MaterialEditor->CreateNewMaterialExpression(NewExpressionClass, NodePos, bAutoSelect, bAutoAssignResource);
	}
	return NULL;
}

UMaterialExpressionComment* FMaterialEditorUtilities::CreateNewMaterialExpressionComment(const class UEdGraph* Graph, const FVector2D& NodePos)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if (MaterialEditor.IsValid())
	{
		return MaterialEditor->CreateNewMaterialExpressionComment(NodePos);
	}
	return NULL;
}

void FMaterialEditorUtilities::ForceRefreshExpressionPreviews(const class UEdGraph* Graph)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->ForceRefreshExpressionPreviews();
	}
}

void FMaterialEditorUtilities::AddToSelection(const class UEdGraph* Graph, UMaterialExpression* Expression)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->AddToSelection(Expression);
	}
}

void FMaterialEditorUtilities::DeleteSelectedNodes(const class UEdGraph* Graph)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->DeleteSelectedNodes();
	}
}


void FMaterialEditorUtilities::DeleteNodes(const class UEdGraph* Graph, const TArray<UEdGraphNode*>& NodesToDelete)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->DeleteNodes(NodesToDelete);
	}
}

FText FMaterialEditorUtilities::GetOriginalObjectName(const class UEdGraph* Graph)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if (MaterialEditor.IsValid())
	{
		return MaterialEditor->GetOriginalObjectName();
	}
	return FText::GetEmpty();
}

void FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(const class UEdGraph* Graph)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->UpdateMaterialAfterGraphChange();
	}
}

bool FMaterialEditorUtilities::CanPasteNodes(const class UEdGraph* Graph)
{
	bool bCanPaste = false;
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if(MaterialEditor.IsValid())
	{
		bCanPaste = MaterialEditor->CanPasteNodes();
	}
	return bCanPaste;
}

void FMaterialEditorUtilities::PasteNodesHere(class UEdGraph* Graph, const FVector2D& Location)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if(MaterialEditor.IsValid())
	{
		MaterialEditor->PasteNodesHere(Location);
	}
}

int32 FMaterialEditorUtilities::GetNumberOfSelectedNodes(const class UEdGraph* Graph)
{
	int32 SelectedNodes = 0;
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if(MaterialEditor.IsValid())
	{
		SelectedNodes = MaterialEditor->GetNumberOfSelectedNodes();
	}
	return SelectedNodes;
}

void FMaterialEditorUtilities::GetMaterialExpressionActions(FGraphActionMenuBuilder& ActionMenuBuilder, bool bMaterialFunction)
{
	// TODO: Not sure if this is necessary/usable anymore
	// Get all menu extenders for this context menu from the material editor module
	/*IMaterialEditorModule& MaterialEditor = FModuleManager::GetModuleChecked<IMaterialEditorModule>( TEXT("MaterialEditor") );
	TArray<IMaterialEditorModule::FMaterialMenuExtender> MenuExtenderDelegates = MaterialEditor.GetAllMaterialCanvasMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(MaterialEditorPtr.Pin()->GetToolkitCommands()));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);*/

	bool bUseUnsortedMenus = false;
	MaterialExpressionClasses* ExpressionClasses = MaterialExpressionClasses::Get();

	if (bUseUnsortedMenus)
	{
		AddMaterialExpressionCategory(ActionMenuBuilder, FText::GetEmpty(), &ExpressionClasses->AllExpressionClasses, bMaterialFunction);
	}
	else
	{
		// Add Favourite expressions as a category
		const FText FavouritesCategory = LOCTEXT("FavoritesMenu", "Favorites");
		AddMaterialExpressionCategory(ActionMenuBuilder, FavouritesCategory, &ExpressionClasses->FavoriteExpressionClasses, bMaterialFunction);

		// Add each category to the menu
		for (int32 CategoryIndex = 0; CategoryIndex < ExpressionClasses->CategorizedExpressionClasses.Num(); ++CategoryIndex)
		{
			FCategorizedMaterialExpressionNode* CategoryNode = &(ExpressionClasses->CategorizedExpressionClasses[CategoryIndex]);
			AddMaterialExpressionCategory(ActionMenuBuilder, CategoryNode->CategoryName, &CategoryNode->MaterialExpressions, bMaterialFunction);
		}

		if (ExpressionClasses->UnassignedExpressionClasses.Num() > 0)
		{
			AddMaterialExpressionCategory(ActionMenuBuilder, FText::GetEmpty(), &ExpressionClasses->UnassignedExpressionClasses, bMaterialFunction);
		}
	}
}

bool FMaterialEditorUtilities::IsMaterialExpressionInFavorites(UMaterialExpression* InExpression)
{
	return MaterialExpressionClasses::Get()->IsMaterialExpressionInFavorites(InExpression);
}

FMaterialRenderProxy* FMaterialEditorUtilities::GetExpressionPreview(const class UEdGraph* Graph, UMaterialExpression* InExpression)
{
	FMaterialRenderProxy* ExpressionPreview = NULL;
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if(MaterialEditor.IsValid())
	{
		ExpressionPreview = MaterialEditor->GetExpressionPreview(InExpression);
	}
	return ExpressionPreview;
}

void FMaterialEditorUtilities::UpdateSearchResults(const class UEdGraph* Graph)
{
	TSharedPtr<class IMaterialEditor> MaterialEditor = GetIMaterialEditorForObject(Graph);
	if(MaterialEditor.IsValid())
	{
		MaterialEditor->UpdateSearch(false);
	}
}

/////////////////////////////////////////////////////
// Static functions moved from SMaterialEditorCanvas

void FMaterialEditorUtilities::GetVisibleMaterialParameters(const UMaterial* Material, UMaterialInstance* MaterialInstance, TArray<FGuid>& VisibleExpressions)
{
	VisibleExpressions.Empty();

	TUniquePtr<FGetVisibleMaterialParametersFunctionState> FunctionState = MakeUnique<FGetVisibleMaterialParametersFunctionState>(nullptr);
	TArray<FGetVisibleMaterialParametersFunctionState*> FunctionStack;
	FunctionStack.Push(FunctionState.Get());

	for(uint32 i = 0; i < MP_MAX; ++i)
	{
		FExpressionInput* ExpressionInput = ((UMaterial *)Material)->GetExpressionInputForProperty((EMaterialProperty)i);

		if(ExpressionInput)
		{
			GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(ExpressionInput->Expression, ExpressionInput->OutputIndex), MaterialInstance, VisibleExpressions, FunctionStack);
		}
	}

	TArray<UMaterialExpressionCustomOutput*> CustomOutputExpressions;
	Material->GetAllCustomOutputExpressions(CustomOutputExpressions);
	for (UMaterialExpressionCustomOutput* Expression : CustomOutputExpressions)
	{
		GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(Expression, 0), MaterialInstance, VisibleExpressions, FunctionStack);
	}
}

bool FMaterialEditorUtilities::GetStaticSwitchExpressionValue(UMaterialInstance* MaterialInstance, UMaterialExpression* SwitchValueExpression, bool& OutValue, FGuid& OutExpressionID, TArray<FGetVisibleMaterialParametersFunctionState*>& FunctionStack)
{
	// Trace any re-route nodes between the input pin and the actual expression
	UMaterialExpression* TracedExpression = SwitchValueExpression;
	if (UMaterialExpressionReroute* Reroute = Cast<UMaterialExpressionReroute>(TracedExpression))
	{
		TracedExpression = Reroute->TraceInputsToRealInput().Expression;
	}

	// If switch value is a function input expression then we must recursively find the associated input expressions from the parent function/material to evaluate the value.
	UMaterialExpressionFunctionInput* FunctionInputExpression =  Cast<UMaterialExpressionFunctionInput>(TracedExpression);
	if(FunctionInputExpression && FunctionInputExpression->InputType == FunctionInput_StaticBool)
	{
		FGetVisibleMaterialParametersFunctionState* TopmostFunctionState = FunctionStack.Pop();
		check(TopmostFunctionState->FunctionCall);
		const TArray<FFunctionExpressionInput>* FunctionInputs = &TopmostFunctionState->FunctionCall->FunctionInputs;

		// Get the FFunctionExpressionInput which stores information about the input node from the parent that this is linked to.
		const FFunctionExpressionInput* MatchingInput = FindInputById(FunctionInputExpression, *FunctionInputs);
		if (MatchingInput && (MatchingInput->Input.Expression || !FunctionInputExpression->bUsePreviewValueAsDefault))
		{
			GetStaticSwitchExpressionValue(MaterialInstance, MatchingInput->Input.Expression, OutValue, OutExpressionID, FunctionStack);
		}
		else
		{
			GetStaticSwitchExpressionValue(MaterialInstance, FunctionInputExpression->Preview.Expression, OutValue, OutExpressionID, FunctionStack);
		}

		FunctionStack.Push(TopmostFunctionState);
	}

	if(TracedExpression)
	{
		UMaterialExpressionStaticBoolParameter* SwitchParamValue = Cast<UMaterialExpressionStaticBoolParameter>(TracedExpression);

		if(SwitchParamValue)
		{
			MaterialInstance->GetStaticSwitchParameterValue(SwitchParamValue->ParameterName, OutValue, OutExpressionID);
			return true;
		}
	}

	UMaterialExpressionStaticBool* StaticSwitchValue = Cast<UMaterialExpressionStaticBool>(TracedExpression);
	if(StaticSwitchValue)
	{
		OutValue = StaticSwitchValue->Value;
		return true;
	}

	return false;
}

bool FMaterialEditorUtilities::IsFunctionContainingSwitchExpressions(UMaterialFunction* MaterialFunction)
{
	if (MaterialFunction)
	{
		TArray<UMaterialFunction*> DependentFunctions;
		MaterialFunction->GetDependentFunctions(DependentFunctions);
		DependentFunctions.AddUnique(MaterialFunction);
		for (int32 FunctionIndex = 0; FunctionIndex < DependentFunctions.Num(); ++FunctionIndex)
		{
			UMaterialFunction* CurrentFunction = DependentFunctions[FunctionIndex];
			for(int32 ExpressionIndex = 0; ExpressionIndex < CurrentFunction->FunctionExpressions.Num(); ++ExpressionIndex )
			{
				UMaterialExpressionStaticSwitch* StaticSwitchExpression = Cast<UMaterialExpressionStaticSwitch>(CurrentFunction->FunctionExpressions[ExpressionIndex]);
				if (StaticSwitchExpression)
				{
					return true;
				}
			}
		}
	}

	return false;
}

const FFunctionExpressionInput* FMaterialEditorUtilities::FindInputById(const UMaterialExpressionFunctionInput* InputExpression, const TArray<FFunctionExpressionInput>& Inputs)
{
	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); InputIndex++)
	{
		const FFunctionExpressionInput& CurrentInput = Inputs[InputIndex];
		if (CurrentInput.ExpressionInputId == InputExpression->Id && CurrentInput.ExpressionInput->GetOuter() == InputExpression->GetOuter())
		{
			return &CurrentInput;
		}
	}
	return NULL;
}

void FMaterialEditorUtilities::InitExpressions(UMaterial* Material)
{
	FString ParmName;

	Material->EditorComments.Empty();
	Material->Expressions.Empty();

	TArray<UObject*> ChildObjects;
	GetObjectsWithOuter(Material, ChildObjects, /*bIncludeNestedObjects=*/false);

	for ( int32 ChildIdx = 0; ChildIdx < ChildObjects.Num(); ++ChildIdx )
	{
		UMaterialExpression* MaterialExpression = Cast<UMaterialExpression>(ChildObjects[ChildIdx]);
		if( MaterialExpression != NULL && !MaterialExpression->IsPendingKill() )
		{
			// Comment expressions are stored in a separate list.
			if ( MaterialExpression->IsA( UMaterialExpressionComment::StaticClass() ) )
			{
				Material->EditorComments.Add( static_cast<UMaterialExpressionComment*>(MaterialExpression) );
			}
			else
			{
				Material->Expressions.Add( MaterialExpression );
			}
		}
	}

	Material->BuildEditorParameterList();

	// Propagate RF_Transactional to all referenced material expressions.
	Material->SetFlags( RF_Transactional );
	for( int32 MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Material->Expressions.Num() ; ++MaterialExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions[ MaterialExpressionIndex ];

		if(MaterialExpression)
		{
			MaterialExpression->SetFlags( RF_Transactional );
		}
	}
	for( int32 MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Material->EditorComments.Num() ; ++MaterialExpressionIndex )
	{
		UMaterialExpressionComment* Comment = Material->EditorComments[ MaterialExpressionIndex ];
		Comment->SetFlags( RF_Transactional );
	}
}

///////////
// private

void FMaterialEditorUtilities::GetVisibleMaterialParametersFromExpression(
	FMaterialExpressionKey MaterialExpressionKey, 
	UMaterialInstance* MaterialInstance, 
	TArray<FGuid>& VisibleExpressions, 
	TArray<FGetVisibleMaterialParametersFunctionState*>& FunctionStack)
{
	if (!MaterialExpressionKey.Expression)
	{
		return;
	}

	check(MaterialInstance);

	// Bail if we already parsed this expression
	if (FunctionStack.Top()->VisitedExpressions.Contains(MaterialExpressionKey))
	{
		return;
	}

	FunctionStack.Top()->VisitedExpressions.Add(MaterialExpressionKey);
	FunctionStack.Top()->ExpressionStack.Push(MaterialExpressionKey);
	const int32 FunctionDepth = FunctionStack.Num();

	{
		// if it's a material parameter it must be visible so add it to the map
		UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>( MaterialExpressionKey.Expression );
		UMaterialExpressionTextureSampleParameter* TexParam = Cast<UMaterialExpressionTextureSampleParameter>( MaterialExpressionKey.Expression );
		UMaterialExpressionFontSampleParameter* FontParam = Cast<UMaterialExpressionFontSampleParameter>( MaterialExpressionKey.Expression );
		if (Param)
		{
			VisibleExpressions.AddUnique(Param->ExpressionGUID);

			UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>( MaterialExpressionKey.Expression );
			UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>( MaterialExpressionKey.Expression );
			TArray<FName> Names;
			TArray<FGuid> Ids;
			if (ScalarParam)
			{
				MaterialInstance->GetMaterial()->GetAllScalarParameterNames( Names, Ids );
				for( int32 i = 0; i < Names.Num(); i++ )
				{
					if( Names[i] == ScalarParam->ParameterName )
					{
						VisibleExpressions.AddUnique( Ids[ i ] );
					}
				}
			}
			else if (VectorParam)
			{
				MaterialInstance->GetMaterial()->GetAllVectorParameterNames( Names, Ids );
				for( int32 i = 0; i < Names.Num(); i++ )
				{
					if( Names[i] == VectorParam->ParameterName )
					{
						VisibleExpressions.AddUnique( Ids[ i ] );
					}
				}
			}
		}
		else if (TexParam)
		{
			VisibleExpressions.AddUnique( TexParam->ExpressionGUID );
			TArray<FName> Names;
			TArray<FGuid> Ids;
			MaterialInstance->GetMaterial()->GetAllTextureParameterNames( Names, Ids );
			for( int32 i = 0; i < Names.Num(); i++ )
			{
				if( Names[i] == TexParam->ParameterName )
				{
					VisibleExpressions.AddUnique( Ids[ i ] );
				}
			}
		}
		else if (FontParam)
		{
			VisibleExpressions.AddUnique( FontParam->ExpressionGUID );
			TArray<FName> Names;
			TArray<FGuid> Ids;
			MaterialInstance->GetMaterial()->GetAllFontParameterNames( Names, Ids );
			for( int32 i = 0; i < Names.Num(); i++ )
			{
				if( Names[i] == FontParam->ParameterName )
				{
					VisibleExpressions.AddUnique( Ids[ i ] );
				}
			}
		}
	}

	// check if it's a switch expression and branch according to its value
	UMaterialExpressionStaticSwitchParameter* StaticSwitchParamExpression = Cast<UMaterialExpressionStaticSwitchParameter>(MaterialExpressionKey.Expression);
	UMaterialExpressionStaticSwitch* StaticSwitchExpression = Cast<UMaterialExpressionStaticSwitch>(MaterialExpressionKey.Expression);
	UMaterialExpressionMaterialFunctionCall* FunctionCallExpression = Cast<UMaterialExpressionMaterialFunctionCall>(MaterialExpressionKey.Expression);
	UMaterialExpressionFunctionInput* FunctionInputExpression = Cast<UMaterialExpressionFunctionInput>(MaterialExpressionKey.Expression);

	if (StaticSwitchParamExpression)
	{
		bool Value = false;
		FGuid ExpressionID;
		MaterialInstance->GetStaticSwitchParameterValue(StaticSwitchParamExpression->ParameterName, Value, ExpressionID);
		VisibleExpressions.AddUnique(ExpressionID);

		if (Value)
		{
			GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(StaticSwitchParamExpression->A.Expression, StaticSwitchParamExpression->A.OutputIndex), MaterialInstance, VisibleExpressions, FunctionStack);
		}
		else
		{
			GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(StaticSwitchParamExpression->B.Expression, StaticSwitchParamExpression->B.OutputIndex), MaterialInstance, VisibleExpressions, FunctionStack);
		}
	}
	else if (StaticSwitchExpression)
	{
		bool bValue = StaticSwitchExpression->DefaultValue;
		FGuid ExpressionID;

		if (StaticSwitchExpression->Value.Expression)
		{
			GetStaticSwitchExpressionValue(MaterialInstance, StaticSwitchExpression->Value.Expression, bValue, ExpressionID, FunctionStack);

			if (ExpressionID.IsValid())
			{
				VisibleExpressions.AddUnique(ExpressionID);
			}
		}

		if(bValue)
		{
			GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(StaticSwitchExpression->A.Expression, StaticSwitchExpression->A.OutputIndex), MaterialInstance, VisibleExpressions, FunctionStack);
		}
		else
		{
			GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(StaticSwitchExpression->B.Expression, StaticSwitchExpression->B.OutputIndex), MaterialInstance, VisibleExpressions, FunctionStack);
		}
	}
	else if (FunctionCallExpression)
	{
		if (FunctionCallExpression->MaterialFunction)
		{
			for (int32 FunctionCallIndex = 0; FunctionCallIndex < FunctionStack.Num(); FunctionCallIndex++)
			{
				checkSlow(FunctionStack[FunctionCallIndex]->FunctionCall != FunctionCallExpression);
			}

			TUniquePtr<FGetVisibleMaterialParametersFunctionState> NewFunctionState = MakeUnique<FGetVisibleMaterialParametersFunctionState>(FunctionCallExpression);
			FunctionStack.Push(NewFunctionState.Get());
			
			GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(FunctionCallExpression->FunctionOutputs[MaterialExpressionKey.OutputIndex].ExpressionOutput, 0), MaterialInstance, VisibleExpressions, FunctionStack);
		
			check(FunctionStack.Top()->ExpressionStack.Num() == 0);
			FunctionStack.Pop();
		}
	}
	else if (FunctionInputExpression)
	{
		GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(FunctionInputExpression->Preview.Expression, FunctionInputExpression->Preview.OutputIndex), MaterialInstance, VisibleExpressions, FunctionStack);
		
		FGetVisibleMaterialParametersFunctionState* FunctionState = FunctionStack.Pop();

		const FFunctionExpressionInput* MatchingInput = FindInputById(FunctionInputExpression, FunctionState->FunctionCall->FunctionInputs);
		check(MatchingInput);

		GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(MatchingInput->Input.Expression, MatchingInput->Input.OutputIndex), MaterialInstance, VisibleExpressions, FunctionStack);

		FunctionStack.Push(FunctionState);
	}
	else
	{
		const TArray<FExpressionInput*>& ExpressionInputs = MaterialExpressionKey.Expression->GetInputs();
		for (int32 ExpressionInputIndex = 0; ExpressionInputIndex < ExpressionInputs.Num(); ExpressionInputIndex++)
		{
			//retrieve the expression input and then start parsing its children
			FExpressionInput* Input = ExpressionInputs[ExpressionInputIndex];
			GetVisibleMaterialParametersFromExpression(FMaterialExpressionKey(Input->Expression, Input->OutputIndex), MaterialInstance, VisibleExpressions, FunctionStack);
		}
	}

	FMaterialExpressionKey TopExpressionKey = FunctionStack.Top()->ExpressionStack.Pop();
	check(FunctionDepth == FunctionStack.Num());
	//ensure that the top of the stack matches what we expect (the same as MaterialExpressionKey)
	check(MaterialExpressionKey == TopExpressionKey);
}

TSharedPtr<class IMaterialEditor> FMaterialEditorUtilities::GetIMaterialEditorForObject(const UObject* ObjectToFocusOn)
{
	check(ObjectToFocusOn);

	// Find the associated Material
	UMaterial* Material = Cast<UMaterial>(ObjectToFocusOn->GetOuter());

	TSharedPtr<IMaterialEditor> MaterialEditor;
	if (Material != NULL)
	{
		TSharedPtr< IToolkit > FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(Material);
		if (FoundAssetEditor.IsValid())
		{
			MaterialEditor = StaticCastSharedPtr<IMaterialEditor>(FoundAssetEditor);
		}
	}
	return MaterialEditor;
}

void FMaterialEditorUtilities::AddMaterialExpressionCategory(FGraphActionMenuBuilder& ActionMenuBuilder, FText CategoryName, TArray<struct FMaterialExpression>* MaterialExpressions, bool bMaterialFunction)
{
	// Get type of dragged pin
	uint32 FromPinType = 0;
	if (ActionMenuBuilder.FromPin)
	{
		FromPinType = UMaterialGraphSchema::GetMaterialValueType(ActionMenuBuilder.FromPin);
	}

	for (int32 Index = 0; Index < MaterialExpressions->Num(); ++Index)
	{
		const FMaterialExpression& MaterialExpression = (*MaterialExpressions)[Index];
		if (IsAllowedExpressionType(MaterialExpression.MaterialClass, bMaterialFunction))
		{
			if (!ActionMenuBuilder.FromPin || HasCompatibleConnection(MaterialExpression.MaterialClass, FromPinType, ActionMenuBuilder.FromPin->Direction, bMaterialFunction))
			{
				FText CreationName = FText::FromString(MaterialExpression.Name);
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Name"), CreationName);
				FText ToolTip = FText::Format(LOCTEXT("NewMaterialExpressionTooltip", "Adds a {Name} node here"), Arguments);
				if (!MaterialExpression.CreationDescription.IsEmpty())
				{
					ToolTip = MaterialExpression.CreationDescription;
				}
				if (!MaterialExpression.CreationName.IsEmpty())
				{
					CreationName = MaterialExpression.CreationName;
				}

				TSharedPtr<FMaterialGraphSchemaAction_NewNode> NewNodeAction(new FMaterialGraphSchemaAction_NewNode(
					CategoryName,
					CreationName,
					ToolTip, 0, CastChecked<UMaterialExpression>(MaterialExpression.MaterialClass->GetDefaultObject())->GetKeywords()));
				NewNodeAction->MaterialExpressionClass = MaterialExpression.MaterialClass;
				ActionMenuBuilder.AddAction(NewNodeAction);
			}
		}
	}
}

bool FMaterialEditorUtilities::HasCompatibleConnection(UClass* ExpressionClass, uint32 TestType, EEdGraphPinDirection TestDirection, bool bMaterialFunction)
{
	if (TestType != 0)
	{
		UMaterialExpression* DefaultExpression = CastChecked<UMaterialExpression>(ExpressionClass->GetDefaultObject());
		if (TestDirection == EGPD_Output)
		{
			int32 NumInputs = DefaultExpression->GetInputs().Num();
			for (int32 Index = 0; Index < NumInputs; ++Index)
			{
				uint32 InputType = DefaultExpression->GetInputType(Index);
				if (CanConnectMaterialValueTypes(InputType, TestType))
				{
					return true;
				}
			}
		}
		else
		{
			int32 NumOutputs = DefaultExpression->GetOutputs().Num();
			for (int32 Index = 0; Index < NumOutputs; ++Index)
			{
				uint32 OutputType = DefaultExpression->GetOutputType(Index);
				if (CanConnectMaterialValueTypes(TestType, OutputType))
				{
					return true;
				}
			}
		}
		
		if (bMaterialFunction)
		{
			// Specific test as Default object won't have texture input
			if (ExpressionClass == UMaterialExpressionTextureSample::StaticClass() && TestType & MCT_Texture && TestDirection == EGPD_Output)
			{
				return true;
			}
			// Always allow creation of new inputs as they can take any type
			else if (ExpressionClass == UMaterialExpressionFunctionInput::StaticClass())
			{
				return true;
			}
			// Allow creation of outputs for floats and material attributes
			else if (ExpressionClass == UMaterialExpressionFunctionOutput::StaticClass() && TestType & (MCT_Float|MCT_MaterialAttributes))
			{
				return true;
			}
		}
	}

	return false;
}

void FMaterialEditorUtilities::BuildTextureStreamingData(UMaterialInterface* UpdatedMaterial)
{
	const EMaterialQualityLevel::Type QualityLevel = EMaterialQualityLevel::High;
	const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;

	if (UpdatedMaterial)
	{
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		FScopedSlowTask SlowTask(2.f, (LOCTEXT("MaterialEditorUtilities_UpdatingTextureStreamingData", "Updating Texture Streaming Data")));
		SlowTask.MakeDialog(true);

		TSet<UMaterialInterface*> Materials;
		Materials.Add(UpdatedMaterial);

		// Clear the build data.
		const TArray<FMaterialTextureInfo> EmptyTextureStreamingData;

		// Here we also update the parents as we just want to save the delta between the hierarchy.
		// Since the instance may only override partially the parent params, we try to find what the child has overridden.
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(UpdatedMaterial);
		while (MaterialInstance)
		{
			// Clear the data in case the build is canceled.
			MaterialInstance->SetTextureStreamingData(EmptyTextureStreamingData);
			Materials.Add(MaterialInstance);
			MaterialInstance = Cast<UMaterialInstance>(MaterialInstance->Parent);
		};

		// Here we need a full rebuild since the shader changed. Although don't wait for previous shaders to fasten the process.
		if (CompileDebugViewModeShaders(DVSM_OutputMaterialTextureScales, QualityLevel, FeatureLevel, true, false, Materials, SlowTask))
		{
			FMaterialUtilities::FExportErrorManager ExportErrors(FeatureLevel);
			for (UMaterialInterface* MaterialInterface : Materials)
			{
				FMaterialUtilities::ExportMaterialUVDensities(MaterialInterface, QualityLevel, FeatureLevel, ExportErrors);
			}
			ExportErrors.OutputToLog();

			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
		}
	}
}

#undef LOCTEXT_NAMESPACE