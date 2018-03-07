// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformMemory.h"
#include "Containers/UnrealString.h"

/** 
 * Symbol information associated with a program counter. 
 * FString version.
 * To be used by external tools.
 */
struct CORE_API FProgramCounterSymbolInfoEx
{
	/** Module name. */
	FString	ModuleName;

	/** Function name. */
	FString	FunctionName;

	/** Filename. */
	FString	Filename;

	/** Line number in file. */
	uint32	LineNumber;

	/** Symbol displacement of address.	*/
	uint64	SymbolDisplacement;

	/** Program counter offset into module. */
	uint64	OffsetInModule;

	/** Program counter. */
	uint64	ProgramCounter;

	/** Default constructor. */
	FProgramCounterSymbolInfoEx( FString InModuleName, FString InFunctionName, FString InFilename, uint32 InLineNumber, uint64 InSymbolDisplacement, uint64 InOffsetInModule, uint64 InProgramCounter );
};


/** Enumerates crash description versions. */
enum class ECrashDescVersions : int32
{
	/** Introduces a new crash description format. */
	VER_1_NewCrashFormat,

	/** Added misc properties (CPU,GPU,OS,etc), memory related stats and platform specific properties as generic payload. */
	VER_2_AddedNewProperties,

	/** Using crash context when available. */
	VER_3_CrashContext = 3,
};

/** Enumerates crash dump modes. */
enum class ECrashDumpMode : int32
{
	/** Default minidump settings. */
	Default = 0,

	/** Full memory crash minidump */
	FullDump = 1,

	/** Full memory crash minidump, even on ensures */
	FullDumpAlways = 2,
};

/**
 *	Contains a runtime crash's properties that are common for all platforms.
 *	This may change in the future.
 */
struct CORE_API FGenericCrashContext
{
public:

	/**
	* We can't gather memory stats in crash handling function, so we gather them just before raising
	* exception and use in crash reporting.
	*/
	static FPlatformMemoryStats CrashMemoryStats;
	
	static const ANSICHAR* CrashContextRuntimeXMLNameA;
	static const TCHAR* CrashContextRuntimeXMLNameW;

	static const ANSICHAR* CrashConfigFileNameA;
	static const TCHAR* CrashConfigFileNameW;
	static const FString CrashConfigExtension;
	static const FString ConfigSectionName;
	static const FString CrashConfigPurgeDays;
	static const FString CrashGUIDRootPrefix;

	static const FString CrashContextExtension;
	static const FString RuntimePropertiesTag;
	static const FString PlatformPropertiesTag;
	static const FString EnabledPluginsTag;
	static const FString UE4MinidumpName;
	static const FString NewLineTag;
	static const int32 CrashGUIDLength = 128;

	static const FString CrashTypeCrash;
	static const FString CrashTypeAssert;
	static const FString CrashTypeEnsure;
	static const FString CrashTypeGPU;

	static const FString EngineModeExUnknown;
	static const FString EngineModeExDirty;
	static const FString EngineModeExVanilla;

	/** Initializes crash context related platform specific data that can be impossible to obtain after a crash. */
	static void Initialize();

	/**
	 * @return true, if the generic crash context has been initialized.
	 */
	static bool IsInitalized()
	{
		return bIsInitialized;
	}

	/** Default constructor. */
	FGenericCrashContext();

	virtual ~FGenericCrashContext() { }

	/** Serializes all data to the buffer. */
	void SerializeContentToBuffer();

	/**
	 * @return the buffer containing serialized data.
	 */
	const FString& GetBuffer() const
	{
		return CommonBuffer;
	}

	/**
	 * @return a globally unique crash name.
	 */
	void GetUniqueCrashName(TCHAR* GUIDBuffer, int32 BufferSize) const;

	/**
	 * @return whether this crash is a full memory minidump
	 */
	const bool IsFullCrashDump() const;

	/**
	 * @return whether this crash is a full memory minidump if the crash context is for an ensure
	 */
	const bool IsFullCrashDumpOnEnsure() const;

	/** Serializes crash's informations to the specified filename. Should be overridden for platforms where using FFileHelper is not safe, all POSIX platforms. */
	virtual void SerializeAsXML( const TCHAR* Filename );

	/** Writes a common property to the buffer. */
	void AddCrashProperty( const TCHAR* PropertyName, const TCHAR* PropertyValue );

	/** Writes a common property to the buffer. */
	template <typename Type>
	void AddCrashProperty( const TCHAR* PropertyName, const Type& Value )
	{
		AddCrashProperty( PropertyName, *TTypeToString<Type>::ToString( Value ) );
	}

	/** Escapes a specified XML string, naive implementation. */
	static FString EscapeXMLString( const FString& Text );

	/** Unescapes a specified XML string, naive implementation. */
	static FString UnescapeXMLString( const FString& Text );

	/** Helper to get the standard string for the crash type based on crash event bool values. */
	static const TCHAR* GetCrashTypeString(bool InIsEnsure, bool InIsAssert, bool bIsGPUCrashed);

	/** Gets the "vanilla" status string. */
	static const TCHAR* EngineModeExString();

	/** Helper to get the crash report client config filepath saved by this instance and copied to each crash report folder. */
	static const TCHAR* GetCrashConfigFilePath();

	/** Helper to get the crash report client config folder used by GetCrashConfigFilePath(). */
	static const TCHAR* GetCrashConfigFolder();

	/** Helper to clean out old files in the crash report client config folder. */
	static void PurgeOldCrashConfig();

	/** Adds a plugin descriptor string to the enabled plugins list in the crash context */
	static void AddPlugin(const FString& PluginDesc);

	/**
	 * @return whether this crash is a non-crash event
	 */
	bool GetIsEnsure() const { return bIsEnsure; }

protected:
	bool bIsEnsure;

private:
	/** Serializes platform specific properties to the buffer. */
	virtual void AddPlatformSpecificProperties();

	/** Writes header information to the buffer. */
	void AddHeader();

	/** Writes footer to the buffer. */
	void AddFooter();

	void BeginSection( const TCHAR* SectionName );
	void EndSection( const TCHAR* SectionName );

	/** Called once when GConfig is initialized. Opportunity to cache values from config. */
	static void InitializeFromConfig();

	/**	Whether the Initialize() has been called */
	static bool bIsInitialized;

	/**	Static counter records how many crash contexts have been constructed */
	static int32 StaticCrashContextIndex;

	/** The buffer used to store the crash's properties. */
	FString CommonBuffer;

	/**	Records which crash context we were using the StaticCrashContextIndex counter */
	int32 CrashContextIndex;

	// FNoncopyable
	FGenericCrashContext( const FGenericCrashContext& );
	FGenericCrashContext& operator=(const FGenericCrashContext&);
};

struct CORE_API FGenericMemoryWarningContext
{};
