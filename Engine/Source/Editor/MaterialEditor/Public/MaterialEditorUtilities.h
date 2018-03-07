// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "EdGraph/EdGraphNode.h"
#include "MaterialShared.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

class IMaterialEditor;
class UMaterial;
class UMaterialExpressionComment;
class UMaterialExpressionFunctionInput;
class UMaterialFunction;
class UMaterialInstance;
class UMaterialInterface;
struct FGraphActionMenuBuilder;

//////////////////////////////////////////////////////////////////////////
// FMaterialEditorUtilities

class FGetVisibleMaterialParametersFunctionState
{
public:

	FGetVisibleMaterialParametersFunctionState(UMaterialExpressionMaterialFunctionCall* InFunctionCall) :
		FunctionCall(InFunctionCall)
	{}

	class UMaterialExpressionMaterialFunctionCall* FunctionCall;
	TArray<FMaterialExpressionKey> ExpressionStack;
	TSet<FMaterialExpressionKey> VisitedExpressions;
};

class MATERIALEDITOR_API FMaterialEditorUtilities
{
public:
	/**
	 * Creates a new material expression of the specified class on the material represented by a graph.
	 *
	 * @param	Graph				Graph representing a material.
	 * @param	NewExpressionClass	The type of material expression to add.  Must be a child of UMaterialExpression.
	 * @param	NodePos				Position of the new node.
	 * @param	bAutoSelect			If true, deselect all expressions and select the newly created one.
	 * @param	bAutoAssignResource	If true, assign resources to new expression.
	 */
	static UMaterialExpression* CreateNewMaterialExpression(const class UEdGraph* Graph, UClass* NewExpressionClass, const FVector2D& NodePos, bool bAutoSelect, bool bAutoAssignResource);

	/**
	 * Creates a new material expression comment on the material represented by a graph.
	 *
	 * @param	Graph	Graph to add comment to
	 * @param	NodePos	Position to place new comment at
	 *
	 * @return	UMaterialExpressionComment*	Newly created comment
	 */
	static UMaterialExpressionComment* CreateNewMaterialExpressionComment(const class UEdGraph* Graph, const FVector2D& NodePos);

	/**
	 * Refreshes all material expression previews, regardless of whether or not realtime previews are enabled.
	 *
	 * @param	Graph	Graph representing a material.
	 */
	static void ForceRefreshExpressionPreviews(const class UEdGraph* Graph);

	/**
	 * Add the specified object to the list of selected objects
	 *
	 * @param	Graph	Graph representing a material.
	 * @param	Obj		Object to add to selection.
	 */
	static void AddToSelection(const class UEdGraph* Graph, UMaterialExpression* Expression);

	/**
	 * Disconnects and removes the selected material graph nodes.
	 *
	 * @param	Graph	Graph representing a material.
	 */
	static void DeleteSelectedNodes(const class UEdGraph* Graph);

	/**
	 * Delete the specified nodes from the graph.
	 * @param Graph Graph representing the material.
	 * @param NodesToDelete Array of nodes to be removed from the graph.
	*/
	static void DeleteNodes(const class UEdGraph* Graph, const TArray<UEdGraphNode*>& NodesToDelete);


	/**
	 * Gets the name of the material or material function that we are editing
	 *
	 * @param	Graph	Graph representing a material or material function.
	 */
	static FText GetOriginalObjectName(const class UEdGraph* Graph);

	/**
	 * Re-links the material and updates its representation in the editor
	 *
	 * @param	Graph	Graph representing a material or material function.
	 */
	static void UpdateMaterialAfterGraphChange(const class UEdGraph* Graph);

	/** Can we paste to this graph? */
	static bool CanPasteNodes(const class UEdGraph* Graph);

	/** Perform paste on graph, at location */
	static void  PasteNodesHere(class UEdGraph* Graph, const FVector2D& Location);

	/** Gets the number of selected nodes on this graph */
	static int32 GetNumberOfSelectedNodes(const class UEdGraph* Graph);

	/**
	 * Get all actions for placing material expressions
	 *
	 * @param [in,out]	ActionMenuBuilder	The output menu builder.
	 * @param bMaterialFunction				Whether we're dealing with a Material Function
	 */
	static void GetMaterialExpressionActions(FGraphActionMenuBuilder& ActionMenuBuilder, bool bMaterialFunction);

	/**
	 * Check whether an expression is in the favourites list
	 *
	 * @param	InExpression	The Expression we are checking
	 */
	static bool IsMaterialExpressionInFavorites(UMaterialExpression* InExpression);

	/**
	 * Get a preview material render proxy for an expression
	 *
	 * @param	Graph			Graph representing material
	 * @param	InExpression	Expression we want preview for
	 *
	 * @return	FMaterialRenderProxy*	Preview for this expression or NULL
	 */
	static FMaterialRenderProxy* GetExpressionPreview(const class UEdGraph* Graph, UMaterialExpression* InExpression);

	/**
	 * Updates the material editor search results
	 *
	 * @param	Graph			Graph representing material
	 */
	static void UpdateSearchResults(const class UEdGraph* Graph);

	/////////////////////////////////////////////////////
	// Static functions moved from SMaterialEditorCanvas

	/**
	 * Retrieves all visible parameters within the material.
	 *
	 * @param	Material			The material to retrieve the parameters from.
	 * @param	MaterialInstance	The material instance that contains all parameter overrides.
	 * @param	VisisbleExpressions	The array that will contain the id's of the visible parameter expressions.
	 */
	static void GetVisibleMaterialParameters(const UMaterial *Material, UMaterialInstance *MaterialInstance, TArray<FGuid> &VisibleExpressions);

	/** Returns true if the function or dependent functions contain a static switch expression */
	static bool IsFunctionContainingSwitchExpressions(UMaterialFunction* MaterialFunction);

	/** Finds an input in the passed in array with a matching Id. */
	static const FFunctionExpressionInput* FindInputById(const UMaterialExpressionFunctionInput* InputExpression, const TArray<FFunctionExpressionInput>& Inputs);

	/** 
	* Returns the value for a static switch material expression.
	*
	* @param	MaterialInstance		The material instance that contains all parameter overrides.
	* @param	SwitchValueExpression	The switch expression to find the value for.
	* @param	OutValue				The value for the switch expression.
	* @param	OutExpressionID			The Guid of the expression that is input as the switch value.
	* @param	FunctionStack			The current function stack frame.
	* 
	* @return	Returns true if a value for the switch expression is found, otherwise returns false.
	*/
	static bool GetStaticSwitchExpressionValue(UMaterialInstance* MaterialInstance, UMaterialExpression *SwitchValueExpression, bool& OutValue, FGuid& OutExpressionID, TArray<FGetVisibleMaterialParametersFunctionState*>& FunctionStack);

	/**
	 * Populates the specified material's Expressions array (eg if cooked out or old content).
	 * Also ensures materials and expressions are RF_Transactional for undo/redo support.
	 *
	 * @param	Material	Material we are initializing
	 */
	static void InitExpressions(UMaterial* Material);

	/**
	 * Build the texture streaming data for a given material. Also update the parent hierarchy has only the delta are stored.
	 *
	 * @param	MaterialInterface	The material to update.
	 */
	static void BuildTextureStreamingData(UMaterialInterface* MaterialInterface);

	/** Get IMaterialEditor for given object, if it exists */
	static TSharedPtr<class IMaterialEditor> GetIMaterialEditorForObject(const UObject* ObjectToFocusOn);

private:

	/**
	 * Recursively walks the expression tree and parses the visible expression branches.
	 *
	 * @param	MaterialExpression				The expression to parse.
	 * @param	MaterialInstance				The material instance that contains all parameter overrides.
	 * @param	VisisbleExpressions				The array that will contain the id's of the visible parameter expressions.
	 */
	static void GetVisibleMaterialParametersFromExpression(
		FMaterialExpressionKey MaterialExpressionKey, 
		UMaterialInstance* MaterialInstance, 
		TArray<FGuid>& VisibleExpressions, 
		TArray<FGetVisibleMaterialParametersFunctionState*>& FunctionStack);

	/**
	 * Adds a category of Material Expressions to an action builder
	 *
	 * @param	ActionMenuBuilder	The builder to add to.
	 * @param	CategoryName		The name of the category.
	 * @param	MaterialExpressions	List of Material Expressions in the category.
	 * @param	bMaterialFunction	Whether we are building for a material function.
	 */
	static void AddMaterialExpressionCategory(FGraphActionMenuBuilder& ActionMenuBuilder, FText CategoryName, TArray<struct FMaterialExpression>* MaterialExpressions, bool bMaterialFunction);

	/**
	 * Checks whether a Material Expression class has any connections that are compatible with a type/direction
	 *
	 * @param	ExpressionClass		Class of Expression we are testing against.
	 * @param	TestType			Material Value Type we are testing.
	 * @param	TestDirection		Pin Direction we are testing.
	 * @param	bMaterialFunction	Whether we are testing for a material function.
	*/
	static bool HasCompatibleConnection(UClass* ExpressionClass, uint32 TestType, EEdGraphPinDirection TestDirection, bool bMaterialFunction);

	/** Constructor */
	FMaterialEditorUtilities() {}
};
