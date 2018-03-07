// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "ISourceControlRevision.h"
#include "TextProperty.h"
#include "SourceControlHelpers.generated.h"


class ISourceControlProvider;

/** 
 * Delegate used for performing operation on files that may need a checkout, but before they are added to source control 
 * @param	InDestFile			The filename that was potentially checked out
 * @param	InFileDescription	Description of the file to display to the user, e.g. "Text" or "Image"
 * @param	OutFailReason		Text describing why the operation failed
 * @return true if the operation was successful
 */
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnPostCheckOut, const FString& /*InDestFile*/, const FText& /*InFileDescription*/, FText& /*OutFailReason*/);

// For backwards compatibility
typedef class USourceControlHelpers SourceControlHelpers;

UCLASS(transient)
class USourceControlHelpers : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * Helper function to get the ini filename for storing source control settings
	 * @return the filename
	 */
	static SOURCECONTROL_API const FString& GetSettingsIni();

	/**
	 * Helper function to get the ini filename for storing global source control settings
	 * @return the filename
	 */
	static SOURCECONTROL_API const FString& GetGlobalSettingsIni();

	/**
	 * Helper function to get a filename for a package name.
	 * @param	InPackageName	The package name to get the filename for
	 * @return the filename
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Editor Source Control Helpers")
	static SOURCECONTROL_API FString PackageFilename( const FString& InPackageName );

	/**
	 * Helper function to get a filename for a package.
	 * @param	InPackage	The package to get the filename for
	 * @return the filename
	 */
	static SOURCECONTROL_API FString PackageFilename( const UPackage* InPackage );

	/**
	 * Helper function to convert package array into filename array.
	 * @param	InPackages	The package array
	 * @return an array of filenames
	 */
	static SOURCECONTROL_API TArray<FString> PackageFilenames( const TArray<UPackage*>& InPackages );

	/**
	 * Helper function to convert package name array into a filename array.
	 * @param	InPackageNames	The package name array
	 * @return an array of filenames
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Editor Source Control Helpers")
	static SOURCECONTROL_API TArray<FString> PackageFilenames( const TArray<FString>& InPackageNames );
	
	/**
	 * Helper function to convert a filename array to absolute paths.
	 * @param	InFileNames	The filename array
	 * @return an array of filenames, transformed into absolute paths
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Editor Source Control Helpers")
	static SOURCECONTROL_API TArray<FString> AbsoluteFilenames( const TArray<FString>& InFileNames );

	/**
	 * Helper function to get a list of files that are unchanged & revert them. This runs synchronous commands.
	 * @param	InProvider	The provider to use
	 * @param	InFiles		The files to operate on
	 */
	static SOURCECONTROL_API void RevertUnchangedFiles( ISourceControlProvider& InProvider, const TArray<FString>& InFiles );

	/**
	 * Helper function to annotate a file using a label
	 * @param	InProvider	The provider to use
	 * @param	InLabel		The label to use to retrieve the file
	 * @param	InFile		The file to annotate
	 * @param	OutLines	Output array of annotated lines
	 * @returns true if successful
	 */
	static SOURCECONTROL_API bool AnnotateFile( ISourceControlProvider& InProvider, const FString& InLabel, const FString& InFile, TArray<FAnnotationLine>& OutLines );

	/**
	 * Helper function to annotate a file using a changelist/checkin identifier
	 * @param	InProvider				The provider to use
	 * @param	InCheckInIdentifier		The changelist/check identifier to use to retrieve the file
	 * @param	InFile					The file to annotate
	 * @param	OutLines				Output array of annotated lines
	 * @returns true if successful
	 */
	static SOURCECONTROL_API bool AnnotateFile( ISourceControlProvider& InProvider, int32 InCheckInIdentifier, const FString& InFile, TArray<FAnnotationLine>& OutLines );

	/**
	 * Helper function to check out a file
	 * @param	InFile		The file path to check in
	 * @return	Success or failure of the checkout operation
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Editor Source Control Helpers")
	static SOURCECONTROL_API bool CheckOutFile( const FString& InFile );

	/**
	 * Helper function to mark a file for add. Does nothing (and returns true) if the file is already under SC
	 * @param	InFile		The file path to check in
	 * @return	Success or failure of the mark for add operation
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Editor Source Control Helpers")
	static SOURCECONTROL_API bool MarkFileForAdd( const FString& InFile );

	/**
	 * Helper function perform an operation on files in our 'source controlled' directories, handling checkout/add etc.
	 * @param	InDestFile			The path to the destination file
	 * @param	InFileDescription	Description of the file to display to the user, e.g. "Text" or "Image"
	 * @param	OnPostCheckOut		Delegate used for performing operation on files that may need a checkout, but before they are added to source control 
	 * @param	OutFailReason		Text describing why the operation failed
	 * @return	Success or failure of the operation
	 */
	static SOURCECONTROL_API bool CheckoutOrMarkForAdd( const FString& InDestFile, const FText& InFileDescription, const FOnPostCheckOut& OnPostCheckOut, FText& OutFailReason );

	/**
	 * Helper function to copy a file into our 'source controlled' directories, handling checkout/add etc.
	 * @param	InDestFile			The path to the destination file
	 * @param	InSourceFile		The path to the source file
	 * @param	InFileDescription	Description of the file to display to the user, e.g. "Text" or "Image"
	 * @param	OutFailReason		Text describing why the operation failed
	 * @return	Success or failure of the operation
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Editor Source Control Helpers")
	static SOURCECONTROL_API bool CopyFileUnderSourceControl( const FString& InDestFile, const FString& InSourceFile, const FText& InFileDescription, FText& OutFailReason );

	/**
	 * Helper function to branch/integrate packages from one location to another
	 * @param	DestPackage			The destination package
	 * @param	SourcePackage		The source package
	 * @return true if the file packages were successfully branched.
	 */
	static SOURCECONTROL_API bool BranchPackage( UPackage* DestPackage, UPackage* SourcePackage );
};

/** 
 * Helper class that ensures FSourceControl is properly initialized and shutdown by calling Init/Close in
 * its constructor/destructor respectively.
 */
class SOURCECONTROL_API FScopedSourceControl
{
public:
	/** Constructor; Initializes Source Control Provider */
	FScopedSourceControl();

	/** Destructor; Closes Source Control Provider */
	~FScopedSourceControl();

	/** Get the provider we are using */
	ISourceControlProvider& GetProvider();
};
