// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SurveyTitleCdnStorage.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"

IOnlineTitleFilePtr FSurveyTitleCdnStorage::Create( const FString& IndexUrl )
{
	return MakeShareable( new FSurveyTitleCdnStorage( IndexUrl ) );
}

FSurveyTitleCdnStorage::FSurveyTitleCdnStorage( const FString& InIndexUrl )
	: IndexUrl( InIndexUrl )
{

}

bool FSurveyTitleCdnStorage::GetFileContents(const FString& FileName, TArray<uint8>& FileContents)
{
	const TArray<FCloudFile>* FilesPtr = &Files;
	if (FilesPtr != NULL)
	{
		for (TArray<FCloudFile>::TConstIterator It(*FilesPtr); It; ++It)
		{
			if (It->FileName == FileName)
			{
				FileContents = It->Data;
				return true;
			}
		}
	}
	return false;
}

bool FSurveyTitleCdnStorage::ClearFiles()
{
	for (int Idx=0; Idx < Files.Num(); Idx++)
	{
		if (Files[Idx].AsyncState == EOnlineAsyncTaskState::InProgress)
		{
			UE_LOG(LogEpicSurvey, Verbose, TEXT("Cant clear files. Pending file op for %s"), *Files[Idx].FileName);
			return false;
		}
	}
	// remove all cached file entries
	Files.Empty();
	return true;

}

bool FSurveyTitleCdnStorage::ClearFile(const FString& FileName)
{
	for (int Idx=0; Idx < Files.Num(); Idx++)
	{
		if (Files[Idx].FileName == FileName)
		{
			if (Files[Idx].AsyncState == EOnlineAsyncTaskState::InProgress)
			{
				UE_LOG(LogEpicSurvey, Verbose, TEXT("Cant clear file. Pending file op for %s"), *Files[Idx].FileName);
				return false;
			}
			else
			{
				Files.RemoveAt(Idx);
				return true;
			}
		}
	}
	return false;
}

void FSurveyTitleCdnStorage::DeleteCachedFiles(bool bSkipEnumerated)
{
	// not implemented
}

bool FSurveyTitleCdnStorage::EnumerateFiles(const FPagedQuery& Page)
{
	// Make sure an enumeration request  is not currently pending
	if(!EnumerateFilesRequests.IsEmpty())
	{
		UE_LOG(LogEpicSurvey, Verbose, TEXT("EnumerateFiles request failed. Request already in progress."));
		TriggerOnEnumerateFilesCompleteDelegates(false, TEXT("Request already in progress"));
		return false;
	}

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	EnumerateFilesRequests.Enqueue(HttpRequest);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(AsShared(), &FSurveyTitleCdnStorage::EnumerateFiles_HttpRequestComplete);
	HttpRequest->SetURL( IndexUrl );
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->ProcessRequest();
	return true;
	
}	

void FSurveyTitleCdnStorage::GetFileList(TArray<FCloudFileHeader>& OutFiles) 
{
	TArray<FCloudFileHeader>* FilesPtr = &FileHeaders;
	if (FilesPtr != NULL)
	{
		OutFiles = *FilesPtr;
	}
	else
	{
		OutFiles.Empty();
	}
}

FCloudFile* FSurveyTitleCdnStorage::GetCloudFile(const FString& FileName, bool bCreateIfMissing)
{
	FCloudFile* CloudFile = NULL;
	for (int Idx=0; Idx < Files.Num(); Idx++)
	{
		if (Files[Idx].FileName == FileName)
		{
			CloudFile = &Files[Idx];
			break;
		}
	}
	if (CloudFile == NULL && bCreateIfMissing)
	{
		CloudFile = new(Files) FCloudFile(FileName);
	}
	return CloudFile;
}

FString FSurveyTitleCdnStorage::GetLocalFilePath(const FString& FileName)
{
	return FPaths::CloudDir() +  FPaths::GetCleanFilename(FileName);
}

void FSurveyTitleCdnStorage::SaveCloudFileToDisk(const FString& Filename, const TArray<uint8>& Data)
{
	// save local disk copy as well
	FString LocalFilePath = GetLocalFilePath( Filename );
	bool bSavedLocal = FFileHelper::SaveArrayToFile(Data, *LocalFilePath);
	if (bSavedLocal)
	{
		UE_LOG(LogEpicSurvey, Verbose, TEXT("WriteUserFile request complete. Local file cache updated =%s"), 
			*LocalFilePath);
	}
	else
	{
		UE_LOG(LogEpicSurvey, Verbose, TEXT("WriteUserFile request complete. Local file cache failed to update =%s"), 
			*LocalFilePath);
	}
}

FCloudFileHeader* FSurveyTitleCdnStorage::GetCloudFileHeader(const FString& FileName)
{
	FCloudFileHeader* CloudFileHeader = NULL;

	for (int Idx=0; Idx < FileHeaders.Num(); Idx++)
	{
		if (FileHeaders[Idx].DLName == FileName)
		{
			CloudFileHeader = &FileHeaders[Idx];
			break;
		}
	}
	
	return CloudFileHeader;
}

bool FSurveyTitleCdnStorage::ReadFile(const FString& FileName) 
{
	FString ErrorStr;

	// Make sure valid filename was specified
	if (FileName.IsEmpty() || FileName.Contains(TEXT(" ")))
	{
		ErrorStr = FString::Printf(TEXT("Invalid filename filename=%s"), *FileName);
	}
	
	// Make sure a file request for this file is not currently pending
	for (FFileRequestsMap::TConstIterator It(FileRequests); It; ++It)
	{
		if (It.Value() == FPendingFileRequest(FileName))
		{
			ErrorStr = FString::Printf(TEXT("File request already pending for filename=%s"), *FileName);
			break;
		}
	}
	
	if (!ErrorStr.IsEmpty())
	{
		UE_LOG(LogEpicSurvey, Verbose, TEXT("ReadFile request failed. %s"), *ErrorStr);
		TriggerOnReadFileCompleteDelegates(false, FileName);
		return false;
	}

	// Mark file entry as in progress
	FCloudFile* CloudFile = GetCloudFile(FileName, true);
	CloudFile->AsyncState = EOnlineAsyncTaskState::InProgress;

	
	// load file from disk		
	FString LocalFilePath = GetLocalFilePath(FileName);
	bool bLoadedFile = FFileHelper::LoadFileToArray(CloudFile->Data, *LocalFilePath, FILEREAD_Silent);
	if (bLoadedFile)
	{
		UE_LOG(LogEpicSurvey, Verbose, TEXT("ReadFile request. Local file read from cache =%s"), 
				*LocalFilePath);

		//@todo samz - move reading/hashing of local file to async task

		// verify hash of file if it exists
		FCloudFileHeader* CloudFileHeader = GetCloudFileHeader( FileName);
		if (CloudFileHeader != NULL &&
			!CloudFileHeader->Hash.IsEmpty())
		{
			uint8 LocalHash[20];
			FSHA1::HashBuffer(CloudFile->Data.GetData(), CloudFile->Data.Num(), LocalHash);
			// concatenate 20 bye SHA1 hash to string
			FString LocalHashStr;
			for (int Idx=0; Idx < 20; Idx++)
			{
				LocalHashStr += FString::Printf(TEXT("%02x"),LocalHash[Idx]);
			}
			// if hash matches then just use the local file
			if (CloudFileHeader->Hash == LocalHashStr)
			{
				UE_LOG(LogEpicSurvey, Verbose, TEXT("Local file hash matches cloud header. No need to download for filename=%s"),  *FileName);

				CloudFile->AsyncState = EOnlineAsyncTaskState::Done;
				TriggerOnReadFileCompleteDelegates(true, FileName);
				return true;
			}
		}
	}
	else
	{
		UE_LOG(LogEpicSurvey, Verbose, TEXT("ReadFile request. Local file failed to read from cache =%s"), 
				*LocalFilePath);
	}
	
	// Empty local that was loaded
	CloudFile->Data.Empty();

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	FileRequests.Add(HttpRequest, FPendingFileRequest(FileName));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(AsShared(), &FSurveyTitleCdnStorage::ReadFile_HttpRequestComplete);
	HttpRequest->SetURL( FileName );
	HttpRequest->SetVerb(TEXT("GET"));

	return HttpRequest->ProcessRequest();
}

void FSurveyTitleCdnStorage::EnumerateFiles_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	{
		// Scoped because Request isn't used, if it needs to be used be sure to use Request.Pin() it and check validity
		TWeakPtr<IHttpRequest> Request;
		EnumerateFilesRequests.Dequeue(Request);
	}

	bool bResult = false;
	FString ResponseStr, ErrorStr;

	if (bSucceeded && HttpResponse.IsValid())
	{
		ResponseStr = HttpResponse->GetContentAsString();
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			UE_LOG(LogEpicSurvey, Verbose, TEXT("EnumerateFiles request complete. url=%s code=%d response=%s"), 
				*HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *ResponseStr);

			FileHeaders.Empty();

			// Make sure the response is an array
			if (!ResponseStr.StartsWith(TEXT("[")))
			{
				ResponseStr = FString(TEXT("[")) + ResponseStr + FString(TEXT("]"));
			}

			// Json parser expects arrays to always be wrapped with object
			FString ResponseStrJson = FString(TEXT("{\"files\":")) + ResponseStr + FString(TEXT("}"));
			// Create the Json parser
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(ResponseStrJson);

			if (FJsonSerializer::Deserialize(JsonReader,JsonObject) &&
				JsonObject.IsValid())
			{
				// Parse the array of file headers
				TArray<TSharedPtr<FJsonValue> > JsonFileHeaders = JsonObject->GetArrayField(TEXT("files"));
				for (TArray<TSharedPtr<FJsonValue> >::TConstIterator It(JsonFileHeaders); It; ++It)
				{
					TSharedPtr<FJsonObject> JsonFileHeader = (*It)->AsObject();
					if (JsonFileHeader.IsValid())
					{
						FCloudFileHeader FileHeader;
						if (JsonFileHeader->HasField(TEXT("hash")))
						{
							FileHeader.Hash = JsonFileHeader->GetStringField(TEXT("hash"));
						}
						if (JsonFileHeader->HasField(TEXT("uniqueFilename")))
						{
							FileHeader.DLName = JsonFileHeader->GetStringField(TEXT("uniqueFilename"));
						}
						if (JsonFileHeader->HasField(TEXT("filename")))
						{
							FileHeader.FileName = JsonFileHeader->GetStringField(TEXT("filename"));
						}
						if (JsonFileHeader->HasField(TEXT("length")))
						{
							FileHeader.FileSize = FMath::TruncToInt(JsonFileHeader->GetNumberField(TEXT("length")));
						}

						if (FileHeader.FileName.IsEmpty())
						{
							FileHeader.FileName = FileHeader.DLName;
						}

						if (FileHeader.Hash.IsEmpty() ||
							FileHeader.DLName.IsEmpty())
						{
							UE_LOG(LogEpicSurvey, Verbose, TEXT("Invalid file entry hash=%s dlname=%s filename=%s"),
								*FileHeader.Hash, *FileHeader.DLName, *FileHeader.FileName);
						}
						else
						{
							FileHeaders.Add(FileHeader);
						}
					}
				}
				bResult = true;
			}	
			else
			{
				ErrorStr = FString::Printf(TEXT("Invalid response payload=%s"),
					*ResponseStr);
			}
			
		}
		else
		{
			ErrorStr = FString::Printf(TEXT("Invalid response. code=%d error=%s"),
				HttpResponse->GetResponseCode(), *ResponseStr);
		}
	}


	if (!ErrorStr.IsEmpty())
	{
		UE_LOG(LogEpicSurvey, Verbose, TEXT("EnumerateFiles request failed. %s"), *ErrorStr);
	}

	TriggerOnEnumerateFilesCompleteDelegates(bResult, ErrorStr);
}

void FSurveyTitleCdnStorage::ReadFile_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	bool bResult = false;
	FString ResponseStr, ErrorStr;

	// should have a pending Http request
	FPendingFileRequest PendingRequest = FileRequests.FindChecked(HttpRequest);
	FileRequests.Remove(HttpRequest);

	// Cloud file being operated on
	FCloudFile* CloudFile = GetCloudFile(PendingRequest.FileName, true);
	CloudFile->AsyncState = EOnlineAsyncTaskState::Failed;
	CloudFile->Data.Empty();
	
	if (bSucceeded && 
		HttpResponse.IsValid())
	{
		
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{

			UE_LOG(LogEpicSurvey, Verbose, TEXT("ReadFile request complete. url=%s code=%d"), 
				*HttpRequest->GetURL(), HttpResponse->GetResponseCode());

			// update the memory copy of the file with data that was just downloaded
			CloudFile = GetCloudFile(PendingRequest.FileName, true);
			CloudFile->AsyncState = EOnlineAsyncTaskState::Done;
			CloudFile->Data = HttpResponse->GetContent();

			// cache to disk on successful download
			SaveCloudFileToDisk(CloudFile->FileName,CloudFile->Data);

			bResult = true;
		}
		else
		{
			ErrorStr = FString::Printf(TEXT("Invalid response. code=%d error=%s"),
				HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
		}
	}
	else
	{
		ErrorStr = TEXT("No response");
	}
	if (!ErrorStr.IsEmpty())
	{
		UE_LOG(LogEpicSurvey, Verbose, TEXT("ReadFile request failed. %s"), *ErrorStr);
	}

	TriggerOnReadFileCompleteDelegates(bResult, PendingRequest.FileName);
}
