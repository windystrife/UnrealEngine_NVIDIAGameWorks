// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "Textures/SlateIcon.h"
#include "Widgets/SWindow.h"
#include "Framework/Docking/WorkspaceItem.h"

class FJsonObject;
class FMenuBuilder;
class FMultiBox;
class FProxyTabmanager;
class SDockingArea;
class SDockingTabStack;
class FLayoutExtender;
struct FTabMatcher;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnActiveTabChanged,
	/** Previously active tab */
	TSharedPtr<SDockTab>,
	/** Newly active tab */
	TSharedPtr<SDockTab> );
	

struct FTabId
{
	FTabId( )
		: InstanceId(INDEX_NONE)
	{ }

	FTabId( const FName& InTabType, const int32 InInstanceId )
		: TabType(InTabType)
		, InstanceId(InInstanceId)
	{ }

	FTabId( const FName& InTabType )
		: TabType(InTabType)
		, InstanceId(INDEX_NONE)
	{ }

	/** Document tabs allow multiple instances of the same tab type. The placement rules for these tabs are left up for the specific use-cases. These tabs are not persisted. */
	bool IsTabPersistable() const
	{
		return InstanceId == INDEX_NONE;
	}

	bool operator==( const FTabId& Other ) const
	{
		return TabType == Other.TabType && (InstanceId == INDEX_NONE || Other.InstanceId == INDEX_NONE || InstanceId == Other.InstanceId) ;
	}

	FString ToString() const
	{
		return (InstanceId == INDEX_NONE)
			? TabType.ToString()
			: FString::Printf( TEXT("%s : %d"), *(TabType.ToString()), InstanceId );
	}

	FText ToText() const
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("TabType"), FText::FromName( TabType ) );
		Args.Add( TEXT("InstanceIdNumber"), FText::AsNumber( InstanceId ) );

		return (InstanceId == INDEX_NONE)
			? FText::FromName( TabType )
			: FText::Format( NSLOCTEXT("TabManager", "TabIdFormat", "{TabType} : {InstanceIdNumber}"), Args );
	}

	friend uint32 GetTypeHash(const FTabId& In)
	{
		return GetTypeHash(In.TabType) ^ In.InstanceId;
	}

	FName TabType;
	int32 InstanceId;
};


class FSpawnTabArgs
{
	public:
	FSpawnTabArgs( const TSharedPtr<SWindow>& InOwnerWindow, const FTabId& InTabBeingSpawenedId )
	: TabIdBeingSpawned(InTabBeingSpawenedId)
	, OwnerWindow(InOwnerWindow)
	{
	}

	const TSharedPtr<SWindow>& GetOwnerWindow() const
	{
		return OwnerWindow;
	}

	const FTabId& GetTabId() const 
	{
		return TabIdBeingSpawned;
	}

	private:
	FTabId TabIdBeingSpawned;
	TSharedPtr<SWindow> OwnerWindow;
};


/**
 * Invoked when a tab needs to be spawned.
 */
DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<SDockTab>, FOnSpawnTab, const FSpawnTabArgs& );

/**
 * Allows users to provide custom logic when searching for a tab to reuse.
 * The TabId that is being searched for is provided as a courtesy, but does not have to be respected.
 */
DECLARE_DELEGATE_RetVal_OneParam( TSharedPtr<SDockTab>, FOnFindTabToReuse, const FTabId& )

/** An enum to describe how TabSpawnerEntries will be handled by menus. */
namespace ETabSpawnerMenuType
{
	enum Type
	{
		Enabled,		// Display this spawner in menus
		Disabled,		// Display this spawner in menus, but make it disabled
		Hidden,			// Do not display this spawner in menus, it will be invoked manually
	};
}

struct FTabSpawnerEntry : public FWorkspaceItem
{
	FTabSpawnerEntry( const FName& InTabType, const FOnSpawnTab& InSpawnTabMethod )
		: FWorkspaceItem( FText(), FSlateIcon(), false )
		, TabType( InTabType )
		, OnSpawnTab( InSpawnTabMethod )
		, OnFindTabToReuse()
		, MenuType(ETabSpawnerMenuType::Enabled)
		, bAutoGenerateMenuEntry(true)
		, SpawnedTabPtr()
	{
	}

	FTabSpawnerEntry& SetIcon( const FSlateIcon& InIcon)
	{
		Icon = InIcon;
		return *this;
	}

	FTabSpawnerEntry& SetDisplayName( const FText& InLegibleName )
	{
		DisplayName = InLegibleName;
		return *this;
	}

	FTabSpawnerEntry& SetTooltipText( const FText& InTooltipText )
	{
		TooltipText = InTooltipText;
		return *this;
	}

	FTabSpawnerEntry& SetGroup( const TSharedRef<FWorkspaceItem>& InGroup )
	{
		InGroup->AddItem(SharedThis(this));
		return *this;
	}

	FTabSpawnerEntry& SetReuseTabMethod( const FOnFindTabToReuse& InReuseTabMethod )
	{
		OnFindTabToReuse = InReuseTabMethod;
		return *this;
	}

	FTabSpawnerEntry& SetMenuType( const TAttribute<ETabSpawnerMenuType::Type>& InMenuType )
	{
		MenuType = InMenuType;
		return *this;
	}

	FTabSpawnerEntry& SetAutoGenerateMenuEntry( bool bInAutoGenerateMenuEntry )
	{
		bAutoGenerateMenuEntry = bInAutoGenerateMenuEntry;
		return *this;
	}

	virtual TSharedPtr<FTabSpawnerEntry> AsSpawnerEntry() override
	{
		return SharedThis(this);
	}

private:
	FName TabType;
	FOnSpawnTab OnSpawnTab;
	/** When this method is not provided, we assume that the tab should only allow 0 or 1 instances */
	FOnFindTabToReuse OnFindTabToReuse;
	/** Whether this menu item should be enabled, disabled, or hidden */
	TAttribute<ETabSpawnerMenuType::Type> MenuType;
	/** Whether to automatically generate a menu entry for this tab spawner */
	bool bAutoGenerateMenuEntry;

	TWeakPtr<SDockTab> SpawnedTabPtr;

	FORCENOINLINE bool IsSoleTabInstanceSpawned() const
	{
		// Items that allow multiple instances need a custom way to find and reuse tabs.
		return SpawnedTabPtr.IsValid();
	}

	friend class FTabManager;
};


namespace ETabState
{
	enum Type
	{
		OpenedTab = 0x1 << 0,
		ClosedTab = 0x1 << 1
	};
}


class SLATE_API FTabManager : public TSharedFromThis<FTabManager>
{
	friend class FGlobalTabmanager;
	public:

		class FStack;
		class FSplitter;
		class FArea;
		class FLayout;

		DECLARE_DELEGATE_OneParam( FOnPersistLayout, const TSharedRef<FLayout>& );

		class SLATE_API FLayoutNode : public TSharedFromThis<FLayoutNode>
		{
			friend class FTabManager;

		public:

			virtual ~FLayoutNode() { }
			
			virtual TSharedPtr<FStack> AsStack();

			virtual TSharedPtr<FSplitter> AsSplitter();

			virtual TSharedPtr<FArea> AsArea();

			float GetSizeCoefficient() const { return SizeCoefficient; }

		protected:
			FLayoutNode()
			: SizeCoefficient(1.0f)
			{
			}

			float SizeCoefficient;
		};

		struct FTab
		{
			FTab( const FTabId& InTabId, ETabState::Type InTabState )
			: TabId(InTabId)
			, TabState(InTabState)
			{
			}

			bool operator==( const FTab& Other ) const
			{
				return this->TabId == Other.TabId && this->TabState == Other.TabState;
			}

			FTabId TabId;
			ETabState::Type TabState;
		};

		class SLATE_API FStack : public FLayoutNode
		{
				friend class FTabManager;
				friend class FLayout;
				friend class SDockingTabStack;

			public:				

				TSharedRef<FStack> AddTab( const FName& TabType, ETabState::Type TabState )
				{
					Tabs.Add( FTab( FTabId(TabType), TabState ) );
					return SharedThis(this);
				}

				TSharedRef<FStack> AddTab( const FTabId& TabId, ETabState::Type TabState )
				{
					Tabs.Add( FTab(TabId, TabState) );
					return SharedThis(this);
				}

				TSharedRef<FStack> SetSizeCoefficient( const float InSizeCoefficient )
				{
					SizeCoefficient = InSizeCoefficient;
					return SharedThis(this);
				}

				TSharedRef<FStack> SetHideTabWell( const bool InHideTabWell )
				{
					bHideTabWell = InHideTabWell;
					return SharedThis(this);
				}

				TSharedRef<FStack> SetForegroundTab( const FTabId& TabId )
				{
					ForegroundTabId = TabId;
					return SharedThis(this);
				}

				virtual TSharedPtr<FStack> AsStack() override
				{
					return SharedThis(this);
				}

				virtual ~FStack()
				{
				}

			protected:

				FStack()
				: Tabs()
				, bHideTabWell(false)
				, ForegroundTabId(NAME_None)
				{
				}

				TArray<FTab> Tabs;
				bool bHideTabWell;
				FTabId ForegroundTabId;
		};


		class SLATE_API FSplitter : public FLayoutNode
		{
				friend class FTabManager;
		
			public:

				TSharedRef<FSplitter> Split( TSharedRef<FLayoutNode> InNode )
				{
					ChildNodes.Add(InNode);
					return SharedThis(this);
				}

				TSharedRef<FSplitter> SetSizeCoefficient( const float InSizeCoefficient )
				{
					SizeCoefficient = InSizeCoefficient;
					return SharedThis(this);
				}

				TSharedRef<FSplitter> SetOrientation( const EOrientation InOrientation )
				{
					Orientation = InOrientation;
					return SharedThis(this);
				}

				virtual TSharedPtr<FSplitter> AsSplitter() override
				{
					return SharedThis(this);
				}

				EOrientation GetOrientation() const { return Orientation; }

				virtual ~FSplitter()
				{
				}

			protected:
				FSplitter()
				: Orientation(Orient_Horizontal)
				{
				}

				EOrientation Orientation;
				TArray< TSharedRef<FLayoutNode> > ChildNodes;
		};


		class SLATE_API FArea : public FSplitter
		{
				friend class FTabManager;
		
			public:			
				enum EWindowPlacement
				{
					Placement_NoWindow,
					Placement_Automatic,
					Placement_Specified
				};

				TSharedRef<FArea> Split( TSharedRef<FLayoutNode> InNode )
				{
					ChildNodes.Add(InNode);
					return SharedThis(this);
				}
				
				TSharedRef<FArea> SetOrientation( const EOrientation InOrientation )
				{
					Orientation = InOrientation;
					return SharedThis(this);
				}

				TSharedRef<FArea> SetWindow( FVector2D InPosition, bool IsMaximized )
				{
					WindowPlacement = Placement_Specified;
					UnscaledWindowPosition = InPosition;
					bIsMaximized = IsMaximized;
					return SharedThis(this);
				}

				virtual TSharedPtr<FArea> AsArea() override
				{
					return SharedThis(this);
				}

				virtual ~FArea()
				{
				}

			protected:
				FArea( const float InWidth, const float InHeight )
				: WindowPlacement(Placement_Automatic)
				, UnscaledWindowPosition(FVector2D(0,0))
				, UnscaledWindowSize(InWidth, InHeight)
				, bIsMaximized( false )
				{
				}

				EWindowPlacement WindowPlacement;
				FVector2D UnscaledWindowPosition;
				FVector2D UnscaledWindowSize;
				bool bIsMaximized;
		};


		class SLATE_API FLayout : public TSharedFromThis<FLayout>
		{
				friend class FTabManager;
				
			public:

				TSharedRef<FLayout> AddArea( const TSharedRef<FArea>& InArea )
				{
					Areas.Add( InArea );
					return SharedThis(this);
				}

				const TWeakPtr<FArea>& GetPrimaryArea() const
				{
					return PrimaryArea;
				}
				
			public:
				static TSharedPtr<FTabManager::FLayout> NewFromString( const FString& LayoutAsText );
				FName GetLayoutName() const;
				FString ToString() const;

				void ProcessExtensions(const FLayoutExtender& Extender);

			protected:
				static TSharedRef<class FJsonObject> PersistToString_Helper(const TSharedRef<FLayoutNode>& NodeToPersist);
				static TSharedRef<FLayoutNode> NewFromString_Helper( TSharedPtr<FJsonObject> JsonObject );

				FLayout(const FName& InLayoutName)
					: LayoutName(InLayoutName)
				{}

				TWeakPtr< FArea > PrimaryArea;
				TArray< TSharedRef<FArea> > Areas;
				/** The layout will be saved into a config file with this name. E.g. LevelEditorLayout or MaterialEditorLayout */
				FName LayoutName;
		};	


		friend class FPrivateApi;
		class FPrivateApi
		{
			public:
				FPrivateApi( FTabManager& InTabManager )
				: TabManager( InTabManager )
				{
				}

				TSharedPtr<SWindow> GetParentWindow() const;
				void OnDockAreaCreated( const TSharedRef<SDockingArea>& NewlyCreatedDockArea );
				/** Notify the tab manager that a tab has been relocated. If the tab now lives in a new window, the NewOwnerWindow should be a valid pointer. */
				void OnTabRelocated( const TSharedRef<SDockTab>& RelocatedTab, const TSharedPtr<SWindow>& NewOwnerWindow );
				void OnTabOpening( const TSharedRef<SDockTab>& TabBeingOpened );
				void OnTabClosing( const TSharedRef<SDockTab>& TabBeingClosed );
				void OnDockAreaClosing( const TSharedRef<SDockingArea>& DockAreaThatIsClosing );
				void OnTabManagerClosing();
				bool CanTabLeaveTabWell(const TSharedRef<const SDockTab>& TabToTest) const;
				const TArray< TWeakPtr<SDockingArea> >& GetLiveDockAreas() const;
				/**
				 * Notify the tab manager that the NewForegroundTab was brought to front and the BackgroundedTab was send to the background as a result.
				 */
				void OnTabForegrounded( const TSharedPtr<SDockTab>& NewForegroundTab, const TSharedPtr<SDockTab>& BackgroundedTab );

				void ShowWindows();
				void HideWindows();

			private:
				FTabManager& TabManager;
				
		};

		FTabManager::FPrivateApi& GetPrivateApi();

	public:
		class SLATE_API FSearchPreference
		{
		public:
			virtual TSharedPtr<SDockTab> Search(const FTabManager& Manager, FName PlaceholderId, const TSharedRef<SDockTab>& UnmanagedTab) const = 0;
			virtual ~FSearchPreference() {}
		};

		class SLATE_API FRequireClosedTab : public FSearchPreference
		{
		public:
			virtual TSharedPtr<SDockTab> Search(const FTabManager& Manager, FName PlaceholderId, const TSharedRef<SDockTab>& UnmanagedTab) const override;
		};

		class SLATE_API FLiveTabSearch : public FSearchPreference
		{
		public:
			FLiveTabSearch(FName InSearchForTabId = NAME_None);

			virtual TSharedPtr<SDockTab> Search(const FTabManager& Manager, FName PlaceholderId, const TSharedRef<SDockTab>& UnmanagedTab) const override;

		private:
			FName SearchForTabId;
		};

		class SLATE_API FLastMajorOrNomadTab : public FSearchPreference
		{
		public:
			FLastMajorOrNomadTab(FName InFallbackTabId);

			virtual TSharedPtr<SDockTab> Search(const FTabManager& Manager, FName PlaceholderId, const TSharedRef<SDockTab>& UnmanagedTab) const override;
		private:
			FName FallbackTabId;
		};

	public:
		static TSharedRef<FLayout> NewLayout( const FName LayoutName )
		{
			return MakeShareable( new FLayout(LayoutName) );
		}
		
		static TSharedRef<FArea> NewPrimaryArea()
		{
			TSharedRef<FArea>Area = MakeShareable( new FArea(0,0) );
			Area->WindowPlacement = FArea::Placement_NoWindow;
			return Area;
		}

		static TSharedRef<FArea> NewArea( const float Width, const float Height )
		{
			return MakeShareable( new FArea( Width, Height ) );
		}

		static TSharedRef<FArea> NewArea( const FVector2D& WindowSize )
		{
			return MakeShareable( new FArea( WindowSize.X, WindowSize.Y ) );
		}
		
		static TSharedRef<FStack> NewStack() 
		{
			return MakeShareable( new FStack() );
		}
		
		static TSharedRef<FSplitter> NewSplitter()
		{
			return MakeShareable( new FSplitter() );
		}

		static void RegisterDefaultTabWindowSize(const FTabId& TabName, const FVector2D DefaultSize)
		{
			DefaultTabWindowSizeMap.Add(TabName, DefaultSize);
		}

		static void UnregisterDefaultTabWindowSize(const FTabId& TabName)
		{
			DefaultTabWindowSizeMap.Remove(TabName);
		}

		void SetOnPersistLayout( const FOnPersistLayout& InHandler );

		/** Close all live areas and wipe all the persisted areas. */
		void CloseAllAreas();

		/** Gather the persistent layout */
		TSharedRef<FTabManager::FLayout> PersistLayout() const;

		/** Gather the persistent layout and execute the custom delegate for saving it to persistent storage (e.g. into config files) */
		void SavePersistentLayout();

		/**
		 * Register a new tab spawner with the tab manager.  The spawner will be called when anyone calls
		 * InvokeTab().
		 * @param TabId The TabId to register the spawner for.
		 * @param OnSpawnTab The callback that will be used to spawn the tab.
		 * @return The registration entry for the spawner.
		 */
		FTabSpawnerEntry& RegisterTabSpawner( const FName TabId, const FOnSpawnTab& OnSpawnTab );
		
		/**
		 * Unregisters the tab spawner matching the provided TabId.
		 * @param TabId The TabId to remove the spawner for.
		 * @return true if a spawner was found for this TabId, otherwise false.
		 */
		bool UnregisterTabSpawner( const FName TabId );

		/**
		 * Unregisters all tab spawners.
		 */
		void UnregisterAllTabSpawners();

		TSharedPtr<SWidget> RestoreFrom( const TSharedRef<FLayout>& Layout, const TSharedPtr<SWindow>& ParentWindow, const bool bEmbedTitleAreaContent = false );

		void PopulateLocalTabSpawnerMenu( FMenuBuilder& PopulateMe );

		void PopulateTabSpawnerMenu(FMenuBuilder& PopulateMe, TSharedRef<FWorkspaceItem> MenuStructure);

		void PopulateTabSpawnerMenu( FMenuBuilder& PopulateMe, TSharedRef<FWorkspaceItem> MenuStructure, bool bIncludeOrphanedMenus);

		void PopulateTabSpawnerMenu( FMenuBuilder &PopulateMe, const FName& TabType );

		virtual void DrawAttention( const TSharedRef<SDockTab>& TabToHighlight );

		struct ESearchPreference
		{
			enum Type
			{
				PreferLiveTab,
				RequireClosedTab
			};
		};

		/** Insert a new UnmanagedTab document tab next to an existing tab (closed or open) that has the PlaceholdId. */
		void InsertNewDocumentTab( FName PlaceholderId, const FSearchPreference& SearchPreference, const TSharedRef<SDockTab>& UnmanagedTab );
		
		/** Insert a new UnmanagedTab document tab next to an existing tab (closed or open) that has the PlaceholdId. */
		void InsertNewDocumentTab(FName PlaceholderId, ESearchPreference::Type SearchPreference, const TSharedRef<SDockTab>& UnmanagedTab);

		/**
		 * Much like InsertNewDocumentTab, but the UnmanagedTab is not seen by the user as newly-created.
		 * e.g. Opening an restores multiple previously opened documents; these are not seen as new tabs.
		 */
		void RestoreDocumentTab(FName PlaceholderId, ESearchPreference::Type SearchPreference, const TSharedRef<SDockTab>& UnmanagedTab);

		/**
		 * Opens tab if it is closed at the last known location.  If it already exists, it will draw attention to the tab.
		 * 
		 * @param TabId The tab identifier.
		 * @return The existing or newly spawned tab instance.
		 */
		virtual TSharedRef<SDockTab> InvokeTab( const FTabId& TabId );

		/**
		 * Finds the first instance of an existing tab with the given tab id.
		 *
		 * @param TabId The tab identifier.
		 * @return The existing tab instance if found, otherwise null.
		 */
		TSharedPtr<SDockTab> FindExistingLiveTab(const FTabId& TabId) const;

		virtual ~FTabManager()
		{
		}

		/**
		 * Set the multi-box to use for generating a native, global menu bar.
		 * @param NewMenuMutliBox The multi-box to generate the global menu bar from.
		 */
		void SetMenuMultiBox(const TSharedPtr< FMultiBox >& NewMenuMutliBox);

		/**
		 * Update the native, global menu bar if it is being used.
		 * @param bForce Used to force an update even if the parent window doesn't contain the widget with keyboard focus.
		 */
		void UpdateMainMenu(bool const bForce);

		/** Provide a tab that will be the main tab and cannot be closed. */
		void SetMainTab(const TSharedRef<const SDockTab>& InTab);

		/* Prevent or allow all tabs to be drag */
		void SetCanDoDragOperation(bool CanDoDragOperation) { bCanDoDragOperation = CanDoDragOperation; }
		
		/* Return true if we can do drag operation */
		bool GetCanDoDragOperation() { return bCanDoDragOperation; }

		/** @return if the provided tab can be closed. */
		bool IsTabCloseable(const TSharedRef<const SDockTab>& InTab) const;

		/** @return The local workspace menu root */
		const TSharedRef<FWorkspaceItem> GetLocalWorkspaceMenuRoot() const;

		/** Adds a category to the local workspace menu by name */
		TSharedRef<FWorkspaceItem> AddLocalWorkspaceMenuCategory( const FText& CategoryTitle );

		/** Adds an existing workspace item to the local workspace menu */
		void AddLocalWorkspaceMenuItem( const TSharedRef<FWorkspaceItem>& CategoryItem );

		/** Clears all categories in the local workspace menu */
		void ClearLocalWorkspaceMenuCategories();

		/** @return true if the tab has a factory registered for it that allows it to be spawned. */
		bool CanSpawnTab( FName TabId );

		/** Returns the owner tab (if it exists) */
		TSharedPtr<SDockTab> GetOwnerTab() { return OwnerTabPtr.Pin(); }

	protected:
		void InvokeTabForMenu( FName TabId );

	protected:
		
		void InsertDocumentTab( FName PlaceholderId, const FSearchPreference& SearchPreference, const TSharedRef<SDockTab>& UnmanagedTab, bool bPlaySpawnAnim );

		virtual void OpenUnmanagedTab(FName PlaceholderId, const FSearchPreference& SearchPreference, const TSharedRef<SDockTab>& UnmanagedTab);
			
		void PopulateTabSpawnerMenu_Helper( FMenuBuilder& PopulateMe, struct FPopulateTabSpawnerMenu_Args Args );

		void MakeSpawnerMenuEntry( FMenuBuilder &PopulateMe, const TSharedPtr<FTabSpawnerEntry> &SpawnerNode );

		TSharedRef<SDockTab> InvokeTab_Internal( const FTabId& TabId );

		/** Finds the last major or nomad tab in a particular window. */
		TSharedPtr<SDockTab> FindLastTabInWindow(TSharedPtr<SWindow> Window) const;

		TSharedPtr<SDockingTabStack> FindPotentiallyClosedTab( const FTabId& ClosedTabId );

		typedef TMap< FName, TSharedRef<FTabSpawnerEntry> > FTabSpawner;
		
		static TSharedRef<FTabManager> New( const TSharedPtr<SDockTab>& InOwnerTab, const TSharedRef<FTabSpawner>& InNomadTabSpawner )
		{
			return MakeShareable( new FTabManager(InOwnerTab, InNomadTabSpawner) );
		}

		FTabManager( const TSharedPtr<SDockTab>& InOwnerTab, const TSharedRef<FTabManager::FTabSpawner> & InNomadTabSpawner );

		TSharedRef<SDockingArea> RestoreArea( const TSharedRef<FArea>& AreaToRestore, const TSharedPtr<SWindow>& InParentWindow, const bool bEmbedTitleAreaContent = false );

		TSharedRef<class SDockingNode> RestoreArea_Helper( const TSharedRef<FLayoutNode>& LayoutNode, const TSharedPtr<SWindow>& ParentWindow, const bool bEmbedTitleAreaContent );

		void RestoreSplitterContent( const TSharedRef<FSplitter>& SplitterNode, const TSharedRef<class SDockingSplitter>& SplitterWidget, const TSharedPtr<SWindow>& ParentWindow );
		
		bool IsValidTabForSpawning( const FTab& SomeTab ) const;
		TSharedRef<SDockTab> SpawnTab( const FTabId& TabId, const TSharedPtr<SWindow>& ParentWindow );

		TSharedPtr<class SDockingTabStack> FindTabInLiveAreas( const FTabMatcher& TabMatcher ) const;
		static TSharedPtr<class SDockingTabStack> FindTabInLiveArea( const FTabMatcher& TabMatcher, const TSharedRef<SDockingArea>& InArea );

		template<typename MatchFunctorType> static bool HasAnyMatchingTabs( const TSharedRef<FTabManager::FLayoutNode>& SomeNode, const MatchFunctorType& Matcher );
		bool HasOpenTabs( const TSharedRef<FTabManager::FLayoutNode>& SomeNode ) const;
		bool HasValidTabs( const TSharedRef<FTabManager::FLayoutNode>& SomeNode ) const;

		/**
		 * Notify the tab manager that the NewForegroundTab was brought to front and the BackgroundedTab was send to the background as a result.
		 */
		virtual void OnTabForegrounded( const TSharedPtr<SDockTab>& NewForegroundTab, const TSharedPtr<SDockTab>& BackgroundedTab );
		virtual void OnTabRelocated( const TSharedRef<SDockTab>& RelocatedTab, const TSharedPtr<SWindow>& NewOwnerWindow );
		virtual void OnTabOpening( const TSharedRef<SDockTab>& TabBeingOpened );
		virtual void OnTabClosing( const TSharedRef<SDockTab>& TabBeingClosed );
		/** Invoked when a tab manager is closing down. */
		virtual void OnTabManagerClosing();
		/** Check these all tabs to see if it is OK to close them. Ignore the TabsToIgnore */
		virtual bool CanCloseManager( const TSet< TSharedRef<SDockTab> >& TabsToIgnore = TSet< TSharedRef<SDockTab> >() );

		static void GetAllStacks( const TSharedRef<SDockingArea>& InDockArea, TArray< TSharedRef<SDockingTabStack> >& OutTabStacks );

		/** @return the stack that is under NodeToSearchUnder and contains TabIdToFind; Invalid pointer if not found. */
		static TSharedPtr<FTabManager::FStack> FindTabUnderNode( const FTabMatcher& Matcher, const TSharedRef<FTabManager::FLayoutNode>& NodeToSearchUnder );
		int32 FindTabInCollapsedAreas( const FTabMatcher& Matcher );
		void RemoveTabFromCollapsedAreas( const FTabMatcher& Matcher );

		/** Called when tab(s) have been added or windows created */
		virtual void UpdateStats();
		
	private:
		/** Checks all dock areas and adds up the number of open tabs and unique parent windows in the manager */
		void GetRecordableStats( int32& OutTabCount, TArray<TSharedPtr<SWindow>>& OutUniqueParentWindows ) const;

	protected:
		FTabSpawner TabSpawner;
		TSharedRef<FTabSpawner> NomadTabSpawner;
		TSharedPtr<FTabSpawnerEntry> FindTabSpawnerFor(FName TabId);

		TArray< TWeakPtr<SDockingArea> > DockAreas;
		TArray< TSharedRef<FTabManager::FArea> > CollapsedDockAreas;

		/** The root for the local editor's tab spawner workspace menu */
		TSharedPtr<FWorkspaceItem> LocalWorkspaceMenuRoot;

		/** A Major tab that contains this TabManager's widgets. */
		TWeakPtr<SDockTab> OwnerTabPtr;

		/** The current menu multi-box for the tab, used to construct platform native main menus */
		TSharedPtr< FMultiBox > MenuMultiBox;

		/** Protected private API that must only be accessed by the docking framework internals */
		TSharedRef<FPrivateApi> PrivateApi;

		/** The name of the layout being used */
		FName ActiveLayoutName;

		/** Invoked when the tab manager is about to close */
		FOnPersistLayout OnPersistLayout_Handler;

		/**
		 * Instance ID for document tabs. Allows us to distinguish between different document tabs at runtime.
		 * This ID is never meant to be persisted, simply used to disambiguate between different documents, since most of thenm
		 * will have the same Tab Type (which is usually document).
		 */
		int32 LastDocumentUID;

		/** The fallback size for a window */
		const static FVector2D FallbackWindowSize;

		/** Default tab window sizes for newly-created tabs */
		static TMap<FTabId, FVector2D> DefaultTabWindowSizeMap;

		/** Returns the default window size for the TabId, or the fallback window size if it wasn't registered */
		static FVector2D GetDefaultTabWindowSize(const FTabId& TabId);

		/**
		 * Defensive: True when we are saving the visual state.
		 * Inevitably someone will ask us to save layout while saving layout.
		 * We will ignore that request them.
		 */
		bool bIsSavingVisualState;

		/** The main tab, this tab cannot be closed. */
		TWeakPtr<const SDockTab> MainNonCloseableTab;

		/** The last window we docked a nomad or major tab into */
		TWeakPtr<SWindow> LastMajorDockWindow;

		/* Prevent or allow Drag operation. */
		bool bCanDoDragOperation;
};



class FProxyTabmanager;


class SLATE_API FGlobalTabmanager : public FTabManager
{
public:	

	static const TSharedRef<FGlobalTabmanager>& Get();

	/** Subscribe to notifications about the active tab changing */
	FDelegateHandle OnActiveTabChanged_Subscribe( const FOnActiveTabChanged::FDelegate& InDelegate );

	/** Unsubscribe to notifications about the active tab changing */
	void OnActiveTabChanged_Unsubscribe( FDelegateHandle Handle );

	/** Subscribe to notifications about a foreground tab changing */
	FDelegateHandle OnTabForegrounded_Subscribe(const FOnActiveTabChanged::FDelegate& InDelegate);

	/** Unsubscribe to notifications about a foreground tab changing */
	void OnTabForegrounded_Unsubscribe(FDelegateHandle Handle);

	/** @return the currently active tab; NULL pointer if there is no active tab */
	TSharedPtr<SDockTab> GetActiveTab() const;

	/** Can the manager activate this Tab as the new active tab? */
	bool CanSetAsActiveTab(const TSharedPtr<SDockTab>& Tab);

	/** Activate the NewActiveTab. If NewActiveTab is NULL, the active tab is cleared. */
	void SetActiveTab( const TSharedPtr<SDockTab>& NewActiveTab );

	FTabSpawnerEntry& RegisterNomadTabSpawner( const FName TabId, const FOnSpawnTab& OnSpawnTab );
	
	void UnregisterNomadTabSpawner( const FName TabId );

	void SetApplicationTitle( const FText& AppTitle );

	const FText& GetApplicationTitle() const;

	static TSharedRef<FGlobalTabmanager> New()
	{
		return MakeShareable( new FGlobalTabmanager() );
	}

	virtual bool CanCloseManager( const TSet< TSharedRef<SDockTab> >& TabsToIgnore = TSet< TSharedRef<SDockTab> >()) override;

	/** Gets the major tab for the manager */
	TSharedPtr<SDockTab> GetMajorTabForTabManager(const TSharedRef<FTabManager>& ChildManager);

	/** Draw the user's attention to a child tab manager */
	void DrawAttentionToTabManager( const TSharedRef<FTabManager>& ChildManager );

	TSharedRef<FTabManager> NewTabManager( const TSharedRef<SDockTab>& InOwnerTab );
	
	/**
	 * Update the native, global menu bar if it is being used for a specific tab managed by the global tab manager.
	 * @param ForTab The tab to update the main menu.
	 * @param bForce Used to force an update even if the parent window doesn't contain the widget with keyboard focus.
	 */
	void UpdateMainMenu(const TSharedRef<SDockTab>& ForTab, bool const bForce);

	/** Persist and serialize the layout of every TabManager and the custom visual state of every Tab. */
	void SaveAllVisualState();

	/** Provide a window under which all other windows in this application should nest. */
	void SetRootWindow( const TSharedRef<SWindow> InRootWindow );

	/** The window under which all other windows in our app nest; might be null */
	TSharedPtr<SWindow> GetRootWindow() const;

	/** Adds a legacy tab type to the tab type redirection map so tabs loaded with this type will be automatically converted to the new type */
	void AddLegacyTabType(FName InLegacyTabType, FName InNewTabType);

	/** Returns true if the specified tab type is registered as a legacy tab */
	bool IsLegacyTabType(FName InTabType) const;

	/** If the specified TabType is deprecated, returns the new replacement tab type. Otherwise, returns InTabType */
	FName GetTabTypeForPotentiallyLegacyTab(FName InTabType) const;

	/** Returns the highest number of tabs that were open simultaneously during this session */
	int32 GetMaximumTabCount() const { return AllTabsMaxCount; }

	/** Returns the highest number of parent windows that were open simultaneously during this session */
	int32 GetMaximumWindowCount() const { return AllAreasWindowMaxCount; }

	void SetProxyTabManager(TSharedPtr<FProxyTabmanager> InProxyTabManager);

	DECLARE_DELEGATE(FOnOverrideDockableAreaRestore);
	/** Used to override dockable area restoration behavior */
	FOnOverrideDockableAreaRestore OnOverrideDockableAreaRestore_Handler;

protected:
	virtual void OnTabForegrounded( const TSharedPtr<SDockTab>& NewForegroundTab, const TSharedPtr<SDockTab>& BackgroundedTab ) override;
	virtual void OnTabRelocated( const TSharedRef<SDockTab>& RelocatedTab, const TSharedPtr<SWindow>& NewOwnerWindow ) override;
	virtual void OnTabClosing( const TSharedRef<SDockTab>& TabBeingClosed ) override;
	virtual void UpdateStats() override;

	virtual void OpenUnmanagedTab(FName PlaceholderId, const FSearchPreference& SearchPreference, const TSharedRef<SDockTab>& UnmanagedTab) override;

public:
	virtual void OnTabManagerClosing() override;


private:
	
	/** Pairs of Major Tab and the TabManager that manages tabs within it. */
	struct FSubTabManager
	{
		FSubTabManager( const TSharedRef<SDockTab>& InMajorTab, const TSharedRef<FTabManager>& InTabManager )
		: MajorTab( InMajorTab )
		, TabManager( InTabManager )
		{
		}

		TWeakPtr<SDockTab> MajorTab;
		TWeakPtr<FTabManager> TabManager;
	};

	struct FindByTab
	{
		FindByTab(const TSharedRef<SDockTab>& InTabToFind)
			: TabToFind(InTabToFind)
		{
		}

		bool operator()(const FGlobalTabmanager::FSubTabManager& TabManagerPair) const
		{
			return TabManagerPair.TabManager.IsValid() && TabManagerPair.MajorTab.IsValid() && TabManagerPair.MajorTab.Pin() == TabToFind;
		}

		const TSharedRef<SDockTab>& TabToFind;
	};

	struct FindByManager
	{
		FindByManager(const TSharedRef<FTabManager>& InManagerToFind)
			: ManagerToFind(InManagerToFind)
		{
		}

		bool operator()(const FGlobalTabmanager::FSubTabManager& TabManagerPair) const
		{
			return TabManagerPair.TabManager.IsValid() && TabManagerPair.MajorTab.IsValid() && TabManagerPair.TabManager.Pin() == ManagerToFind;
		}

		const TSharedRef<FTabManager>& ManagerToFind;
	};

	FGlobalTabmanager()
	: FTabManager( TSharedPtr<SDockTab>(), MakeShareable( new FTabSpawner() ) )
	, AllTabsMaxCount(0)
	, AllAreasWindowMaxCount(0)
	{
	}

	TArray< FSubTabManager > SubTabManagers;

	/** The currently active tab; NULL if there is no active tab. */
	TWeakPtr<SDockTab> ActiveTabPtr;

	FOnActiveTabChanged OnActiveTabChanged;

	FOnActiveTabChanged TabForegrounded;

	FText AppTitle;

	/** A window under which all of the windows in this application will nest. */
	TWeakPtr<SWindow> RootWindowPtr;

	/** A map that correlates deprecated tab types to new tab types */
	TMap<FName, FName> LegacyTabTypeRedirectionMap;

	/** Keeps track of the running-maximum number of tabs in all dock areas and sub-managers during this session */
	int32 AllTabsMaxCount;

	/** Keeps track of the running-maximum number of unique parent windows in all dock areas and sub-managers during this session */
	int32 AllAreasWindowMaxCount;

	/**  */
	TSharedPtr<FProxyTabmanager> ProxyTabManager;
};

//#HACK VREDITOR - Had to introduce the proxy tab manager to steal asset tabs.

DECLARE_MULTICAST_DELEGATE_TwoParams(FIsTabSupportedEvent, FTabId /* TabId */, bool& /* bOutIsSupported */ );
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTabEvent, TSharedPtr<SDockTab>);


class SLATE_API FProxyTabmanager : public FTabManager
{
public:

	FProxyTabmanager(TSharedRef<SWindow> InParentWindow)
		: FTabManager(TSharedPtr<SDockTab>(), MakeShareable(new FTabSpawner()))
		, ParentWindow(InParentWindow)
	{
		bCanDoDragOperation = false;
	}

	virtual void OpenUnmanagedTab(FName PlaceholderId, const FSearchPreference& SearchPreference, const TSharedRef<SDockTab>& UnmanagedTab) override;
	virtual void DrawAttention(const TSharedRef<SDockTab>& TabToHighlight) override;
	
	bool IsTabSupported( const FTabId TabId ) const;
	void SetParentWindow(TSharedRef<SWindow> InParentWindow);

public:
	FIsTabSupportedEvent OnIsTabSupported;
	FOnTabEvent OnTabOpened;
	FOnTabEvent OnAttentionDrawnToTab;

private:
	TWeakPtr<SWindow> ParentWindow;
};
