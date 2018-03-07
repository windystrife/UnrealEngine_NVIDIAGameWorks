// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "Materials/Material.h"
#include "MaterialGraph.generated.h"

class UMaterialExpressionComment;

DECLARE_DELEGATE_RetVal( bool, FRealtimeStateGetter );
DECLARE_DELEGATE( FSetMaterialDirty );
DECLARE_DELEGATE_OneParam( FToggleExpressionCollapsed, UMaterialExpression* );

/**
 * A human-readable name - material expression input pair.
 */
struct FMaterialInputInfo
{
	/** Constructor */
	FMaterialInputInfo()
	{
	}

	/** Constructor */
	FMaterialInputInfo(const FText& InName, EMaterialProperty InProperty, const FText& InToolTip)
		:	Name( InName )
		,	Property( InProperty )
		,	ToolTip( InToolTip )
	{
	}

	FExpressionInput& GetExpressionInput(UMaterial* Material) const
	{
		check(Material);

		auto Ret = Material->GetExpressionInputForProperty(Property);

		// if this happens UMaterialGraph::RebuildGraph() registered something it shouldn't
		check(Ret);

		return *Ret;
	}

	bool IsVisiblePin(const UMaterial* Material, bool bIgnoreMaterialAttributes = false) const
	{
		if(Material->bUseMaterialAttributes && !bIgnoreMaterialAttributes)
		{
			return Property == MP_MaterialAttributes;
		}
		else if( Material->IsUIMaterial() )
		{
			if( Property == MP_EmissiveColor || Property == MP_Opacity || Property == MP_OpacityMask || Property == MP_WorldPositionOffset )
			{
				return true;
			}

			if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
			{
				return (Property - MP_CustomizedUVs0) < Material->NumCustomizedUVs;
			}

			return false;

		}
		else
		{
			if(Property == MP_MaterialAttributes)
			{
				return false;
			}

			if(Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
			{
				return (Property - MP_CustomizedUVs0) < Material->NumCustomizedUVs;
			}

			return true;
		}
	}

	const FText& GetName() const { return Name; }

	EMaterialProperty GetProperty() const { return Property; }

	const FText& GetToolTip() const { return ToolTip; }

private:
	/** Name of the input shown to user */
	FText Name;
	/** Type of the input */
	EMaterialProperty Property;
	/** The tool-tip describing this input's purpose */
	FText ToolTip;

};

UCLASS()
class UNREALED_API UMaterialGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	/** Material this Graph represents */
	UPROPERTY()
	class UMaterial*				Material;

	/** Material Function this Graph represents (NULL for Materials) */
	UPROPERTY()
	class UMaterialFunction*		MaterialFunction;

	/** Root node representing Material inputs (NULL for Material Functions) */
	UPROPERTY()
	class UMaterialGraphNode_Root*	RootNode;

	/** List of Material Inputs (not set up for Material Functions) */
	TArray<FMaterialInputInfo> MaterialInputs;

	/** Checks if Material Editor is in realtime mode, so we update SGraphNodes every frame */
	FRealtimeStateGetter RealtimeDelegate;

	/** Marks the Material Editor as dirty so that user prompted to apply change */
	FSetMaterialDirty MaterialDirtyDelegate;

	/** Toggles the bCollapsed flag of a material expression and updates material editor */
	FToggleExpressionCollapsed ToggleCollapsedDelegate;

	/** The name of the material that we are editing */
	UPROPERTY()
	FString	OriginalMaterialFullName;

public:
	/**
	 * Completely rebuild the graph from the material, removing all old nodes
	 */
	void RebuildGraph();

	/**
	 * Add an Expression to the Graph
	 *
	 * @param	Expression	Expression to add
	 *
	 * @return	UMaterialGraphNode*	Newly created Graph node to represent expression
	 */
	class UMaterialGraphNode*			AddExpression(UMaterialExpression* Expression);

	/**
	 * Add a Comment to the Graph
	 *
	 * @param	Comment			Comment to add
	 * @param	bIsUserInvoked	Whether the comment has just been created by the user via the UI
	 *
	 * @return	UMaterialGraphNode_Comment*	Newly created Graph node to represent comment
	 */
	class UMaterialGraphNode_Comment*	AddComment(UMaterialExpressionComment* Comment, bool bIsUserInvoked = false);

	/** Link all of the Graph nodes using the Material's connections */
	void LinkGraphNodesFromMaterial();

	/** Link the Material using the Graph node's connections */
	void LinkMaterialExpressionsFromGraph() const;

	/**
	 * Check whether a material input should be marked as active
	 *
	 * @param	GraphPin	Pin representing the material input
	 */
	bool IsInputActive(class UEdGraphPin* GraphPin) const;

	/**
	 * Get a list of nodes representing expressions that are not used in the Material
	 *
	 * @param	UnusedNodes	Array to contain nodes representing unused expressions
	 */
	void GetUnusedExpressions(TArray<class UEdGraphNode*>& UnusedNodes) const;

private:
	/**
	 * Remove all Nodes from the graph
	 */
	void RemoveAllNodes();

	/**
	 * Gets a valid output index, matching mask values if necessary
	 *
	 * @param	Input	Input we are finding an output index for
	 */
	int32 GetValidOutputIndex(FExpressionInput* Input) const;

	FText GetEmissivePinName() const;
	FText GetBaseColorPinName() const;
	FText GetOpacityPinName() const;
	FText GetMetallicPinName() const;
	FText GetNormalPinName() const;
	FText GetWorldPositionOffsetPinName() const;
	FText GetSubsurfacePinName() const;
	FText GetCustomDataPinName( uint32 Index ) const;
};
