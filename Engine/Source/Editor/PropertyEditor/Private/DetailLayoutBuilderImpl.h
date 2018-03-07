// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IPropertyUtilities.h"
#include "PropertyNode.h"
#include "PropertyHandle.h"
#include "IDetailsViewPrivate.h"
#include "SDetailsViewBase.h"
#include "DetailLayoutBuilder.h"

class FDetailCategoryImpl;
class IDetailCategoryBuilder;
class IPropertyUtilities;

class FDetailLayoutBuilderImpl : public IDetailLayoutBuilder, public TSharedFromThis<FDetailLayoutBuilderImpl>
{
public:
	FDetailLayoutBuilderImpl(
		TSharedPtr<FComplexPropertyNode>& InRootNode,
		FClassToPropertyMap& InPropertyMap,
		const TSharedRef<IPropertyUtilities>& InPropertyUtilities,
		const TSharedPtr<IDetailsViewPrivate>& InDetailsView);

	/** IDetailLayoutBuilder Interface */
	virtual const IDetailsView* GetDetailsView() const override;
	virtual void GetObjectsBeingCustomized(TArray< TWeakObjectPtr<UObject> >& OutObjects) const override;
	virtual void GetStructsBeingCustomized(TArray< TSharedPtr<FStructOnScope> >& OutStructs) const override;
	virtual IDetailCategoryBuilder& EditCategory(FName CategoryName, const FText& NewLocalizedDisplayName = FText::GetEmpty(), ECategoryPriority::Type CategoryType = ECategoryPriority::Default) override;
	virtual IDetailPropertyRow& AddPropertyToCategory(TSharedPtr<IPropertyHandle> InPropertyHandle) override;
	virtual FDetailWidgetRow& AddCustomRowToCategory(TSharedPtr<IPropertyHandle> InPropertyHandle, const FText& InCustomSearchString, bool bForAdvanced = false) override;
	virtual TSharedRef<IPropertyHandle> GetProperty(const FName PropertyPath, const UClass* ClassOutermost, FName InInstanceName) override;
	virtual FName GetTopLevelProperty() override;
	virtual void HideProperty(const TSharedPtr<IPropertyHandle> Property) override;
	virtual void HideProperty(FName PropertyPath, const UClass* ClassOutermost = NULL, FName InstanceName = NAME_None) override;
	virtual void ForceRefreshDetails() override;
	virtual TSharedPtr<FAssetThumbnailPool> GetThumbnailPool() const override;
	virtual bool IsPropertyVisible(TSharedRef<IPropertyHandle> PropertyHandle) const override;
	virtual bool IsPropertyVisible(const struct FPropertyAndParent& PropertyAndParent) const override;
	virtual void HideCategory(FName CategoryName) override;
	virtual const TSharedRef<IPropertyUtilities> GetPropertyUtilities() const override;
	virtual UClass* GetBaseClass() const override;
	virtual const TArray<TWeakObjectPtr<UObject>>& GetSelectedObjects() const override;
	virtual bool HasClassDefaultObject() const override;
	/**
	 * Creates a default category. The SDetails view will generate widgets in default categories
	 *
	 * @param CategoryName	The name of the category to create
	 */
	FDetailCategoryImpl& DefaultCategory(FName CategoryName);

	/**
	* Return true if the category exist
	*/
	bool HasCategory(FName CategoryName);

	/**
	 * Generates the layout for this detail builder
	 */
	void GenerateDetailLayout();

	/**
	 * Filters the layout based on the passed in filter
	 */
	void FilterDetailLayout(const FDetailFilter& InFilter);

	/**
	 * Sets the current class being asked for customization
	 * Used for looking up properties if a class is not provided
	 *
	 * @param CurrentClass	The current class being customized
	 * @param VariableName The variable name of the class being customized (used for inner classes/structs where there can be multiple instances of them)
	 */
	void SetCurrentCustomizationClass(UStruct* CurrentClass, FName VariableName);

	/** @return The current class variable name being customized */
	FName GetCurrentCustomizationVariableName() const { return CurrentCustomizationVariableName; }

	/**
	 * Finds a property node for the current property
	 *
	 * @param PropertyPath	The path to the property
	 * @param ClassOutermost	The outer class of the property
	 * @param bMarkPropertyAsCustomized	Whether or not to mark the property as customized so it does not appear in the default layout
	 * @return The found property node
	 */
	TSharedPtr<FPropertyNode> GetPropertyNode(FName PropertyPath, const UClass* ClassOutermost, FName InstanceName) const;

	/**
	 * Gets the property node from the provided handle
	 *
	 * @param PropertyHandle	The property handle to get the node from
	 * @param bMarkAsCustomized	Whether or not to mark the value as customized
	 */
	TSharedPtr<FPropertyNode> GetPropertyNode(TSharedPtr<IPropertyHandle> PropertyHandle) const;

	/**
	 * Marks a property as customized
	 *
	 * @param PropertyNode	The property node to mark as customized
	 */
	void SetCustomProperty(const TSharedPtr<FPropertyNode>& PropertyNode);

	/**
	 * @return All tree nodes that should be visible in the tree
	 */
	FDetailNodeList& GetFilteredRootTreeNodes() { return FilteredRootTreeNodes; }

	FDetailNodeList& GetAllRootTreeNodes() { return AllRootTreeNodes; }

	/**
	 * @return true if the layout has any details
	 */
	bool HasDetails() const { return AllRootTreeNodes.Num() > 0; }

	/**
	 * Ticks tickable nodes (if any)
	 */
	void Tick(float DeltaTime);

	/**
	 * Adds a node that should be ticked each frame
	 *
	 * @param TickableNode	The node to tick
	 */
	void AddTickableNode(FDetailTreeNode& TickableNode);

	/**
	 * Removes a node that should no longer be ticked each frame
	 *
	 * @param TickableNode	The node to remove
	 */
	void RemoveTickableNode(FDetailTreeNode& TickableNode);

	/**
	 * @return The current filter that is being used to show or hide rows
	 */
	const FDetailFilter& GetCurrentFilter() const { return CurrentFilter; }

	/**
	 * Saves the expansion state of a tree node
	 *
	 * @param NodePath	The path to the detail node to save
	 * @param bIsExpanded	true if the node is expanded, false otherwise
	 */
	void SaveExpansionState(const FString& NodePath, bool bIsExpanded);

	/**
	 * Gets the saved expansion state of a tree node in this category
	 *
	 * @param NodePath	The path to the detail node to get
	 * @return true if the node should be expanded, false otherwise
	 */
	bool GetSavedExpansionState(const FString& NodePath) const;

	/**
	 * Makes a property handle from a property node
	 *
	 * @param PropertyNodePtr	The property node to make a handle for.
	 */
	TSharedRef<IPropertyHandle> GetPropertyHandle(TSharedPtr<FPropertyNode> PropertyNodePtr);

	/**
	 * Adds an external property root node to the list of root nodes that the details new needs to manage
	 *
	 * @param InExternalRootNode		The node to add
	 */
	void AddExternalRootPropertyNode(TSharedRef<FComplexPropertyNode> InExternalRootNode);

	/** @return The details view that owns this layout */
	IDetailsViewPrivate* GetDetailsView() { return DetailsView; }
	/** @return The root node for this customization */
	TSharedPtr<FComplexPropertyNode> GetRootNode() const { return RootNode.Pin(); }

	FRootPropertyNodeList& GetExternalRootPropertyNodes() { return ExternalRootPropertyNodes;  }

	/** @return True if the layout is for an external root property node and not in the main set of objects the details panel is observing */
	bool IsLayoutForExternalRoot() const { return bLayoutForExternalRoot; }

private:
	/**
	 * Finds a property node for the current property by searching in a fast lookup map or a path search if required
	 *
	 * @param PropertyPath	The path to the property
	 * @param ClassOutermost	The outer class of the property
	 * @param bMarkPropertyAsCustomized	Whether or not to mark the property as customized so it does not appear in the default layout
	 * @return The found property node
	 */
	TSharedPtr<FPropertyNode> GetPropertyNodeInternal( FName PropertyPath, const UClass* ClassOutermost, FName InstanceName ) const;

	/**
	 * Builds a list of simple and advanced categories that should be displayed
	 */
	void BuildCategories( const FCategoryMap& CategoryMap, TArray< TSharedRef<FDetailCategoryImpl> >& OutSimpleCategories, TArray< TSharedRef<FDetailCategoryImpl> >& OutAdvancedCategories );

private:
	/** The root property node for this customization */
	TWeakPtr<FComplexPropertyNode> RootNode;
	/** External property nodes which need to validated each tick */
	FRootPropertyNodeList ExternalRootPropertyNodes;
	/** A mapping of category names to categories which have been customized */
	FCategoryMap CustomCategoryMap;
	/** A mapping of category names to categories which have been not customized */
	FCategoryMap DefaultCategoryMap;
	/** A mapping of classes to top level properties in that class */
	FClassToPropertyMap& PropertyMap;
	/** Force hidden categories set by the user */
	TSet<FName> ForceHiddenCategories;
	/** Nodes that require ticking */
	TSet<FDetailTreeNode*> TickableNodes;
	/** Current filter applied to the view */
	FDetailFilter CurrentFilter;
	/** All RootTreeNodes */
	FDetailNodeList AllRootTreeNodes;
	/** All generated category widgets for rebuilding them if needed */
	FDetailNodeList FilteredRootTreeNodes;
	/** The current variable name of the class being customized (inner class instances)*/
	FName CurrentCustomizationVariableName;
	/** The global property utilties.  This is weak to avoid circular ref but it should always be valid if this class exists*/
	const TWeakPtr<class IPropertyUtilities> PropertyDetailsUtilities;
	/** The view where this detail customizer resides */
	class IDetailsViewPrivate* DetailsView;
	/** The current class being customized */
	UStruct* CurrentCustomizationClass;
	/** True if the layout is for an external root property node and not in the main set of objects the details panel is observing */
	bool bLayoutForExternalRoot;
};

