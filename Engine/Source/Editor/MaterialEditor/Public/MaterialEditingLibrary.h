// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Materials/MaterialInterface.h"
#include "SceneTypes.h"

#include "MaterialEditingLibrary.generated.h"

class UMaterialFunction;
class MaterialInstance;

/** Blueprint library for creating/editing Materials */
UCLASS()
class MATERIALEDITOR_API UMaterialEditingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	*	Create a new material expression node within the supplied material, optionally specifying asset to use
	*	@note	If a MaterialFunction and Material are specified, expression is added to Material and not MaterialFunction, assuming Material is a preview that will be copied to Function later by user.
	*	@param	Material			Material asset to add an expression to
	*	@param	MaterialFunction	Specified if adding an expression to a MaterialFunction, used as Outer for new expression object
	*	@param	SelectedAsset		If specified, new node will attempt to use this asset, if of the appropriate type (e.g. Texture for a TextureSampler)
	*	@param	ExpressionClass		Class of expression to add
	*	@param	NodePosX			X position of new expression node
	*	@param	NodePosY			Y position of new expression node
	*/
	static UMaterialExpression* CreateMaterialExpressionEx(UMaterial* Material, UMaterialFunction* MaterialFunction, TSubclassOf<UMaterialExpression> ExpressionClass, UObject* SelectedAsset = nullptr, int32 NodePosX = 0, int32 NodePosY = 0);

	/**
	*	Rebuilds dependent Material Instance Editors
	*	@param	BaseMaterial	Material that MaterialInstance must be based on for Material Instance Editor to be rebuilt
	*/
	static void RebuildMaterialInstanceEditors(UMaterial* BaseMaterial);

	///////////// MATERIAL EDITING


	/** Returns number of material expressions in the supplied material */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static int32 GetNumMaterialExpressions(const UMaterial* Material);

	/** Delete all material expressions in the supplied material */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void DeleteAllMaterialExpressions(UMaterial* Material);

	/** Delete a specific expression from a material. Will disconnect from other expressions. */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void DeleteMaterialExpression(UMaterial* Material, UMaterialExpression* Expression);

	/** 
	 *	Create a new material expression node within the supplied material 
	 *	@param	Material			Material asset to add an expression to
	 *	@param	ExpressionClass		Class of expression to add
	 *	@param	NodePosX			X position of new expression node
	 *	@param	NodePosY			Y position of new expression node
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static UMaterialExpression* CreateMaterialExpression(UMaterial* Material, TSubclassOf<UMaterialExpression> ExpressionClass, int32 NodePosX=0, int32 NodePosY=0);


	/** 
	 *	Enable a particular usage for the supplied material (e.g. SkeletalMesh, ParticleSprite etc)
	 *	@param	Material			Material to change usage for
	 *	@param	Usage				New usage type to enable for this material
	 *	@param	bNeedsRecompile		Returned to indicate if material needs recompiling after this change
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialUsage(UMaterial* Material, EMaterialUsage Usage, bool& bNeedsRecompile);

	/** 
	 *	Connect a material expression output to one of the material property inputs (e.g. diffuse color, opacity etc)
	 *	@param	FromExpression		Expression to make connection from
	 *	@param	FromOutputName		Name of output of FromExpression to make connection from
	 *	@param	Property			Property input on material to make connection to
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool ConnectMaterialProperty(UMaterialExpression* FromExpression, FString FromOutputName, EMaterialProperty Property);

	/**
	 *	Create connection between two material expressions
	 *	@param	FromExpression		Expression to make connection from
	 *	@param	FromOutputName		Name of output of FromExpression to make connection from. Leave empty to use first output.
	 *	@param	ToExpression		Expression to make connection to
	 *	@param	ToInputName			Name of input of ToExpression to make connection to. Leave empty to use first input.
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool ConnectMaterialExpressions(UMaterialExpression* FromExpression, FString FromOutputName, UMaterialExpression* ToExpression, FString ToInputName);

	/** 
	 *	Trigger a recompile of a material. Must be performed after making changes to the graph to have changes reflected.
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void RecompileMaterial(UMaterial* Material);

	//////// MATERIAL FUNCTION EDITING

	/** Returns number of material expressions in the supplied material */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static int32 GetNumMaterialExpressionsInFunction(const UMaterialFunction* MaterialFunction);

	/**
	*	Create a new material expression node within the supplied material function
	*	@param	MaterialFunction	Material function asset to add an expression to
	*	@param	ExpressionClass		Class of expression to add
	*	@param	NodePosX			X position of new expression node
	*	@param	NodePosY			Y position of new expression node
	*/
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static UMaterialExpression* CreateMaterialExpressionInFunction(UMaterialFunction* MaterialFunction, TSubclassOf<UMaterialExpression> ExpressionClass, int32 NodePosX = 0, int32 NodePosY = 0);

	/** Delete all material expressions in the supplied material function */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void DeleteAllMaterialExpressionsInFunction(UMaterialFunction* MaterialFunction);

	/** Delete a specific expression from a material function. Will disconnect from other expressions. */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void DeleteMaterialExpressionInFunction(UMaterialFunction* MaterialFunction, UMaterialExpression* Expression);

	/**
	 *	Update a Material Function after edits have been made.
	 *	Will recompile any Materials that use the supplied Material Function.
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing", meta = (HidePin = "PreviewMaterial"))
	static void UpdateMaterialFunction(UMaterialFunction* MaterialFunction, UMaterial* PreviewMaterial = nullptr);


	//////// MATERIAL INSTANCE CONSTANT EDITING

	/** Set the parent Material or Material Instance to use for this Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void SetMaterialInstanceParent(UMaterialInstanceConstant* Instance, UMaterialInterface* NewParent);

	/** Clears all material parameters set by this Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void ClearAllMaterialInstanceParameters(UMaterialInstanceConstant* Instance);


	/** Get the current scalar (float) parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static float GetMaterialInstanceScalarParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName);

	/** Set the scalar (float) parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceScalarParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, float Value);


	/** Get the current texture parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static UTexture* GetMaterialInstanceTextureParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName);

	/** Set the texture parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceTextureParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, UTexture* Value);


	/** Get the current vector parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static FLinearColor GetMaterialInstanceVectorParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName);

	/** Set the vector parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceVectorParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, FLinearColor Value);


	/** Called after making modifications to a Material Instance to recompile shaders etc. */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void UpdateMaterialInstance(UMaterialInstanceConstant* Instance);
};