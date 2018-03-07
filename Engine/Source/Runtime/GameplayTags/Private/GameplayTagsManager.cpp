// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/CoreDelegates.h"

#if WITH_EDITOR
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"
#include "Editor.h"
#include "PropertyHandle.h"
FSimpleMulticastDelegate UGameplayTagsManager::OnEditorRefreshGameplayTagTree;
#endif
#include "IConsoleManager.h"


#define LOCTEXT_NAMESPACE "GameplayTagManager"

UGameplayTagsManager* UGameplayTagsManager::SingletonManager = nullptr;

UGameplayTagsManager::UGameplayTagsManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFastReplication = false;
	bShouldWarnOnInvalidTags = true;
	bDoneAddingNativeTags = false;
	NetIndexFirstBitSegment = 16;
	NetIndexTrueBitNum = 16;
	NumBitsForContainerSize = 6;
}

void UGameplayTagsManager::LoadGameplayTagTables()
{
	GameplayTagTables.Empty();

	UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();

	for (FSoftObjectPath DataTablePath : MutableDefault->GameplayTagTableList)
	{
		UDataTable* TagTable = LoadObject<UDataTable>(nullptr, *DataTablePath.ToString(), nullptr, LOAD_None, nullptr);

		// Handle case where the module is dynamically-loaded within a LoadPackage stack, which would otherwise
		// result in the tag table not having its RowStruct serialized in time. Without the RowStruct, the tags manager
		// will not be initialized correctly.
		if (TagTable && IsLoading())
		{
			FLinkerLoad* TagLinker = TagTable->GetLinker();
			if (TagLinker)
			{
				TagTable->GetLinker()->Preload(TagTable);
			}
		}
		GameplayTagTables.Add(TagTable);
	}
}

struct FCompareFGameplayTagNodeByTag
{
	FORCEINLINE bool operator()( const TSharedPtr<FGameplayTagNode>& A, const TSharedPtr<FGameplayTagNode>& B ) const
	{
		// Note: GetSimpleTagName() is not good enough here. The individual tag nodes are share frequently (E.g, Dog.Tail, Cat.Tail have sub nodes with the same simple tag name)
		// Compare with equal FNames will look at the backing number/indice to the FName. For FNames used elsewhere, like "A" for example, this can cause non determinism in platforms
		// (For example if static order initialization differs on two platforms, the "version" of the "A" FName that two places get could be different, causing this comparison to also be)
		return (A->GetCompleteTagName().Compare(B->GetCompleteTagName())) < 0;
	}
};

void UGameplayTagsManager::ConstructGameplayTagTree()
{
	if (!GameplayRootTag.IsValid())
	{
		GameplayRootTag = MakeShareable(new FGameplayTagNode());

		// Add native tags first
		for (FName TagToAdd : NativeTagsToAdd)
		{
			AddTagTableRow(FGameplayTagTableRow(TagToAdd), FGameplayTagSource::GetNativeName());
		}

		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: Construct from data asset"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		
			for (auto It(GameplayTagTables.CreateIterator()); It; It++)
			{
				if (*It)
				{
					PopulateTreeFromDataTable(*It);
				}
			}
		}

		UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();
		FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

		// Create native source
		FindOrAddTagSource(FGameplayTagSource::GetNativeName(), EGameplayTagSourceType::Native);

		if (ShouldImportTagsFromINI())
		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: ImportINI"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			// Copy from deprecated list in DefaultEngine.ini
			TArray<FString> EngineConfigTags;
			GConfig->GetArray(TEXT("/Script/GameplayTags.GameplayTagsSettings"), TEXT("+GameplayTags"), EngineConfigTags, DefaultEnginePath);
			
			for (const FString& EngineConfigTag : EngineConfigTags)
			{
				MutableDefault->GameplayTagList.AddUnique(FGameplayTagTableRow(FName(*EngineConfigTag)));
			}

			// Copy from deprecated list in DefaultGamplayTags.ini
			EngineConfigTags.Empty();
			GConfig->GetArray(TEXT("/Script/GameplayTags.GameplayTagsSettings"), TEXT("+GameplayTags"), EngineConfigTags, MutableDefault->GetDefaultConfigFilename());

			for (const FString& EngineConfigTag : EngineConfigTags)
			{
				MutableDefault->GameplayTagList.AddUnique(FGameplayTagTableRow(FName(*EngineConfigTag)));
			}

#if WITH_EDITOR
			MutableDefault->SortTags();
#endif

			FName TagSource = FGameplayTagSource::GetDefaultName();
			FGameplayTagSource* DefaultSource = FindOrAddTagSource(TagSource, EGameplayTagSourceType::DefaultTagList);

			for (const FGameplayTagTableRow& TableRow : MutableDefault->GameplayTagList)
			{
				AddTagTableRow(TableRow, TagSource);
			}

			// Extra tags
		
			// Read all tags from the ini
			TArray<FString> FilesInDirectory;
			IFileManager::Get().FindFilesRecursive(FilesInDirectory, *(FPaths::ProjectConfigDir() / TEXT("Tags")), TEXT("*.ini"), true, false);
			FilesInDirectory.Sort();
			for (FString& FileName : FilesInDirectory)
			{
				TagSource = FName(*FPaths::GetCleanFilename(FileName));
				FGameplayTagSource* FoundSource = FindOrAddTagSource(TagSource, EGameplayTagSourceType::TagList);

				UE_LOG(LogGameplayTags, Display, TEXT("Loading Tag File: %s"), *FileName);

				// Check deprecated locations
				TArray<FString> Tags;
				if (GConfig->GetArray(TEXT("UserTags"), TEXT("GameplayTags"), Tags, FileName))
				{
					for (const FString& Tag : Tags)
					{
						FoundSource->SourceTagList->GameplayTagList.AddUnique(FGameplayTagTableRow(FName(*Tag)));
					}
				}
				else
				{
					// Load from new ini
					FoundSource->SourceTagList->LoadConfig(UGameplayTagsList::StaticClass(), *FileName);
				}

#if WITH_EDITOR
				if (GIsEditor || IsRunningCommandlet()) // Sort tags for UI Purposes but don't sort in -game scenerio since this would break compat with noneditor cooked builds
				{
					FoundSource->SourceTagList->SortTags();
				}
#endif

				for (const FGameplayTagTableRow& TableRow : FoundSource->SourceTagList->GameplayTagList)
				{
					AddTagTableRow(TableRow, TagSource);
				}
			}
		}

#if WITH_EDITOR
		// Add any transient editor-only tags
		for (FName TransientTag : TransientEditorTags)
		{
			AddTagTableRow(FGameplayTagTableRow(TransientTag), FGameplayTagSource::GetTransientEditorName());
		}
#endif

		// Grab the commonly replicated tags
		CommonlyReplicatedTags.Empty();
		for (FName TagName : MutableDefault->CommonlyReplicatedTags)
		{
			FGameplayTag Tag = RequestGameplayTag(TagName);
			if (Tag.IsValid())
			{
				CommonlyReplicatedTags.Add(Tag);
			}
			else
			{
				UE_LOG(LogGameplayTags, Warning, TEXT("%s was found in the CommonlyReplicatedTags list but doesn't appear to be a valid tag!"), *TagName.ToString());
			}
		}

		bUseFastReplication = MutableDefault->FastReplication;
		bShouldWarnOnInvalidTags = MutableDefault->WarnOnInvalidTags;
		NumBitsForContainerSize = MutableDefault->NumBitsForContainerSize;
		NetIndexFirstBitSegment = MutableDefault->NetIndexFirstBitSegment;

		if (ShouldUseFastReplication())
		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: Reconstruct NetIndex"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			ConstructNetIndex();
		}

		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: GameplayTagTreeChangedEvent.Broadcast"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			IGameplayTagsModule::OnGameplayTagTreeChanged.Broadcast();
		}

		// Update the TagRedirects map
		TagRedirects.Empty();

		// Check the deprecated location
		bool bFoundDeprecated = false;
		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("/Script/Engine.Engine"), false, true, DefaultEnginePath);

		if (PackageRedirects)
		{
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("+GameplayTagRedirects"))
				{
					FName OldTagName = NAME_None;
					FName NewTagName;

					if (FParse::Value(*It.Value().GetValue(), TEXT("OldTagName="), OldTagName))
					{
						if (FParse::Value(*It.Value().GetValue(), TEXT("NewTagName="), NewTagName))
						{
							FGameplayTagRedirect Redirect;
							Redirect.OldTagName = OldTagName;
							Redirect.NewTagName = NewTagName;

							MutableDefault->GameplayTagRedirects.AddUnique(Redirect);

							bFoundDeprecated = true;
						}
					}
				}
			}
		}

		if (bFoundDeprecated)
		{
			UE_LOG(LogGameplayTags, Log, TEXT("GameplayTagRedirects is in a deprecated location, after editing GameplayTags developer settings you must remove these manually"));
		}

		// Check settings object
		for (const FGameplayTagRedirect& Redirect : MutableDefault->GameplayTagRedirects)
		{
			FName OldTagName = Redirect.OldTagName;
			FName NewTagName = Redirect.NewTagName;

			if (ensureMsgf(!TagRedirects.Contains(OldTagName), TEXT("Old tag %s is being redirected to more than one tag. Please remove all the redirections except for one."), *OldTagName.ToString()))
			{
				FGameplayTag OldTag = RequestGameplayTag(OldTagName, false); //< This only succeeds if OldTag is in the Table!
				if (OldTag.IsValid())
				{
					FGameplayTagContainer MatchingChildren = RequestGameplayTagChildren(OldTag);

					FString Msg = FString::Printf(TEXT("Old tag (%s) which is being redirected still exists in the table!  Generally you should "
						TEXT("remove the old tags from the table when you are redirecting to new tags, or else users will ")
						TEXT("still be able to add the old tags to containers.")), *OldTagName.ToString());

					if (MatchingChildren.Num() == 0)
					{
						UE_LOG(LogGameplayTags, Warning, TEXT("%s"), *Msg);
					}
					else
					{
						Msg += TEXT("\nSuppressed warning due to redirected tag being a single component that matched other hierarchy elements.");
						UE_LOG(LogGameplayTags, Log, TEXT("%s"), *Msg);
					}
				}

				FGameplayTag NewTag = (NewTagName != NAME_None) ? RequestGameplayTag(NewTagName, false) : FGameplayTag();

				// Basic infinite recursion guard
				int32 IterationsLeft = 10;
				while (!NewTag.IsValid() && NewTagName != NAME_None)
				{
					bool bFoundRedirect = false;

					// See if it got redirected again
					for (const FGameplayTagRedirect& SecondRedirect : MutableDefault->GameplayTagRedirects)
					{
						if (SecondRedirect.OldTagName == NewTagName)
						{
							NewTagName = SecondRedirect.NewTagName;
							NewTag = RequestGameplayTag(NewTagName, false);
							bFoundRedirect = true;
							break;
						}
					}
					IterationsLeft--;

					if (!bFoundRedirect || IterationsLeft <= 0)
					{
						UE_LOG(LogGameplayTags, Warning, TEXT("Invalid new tag %s!  Cannot replace old tag %s."),
							*Redirect.NewTagName.ToString(), *Redirect.OldTagName.ToString());
						break;
					}
				}

				if (NewTag.IsValid())
				{
					// Populate the map
					TagRedirects.Add(OldTagName, NewTag);
				}
			}
		}
	}
}

int32 PrintNetIndiceAssignment = 0;
static FAutoConsoleVariableRef CVarPrintNetIndiceAssignment(TEXT("GameplayTags.PrintNetIndiceAssignment"), PrintNetIndiceAssignment, TEXT("Logs GameplayTag NetIndice assignment"), ECVF_Default );
void UGameplayTagsManager::ConstructNetIndex()
{
	NetworkGameplayTagNodeIndex.Empty();

	GameplayTagNodeMap.GenerateValueArray(NetworkGameplayTagNodeIndex);

	NetworkGameplayTagNodeIndex.Sort(FCompareFGameplayTagNodeByTag());

	check(CommonlyReplicatedTags.Num() <= NetworkGameplayTagNodeIndex.Num());

	// Put the common indices up front
	for (int32 CommonIdx=0; CommonIdx < CommonlyReplicatedTags.Num(); ++CommonIdx)
	{
		int32 BaseIdx=0;
		FGameplayTag& Tag = CommonlyReplicatedTags[CommonIdx];

		bool Found = false;
		for (int32 findidx=0; findidx < NetworkGameplayTagNodeIndex.Num(); ++findidx)
		{
			if (NetworkGameplayTagNodeIndex[findidx]->GetCompleteTag() == Tag)
			{
				NetworkGameplayTagNodeIndex.Swap(findidx, CommonIdx);
				Found = true;
				break;
			}
		}

		// A non fatal error should have been thrown when parsing the CommonlyReplicatedTags list. If we make it here, something is seriously wrong.
		checkf( Found, TEXT("Tag %s not found in NetworkGameplayTagNodeIndex"), *Tag.ToString() );
	}

	InvalidTagNetIndex = NetworkGameplayTagNodeIndex.Num()+1;
	NetIndexTrueBitNum = FMath::CeilToInt(FMath::Log2(InvalidTagNetIndex));
	
	// This should never be smaller than NetIndexTrueBitNum
	NetIndexFirstBitSegment = FMath::Min<int64>(NetIndexFirstBitSegment, NetIndexTrueBitNum);

	// This is now sorted and it should be the same on both client and server
	if (NetworkGameplayTagNodeIndex.Num() >= INVALID_TAGNETINDEX)
	{
		ensureMsgf(false, TEXT("Too many tags in dictionary for networking! Remove tags or increase tag net index size"));

		NetworkGameplayTagNodeIndex.SetNum(INVALID_TAGNETINDEX - 1);
	}



	UE_CLOG(PrintNetIndiceAssignment, LogGameplayTags, Display, TEXT("Assigning NetIndices to %d tags."), NetworkGameplayTagNodeIndex.Num() );

	for (FGameplayTagNetIndex i = 0; i < NetworkGameplayTagNodeIndex.Num(); i++)
	{
		if (NetworkGameplayTagNodeIndex[i].IsValid())
		{
			NetworkGameplayTagNodeIndex[i]->NetIndex = i;

			UE_CLOG(PrintNetIndiceAssignment, LogGameplayTags, Display, TEXT("Assigning NetIndex (%d) to Tag (%s)"), i, *NetworkGameplayTagNodeIndex[i]->GetCompleteTag().ToString());
		}
		else
		{
			UE_LOG(LogGameplayTags, Warning, TEXT("TagNode Indice %d is invalid!"), i);
		}
	}
}

FName UGameplayTagsManager::GetTagNameFromNetIndex(FGameplayTagNetIndex Index) const
{
	if (Index >= NetworkGameplayTagNodeIndex.Num())
	{
		// Ensure Index is the invalid index. If its higher than that, then something is wrong.
		ensureMsgf(Index == InvalidTagNetIndex, TEXT("Received invalid tag net index %d! Tag index is out of sync on client!"), Index);
		return NAME_None;
	}
	return NetworkGameplayTagNodeIndex[Index]->GetCompleteTagName();
}

FGameplayTagNetIndex UGameplayTagsManager::GetNetIndexFromTag(const FGameplayTag &InTag) const
{
	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(InTag);

	if (GameplayTagNode.IsValid())
	{
		return GameplayTagNode->GetNetIndex();
	}

	return InvalidTagNetIndex;
}

bool UGameplayTagsManager::ShouldImportTagsFromINI() const
{
	UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();
	FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

	// Deprecated path
	bool ImportFromINI = false;
	if (GConfig->GetBool(TEXT("GameplayTags"), TEXT("ImportTagsFromConfig"), ImportFromINI, DefaultEnginePath))
	{
		if (ImportFromINI)
		{
			MutableDefault->ImportTagsFromConfig = ImportFromINI;
			UE_LOG(LogGameplayTags, Log, TEXT("ImportTagsFromConfig is in a deprecated location, open and save GameplayTag settings to fix"));
		}
		return ImportFromINI;
	}

	return MutableDefault->ImportTagsFromConfig;
}

void UGameplayTagsManager::RedirectTagsForContainer(FGameplayTagContainer& Container, UProperty* SerializingProperty) const
{
	TSet<FName> NamesToRemove;
	TSet<const FGameplayTag*> TagsToAdd;

	// First populate the NamesToRemove and TagsToAdd sets by finding tags in the container that have redirects
	for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FName TagName = TagIt->GetTagName();
		const FGameplayTag* NewTag = TagRedirects.Find(TagName);
		if (NewTag)
		{
			NamesToRemove.Add(TagName);
			if (NewTag->IsValid())
			{
				TagsToAdd.Add(NewTag);
			}
		}
#if WITH_EDITOR
		else if (SerializingProperty)
		{
			// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
			FGameplayTag OldTag = RequestGameplayTag(TagName, false);
			if (!OldTag.IsValid() && ShouldWarnOnInvalidTags())
			{
				UE_LOG(LogGameplayTags, Warning, TEXT("Invalid GameplayTag %s found while loading property %s."), *TagName.ToString(), *GetPathNameSafe(SerializingProperty));
			}
		}
#endif
	}

	// Remove all tags from the NamesToRemove set
	for (FName RemoveName : NamesToRemove)
	{
		FGameplayTag OldTag = RequestGameplayTag(RemoveName, false);
		if (OldTag.IsValid())
		{
			Container.RemoveTag(OldTag);
		}
		else
		{
			Container.RemoveTagByExplicitName(RemoveName);
		}
	}

	// Add all tags from the TagsToAdd set
	for (const FGameplayTag* AddTag : TagsToAdd)
	{
		check(AddTag);
		Container.AddTag(*AddTag);
	}
}

void UGameplayTagsManager::RedirectSingleGameplayTag(FGameplayTag& Tag, UProperty* SerializingProperty) const
{
	const FName TagName = Tag.GetTagName();
	const FGameplayTag* NewTag = TagRedirects.Find(TagName);
	if (NewTag)
	{
		if (NewTag->IsValid())
		{
			Tag = *NewTag;
		}
	}
#if WITH_EDITOR
	else if (TagName != NAME_None && SerializingProperty)
	{
		// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
		FGameplayTag OldTag = RequestGameplayTag(TagName, false);
		if (!OldTag.IsValid() && ShouldWarnOnInvalidTags())
		{
			UE_LOG(LogGameplayTags, Warning, TEXT("Invalid GameplayTag %s found while loading property %s."), *TagName.ToString(), *GetPathNameSafe(SerializingProperty));
		}
	}
#endif
}

void UGameplayTagsManager::InitializeManager()
{
	check(!SingletonManager);

	SingletonManager = NewObject<UGameplayTagsManager>(GetTransientPackage(), NAME_None);
	SingletonManager->AddToRoot();

	UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();
	FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

	TArray<FString> GameplayTagTables;
	GConfig->GetArray(TEXT("GameplayTags"), TEXT("+GameplayTagTableList"), GameplayTagTables, DefaultEnginePath);

	// Report deprecation
	if (GameplayTagTables.Num() > 0)
	{
		UE_LOG(LogGameplayTags, Log, TEXT("GameplayTagTableList is in a deprecated location, open and save GameplayTag settings to fix"));
		for (const FString& DataTable : GameplayTagTables)
		{
			MutableDefault->GameplayTagTableList.AddUnique(DataTable);
		}
	}

	SingletonManager->LoadGameplayTagTables();
	SingletonManager->ConstructGameplayTagTree();

	// Bind to end of engine init to be done adding native tags
	FCoreDelegates::OnPostEngineInit.AddUObject(SingletonManager, &UGameplayTagsManager::DoneAddingNativeTags);
}

void UGameplayTagsManager::PopulateTreeFromDataTable(class UDataTable* InTable)
{
	checkf(GameplayRootTag.IsValid(), TEXT("ConstructGameplayTagTree() must be called before PopulateTreeFromDataTable()"));
	static const FString ContextString(TEXT("UGameplayTagsManager::PopulateTreeFromDataTable"));
	
	TArray<FGameplayTagTableRow*> TagTableRows;
	InTable->GetAllRows<FGameplayTagTableRow>(ContextString, TagTableRows);

	FName SourceName = InTable->GetOutermost()->GetFName();

	FGameplayTagSource* FoundSource = FindOrAddTagSource(SourceName, EGameplayTagSourceType::DataTable);

	for (const FGameplayTagTableRow* TagRow : TagTableRows)
	{
		if (TagRow)
		{
			AddTagTableRow(*TagRow, SourceName);
		}
	}
}

void UGameplayTagsManager::AddTagTableRow(const FGameplayTagTableRow& TagRow, FName SourceName)
{
	TSharedPtr<FGameplayTagNode> CurNode = GameplayRootTag;

	// Split the tag text on the "." delimiter to establish tag depth and then insert each tag into the
	// gameplay tag tree
	TArray<FString> SubTags;
	TagRow.Tag.ToString().ParseIntoArray(SubTags, TEXT("."), true);

	for (int32 SubTagIdx = 0; SubTagIdx < SubTags.Num(); ++SubTagIdx)
	{
		TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = CurNode.Get()->GetChildTagNodes();

		bool bFromDictionary = (SubTagIdx == (SubTags.Num() - 1));
		int32 InsertionIdx = InsertTagIntoNodeArray(*SubTags[SubTagIdx], CurNode, ChildTags, bFromDictionary ? SourceName : NAME_None, TagRow.DevComment);

		CurNode = ChildTags[InsertionIdx];
	}
}

UGameplayTagsManager::~UGameplayTagsManager()
{
	DestroyGameplayTagTree();
	SingletonManager = nullptr;
}

void UGameplayTagsManager::DestroyGameplayTagTree()
{
	if (GameplayRootTag.IsValid())
	{
		GameplayRootTag->ResetNode();
		GameplayRootTag.Reset();
		GameplayTagNodeMap.Reset();
	}
}

int32 UGameplayTagsManager::InsertTagIntoNodeArray(FName Tag, TSharedPtr<FGameplayTagNode> ParentNode, TArray< TSharedPtr<FGameplayTagNode> >& NodeArray, FName SourceName, const FString& DevComment)
{
	int32 InsertionIdx = INDEX_NONE;
	int32 WhereToInsert = INDEX_NONE;

	// See if the tag is already in the array
	for (int32 CurIdx = 0; CurIdx < NodeArray.Num(); ++CurIdx)
	{
		if (NodeArray[CurIdx].IsValid())
		{
			if (NodeArray[CurIdx].Get()->GetSimpleTagName() == Tag)
			{
				InsertionIdx = CurIdx;
				break;
			}
			else if (NodeArray[CurIdx].Get()->GetSimpleTagName() > Tag && WhereToInsert == INDEX_NONE)
			{
				// Insert new node before this
				WhereToInsert = CurIdx;
			}
		}
	}

	if (InsertionIdx == INDEX_NONE)
	{
		if (WhereToInsert == INDEX_NONE)
		{
			// Insert at end
			WhereToInsert = NodeArray.Num();
		}

		// Don't add the root node as parent
		TSharedPtr<FGameplayTagNode> TagNode = MakeShareable(new FGameplayTagNode(Tag, ParentNode != GameplayRootTag ? ParentNode : nullptr));

		// Add at the sorted location
		InsertionIdx = NodeArray.Insert(TagNode, WhereToInsert);

		FGameplayTag GameplayTag = TagNode->GetCompleteTag();

		{
#if WITH_EDITOR
			// This critical section is to handle an editor-only issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
			// This function is not generically threadsafe.
			FScopeLock Lock(&GameplayTagMapCritical);
#endif
			GameplayTagNodeMap.Add(GameplayTag, TagNode);
		}
	}

#if WITH_EDITOR
	static FName NativeSourceName = FGameplayTagSource::GetNativeName();
	// Set/update editor only data
	if (NodeArray[InsertionIdx]->SourceName.IsNone() && !SourceName.IsNone())
	{
		NodeArray[InsertionIdx]->SourceName = SourceName;
	}
	else if (SourceName == NativeSourceName)
	{
		// Native overrides other types
		NodeArray[InsertionIdx]->SourceName = SourceName;
	}

	if (NodeArray[InsertionIdx]->DevComment.IsEmpty() && !DevComment.IsEmpty())
	{
		NodeArray[InsertionIdx]->DevComment = DevComment;
	}
#endif

	return InsertionIdx;
}

void UGameplayTagsManager::PrintReplicationIndices()
{

	UE_LOG(LogGameplayTags, Display, TEXT("::PrintReplicationIndices (TOTAL %d"), GameplayTagNodeMap.Num());

	for (auto It : GameplayTagNodeMap)
	{
		FGameplayTag Tag = It.Key;
		TSharedPtr<FGameplayTagNode> Node = It.Value;

		UE_LOG(LogGameplayTags, Display, TEXT("Tag %s NetIndex: %d"), *Tag.ToString(), Node->GetNetIndex());
	}

}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UGameplayTagsManager::PrintReplicationFrequencyReport()
{
	UE_LOG(LogGameplayTags, Warning, TEXT("================================="));
	UE_LOG(LogGameplayTags, Warning, TEXT("Gameplay Tags Replication Report"));

	UE_LOG(LogGameplayTags, Warning, TEXT("\nTags replicated solo:"));
	ReplicationCountMap_SingleTags.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_SingleTags)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}
	
	// ---------------------------------------

	UE_LOG(LogGameplayTags, Warning, TEXT("\nTags replicated in containers:"));
	ReplicationCountMap_Containers.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_Containers)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	// ---------------------------------------

	UE_LOG(LogGameplayTags, Warning, TEXT("\nAll Tags replicated:"));
	ReplicationCountMap.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	TMap<int32, int32> SavingsMap;
	int32 BaselineCost = 0;
	for (int32 Bits=1; Bits < NetIndexTrueBitNum; ++Bits)
	{
		int32 TotalSavings = 0;
		BaselineCost = 0;

		FGameplayTagNetIndex ExpectedNetIndex=0;
		for (auto& It : ReplicationCountMap)
		{
			int32 ExpectedCostBits = 0;
			bool FirstSeg = ExpectedNetIndex < FMath::Pow(2, Bits);
			if (FirstSeg)
			{
				// This would fit in the first Bits segment
				ExpectedCostBits = Bits+1;
			}
			else
			{
				// Would go in the second segment, so we pay the +1 cost
				ExpectedCostBits = NetIndexTrueBitNum+1;
			}

			int32 Savings = (NetIndexTrueBitNum - ExpectedCostBits) * It.Value;
			BaselineCost += NetIndexTrueBitNum * It.Value;

			//UE_LOG(LogGameplayTags, Warning, TEXT("[Bits: %d] Tag %s would save %d bits"), Bits, *It.Key.ToString(), Savings);
			ExpectedNetIndex++;
			TotalSavings += Savings;
		}

		SavingsMap.FindOrAdd(Bits) = TotalSavings;
	}

	SavingsMap.ValueSort(TGreater<int32>());
	int32 BestBits = 0;
	for (auto& It : SavingsMap)
	{
		if (BestBits == 0)
		{
			BestBits = It.Key;
		}

		UE_LOG(LogGameplayTags, Warning, TEXT("%d bits would save %d (%.2f)"), It.Key, It.Value, (float)It.Value / (float)BaselineCost);
	}

	UE_LOG(LogGameplayTags, Warning, TEXT("\nSuggested config:"));

	// Write out a nice copy pastable config
	int32 Count=0;
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("+CommonlyReplicatedTags=%s"), *It.Key.ToString());

		if (Count == FMath::Pow(2, BestBits))
		{
			// Print a blank line out, indicating tags after this are not necessary but still may be useful if the user wants to manually edit the list.
			UE_LOG(LogGameplayTags, Warning, TEXT(""));
		}

		if (Count++ >= FMath::Pow(2, BestBits+1))
		{
			break;
		}
	}

	UE_LOG(LogGameplayTags, Warning, TEXT("NetIndexFirstBitSegment=%d"), BestBits);

	UE_LOG(LogGameplayTags, Warning, TEXT("================================="));
}

void UGameplayTagsManager::NotifyTagReplicated(FGameplayTag Tag, bool WasInContainer)
{
	ReplicationCountMap.FindOrAdd(Tag)++;

	if (WasInContainer)
	{
		ReplicationCountMap_Containers.FindOrAdd(Tag)++;
	}
	else
	{
		ReplicationCountMap_SingleTags.FindOrAdd(Tag)++;
	}
	
}
#endif

#if WITH_EDITOR

static void RecursiveRootTagSearch(const FString& InFilterString, const TArray<TSharedPtr<FGameplayTagNode> >& GameplayRootTags, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray)
{
	FString CurrentFilter, RestOfFilter;
	if (!InFilterString.Split(TEXT("."), &CurrentFilter, &RestOfFilter))
	{
		CurrentFilter = InFilterString;
	}

	for (int32 iTag = 0; iTag < GameplayRootTags.Num(); ++iTag)
	{
		FString RootTagName = GameplayRootTags[iTag].Get()->GetSimpleTagName().ToString();

		if (RootTagName.Equals(CurrentFilter) == true)
		{
			if (RestOfFilter.IsEmpty())
			{
				// We've reached the end of the filter, add tags
				OutTagArray.Add(GameplayRootTags[iTag]);
			}
			else
			{
				// Recurse into our children
				RecursiveRootTagSearch(RestOfFilter, GameplayRootTags[iTag]->GetChildTagNodes(), OutTagArray);
			}
		}		
	}
}

void UGameplayTagsManager::GetFilteredGameplayRootTags(const FString& InFilterString, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray) const
{
	TArray<FString> PreRemappedFilters;
	TArray<FString> Filters;
	TArray<TSharedPtr<FGameplayTagNode>>& GameplayRootTags = GameplayRootTag->GetChildTagNodes();

	OutTagArray.Empty();
	if( InFilterString.ParseIntoArray( PreRemappedFilters, TEXT( "," ), true ) > 0 )
	{
		const UGameplayTagsSettings* CDO = GetDefault<UGameplayTagsSettings>();
		for (FString& Str : PreRemappedFilters)
		{
			bool Remapped = false;
			for (const FGameplayTagCategoryRemap& RemapInfo : CDO->CategoryRemapping)
			{
				if (RemapInfo.BaseCategory == Str)
				{
					Remapped = true;
					Filters.Append(RemapInfo.RemapCategories);
				}
			}
			if (Remapped == false)
			{
				Filters.Add(Str);
			}
		}		

		// Check all filters in the list
		for (int32 iFilter = 0; iFilter < Filters.Num(); ++iFilter)
		{
			RecursiveRootTagSearch(Filters[iFilter], GameplayRootTags, OutTagArray);
		}

		if (OutTagArray.Num() == 0)
		{
			// We had filters but nothing matched. Ignore the filters.
			// This makes sense to do with engine level filters that games can optionally specify/override.
			// We never want to impose tag structure on projects, but still give them the ability to do so for their project.
			OutTagArray = GameplayRootTags;
		}

	}
	else
	{
		// No Filters just return them all
		OutTagArray = GameplayRootTags;
	}
}

FString UGameplayTagsManager::GetCategoriesMetaFromPropertyHandle(TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	// Global delegate override. Useful for parent structs that want to override tag categories based on their data (e.g. not static property meta data)
	FString DelegateOverrideString;
	OnGetCategoriesMetaFromPropertyHandle.Broadcast(PropertyHandle, DelegateOverrideString);
	if (DelegateOverrideString.IsEmpty() == false)
	{
		return DelegateOverrideString;
	}


	const FName CategoriesName = TEXT("Categories");
	FString Categories;

	auto GetMetaData = ([&](UField* Field)
	{
		if (Field->HasMetaData(CategoriesName))
		{
			Categories = Field->GetMetaData(CategoriesName);
			return true;
		}

		return false;
	});
	
	while(PropertyHandle.IsValid())
	{
		if (UProperty* Property = PropertyHandle->GetProperty())
		{
			/**
			 *	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories="GameplayCue"))
			 *	FGameplayTag GameplayCueTag;
			 */
			if (GetMetaData(Property))
			{
				break;
			}

			/**
			 *	USTRUCT(meta=(Categories="EventKeyword"))
			 *	struct FGameplayEventKeywordTag : public FGameplayTag
			 */
			if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
			{
				if (GetMetaData(StructProperty->Struct))
				{
					break;
				}
			}

			/**	TArray<FGameplayEventKeywordTag> QualifierTagTestList; */
			if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
			{
				if (GetMetaData(ArrayProperty->Inner))
				{
					break;
				}
			}
		}
		PropertyHandle = PropertyHandle->GetParentHandle();
	}
	
	return Categories;
}

FString UGameplayTagsManager::GetCategoriesMetaFromFunction(UFunction* ThisFunction) const
{
	FString FilterString;
	if (ThisFunction->HasMetaData(TEXT("GameplayTagFilter")))
	{
		FilterString = ThisFunction->GetMetaData(TEXT("GameplayTagFilter"));
	}
	return FilterString;
}

void UGameplayTagsManager::GetAllTagsFromSource(FName TagSource, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray) const
{
	for (const TPair<FGameplayTag, TSharedPtr<FGameplayTagNode>>& NodePair : GameplayTagNodeMap)
	{
		if (NodePair.Value->SourceName == TagSource)
		{
			OutTagArray.Add(NodePair.Value);
		}
	}
}

bool UGameplayTagsManager::IsDictionaryTag(FName TagName) const
{
	TSharedPtr<FGameplayTagNode> Node = FindTagNode(TagName);
	if (Node.IsValid() && Node->SourceName != NAME_None)
	{
		return true;
	}

	return false;
}

bool UGameplayTagsManager::GetTagEditorData(FName TagName, FString& OutComment, FName &OutTagSource) const
{
	TSharedPtr<FGameplayTagNode> Node = FindTagNode(TagName);
	if (Node.IsValid())
	{
		OutComment = Node->DevComment;
		OutTagSource = Node->SourceName;
		return true;
	}
	return false;
}

void UGameplayTagsManager::EditorRefreshGameplayTagTree()
{
	DestroyGameplayTagTree();
	LoadGameplayTagTables();
	ConstructGameplayTagTree();

	OnEditorRefreshGameplayTagTree.Broadcast();
}

FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagChildrenInDictionary(const FGameplayTag& GameplayTag) const
{
	// Note this purposefully does not include the passed in GameplayTag in the container.
	FGameplayTagContainer TagContainer;

	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(GameplayTag);
	if (GameplayTagNode.IsValid())
	{
		AddChildrenTags(TagContainer, GameplayTagNode, true, true);
	}
	return TagContainer;
}

void UGameplayTagsManager::NotifyGameplayTagDoubleClickedEditor(FString TagName)
{
	FGameplayTag Tag = RequestGameplayTag(FName(*TagName), false);
	if(Tag.IsValid())
	{
		FSimpleMulticastDelegate Delegate;
		OnGatherGameplayTagDoubleClickedEditor.Broadcast(Tag, Delegate);
		Delegate.Broadcast();
	}
}

bool UGameplayTagsManager::ShowGameplayTagAsHyperLinkEditor(FString TagName)
{
	FGameplayTag Tag = RequestGameplayTag(FName(*TagName), false);
	if(Tag.IsValid())
	{
		FSimpleMulticastDelegate Delegate;
		OnGatherGameplayTagDoubleClickedEditor.Broadcast(Tag, Delegate);
		return Delegate.IsBound();
	}
	return false;
}

#endif // WITH_EDITOR

const FGameplayTagSource* UGameplayTagsManager::FindTagSource(FName TagSourceName) const
{
	for (const FGameplayTagSource& TagSource : TagSources)
	{
		if (TagSource.SourceName == TagSourceName)
		{
			return &TagSource;
		}
	}
	return nullptr;
}

void UGameplayTagsManager::FindTagSourcesWithType(EGameplayTagSourceType TagSourceType, TArray<const FGameplayTagSource*>& OutArray) const
{
	for (const FGameplayTagSource& TagSource : TagSources)
	{
		if (TagSource.SourceType == TagSourceType)
		{
			OutArray.Add(&TagSource);
		}
	}
}

FGameplayTagSource* UGameplayTagsManager::FindOrAddTagSource(FName TagSourceName, EGameplayTagSourceType SourceType)
{
	const FGameplayTagSource* FoundSource = FindTagSource(TagSourceName);
	if (FoundSource)
	{
		return const_cast<FGameplayTagSource*>(FoundSource);
	}

	// Need to make a new one

	FGameplayTagSource* NewSource = new(TagSources) FGameplayTagSource(TagSourceName, SourceType);

	if (SourceType == EGameplayTagSourceType::DefaultTagList)
	{
		NewSource->SourceTagList = GetMutableDefault<UGameplayTagsSettings>();
	}
	else if (SourceType == EGameplayTagSourceType::TagList)
	{
		NewSource->SourceTagList = NewObject<UGameplayTagsList>(this, TagSourceName, RF_Transient);
		NewSource->SourceTagList->ConfigFileName = FString::Printf(TEXT("%sTags/%s"), *FPaths::SourceConfigDir(), *TagSourceName.ToString());
	}

	return NewSource;
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::RequestGameplayTag"), STAT_UGameplayTagsManager_RequestGameplayTag, STATGROUP_GameplayTags);

FGameplayTag UGameplayTagsManager::RequestGameplayTag(FName TagName, bool ErrorIfNotFound) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_RequestGameplayTag);
#if WITH_EDITOR
	// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
	// This function is not generically threadsafe.
	FScopeLock Lock(&GameplayTagMapCritical);
#endif

	FGameplayTag PossibleTag(TagName);

	if (GameplayTagNodeMap.Contains(PossibleTag))
	{
		return PossibleTag;
	}
	else if (ErrorIfNotFound)
	{
		static TSet<FName> MissingTagName;
		if (!MissingTagName.Contains(TagName))
		{
			ensureAlwaysMsgf(false, TEXT("Requested Tag %s was not found. Check tag data table."), *TagName.ToString());
			MissingTagName.Add(TagName);
		}
	}
	return FGameplayTag();
}

FGameplayTag UGameplayTagsManager::FindGameplayTagFromPartialString_Slow(FString PartialString) const
{
#if WITH_EDITOR
	// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
	// This function is not generically threadsafe.
	FScopeLock Lock(&GameplayTagMapCritical);
#endif

	// Exact match first
	FGameplayTag PossibleTag(*PartialString);
	if (GameplayTagNodeMap.Contains(PossibleTag))
	{
		return PossibleTag;
	}

	// Find shortest tag name that contains the match string
	FGameplayTag FoundTag;
	FGameplayTagContainer AllTags;
	RequestAllGameplayTags(AllTags, false);

	int32 BestMatchLength = MAX_int32;
	for (FGameplayTag MatchTag : AllTags)
	{
		FString Str = MatchTag.ToString();
		if (Str.Contains(PartialString))
		{
			if (Str.Len() < BestMatchLength)
			{
				FoundTag = MatchTag;
				BestMatchLength = Str.Len();
			}
		}
	}
	
	return FoundTag;
}

FGameplayTag UGameplayTagsManager::AddNativeGameplayTag(FName TagName)
{
	if (TagName.IsNone())
	{
		return FGameplayTag();
	}

	// Unsafe to call after done adding
	if (ensure(!bDoneAddingNativeTags))
	{
		FGameplayTag NewTag = FGameplayTag(TagName);

		if (!NativeTagsToAdd.Contains(TagName))
		{
			NativeTagsToAdd.Add(TagName);
		}

		AddTagTableRow(FGameplayTagTableRow(TagName), FGameplayTagSource::GetNativeName());

		return NewTag;
	}

	return FGameplayTag();
}
void UGameplayTagsManager::CallOrRegister_OnDoneAddingNativeTagsDelegate(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (bDoneAddingNativeTags)
	{
		Delegate.Execute();
	}
	else
	{
		bool bAlreadyBound = Delegate.GetUObject() != nullptr ? OnDoneAddingNativeTagsDelegate().IsBoundToObject(Delegate.GetUObject()) : false;
		if (!bAlreadyBound)
		{
			OnDoneAddingNativeTagsDelegate().Add(Delegate);
		}
	}
}

FSimpleMulticastDelegate& UGameplayTagsManager::OnDoneAddingNativeTagsDelegate()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

FSimpleMulticastDelegate& UGameplayTagsManager::OnLastChanceToAddNativeTags()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

void UGameplayTagsManager::DoneAddingNativeTags()
{
	// Safe to call multiple times, only works the first time
	if (!bDoneAddingNativeTags)
	{
		UE_LOG(LogGameplayTags, Display, TEXT("UGameplayTagsManager::DoneAddingNativeTags. DelegateIsBound: %d"), (int32)OnLastChanceToAddNativeTags().IsBound());
		OnLastChanceToAddNativeTags().Broadcast();
		bDoneAddingNativeTags = true;

		if (ShouldUseFastReplication())
		{
			ConstructNetIndex();
		}
		OnDoneAddingNativeTagsDelegate().Broadcast();
	}
}

FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagParents(const FGameplayTag& GameplayTag) const
{
	const FGameplayTagContainer* ParentTags = GetSingleTagContainer(GameplayTag);

	if (ParentTags)
	{
		return ParentTags->GetGameplayTagParents();
	}
	return FGameplayTagContainer();
}

void UGameplayTagsManager::RequestAllGameplayTags(FGameplayTagContainer& TagContainer, bool OnlyIncludeDictionaryTags) const
{
	TArray<TSharedPtr<FGameplayTagNode>> ValueArray;
	GameplayTagNodeMap.GenerateValueArray(ValueArray);
	for (const TSharedPtr<FGameplayTagNode>& TagNode : ValueArray)
	{
#if WITH_EDITOR
		bool DictTag = IsDictionaryTag(TagNode->GetCompleteTagName());
#else
		bool DictTag = false;
#endif 
		if (!OnlyIncludeDictionaryTags || DictTag)
		{
			const FGameplayTag* Tag = GameplayTagNodeMap.FindKey(TagNode);
			check(Tag);
			TagContainer.AddTagFast(*Tag);
		}
	}
}

FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagChildren(const FGameplayTag& GameplayTag) const
{
	FGameplayTagContainer TagContainer;
	// Note this purposefully does not include the passed in GameplayTag in the container.
	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(GameplayTag);
	if (GameplayTagNode.IsValid())
	{
		AddChildrenTags(TagContainer, GameplayTagNode, true, false);
	}
	return TagContainer;
}

FGameplayTag UGameplayTagsManager::RequestGameplayTagDirectParent(const FGameplayTag& GameplayTag) const
{
	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(GameplayTag);
	if (GameplayTagNode.IsValid())
	{
		TSharedPtr<FGameplayTagNode> Parent = GameplayTagNode->GetParentTagNode();
		if (Parent.IsValid())
		{
			return Parent->GetCompleteTag();
		}
	}
	return FGameplayTag();
}

void UGameplayTagsManager::AddChildrenTags(FGameplayTagContainer& TagContainer, TSharedPtr<FGameplayTagNode> GameplayTagNode, bool RecurseAll, bool OnlyIncludeDictionaryTags) const
{
	if (GameplayTagNode.IsValid())
	{
		TArray< TSharedPtr<FGameplayTagNode> >& ChildrenNodes = GameplayTagNode->GetChildTagNodes();
		for (TSharedPtr<FGameplayTagNode> ChildNode : ChildrenNodes)
		{
			if (ChildNode.IsValid())
			{
				bool bShouldInclude = true;

#if WITH_EDITORONLY_DATA
				if (OnlyIncludeDictionaryTags && ChildNode->SourceName == NAME_None)
				{
					// Only have info to do this in editor builds
					bShouldInclude = false;
				}
#endif	
				if (bShouldInclude)
				{
					TagContainer.AddTag(ChildNode->GetCompleteTag());
				}

				if (RecurseAll)
				{
					AddChildrenTags(TagContainer, ChildNode, true, OnlyIncludeDictionaryTags);
				}
			}

		}
	}
}

void UGameplayTagsManager::SplitGameplayTagFName(const FGameplayTag& Tag, TArray<FName>& OutNames) const
{
	TSharedPtr<FGameplayTagNode> CurNode = FindTagNode(Tag);
	while (CurNode.IsValid())
	{
		OutNames.Insert(CurNode->GetSimpleTagName(), 0);
		CurNode = CurNode->GetParentTagNode();
	}
}

int32 UGameplayTagsManager::GameplayTagsMatchDepth(const FGameplayTag& GameplayTagOne, const FGameplayTag& GameplayTagTwo) const
{
	TSet<FName> Tags1;
	TSet<FName> Tags2;

	TSharedPtr<FGameplayTagNode> TagNode = FindTagNode(GameplayTagOne);
	if (TagNode.IsValid())
	{
		GetAllParentNodeNames(Tags1, TagNode);
	}

	TagNode = FindTagNode(GameplayTagTwo);
	if (TagNode.IsValid())
	{
		GetAllParentNodeNames(Tags2, TagNode);
	}

	return Tags1.Intersect(Tags2).Num();
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::GetAllParentNodeNames"), STAT_UGameplayTagsManager_GetAllParentNodeNames, STATGROUP_GameplayTags);

void UGameplayTagsManager::GetAllParentNodeNames(TSet<FName>& NamesList, TSharedPtr<FGameplayTagNode> GameplayTag) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_GetAllParentNodeNames);

	NamesList.Add(GameplayTag->GetCompleteTagName());
	TSharedPtr<FGameplayTagNode> Parent = GameplayTag->GetParentTagNode();
	if (Parent.IsValid())
	{
		GetAllParentNodeNames(NamesList, Parent);
	}
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::ValidateTagCreation"), STAT_UGameplayTagsManager_ValidateTagCreation, STATGROUP_GameplayTags);

bool UGameplayTagsManager::ValidateTagCreation(FName TagName) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_ValidateTagCreation);

	return FindTagNode(TagName).IsValid();
}

FGameplayTagTableRow::FGameplayTagTableRow(FGameplayTagTableRow const& Other)
{
	*this = Other;
}

FGameplayTagTableRow& FGameplayTagTableRow::operator=(FGameplayTagTableRow const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}

	Tag = Other.Tag;
	DevComment = Other.DevComment;

	return *this;
}

bool FGameplayTagTableRow::operator==(FGameplayTagTableRow const& Other) const
{
	return (Tag == Other.Tag);
}

bool FGameplayTagTableRow::operator!=(FGameplayTagTableRow const& Other) const
{
	return (Tag != Other.Tag);
}

bool FGameplayTagTableRow::operator<(FGameplayTagTableRow const& Other) const
{
	return (Tag < Other.Tag);
}

FGameplayTagNode::FGameplayTagNode(FName InTag, TSharedPtr<FGameplayTagNode> InParentNode)
	: Tag(InTag)
	, ParentNode(InParentNode)
	, NetIndex(INVALID_TAGNETINDEX)
{
	TArray<FGameplayTag> ParentCompleteTags;

	TSharedPtr<FGameplayTagNode> CurNode = InParentNode;

	// Stop iterating at root node
	while (CurNode.IsValid() && CurNode->GetSimpleTagName() != NAME_None)
	{
		ParentCompleteTags.Add(CurNode->GetCompleteTag());
		CurNode = CurNode->GetParentTagNode();
	}

	FString CompleteTagString = InTag.ToString();

	if (ParentCompleteTags.Num() > 0)
	{
		// If we have a parent, add parent., which will includes all earlier parents
		CompleteTagString = FString::Printf(TEXT("%s.%s"), *ParentCompleteTags[0].ToString(), *InTag.ToString());
	}
	
	// Manually construct the tag container as we want to bypass the safety checks
	CompleteTagWithParents.GameplayTags.Add(FGameplayTag(FName(*CompleteTagString)));
	CompleteTagWithParents.ParentTags = ParentCompleteTags;
}

void FGameplayTagNode::ResetNode()
{
	Tag = NAME_None;
	CompleteTagWithParents.Reset();
	NetIndex = INVALID_TAGNETINDEX;

	for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
	{
		ChildTags[ChildIdx]->ResetNode();
	}

	ChildTags.Empty();
	ParentNode.Reset();
}

#undef LOCTEXT_NAMESPACE
