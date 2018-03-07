// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class INetworkFileServer;

/**
 * Delegate type for handling file requests from a network client.
 *
 * The first parameter is the name of the requested file.
 * The second parameter will hold the list of unsolicited files to send back.
 */
DECLARE_DELEGATE_ThreeParams(FFileRequestDelegate, const FString&, const FString&, TArray<FString>&);

struct FShaderRecompileData
{
	FString PlatformName;
	/** The platform to compile shaders for, corresponds to EShaderPlatform, but a value of -1 indicates to compile for all target shader platforms. */
	int32 ShaderPlatform;
	TArray<FString>* ModifiedFiles;
	TArray<uint8>* MeshMaterialMaps;
	TArray<FString> MaterialsToLoad;
	TArray<uint8> SerializedShaderResources;
	bool bCompileChangedShaders;

	FShaderRecompileData() :
		ShaderPlatform(-1),
		bCompileChangedShaders(true)
	{}

	FShaderRecompileData& operator=(const FShaderRecompileData& Other)
	{
		PlatformName = Other.PlatformName;
		ShaderPlatform = Other.ShaderPlatform;
		ModifiedFiles = Other.ModifiedFiles;
		MeshMaterialMaps = Other.MeshMaterialMaps;
		MaterialsToLoad = Other.MaterialsToLoad;
		SerializedShaderResources = Other.SerializedShaderResources;
		bCompileChangedShaders = Other.bCompileChangedShaders;

		return *this;
	}
};


/**
 * Delegate type for handling shader recompilation requests from a network client.
 */
DECLARE_DELEGATE_OneParam(FRecompileShadersDelegate, const FShaderRecompileData&);

/**
 * Delegate which returns an override for the sandbox path
 */
DECLARE_DELEGATE_RetVal( FString, FSandboxPathDelegate);

/**
 * Delegate which is called when an outside system modifies a file
 */
DECLARE_MULTICAST_DELEGATE_OneParam( FOnFileModifiedDelegate, const FString& );


/**
 * Delegate which is called when a new connection is made to a file server client
 * 
 * @param 1 Version string
 * @param 2 Platform name
 * @return return false if the connection should be destroyed
 */
DECLARE_DELEGATE_RetVal_TwoParams( bool, FNewConnectionDelegate, const FString&, const FString& );


/**
 * Delegate which returns a list of all the files which should already be deployed to the devkit
 *
 * @param 1 IN, Platform to get precooked file list
 * @param 2 OUT, list of precooked files 
 */
typedef TMap<FString,FDateTime> FFileTimeMap;
DECLARE_DELEGATE_TwoParams( FInitialPrecookedListDelegate, const FString&, FFileTimeMap& );



// container struct for delegates which the network file system uses
struct FNetworkFileDelegateContainer
{
public:
	FNetworkFileDelegateContainer() : 
		NewConnectionDelegate(nullptr), 
		InitialPrecookedListDelegate(nullptr),
		SandboxPathOverrideDelegate(nullptr),
		FileRequestDelegate(nullptr),
		RecompileShadersDelegate(nullptr),
		OnFileModifiedCallback(nullptr)
	{}
	FNewConnectionDelegate NewConnectionDelegate; 
	FInitialPrecookedListDelegate InitialPrecookedListDelegate;
	FSandboxPathDelegate SandboxPathOverrideDelegate;
	FFileRequestDelegate FileRequestDelegate;
	FRecompileShadersDelegate RecompileShadersDelegate;

	FOnFileModifiedDelegate* OnFileModifiedCallback; // this is called from other systems to notify the network file system that a file has been modified hence the terminology callback
};


enum ENetworkFileServerProtocol
{
	NFSP_Tcp,
	NFSP_Http,
};

/**
 * Interface for network file system modules.
 */
class INetworkFileSystemModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a new network file server.
	 *
	 * @param InPort The port number to bind to (-1 = default port, 0 = any available port).
	 * @param Streaming Whether it should be a streaming server.
	 * @param InFileRequestDelegate An optional delegate to be invoked when a file is requested by a client.
	 * @param InRecompileShadersDelegate An optional delegate to be invoked when shaders need to be recompiled.
	 *
	 * @return The new file server, or nullptr if creation failed.
	 */
	virtual INetworkFileServer* CreateNetworkFileServer( bool bLoadTargetPlatforms, int32 Port = -1, FNetworkFileDelegateContainer InNetworkFileDelegateContainer = FNetworkFileDelegateContainer(), const ENetworkFileServerProtocol Protocol = NFSP_Tcp ) const = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~INetworkFileSystemModule( ) { }
};
