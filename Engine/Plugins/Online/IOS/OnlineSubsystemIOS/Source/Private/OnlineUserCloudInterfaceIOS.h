// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineUserCloudInterface.h"

#ifdef __IPHONE_8_0
#import <CloudKit/CloudKit.h>

@class IOSCloudStorage;

@interface IOSCloudStorage : NSObject

@property(retain) CKContainer* CloudContainer;
@property(retain) CKDatabase* SharedDatabase;
@property(retain) CKDatabase* UserDatabase;
@property(retain) id iCloudToken;

-(IOSCloudStorage*)init:(bool)registerHandler;
-(bool)readFile:(NSString*)fileName sharedDB:(bool)shared completionHandler:(void(^)(CKRecord* record, NSError* error))handler;
-(bool)writeFile:(NSString*)fileName contents:(NSData*)fileContents sharedDB:(bool)shared completionHandler:(void(^)(CKRecord* record, NSError* error))handler;
-(bool)deleteFile:(NSString*)fileName sharedDB:(bool)shared completionHandler:(void(^)(CKRecordID* record, NSError* error))handler;
-(bool)query:(bool)shared fetchHandler : (void(^)(CKRecord* record))fetch completionHandler : (void(^)(CKQueryCursor* record, NSError* error))complete;

-(void)iCloudAccountAvailabilityChanged:(NSNotification*)notification;

+(IOSCloudStorage*)cloudStorage;

@end
#endif

/**
*	FOnlineUserCloudInterfaceIOS - Implementation of user cloud storage for IOS
*/
class FOnlineUserCloudInterfaceIOS : public IOnlineUserCloud
{
protected:

	FCloudFile* GetCloudFile(const FString& FileName, bool bCreateIfMissing = false);
    FCloudFileHeader* GetCloudFileHeader(const FString& FileName, bool bCreateIfMissing = false);
	bool ClearFiles();
	bool ClearCloudFile(const FString& FileName);

public:
    FOnlineUserCloudInterfaceIOS(){ MetaDataState = EOnlineAsyncTaskState::Done; }
	virtual ~FOnlineUserCloudInterfaceIOS();

	// IOnlineUserCloud
	virtual bool GetFileContents(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) override;
	virtual bool ClearFiles(const FUniqueNetId& UserId) override;
	virtual bool ClearFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual void EnumerateUserFiles(const FUniqueNetId& UserId) override;
	virtual void GetUserFileList(const FUniqueNetId& UserId, TArray<FCloudFileHeader>& UserFiles) override;
	virtual bool ReadUserFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual bool WriteUserFile(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) override;
	virtual void CancelWriteUserFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual bool DeleteUserFile(const FUniqueNetId& UserId, const FString& FileName, bool bShouldCloudDelete, bool bShouldLocallyDelete) override;
	virtual bool RequestUsageInfo(const FUniqueNetId& UserId) override;
	virtual void DumpCloudState(const FUniqueNetId& UserId) override;
	virtual void DumpCloudFileState(const FUniqueNetId& UserId, const FString& FileName) override;

private:
	/** File metadata */
	TArray<struct FCloudFileHeader> CloudMetaData;
    /** File metadata query state */
    EOnlineAsyncTaskState::Type MetaDataState;
	/** File cache */
	TArray<struct FCloudFile> CloudFileData;

    /** Critical section for thread safe operation on cloud files */
    FCriticalSection CloudDataLock;
};

typedef TSharedPtr<FOnlineUserCloudInterfaceIOS, ESPMode::ThreadSafe> FOnlineUserCloudIOSPtr;
