
#include "OnlineUserCloudOculus.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "Misc/ConfigCacheIni.h"
#include "OnlineSubsystemOculusTypes.h"
#include "OnlineIdentityOculus.h"
#include "OnlineSubsystemOculusPackage.h"
#if USING_CODE_ANALYSIS
#pragma warning( push )
#pragma warning( disable : ALL_CODE_ANALYSIS_WARNINGS )
#endif	// USING_CODE_ANALYSIS
#include <algorithm>
#if USING_CODE_ANALYSIS
#pragma warning( pop )
#endif	// USING_CODE_ANALYSIS

const FString FOnlineUserCloudOculus::SEPARATOR = TEXT("/");

const FString FOnlineUserCloudOculus::DEFAULT_BUCKET_KEY = TEXT("DefaultUserCloudBucket");

const FString FOnlineUserCloudOculus::ALL_BUCKETS_KEY = TEXT("UserCloudBuckets");

FOnlineUserCloudOculus::FOnlineUserCloudOculus(FOnlineSubsystemOculus& InSubsystem)
	: OculusSubsystem(InSubsystem)
	, EnumerateBucketsCounter(-1)
{
	// see if the game defined a default storage bucket
	GConfig->GetString(TEXT("OnlineSubsystemOculus"), *DEFAULT_BUCKET_KEY, DefaultBucket, GEngineIni);

	// build a list of all the defined buckets
	GConfig->GetArray(TEXT("OnlineSubsystemOculus"), *ALL_BUCKETS_KEY, Buckets, GEngineIni);

	// make sure the default is included in the buckets list
	if (!DefaultBucket.IsEmpty()) 
	{
		Buckets.AddUnique(DefaultBucket);
	}
}

void FOnlineUserCloudOculus::EnumerateUserFiles(const FUniqueNetId& UserId)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || UserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can only enumerate save data for logged in player"));
		TriggerOnEnumerateUserFilesCompleteDelegates(false, *LoggedInPlayerId);
		return;
	}

	if (EnumerateBucketsCounter >= 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("EnumerateUserFiles already in progress."));
		TriggerOnEnumerateUserFilesCompleteDelegates(false, *LoggedInPlayerId);
		return;
	}

	if (Buckets.Num() == 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("No Oculus Cloud Storage buckets were defined in %s."), *GEngineIni);
		TriggerOnEnumerateUserFilesCompleteDelegates(false, *LoggedInPlayerId);
		return;
	}

	EnumerateCache.Reset();
	EnumerateBucketsCounter = 0;
	RequestEnumeratePagedBuckets(LoggedInPlayerId, nullptr);
}

void FOnlineUserCloudOculus::RequestEnumeratePagedBuckets(TSharedPtr<const FUniqueNetId> UserId, ovrCloudStorageMetadataArrayHandle previousPage)
{
	auto Delegate = FOculusMessageOnCompleteDelegate::CreateLambda([this, UserId](ovrMessageHandle Message, bool bIsError)
	{
		const FString& BucketName(Buckets[EnumerateBucketsCounter]);

		if (bIsError)
		{
			// NOTE: this may be too harsh.  depending on the error we might be able to keep going.
			// check when better error handling is enabled in t11136437
			UE_LOG_ONLINE(Warning, TEXT("Failed to Enumerate bucket: %s"), *BucketName);
			EnumerateBucketsCounter = -1;
			EnumerateCache.Reset();
			TriggerOnEnumerateUserFilesCompleteDelegates(false, *UserId);
			return;
		}

		ovrCloudStorageMetadataArrayHandle response = ovr_Message_GetCloudStorageMetadataArray(Message);

		// adds all the keys to the cache in the format: BucketName / Key
		for (size_t i = 0; i < ovr_CloudStorageMetadataArray_GetSize(response); i++) {
			ovrCloudStorageMetadataHandle Metadatum = ovr_CloudStorageMetadataArray_GetElement(response, i);
			FString Key = ovr_CloudStorageMetadata_GetKey(Metadatum);
			FString Filename = FString::Printf(TEXT("%s%s%s"), *BucketName, *SEPARATOR, *Key);
			unsigned int Size = ovr_CloudStorageMetadata_GetDataSize(Metadatum);
			EnumerateCache.Add(FCloudFileHeader("", Filename, Size));
		}

		// First check to see if we need to get another page of entires for this Bucket
		if (ovr_CloudStorageMetadataArray_HasNextPage(response))
		{
			RequestEnumeratePagedBuckets(UserId, response);
		}
		// see if we need to move on to the next bucket
		else if (EnumerateBucketsCounter < (Buckets.Num() - 1))
		{
			EnumerateBucketsCounter++;
			RequestEnumeratePagedBuckets(UserId, nullptr);
		}
		// Otherwise we are done - notify the App
		else
		{
			EnumerateBucketsCounter = -1;
			TriggerOnEnumerateUserFilesCompleteDelegates(true, *UserId);
		}
	});

	if (previousPage)
	{
		OculusSubsystem.AddRequestDelegate(ovr_CloudStorage_GetNextCloudStorageMetadataArrayPage(previousPage), std::move(Delegate));
	}
	else
	{
		OculusSubsystem.AddRequestDelegate(ovr_CloudStorage_LoadBucketMetadata(TCHAR_TO_UTF8(*Buckets[EnumerateBucketsCounter])), std::move(Delegate));
	}
}

void FOnlineUserCloudOculus::GetUserFileList(const FUniqueNetId& UserId, TArray<FCloudFileHeader>& UserFiles)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || UserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can get file list for logged in player"));
		return;
	}

	UserFiles = MoveTemp(EnumerateCache);
}

bool FOnlineUserCloudOculus::WriteUserFile(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || UserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can only save data for logged in player"));
		return false;
	}

	FString BucketName;
	FString Key;
	if (!(FileName.Split(SEPARATOR, &BucketName, &Key)))
	{
		BucketName = DefaultBucket;
		Key = FileName;
	}

	// store the save data in a temporary buffer until the Oculus Platform threadpool can process the request
	TArray<uint8> *TmpBuffer = new TArray<uint8>(MoveTemp(FileContents));

	auto DelegateLambda = FOculusMessageOnCompleteDelegate::CreateLambda([this, BucketName, Key, LoggedInPlayerId, FileName, TmpBuffer](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to Save: %s%s%s"), *BucketName, *SEPARATOR, *Key);
		}
		else
		{
			check(BucketName == UTF8_TO_TCHAR(ovr_CloudStorageUpdateResponse_GetBucket(ovr_Message_GetCloudStorageUpdateResponse(Message))));
			check(Key == UTF8_TO_TCHAR(ovr_CloudStorageUpdateResponse_GetKey(ovr_Message_GetCloudStorageUpdateResponse(Message))));
		}

		delete TmpBuffer;
		TriggerOnWriteUserFileCompleteDelegates(!bIsError, *LoggedInPlayerId, FileName);
	});

	OculusSubsystem.AddRequestDelegate(
		ovr_CloudStorage_Save(TCHAR_TO_UTF8(*BucketName), TCHAR_TO_UTF8(*Key), TmpBuffer->GetData(), TmpBuffer->Num(), 0, nullptr),
		std::move(DelegateLambda));

	return true;
}

bool FOnlineUserCloudOculus::ReadUserFile(const FUniqueNetId& UserId, const FString& FileName)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || UserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can only read data for logged in player"));
		return false;
	}

	FString BucketName;
	FString Key;
	if (!(FileName.Split(SEPARATOR, &BucketName, &Key)))
	{
		BucketName = DefaultBucket;
		Key = FileName;
	}

	OculusSubsystem.AddRequestDelegate(
		ovr_CloudStorage_Load(TCHAR_TO_UTF8(*BucketName), TCHAR_TO_UTF8(*Key)),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, BucketName, Key, LoggedInPlayerId, FileName](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to Load: %s%s%s"), *BucketName, *SEPARATOR, *Key);
		}
		else
		{
			ovrCloudStorageDataHandle response = ovr_Message_GetCloudStorageData(Message);
			check(BucketName == UTF8_TO_TCHAR(ovr_CloudStorageData_GetBucket(response)));
			check(Key == UTF8_TO_TCHAR(ovr_CloudStorageData_GetKey(response)));

			int64 BlobSize = ovr_CloudStorageData_GetDataSize(response);
			const void* RawBlob = ovr_CloudStorageData_GetData(response);

			TArray<uint8> Blob;
			Blob.Insert(static_cast<const uint8 *>(RawBlob), BlobSize, 0);

			ReadCache.Add(FileName, MoveTemp(Blob));
		}
		TriggerOnReadUserFileCompleteDelegates(!bIsError, *LoggedInPlayerId, FileName);
	}));

	return true;
}

bool FOnlineUserCloudOculus::GetFileContents(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || UserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can only read data for logged in player"));
		return false;
	}

	if (!ReadCache.Contains(FileName))
	{
		UE_LOG_ONLINE(Warning, TEXT("No data from ReadUserFile for: %s"), *FileName);
		return false;
	}

	FileContents = ReadCache.FindAndRemoveChecked(FileName);
	return true;
}

bool FOnlineUserCloudOculus::ClearFiles(const FUniqueNetId& UserId)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || UserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can only clear data for logged in player"));
		return false;
	}

	ReadCache.Reset();
	return true;
}

bool FOnlineUserCloudOculus::ClearFile(const FUniqueNetId& UserId, const FString& FileName)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || UserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can only clear data for logged in player"));
		return false;
	}

	if (ReadCache.Contains(FileName))
	{
		ReadCache.Remove(FileName);
		return true;
	}

	return false;
}

void FOnlineUserCloudOculus::CancelWriteUserFile(const FUniqueNetId& UserId, const FString& FileName)
{
	UE_LOG_ONLINE(Warning, TEXT("CancelWriteUserFile not supported by API"));
}

bool FOnlineUserCloudOculus::DeleteUserFile(const FUniqueNetId& UserId, const FString& FileName, bool bShouldCloudDelete, bool bShouldLocallyDelete)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LoggedInPlayerId.IsValid() || UserId != *LoggedInPlayerId)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can only save data for logged in player"));
		return false;
	}

	if (bShouldLocallyDelete && ReadCache.Contains(FileName))
	{
		ReadCache.FindAndRemoveChecked(FileName);
	}

	if (bShouldCloudDelete)
	{
		FString BucketName;
		FString Key;
		if (!(FileName.Split(SEPARATOR, &BucketName, &Key)))
		{
			BucketName = DefaultBucket;
			Key = FileName;
		}

		auto DelegateLambda = FOculusMessageOnCompleteDelegate::CreateLambda([this, BucketName, Key, LoggedInPlayerId, FileName](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				UE_LOG_ONLINE(Warning, TEXT("Failed to Delete: %s%s%s"), *BucketName, *SEPARATOR, *Key);
			}

			TriggerOnDeleteUserFileCompleteDelegates(!bIsError, *LoggedInPlayerId, FileName);
		});

		OculusSubsystem.AddRequestDelegate(
			ovr_CloudStorage_Delete(TCHAR_TO_UTF8(*BucketName), TCHAR_TO_UTF8(*Key)),
			std::move(DelegateLambda));
	}
	else
	{
		TriggerOnDeleteUserFileCompleteDelegates(false, *LoggedInPlayerId, FileName);
	}

	return true;
}

bool FOnlineUserCloudOculus::RequestUsageInfo(const FUniqueNetId& UserId)
{
	UE_LOG_ONLINE(Warning, TEXT("RequestUsageInfo not supported by API"));
	return false;
}

void FOnlineUserCloudOculus::DumpCloudState(const FUniqueNetId& UserId)
{
	UE_LOG_ONLINE(Warning, TEXT("DumpCloudState not supported by API"));
}

void FOnlineUserCloudOculus::DumpCloudFileState(const FUniqueNetId& UserId, const FString& FileName)
{
	UE_LOG_ONLINE(Warning, TEXT("DumpCloudFileState not supported by API"));
}
