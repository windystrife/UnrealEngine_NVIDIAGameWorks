// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "PropertyPath.h"

class FComplexPropertyNode;
class FNotifyHook;
class FObjectPropertyNode;
class FPropertyItemValueDataTrackerSlate;
class FPropertyNode;
class FStructurePropertyNode;

DECLARE_LOG_CATEGORY_EXTERN(LogPropertyNode, Log, All);

namespace EPropertyNodeFlags
{
	typedef uint32 Type;

	const Type	IsSeen							= 1 << 0;		/** true if this node can be seen based on current parent expansion.  Does not take into account clipping*/
	const Type	IsSeenDueToFiltering			= 1 << 1;		/** true if this node has been accepted by the filter*/
	const Type	IsSeenDueToChildFiltering		= 1 << 2;		/** true if this node or one of it's children is seen due to filtering.  It will then be forced on as well.*/
	const Type	IsParentSeenDueToFiltering		= 1 << 3;		/** True if the parent was visible due to filtering*/
	const Type	IsSeenDueToChildFavorite		= 1 << 4;		/** True if this node is seen to it having a favorite as a child */
	
	const Type	Expanded						= 1 << 5;		/** true if this node should display its children*/
	const Type	CanBeExpanded					= 1 << 6;		/** true if this node is able to be expanded */

	const Type	EditInlineNew					= 1 << 7;		/** true if the property can be expanded into the property window. */

	const Type	SingleSelectOnly				= 1 << 8;		/** true if only a single object is selected. */
	const Type  ShowCategories					= 1 << 9;		/** true if this node should show categories.  Different*/

	const Type  HasEverBeenExpanded				= 1 << 10;	/** true if expand has ever been called on this node */

	const Type	IsBeingFiltered					= 1 << 11;	/** true if the node is being filtered. If this is true, seen flags should be checked for visibility.  If this is false the node has no filter and is visible */

	const Type  IsFavorite						= 1 << 12;	/** true if this item has been dubbed a favorite by the user */

	const Type  NoChildrenDueToCircularReference= 1 << 13;	/** true if this node has no children (but normally would) due to circular referencing */

	const Type	AutoExpanded					= 1 << 14;	/** true if this node was autoexpanded due to being filtered */
	const Type	ShouldShowHiddenProperties		= 1 << 15;	/** true if this node should all properties not just those with the correct flag(s) to be shown in the editor */
	const Type	IsAdvanced						= 1 << 16;	/** true if the property node is advanced (i.e it only shows up in advanced sections) */
	const Type	IsCustomized					= 1 << 17;	/** true if this node's visual representation has been customized by the editor */
	
	const Type	RequiresValidation				= 1 << 18; /** true if this node could unexpectedly change (array changes, editinlinenew changes) */

	const Type	ShouldShowDisableEditOnInstance = 1 << 19; /** true if this node should show child properties marked CPF_DisableEditOnInstance */

	const Type	IsReadOnly						= 1 << 20; /** true if this node is overridden to appear as read-only */

	const Type	SkipChildValidation				= 1 << 21; /** true if this node should skip child validation */

	const Type  ShowInnerObjectProperties		= 1 << 22;

	const Type	HasCustomResetToDefault			= 1 << 23;	/** true if this node's visual representation of reset to default has been customized*/

	const Type 	NoFlags							= 0;

};

namespace FPropertyNodeConstants
{
	const int32 NoDepthRestrictions = -1;

	/** Character used to deliminate sub-categories in category path names */
	const TCHAR CategoryDelimiterChar = TCHAR( '|' );
};

class FPropertySettings
{
public:
	static FPropertySettings& Get();
	bool ShowFriendlyPropertyNames() const { return bShowFriendlyPropertyNames; }
	bool ShowHiddenProperties() const { return bShowHiddenProperties; }
	bool ExpandDistributions() const { return bExpandDistributions; }
private:
	FPropertySettings();
private:
	bool bShowFriendlyPropertyNames;
	bool bExpandDistributions;
	bool bShowHiddenProperties;
};

class FPropertyItemValueDataTrackerSlate;
class FPropertyNode;

struct FAddressPair
{
	FAddressPair(const UObject* InObject, uint8* Address, bool bInIsStruct)
		: Object( InObject)
		, ReadAddress( Address )
		, bIsStruct(bInIsStruct)
	{}
	TWeakObjectPtr<const UObject> Object;
	uint8* ReadAddress;
	bool bIsStruct;
};

template<> struct TIsPODType<FAddressPair> { enum { Value = true }; };

struct FReadAddressListData
{
public:
	FReadAddressListData()
		: bAllValuesTheSame( false )
		, bRequiresCache( true )
	{
	}
	void Add(const UObject* Object, uint8* Address, bool bIsStruct = false)
	{
		ReadAddresses.Add(FAddressPair(Object, Address, bIsStruct));
	}

	int32 Num() const
	{
		return ReadAddresses.Num();
	}

	uint8* GetAddress( int32 Index )
	{
		const FAddressPair& Pair = ReadAddresses[Index];
		return (Pair.Object.IsValid() || Pair.bIsStruct) ? Pair.ReadAddress : 0;
	}

	bool IsValidIndex( int32 Index ) const
	{
		return ReadAddresses.IsValidIndex(Index);
	}

	void Reset()
	{
		ReadAddresses.Reset();
		bAllValuesTheSame = false;
		bRequiresCache = true;
	}
	
	bool bAllValuesTheSame;
	bool bRequiresCache;
private:
	TArray<FAddressPair> ReadAddresses;
};

/**
 * A list of read addresses for a property node which contains the address for the nodes UProperty on each object
 */
class FReadAddressList
{
	friend class FPropertyNode;
public:
	FReadAddressList()
		: ReadAddressListData(nullptr)
	{}

	int32 Num() const
	{
		return (ReadAddressListData != nullptr) ? ReadAddressListData->Num() : 0;
	}

	uint8* GetAddress( int32 Index )
	{
		return ReadAddressListData->GetAddress( Index );
	}
	
	bool IsValidIndex( int32 Index ) const
	{
		return ReadAddressListData->IsValidIndex( Index );
	}

	void Reset()
	{
		if (ReadAddressListData != nullptr)
		{
			ReadAddressListData->Reset();
		}
	}

private:
	FReadAddressListData* ReadAddressListData;
};



/**
 * Parameters for initializing a property node
 */
struct FPropertyNodeInitParams
{
	/** The parent of the property node */
	TSharedPtr<FPropertyNode> ParentNode;
	/** The property that the node observes and modifies*/
	UProperty* Property;
	/** Offset to the property data within either a fixed array or a dynamic array */
	int32 ArrayOffset;
	/** Index of the property in its array parent */
	int32 ArrayIndex;
	/** Whether or not to create any children */
	bool bAllowChildren;
	/** Whether or not to allow hidden properties (ones without CPF_Edit) to be visible */
	bool bForceHiddenPropertyVisibility;
	/** Whether or not to create category nodes (note: this setting is only valid for the root node of a property tree. The setting will propagate to children) */
	bool bCreateCategoryNodes;
	/** Whether or not to create nodes for properties marked CPF_DisableEditOnInstance */
	bool bCreateDisableEditOnInstanceNodes;

	FPropertyNodeInitParams()
		: ParentNode(nullptr)
		, Property(nullptr)
		, ArrayOffset(0)
		, ArrayIndex( INDEX_NONE )
		, bAllowChildren( true )
		, bForceHiddenPropertyVisibility( false )
		, bCreateCategoryNodes( true )
		, bCreateDisableEditOnInstanceNodes( true )
	{}
};

/** Describes in which way an array property change has happend. This is used
	for propagation of array property changes to the instances of archetype objects. */
struct EPropertyArrayChangeType
{
	enum Type
	{
		/** A value was added to the array */
		Add,
		/** The array was cleared */
		Clear,
		/** A new item has been inserted. */
		Insert,
		/** An item has been deleted */
		Delete,
		/** An item has been duplicated */
		Duplicate,
		/** Two items have been swapped */
		Swap,
	};
};

class FComplexPropertyNode;

enum EPropertyDataValidationResult : uint8
{
	/** The object(s) being viewed are now invalid */
	ObjectInvalid,
	/** Non dynamic array property nodes were added or removed that would require a refresh */
	PropertiesChanged,
	/** An edit inline new value changed,  In the tree this rebuilds the nodes, in the details view we don't need to do this */
	EditInlineNewValueChanged,
	/** The size of an array changed (delete,insert,add) */
	ArraySizeChanged,
	/** All data is valid */
	DataValid,
};

/**
 * The base class for all property nodes                                                              
 */
class FPropertyNode : public TSharedFromThis<FPropertyNode>
{
public:

	FPropertyNode(void);
	virtual ~FPropertyNode(void);

	/**
	 * Init Tree Node internally (used only derived classes to pass through variables that are common to all nodes
	 * @param InitParams	Parameters for how the node should be initialized
	 */
	void InitNode(const FPropertyNodeInitParams& InitParams);

	/**
	 * Indicates that children of this node should be rebuilt next tick.  Some topology changes will require this
	 */
	void RequestRebuildChildren() { bRebuildChildrenRequested = true; }

	/**
	 * Used for rebuilding this nodes children
	 */
	void RebuildChildren();

	/**
	 * For derived windows to be able to add their nodes to the child array
	 */
	void AddChildNode(TSharedPtr<FPropertyNode> InNode);

	/**
	 * Clears cached read address data
	 */
	void ClearCachedReadAddresses(bool bRecursive = true);

	/**
	 * Interface function to get at the dervied FObjectPropertyNodeWx class
	 */
	virtual class FObjectPropertyNode* AsObjectNode() { return nullptr; }
	virtual const FObjectPropertyNode* AsObjectNode() const { return nullptr; }

	virtual FComplexPropertyNode* AsComplexNode() { return nullptr; }
	virtual const FComplexPropertyNode* AsComplexNode() const { return nullptr; }

	/**
	 * Interface function to get at the dervied FCategoryPropertyNodeWx class
	 */
	virtual class FCategoryPropertyNode* AsCategoryNode() { return nullptr; }

	/**
	 * Interface function to get at the dervied FItemPropertyNodeWx class
	 */
	virtual class FItemPropertyNode* AsItemPropertyNode() { return nullptr; }

	/**
	 * Follows the chain of items upwards until it finds the object window that houses this item.
	 */
	class FComplexPropertyNode* FindComplexParent();
	const class FComplexPropertyNode* FindComplexParent() const;

	class FObjectPropertyNode* FindObjectItemParent();
	const class FObjectPropertyNode* FindObjectItemParent() const;

	/**
	 * Follows the top-most object window that contains this property window item.
	 */
	class FObjectPropertyNode* FindRootObjectItemParent();

	/**
	 * Used to see if any data has been destroyed from under the property tree.  Should only be called during Tick
	 */
	EPropertyDataValidationResult EnsureDataIsValid();

	//////////////////////////////////////////////////////////////////////////
	//Flags
	uint32 HasNodeFlags(const EPropertyNodeFlags::Type InTestFlags) const { return PropertyNodeFlags & InTestFlags; }
	/**
	 * Sets the flags used by the window and the root node
	 * @param InFlags - flags to turn on or off
	 * @param InOnOff - whether to toggle the bits on or off
	 */
	void SetNodeFlags(const EPropertyNodeFlags::Type InFlags, const bool InOnOff);

	/**
	 * Finds a child of this property node
	 *
	 * @param InPropertyName	The name of the property to find
	 * @param bRecurse		true if we should recurse into children's children and so on.
	 */
	TSharedPtr<FPropertyNode> FindChildPropertyNode(const FName InPropertyName, bool bRecurse = false);

	/**
	 * Returns the parent node in the hierarchy
	 */
	FPropertyNode*			GetParentNode() { return ParentNodeWeakPtr.IsValid() ? ParentNodeWeakPtr.Pin().Get() : nullptr; }
	const FPropertyNode*	GetParentNode() const { return ParentNodeWeakPtr.IsValid() ? ParentNodeWeakPtr.Pin().Get() : nullptr; }
	TSharedPtr<FPropertyNode> GetParentNodeSharedPtr() { return ParentNodeWeakPtr.Pin(); }
	/**
	 * Returns the Property this Node represents
	 */
	UProperty*			GetProperty() { return Property.Get(); }
	const UProperty*	GetProperty() const { return Property.Get(); }

	/**
	 * Accessor functions for internals
	 */
	const int32 GetArrayOffset() const { return ArrayOffset; }
	const int32 GetArrayIndex() const { return ArrayIndex; }

	/**
	 * Return number of children that survived being filtered
	 */
	int32 GetNumChildNodes() const { return ChildNodes.Num(); }

	/**
	 * Returns the matching Child node
	 */
	TSharedPtr<FPropertyNode>& GetChildNode(const int32 ChildIndex)
	{
		check(ChildNodes[ChildIndex].IsValid());
		return ChildNodes[ChildIndex];
	}

	/**
	 * Returns the matching Child node
	 */
	const TSharedPtr<FPropertyNode>& GetChildNode(const int32 ChildIndex) const
	{
		check(ChildNodes[ChildIndex].IsValid());
		return ChildNodes[ChildIndex];
	}

	/**
	 * Returns the Child node whose ArrayIndex matches the supplied ChildIndex
	 */
	bool GetChildNode(const int32 ChildArrayIndex, TSharedPtr<FPropertyNode>& OutChildNode);

	/**
	* Returns the Child node whose ArrayIndex matches the supplied ChildIndex
	*/
	bool GetChildNode(const int32 ChildArrayIndex, TSharedPtr<FPropertyNode>& OutChildNode) const;

	/** @return whether this window's property is constant (can't be edited by the user) */
	bool IsEditConst() const;

	/**
	 * Gets the full name of this node
	 * @param PathPlusIndex - return value with full path of node
	 * @param bWithArrayIndex - If True, adds an array index (where appropriate)
	 * @param StopParent	- Stop at this parent (if any). Does NOT include it in the path
	 * @param bIgnoreCategories - Skip over categories
	 */
	virtual bool GetQualifiedName(FString& PathPlusIndex, const bool bWithArrayIndex, const FPropertyNode* StopParent = nullptr, bool bIgnoreCategories = false) const;

	// The bArrayPropertiesCanDifferInSize flag is an override for array properties which want to display
	// e.g. the "Clear" and "Empty" buttons, even though the array properties may differ in the number of elements.
	bool GetReadAddress(
		bool InRequiresSingleSelection,
		FReadAddressList& OutAddresses,
		bool bComparePropertyContents = true,
		bool bObjectForceCompare = false,
		bool bArrayPropertiesCanDifferInSize = false);

	/**
	 * fills in the OutAddresses array with the addresses of all of the available objects.
	 * @param OutAddresses	Storage array for all of the objects' addresses.
	 */
	bool GetReadAddress(FReadAddressList& OutAddresses);

	/**
	 * Gets read addresses without accessing cached data.  Is less efficient but gets the must up to date data
	 */
	virtual bool GetReadAddressUncached(FPropertyNode& InNode, bool InRequiresSingleSelection, FReadAddressListData* OutAddresses, bool bComparePropertyContents = true, bool bObjectForceCompare = false, bool bArrayPropertiesCanDifferInSize = false) const;
	virtual bool GetReadAddressUncached(FPropertyNode& InNode, FReadAddressListData& OutAddresses) const;

	/**
	 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
	 *
	 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
	 *
	 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
	 */
	virtual uint8* GetValueBaseAddress(uint8* Base);

	/**
	 * Calculates the memory address for the data associated with this item's value.  For most properties, identical to GetValueBaseAddress.  For items corresponding
	 * to dynamic array elements, the pointer returned will be the location for that element's data.
	 *
	 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to whatever type is the Inner for the dynamic array)
	 */
	virtual uint8* GetValueAddress(uint8* Base);

	/**
	 * Sets the display name override to use instead of the display name
	 */
	virtual void SetDisplayNameOverride(const FText& InDisplayNameOverride) {}

	/**
	* @return true if the property is mark as a favorite
	*/
	virtual void SetFavorite(bool FavoriteValue) {}

	/**
	* @return true if the property is mark as a favorite
	*/
	virtual bool IsFavorite() const { return false; }

	/**
	* Set the permission to display the favorite icon
	*/
	virtual void SetCanDisplayFavorite(bool CanDisplayFavoriteIcon) {}

	/**
	* Set the permission to display the favorite icon
	*/
	virtual bool CanDisplayFavorite() const { return false; }

	/**
	 * @return The formatted display name for the property in this node
	 */
	virtual FText GetDisplayName() const { return FText::GetEmpty(); }

	/**
	 * Sets the tooltip override to use instead of the property tooltip
	 */
	virtual void SetToolTipOverride(const FText& InToolTipOverride) {}

	/**
	 * @return The tooltip for the property in this node
	 */
	virtual FText GetToolTipText() const { return FText::GetEmpty(); }

	/**
	 * If there is a property, sees if it matches.  Otherwise sees if the entire parent structure matches
	 */
	bool GetDiffersFromDefault();

	/**
	 * @return The label for displaying a reset to default value
	 */
	FText GetResetToDefaultLabel();

	/**
	 * If there is a property, resets it to default.  Otherwise resets the entire parent structure
	 */
	void ResetToDefault(FNotifyHook* InNotifyHook);

	/**
	 * @return If this property node is associated with a property that can be reordered within an array
	 */
	bool IsReorderable();

	/**Walks up the hierarchy and return true if any parent node is a favorite*/
	bool IsChildOfFavorite(void) const;

	void NotifyPreChange(UProperty* PropertyAboutToChange, class FNotifyHook* InNotifyHook);

	void NotifyPostChange(FPropertyChangedEvent& InPropertyChangedEvent, class FNotifyHook* InNotifyHook);

	void SetOnRebuildChildren(FSimpleDelegate InOnRebuildChildren);

	/**
	 * Propagates the property change to all instances of an archetype
	 *
	 * @param	ModifiedObject	Object which property has been modified
	 * @param	NewValue		New value of the property
	 * @param	PreviousValue	Value of the property before the modification
	 */
	void PropagatePropertyChange(UObject* ModifiedObject, const TCHAR* NewValue, const FString& PreviousValue);

	/**
	 * Propagates the property change of a container property to all instances of an archetype
	 *
	 * @param	ModifiedObject				Object which property has been modified
	 * @param	OriginalContainerContent	Original content of the container before the modification ( as returned by ExportText_Direct )
	 * @param	ChangeType					In which way was the container modified
	 * @param	Index						Index of the modified item
	 */
	void PropagateContainerPropertyChange(UObject* ModifiedObject, const FString& OriginalContainerContent,
		EPropertyArrayChangeType::Type ChangeType, int32 Index, TMap<UObject*, bool>* PropagationResult = nullptr, int32 SwapIndex = INDEX_NONE);

	static void AdditionalInitializationUDS(UProperty* Property, uint8* RawPtr);

	/** Broadcasts when a property value changes */
	DECLARE_EVENT(FPropertyNode, FPropertyValueChangedEvent);
	FPropertyValueChangedEvent& OnPropertyValueChanged() { return PropertyValueChangedEvent; }

	/** Broadcasts when a child of this property changes */
	FPropertyValueChangedEvent& OnChildPropertyValueChanged() { return ChildPropertyValueChangedEvent; }

	/** Broadcasts when a property value changes */
	DECLARE_EVENT(FPropertyNode, FPropertyValuePreChangeEvent);
	FPropertyValuePreChangeEvent& OnPropertyValuePreChange() { return PropertyValuePreChangeEvent; }

	/** Broadcasts when a child of this property changes */
	FPropertyValuePreChangeEvent& OnChildPropertyValuePreChange() { return ChildPropertyValuePreChangeEvent; }

	/**
	 * Marks window's seem due to filtering flags
	 * @param InFilterStrings	- List of strings that must be in the property name in order to display
	 *					Empty InFilterStrings means display as normal
	 *					All strings must match in order to be accepted by the filter
	 * @param bParentAllowsVisible - is NOT true for an expander that is NOT expanded
	 */
	void FilterNodes(const TArray<FString>& InFilterStrings, const bool bParentSeenDueToFiltering = false);

	/**
	 * Marks windows as visible based on the filter strings or standard visibility
	 *
	 * @param bParentAllowsVisible - is NOT true for an expander that is NOT expanded
	 */
	void ProcessSeenFlags(const bool bParentAllowsVisible);

	/**
	 * Marks windows as visible based their favorites status
	 */
	void ProcessSeenFlagsForFavorites(void);

	/**
	 * @return true if this node should be visible in a tree
	 */
	bool IsVisible() const { return HasNodeFlags(EPropertyNodeFlags::IsBeingFiltered) == 0 || HasNodeFlags(EPropertyNodeFlags::IsSeen) || HasNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFiltering); };

	static TSharedRef< FPropertyPath > CreatePropertyPath(const TSharedRef< FPropertyNode >& PropertyNode)
	{
		TArray< FPropertyInfo > Properties;
		FPropertyNode* CurrentNode = &PropertyNode.Get();

		if (CurrentNode != nullptr && CurrentNode->AsCategoryNode() != nullptr)
		{
			TSharedRef< FPropertyPath > NewPath = MakeShareable(new FPropertyPath());
			return NewPath;
		}

		while (CurrentNode != nullptr)
		{
			if (CurrentNode->AsItemPropertyNode() != nullptr)
			{
				FPropertyInfo NewPropInfo;
				NewPropInfo.Property = CurrentNode->GetProperty();
				NewPropInfo.ArrayIndex = CurrentNode->GetArrayIndex();

				Properties.Add(NewPropInfo);
			}

			CurrentNode = CurrentNode->GetParentNode();
		}

		TSharedRef< FPropertyPath > NewPath = MakeShareable(new FPropertyPath());

		for (int PropertyIndex = Properties.Num() - 1; PropertyIndex >= 0; --PropertyIndex)
		{
			NewPath->AddProperty(Properties[PropertyIndex]);
		}

		return NewPath;
	}

	static TSharedPtr< FPropertyNode > FindPropertyNodeByPath(const TSharedPtr< FPropertyPath > Path, const TSharedRef< FPropertyNode >& StartingNode)
	{
		if (!Path.IsValid() || Path->GetNumProperties() == 0)
		{
			return StartingNode;
		}

		bool FailedToFindProperty = false;
		TSharedPtr< FPropertyNode > PropertyNode = StartingNode;
		for (int PropertyIndex = 0; PropertyIndex < Path->GetNumProperties() && !FailedToFindProperty; PropertyIndex++)
		{
			FailedToFindProperty = true;
			const FPropertyInfo& PropInfo = Path->GetPropertyInfo(PropertyIndex);

			TArray< TSharedRef< FPropertyNode > > ChildrenStack;
			ChildrenStack.Push(PropertyNode.ToSharedRef());
			while (ChildrenStack.Num() > 0)
			{
				const TSharedRef< FPropertyNode > CurrentNode = ChildrenStack.Pop();

				for (int32 ChildIndex = 0; ChildIndex < CurrentNode->GetNumChildNodes(); ++ChildIndex)
				{
					const TSharedPtr< FPropertyNode > ChildNode = CurrentNode->GetChildNode(ChildIndex);

					if (ChildNode->AsItemPropertyNode() == nullptr)
					{
						ChildrenStack.Add(ChildNode.ToSharedRef());
					}
					else if (ChildNode.IsValid() &&
						ChildNode->GetProperty() == PropInfo.Property.Get() &&
						ChildNode->GetArrayIndex() == PropInfo.ArrayIndex)
					{
						PropertyNode = ChildNode;
						FailedToFindProperty = false;
						break;
					}
				}
			}
		}

		if (FailedToFindProperty)
		{
			PropertyNode = nullptr;
		}

		return PropertyNode;
	}


	static TArray< FPropertyInfo > GetPossibleExtensionsForPath(const TSharedPtr< FPropertyPath > Path, const TSharedRef< FPropertyNode >& StartingNode)
	{
		TArray< FPropertyInfo > PossibleExtensions;
		TSharedPtr< FPropertyNode > PropertyNode = FindPropertyNodeByPath(Path, StartingNode);

		if (!PropertyNode.IsValid())
		{
			return PossibleExtensions;
		}

		for (int32 ChildIndex = 0; ChildIndex < PropertyNode->GetNumChildNodes(); ++ChildIndex)
		{
			TSharedPtr< FPropertyNode > CurrentNode = PropertyNode->GetChildNode(ChildIndex);

			if (CurrentNode.IsValid() && CurrentNode->AsItemPropertyNode() != nullptr)
			{
				FPropertyInfo NewPropInfo;
				NewPropInfo.Property = CurrentNode->GetProperty();
				NewPropInfo.ArrayIndex = CurrentNode->GetArrayIndex();

				bool AlreadyExists = false;
				for (auto ExtensionIter = PossibleExtensions.CreateConstIterator(); ExtensionIter; ++ExtensionIter)
				{
					if (*ExtensionIter == NewPropInfo)
					{
						AlreadyExists = true;
						break;
					}
				}

				if (!AlreadyExists)
				{
					PossibleExtensions.Add(NewPropInfo);
				}
			}
		}

		return PossibleExtensions;
	}

	/**
	 * Adds a restriction to the possible values for this property.
	 * @param Restriction	The restriction being added to this property.
	 */
	virtual void AddRestriction(TSharedRef<const class FPropertyRestriction> Restriction);

	/**
	* Tests if a value is hidden for this property
	* @param Value			The value to test for being hidden.
	* @return				True if this value is hidden.
	*/
	bool IsHidden(const FString& Value) const
	{
		return IsHidden(Value, nullptr);
	}

	/**
	 * Tests if a value is disabled for this property
	 * @param Value			The value to test for being disabled.
	 * @return				True if this value is disabled.
	 */
	bool IsDisabled(const FString& Value) const
	{
		return IsDisabled(Value, nullptr);
	}

	bool IsRestricted(const FString& Value) const
	{
		return IsHidden(Value) || IsDisabled(Value);
	}

	/**
	* Tests if a value is hidden for this property.
	* @param Value			The value to test for being hidden.
	* @param OutReasons		If hidden, the reasons why.
	* @return				True if this value is hidden.
	*/
	virtual bool IsHidden(const FString& Value, TArray<FText>* OutReasons) const;

	/**
	 * Tests if a value is disabled for this property.
	 * @param Value			The value to test for being disabled.
	 * @param OutReasons	If disabled, the reasons why.
	 * @return				True if this value is disabled.
	 */
	virtual bool IsDisabled(const FString& Value, TArray<FText>* OutReasons) const;

	/**
	* Tests if a value is restricted for this property.
	* @param Value			The value to test for being restricted.
	* @param OutReasons		If restricted, the reasons why.
	* @return				True if this value is restricted.
	*/
	bool IsRestricted(const FString& Value, TArray<FText>& OutReasons) const;

	/**
	 * Generates a consistent tooltip describing this restriction for use in the editor.
	 * @param Value			The value to test for restriction and generate the tooltip from.
	 * @param OutTooltip	The tooltip describing why this value is restricted.
	 * @return				True if this value is restricted.
	 */
	virtual bool GenerateRestrictionToolTip(const FString& Value, FText& OutTooltip)const;

	const TArray<TSharedRef<const class FPropertyRestriction>>& GetRestrictions() const
	{
		return Restrictions;
	}

	FPropertyChangedEvent& FixPropertiesInEvent(FPropertyChangedEvent& Event);

	/** Set metadata value for 'Key' to 'Value' on this property instance (as opposed to the class) */
	void SetInstanceMetaData(const FName& Key, const FString& Value);

	/**
	 * Get metadata value for 'Key' for this property instance (as opposed to the class)
	 * 
	 * @return Pointer to metadata value; nullptr if Key not found
	 */
	const FString* GetInstanceMetaData(const FName& Key) const;

	bool ParentOrSelfHasMetaData(const FName& MetaDataKey) const;

	/**
	 * Invalidates the cached state of this node in all children;
	 */
	void InvalidateCachedState();

	static void SetupKeyValueNodePair( TSharedPtr<FPropertyNode>& KeyNode, TSharedPtr<FPropertyNode>& ValueNode )
	{
		check( KeyNode.IsValid() && ValueNode.IsValid() );
		check( !KeyNode->PropertyKeyNode.IsValid() && !ValueNode->PropertyKeyNode.IsValid() );

		ValueNode->PropertyKeyNode = KeyNode;
	}

	TSharedPtr<FPropertyNode>& GetPropertyKeyNode() { return PropertyKeyNode; }

	const TSharedPtr<FPropertyNode>& GetPropertyKeyNode() const { return PropertyKeyNode; }

protected:

	TSharedRef<FEditPropertyChain> BuildPropertyChain( UProperty* PropertyAboutToChange );

	/**
	 * Destroys all node within the hierarchy
	 */
	void DestroyTree(const bool bInDestroySelf=true);

	/**
	 * Interface function for Custom Setup of Node (priot to node flags being set)
	 */
	virtual void InitBeforeNodeFlags () {};

	/**
	 * Interface function for Custom expansion Flags.  Default is objects and categories which always expand
	 */
	virtual void InitExpansionFlags (){ SetNodeFlags(EPropertyNodeFlags::CanBeExpanded, true); };

	/**
	 * Interface function for Creating Child Nodes
	 */
	virtual void InitChildNodes() = 0;

	/**
	 * Does the string compares to ensure this Name is acceptable to the filter that is passed in
	 */
	bool IsFilterAcceptable(const TArray<FString>& InAcceptableNames, const TArray<FString>& InFilterStrings);
	/**
	 * Make sure that parent nodes are expanded
	 */
	void ExpandParent( bool bInRecursive );

	/** @return		The property stored at this node, to be passed to Pre/PostEditChange. */
	UProperty*		GetStoredProperty()		{ return nullptr; }

	bool GetDiffersFromDefaultForObject( FPropertyItemValueDataTrackerSlate& ValueTracker, UProperty* InProperty );

	FString GetDefaultValueAsStringForObject( FPropertyItemValueDataTrackerSlate& ValueTracker, UObject* InObject, UProperty* InProperty );
	
	/**
	* Gets the default value of the property as string.
	*/
	FString GetDefaultValueAsString();

	/**
	 * Helper function to obtain the display name for an enum property
	 * @param InEnum		The enum whose metadata to pull from
	 * @param DisplayName	The name of the enum value to adjust
	 *
	 * @return	true if the DisplayName has been changed
	 */
	bool AdjustEnumPropDisplayName( UEnum *InEnum, FString& DisplayName ) const;

	/**
	 * Helper function for derived members to be able to 
	 * broadcast property changed notifications
	 */
	void BroadcastPropertyChangedDelegates();


	/**
	* Helper function for derived members to be able to
	* broadcast property pre-change notifications
	*/
	void BroadcastPropertyPreChangeDelegates();

	/**
	 * Gets a value tracker for the default of this property in the passed in object
	 *
	 * @param Object	The object to get the value for
	 * @param ObjIndex	The index of the object in the parent property node's object array (for caching)
	 */
	TSharedPtr< FPropertyItemValueDataTrackerSlate > GetValueTracker( UObject* Object, uint32 ObjIndex );

	/**
	 * Updates and caches the current edit const state of this property
	 */
	void UpdateEditConstState();

	/**
	 * Checks to see if the supplied property of a child node requires validation
	 * @param	InChildProp		The property of the child node
	 * @return	True if the property requires validation, false otherwise
	 */
	static bool DoesChildPropertyRequireValidation(UProperty* InChildProp);

protected:
	/**
	 * The node that is the parent of this node or nullptr for the root
	 */
	TWeakPtr<FPropertyNode> ParentNodeWeakPtr;
	//@todo consolidate with ParentNodeWeakPtr, ParentNode is legacy
	FPropertyNode* ParentNode;

	/**	The property node, if any, that serves as the key value for this node */
	TSharedPtr<FPropertyNode> PropertyKeyNode;

	/** Cached read addresses for this property node */
	FReadAddressListData CachedReadAddresses;

	/** List of per object default value trackers associated with this property node */
	TArray< TSharedPtr<FPropertyItemValueDataTrackerSlate> > ObjectDefaultValueTrackers;

	/** List of all child nodes this node is responsible for */
	TArray< TSharedPtr<FPropertyNode> > ChildNodes;

	/** Called when this node's children are rebuilt */
	FSimpleDelegate OnRebuildChildren;

	/** Called when this node's property value is about to change (called during NotifyPreChange) */
	FPropertyValuePreChangeEvent PropertyValuePreChangeEvent;

	/** Called when a child's property value is about to change */
	FPropertyValuePreChangeEvent ChildPropertyValuePreChangeEvent;

	/** Called when this node's property value has changed (called during NotifyPostChange) */
	FPropertyValueChangedEvent PropertyValueChangedEvent;
	
	/** Called when a child's property value has changed */
	FPropertyValueChangedEvent ChildPropertyValueChangedEvent;

	/** The property being displayed/edited. */
	TWeakObjectPtr<UProperty> Property;

	/** Offset to the property data within either a fixed array or a dynamic array */
	int32 ArrayOffset;

	/** The index of the property if it is inside an array, set, or map (internally, we'll use set/map helpers that store element indices in an array) */
	int32 ArrayIndex;

	/** Safety Value representing Depth in the property tree used to stop diabolical topology cases
	 * -1 = No limit on children
	 *  0 = No more children are allowed.  Do not process child nodes
	 *  >0 = A limit has been set by the property and will tick down for successive children
	 */
	int32 MaxChildDepthAllowed;

	/**
	 * Used for flags to determine if the node is seen (if not seen and never been created, don't create it on display)
	 */
	EPropertyNodeFlags::Type PropertyNodeFlags;

	/** If true, children of this node will be rebuilt next tick. */
	bool bRebuildChildrenRequested;

	/** An array of restrictions limiting this property's potential values in property editors.*/
	TArray<TSharedRef<const class FPropertyRestriction>> Restrictions;

	/** Optional reference to a tree node that is displaying this property */
	TWeakPtr< class FDetailTreeNode > TreeNode;

	/**
	 * Stores metadata for this instasnce of the property (in contrast
	 * to regular metadata, which is stored per-class)
	 */
	TMap<FName, FString> InstanceMetaData;

	/**
	* The property path for this property
	*/
	FString PropertyPath;

	/**
	* Cached state of flags that are expensive to update
	* These update when values are changed in the details panel
	*/
	mutable bool bIsEditConst;
	mutable bool bUpdateEditConstState;
	mutable bool bDiffersFromDefault;
	mutable bool bUpdateDiffersFromDefault;
};

class FComplexPropertyNode : public FPropertyNode
{
public:

	enum EPropertyType
	{
		EPT_Object,
		EPT_StandaloneStructure,
	};

	FComplexPropertyNode() : FPropertyNode() {}
	virtual ~FComplexPropertyNode() {}

	virtual FComplexPropertyNode* AsComplexNode() override{ return this; }
	virtual const FComplexPropertyNode* AsComplexNode() const override{ return this; }

	virtual class FStructurePropertyNode* AsStructureNode() { return nullptr; }
	virtual const FStructurePropertyNode* AsStructureNode() const { return nullptr; }

	virtual UStruct* GetBaseStructure() = 0;
	virtual const UStruct* GetBaseStructure() const = 0;

	virtual int32 GetInstancesNum() const = 0;
	virtual uint8* GetMemoryOfInstance(int32 Index) = 0;
	virtual TWeakObjectPtr<UObject> GetInstanceAsUObject(int32 Index) = 0;
	virtual EPropertyType GetPropertyType() const = 0;

	virtual void Disconnect() = 0;
};
