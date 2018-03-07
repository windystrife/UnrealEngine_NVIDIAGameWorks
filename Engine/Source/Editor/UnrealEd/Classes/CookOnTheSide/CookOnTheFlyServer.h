// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Package.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformProcess.h"
#include "TickableEditorObject.h"
#include "IPlatformFileSandboxWrapper.h"
#include "INetworkFileSystemModule.h"
#include "CookOnTheFlyServer.generated.h"


class FAssetRegistryGenerator;
class ITargetPlatform;
struct FPropertyChangedEvent;
class IPlugin;
class IAssetRegistry;

enum class ECookInitializationFlags
{
	None =										0x00000000, // No flags
	//unused =									0x00000001, 
	Iterative =									0x00000002, // use iterative cooking (previous cooks will not be cleaned unless detected out of date, experimental)
	SkipEditorContent =							0x00000004, // do not cook any content in the content\editor directory
	Unversioned =								0x00000008, // save the cooked packages without a version number
	AutoTick =									0x00000010, // enable ticking (only works in the editor)
	AsyncSave =									0x00000020, // save packages async
	IncludeServerMaps =							0x00000080, // should we include the server maps when cooking
	UseSerializationForPackageDependencies =	0x00000100, // should we use the serialization code path for generating package dependencies (old method will be deprecated)
	BuildDDCInBackground =						0x00000200, // build ddc content in background while the editor is running (only valid for modes which are in editor IsCookingInEditor())
	GeneratedAssetRegistry =					0x00000400, // have we generated asset registry yet
	OutputVerboseCookerWarnings =				0x00000800, // output additional cooker warnings about content issues
	EnablePartialGC =							0x00001000, // mark up with an object flag objects which are in packages which we are about to use or in the middle of using, this means we can gc more often but only gc stuff which we have finished with
	TestCook =									0x00002000, // test the cooker garbage collection process and cooking (cooker will never end just keep testing).
	//unused =									0x00004000,
	LogDebugInfo =								0x00008000, // enables additional debug log information
	IterateSharedBuild =						0x00010000, // iterate from a build in the SharedIterativeBuild directory 
	IgnoreIniSettingsOutOfDate =				0x00020000, // if the inisettings say the cook is out of date keep using the previously cooked build
	IgnoreScriptPackagesOutOfDate =				0x00040000, // for incremental cooking, ignore script package changes
};
ENUM_CLASS_FLAGS(ECookInitializationFlags);

enum class ECookByTheBookOptions
{
	None =								0x00000000, // no flags
	CookAll	=							0x00000001, // cook all maps and content in the content directory
	MapsOnly =							0x00000002, // cook only maps
	NoDevContent =						0x00000004, // don't include dev content
	LeakTest =							0x00000008, // test for uobject leaks after each level load
	ForceDisableCompressed =			0x00000010, // force compression to be disabled even if the cooker was initialized with it enabled
	ForceEnableCompressed =				0x00000020, // force compression to be on even if the cooker was initialized with it disabled
	ForceDisableSaveGlobalShaders =		0x00000040, // force global shaders to not be saved (used if cooking multiple times for the same platform and we know we are up todate)
	NoGameAlwaysCookPackages =			0x00000080, // don't include the packages specified by the game in the cook (this cook will probably be missing content unless you know what you are doing)
	NoAlwaysCookMaps =					0x00000100, // don't include always cook maps (this cook will probably be missing content unless you know what you are doing)
	NoDefaultMaps =						0x00000200, // don't include default cook maps (this cook will probably be missing content unless you know what you are doing)
	NoSlatePackages =					0x00000400, // don't include slate content (this cook will probably be missing content unless you know what you are doing)
	NoInputPackages =					0x00000800, // don't include slate content (this cook will probably be missing content unless you know what you are doing)
	DisableUnsolicitedPackages =		0x00001000, // don't cook any packages which aren't in the files to cook list (this is really dangerious as if you request a file it will not cook all it's dependencies automatically)
};
ENUM_CLASS_FLAGS(ECookByTheBookOptions);


UENUM()
namespace ECookMode
{
	enum Type
	{
		/** Default mode, handles requests from network. */
		CookOnTheFly,
		/** Cook on the side. */
		CookOnTheFlyFromTheEditor,
		/** Precook all resources while in the editor. */
		CookByTheBookFromTheEditor,
		/** Cooking by the book (not in the editor). */
		CookByTheBook,
	};
}

UENUM()
enum class ECookTickFlags : uint8
{
	None =									0x00000000, /* no flags */
	MarkupInUsePackages =					0x00000001, /** Markup packages for partial gc */
};
ENUM_CLASS_FLAGS(ECookTickFlags);

// hudson is the name of my favorite dwagon

DECLARE_STATS_GROUP(TEXT("Cooking"), STATGROUP_Cooking, STATCAT_Advanced);

UCLASS()
class UNREALED_API UCookOnTheFlyServer : public UObject, public FTickableEditorObject, public FExec
{
	GENERATED_BODY()

		UCookOnTheFlyServer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:

	/** Array which has been made thread safe :) */
	template<typename Type, typename SynchronizationObjectType, typename ScopeLockType>
	struct FUnsynchronizedQueue
	{
	private:
		mutable SynchronizationObjectType	SynchronizationObject; // made this mutable so this class can have const functions and still be thread safe
		TArray<Type>		Items;
	public:
		void Enqueue(const Type& Item)
		{
			ScopeLockType ScopeLock(&SynchronizationObject);
			Items.Add(Item);
		}
		void EnqueueUnique( const Type& Item )
		{
			ScopeLockType ScopeLock(&SynchronizationObject);
			Items.AddUnique(Item);
		}
		bool Dequeue(Type* Result)
		{
			ScopeLockType ScopeLock(&SynchronizationObject);
			if (Items.Num())
			{
				*Result = Items[0];
				Items.RemoveAt(0);
				return true;
			}
			return false;
		}
		void DequeueAll(TArray<Type>& Results)
		{
			ScopeLockType ScopeLock(&SynchronizationObject);
			Results += Items;
			Items.Empty();
		}

		bool HasItems() const
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			return Items.Num() > 0;
		}

		void Remove( const Type& Item ) 
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			Items.Remove( Item );
		}

		void CopyItems( TArray<Type> &InItems ) const
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			InItems = Items;
		}

		int Num() const 
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			return Items.Num();
		}

		void Empty()
		{
			ScopeLockType ScopeLock( &SynchronizationObject );
			Items.Empty();
		}
	};



	struct FDummyCriticalSection
	{
	public:
		FORCEINLINE void Lock() { }
		FORCEINLINE void Unlock() { }
	};

	struct FDummyScopeLock
	{
	public:
		FDummyScopeLock( FDummyCriticalSection * ) { }
	};

public:


	template<typename Type>
	struct FThreadSafeQueue : public FUnsynchronizedQueue<Type, FCriticalSection, FScopeLock>
	{
		/**
		* Don't add any functions here, this is just a overqualified typedef
		* Add functions / functionality to the FUnsynchronizedQueue
		*/
	};

	template<typename Type>
	struct FQueue : public FUnsynchronizedQueue<Type, FDummyCriticalSection, FDummyScopeLock>
	{
		/**
		* Don't add any functions here, this is just a overqualified typedef
		* Add functions / functionality to the FUnsynchronizedQueue
		*/
	};

public:
	/** cooked file requests which includes platform which file is requested for */
	struct FFilePlatformRequest
	{
	protected:
		FName Filename;
		TArray<FName> PlatformNames;
	public:

		// yes we have some friends
		friend uint32 GetTypeHash(const UCookOnTheFlyServer::FFilePlatformRequest& Key);

		FFilePlatformRequest() { }


		FFilePlatformRequest( const FName& InFileName, const FName& InPlatformName ) : Filename( InFileName )
		{ PlatformNames.Add( InPlatformName ); }

		FFilePlatformRequest( const FName& InFilename, const TArray<FName>& InPlatformName ) : Filename( InFilename )
		{ PlatformNames = InPlatformName; }

		FFilePlatformRequest( const FName& InFilename, TArray<FName>&& InPlatformName ) : Filename( InFilename ), PlatformNames(MoveTemp(InPlatformName)) { }
		FFilePlatformRequest( const FFilePlatformRequest& InFilePlatformRequest ) : Filename( InFilePlatformRequest.Filename ), PlatformNames( InFilePlatformRequest.PlatformNames ) { }
		FFilePlatformRequest( FFilePlatformRequest&& InFilePlatformRequest ) : Filename( MoveTemp(InFilePlatformRequest.Filename) ), PlatformNames( MoveTemp(InFilePlatformRequest.PlatformNames) ) { }

		void SetFilename( const FString &InFilename ) 
		{
			Filename = FName(*InFilename);
		}

		const FName &GetFilename() const
		{
			return Filename;
		}

		const TArray<FName>& GetPlatformNames() const
		{
			return PlatformNames;
		}

		void RemovePlatform( const FName &Platform )
		{
			PlatformNames.Remove(Platform);
		}

		void AddPlatform( const FName &Platform )
		{
			check( Platform != NAME_None );
			PlatformNames.Add(Platform );
		}

		bool HasPlatform( const FName &Platform ) const
		{
			return PlatformNames.Find(Platform) != INDEX_NONE;
		}

		bool IsValid()  const
		{
			return Filename != NAME_None;
		}

		void Clear()
		{
			Filename = TEXT("");
			PlatformNames.Empty();
		}

		FFilePlatformRequest &operator=( FFilePlatformRequest &&InFileRequest )
		{
			Filename = MoveTemp( InFileRequest.Filename );
			PlatformNames = MoveTemp( InFileRequest.PlatformNames );
			return *this;
		}

		bool operator ==( const FFilePlatformRequest &InFileRequest ) const
		{
			if ( InFileRequest.Filename == Filename )
			{
				if ( InFileRequest.PlatformNames == PlatformNames )
				{
					return true;
				}
			}
			return false;
		}

		FORCEINLINE FString ToString() const
		{
			FString Result = FString::Printf(TEXT("%s;"), *Filename.ToString());

			for ( const FName& Platform : PlatformNames )
			{
				Result += FString::Printf(TEXT("%s,"), *Platform.ToString() );
			}
			return Result;
		}

	};


private:

	struct FFilePlatformCookedPackage : public FFilePlatformRequest
	{
	private:
		TArray<bool> bSucceededSavePackage; // one bool for each platform
	public:
		FFilePlatformCookedPackage(const FFilePlatformRequest& InFilePlatformRequest, const TArray<bool>& bInSuccededSavePackage) : FFilePlatformRequest(InFilePlatformRequest.GetFilename(), InFilePlatformRequest.GetPlatformNames()), bSucceededSavePackage(bInSuccededSavePackage) { check(PlatformNames.Num() == bSucceededSavePackage.Num()); }
		FFilePlatformCookedPackage(const FName& InFilename, const TArray<FName>& InPlatformName) : FFilePlatformRequest(InFilename, InPlatformName)
		{ 
			// only use this constructor to short hand when packages fail
			for ( int32 I = 0; I < InPlatformName.Num(); ++I )
			{
				bSucceededSavePackage.Add(false);
			}
			check(PlatformNames.Num() == bSucceededSavePackage.Num());
		}
		FFilePlatformCookedPackage(const FName& InFilename, const TArray<FName>& InPlatformName, const TArray<bool>& bInSuccededSavePackage) : FFilePlatformRequest(InFilename, InPlatformName), bSucceededSavePackage(bInSuccededSavePackage) { check(PlatformNames.Num() == bSucceededSavePackage.Num()); }
		FFilePlatformCookedPackage(const FName& InFilename, TArray<FName>&& InPlatformName, TArray<bool>&& bInSuccededSavePackage) : FFilePlatformRequest(InFilename, MoveTemp(InPlatformName)), bSucceededSavePackage(MoveTemp(bInSuccededSavePackage)) { check(PlatformNames.Num() == bSucceededSavePackage.Num()); }
		FFilePlatformCookedPackage(const FFilePlatformCookedPackage& InFilePlatformRequest) : FFilePlatformRequest(InFilePlatformRequest.Filename, InFilePlatformRequest.PlatformNames), bSucceededSavePackage(InFilePlatformRequest.bSucceededSavePackage) { check(PlatformNames.Num() == bSucceededSavePackage.Num());  }
		FFilePlatformCookedPackage(FFilePlatformCookedPackage&& InFilePlatformRequest) : FFilePlatformRequest(MoveTemp(InFilePlatformRequest.Filename), MoveTemp(InFilePlatformRequest.PlatformNames)), bSucceededSavePackage(InFilePlatformRequest.bSucceededSavePackage) { check(PlatformNames.Num() == bSucceededSavePackage.Num()); }

		inline const void AddPlatform( const FName& Platform, bool bSucceeded )
		{
			check(PlatformNames.Num() == bSucceededSavePackage.Num());
			check(Platform != NAME_None);
			PlatformNames.Add(Platform);
			bSucceededSavePackage.Add(bSucceeded);
			check( PlatformNames.Num() == bSucceededSavePackage.Num() );
		}

		inline void RemovePlatform(const FName &Platform)
		{
			check(PlatformNames.Num() == bSucceededSavePackage.Num());
			int32 Index = PlatformNames.IndexOfByKey(Platform);
			if (Index != -1)
			{
				PlatformNames.RemoveAt(Index);
				bSucceededSavePackage.RemoveAt(Index);
			}
			check(PlatformNames.Num() == bSucceededSavePackage.Num());
		}

		inline const bool HasSucceededSavePackage(const FName& PlatformName) const 
		{ 
			int32 Index = PlatformNames.IndexOfByKey(PlatformName);
			if ( (Index != -1) && (Index < bSucceededSavePackage.Num()) )
			{
				return bSucceededSavePackage[Index]; 
			}
			return false;
		}
	};

	/** Helper list of all files which have been cooked */
	struct FThreadSafeFilenameSet
	{
	private:
		mutable FCriticalSection	SynchronizationObject;
		TMap<FName, FFilePlatformCookedPackage> FilesProcessed;
	public:

		void Lock()
		{
			SynchronizationObject.Lock();
		}
		void Unlock()
		{
			SynchronizationObject.Unlock();
		}

		void Add(const FFilePlatformCookedPackage& Request)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			check(Request.IsValid());

			// see if it's already in the requests list
			FFilePlatformCookedPackage *ExistingRequest = FilesProcessed.Find(Request.GetFilename() );

			if ( ExistingRequest )
			{
				check( ExistingRequest->GetFilename() == Request.GetFilename() );
				for ( const FName& Platform : Request.GetPlatformNames() )
				{
					const bool bSucceeded = Request.HasSucceededSavePackage(Platform);
					ExistingRequest->AddPlatform(Platform, bSucceeded);
				}
			}
			else
				FilesProcessed.Add(Request.GetFilename(), Request);
		}
		bool Exists(const FFilePlatformRequest & Request) const
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			const FFilePlatformCookedPackage* OurRequest = FilesProcessed.Find(Request.GetFilename());

			if (!OurRequest)
			{
				return false;
			}

			// make sure all the platforms are completed
			for ( const FName& Platform : Request.GetPlatformNames() )
			{
				if ( !OurRequest->GetPlatformNames().Contains( Platform ) )
				{
					return false;
				}
			}

			return true;
			// return FilesProcessed.Contains(Filename);
		}
		// two versions of this function so I don't have to create temporary FFIleplatformRequest in some cases to call the exists function
		bool Exists( const FName& Filename, const TArray<FName>& PlatformNames ) const
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			const FFilePlatformRequest* OurRequest = FilesProcessed.Find( Filename );

			if (!OurRequest)
			{
				return false;
			}

			// make sure all the platforms are completed
			for ( const FName& Platform : PlatformNames )
			{
				if ( !OurRequest->GetPlatformNames().Contains( Platform ) )
				{
					return false;
				}
			}

			return true;
		}

		// do we want failed packages or not
		bool Exists( const FName& Filename, const TArray<FName>& PlatformNames, bool bIncludeFailed ) const
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			const FFilePlatformCookedPackage* OurRequest = FilesProcessed.Find(Filename);

			if (!OurRequest)
			{
				return false;
			}

			if (bIncludeFailed == false)
			{
				bool allFailed = true;
				for ( const auto& PlatformName : PlatformNames )
				{
					if (OurRequest->HasSucceededSavePackage(PlatformName))
					{
						allFailed = false;
						break;
					}
				}
				if ( allFailed )
				{
					return false;
				}
			}

			// make sure all the platforms are completed
			for (const FName& Platform : PlatformNames)
			{
				if (!OurRequest->GetPlatformNames().Contains(Platform))
				{
					return false;
				}
			}
			return true;
		}

		void RemoveAllFilesForPlatform( const FName& PlatformName )
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			for ( auto& Request : FilesProcessed )
			{	
				Request.Value.RemovePlatform( PlatformName );
			}
		}

		bool GetCookedPlatforms( const FName& Filename, TArray<FName>& PlatformList ) const
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			const FFilePlatformCookedPackage* Request = FilesProcessed.Find(Filename);
			if ( Request )
			{
				PlatformList = Request->GetPlatformNames();
				return true;
			}
			return false;
		}
		int RemoveFile( const FName& Filename )
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			return FilesProcessed.Remove( Filename );
		}

		bool RemoveFileForPlatform( const FName& Filename, const FName& PlatformName )
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			FFilePlatformCookedPackage* ProcessedFile = FilesProcessed.Find(Filename);
			if( ProcessedFile )
			{
				ProcessedFile->RemovePlatform(PlatformName);
				return true; 
			}
			return false;
		}

		void GetCookedFilesForPlatform(const FName& PlatformName, TArray<FName>& CookedFiles, bool bGetFailedCookedPackages = true, bool bGetSuccessfulCookedPackages = true)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			for (const auto& CookedFile : FilesProcessed)
			{
				if (CookedFile.Value.HasPlatform(PlatformName))
				{
					bool bHasSucceededSavePackage = CookedFile.Value.HasSucceededSavePackage(PlatformName);
					if ((bHasSucceededSavePackage && bGetSuccessfulCookedPackages) ||
						((bHasSucceededSavePackage == false) && bGetFailedCookedPackages) )
						CookedFiles.Add(CookedFile.Value.GetFilename());
				}
			}
		}
		void Empty(int32 ExpectedNumElements = 0)
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			FilesProcessed.Empty(ExpectedNumElements);
		}
	};


	struct FFilenameQueue
	{
	private:
		TArray<FName>			Queue;
		TMap<FName, TArray<FName>> PlatformList;
		mutable FCriticalSection SynchronizationObject;
	public:

		template <class PREDICATE_CLASS>
		void Sort(const PREDICATE_CLASS& Predicate)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			Queue.Sort(Predicate);
		}

		const TArray<FName>& GetQueue() const { return Queue;  }
		void EnqueueUnique(const FFilePlatformRequest& Request, bool ForceEnqueFront = false)
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			TArray<FName>* Platforms = PlatformList.Find(Request.GetFilename());
			if ( Platforms == NULL )
			{
				PlatformList.Add(Request.GetFilename(), Request.GetPlatformNames());
				Queue.Add(Request.GetFilename());
			}
			else
			{
				// add the requested platforms to the platform list
				for ( const FName& Platform : Request.GetPlatformNames() )
				{
					Platforms->AddUnique(Platform);
				}
			}

			if ( ForceEnqueFront )
			{
				int32 Index = Queue.Find(Request.GetFilename());
				check( Index != INDEX_NONE );
				if ( Index != 0 )
				{
					Queue.Swap(0, Index);
				}
			}
		}

		bool Dequeue(FFilePlatformRequest* Result)
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			if (Queue.Num())
			{
				FName Filename = Queue[0];
				Queue.RemoveAt(0);
				TArray<FName> Platforms = PlatformList.FindChecked(Filename);
				PlatformList.Remove(Filename);
				*Result = FFilePlatformRequest(MoveTemp(Filename), MoveTemp(Platforms));
				return true;
			}
			return false;
		}

		void DequeueAllRequests( TArray<FFilePlatformRequest>& RequestArray )
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			if ( Queue.Num() )
			{
				for ( const auto& Request : PlatformList )
				{
					RequestArray.Add( FFilePlatformRequest( Request.Key, Request.Value ) );
				}
				PlatformList.Empty();
				Queue.Empty();
			}
		}

		bool Exists( const FName& Filename, const TArray<FName>& PlatformNames ) const 
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			const TArray<FName>* Platforms = PlatformList.Find( Filename );
			if ( Platforms == NULL )
				return false;

			for ( const FName& PlatformName : PlatformNames )
			{
				if ( !Platforms->Contains( PlatformName ) )
					return false;
			}
			return true;
		}

		bool Exists(const FName& Filename)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			const TArray<FName>* Platforms = PlatformList.Find(Filename);
			if (Platforms == NULL)
				return false;
			return true;
		}

		bool HasItems() const
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			return Queue.Num() > 0;
		}

		int Num() const 
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			return Queue.Num();
		}


		void Empty()  
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			Queue.Empty();
			PlatformList.Empty();
		}
	};

	struct FThreadSafeUnsolicitedPackagesList
	{
	private:
		FCriticalSection SyncObject;
		TArray<FFilePlatformRequest> CookedPackages;
	public:
		void AddCookedPackage( const FFilePlatformRequest& PlatformRequest )
		{
			FScopeLock S( &SyncObject );
			CookedPackages.Add( PlatformRequest );
		}
		void GetPackagesForPlatformAndRemove( const FName& Platform, TArray<FName> PackageNames )
		{
			FScopeLock S( &SyncObject );

			for ( int I = CookedPackages.Num()-1; I >= 0; --I )
			{
				FFilePlatformRequest &Request = CookedPackages[I];

				if ( Request.GetPlatformNames().Contains( Platform ) )
				{
					// remove the platform
					Request.RemovePlatform( Platform );

					if ( Request.GetPlatformNames().Num() == 0 )
					{
						CookedPackages.RemoveAt(I);
					}
				}
			}
		}
		void Empty()
		{
			FScopeLock S( &SyncObject );
			CookedPackages.Empty();
		}
	};


	struct FCachedPackageFilename
	{
	public:
		FCachedPackageFilename(FString &&InPackageFilename, FString &&InStandardFilename, FName InStandardFileFName ) :
			PackageFilename( MoveTemp( InPackageFilename )),
			StandardFilename(MoveTemp(InStandardFilename)),
			StandardFileFName( InStandardFileFName )
		{
		}

		FCachedPackageFilename( const FCachedPackageFilename &In )
		{
			PackageFilename = In.PackageFilename;
			StandardFilename = In.StandardFilename;
			StandardFileFName = In.StandardFileFName;
		}

		FCachedPackageFilename( FCachedPackageFilename &&In )
		{
			PackageFilename = MoveTemp(In.PackageFilename);
			StandardFilename = MoveTemp(In.StandardFilename);
			StandardFileFName = In.StandardFileFName;
		}

		FString PackageFilename; // this is also full path
		FString StandardFilename;
		FName StandardFileFName;
	};

	/** Simple thread safe proxy for TSet<FName> */
	class FThreadSafeNameSet
	{
		TSet<FName> Names;
		FCriticalSection NamesCritical;
	public:
		void Add(FName InName)
		{
			FScopeLock NamesLock(&NamesCritical);
			Names.Add(InName);
		}
		bool AddUnique(FName InName)
		{
			FScopeLock NamesLock(&NamesCritical);
			if (!Names.Contains(InName))
			{
				Names.Add(InName);
				return true;
			}
			return false;
		}
		bool Contains(FName InName)
		{
			FScopeLock NamesLock(&NamesCritical);
			return Names.Contains(InName);
		}
		void Remove(FName InName)
		{
			FScopeLock NamesLock(&NamesCritical);
			Names.Remove(InName);
		}
		void Empty()
		{
			FScopeLock NamesLock(&NamesCritical);
			Names.Empty();
		}
		void GetNames(TSet<FName>& OutNames)
		{
			FScopeLock NamesLock(&NamesCritical);
			OutNames.Append(Names);
		}
	};
private:
	/** Current cook mode the cook on the fly server is running in */
	ECookMode::Type CurrentCookMode;
	/** Directory to output to instead of the default should be empty in the case of DLC cooking */ 
	FString OutputDirectoryOverride;
	//////////////////////////////////////////////////////////////////////////
	// Cook by the book options
public:
	struct FChildCooker
	{
		FChildCooker() : ReadPipe(nullptr), ReturnCode(-1), bFinished(false),  Thread(nullptr) { }

		FProcHandle ProcessHandle;
		FString ResponseFileName;
		FString BaseResponseFileName;
		void* ReadPipe;
		int32 ReturnCode;
		bool bFinished;
		FRunnableThread* Thread;
	};
private:
	struct FCookByTheBookOptions
	{
	public:
		FCookByTheBookOptions() : bLeakTest(false),
			bGenerateStreamingInstallManifests(false),
			bGenerateDependenciesForMaps(false),
			bRunning(false),
			CookTime( 0.0 ),
			CookStartTime( 0.0 ), 
			bErrorOnEngineContentUse(false),
			bIsChildCooker(false),
			bDisableUnsolicitedPackages(false),
			ChildCookIdentifier(-1)
		{ }

		/** Should we test for UObject leaks */
		bool bLeakTest;
		/** Should we generate streaming install manifests (only valid option in cook by the book) */
		bool bGenerateStreamingInstallManifests;
		/** Should we generate a seperate manifest for map dependencies */
		bool bGenerateDependenciesForMaps;
		/** Is cook by the book currently running */
		bool bRunning;
		/** Cancel has been queued will be processed next tick */
		bool bCancel;
		/** DlcName setup if we are cooking dlc will be used as the directory to save cooked files to */
		FString DlcName;
		/** Create a release from this manifest and store it in the releases directory for this cgame */
		FString CreateReleaseVersion;
		/** Leak test: last gc items (only valid when running from commandlet requires gc between each cooked package) */
		TSet<FWeakObjectPtr> LastGCItems;
		/** Dependency graph of maps as root objects. */
		TMap<FName, TMap< FName, TSet <FName> > > MapDependencyGraphs; 
		/** If a cook is cancelled next cook will need to resume cooking */ 
		TArray<FFilePlatformRequest> PreviousCookRequests; 
		/** If we are based on a release version of the game this is the set of packages which were cooked in that release. Map from platform name to list of uncooked package filenames */
		TMap<FName,TArray<FName> > BasedOnReleaseCookedPackages;
		/** Timing information about cook by the book */
		double CookTime;
		double CookStartTime;
		/** error when detecting engine content being used in this cook */
		bool bErrorOnEngineContentUse;
		bool bIsChildCooker;
		bool bDisableUnsolicitedPackages;
		int32 ChildCookIdentifier;
		FString ChildCookFilename;
		TSet<FName> ChildUnsolicitedPackages;
		TArray<FChildCooker> ChildCookers;
		TArray<FName> TargetPlatformNames;
		TArray<FName> StartupPackages;
	};
	FCookByTheBookOptions* CookByTheBookOptions;


	//////////////////////////////////////////////////////////////////////////
	// Cook on the fly options
	/** Cook on the fly server uses the NetworkFileServer */
	TArray<class INetworkFileServer*> NetworkFileServers;
	FOnFileModifiedDelegate FileModifiedDelegate;

	//////////////////////////////////////////////////////////////////////////
	// General cook options
	TArray<UClass*> FullGCAssetClasses;
	/** Number of packages to load before performing a garbage collect. Set to 0 to never GC based on number of loaded packages */
	uint32 PackagesPerGC;
	/** Amount of time that is allowed to be idle before forcing a garbage collect. Set to 0 to never force GC due to idle time */
	double IdleTimeToGC;
	/** Max memory the cooker should use before forcing a gc */
	uint64 MaxMemoryAllowance;
	/** Min memory before the cooker should partial gc */
	uint64 MinMemoryBeforeGC;
	/** If we have less then this much memory free then finish current task and kick off gc */
	uint64 MinFreeMemory;
	/** Max number of packages to save before we partial gc */
	int32 MaxNumPackagesBeforePartialGC;
	/** Max number of conncurrent shader jobs reducing this too low will increase cook time */
	int32 MaxConcurrentShaderJobs;
	ECookInitializationFlags CookFlags;
	TUniquePtr<class FSandboxPlatformFile> SandboxFile;
	bool bIsInitializingSandbox; // stop recursion into callbacks when we are initializing sandbox
	mutable bool bIgnoreMarkupPackageAlreadyLoaded; // avoid marking up packages as already loaded (want to put this around some functionality as we want to load packages fully some times)
	bool bIsSavingPackage; // used to stop recursive mark package dirty functions


	TMap<FName, int32> MaxAsyncCacheForType; // max number of objects of a specific type which are allowed to async cache at once
	mutable TMap<FName, int32> CurrentAsyncCacheForType; // max number of objects of a specific type which are allowed to async cache at once

	/** List of additional plugin directories to remap into the sandbox as needed */
	TArray<TSharedRef<IPlugin> > PluginsToRemap;

	//////////////////////////////////////////////////////////////////////////
	// precaching system
	// this system precaches materials and textures before we have considered the object as requiring save so as to utilize the system when it's idle
	TArray<FWeakObjectPtr> CachedMaterialsToCacheArray;
	TArray<FWeakObjectPtr> CachedTexturesToCacheArray;
	int32 LastUpdateTick;
	int32 MaxPrecacheShaderJobs;
	void TickPrecacheObjectsForPlatforms(const float TimeSlice, const TArray<const ITargetPlatform*>& TargetPlatform);

	// presave system
	// call this to save packages which are in memory as cooked packages, useful when the editor is idle
	// shouldn't consume additional resources
	TArray<const ITargetPlatform*> PresaveTargetPlatforms;
	void OpportunisticSaveInMemoryPackages();

	//////////////////////////////////////////////////////////////////////////

	// data about the current packages being processed
	// stores temporal state like finished cache as an optimization so we don't need to 
	struct FReentryData
	{
		FName FileName;
		bool bBeginCacheFinished;
		int BeginCacheCount;
		bool bFinishedCacheFinished;
		bool bIsValid;
		TArray<UObject*> CachedObjectsInOuter;
		TMap<FName, int32> BeginCacheCallCount;

		FReentryData() : FileName(NAME_None), bBeginCacheFinished(false), BeginCacheCount(0), bFinishedCacheFinished(false), bIsValid(false)
		{ }

		void Reset( const FName& InFilename )
		{
			FileName = InFilename;
			bBeginCacheFinished = false;
			BeginCacheCount = 0;
			bIsValid = false;
		}
	};

	mutable TMap<FName, FReentryData> PackageReentryData;

	FReentryData& GetReentryData(const UPackage* Package) const;

	FThreadSafeQueue<struct FRecompileRequest*> RecompileRequests;
	FFilenameQueue CookRequests; // list of requested files
	FThreadSafeUnsolicitedPackagesList UnsolicitedCookedPackages;
	FThreadSafeFilenameSet CookedPackages; // set of files which have been cooked when needing to recook a file the entry will need to be removed from here
	FThreadSafeNameSet NeverCookPackageList;
	FThreadSafeNameSet UncookedEditorOnlyPackages; // set of packages that have been rejected due to being referenced by editor-only properties


	FString GetCachedPackageFilename( const FName& PackageName ) const;
	FString GetCachedStandardPackageFilename( const FName& PackageName ) const;
	FName GetCachedStandardPackageFileFName( const FName& PackageName ) const;
	FString GetCachedPackageFilename( const UPackage* Package ) const;
	FString GetCachedStandardPackageFilename( const UPackage* Package ) const;
	FName GetCachedStandardPackageFileFName( const UPackage* Package ) const;
	const FString& GetCachedSandboxFilename( const UPackage* Package, TUniquePtr<class FSandboxPlatformFile>& SandboxFile ) const;
	const FName* GetCachedPackageFilenameToPackageFName(const FName& StandardPackageFilename) const;
	const FCachedPackageFilename& Cache(const FName& PackageName) const;
	void ClearPackageFilenameCache() const;
	bool ClearPackageFilenameCacheForPackage( const UPackage* Package ) const;
	bool ClearPackageFilenameCacheForPackage( const FName& PackageName ) const;

	FString ConvertCookedPathToUncookedPath(const FString& CookedPackageName) const;

	// get dependencies for this package 
	const TArray<FName>& GetFullPackageDependencies(const FName& PackageName) const;
	mutable TMap<FName, TArray<FName>> CachedFullPackageDependencies;

	// declared mutable as it's used to cache package filename strings and I don't want to declare all functions using it as non const
	// used by GetCached * Filename functions
	mutable TMap<FName, FCachedPackageFilename> PackageFilenameCache; // filename cache (only process the string operations once)
	mutable TMap<FName, FName> PackageFilenameToPackageFNameCache;

	/** Cached copy of asset registry */
	IAssetRegistry* AssetRegistry;

	/** Map of platform name to asset registry generators, which hold the state of asset registry data for a platform */
	TMap<FName, FAssetRegistryGenerator*> RegistryGenerators;

	/** List of filenames that may be out of date in the asset registry */
	TSet<FName> ModifiedAssetFilenames;

	//////////////////////////////////////////////////////////////////////////
	// iterative ini settings checking
	// growing list of ini settings which are accessed over the course of the cook

	// tmap of the Config name, Section name, Key name, to the value
	typedef TMap<FName, TMap<FName, TMap<FName, TArray<FString>>>> FIniSettingContainer;

	mutable bool IniSettingRecurse;
	mutable FIniSettingContainer AccessedIniStrings;
	TArray<const FConfigFile*> OpenConfigFiles;
	TArray<FString> ConfigSettingBlacklist;
	void OnFConfigDeleted(const FConfigFile* Config);
	void OnFConfigCreated(const FConfigFile* Config);

	void ProcessAccessedIniSettings(const FConfigFile* Config, FIniSettingContainer& AccessedIniStrings) const;

	/**
	* OnTargetPlatformChangedSupportedFormats
	* called when target platform changes the return value of supports shader formats 
	* used to reset the cached cooked shaders
	*/
	void OnTargetPlatformChangedSupportedFormats(const ITargetPlatform* TargetPlatform);

	/**
	 * Returns the current set of cooking targetplatforms
	 *  mostly used for cook on the fly or in situations where the cooker can't figure out what the target platform is
	 * 
	 * @return Array of target platforms which are can be used
	 */
	const TArray<ITargetPlatform*>& GetCookingTargetPlatforms() const;

	/** Cached cooking target platforms from the targetmanager, these are used when we don't know what platforms we should be targeting */
	mutable TArray<ITargetPlatform*> CookingTargetPlatforms;

public:

	enum ECookOnTheSideResult
	{
		COSR_CookedMap				= 0x00000001,
		COSR_CookedPackage			= 0x00000002,
		COSR_ErrorLoadingPackage	= 0x00000004,
		COSR_RequiresGC				= 0x00000008,
		COSR_WaitingOnCache			= 0x00000010,
		COSR_WaitingOnChildCookers	= 0x00000020,
		COSR_MarkedUpKeepPackages	= 0x00000040
	};


	virtual ~UCookOnTheFlyServer();

	/**
	* FTickableEditorObject interface used by cook on the side
	*/
	TStatId GetStatId() const override;
	void Tick(float DeltaTime) override;
	bool IsTickable() const override;
	ECookMode::Type GetCookMode() const { return CurrentCookMode; }


	/**
	* FExec interface used in the editor
	*/
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Dumps cooking stats to the log
	 *  run from the exec command "Cook stats"
	 */
	void DumpStats();

	/**
	* Initialize the CookServer so that either CookOnTheFly can be called or Cook on the side can be started and ticked
	*/
	void Initialize( ECookMode::Type DesiredCookMode, ECookInitializationFlags InCookInitializationFlags, const FString& OutputDirectoryOverride = FString() );

	/**
	* Cook on the side, cooks while also running the editor...
	*
	* @param  BindAnyPort					Whether to bind on any port or the default port.
	*
	* @return true on success, false otherwise.
	*/
	bool StartNetworkFileServer( bool BindAnyPort );

	/**
	* Broadcast our the fileserver presence on the network
	*/
	bool BroadcastFileserverPresence( const FGuid &InstanceId );
	/** 
	* Stop the network file server
	*
	*/
	void EndNetworkFileServer();


	struct FCookByTheBookStartupOptions
	{
	public:
		TArray<ITargetPlatform*> TargetPlatforms;
		TArray<FString> CookMaps;
		TArray<FString> CookDirectories;
		TArray<FString> NeverCookDirectories;
		TArray<FString> CookCultures; 
		TArray<FString> IniMapSections;
		TArray<FString> CookPackages; // list of packages we should cook, used to specify specific packages to cook
		ECookByTheBookOptions CookOptions;
		FString DLCName;
		FString CreateReleaseVersion;
		FString BasedOnReleaseVersion;
		FString ChildCookFileName; // if we are the child cooker 
		int32 ChildCookIdentifier; // again, only if you are the child cooker
		bool bGenerateStreamingInstallManifests; 
		bool bGenerateDependenciesForMaps; 
		bool bErrorOnEngineContentUse; // this is a flag for dlc, will cause the cooker to error if the dlc references engine content
		int32 NumProcesses;
		FCookByTheBookStartupOptions() :
			CookOptions(ECookByTheBookOptions::None),
			DLCName(FString()),
			ChildCookIdentifier(-1),
			bGenerateStreamingInstallManifests(false),
			bGenerateDependenciesForMaps(false),
			bErrorOnEngineContentUse(false),
			NumProcesses(0)
		{ }
	};

	/**
	* Start a cook by the book session
	* Cook on the fly can't run at the same time as cook by the book
	*/
	void StartCookByTheBook( const FCookByTheBookStartupOptions& CookByTheBookStartupOptions );

	/**
	* ValidateCookByTheBookSettings
	* look at the cookByTheBookOptions and ensure there isn't any conflicting settings
	*/
	void ValidateCookByTheBookSettings() const;

	/**
	* ValidateCookOnTheFlySettings
	* look at the initialization flags and other cooker settings make sure the programmer that thought of checking them are ok
	*/
	void ValidateCookOnTheFlySettings() const;

	/**
	* Queue a cook by the book cancel (you might want to do this instead of calling cancel directly so that you don't have to be in the game thread when canceling
	*/
	void QueueCancelCookByTheBook();

	/**
	* Cancel the currently running cook by the book (needs to be called from the game thread)
	*/
	void CancelCookByTheBook();

	bool IsCookByTheBookRunning() const;

	/**
	* Get any packages which are in memory, these were probably required to be loaded because of the current package we are cooking, so we should probably cook them also
	*/
	void GetUnsolicitedPackages(TArray<UPackage*>& PackagesToSave, bool &ContainsFullGCAssetClasses, const TArray<FName>& TargetPlatformNames) const;

	/**
	* PostLoadPackageFixup
	* after a package is loaded we might want to fix up some stuff before it gets saved
	*/
	void PostLoadPackageFixup(UPackage* Package);

	/**
	* Handles cook package requests until there are no more requests, then returns
	*
	* @param  CookFlags output of the things that might have happened in the cook on the side
	* 
	* @return returns ECookOnTheSideResult
	*/
	uint32 TickCookOnTheSide( const float TimeSlice, uint32 &CookedPackagesCount, ECookTickFlags TickFlags = ECookTickFlags::None );

	/**
	* Clear all the previously cooked data all cook requests from now on will be considered recook requests
	*/
	void ClearAllCookedData();


	/**
	* Clear any cached cooked platform data for a platform
	*  call ClearCachedCookedPlatformData on all Uobjects
	* @param PlatformName platform to clear all the cached data for
	*/
	void ClearCachedCookedPlatformDataForPlatform( const FName& PlatformName );


	/**
	* Clear all the previously cooked data for the platform passed in 
	* 
	* @param name of the platform to clear the cooked packages for
	*/
	void ClearPlatformCookedData( const FName& PlatformName );


	/**
	* Recompile any global shader changes 
	* if any are detected then clear the cooked platform data so that they can be rebuilt
	* 
	* @return return true if shaders were recompiled
	*/
	bool RecompileChangedShaders(const TArray<FName>& TargetPlatforms);


	/**
	* Force stop whatever pending cook requests are going on and clear all the cooked data
	* Note cook on the side / cook on the fly clients may not be able to recover from this if they are waiting on a cook request to complete
	*/
	void StopAndClearCookedData();

	/** 
	* Process any shader recompile requests
	*/
	void TickRecompileShaderRequests();

	bool HasCookRequests() const { return CookRequests.HasItems(); }

	bool HasRecompileShaderRequests() const { return RecompileRequests.HasItems(); }

	uint32 NumConnections() const;

	/**
	* Is this cooker running in the editor
	*
	* @return true if we are running in the editor
	*/
	bool IsCookingInEditor() const;

	/**
	* Is this cooker running in real time mode (where it needs to respect the timeslice) 
	* 
	* @return returns true if this cooker is running in realtime mode like in the editor
	*/
	bool IsRealtimeMode() const;

	/**
	* Helper function returns if we are in any cook by the book mode
	*
	* @return if the cook mode is a cook by the book mode
	*/
	bool IsCookByTheBookMode() const;

	/**
	* Helper function returns if we are in any cook on the fly mode
	*
	* @return if the cook mode is a cook on the fly mode
	*/
	bool IsCookOnTheFlyMode() const;


	virtual void BeginDestroy() override;

	/**
	* SetFullGCAssetClasses FullGCAssetClasses is used to determine when TickCookOnTheSide returns RequiresGC
	*   When one of these classes is saved it will return COSR_RequiresGC
	*/
	void SetFullGCAssetClasses( const TArray<UClass*>& InFullGCAssetClasses );

	/** Returns the configured number of packages to process before GC */
	uint32 GetPackagesPerGC() const;

	/** Returns the configured number of packages to process before partial GC */
	uint32 GetPackagesPerPartialGC() const;

	/** Returns the target max concurrent shader jobs */
	int32 GetMaxConcurrentShaderJobs() const;

	/** Returns the configured amount of idle time before forcing a GC */
	double GetIdleTimeToGC() const;

	/** Returns the configured amount of memory allowed before forcing a GC */
	uint64 GetMaxMemoryAllowance() const;

	/** Mark package as keep around for the cooker (don't GC) */
	void MarkGCPackagesToKeepForCooker();

	bool HasExceededMaxMemory() const;

	/**
	* RequestPackage to be cooked
	*
	* @param StandardPackageFName name of the package in standard format as returned by FPaths::MakeStandardFilename
	* @param TargetPlatforms name of the targetplatforms we want this package cooked for
	* @param bForceFrontOfQueue should we put this package in the front of the cook queue (next to be processed) or at the end
	*/
	bool RequestPackage(const FName& StandardPackageFName, const TArray<FName>& TargetPlatforms, const bool bForceFrontOfQueue);

	/**
	* RequestPackage to be cooked
	* This function can only be called while the cooker is in cook by the book mode
	*
	* @param StandardPackageFName name of the package in standard format as returned by FPaths::MakeStandardFilename
	* @param bForceFrontOfQueue should we put this package in the front of the cook queue (next to be processed) or at the end
	*/
	bool RequestPackage(const FName& StandardPackageFName, const bool bForceFrontOfQueue);


	/**
	* Callbacks from editor 
	*/

	void OnObjectModified( UObject *ObjectMoving );
	void OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);
	void OnObjectUpdated( UObject *Object );
	void OnObjectSaved( UObject *ObjectSaved );

	/**
	* Marks a package as dirty for cook
	* causes package to be recooked on next request (and all dependent packages which are currently cooked)
	*/
	void MarkPackageDirtyForCooker( UPackage *Package );

	/**
	* MaybeMarkPackageAsAlreadyLoaded
	* Mark the package as already loaded if we have already cooked the package for all requested target platforms
	* this hints to the objects on load that we don't need to load all our bulk data
	* 
	* @param Package to mark as not requiring reload
	*/
	void MaybeMarkPackageAsAlreadyLoaded(UPackage* Package);

	/**
	* Callbacks from UObject globals
	*/
	void PreGarbageCollect();

private:

	//////////////////////////////////////////////////////////////////////////
	// cook by the book specific functions
	/**
	* Collect all the files which need to be cooked for a cook by the book session
	*/
	void CollectFilesToCook(TArray<FName>& FilesInPath, 
		const TArray<FString>& CookMaps, const TArray<FString>& CookDirectories, const TArray<FString>& CookCultures, 
		const TArray<FString>& IniMapSections, ECookByTheBookOptions FilesToCookFlags);

	/**
	* AddFileToCook add file to cook list 
	*/
	void AddFileToCook( TArray<FName>& InOutFilesToCook, const FString &InFilename ) const;

	/**
	* Call back from the TickCookOnTheSide when a cook by the book finishes (when started form StartCookByTheBook)
	*/
	void CookByTheBookFinished();

	/**
	* StartChildCookers to help out with cooking
	* only valid in cook by the book (not from the editor)
	* 
	* @param NumChildCookersToSpawn, number of child cookers we want to use
	*/
	void StartChildCookers(int32 NumChildCookersToSpawn, const TArray<FName>& TargetPlatformNames, const FString& ExtraCmdParams = FString());

	/**
	* TickChildCookers
	* output the information form the child cookers to the main cooker output
	* 
	* @return return true if all child cookers are finished
	*/
	bool TickChildCookers();

	/**
	* CleanUPChildCookers
	* can only be called after TickChildCookers returns true
	*/
	void CleanUpChildCookers();

	/**
	* Get all the packages which are listed in asset registry passed in.  
	*
	* @param AssetRegistryPath path of the assetregistry.bin file to read
	* @param OutPackageNames out list of uncooked package filenames which were contained in the asset registry file
	* @return true if successfully read false otherwise
	*/
	bool GetAllPackageFilenamesFromAssetRegistry( const FString& AssetRegistryPath, TArray<FName>& OutPackageFilenames ) const;

	/**
	* BuildMapDependencyGraph
	* builds a map of dependencies from maps
	* 
	* @param PlatformName name of the platform we want to build a map for
	*/
	void BuildMapDependencyGraph(const FName& PlatformName);

	/**
	* WriteMapDependencyGraph
	* write a previously built map dependency graph out to the sandbox directory for a platform
	*
	* @param PlatformName name of the platform we want to save out the dependency graph for
	*/
	void WriteMapDependencyGraph(const FName& PlatformName);

	/**
	* IsChildCooker, 
	* returns if this cooker is a sue chef for some other master chef.
	*/
	bool IsChildCooker() const
	{
		if (IsCookByTheBookMode())
		{
			return !(CookByTheBookOptions->ChildCookFilename.IsEmpty());
		}
		return false;
	}


	//////////////////////////////////////////////////////////////////////////
	// cook on the fly specific functions

	/**
	 * When we get a new connection from the network make sure the version is compatible 
	 *		Will terminate the connection if return false
	 * 
	 * @return return false if not compatible, true if it is
	 */
	bool HandleNetworkFileServerNewConnection( const FString& VersionInfo, const FString& PlatformName );

	void GetCookOnTheFlyUnsolicitedFiles(const FName& PlatformName, TArray<FString> UnsolicitedFiles, const FString& Filename);

	/**
	* Cook requests for a package from network
	*  blocks until cook is complete
	* 
	* @param  Filename	requested file to cook
	* @param  PlatformName platform to cook for
	* @param  out UnsolicitedFiles return a list of files which were cooked as a result of cooking the requested package
	*/
	void HandleNetworkFileServerFileRequest( const FString& Filename, const FString& PlatformName, TArray<FString>& UnsolicitedFiles );

	/**
	* Shader recompile request from network
	*  blocks until shader recompile complete
	*
	* @param  RecompileData input params for shader compile and compiled shader output
	*/
	void HandleNetworkFileServerRecompileShaders(const struct FShaderRecompileData& RecompileData);

	/**
	 * Get the sandbox path we want the network file server to use
	 */
	FString HandleNetworkGetSandboxPath();

	void GetCookOnTheFlyUnsolicitedFiles( const FName& PlatformName, TArray<FString>& UnsolicitedFiles );

	/**
	 * HandleNetworkGetPrecookedList 
	 * this is used specifically for cook on the fly with shared cooked builds
	 * returns the list of files which are still valid in the pak file which was initially loaded
	 * 
	 * @param PrecookedFileList all the files which are still valid in the client pak file
	 */
	void HandleNetworkGetPrecookedList( const FString& PlatformName, TMap<FString, FDateTime>& PrecookedFileList );

	//////////////////////////////////////////////////////////////////////////
	// general functions

	/**
	* Determines if a package should be cooked
	* 
	* @param InFileName		package file name
	* @param InPlatformName	desired platform to cook for
	* @return If the package should be cooked
	*/
	bool ShouldCook(const FString& InFileName, const FName& InPlatformName);

	/**
	 * Tries to save all the UPackages in the PackagesToSave list
	 *  uses the timer to time slice, any packages not saved are requeued in the CookRequests list
	 *  internal function should not be used externally Call Tick / RequestPackage to initiate
	 * 
	 * @param PackagesToSave packages requested save
	 * @param TargetPlatformNames list of target platforms names that we want to save this package for
	 * @param TargetPlatformsToCache list of target platforms that we want to cache uobjects for, might not be the same as the list of packages to save
	 * @param Timer FCookerTimer struct which defines the timeslicing behavior 
	 * @param FirstUnsolicitedPackage first package which was not actually requested, unsolicited packages are prioritized differently, they are saved the same way
	 * @param Result (in+out) used to modify the result of the operation and add any relevant flags
	 * @return returns true if we saved all the packages false if we bailed early for any reason
	 */
	bool SaveCookedPackages(TArray<UPackage*>& PackagesToSave, const TArray<FName>& TargetPlatformNames, const TArray<const ITargetPlatform*>& TargetPlatformsToCache, struct FCookerTimer& Timer, int32 FirstUnsolicitedPackage, uint32& CookedPackageCount, uint32& Result);

	/**
	 * Returns all packages which are found in memory which aren't cooked
	 * @param PackagesToSave (in+out) filled with all packages in memory
	 * @param TargetPlatformNames list of target platforms to find unsolicited packages for 
	 * @param ContainsFullAssetGCClasses do these packages contain any of the assets which require a GC after cooking 
	 *				(this is mostly historical for when objects like UWorld were global, almost nothing should require a GC to work correctly after being cooked anymore).
	 */
	void GetAllUnsolicitedPackages(TArray<UPackage*>& PackagesToSave, const TArray<FName>& TargetPlatformNames, bool& ContainsFullAssetGCClasses);


	/**
	 * Loads a package and prepares it for cooking
	 *  this is the same as a normal load but also ensures that the sublevels are loaded if they are streaming sublevels
	 *
	 * @param BuildFilename long package name of the package to load 
	 * @return UPackage of the package loaded, null if the file didn't exist or other failure
	 */
	UPackage* LoadPackageForCooking(const FString& BuildFilename);

	/**
	* Makes sure a package is fully loaded before we save it out
	* returns true if it succeeded
	*/
	bool MakePackageFullyLoaded(UPackage* Package) const;

	/**
	* Initialize the sandbox 
	*/
	void InitializeSandbox();

	/**
	* Clean up the sandbox
	*/
	void TermSandbox();

	/**
	* GetDependencies
	* 
	* @param Packages List of packages to use as the root set for dependency checking
	* @param Found return value, all objects which package is dependent on
	*/
	void GetDependencies( const TSet<UPackage*>& Packages, TSet<UObject*>& Found);


	/**
	* GetDependencies
	* get package dependencies according to the asset registry
	* 
	* @param Packages List of packages to use as the root set for dependency checking
	* @param Found return value, all objects which package is dependent on
	*/
	void GetDependentPackages( const TSet<UPackage*>& Packages, TSet<FName>& Found);

	/**
	* GetDependencies
	* get package dependencies according to the asset registry
	*
	* @param Root set of packages to use when looking for dependencies
	* @param FoundPackages list of packages which were found
	*/
	void GetDependentPackages(const TSet<FName>& RootPackages, TSet<FName>& FoundPackages);

	/**
	* ContainsWorld
	* use the asset registry to determine if a Package contains a UWorld or ULevel object
	* 
	* @param PackageName to return if it contains the a UWorld object or a ULevel
	* @return true if the Package contains a UWorld or ULevel false otherwise
	*/
	bool ContainsMap(const FName& PackageName) const;


	/** 
	 * Returns true if this package contains a redirector, and fills in paths
	 *
	 * @param PackageName to return if it contains a redirector
	 * @param RedirectedPaths map of original to redirected object paths
	 * @return true if the Package contains a redirector false otherwise
	 */
	bool ContainsRedirector(const FName& PackageName, TMap<FName, FName>& RedirectedPaths) const;
	
	/**
	 * Calls BeginCacheForCookedPlatformData on all UObjects in the package
	 *
	 * @param Package the package used to gather all uobjects from
	 * @param TargetPlatforms target platforms to cache for
	 * @return false if time slice was reached, true if all objects have had BeginCacheForCookedPlatformData called
	 */
	bool BeginPackageCacheForCookedPlatformData(UPackage* Package, const TArray<const ITargetPlatform*>& TargetPlatforms, struct FCookerTimer& Timer) const;
	
	/**
	 * Returns true when all objects in package have all their cooked platform data loaded
	 *	confirms that BeginCacheForCookedPlatformData is called and will return true after all objects return IsCachedCookedPlatformDataLoaded true
	 *
	 * @param Package the package used to gather all uobjects from
	 * @param TargetPlatforms target platforms to cache for
	 * @return false if time slice was reached, true if all return true for IsCachedCookedPlatformDataLoaded 
	 */
	bool FinishPackageCacheForCookedPlatformData(UPackage* Package, const TArray<const ITargetPlatform*>& TargetPlatforms, struct FCookerTimer& Timer) const;

	/**
	* GetCurrentIniVersionStrings gets the current ini version strings for compare against previous cook
	* 
	* @param IniVersionStrings return list of the important current ini version strings
	* @return false if function fails (should assume all platforms are out of date)
	*/
	bool GetCurrentIniVersionStrings( const ITargetPlatform* TargetPlatform, FIniSettingContainer& IniVersionStrings ) const;

	/**
	* GetCookedIniVersionStrings gets the ini version strings used in previous cook for specified target platform
	* 
	* @param IniVersionStrings return list of the previous cooks ini version strings
	* @return false if function fails to find the ini version strings
	*/
	bool GetCookedIniVersionStrings( const ITargetPlatform* TargetPlatform, FIniSettingContainer& IniVersionStrings, TMap<FString, FString>& AdditionalStrings ) const;


	/**
	* Convert a path to a full sandbox path
	* is effected by the cooking dlc settings
	* This function should be used instead of calling the FSandbox Sandbox->ConvertToSandboxPath functions
	*/
	FString ConvertToFullSandboxPath( const FString &FileName, bool bForWrite = false ) const;
	FString ConvertToFullSandboxPath( const FString &FileName, bool bForWrite, const FString& PlatformName ) const;

	/**
	* GetSandboxAssetRegistryFilename
	* 
	* return full path of the asset registry in the sandbox
	*/
	const FString GetSandboxAssetRegistryFilename();

	const FString GetCookedAssetRegistryFilename(const FString& PlatformName);

	/**
	* Get the sandbox root directory for that platform
	* is effected by the CookingDlc settings
	* This should be used instead of calling the Sandbox function
	*/
	FString GetSandboxDirectory( const FString& PlatformName ) const;

	inline bool IsCookingDLC() const
	{
		// can only cook dlc in cook by the book
		// we are cooking dlc when the dlc name is setup
		if ( CookByTheBookOptions )
		{
			return  !CookByTheBookOptions->DlcName.IsEmpty();
		}
		return false;
	}

	/**
	* GetBaseDirectoryForDLC
	* 
	* @return return the path to the DLC
	*/
	FString GetBaseDirectoryForDLC() const;

	inline bool IsCreatingReleaseVersion()
	{
		if ( CookByTheBookOptions )
		{
			return !CookByTheBookOptions->CreateReleaseVersion.IsEmpty();
		}
		return false;
	}

	/**
	* Loads the cooked ini version settings maps into the Ini settings cache
	* 
	* @param TargetPlatforms to look for ini settings for
	*/
	//bool CacheIniVersionStringsMap( const ITargetPlatform* TargetPlatform ) const;

	/**
	* Checks if important ini settings have changed since last cook for each target platform 
	* 
	* @param TargetPlatforms to check if out of date
	* @param OutOfDateTargetPlatforms return list of out of date target platforms which should be cleaned
	*/
	bool IniSettingsOutOfDate( const ITargetPlatform* TargetPlatform ) const;

	/**
	* Saves ini settings which are in the memory cache to the hard drive in ini files
	*
	* @param TargetPlatforms to save
	*/
	bool SaveCurrentIniSettings( const ITargetPlatform* TargetPlatform ) const;



	/**
	* IsCookFlagSet
	* 
	* @param InCookFlag used to check against the current cook flags
	* @return true if the cook flag is set false otherwise
	*/
	bool IsCookFlagSet( const ECookInitializationFlags& InCookFlags ) const 
	{
		return (CookFlags & InCookFlags) != ECookInitializationFlags::None;
	}

	/** If true, the maximum file length of a package being saved will be reduced by 32 to compensate for compressed package intermediate files */
	bool ShouldConsiderCompressedPackageFileLengthRequirements() const;

	/**
	*	Cook (save) the given package
	*
	*	@param	Package				The package to cook/save
	*	@param	SaveFlags			The flags to pass to the SavePackage function
	*	@param	bOutWasUpToDate		Upon return, if true then the cooked package was cached (up to date)
	*
	*	@return	ESavePackageResult::Success if packages was cooked
	*/
	void SaveCookedPackage(UPackage* Package, uint32 SaveFlags, TArray<FSavePackageResultStruct>& SavePackageResults);
	/**
	*	Cook (save) the given package
	*
	*	@param	Package				The package to cook/save
	*	@param	SaveFlags			The flags to pass to the SavePackage function
	*	@param	bOutWasUpToDate		Upon return, if true then the cooked package was cached (up to date)
	*	@param  TargetPlatformNames Only cook for target platforms which are included in this array (if empty cook for all target platforms specified on commandline options)
	*									TargetPlatformNames is in and out value returns the platforms which the SaveCookedPackage function saved for
	*
	*	@return	ESavePackageResult::Success if packages was cooked
	*/
	void SaveCookedPackage(UPackage* Package, uint32 SaveFlags, TArray<FName> &TargetPlatformNames, TArray<FSavePackageResultStruct>& SavePackageResults);


	/**
	*  Save the global shader map
	*  
	*  @param	Platforms		List of platforms to make global shader maps for
	*/
	void SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms);


	/** Create sandbox file in directory using current settings supplied */
	void CreateSandboxFile();
	/** Gets the output directory respecting any command line overrides */
	FString GetOutputDirectoryOverride() const;

	/** Cleans sandbox folders for all target platforms */
	void CleanSandbox( const bool bIterative );

	/**
	* Populate the cooked packages list from the on disk content using time stamps and dependencies to figure out if they are ok
	* delete any local content which is out of date
	* 
	* @param Platforms to process
	*/
	void PopulateCookedPackagesFromDisk( const TArray<ITargetPlatform*>& Platforms );

	/**
	* Searches the disk for all the cooked files in the sandbox path provided
	* Returns a map of the uncooked file path matches to the cooked file path for each package which exists
	*
	* @param UncookedpathToCookedPath out Map of the uncooked path matched with the cooked package which exists
	* @param SandboxPath path to search for cooked packages in
	*/
	void GetAllCookedFiles(TMap<FName, FName>& UncookedPathToCookedPath, const FString& SandboxPath);

	/** Generates asset registry */
	void GenerateAssetRegistry();

	/** Generates long package names for all files to be cooked */
	void GenerateLongPackageNames(TArray<FName>& FilesInPath);


	void GetDependencies( UPackage* Package, TArray<UPackage*> Dependencies );

};

FORCEINLINE uint32 GetTypeHash(const UCookOnTheFlyServer::FFilePlatformRequest &Key)
{
	uint32 Hash = GetTypeHash( Key.Filename );
	for ( const FName& PlatformName : Key.PlatformNames )
	{
		Hash += Hash << 2 ^ GetTypeHash( PlatformName );
	}
	return Hash;
}
