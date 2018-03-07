// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemOculus.h"
#include "OnlineUserCloudInterface.h"
#include "OnlineSubsystemOculusTypes.h"
#include "OnlineSubsystemOculusPackage.h"

#if USING_CODE_ANALYSIS
#pragma warning( push )
#pragma warning( disable : ALL_CODE_ANALYSIS_WARNINGS )
#endif	// USING_CODE_ANALYSIS
#include <atomic>
#if USING_CODE_ANALYSIS
#pragma warning( pop )
#endif	// USING_CODE_ANALYSIS

class FOnlineUserCloudOculus : public IOnlineUserCloud
{
public:
	FOnlineUserCloudOculus(FOnlineSubsystemOculus& InSubsystem);
	virtual ~FOnlineUserCloudOculus() = default;

	virtual void EnumerateUserFiles(const FUniqueNetId& UserId) override;

	virtual void GetUserFileList(const FUniqueNetId& UserId, TArray<FCloudFileHeader>& UserFiles) override;

	virtual bool ReadUserFile(const FUniqueNetId& UserId, const FString& FileName) override;

	virtual bool GetFileContents(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) override;

	virtual bool ClearFiles(const FUniqueNetId& UserId) override;

	virtual bool ClearFile(const FUniqueNetId& UserId, const FString& FileName) override;

	virtual bool WriteUserFile(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) override;

	virtual void CancelWriteUserFile(const FUniqueNetId& UserId, const FString& FileName) override;

	virtual bool DeleteUserFile(const FUniqueNetId& UserId, const FString& FileName, bool bShouldCloudDelete, bool bShouldLocallyDelete) override;

	virtual bool RequestUsageInfo(const FUniqueNetId& UserId) override;

	virtual void DumpCloudState(const FUniqueNetId& UserId) override;

	virtual void DumpCloudFileState(const FUniqueNetId& UserId, const FString& FileName) override;

private:

	/** Separator used to parse the BucketName and Key from a FileName parameter */
	static const FString SEPARATOR;

	/** Tag used to find the default bucket name in DefaultEngine.ini */
	static const FString DEFAULT_BUCKET_KEY;

	/** Tag used to find the all bucket names in DefaultEngine.ini */
	static const FString ALL_BUCKETS_KEY;

	FOnlineSubsystemOculus& OculusSubsystem;

	/** Default Bucket to store saves if none is specified from FileName - set in DefaultEngine.ini */
	FString DefaultBucket;

	/** All Buckets defined in DefaultEngine.ini */
	TArray<FString> Buckets;

	/** caches results from ReadUserFile until GetFileContents is called */
	TMap<FString, TArray<uint8>> ReadCache;

private:
	/** Helper method to generate requests looping over all the buckets and pages in a bucket*/
	void RequestEnumeratePagedBuckets(TSharedPtr<const FUniqueNetId> UserId, ovrCloudStorageMetadataArrayHandle previousPage);

	/** caches results from RequestEnumeratePagedBuckets until GetUserFileList is called */
	TArray<FCloudFileHeader> EnumerateCache;

	/** Used by EnumerateUserFiles/RequestEnumeratePagedBuckets to keep track of current bucket position */
	std::atomic<int> EnumerateBucketsCounter;
};