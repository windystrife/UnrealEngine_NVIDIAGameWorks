// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBaseUtility.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "PrimaryAssetId.h"

class FConfigCacheIni;
class FEditPropertyChain;
class ITargetPlatform;
class ITransactionObjectAnnotation;
struct FFrame;
struct FObjectInstancingGraph;
struct FPropertyChangedChainEvent;

DECLARE_LOG_CATEGORY_EXTERN(LogObj, Log, All);


namespace ECastCheckedType
{
	enum Type
	{
		NullAllowed,
		NullChecked
	};
};

//
// The base class of all objects.
//
class COREUOBJECT_API UObject : public UObjectBaseUtility
{
	// Declarations.
	DECLARE_CLASS(UObject,UObject,CLASS_Abstract|CLASS_NoExport|CLASS_Intrinsic,CASTCLASS_None,TEXT("/Script/CoreUObject"),NO_API)
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UObject)
	static UObject* __VTableCtorCaller(FVTableHelper& Helper)
	{
		return new (EC_InternalUseOnlyConstructor, (UObject*)GetTransientPackage(), NAME_None, RF_NeedLoad | RF_ClassDefaultObject | RF_TagGarbageTemp) UObject(Helper);
	}

	typedef UObject WithinClass;
	static const TCHAR* StaticConfigName() {return TEXT("Engine");}

	// Constructors and destructors.
	UObject();
	UObject(const FObjectInitializer& ObjectInitializer);
	UObject( EStaticConstructor, EObjectFlags InFlags );
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	UObject(FVTableHelper& Helper);

	static void StaticRegisterNativesUObject() 
	{
	}

	UObject* CreateDefaultSubobject(FName SubobjectFName, UClass* ReturnType, UClass* ClassToCreateByDefault, bool bIsRequired, bool bAbstract, bool bIsTransient);

	/**
	* Create a component or subobject only to be used with the editor.
	* @param	TReturnType					class of return type, all overrides must be of this type
	* @param	SubobjectName				name of the new component
	* @param	bTransient					true if the component is being assigned to a transient property. This does not make the component itself transient, but does stop it from inheriting parent defaults
	*/
	template<class TReturnType>
	TReturnType* CreateEditorOnlyDefaultSubobject(FName SubobjectName, bool bTransient = false)
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(CreateEditorOnlyDefaultSubobjectImpl(SubobjectName, ReturnType, bTransient));
	}

	/**
	* Create a component or subobject
	* @param	TReturnType					class of return type, all overrides must be of this type
	* @param	SubobjectName				name of the new component
	* @param	bTransient					true if the component is being assigned to a transient property. This does not make the component itself transient, but does stop it from inheriting parent defaults
	*/
	template<class TReturnType>
	TReturnType* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false)
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(CreateDefaultSubobject(SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/ true, /*bIsAbstract =*/ false, bTransient));
	}
	
	/**
	* Create a component or subobject
	* @param TReturnType class of return type, all overrides must be of this type
	* @param TClassToConstructByDefault class to construct by default
	* @param SubobjectName name of the new component
	* @param bTransient		true if the component is being assigned to a transient property. This does not make the component itself transient, but does stop it from inheriting parent defaults
	*/
	template<class TReturnType, class TClassToConstructByDefault>
	TReturnType* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false)
	{
		return static_cast<TReturnType*>(CreateDefaultSubobject(SubobjectName, TReturnType::StaticClass(), TClassToConstructByDefault::StaticClass(), /*bIsRequired =*/ true, /*bIsAbstract =*/ false, bTransient));
	}
	
	/**
	* Create optional component or subobject. Optional subobjects may not get created
	* when a derived class specified DoNotCreateDefaultSubobject with the subobject's name.
	* @param	TReturnType					class of return type, all overrides must be of this type
	* @param	SubobjectName				name of the new component
	* @param	bTransient					true if the component is being assigned to a transient property. This does not make the component itself transient, but does stop it from inheriting parent defaults
	*/
	template<class TReturnType>
	TReturnType* CreateOptionalDefaultSubobject(FName SubobjectName, bool bTransient = false)
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(CreateDefaultSubobject(SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/ false, /*bIsAbstract =*/ false, bTransient));
	}
	
	/**
	* Create optional component or subobject. Optional subobjects may not get created
	* when a derived class specified DoNotCreateDefaultSubobject with the subobject's name.
	* @param	TReturnType					class of return type, all overrides must be of this type
	* @param	SubobjectName				name of the new component
	* @param	bTransient					true if the component is being assigned to a transient property. This does not make the component itself transient, but does stop it from inheriting parent defaults
	*/
	template<class TReturnType>
	TReturnType* CreateAbstractDefaultSubobject(FName SubobjectName, bool bTransient = false)
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(CreateDefaultSubobject(SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/ true, /*bIsAbstract =*/ true, bTransient));
	}

	/**
	* Gets all default subobjects associated with this object instance.
	* @param	OutDefaultSubobjects	Array containing all default subobjects of this object.
	*/
	void GetDefaultSubobjects(TArray<UObject*>& OutDefaultSubobjects);

	/**
	* Finds a subobject associated with this object instance by its name
	* @param	OutDefaultSubobjects	Array containing all default subobjects of this object.
	*/
	UObject* GetDefaultSubobjectByName(FName ToFind);

	//==========================================
	// UObject interface.
	//==========================================	
protected:
    /** 
     * This function actually does the work for the GetDetailInfo and is virtual.  
     * It should only be called from GetDetailedInfo as GetDetailedInfo is safe to call on NULL object pointers
     **/
	virtual FString GetDetailedInfoInternal() const { return TEXT("No_Detailed_Info_Specified"); }
public:
	/**
	 * Called after the C++ constructor and after the properties have been initialized, including those loaded from config.
	 * mainly this is to emulate some behavior of when the constructor was called after the properties were initialized.
	 */
	virtual void PostInitProperties();

	/**
	* Called after the C++ constructor has run on the CDO for a class. This is an obscure routine used to deal with the recursion 
	* in the construction of the default materials
	*/
	virtual void PostCDOContruct()
	{
	}

	/**
	 * Called from within SavePackage on the passed in base/ root. The return value of this function will be passed to
	 * PostSaveRoot. This is used to allow objects used as base to perform required actions before saving and cleanup
	 * afterwards.
	 * @param Filename: Name of the file being saved to (includes path)
	 *
	 * @return	Whether PostSaveRoot needs to perform internal cleanup
	 */
	virtual bool PreSaveRoot(const TCHAR* Filename)
	{
		return false;
	}
	/**
	 * Called from within SavePackage on the passed in base/ root. This function is being called after the package
	 * has been saved and can perform cleanup.
	 *
	 * @param	bCleanupIsRequired	Whether PreSaveRoot dirtied state that needs to be cleaned up
	 */
	virtual void PostSaveRoot( bool bCleanupIsRequired ) {}

	/**
	 * Presave function. Gets called once before an object gets serialized for saving. This function is necessary
	 * for save time computation as Serialize gets called three times per object from within SavePackage.
	 *
	 * @warning: Objects created from within PreSave will NOT have PreSave called on them!!!
	 */
	virtual void PreSave(const class ITargetPlatform* TargetPlatform);

	/**
	 * Note that the object will be modified.  If we are currently recording into the 
	 * transaction buffer (undo/redo), save a copy of this object into the buffer and 
	 * marks the package as needing to be saved.
	 *
	 * @param	bAlwaysMarkDirty	if true, marks the package dirty even if we aren't
	 *								currently recording an active undo/redo transaction
	 * @return true if the object was saved to the transaction buffer
	 */
	virtual bool Modify( bool bAlwaysMarkDirty=true );

	/** 
	 * Utility to allow overrides of Modify to avoid doing work if the base class is not going modify anyways.
	 */
	bool CanModify() const;

#if WITH_EDITOR
	/** 
	 * Called when the object was loaded from another class via active class redirects.
	 */
	virtual void LoadedFromAnotherClass(const FName& OldClassName) {}
#endif

	/** 
	 * Do any object-specific cleanup required immediately after loading an object, 
	 * and immediately after any undo/redo.
	 */
	virtual void PostLoad();

	/**
	 * Instances components for objects being loaded from disk, if necessary.  Ensures that component references
	 * between nested components are fixed up correctly.
	 *
	 * @param	OuterInstanceGraph	when calling this method on subobjects, specifies the instancing graph which contains all instanced
	 *								subobjects and components for a subobject root.
	 */
	virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph);
	
	/**
	 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	 * asynchronous cleanup process.
	 */
	virtual void BeginDestroy();

	/**
	 * Called to check if the object is ready for FinishDestroy.  This is called after BeginDestroy to check the completion of the
	 * potentially asynchronous object cleanup.
	 * @return True if the object's asynchronous cleanup has completed and it is ready for FinishDestroy to be called.
	 */
	virtual bool IsReadyForFinishDestroy() { return true; }

#if WITH_EDITOR
	/**
	 * Called in response to the linker changing, this can only happen in the editor
	 */
	virtual void PostLinkerChange() {}
#endif

	/**
	 * Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
	 *
	 * note: because properties are destroyed here, Super::FinishDestroy() should always be called at the end of your child class's
	 * FinishDestroy() method, rather than at the beginning.
	 */
	virtual void FinishDestroy();

	/** UObject serializer. */
	virtual void Serialize( FArchive& Ar );

	virtual void ShutdownAfterError() {}

	/** 
	 * This is called when property is modified by InterpPropertyTracks
	 *
	 * @param PropertyThatChanged	Property that changed
	 */
	virtual void PostInterpChange(UProperty* PropertyThatChanged) {}

#if WITH_EDITOR
	/** 
	 * This is called when property is about to be modified by InterpPropertyTracks
	 *
	 * @param PropertyThatWillChange	Property that will be changed
	 */
	virtual void PreEditChange(UProperty* PropertyAboutToChange);

	/**
	 * This alternate version of PreEditChange is called when properties inside structs are modified.  The property that was actually modified
	 * is located at the tail of the list.  The head of the list of the UStructProperty member variable that contains the property that was modified.
	 *
	 * @param PropertyAboutToChange the property that is about to be modified
	 */
	virtual void PreEditChange( class FEditPropertyChain& PropertyAboutToChange );

	/**
	 * Called by the editor to query whether a property of this object is allowed to be modified.
	 * The property editor uses this to disable controls for properties that should not be changed.
	 * When overriding this function you should always call the parent implementation first.
	 *
	 * @param	InProperty	The property to query
	 *
	 * @return	true if the property can be modified in the editor, otherwise false
	 */
	virtual bool CanEditChange( const UProperty* InProperty ) const;

	/** 
	 * Intentionally non-virtual as it calls the FPropertyChangedEvent version
	 */
	void PostEditChange();

	/**
	 * Called when a property on this object has been modified externally
	 *
	 * @param PropertyThatChanged the property that was modified
	 */
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * This alternate version of PostEditChange is called when properties inside structs are modified.  The property that was actually modified
	 * is located at the tail of the list.  The head of the list of the UStructProperty member variable that contains the property that was modified.
	 */
	virtual void PostEditChangeChainProperty( struct FPropertyChangedChainEvent& PropertyChangedEvent );

	/** Gathers external data required for applying an undo transaction */
	virtual TSharedPtr<ITransactionObjectAnnotation> GetTransactionAnnotation() const { return NULL; }

	/** Called before applying a transaction to the object.  Default implementation simply calls PreEditChange. */
	virtual void PreEditUndo();

	/** Called after applying a transaction to the object.  Default implementation simply calls PostEditChange. */
	virtual void PostEditUndo();

	/** Called after applying a transaction to the object in cases where transaction annotation was provided. Default implementation simply calls PostEditChange. */
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation);

	/**
	 * Test the selection state of a UObject
	 *
	 * @return		true if the object is selected, false otherwise.
	 * @todo UE4 this doesn't belong here, but it doesn't belong anywhere else any better
	 */
private:
	virtual bool IsSelectedInEditor() const;
public:
#endif // WITH_EDITOR

	// @todo document
	virtual void PostRename(UObject* OldOuter, const FName OldName) {}
	
	/**
	 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure UStaticMesh's UModel gets copied as well.
	 * Note: NOT called on components on actor duplication (alt-drag or copy-paste).  Use PostEditImport as well to cover that case.
	 */
	virtual void PostDuplicate(bool bDuplicateForPIE) {}

	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) 
	{
		PostDuplicate(DuplicateMode == EDuplicateMode::PIE);
	}

	/**
	 * Called during saving to determine the load flags to save with the object.
	 * If false, this object will be discarded on clients
	 *
	 * @return	true if this object should be loaded on clients
	 */
	virtual bool NeedsLoadForClient() const;

	/**
	 * Called during saving to determine the load flags to save with the object.
	 * If false, this object will be discarded on servers
	 *
	 * @return	true if this object should be loaded on servers
	 */
	virtual bool NeedsLoadForServer() const;

	/**
	 * Called during saving to determine the load flags to save with the object.
	 * If false, this object will still get loaded if NeedsLoadForServer/Client are true
	 * 
	 * @return	true if this object should always be loaded for editor game
	 */
	virtual bool NeedsLoadForEditorGame() const
	{
		return false;
	}

	/**
	* Called during saving to determine if the object is forced to be editor only or not
	*
	* @return	true if this object should never be loaded outside the editor
	*/
	virtual bool IsEditorOnly() const
	{
		return false;
	}

	/**
	* Called during async load to determine if PostLoad can be called on the loading thread.
	*
	* @return	true if this object's PostLoad is thread safe
	*/
	virtual bool IsPostLoadThreadSafe() const
	{
		return false;
	}

	/**
	* Called during cooking. Must return all objects that will be Preload()ed when this is serialized at load time. Only used by the EDL.
	*
	* @param	OutDeps				all objects that will be preloaded when this is serialized at load time
	*/
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps);

	/**
	* Called during cooking. Returns a list of objects. The packages containing those objects will be prestreamed, when the package containing this is loaded. Only used by the EDL.
	*
	* @param	OutPrestream				all objects that will be prestreamed when this packages is streamed
	*/
	virtual void GetPrestreamPackages(TArray<UObject*>& OutPrestream)
	{
	}

	/**
	*	Update the list of classes that we should exclude from dedicated server builds
	*/
	static void UpdateClassesExcludedFromDedicatedServer(const TArray<FString>& InClassNames, const TArray<FString>& InModulesNames);

	/**
	*	Update the list of classes that we should exclude from dedicated client builds
	*/
	static void UpdateClassesExcludedFromDedicatedClient(const TArray<FString>& InClassNames, const TArray<FString>& InModulesNames);

	/** 
	 *	Determines if you can create an object from the supplied template in the current context (editor, client only, dedicated server, game/listen) 
	 *	This calls NeedsLoadForClient & NeedsLoadForServer
	 */
	static bool CanCreateInCurrentContext(UObject* Template);

	/**
	* Exports the property values for the specified object as text to the output device. Required for Copy&Paste
	* Most objects don't need this as unreal script can handle most cases.
	*
	* @param	Out				the output device to send the exported text to
	* @param	Indent			number of spaces to prepend to each line of output
	*
	* see also: ImportCustomProperties()
	*/
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent)	{}

	/**
	* Exports the property values for the specified object as text to the output device. Required for Copy&Paste
	* Most objects don't need this as unreal script can handle most cases.
	*
	* @param	SourceText		the input data (zero terminated), must not be 0
	* @param	Warn			for error reporting, must not be 0
	*
	* see also: ExportCustomProperties()
	*/
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) {}

	/**
	 * Called after importing property values for this object (paste, duplicate or .t3d import)
	 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	 * are unsupported by the script serialization
	 */
	virtual void PostEditImport() {}

	/**
	 * Called from ReloadConfig after the object has reloaded its configuration data.
	 */
	virtual void PostReloadConfig( class UProperty* PropertyThatWasLoaded ) {}

	/** Rename this object to a unique name.*/
	virtual bool Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None );


	/** @return a one line description of an object for viewing in the thumbnail view of the generic browser */
	virtual FString GetDesc() { return TEXT( "" ); }

#if WITH_ENGINE
	virtual class UWorld* GetWorld() const;
	class UWorld* GetWorldChecked(bool& bSupported) const;
	bool ImplementsGetWorld() const;
#endif

	/**
	 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
	 * to have natively serialized property values included in things like diffcommandlet output.
	 *
	 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
	 *								the property and the map's value should be the textual representation of the property's value.  The property value should
	 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
	 *								as the delimiter between elements, etc.)
	 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
	 *
	 * @return	true if property values were added to the map.
	 */
	virtual bool GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, uint32 ExportFlags=0 ) const
	{
		return false;
	}

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor. The
	 * default behavior is to return 0 which indicates that the resource shouldn't
	 * display its size.
	 *
	 * @param	Mode	Indicates which resource size should be returned
	 * @return	Size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode)
	{
		return GetResourceSizeBytes(Mode);
	}

	/**
	 * Get the size of the object/resource for display to artists/LDs in the Editor.
	 * This is the extended version which separates up the used memory into different memory regions (the actual definition of which may be platform specific).
	 *
	 * @param	CumulativeResourceSize	Struct used to count up the cumulative size of the resource as to be displayed to artists/LDs in the Editor.
	 */
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
	{
	}

	/**
	 * Get the size of the object/resource for display to artists/LDs in the Editor.
	 * This is the simple version which just returns the total number of bytes used by this object.
	 *
	 * @param	Mode					Indicates which resource size should be returned.
	 * @return The cumulative size of the resource as to be displayed to artists/LDs in the Editor.
	 */
	SIZE_T GetResourceSizeBytes(EResourceSizeMode::Type Mode)
	{
		FResourceSizeEx ResSize = FResourceSizeEx(Mode);
		GetResourceSizeEx(ResSize);
		return ResSize.GetTotalMemoryBytes();
	}

	/** 
	 * Returns the name of the exporter factory used to export this object
	 * Used when multiple factories have the same extension
	 */
	virtual FName GetExporterName( void )
	{
		return( FName( TEXT( "" ) ) );
	}

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param InThis Object to collect references from.
	 * @param Collector	FReferenceCollector objects to be used to collect references.
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/**
	 * Helper function to call AddReferencedObjects for this object's class.
	 *
	 * @param Collector	FReferenceCollector objects to be used to collect references.
	 */
	void CallAddReferencedObjects(FReferenceCollector& Collector);

	/**
	 * Save information for StaticAllocateObject in the case of overwriting an existing object.
	 * StaticAllocateObject will call delete on the result after calling Restore()
	 *
	 * @return An FRestoreForUObjectOverwrite that can restore the object or NULL if this is not necessary.
	 */
	virtual FRestoreForUObjectOverwrite* GetRestoreForUObjectOverwrite()
	{
		return NULL;
	}

	/**
	 * Returns whether native properties are identical to the one of the passed in component.
	 *
	 * @param	Other	Other component to compare against
	 *
	 * @return true if native properties are identical, false otherwise
	 */
	virtual bool AreNativePropertiesIdenticalTo( UObject* Other ) const
	{
		return true;
	}

	/**
	 * Gathers a list of asset registry searchable tags which are name/value pairs with some type information
	 * This only needs to be implemented for asset objects
	 *
	 * @param	OutTags		A list of key-value pairs associated with this object and their types
	 */
	struct FAssetRegistryTag
	{
		/** Enum specifying the type of this tag */
		enum ETagType
		{
			/** This tag should not be shown in the UI */
			TT_Hidden,
			/** This tag should be shown, and sorted alphabetically in the UI */
			TT_Alphabetical,
			/** This tag should be shown, and is a number */
			TT_Numerical,
			/** This tag should be shown, and is an "x" delimited list of dimensions */ 
			TT_Dimensional,
			/** This tag should be shown, and is a timestamp formatted via FDateTime::ToString */
			TT_Chronological,
		};

		/** Flags controlling how this tag should be shown in the UI */
		enum ETagDisplay
		{
			/** No special display */
			TD_None = 0,
			/** For TT_Chronological, include the date */
			TD_Date = 1<<0,
			/** For TT_Chronological, include the time */
			TD_Time = 1<<1,
			/** For TT_Chronological, specifies that the timestamp should be displayed using the invariant timezone (typically for timestamps that are already in local time) */
			TD_InvariantTz = 1<<2,
			/** For TT_Numerical, specifies that the number is a value in bytes that should be displayed using FText::AsMemory */
			TD_Memory = 1<<3,
		};
		
		FName Name;
		FString Value;
		ETagType Type;
		uint32 DisplayFlags;

		FAssetRegistryTag(FName InName, const FString& InValue, ETagType InType, uint32 InDisplayFlags = TD_None)
			: Name(InName), Value(InValue), Type(InType), DisplayFlags(InDisplayFlags) {}

		/** Gathers a list of asset registry searchable tags from given objects properties */
		COREUOBJECT_API static void GetAssetRegistryTagsFromSearchableProperties(const UObject* Object, TArray<FAssetRegistryTag>& OutTags);

		/** Returns true if this FName is a special UStruct that should be exported even if not tagged, with the struct name as the tag name */
		COREUOBJECT_API static bool IsUniqueAssetRegistryTagStruct(FName StructName, ETagType& TagType);
	};
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const;

	/** Get the common tag name used for all asset source file import paths */
	static const FName& SourceFileTagName();

#if WITH_EDITOR
	/**
	 * Additional data pertaining to asset registry tags used by the editor
	 */
	struct FAssetRegistryTagMetadata
	{
		FText DisplayName;
		FText TooltipText;
		FText Suffix;
		FString ImportantValue;

		/** Set override display name */
		FAssetRegistryTagMetadata& SetDisplayName(const FText& InDisplayName)
		{
			DisplayName = InDisplayName;
			return *this;
		}

		/** Set tooltip text pertaining to the asset registry tag in the column view header */
		FAssetRegistryTagMetadata& SetTooltip(const FText& InTooltipText)
		{
			TooltipText = InTooltipText;
			return *this;
		}

		/** Set suffix appended to the tag value */
		FAssetRegistryTagMetadata& SetSuffix(const FText& InSuffix)
		{
			Suffix = InSuffix;
			return *this;
		}

		/** Set value deemed to be 'important' for this registry tag */
		FAssetRegistryTagMetadata& SetImportantValue(const FString& InImportantValue)
		{
			ImportantValue = InImportantValue;
			return *this;
		}
	};

	/** Gathers a collection of asset registry tag metadata */
	virtual void GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const;
#endif

	/** Returns true if this object is considered an asset. */
	virtual bool IsAsset() const;

	/**
	 * Returns an Type:Name pair representing the PrimaryAssetId for this object.
	 * Assets that need to be globally referenced at runtime should return a valid Identifier.
	 * If this is valid, the object can be referenced by identifier using the AssetManager 
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const;

	/** Returns true if this object is considered a localized resource. */
	virtual bool IsLocalizedResource() const;

	/** Returns true if this object is safe to add to the root set. */
	virtual bool IsSafeForRootSet() const;

	/** 
	 * Tags objects that are part of the same asset with the specified object flag, used for GC checking
	 *
	 * @param	ObjectFlags	Object Flags to enable on the related objects
	 */
	virtual void TagSubobjects(EObjectFlags NewFlags);

	/** Returns properties that are replicated for the lifetime of the actor channel */
	virtual void GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const;

	/** IsNameStableForNetworking means an object can be referred to its path name (relative to outer) over the network */
	virtual bool IsNameStableForNetworking() const;

	/** IsFullNameStableForNetworking means an object can be referred to its full path name over the network */
	virtual bool IsFullNameStableForNetworking() const;

	/** IsSupportedForNetworking means an object can be referenced over the network */
	virtual bool IsSupportedForNetworking() const;

	/** Returns a list of sub-objects that have stable names for networking */
	virtual void GetSubobjectsWithStableNamesForNetworking(TArray<UObject*> &ObjList) {}

	/** Called right before receiving a bunch */
	virtual void PreNetReceive();

	/** Called right after receiving a bunch */
	virtual void PostNetReceive();

	/** Called right after calling all OnRep notifies (called even when there are no notifies) */
	virtual void PostRepNotifies() {}

	/** Called right before being marked for destruction due to network replication */
	virtual void PreDestroyFromReplication();

	/**
 	 *******************************************************
	 * Non virtual functions, not intended to be overridden
	 *******************************************************
	 */

	/**
	 * Test the selection state of a UObject
	 *
	 * @return		true if the object is selected, false otherwise.
	 * @todo UE4 this doesn't belong here, but it doesn't belong anywhere else any better
	 */
	bool IsSelected() const;

#if WITH_EDITOR
	/**
	 * Serializes all objects which have this object as their archetype into GMemoryArchive, then recursively calls this function
	 * on each of those objects until the full list has been processed.
	 * Called when a property value is about to be modified in an archetype object. 
	 *
	 * @param	AffectedObjects		the array of objects which have this object in their ObjectArchetype chain and will be affected by the change.
	 *								Objects which have this object as their direct ObjectArchetype are removed from the list once they're processed.
	 */
	void PropagatePreEditChange( TArray<UObject*>& AffectedObjects, FEditPropertyChain& PropertyAboutToChange );

	/**
	 * De-serializes all objects which have this object as their archetype from the GMemoryArchive, then recursively calls this function
	 * on each of those objects until the full list has been processed.
	 *
	 * @param	AffectedObjects		the array of objects which have this object in their ObjectArchetype chain and will be affected by the change.
	 *								Objects which have this object as their direct ObjectArchetype are removed from the list once they're processed.
	 */
	void PropagatePostEditChange( TArray<UObject*>& AffectedObjects, FPropertyChangedChainEvent& PropertyChangedEvent );
#endif // WITH_EDITOR

	/**
	 * Serializes the script property data located at Data.  When saving, only saves those properties which differ from the corresponding
	 * value in the specified 'DiffObject' (usually the object's archetype).
	 *
	 * @param	Ar				the archive to use for serialization
	 */
	void SerializeScriptProperties( FArchive& Ar ) const;

	/**
	 * Wrapper function for InitProperties() which handles safely tearing down this object before re-initializing it
	 * from the specified source object.
	 *
	 * @param	SourceObject	the object to use for initializing property values in this object.  If not specified, uses this object's archetype.
	 * @param	InstanceGraph	contains the mappings of instanced objects and components to their templates
	 */
	void ReinitializeProperties( UObject* SourceObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL );

	/**
	 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
	 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
	 * you have a component of interest but what you really want is some characteristic that you can use to track
	 * down where it came from.  
	 *
	 * @note	safe to call on NULL object pointers!
	 */
	FString GetDetailedInfo() const;

	/**
	 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	 * asynchronous cleanup process.
	 */
	bool ConditionalBeginDestroy();
	bool ConditionalFinishDestroy();
	
	/** PostLoad if needed. */
	void ConditionalPostLoad();

	/**
	 * Instances subobjects and components for objects being loaded from disk, if necessary.  Ensures that references
	 * between nested components are fixed up correctly.
	 *
	 * @param	OuterInstanceGraph	when calling this method on subobjects, specifies the instancing graph which contains all instanced
	 *								subobjects and components for a subobject root.
	 */
	void ConditionalPostLoadSubobjects( struct FObjectInstancingGraph* OuterInstanceGraph=NULL );
#if WITH_EDITOR
	/**
	 * Starts caching of platform specific data for the target platform
	 * Called when cooking before serialization so that object can prepare platform specific data
	 * Not called during normal loading of objects
	 * 
	 * @param	TargetPlatform	target platform to cache platform specific data for
	 */
	virtual void BeginCacheForCookedPlatformData( const ITargetPlatform* TargetPlatform ) {  }
	
	/**
	 * Have we finished loading all the cooked platform data for the target platforms requested in BeginCacheForCookedPlatformData
	 * 
	 * @param	TargetPlatform target platform to check for cooked platform data
	 */
	virtual bool IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) { return true; }

	/**
	 * All caching has finished for this object (all IsCachedCookedPlatformDataLoaded functions have finished for all platforms)
	 */
	virtual void WillNeverCacheCookedPlatformDataAgain() { }

	/**
	 * Clears cached cooked platform data for specific platform
	 * 
	 * @param	TargetPlatform	target platform to cache platform specific data for
	 */
	virtual void ClearCachedCookedPlatformData( const ITargetPlatform* TargetPlatform ) {  }

	/**
	 * Clear all cached cooked platform data
	 * 
	 * @param	TargetPlatform	target platform to cache platform specific data for
	 */
	virtual void ClearAllCachedCookedPlatformData() { }

	/**
	 * Called during cook to allow objects to generate additional cooked files alongside their cooked package.
	 * @note These should typically match the name of the package, but with a different extension.
	 *
	 * @param	PackageFilename full path to the package that this object is being saved to on disk
	 * @param	TargetPlatform	target platform to cook additional files for
	 */
	virtual void CookAdditionalFiles( const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform ) { }
#endif
	/**
	 * Determine if this object has SomeObject in its archetype chain.
	 */
	inline bool IsBasedOnArchetype( const UObject* const SomeObject ) const;

	UFunction* FindFunction( FName InName ) const;
	UFunction* FindFunctionChecked( FName InName ) const;

	/**
	 * Given OtherObject (which will be the same type as 'this'), recursively find any matching sub-objects from 'this' that also exist within OtherObject, and add the mappings to ObjectMapping.
	 *
	 * @param	OtherObject		The to find matching sub-objects within.
	 * @param	ObjectMapping	The complete mapping between this object hierarchy and the other object hierarchy.
	 */
	virtual void BuildSubobjectMapping(UObject* OtherObject, TMap<UObject*, UObject*>& ObjectMapping) const;

	/**
	 * Uses the TArchiveObjectReferenceCollector to build a list of all components referenced by this object which have this object as the outer
	 *
	 * @param	OutDefaultSubobjects	the array that should be populated with the default subobjects "owned" by this object
	 * @param	bIncludeNestedSubobjects	controls whether subobjects which are contained by this object, but do not have this object
	 *										as its direct Outer should be included
	 */
	void CollectDefaultSubobjects( TArray<UObject*>& OutDefaultSubobjects, bool bIncludeNestedSubobjects=false );

	/**
	 * Checks default sub-object assumptions.
	 *
	 * @param bForceCheck Force checks even if not enabled globally.
	 * @return true if the assumptions are met, false otherwise.
	 */
	bool CheckDefaultSubobjects(bool bForceCheck = false);

	/**
	 * Save configuration.
	 * @warning: Must be safe on class-default metaobjects.
	 * !!may benefit from hierarchical propagation, deleting keys that match superclass...not sure what's best yet.
	 */
	void SaveConfig( uint64 Flags=CPF_Config, const TCHAR* Filename=NULL, FConfigCacheIni* Config=GConfig );

	/**
	 * Saves just the section(s) for this class into the default ini file for the class (with just the changes from base)
	 */
	void UpdateDefaultConfigFile(const FString& SpecificFileLocation = "");

	/**
	 * Saves just the section(s) for this class into the global user ini file for the class (with just the changes from base)
	 */
	void UpdateGlobalUserConfigFile();

	/**
	 * Saves just the property into the global user ini file for the class (with just the changes from base)
	 */
	void UpdateSinglePropertyInConfigFile(const UProperty* InProperty, const FString& InConfigIniName);

private:
	/**
	 * Saves just the section(s) for this class into the given ini file for the class (with just the changes from base)
	 */
	void UpdateSingleSectionOfConfigFile(const FString& ConfigIniName);

	/**
	 * Ensures that current thread is NOT during vtable ptr retrieval process
	 * of some UClass.
	 */
	void EnsureNotRetrievingVTablePtr() const;

public:
	
	/**
	 * Get the default config filename for the specified UObject
	 */
	FString GetDefaultConfigFilename() const;

	/**
	 * Get the global user override config filename for the specified UObject
	 */
	FString GetGlobalUserConfigFilename() const;

	/** Returns the override config hierarchy platform (if NDAd platforms need defaults to not be in Base*.ini but still want editor to load them) */
	virtual const TCHAR* GetConfigOverridePlatform() const { return nullptr; }

	/**
	 * Imports property values from an .ini file.
	 *
	 * @param	Class				the class to use for determining which section of the ini to retrieve text values from
	 * @param	Filename			indicates the filename to load values from; if not specified, uses ConfigClass's ClassConfigName
	 * @param	PropagationFlags	indicates how this call to LoadConfig should be propagated; expects a bitmask of UE4::ELoadConfigPropagationFlags values.
	 * @param	PropertyToLoad		if specified, only the ini value for the specified property will be imported.
	 */
	void LoadConfig( UClass* ConfigClass=NULL, const TCHAR* Filename=NULL, uint32 PropagationFlags=UE4::LCPF_None, class UProperty* PropertyToLoad=NULL );

	/**
	 * Wrapper method for LoadConfig that is used when reloading the config data for objects at runtime which have already loaded their config data at least once.
	 * Allows the objects the receive a callback that it's configuration data has been reloaded.
	 *
	 * @param	Class				the class to use for determining which section of the ini to retrieve text values from
	 * @param	Filename			indicates the filename to load values from; if not specified, uses ConfigClass's ClassConfigName
	 * @param	PropagationFlags	indicates how this call to LoadConfig should be propagated; expects a bitmask of UE4::ELoadConfigPropagationFlags values.
	 * @param	PropertyToLoad		if specified, only the ini value for the specified property will be imported
	 */
	void ReloadConfig( UClass* ConfigClass=NULL, const TCHAR* Filename=NULL, uint32 PropagationFlags=UE4::LCPF_None, class UProperty* PropertyToLoad=NULL );

	/** Import an object from a file. */
	void ParseParms( const TCHAR* Parms );

	/**
	 * Outputs a string to an arbitrary output device, describing the list of objects which are holding references to this one.
	 *
	 * @param	Ar						the output device to send output to
	 * @param	Referencers				optionally allows the caller to specify the list of references to output.
	 */
	void OutputReferencers( FOutputDevice& Ar, FReferencerInformationList* Referencers=NULL );
	
	// @todo document
	void RetrieveReferencers( TArray<FReferencerInformation>* OutInternalReferencers, TArray<FReferencerInformation>* OutExternalReferencers);

	/**
	 * Changes the linker and linker index to the passed in one. A linker of NULL and linker index of INDEX_NONE
	 * indicates that the object is without a linker.
	 *
	 * @param LinkerLoad				New LinkerLoad object to set
	 * @param LinkerIndex				New LinkerIndex to set
	 * @param bShouldDetachExisting		If true, detach existing linker and call PostLinkerChange
	 */
	void SetLinker( FLinkerLoad* LinkerLoad, int32 LinkerIndex, bool bShouldDetachExisting=true );

	/**
	 * Return the template that an object with this class, outer and name would be
	 * 
	 * @return the archetype for this object
	 */
	static UObject* GetArchetypeFromRequiredInfo(UClass* Class, UObject* Outer, FName Name, EObjectFlags ObjectFlags);

	/**
	 * Return the template this object is based on. 
	 * 
	 * @return the archetype for this object
	 */
	UObject* GetArchetype() const
	{
		return GetArchetypeFromRequiredInfo(GetClass(), GetOuter(), GetFName(), GetFlags());
	}

	/**
	 * Builds a list of objects which have this object in their archetype chain.
	 *
	 * @param	Instances	receives the list of objects which have this one in their archetype chain
	 */
	void GetArchetypeInstances( TArray<UObject*>& Instances );

	/**
	 * Wrapper for calling UClass::InstanceSubobjectTemplates() for this object.
	 */
	void InstanceSubobjectTemplates( struct FObjectInstancingGraph* InstanceGraph = NULL );

	/**
	 * Returns true if this object implements the interface T, false otherwise.
	 */
	template<class T>
	bool Implements() const;

	//==========================================
	// Kismet Virtual Machine
	//==========================================
	//@todo UE4 - do these really need to be virtual to support actors?

	//
	// Script processing functions.
	//
	virtual void ProcessEvent( UFunction* Function, void* Parms );

	/**
	 * Return the space this function should be called.   Checks to see if this function should
	 * be called locally, remotely, or simply absorbed under the given conditions
	 *
	 * @param Function function to call
	 * @param Parameters arguments to the function call
	 * @param Stack stack frame for the function call
	 * @return bitmask representing all callspaces that apply to this UFunction in the given context
	 */
	virtual int32 GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack )
	{
		return FunctionCallspace::Local;
	}

	/**
	 * Call the actor's function remotely
	 *
	 * @param Function function to call
	 * @param Parameters arguments to the function call
	 * @param Stack stack frame for the function call
	 */
	virtual bool CallRemoteFunction( UFunction* Function, void* Parms, struct FOutParmRec* OutParms, FFrame* Stack )
	{
		return false;
	}

	/** Command line. */
	bool CallFunctionByNameWithArguments( const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor, bool bForceCallWithNonExec = false );

	// Call a function
	void CallFunction( FFrame& Stack, RESULT_DECL, UFunction* Function );

	//
	// Internal function call processing.
	// @warning: might not write anything to Result if proper type isn't returned.
	//
	void ProcessInternal( FFrame& Stack, RESULT_DECL );

	/**
	 * This function handles a console exec sent to the object; it is virtual so 'nexus' objects like
	 * a player controller can reroute the command to several different objects.
	 */
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
	{
		return CallFunctionByNameWithArguments(Cmd, Ar, Executor);
	}

	/** advances Stack's code past the parameters to the given Function and if the function has a return value, copies the zero value for that property to the memory for the return value
	 * @param Stack the script stack frame
	 * @param Result pointer to where the return value should be written
	 * @param Function the function being called
	 */
	void SkipFunction(FFrame& Stack, RESULT_DECL, UFunction* Function);

	/**
	 * Called on the target when a class is loaded with ClassGeneratedBy is loaded.  Should regenerate the class if needed, and return the updated class
	 * @return	Updated instance of the class, if needed, or NULL if no regeneration was necessary
	 */
	virtual UClass* RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO, TArray<UObject*>& ObjLoaded) { return NULL; };

	/** 
	 * Returns whether this object is contained in or part of a blueprint object
	 */
	bool IsInBlueprint() const;

	/** 
	 *  Destroy properties that won't be destroyed by the native destructor
	 */
	void DestroyNonNativeProperties();

	virtual void MarkAsEditorOnlySubobject() { }

	// UnrealScript intrinsics.

	// Undefined native handler
	DECLARE_FUNCTION(execUndefined);

	
	// @todo document below declared functions
	// Variables
	DECLARE_FUNCTION(execLocalVariable);
	DECLARE_FUNCTION(execInstanceVariable);
	DECLARE_FUNCTION(execDefaultVariable);
	DECLARE_FUNCTION(execLocalOutVariable);
	DECLARE_FUNCTION(execInterfaceVariable);
	DECLARE_FUNCTION(execInterfaceContext);
	DECLARE_FUNCTION(execArrayElement);
	DECLARE_FUNCTION(execBoolVariable);
	DECLARE_FUNCTION(execClassDefaultVariable);
	DECLARE_FUNCTION(execEndFunctionParms);


	// Do Nothing 
	DECLARE_FUNCTION(execNothing);
	DECLARE_FUNCTION(execNothingOp4a);

	/** Breakpoint; only observed in the editor; executing it at any other time is a NOP */
	DECLARE_FUNCTION(execBreakpoint);

	/** Tracepoint; only observed in the editor; executing it at any other time is a NOP */
	DECLARE_FUNCTION(execTracepoint);
	DECLARE_FUNCTION(execWireTracepoint);

	/** Instrumentation event for profiling; only observed in the builds with blueprint instrumentation */
	DECLARE_FUNCTION(execInstrumentation);

	DECLARE_FUNCTION(execEndOfScript);

	/** failsafe for functions that return a value - returns the zero value for a property and logs that control reached the end of a non-void function */
	DECLARE_FUNCTION(execReturnNothing);
	DECLARE_FUNCTION(execEmptyParmValue);

	// Commands
	DECLARE_FUNCTION(execJump);
	DECLARE_FUNCTION(execJumpIfNot);
	DECLARE_FUNCTION(execAssert);

	/** 
	 * Push a code offset onto the execution flow stack for future execution.  
	 * Current execution continues to the next instruction after the push one.
	 */
	DECLARE_FUNCTION(execPushExecutionFlow);

	/**
	 * Pops a code offset from the execution flow stack and starts execution there.
	 * If there are no stack entries left, it is treated as an execution error.
	 */
	DECLARE_FUNCTION(execPopExecutionFlow);
	DECLARE_FUNCTION(execComputedJump);

	/** 
	 * Pops a code offset from the execution flow stack and starts execution there, if a condition is not true.
	 * If there are no stack entries left, it is treated as an execution error.
	 */
	DECLARE_FUNCTION(execPopExecutionFlowIfNot);


	// Assignment
	DECLARE_FUNCTION(execLet);
	DECLARE_FUNCTION(execLetObj);
	DECLARE_FUNCTION(execLetWeakObjPtr);
	DECLARE_FUNCTION(execLetBool);
	DECLARE_FUNCTION(execLetDelegate);
	DECLARE_FUNCTION(execLetMulticastDelegate);

	// Delegates 
	DECLARE_FUNCTION(execAddMulticastDelegate);
	DECLARE_FUNCTION(execClearMulticastDelegate);
	DECLARE_FUNCTION(execEatReturnValue);
	DECLARE_FUNCTION(execRemoveMulticastDelegate);

	// Context expressions
	DECLARE_FUNCTION(execSelf);
	DECLARE_FUNCTION(execContext);
	DECLARE_FUNCTION(execContext_FailSilent);
	DECLARE_FUNCTION(execStructMemberContext);

	// Function calls
	DECLARE_FUNCTION(execVirtualFunction);
	DECLARE_FUNCTION(execFinalFunction);

	// Struct comparison
	DECLARE_FUNCTION(execStructCmpEq);
	DECLARE_FUNCTION(execStructCmpNe);
	DECLARE_FUNCTION(execStructMember);

	// @todo delegate: Delegate comparison is not supported for multi-cast delegates
	DECLARE_FUNCTION(execEqualEqual_DelegateDelegate);
	DECLARE_FUNCTION(execNotEqual_DelegateDelegate);
	DECLARE_FUNCTION(execEqualEqual_DelegateFunction);
	DECLARE_FUNCTION(execNotEqual_DelegateFunction);

	// Constants
	DECLARE_FUNCTION(execIntConst);
	DECLARE_FUNCTION(execInt64Const);
	DECLARE_FUNCTION(execUInt64Const);
	DECLARE_FUNCTION(execSkipOffsetConst);
	DECLARE_FUNCTION(execFloatConst);
	DECLARE_FUNCTION(execStringConst);
	DECLARE_FUNCTION(execUnicodeStringConst);
	DECLARE_FUNCTION(execTextConst);
	DECLARE_FUNCTION(execObjectConst);
	DECLARE_FUNCTION(execSoftObjectConst);

	// @todo delegate: Multi-cast versions needed for script execution!		(Need Add, Remove, Clear/Empty)
	DECLARE_FUNCTION(execInstanceDelegate);
	DECLARE_FUNCTION(execNameConst);
	DECLARE_FUNCTION(execByteConst);
	DECLARE_FUNCTION(execIntZero);
	DECLARE_FUNCTION(execIntOne);
	DECLARE_FUNCTION(execTrue);
	DECLARE_FUNCTION(execFalse);
	DECLARE_FUNCTION(execNoObject);
	DECLARE_FUNCTION(execNullInterface);
	DECLARE_FUNCTION(execIntConstByte);
	DECLARE_FUNCTION(execRotationConst);
	DECLARE_FUNCTION(execVectorConst);
	DECLARE_FUNCTION(execTransformConst);
	DECLARE_FUNCTION(execStructConst);
	DECLARE_FUNCTION(execSetArray);
	DECLARE_FUNCTION(execSetSet);
	DECLARE_FUNCTION(execSetMap);
	DECLARE_FUNCTION(execArrayConst);
	DECLARE_FUNCTION(execSetConst);
	DECLARE_FUNCTION(execMapConst);

	// Object construction
	DECLARE_FUNCTION(execNew);
	DECLARE_FUNCTION(execClassContext);
	DECLARE_FUNCTION(execNativeParm);

	// Conversions 
	DECLARE_FUNCTION(execDynamicCast);
	DECLARE_FUNCTION(execMetaCast);
	DECLARE_FUNCTION(execPrimitiveCast);
	DECLARE_FUNCTION(execInterfaceCast);

	// Cast functions
	DECLARE_FUNCTION(execObjectToBool);
	DECLARE_FUNCTION(execInterfaceToBool);
	DECLARE_FUNCTION(execObjectToInterface);
	DECLARE_FUNCTION(execInterfaceToInterface);
	DECLARE_FUNCTION(execInterfaceToObject);

	// Dynamic array functions
	// Array support
	DECLARE_FUNCTION(execGetDynArrayElement);
	DECLARE_FUNCTION(execSetDynArrayElement);
	DECLARE_FUNCTION(execGetDynArrayLength);
	DECLARE_FUNCTION(execSetDynArrayLength);
	DECLARE_FUNCTION(execDynArrayInsert);
	DECLARE_FUNCTION(execDynArrayRemove);
	DECLARE_FUNCTION(execDynArrayFind);
	DECLARE_FUNCTION(execDynArrayFindStruct);
	DECLARE_FUNCTION(execDynArrayAdd);
	DECLARE_FUNCTION(execDynArrayAddItem);
	DECLARE_FUNCTION(execDynArrayInsertItem);
	DECLARE_FUNCTION(execDynArrayRemoveItem);
	DECLARE_FUNCTION(execDynArraySort);

	DECLARE_FUNCTION(execBindDelegate);
	DECLARE_FUNCTION(execCallMulticastDelegate);

	DECLARE_FUNCTION(execLetValueOnPersistentFrame);

	DECLARE_FUNCTION(execCallMathFunction);

	DECLARE_FUNCTION(execSwitchValue);

	DECLARE_FUNCTION(execArrayGetByRef);

	// -- K2 support functions
	struct Object_eventExecuteUbergraph_Parms
	{
		int32 EntryPoint;
	};
	void ExecuteUbergraph(int32 EntryPoint)
	{
		Object_eventExecuteUbergraph_Parms Parms;
		Parms.EntryPoint=EntryPoint;
		ProcessEvent(FindFunctionChecked(NAME_ExecuteUbergraph),&Parms);
	}

protected: 

	/** Checks it's ok to perform subobjects check at this time. */
	bool CanCheckDefaultSubObjects(bool bForceCheck, bool& bResult);

	/**
	* Checks default sub-object assumptions.
	*
	* @return true if the assumptions are met, false otherwise.
	*/
	virtual bool CheckDefaultSubobjectsInternal();

private:
	void ProcessContextOpcode(FFrame& Stack, RESULT_DECL, bool bCanFailSilent);

	/**
	* Create a component or subobject only to be used with the editor.
	* @param	Outer						outer to construct the subobject in
	* @param	ReturnType					type of the new component
	* @param	SubobjectName				name of the new component
	* @param	bTransient					true if the component is being assigned to a transient property
	*/
	UObject* CreateEditorOnlyDefaultSubobjectImpl(FName SubobjectName, UClass* ReturnType, bool bTransient = false);
};

/**
* Test validity of object
*
* @param	Test			The object to test
* @return	Return true if the object is usable: non-null and not pending kill
*/
FORCEINLINE bool IsValid(const UObject *Test)
{
	return Test && !Test->IsPendingKill();
}

