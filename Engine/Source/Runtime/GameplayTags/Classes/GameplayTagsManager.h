// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"

#include "GameplayTagsManager.generated.h"

class UGameplayTagsList;

/** Simple struct for a table row in the gameplay tag table and element in the ini list */
USTRUCT()
struct FGameplayTagTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	/** Tag specified in the table */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GameplayTag)
	FName Tag;

	/** Developer comment clarifying the usage of a particular tag, not user facing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GameplayTag)
	FString DevComment;

	/** Constructors */
	FGameplayTagTableRow() {}
	FGameplayTagTableRow(FName InTag, const FString& InDevComment = TEXT("")) : Tag(InTag), DevComment(InDevComment) {}
	GAMEPLAYTAGS_API FGameplayTagTableRow(FGameplayTagTableRow const& Other);

	/** Assignment/Equality operators */
	GAMEPLAYTAGS_API FGameplayTagTableRow& operator=(FGameplayTagTableRow const& Other);
	GAMEPLAYTAGS_API bool operator==(FGameplayTagTableRow const& Other) const;
	GAMEPLAYTAGS_API bool operator!=(FGameplayTagTableRow const& Other) const;
	GAMEPLAYTAGS_API bool operator<(FGameplayTagTableRow const& Other) const;
};

UENUM()
enum class EGameplayTagSourceType : uint8
{
	Native,				// Was added from C++ code
	DefaultTagList,		// The default tag list in DefaultGameplayTags.ini
	TagList,			// Another tag list from an ini in tags/*.ini
	DataTable,			// From a DataTable
	Invalid,			// Not a real source
};

/** Struct defining where gameplay tags are loaded/saved from. Mostly for the editor */
USTRUCT()
struct FGameplayTagSource
{
	GENERATED_USTRUCT_BODY()

	/** Name of this source */
	UPROPERTY()
	FName SourceName;

	/** Type of this source */
	UPROPERTY()
	EGameplayTagSourceType SourceType;

	/** If this is bound to an ini object for saving, this is the one */
	UPROPERTY()
	class UGameplayTagsList* SourceTagList;

	FGameplayTagSource() 
		: SourceName(NAME_None), SourceType(EGameplayTagSourceType::Invalid), SourceTagList(nullptr) 
	{
	}

	FGameplayTagSource(FName InSourceName, EGameplayTagSourceType InSourceType, UGameplayTagsList* InSourceTagList = nullptr) 
		: SourceName(InSourceName), SourceType(InSourceType), SourceTagList(InSourceTagList)
	{
	}

	static FName GetNativeName()
	{
		static FName NativeName = FName(TEXT("Native"));
		return NativeName;
	}

	static FName GetDefaultName()
	{
		static FName DefaultName = FName(TEXT("DefaultGameplayTags.ini"));
		return DefaultName;
	}

#if WITH_EDITOR
	static FName GetTransientEditorName()
	{
		static FName TransientEditorName = FName(TEXT("TransientEditor"));
		return TransientEditorName;
	}
#endif
};

/** Simple tree node for gameplay tags, this stores metadata about specific tags */
USTRUCT()
struct FGameplayTagNode
{
	GENERATED_USTRUCT_BODY()
	FGameplayTagNode(){};

	/** Simple constructor */
	FGameplayTagNode(FName InTag, TSharedPtr<FGameplayTagNode> InParentNode);

	/** Returns a correctly constructed container with only this tag, useful for doing container queries */
	FORCEINLINE const FGameplayTagContainer& GetSingleTagContainer() const { return CompleteTagWithParents; }

	/**
	 * Get the complete tag for the node, including all parent tags, delimited by periods
	 * 
	 * @return Complete tag for the node
	 */
	FORCEINLINE const FGameplayTag& GetCompleteTag() const { return CompleteTagWithParents.Num() > 0 ? CompleteTagWithParents.GameplayTags[0] : FGameplayTag::EmptyTag; }
	FORCEINLINE FName GetCompleteTagName() const { return GetCompleteTag().GetTagName(); }
	FORCEINLINE FString GetCompleteTagString() const { return GetCompleteTag().ToString(); }

	/**
	 * Get the simple tag for the node (doesn't include any parent tags)
	 * 
	 * @return Simple tag for the node
	 */
	FORCEINLINE FName GetSimpleTagName() const { return Tag; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	FORCEINLINE TArray< TSharedPtr<FGameplayTagNode> >& GetChildTagNodes() { return ChildTags; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	FORCEINLINE const TArray< TSharedPtr<FGameplayTagNode> >& GetChildTagNodes() const { return ChildTags; }

	/**
	 * Get the parent tag node of this node
	 * 
	 * @return The parent tag node of this node
	 */
	FORCEINLINE TSharedPtr<FGameplayTagNode> GetParentTagNode() const { return ParentNode; }

	/**
	* Get the net index of this node
	*
	* @return The net index of this node
	*/
	FORCEINLINE FGameplayTagNetIndex GetNetIndex() const { return NetIndex; }

	/** Reset the node of all of its values */
	GAMEPLAYTAGS_API void ResetNode();

private:
	/** Raw name for this tag at current rank in the tree */
	FName Tag;

	/** This complete tag is at GameplayTags[0], with parents in ParentTags[] */
	FGameplayTagContainer CompleteTagWithParents;

	/** Child gameplay tag nodes */
	TArray< TSharedPtr<FGameplayTagNode> > ChildTags;

	/** Owner gameplay tag node, if any */
	TSharedPtr<FGameplayTagNode> ParentNode;
	
	/** Net Index of this node */
	FGameplayTagNetIndex NetIndex;

#if WITH_EDITORONLY_DATA
	/** Package or config file this tag came from. This is the first one added. If None, this is an implicitly added tag */
	FName SourceName;

	/** Comment for this tag */
	FString DevComment;
#endif 

	friend class UGameplayTagsManager;
};

/** Holds data about the tag dictionary, is in a singleton UObject */
UCLASS(config=Engine)
class GAMEPLAYTAGS_API UGameplayTagsManager : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Destructor */
	~UGameplayTagsManager();

	/** Returns the global UGameplayTagsManager manager */
	FORCEINLINE static UGameplayTagsManager& Get()
	{
		if (SingletonManager == nullptr)
		{
			InitializeManager();
		}

		return *SingletonManager;
	}

	/**
	 * Gets the FGameplayTag that corresponds to the TagName
	 *
	 * @param TagName The Name of the tag to search for
	 * @param ErrorIfNotfound: ensure() that tag exists.
	 * 
	 * @return Will return the corresponding FGameplayTag or an empty one if not found.
	 */
	FGameplayTag RequestGameplayTag(FName TagName, bool ErrorIfNotFound=true) const;

	/**
	 *	Searches for a gameplay tag given a partial string. This is slow and intended mainly for console commands/utilities to make
	 *	developer life's easier. This will attempt to match as best as it can. If you pass "A.b" it will match on "A.b." before it matches "a.b.c".
	 */
	FGameplayTag FindGameplayTagFromPartialString_Slow(FString PartialString) const;

	/**
	 * Registers the given name as a gameplay tag, and tracks that it is being directly referenced from code
	 * This can only be called during engine initialization, the table needs to be locked down before replication
	 *
	 * @param TagName The Name of the tag to add
	 * 
	 * @return Will return the corresponding FGameplayTag
	 */
	FGameplayTag AddNativeGameplayTag(FName TagName);

	/** Call to flush the list of native tags, once called it is unsafe to add more */
	void DoneAddingNativeTags();

	static FSimpleMulticastDelegate& OnLastChanceToAddNativeTags();


	void CallOrRegister_OnDoneAddingNativeTagsDelegate(FSimpleMulticastDelegate::FDelegate Delegate);

	/**
	 * Gets a Tag Container containing the supplied tag and all of it's parents as explicit tags
	 *
	 * @param GameplayTag The Tag to use at the child most tag for this container
	 * 
	 * @return A Tag Container with the supplied tag and all its parents added explicitly
	 */
	FGameplayTagContainer RequestGameplayTagParents(const FGameplayTag& GameplayTag) const;

	/**
	 * Gets a Tag Container containing the all tags in the hierarchy that are children of this tag. Does not return the original tag
	 *
	 * @param GameplayTag					The Tag to use at the parent tag
	 * 
	 * @return A Tag Container with the supplied tag and all its parents added explicitly
	 */
	FGameplayTagContainer RequestGameplayTagChildren(const FGameplayTag& GameplayTag) const;

	/** Returns direct parent GameplayTag of this GameplayTag, calling on x.y will return x */
	FGameplayTag RequestGameplayTagDirectParent(const FGameplayTag& GameplayTag) const;

	/**
	 * Helper function to get the stored TagContainer containing only this tag, which has searchable ParentTags
	 * @param GameplayTag		Tag to get single container of
	 * @return					Pointer to container with this tag
	 */
	FORCEINLINE_DEBUGGABLE const FGameplayTagContainer* GetSingleTagContainer(const FGameplayTag& GameplayTag) const
	{
		TSharedPtr<FGameplayTagNode> TagNode = FindTagNode(GameplayTag);
		if (TagNode.IsValid())
		{
			return &(TagNode->GetSingleTagContainer());
		}
		return nullptr;
	}

	/**
	 * Checks node tree to see if a FGameplayTagNode with the tag exists
	 *
	 * @param TagName	The name of the tag node to search for
	 *
	 * @return A shared pointer to the FGameplayTagNode found, or NULL if not found.
	 */
	FORCEINLINE_DEBUGGABLE TSharedPtr<FGameplayTagNode> FindTagNode(const FGameplayTag& GameplayTag) const
	{
		const TSharedPtr<FGameplayTagNode>* Node = GameplayTagNodeMap.Find(GameplayTag);

		if (Node)
		{
			return *Node;
		}
#if WITH_EDITOR
		// Check redirector
		if (GIsEditor && GameplayTag.IsValid())
		{
			FGameplayTag RedirectedTag = GameplayTag;

			RedirectSingleGameplayTag(RedirectedTag, nullptr);

			Node = GameplayTagNodeMap.Find(RedirectedTag);

			if (Node)
			{
				return *Node;
			}
		}
#endif
		return nullptr;
	}

	/**
	 * Checks node tree to see if a FGameplayTagNode with the name exists
	 *
	 * @param TagName	The name of the tag node to search for
	 *
	 * @return A shared pointer to the FGameplayTagNode found, or NULL if not found.
	 */
	FORCEINLINE_DEBUGGABLE TSharedPtr<FGameplayTagNode> FindTagNode(FName TagName) const
	{
		FGameplayTag PossibleTag(TagName);
		return FindTagNode(PossibleTag);
	}

	/** Loads the tag tables referenced in the GameplayTagSettings object */
	void LoadGameplayTagTables();

	/** Helper function to construct the gameplay tag tree */
	void ConstructGameplayTagTree();

	/** Helper function to destroy the gameplay tag tree */
	void DestroyGameplayTagTree();

	/** Splits a tag such as x.y.z into an array of names {x,y,z} */
	void SplitGameplayTagFName(const FGameplayTag& Tag, TArray<FName>& OutNames) const;

	/** Gets the list of all tags in the dictionary */
	void RequestAllGameplayTags(FGameplayTagContainer& TagContainer, bool OnlyIncludeDictionaryTags) const;

	/** Returns true if if the passed in name is in the tag dictionary and can be created */
	bool ValidateTagCreation(FName TagName) const;

	/** Returns the tag source for a given tag source name, or null if not found */
	const FGameplayTagSource* FindTagSource(FName TagSourceName) const;

	/** Fills in an array with all tag sources of a specific type */
	void FindTagSourcesWithType(EGameplayTagSourceType TagSourceType, TArray<const FGameplayTagSource*>& OutArray) const;

	/**
	 * Check to see how closely two FGameplayTags match. Higher values indicate more matching terms in the tags.
	 *
	 * @param GameplayTagOne	The first tag to compare
	 * @param GameplayTagTwo	The second tag to compare
	 *
	 * @return the length of the longest matching pair
	 */
	int32 GameplayTagsMatchDepth(const FGameplayTag& GameplayTagOne, const FGameplayTag& GameplayTagTwo) const;

	/** Returns true if we should import tags from UGameplayTagsSettings objects (configured by INI files) */
	bool ShouldImportTagsFromINI() const;

	/** Should we print loading errors when trying to load invalid tags */
	bool ShouldWarnOnInvalidTags() const
	{
		return bShouldWarnOnInvalidTags;
	}

	/** Should use fast replication */
	bool ShouldUseFastReplication() const
	{
		return bUseFastReplication;
	}

	/** Handles redirectors for an entire container, will also error on invalid tags */
	void RedirectTagsForContainer(FGameplayTagContainer& Container, UProperty* SerializingProperty) const;

	/** Handles redirectors for a single tag, will also error on invalid tag. This is only called for when individual tags are serialized on their own */
	void RedirectSingleGameplayTag(FGameplayTag& Tag, UProperty* SerializingProperty) const;

	/** Gets a tag name from net index and vice versa, used for replication efficiency */
	FName GetTagNameFromNetIndex(FGameplayTagNetIndex Index) const;
	FGameplayTagNetIndex GetNetIndexFromTag(const FGameplayTag &InTag) const;

	/** Cached number of bits we need to replicate tags. That is, Log2(Number of Tags). Will always be <= 16. */
	int32 NetIndexTrueBitNum;
	
	/** The length in bits of the first segment when net serializing tags. We will serialize NetIndexFirstBitSegment + 1 bit to indicatore "more" (more = second segment that is NetIndexTrueBitNum - NetIndexFirstBitSegment) */
	int32 NetIndexFirstBitSegment;

	/** Numbers of bits to use for replicating container size. This can be set via config. */
	int32 NumBitsForContainerSize;

	/** This is the actual value for an invalid tag "None". This is computed at runtime as (Total number of tags) + 1 */
	FGameplayTagNetIndex InvalidTagNetIndex;

	const TArray<TSharedPtr<FGameplayTagNode>>& GetNetworkGameplayTagNodeIndex() const { return NetworkGameplayTagNodeIndex; }

#if WITH_EDITOR
	/** Gets a Filtered copy of the GameplayRootTags Array based on the comma delimited filter string passed in */
	void GetFilteredGameplayRootTags(const FString& InFilterString, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray) const;

	/** Returns "Categories" meta property from given handle, used for filtering by tag wiodget */
	FString GetCategoriesMetaFromPropertyHandle(TSharedPtr<class IPropertyHandle> PropertyHandle) const;

	FString GetCategoriesMetaFromFunction(UFunction* Func) const;

	/** Gets a list of all gameplay tag nodes added by the specific source */
	void GetAllTagsFromSource(FName TagSource, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray) const;

	/** Returns true if this tag is directly in the dictionary already */
	bool IsDictionaryTag(FName TagName) const;

	/** Returns comment and source for tag. If not found return false */
	bool GetTagEditorData(FName TagName, FString& OutComment, FName &OutTagSource) const;

	/** Refresh the gameplaytag tree due to an editor change */
	void EditorRefreshGameplayTagTree();

	/** Gets a Tag Container containing the all tags in the hierarchy that are children of this tag, and were explicitly added to the dictionary */
	FGameplayTagContainer RequestGameplayTagChildrenInDictionary(const FGameplayTag& GameplayTag) const;

	/** This is called when EditorRefreshGameplayTagTree. Useful if you need to do anything editor related when tags are added or removed */
	static FSimpleMulticastDelegate OnEditorRefreshGameplayTagTree;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameplayTagDoubleClickedEditor, FGameplayTag, FSimpleMulticastDelegate& /* OUT */)
	FOnGameplayTagDoubleClickedEditor OnGatherGameplayTagDoubleClickedEditor;

	/** Chance to dynamically change filter string based on a property handle */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGetCategoriesMetaFromPropertyHandle, TSharedPtr<IPropertyHandle>, FString& /* OUT */)
	FOnGetCategoriesMetaFromPropertyHandle OnGetCategoriesMetaFromPropertyHandle;

	/** Allows dynamic hiding of gameplay tags in SGameplayTagWidget. Allows higher order structs to dynamically change which tags are visible based on its own data */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFilterGameplayTagChildren, const FString&  /** FilterString */, TSharedPtr<FGameplayTagNode>& /* TagNode */, bool& /* OUT OutShouldHide */)
	FOnFilterGameplayTagChildren OnFilterGameplayTagChildren;
	
	void NotifyGameplayTagDoubleClickedEditor(FString TagName);
	
	bool ShowGameplayTagAsHyperLinkEditor(FString TagName);


#endif //WITH_EDITOR

	DEPRECATED(4.15, "Call MatchesTag on FGameplayTag instead")
	FORCEINLINE_DEBUGGABLE bool GameplayTagsMatch(const FGameplayTag& GameplayTagOne, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& GameplayTagTwo, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
	{
		SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_GameplayTagsMatch);
		bool bResult = false;
		if (MatchTypeOne == EGameplayTagMatchType::Explicit && MatchTypeTwo == EGameplayTagMatchType::Explicit)
		{
			bResult = GameplayTagOne == GameplayTagTwo;
		}
		else
		{
			// Convert both to their containers and do that match
			const FGameplayTagContainer* ContainerOne = GetSingleTagContainer(GameplayTagOne);
			const FGameplayTagContainer* ContainerTwo = GetSingleTagContainer(GameplayTagTwo);
			if (ContainerOne && ContainerTwo)
			{
				bResult = ContainerOne->DoesTagContainerMatch(*ContainerTwo, MatchTypeOne, MatchTypeTwo, EGameplayContainerMatchType::Any);
			}
		}
		return bResult;
	}

	void PrintReplicationIndices();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Mechanism for tracking what tags are frequently replicated */

	void PrintReplicationFrequencyReport();
	void NotifyTagReplicated(FGameplayTag Tag, bool WasInContainer);

	TMap<FGameplayTag, int32>	ReplicationCountMap;
	TMap<FGameplayTag, int32>	ReplicationCountMap_SingleTags;
	TMap<FGameplayTag, int32>	ReplicationCountMap_Containers;
#endif

private:

	/** Initializes the manager */
	static void InitializeManager();

	/** finished loading/adding native tags */
	static FSimpleMulticastDelegate& OnDoneAddingNativeTagsDelegate();

	/** The Tag Manager singleton */
	static UGameplayTagsManager* SingletonManager;

	friend class FGameplayTagTest;
	friend class FGameplayEffectsTest;
	friend class FGameplayTagsModule;
	friend class FGameplayTagsEditorModule;

	/**
	 * Helper function to insert a tag into a tag node array
	 *
	 * @param Tag					Tag to insert
	 * @param ParentNode			Parent node, if any, for the tag
	 * @param NodeArray				Node array to insert the new node into, if necessary (if the tag already exists, no insertion will occur)
	 * @param SourceName			File tag was added from
	 * @param DevComment			Comment from developer about this tag
	 *
	 * @return Index of the node of the tag
	 */
	int32 InsertTagIntoNodeArray(FName Tag, TSharedPtr<FGameplayTagNode> ParentNode, TArray< TSharedPtr<FGameplayTagNode> >& NodeArray, FName SourceName, const FString& DevComment);

	/** Helper function to populate the tag tree from each table */
	void PopulateTreeFromDataTable(class UDataTable* Table);

	void AddTagTableRow(const FGameplayTagTableRow& TagRow, FName SourceName);

	void AddChildrenTags(FGameplayTagContainer& TagContainer, TSharedPtr<FGameplayTagNode> GameplayTagNode, bool RecurseAll=true, bool OnlyIncludeDictionaryTags=false) const;

	/**
	 * Helper function for GameplayTagsMatch to get all parents when doing a parent match,
	 * NOTE: Must never be made public as it uses the FNames which should never be exposed
	 * 
	 * @param NameList		The list we are adding all parent complete names too
	 * @param GameplayTag	The current Tag we are adding to the list
	 */
	void GetAllParentNodeNames(TSet<FName>& NamesList, TSharedPtr<FGameplayTagNode> GameplayTag) const;

	/** Returns the tag source for a given tag source name, or null if not found */
	FGameplayTagSource* FindOrAddTagSource(FName TagSourceName, EGameplayTagSourceType SourceType);

	/** Constructs the net indices for each tag */
	void ConstructNetIndex();

	/** Roots of gameplay tag nodes */
	TSharedPtr<FGameplayTagNode> GameplayRootTag;

	/** Map of Tags to Nodes - Internal use only. FGameplayTag is inside node structure, do not use FindKey! */
	TMap<FGameplayTag, TSharedPtr<FGameplayTagNode>> GameplayTagNodeMap;

	/** Our aggregated, sorted list of commonly replicated tags. These tags are given lower indices to ensure they replicate in the first bit segment. */
	TArray<FGameplayTag> CommonlyReplicatedTags;

	/** List of gameplay tag sources */
	UPROPERTY()
	TArray<FGameplayTagSource> TagSources;

	/** List of native tags to add when reconstructing tree */
	TSet<FName> NativeTagsToAdd;

	/** Cached runtime value for whether we are using fast replication or not. Initialized from config setting. */
	bool bUseFastReplication;

	/** Cached runtime value for whether we should warn when loading invalid tags */
	bool bShouldWarnOnInvalidTags;

	/** True if native tags have all been added and flushed */
	bool bDoneAddingNativeTags;

#if WITH_EDITOR
	// This critical section is to handle an editor-only issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
	// This class is not generically threadsafe.
	mutable FCriticalSection GameplayTagMapCritical;

	// Transient editor-only tags to support quick-iteration PIE workflows
	TSet<FName> TransientEditorTags;
#endif

	/** Sorted list of nodes, used for network replication */
	TArray<TSharedPtr<FGameplayTagNode>> NetworkGameplayTagNodeIndex;

	/** Holds all of the valid gameplay-related tags that can be applied to assets */
	UPROPERTY()
	TArray<UDataTable*> GameplayTagTables;

	/** The map of ini-configured tag redirectors */
	TMap<FName, FGameplayTag> TagRedirects;
};
