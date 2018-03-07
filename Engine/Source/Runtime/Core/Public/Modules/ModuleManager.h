// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "Delegates/Delegate.h"
#include "Misc/Optional.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleInterface.h"
#include "Modules/Boilerplate/ModuleBoilerplate.h"
#include "Misc/EnumClassFlags.h"

#if WITH_HOT_RELOAD
	/** If true, we are reloading a class for HotReload */
	extern CORE_API bool GIsHotReload;
#endif


/**
 * Enumerates reasons for failed module loads.
 */
enum class EModuleLoadResult
{
	/** Module loaded successfully. */
	Success,

	/** The specified module file could not be found. */
	FileNotFound,

	/** The specified module file is incompatible with the module system. */
	FileIncompatible,

	/** The operating system failed to load the module file. */
	CouldNotBeLoadedByOS,

	/** Module initialization failed. */
	FailedToInitialize
};


/**
 * Enumerates reasons for modules to change.
 *
 * Values of this type will be passed into OnModuleChanged() delegates.
 */
enum class EModuleChangeReason
{
	/** A module has been loaded and is ready to be used. */
	ModuleLoaded,

	/* A module has been unloaded and should no longer be used. */
	ModuleUnloaded,

	/** The paths controlling which plug-ins are loaded have been changed and the given module has been found, but not yet loaded. */
	PluginDirectoryChanged
};


enum class ECheckModuleCompatibilityFlags
{
	None = 0x00,

	// Display the loading of an up-to-date module
	DisplayUpToDateModules = 0x01
};

ENUM_CLASS_FLAGS(ECheckModuleCompatibilityFlags)


/**
 * Structure for reporting module statuses.
 */
struct FModuleStatus
{
	/** Default constructor. */
	FModuleStatus()
		: bIsLoaded(false)
		, bIsGameModule(false)
	{ }

	/** Short name for this module. */
	FString Name;

	/** Full path to this module file on disk. */
	FString FilePath;

	/** Whether the module is currently loaded or not. */
	bool bIsLoaded;

	/** Whether this module contains game play code. */
	bool bIsGameModule;
};

/**
 * Implements the module manager.
 *
 * The module manager is used to load and unload modules, as well as to keep track of all of the
 * modules that are currently loaded. You can access this singleton using FModuleManager::Get().
 */
class CORE_API FModuleManager
	: private FSelfRegisteringExec
{
public:

	/**
	 * Destructor.
	 */
	~FModuleManager();

	/**
	 * Gets the singleton instance of the module manager.
	 *
	 * @return The module manager instance.
	 */
	static FModuleManager& Get( );

public:

	/**
	 * Abandons a loaded module, leaving it loaded in memory but no longer tracking it in the module manager.
	 *
	 * @param InModuleName The name of the module to abandon.  Should not include path, extension or platform/configuration info.  This is just the "module name" part of the module file name.
	 * @see IsModuleLoaded, LoadModule, LoadModuleWithFailureReason, UnloadModule
	 */
	void AbandonModule( const FName InModuleName );

	/**
	 * Adds a module to our list of modules, unless it's already known.
	 *
	 * @param InModuleName The base name of the module file.  Should not include path, extension or platform/configuration info.  This is just the "name" part of the module file name.  Names should be globally unique.
	 */
	void AddModule( const FName InModuleName );

	/**
	 * Gets the specified module.
	 *
	 * @param InModuleName Name of the module to return.
	 * @return 	The module, or nullptr if the module is not loaded.
	 * @see GetModuleChecked, GetModulePtr
	 */
	IModuleInterface* GetModule( const FName InModuleName );

	/**
	 * Checks whether the specified module is currently loaded.
	 *
	 * This is an O(1) operation.
	 *
	 * @param InModuleName The base name of the module file.  Should not include path, extension or platform/configuration info.  This is just the "module name" part of the module file name.  Names should be globally unique.
	 * @return true if module is currently loaded, false otherwise.
	 * @see AbandonModule, LoadModule, LoadModuleWithFailureReason, UnloadModule
	 */
	bool IsModuleLoaded( const FName InModuleName ) const;

	/**
	 * Loads the specified module.
	 *
	 * @param InModuleName The base name of the module file.  Should not include path, extension or platform/configuration info.  This is just the "module name" part of the module file name.  Names should be globally unique.
	 * @param bWasReloaded Indicates that the module has been reloaded (default = false).
	 * @return The loaded module, or nullptr if the load operation failed.
	 * @see AbandonModule, IsModuleLoaded, LoadModuleChecked, LoadModulePtr, LoadModuleWithFailureReason, UnloadModule
	 */
	IModuleInterface* LoadModule( const FName InModuleName, const bool bWasReloaded = false );

	/**
	 * Loads the specified module, checking to ensure it exists.
	 *
	 * @param InModuleName The base name of the module file.  Should not include path, extension or platform/configuration info.  This is just the "module name" part of the module file name.  Names should be globally unique.
	 * @param bWasReloaded Indicates that the module has been reloaded (default = false).
	 * @return The loaded module, or nullptr if the load operation failed.
	 * @see AbandonModule, IsModuleLoaded, LoadModuleChecked, LoadModulePtr, LoadModuleWithFailureReason, UnloadModule
	 */
	IModuleInterface& LoadModuleChecked( const FName InModuleName, const bool bWasReloaded = false );

	/**
	 * Loads a module in memory then calls PostLoad.
	 *
	 * @param InModuleName The name of the module to load.
	 * @param Ar The archive to receive error messages, if any.
	 * @return true on success, false otherwise.
	 * @see UnloadOrAbandonModuleWithCallback
	 */
	bool LoadModuleWithCallback( const FName InModuleName, FOutputDevice &Ar );

	/**
	 * Loads the specified module and returns a result.
	 *
	 * @param InModuleName The base name of the module file.  Should not include path, extension or platform/configuration info.  This is just the "module name" part of the module file name.  Names should be globally unique.
	 * @param OutFailureReason Will contain the result.
	 * @param bWasReloaded Indicates that the module has been reloaded (default = false).
	 * @return The loaded module (null if the load operation failed).
	 * @see AbandonModule, IsModuleLoaded, LoadModule, LoadModuleChecked, LoadModulePtr, UnloadModule
	 */
	IModuleInterface* LoadModuleWithFailureReason( const FName InModuleName, EModuleLoadResult& OutFailureReason, const bool bWasReloaded = false );

	/**
	 * Queries information about a specific module name.
	 *
	 * @param InModuleName Module to query status for.
	 * @param OutModuleStatus Status of the specified module.
	 * @return true if the module was found and the OutModuleStatus is valid, false otherwise.
	 * @see QueryModules
	 */
	bool QueryModule( const FName InModuleName, FModuleStatus& OutModuleStatus ) const;

	/**
	 * Queries information about all of the currently known modules.
	 *
	 * @param OutModuleStatuses Status of all modules.
	 * @see QueryModule
	 */
	void QueryModules( TArray<FModuleStatus>& OutModuleStatuses ) const;

	/**
	 * Unloads a specific module
	 * NOTE: You can manually unload a module before the normal shutdown occurs with this, but be careful as you may be unloading another
	 * module's dependency too early!
	 *
	 * @param InModuleName The name of the module to unload.  Should not include path, extension or platform/configuration info.  This is just the "module name" part of the module file name.
	 * @param bIsShutdown Is this unload module call occurring at shutdown (default = false).
	 * @return true if module was unloaded successfully, false otherwise.
	 * @see AbandonModule, IsModuleLoaded, LoadModule, LoadModuleWithFailureReason
	 */
	bool UnloadModule( const FName InModuleName, bool bIsShutdown = false );

	/**
	 * Calls PreUnload then either unloads or abandons a module in memory, depending on whether the module supports unloading.
	 *
	 * @param InModuleName The name of the module to unload.
	 * @param Ar The archive to receive error messages, if any.
	 * @see LoadModuleWithCallback
	 */
	void UnloadOrAbandonModuleWithCallback( const FName InModuleName, FOutputDevice &Ar);

	/**
	 * Calls PreUnload then abandons a module in memory.
	 *
	 * @param InModuleName The name of the module to unload.
	 * @see LoadModuleWithCallback
	 */
	void AbandonModuleWithCallback( const FName InModuleName );

	/** Delegate that's used by the module manager to find all the valid modules in a directory matching a pattern */
	typedef TMap<FString, FString> FModuleNamesMap;
	DECLARE_DELEGATE_ThreeParams( FQueryModulesDelegate, const FString&, bool, FModuleNamesMap& );
	FQueryModulesDelegate QueryModulesDelegate;

public:

	/**
	  * Gets a module by name, checking to ensure it exists.
	  *
	  * This method checks whether the module actually exists. If the module does not exist, an assertion will be triggered.
	  *
	  * @param ModuleName The module to get.
	  * @return The interface to the module.
	  * @see GetModulePtr, LoadModulePtr, LoadModuleChecked
	  */
	template<typename TModuleInterface>
	static TModuleInterface& GetModuleChecked( const FName ModuleName )
	{
		FModuleManager& ModuleManager = FModuleManager::Get();

		checkf(ModuleManager.IsModuleLoaded(ModuleName), TEXT("Tried to get module interface for unloaded module: '%s'"), *(ModuleName.ToString()));
		return (TModuleInterface&)(*ModuleManager.GetModule(ModuleName));
	}

	/**
	  * Gets a module by name.
	  *
	  * @param ModuleName The module to get.
	  * @return The interface to the module, or nullptr if the module was not found.
	  * @see GetModuleChecked, LoadModulePtr, LoadModuleChecked
	  */
	template<typename TModuleInterface>
	static TModuleInterface* GetModulePtr( const FName ModuleName )
	{
		FModuleManager& ModuleManager = FModuleManager::Get();

		if (!ModuleManager.IsModuleLoaded(ModuleName))
		{
			return nullptr;
		}

		return static_cast<TModuleInterface*>(ModuleManager.GetModule(ModuleName));
	}

	/**
	  * Loads a module by name, checking to ensure it exists.
	  *
	  * This method checks whether the module actually exists. If the module does not exist, an assertion will be triggered.
	  * If the module was already loaded previously, the existing instance will be returned.
	  *
	  * @param ModuleName The module to find and load
	  * @return	Returns the module interface, casted to the specified typename
	  * @see GetModulePtr, LoadModulePtr, LoadModuleChecked
	  */
	template<typename TModuleInterface>
	static TModuleInterface& LoadModuleChecked( const FName InModuleName)
	{
		IModuleInterface& ModuleInterface = FModuleManager::Get().LoadModuleChecked(InModuleName);
		return (TModuleInterface&)(ModuleInterface);
	}

	/**
	  * Loads a module by name.
	  *
	  * @param ModuleName The module to find and load.
	  * @return The interface to the module, or nullptr if the module was not found.
	  * @see GetModulePtr, GetModuleChecked, LoadModuleChecked
	  */
	template<typename TModuleInterface>
	static TModuleInterface* LoadModulePtr( const FName InModuleName)
	{
		return static_cast<TModuleInterface*>(FModuleManager::Get().LoadModule(InModuleName));
	}

public:

	/**
	 * Finds module files on the disk for loadable modules matching the specified wildcard.
	 *
	 * @param WildcardWithoutExtension Filename part (no path, no extension, no build config info) to search for.
	 * @param OutModules List of modules found.
	 */
	void FindModules( const TCHAR* WildcardWithoutExtension, TArray<FName>& OutModules ) const;

	/**
	 * Determines if a module with the given name exists, regardless of whether it is currently loaded.
	 *
	 * @param ModuleName Name of the module to look for.
	 * @return Whether the module exists.
	 */
	bool ModuleExists(const TCHAR* ModuleName) const;

	/**
	 * Gets the number of loaded modules.
	 *
	 * @return The number of modules.
	 */
	int32 GetModuleCount( ) const;

	/**
	 * Unloads modules during the shutdown process. Modules are unloaded in reverse order to when their StartupModule() FINISHES.
	 * The practical implication of this is that if module A depends on another module B, and A loads B during A's StartupModule, 
	 * that B will actually get Unloaded after A during shutdown. This allows A's ShutdownModule() call to still reference module B.
	 * You can manually unload a module yourself which will change this ordering, but be careful as you may be unloading another 
	 * module's dependency!
	 *
	 * This method is Usually called at various points while exiting an application.
	 */
	void UnloadModulesAtShutdown( );


	/** Delegate that's used by the module manager to initialize a registered module that we statically linked with (monolithic only) */
	DECLARE_DELEGATE_RetVal( IModuleInterface*, FInitializeStaticallyLinkedModule )

	/**
	 * Registers an initializer for a module that is statically linked.
	 *
	 * @param InModuleName The name of this module.
	 * @param InInitializerDelegate The delegate that will be called to initialize an instance of this module.
	 */
	void RegisterStaticallyLinkedModule( const FName InModuleName, const FInitializeStaticallyLinkedModule& InInitializerDelegate )
	{
		StaticallyLinkedModuleInitializers.Add( InModuleName, InInitializerDelegate );
	}

	/**
	 * Called by the engine at startup to let the Module Manager know that it's now
	 * safe to process new UObjects discovered by loading C++ modules.
	 */
	void StartProcessingNewlyLoadedObjects();

	/** Adds an engine binaries directory. */
	void AddBinariesDirectory(const TCHAR *InDirectory, bool bIsGameDirectory);

	/**
	 *	Set the game binaries directory
	 *
	 *	@param InDirectory The game binaries directory.
	 */
	void SetGameBinariesDirectory(const TCHAR* InDirectory);

	/**
	*	Gets the game binaries directory
	*/
	FString GetGameBinariesDirectory() const;

	/**
	 * Checks to see if the specified module exists and is compatible with the current engine version. 
	 *
	 * @param InModuleName The base name of the module file.
	 * @return true if module exists and is up to date, false otherwise.
	 */
	bool IsModuleUpToDate( const FName InModuleName ) const;

	/**
	 * Determines whether the specified module contains UObjects.  The module must already be loaded into
	 * memory before calling this function.
	 *
	 * @param ModuleName Name of the loaded module to check.
	 * @return True if the module was found to contain UObjects, or false if it did not (or wasn't loaded.)
	 */
	bool DoesLoadedModuleHaveUObjects( const FName ModuleName ) const;

	/**
	 * Gets the build configuration for compiling modules, as required by UBT.
	 *
	 * @return	Configuration name for UBT.
	 */
	static const TCHAR *GetUBTConfiguration( );

	/** Gets the filename for a module. The return value is a full path of a module known to the module manager. */
	FString GetModuleFilename(FName ModuleName) const;

	/** Sets the filename for a module. The module is not reloaded immediately, but the new name will be used for subsequent unload/load events. */
	void SetModuleFilename(FName ModuleName, const FString& Filename);

	/** Gets the clean filename for a module, without having added it to the module manager. */
	static FString GetCleanModuleFilename(FName ModuleName, bool bIsGameModule);

public:

	/**
	 * Gets an event delegate that is executed when the set of known modules changed, i.e. upon module load or unload.
	 *
	 * The first parameter is the name of the module that changed.
	 * The second parameter is the reason for the change.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT_TwoParams(FModuleManager, FModulesChangedEvent, FName, EModuleChangeReason);
	FModulesChangedEvent& OnModulesChanged( )
	{
		return ModulesChangedEvent;
	}

	/**
	 * Gets a multicast delegate that is executed when any UObjects need processing after a module was loaded.
	 *
	 * @return The delegate.
	 */
	FSimpleMulticastDelegate& OnProcessLoadedObjectsCallback()
	{
		return ProcessLoadedObjectsCallback;
	}

	/**
	 * Gets a delegate that is executed when a module containing UObjects has been loaded.
	 *
	 * The first parameter is the name of the loaded module.
	 *
	 * @return The event delegate.
	 */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsPackageLoadedCallback, FName);
	FIsPackageLoadedCallback& IsPackageLoadedCallback()
	{
		return IsPackageLoaded;
	}

public:

	// FSelfRegisteringExec interface.

	virtual bool Exec( UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

protected:

	/**
	 * Hidden constructor.
	 *
	 * Use the static Get function to return the singleton instance.
	 */
	FModuleManager( )
		: bCanProcessNewlyLoadedObjects(false)
	{ }

private:

	/**
	 * Prevent copy constructor from being triggered.
	 */
	FModuleManager(const FModuleManager&)
	{ }

protected:

	/**
	 * Information about a single module (may or may not be loaded.)
	 */
	class FModuleInfo
	{
	public:

		/** The original file name of the module, without any suffixes added */
		FString OriginalFilename;

		/** File name of this module (.dll file name) */
		FString Filename;

		/** Handle to this module (DLL handle), if it's currently loaded */
		void* Handle;

		/** The module object for this module.  We actually *own* this module, so it's lifetime is controlled by the scope of this shared pointer. */
		TUniquePtr<IModuleInterface> Module;

		/** True if this module was unloaded at shutdown time, and we never want it to be loaded again */
		bool bWasUnloadedAtShutdown;

		/** Arbitrary number that encodes the load order of this module, so we can shut them down in reverse order. */
		int32 LoadOrder;

		/** static that tracks the current load number. Incremented whenever we add a new module*/
		static int32 CurrentLoadOrder;

	public:

		/** Constructor */
		FModuleInfo()
			: Handle(nullptr)
			, bWasUnloadedAtShutdown(false)
			, LoadOrder(CurrentLoadOrder++)
		{ }

		~FModuleInfo()
		{
		}
	};

	typedef TSharedPtr<FModuleInfo, ESPMode::ThreadSafe> ModuleInfoPtr;
	typedef TSharedRef<FModuleInfo, ESPMode::ThreadSafe> ModuleInfoRef;

	/** Type definition for maps of module names to module infos. */
	typedef TMap<FName, ModuleInfoRef> FModuleMap;

public:
	/**
	 * Generates a unique file name for the specified module name by adding a random suffix and checking for file collisions.
	 */
	void MakeUniqueModuleFilename( const FName InModuleName, FString& UniqueSuffix, FString& UniqueModuleFileName ) const;

	void AddModuleToModulesList(const FName InModuleName, FModuleManager::ModuleInfoRef& ModuleInfo);

	/** Clears module path cache */
	void ResetModulePathsCache();

private:
	static void WarnIfItWasntSafeToLoadHere(const FName InModuleName);

	/** Thread safe module finding routine. */
	ModuleInfoPtr FindModule(FName InModuleName);
	ModuleInfoRef FindModuleChecked(FName InModuleName);

	FORCEINLINE TSharedPtr<const FModuleInfo, ESPMode::ThreadSafe> FindModule(FName InModuleName) const
	{
		return const_cast<FModuleManager*>(this)->FindModule(InModuleName);
	}

	FORCEINLINE TSharedRef<const FModuleInfo, ESPMode::ThreadSafe> FindModuleChecked(FName InModuleName) const
	{
		return const_cast<FModuleManager*>(this)->FindModuleChecked(InModuleName);
	}

	/** Compares file versions between the current executing engine version and the specified dll */
	static bool CheckModuleCompatibility(const TCHAR *Filename, ECheckModuleCompatibilityFlags Flags = ECheckModuleCompatibilityFlags::None );

	/** Gets the prefix and suffix for a module file */
	static void GetModuleFilenameFormat(bool bGameModule, FString& OutPrefix, FString& OutSuffix);

	/** Finds modules matching a given name wildcard. */
	void FindModulePaths(const TCHAR *NamePattern, TMap<FName, FString> &OutModulePaths, bool bCanUseCache = true) const;

	/** Finds modules matching a given name wildcard within a given directory. */
	void FindModulePathsInDirectory(const FString &DirectoryName, bool bIsGameDirectory, const TCHAR *NamePattern, TMap<FName, FString> &OutModulePaths) const;

private:
	/** Gets module with given name from Modules or creates a new one. Doesn't modify Modules. */
	ModuleInfoRef GetOrCreateModule(FName InModuleName);

	/** Map of all modules.  Maps the case-insensitive module name to information about that module, loaded or not. */
	FModuleMap Modules;

	/** Map of module names to a delegate that can initialize each respective statically linked module */
	typedef TMap< FName, FInitializeStaticallyLinkedModule > FStaticallyLinkedModuleInitializerMap;
	FStaticallyLinkedModuleInitializerMap StaticallyLinkedModuleInitializers;

	/** True if module manager should automatically register new UObjects discovered while loading C++ modules */
	bool bCanProcessNewlyLoadedObjects;

	/** Cache of known module paths. Used for performance. Can increase editor startup times by up to 30% */
	mutable TOptional<TMap<FName, FString>> ModulePathsCache;

	/** Multicast delegate that will broadcast a notification when modules are loaded, unloaded, or
		our set of known modules changes */
	FModulesChangedEvent ModulesChangedEvent;
	
	/** Multicast delegate called to process any new loaded objects. */
	FSimpleMulticastDelegate ProcessLoadedObjectsCallback;

	/** When module manager is linked against an application that supports UObjects, this delegate will be primed
		at startup to provide information about whether a UObject package is loaded into memory. */
	FIsPackageLoadedCallback IsPackageLoaded;

	/** Array of engine binaries directories. */
	TArray<FString> EngineBinariesDirectories;

	/** Array of game binaries directories. */
	TArray<FString> GameBinariesDirectories;

	/** Critical section object controlling R/W access to Modules. */
	mutable FCriticalSection ModulesCriticalSection;
};

/**
 * Utility class for registering modules that are statically linked.
 */
template< class ModuleClass >
class FStaticallyLinkedModuleRegistrant
{
public:

	/**
	 * Explicit constructor that registers a statically linked module
	 */
	FStaticallyLinkedModuleRegistrant( const ANSICHAR* InModuleName )
	{
		// Create a delegate to our InitializeModule method
		FModuleManager::FInitializeStaticallyLinkedModule InitializerDelegate = FModuleManager::FInitializeStaticallyLinkedModule::CreateRaw(
				this, &FStaticallyLinkedModuleRegistrant<ModuleClass>::InitializeModule );

		// Register this module
		FModuleManager::Get().RegisterStaticallyLinkedModule(
			FName( InModuleName ),	// Module name
			InitializerDelegate );	// Initializer delegate
	}
	
	/**
	 * Creates and initializes this statically linked module.
	 *
	 * The module manager calls this function through the delegate that was created
	 * in the @see FStaticallyLinkedModuleRegistrant constructor.
	 *
	 * @return A pointer to a new instance of the module.
	 */
	IModuleInterface* InitializeModule( )
	{
		return new ModuleClass();
	}
};


/**
 * Function pointer type for InitializeModule().
 *
 * All modules must have an InitializeModule() function. Usually this is declared automatically using
 * the IMPLEMENT_MODULE macro below. The function must be declared using as 'extern "C"' so that the
 * name remains undecorated. The object returned will be "owned" by the caller, and will be deleted
 * by the caller before the module is unloaded.
 */
typedef IModuleInterface* ( *FInitializeModuleFunctionPtr )( void );


/**
 * A default minimal implementation of a module that does nothing at startup and shutdown
 */
class FDefaultModuleImpl
	: public IModuleInterface
{ };


/**
 * Default minimal module class for gameplay modules.  Does nothing at startup and shutdown.
 */
class FDefaultGameModuleImpl
	: public FDefaultModuleImpl
{
	/**
	 * Returns true if this module hosts gameplay code
	 *
	 * @return True for "gameplay modules", or false for engine code modules, plug-ins, etc.
	 */
	virtual bool IsGameModule() const override
	{
		return true;
	}
};

/**
 * Module implementation boilerplate for regular modules.
 *
 * This macro is used to expose a module's main class to the rest of the engine.
 * You must use this macro in one of your modules C++ modules, in order for the 'InitializeModule'
 * function to be declared in such a way that the engine can find it. Also, this macro will handle
 * the case where a module is statically linked with the engine instead of dynamically loaded.
 *
 * This macro is intended for modules that do NOT contain gameplay code.
 * If your module does contain game classes, use IMPLEMENT_GAME_MODULE instead.
 *
 * Usage:   IMPLEMENT_MODULE(<My Module Class>, <Module name string>)
 *
 * @see IMPLEMENT_GAME_MODULE
 */
#if IS_MONOLITHIC

	// If we're linking monolithically we assume all modules are linked in with the main binary.
	#define IMPLEMENT_MODULE( ModuleImplClass, ModuleName ) \
		/** Global registrant object for this module when linked statically */ \
		static FStaticallyLinkedModuleRegistrant< ModuleImplClass > ModuleRegistrant##ModuleName( #ModuleName ); \
		/** Implement an empty function so that if this module is built as a statically linked lib, */ \
		/** static initialization for this lib can be forced by referencing this symbol */ \
		void EmptyLinkFunctionForStaticInitialization##ModuleName(){} \
		PER_MODULE_BOILERPLATE_ANYLINK(ModuleImplClass, ModuleName)

#else

	#define IMPLEMENT_MODULE( ModuleImplClass, ModuleName ) \
		\
		/**/ \
		/* InitializeModule function, called by module manager after this module's DLL has been loaded */ \
		/**/ \
		/* @return	Returns an instance of this module */ \
		/**/ \
		extern "C" DLLEXPORT IModuleInterface* InitializeModule() \
		{ \
			return new ModuleImplClass(); \
		} \
		PER_MODULE_BOILERPLATE \
		PER_MODULE_BOILERPLATE_ANYLINK(ModuleImplClass, ModuleName)

#endif //IS_MONOLITHIC


/**
 * Module implementation boilerplate for game play code modules.
 *
 * This macro works like IMPLEMENT_MODULE but is specifically used for modules that contain game play code.
 * If your module does not contain game classes, use IMPLEMENT_MODULE instead.
 *
 * Usage:   IMPLEMENT_GAME_MODULE(<My Game Module Class>, <Game Module name string>)
 *
 * @see IMPLEMENT_MODULE
 */
#define IMPLEMENT_GAME_MODULE( ModuleImplClass, ModuleName ) \
	IMPLEMENT_MODULE( ModuleImplClass, ModuleName )


/**
 * Macro for declaring the engine directory to check for foreign or nested projects.
 */
#if PLATFORM_DESKTOP
	#ifdef UE_ENGINE_DIRECTORY
		#define IMPLEMENT_FOREIGN_ENGINE_DIR() const TCHAR *GForeignEngineDir = TEXT( PREPROCESSOR_TO_STRING(UE_ENGINE_DIRECTORY) );
	#else
		#define IMPLEMENT_FOREIGN_ENGINE_DIR() const TCHAR *GForeignEngineDir = nullptr;
	#endif
#else
	#define IMPLEMENT_FOREIGN_ENGINE_DIR() 
#endif

/**
 * Macro for declaring the GIsDebugGame variable for monolithic development builds. NB: This define, and the UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME defines like it, should NEVER be 
 * directly used or defined for engine code, because it prevents sharing the same build products with the development build (important for Launcher build sizes). In modular builds, 
 * DebugGame modules will be loaded by specifying the -debug parameter on the command-line.
 */
#if IS_MONOLITHIC && UE_BUILD_DEVELOPMENT
	#define IMPLEMENT_DEBUGGAME() extern const bool GIsDebugGame = (UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME != 0);
#else
	#define IMPLEMENT_DEBUGGAME()
#endif 

#if IS_PROGRAM

	#if IS_MONOLITHIC
		#define IMPLEMENT_APPLICATION( ModuleName, GameName ) \
			/* For monolithic builds, we must statically define the game's name string (See Core.h) */ \
			TCHAR GInternalProjectName[64] = TEXT( GameName ); \
			IMPLEMENT_DEBUGGAME() \
			IMPLEMENT_FOREIGN_ENGINE_DIR() \
			IMPLEMENT_GAME_MODULE(FDefaultGameModuleImpl, ModuleName) \
			PER_MODULE_BOILERPLATE \
			FEngineLoop GEngineLoop;

	#else		

		#define IMPLEMENT_APPLICATION( ModuleName, GameName ) \
			/* For non-monolithic programs, we must set the game's name string before main starts (See Core.h) */ \
			struct FAutoSet##ModuleName \
			{ \
				FAutoSet##ModuleName() \
				{ \
					FCString::Strncpy(GInternalProjectName, TEXT( GameName ), ARRAY_COUNT(GInternalProjectName)); \
				} \
			} AutoSet##ModuleName; \
			PER_MODULE_BOILERPLATE \
			PER_MODULE_BOILERPLATE_ANYLINK(FDefaultGameModuleImpl, ModuleName) \
			FEngineLoop GEngineLoop;
	#endif

#else

/** IMPLEMENT_PRIMARY_GAME_MODULE must be used for at least one game module in your game.  It sets the "name"
	your game when compiling in monolithic mode. This is passed in by UBT from the .uproject name, and manually specifying a 
	name is no longer necessary. */
#if IS_MONOLITHIC
	#if PLATFORM_DESKTOP

		#define IMPLEMENT_PRIMARY_GAME_MODULE( ModuleImplClass, ModuleName, DEPRECATED_GameName ) \
			/* For monolithic builds, we must statically define the game's name string (See Core.h) */ \
			TCHAR GInternalProjectName[64] = TEXT( PREPROCESSOR_TO_STRING(UE_PROJECT_NAME) ); \
			/* Implement the GIsGameAgnosticExe variable (See Core.h). */ \
			bool GIsGameAgnosticExe = false; \
			IMPLEMENT_DEBUGGAME() \
			IMPLEMENT_FOREIGN_ENGINE_DIR() \
			IMPLEMENT_GAME_MODULE( ModuleImplClass, ModuleName ) \
			PER_MODULE_BOILERPLATE \
			void UELinkerFixupCheat() \
			{ \
				extern void UELinkerFixups(); \
				UELinkerFixups(); \
			}

	#else	//PLATFORM_DESKTOP

		#define IMPLEMENT_PRIMARY_GAME_MODULE( ModuleImplClass, ModuleName, DEPRECATED_GameName ) \
			/* For monolithic builds, we must statically define the game's name string (See Core.h) */ \
			TCHAR GInternalProjectName[64] = TEXT( PREPROCESSOR_TO_STRING(UE_PROJECT_NAME) ); \
			IMPLEMENT_DEBUGGAME() \
			PER_MODULE_BOILERPLATE \
			IMPLEMENT_FOREIGN_ENGINE_DIR() \
			IMPLEMENT_GAME_MODULE( ModuleImplClass, ModuleName ) \
			/* Implement the GIsGameAgnosticExe variable (See Core.h). */ \
			bool GIsGameAgnosticExe = false;

	#endif	//PLATFORM_DESKTOP

#else	//IS_MONOLITHIC

	#define IMPLEMENT_PRIMARY_GAME_MODULE( ModuleImplClass, ModuleName, GameName ) \
		/* Nothing special to do for modular builds.  The game name will be set via the command-line */ \
		IMPLEMENT_GAME_MODULE( ModuleImplClass, ModuleName )
#endif	//IS_MONOLITHIC

#endif
