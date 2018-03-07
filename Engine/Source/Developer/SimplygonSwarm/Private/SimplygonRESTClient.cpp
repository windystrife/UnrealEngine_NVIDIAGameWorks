// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SimplygonRESTClient.h"
#include "HAL/RunnableThread.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define HOSTNAME "http://127.0.0.1"
#define PORT ":55002"

DEFINE_LOG_CATEGORY_STATIC(LogSimplygonRESTClient, Verbose, All);

const TCHAR* SIMPLYGON_SWARM_REQUEST_DEBUG_TEMPALTE = TEXT("Error Processing Request %s");

FSimplygonSwarmTask::FSimplygonSwarmTask(const FSwarmTaskkData& InTaskData)
	:
	TaskData(InTaskData),
	State(SRS_UNKNOWN)
{
	bMultiPartUploadInitialized = false;
	TaskData.TaskUploadComplete = false;
	DebugHttpRequestCounter.Set(0);
	IsCompleted.AtomicSet(false);
	APIKey = TEXT("LOCAL");
	TaskData.JobName = TEXT("UE4_SWARM");
}

FSimplygonSwarmTask::~FSimplygonSwarmTask()
{
	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, Warning, TEXT("Destroying Task With Job Id %s"), *JobId);

	TArray<TSharedPtr<IHttpRequest>> OutPendingRequests;
	if (PendingRequests.GetKeys(OutPendingRequests) > 0)
	{
		for (int32 ItemIndex = 0; ItemIndex < OutPendingRequests.Num(); ++ItemIndex)
		{
			UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, Warning, TEXT("Pending Request for task with status %d"), (int32)OutPendingRequests[ItemIndex]->GetStatus());
			OutPendingRequests[ItemIndex]->CancelRequest();
		}
	}
	//check(DebugHttpRequestCounter.GetValue() == 0);
	UploadParts.Empty();

	//unbind the delegates
	OnAssetDownloaded().Unbind();
	OnAssetUploaded().Unbind();
	OnSwarmTaskFailed().Unbind();

}

void FSimplygonSwarmTask::CreateUploadParts(const int32 MaxUploadPartSize)
{
	//create upload parts
	UploadParts.Empty();
	TArray<uint8> fileBlob;
	if (FFileHelper::LoadFileToArray(fileBlob, *TaskData.ZipFilePath))
	{
		UploadSize = fileBlob.Num();
		if (fileBlob.Num() > MaxUploadPartSize)
		{
			int32 NumberOfPartsRequried = fileBlob.Num() / MaxUploadPartSize;
			int32 RemainingBytes = fileBlob.Num() % MaxUploadPartSize;

			for (int32 PartIndex = 0; PartIndex < NumberOfPartsRequried; ++PartIndex)
			{
				FSwarmUploadPart* UploadPartData = new (UploadParts) FSwarmUploadPart();
				int32 Offset = PartIndex * MaxUploadPartSize * sizeof(uint8);
				UploadPartData->Data.AddUninitialized(MaxUploadPartSize);
				FMemory::Memcpy(UploadPartData->Data.GetData(), fileBlob.GetData() + Offset, MaxUploadPartSize * sizeof(uint8));
				UploadPartData->PartNumber = PartIndex + 1;
				UploadPartData->PartUploaded = false;
			}

			if (RemainingBytes > 0)
			{
				//NOTE: need to set Offset before doing a new on UploadParts
				int32 Offset = UploadParts.Num() * MaxUploadPartSize;
				FSwarmUploadPart* UploadPartData = new (UploadParts) FSwarmUploadPart();
				UploadPartData->Data.AddUninitialized(RemainingBytes);
				FMemory::Memcpy(UploadPartData->Data.GetData(), fileBlob.GetData() + Offset, RemainingBytes * sizeof(uint8));
				UploadPartData->PartNumber = NumberOfPartsRequried + 1;
				UploadPartData->PartUploaded = false;
			}
		}
		else
		{
			FSwarmUploadPart* UploadPartData = new (UploadParts) FSwarmUploadPart();
			UploadPartData->Data.AddUninitialized(fileBlob.Num());
			FMemory::Memcpy(UploadPartData->Data.GetData(), fileBlob.GetData(), fileBlob.Num() * sizeof(uint8));
			UploadPartData->PartNumber = 1;
			UploadPartData->PartUploaded = false;
		}

		TotalParts = UploadParts.Num();
		RemainingPartsToUpload.Add(TotalParts);
	}
}

bool FSimplygonSwarmTask::NeedsMultiPartUpload()
{
	return UploadParts.Num() > 1;
}

SimplygonRESTState FSimplygonSwarmTask::GetState() const
{
	FScopeLock Lock(TaskData.StateLock);
	return State;
}

void FSimplygonSwarmTask::SetHost(FString InHostAddress)
{
	HostName = InHostAddress;
}

void FSimplygonSwarmTask::EnableDebugLogging()
{
	bEnableDebugLogging = true;
}

void FSimplygonSwarmTask::SetState(SimplygonRESTState InState)
{
	if (this != nullptr && TaskData.StateLock)
	{
		FScopeLock Lock(TaskData.StateLock);

		//do not update the state if either of these set is already set
		if (InState != State
			&& (State != SRS_ASSETDOWNLOADED && State != SRS_FAILED))
		{
			State = InState;
		}
		else if ((InState != State) && (State != SRS_FAILED) && (InState == SRS_ASSETDOWNLOADED))
		{
			State = SRS_ASSETDOWNLOADED;
			this->IsCompleted.AtomicSet(true);
		}
		else if (InState != State && InState == SRS_FAILED)
		{
			State = SRS_ASSETDOWNLOADED;
		}

	}
	else
	{
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, Error, TEXT("Object in invalid state can not acquire state lock"));
	}

}

bool FSimplygonSwarmTask::IsFinished() const
{
	FScopeLock Lock(TaskData.StateLock);
	return IsCompleted;
}

bool FSimplygonSwarmTask::ParseJsonMessage(FString InJsonMessage, FSwarmJsonResponse& OutResponseData)
{
	bool sucess = false;

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(InJsonMessage);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
	{
		FString Status;
		if (JsonParsed->HasField("JobId"))
		{
			JsonParsed->TryGetStringField("JobId", OutResponseData.JobId);
		}

		if (JsonParsed->HasField("Status"))
		{
			OutResponseData.Status = JsonParsed->GetStringField("Status");
		}

		if (JsonParsed->HasField("OutputAssetId"))
		{
			JsonParsed->TryGetStringField("OutputAssetId", OutResponseData.OutputAssetId);
		}

		if (JsonParsed->HasField("AssetId"))
		{
			JsonParsed->TryGetStringField("AssetId", OutResponseData.AssetId);
		}

		if (JsonParsed->HasField("ProgressPercentage"))
		{
			JsonParsed->TryGetNumberField("ProgressPercentage", OutResponseData.Progress);
		}

		if (JsonParsed->HasField("ProgressPercentage"))
		{
			JsonParsed->TryGetNumberField("ProgressPercentage", OutResponseData.Progress);
		}

		if (JsonParsed->HasField("UploadId"))
		{
			JsonParsed->TryGetStringField("UploadId", OutResponseData.UploadId);
		}

		sucess = true;
	}


	return sucess;
}


// ~ HTTP Request method to communicate with Simplygon REST Interface


void FSimplygonSwarmTask::AccountInfo()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	AddAuthenticationHeader(request);
	request->SetURL(FString::Printf(TEXT("%s/2.3/account?apikey=%s"), *HostName, *APIKey));
	request->SetVerb(TEXT("GET"));

	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::AccountInfo_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());
	}
}

void FSimplygonSwarmTask::AccountInfo_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();
	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response Invalid %s"), *Request->GetURL());
		return;
	}

	if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response Message %s"), *msg);
	}
	else
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
	}
}


void FSimplygonSwarmTask::CreateJob()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/create?apikey=%s&job_name=%s&asset_id=%s"), *HostName, *APIKey, *TaskData.JobName, *InputAssetId);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::CreateJob_Response);

	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());
		SetState(SRS_JOBCREATED_PENDING);
	}
}

void FSimplygonSwarmTask::CreateJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();

	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response Invalid %s"), *Request->GetURL());
		return;
	}

	if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();

		FSwarmJsonResponse Data;
		if (ParseJsonMessage(msg, Data))
		{
			if (!Data.JobId.IsEmpty())
			{
				JobId = Data.JobId;
				UE_LOG(LogSimplygonRESTClient, Display, TEXT("Created JobId: %s"), *Data.JobId);
				SetState(SRS_JOBCREATED);
			}
			else
				UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, Warning, TEXT("Empty JobId for task"));

		}
		else
		{
			SetState(SRS_FAILED);
			UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to parse message %s for Request %s"), *msg, *Request->GetURL());
		}


	}
	else
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::UploadJobSettings()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s/uploadsettings?apikey=%s"), *HostName, *JobId, *APIKey);

	TArray<uint8> data;
	FFileHelper::LoadFileToArray(data, *TaskData.SplFilePath);

	AddAuthenticationHeader(request);
	request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));
	request->SetContent(data);

	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::UploadJobSettings_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());
		SetState(SRS_JOBSETTINGSUPLOADED_PENDING);
	}
}

void FSimplygonSwarmTask::UploadJobSettings_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();

	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		return;
	}

	if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{

		if (!OnAssetUploaded().IsBound())
		{
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("OnAssetUploaded delegate not bound to any object"));
		}
		else
		{
			OnAssetUploaded().Execute(*this);
			SetState(SRS_JOBSETTINGSUPLOADED);
		}
	}
	else
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::ProcessJob()
{
	SetState(SRS_JOBPROCESSING_PENDING);

	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s/Process?apikey=%s"), *HostName, *JobId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("PUT"));

	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::ProcessJob_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s Response code %d"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());
	}
}

void FSimplygonSwarmTask::ProcessJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();

	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		return;
	}

	if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		SetState(SRS_JOBPROCESSING);

	}
	else
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::GetJob()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/job/%s?apikey=%s"), *HostName, *JobId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("GET"));
	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::GetJob_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());
	}
}

void FSimplygonSwarmTask::GetJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();

	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!Response.IsValid())
	{
		SetState(SRS_FAILED);
		return;
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString msg = Response->GetContentAsString();

		FSwarmJsonResponse Data;
		if (!msg.IsEmpty() && ParseJsonMessage(msg, Data))
		{

			if (!Data.Status.IsEmpty())
			{
				if (Data.Status == "Processed")
				{
					if (!Data.OutputAssetId.IsEmpty())
					{
						OutputAssetId = Data.OutputAssetId;
						SetState(SRS_JOBPROCESSED);
						//UE_LOG(LogSimplygonRESTClient, Log, TEXT("Job with Id %s processed"), *Data.JobId);

					}
					else
					{
						SetState(SRS_FAILED);
					}
				}
				if (Data.Status == "Failed")
				{
					SetState(SRS_FAILED);
					UE_LOG(LogSimplygonRESTClient, Error, TEXT("Job with id %s Failed %s"), *Data.JobId);
				}
			}
		}
		else
		{
			//SetState(SRS_FAILED);
		}
	}
	else
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::UploadAsset()
{
	//if multipart upload is need then use multi part request else use older request to upload data
	if (NeedsMultiPartUpload())
	{
		int32 PartsToUpload = RemainingPartsToUpload.GetValue();
		int32 PartIndex = TotalParts - PartsToUpload;
		if (!bMultiPartUploadInitialized && PartsToUpload > 0)
		{
			MultiPartUploadBegin();
		}
		else if (PartsToUpload > 0)
		{
			MultiPartUploadPart(UploadParts[PartIndex].PartNumber);
		}
		else if (PartsToUpload == 0)
		{
			if (!TaskData.TaskUploadComplete)
				MultiPartUploadEnd();
			else
			{
				//FPlatformProcess::Sleep(0.1f);
				MultiPartUploadGet();
			}

		}
	}
	else
	{
		//bail if part has already been uploaded
		if (UploadParts[0].PartUploaded)
		{
			UE_LOG(LogSimplygonRESTClient, Display, TEXT("Skip Already Uploaded Asset."));
			return;
		}

		TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

		FString url = FString::Printf(TEXT("%s/2.3/asset/upload?apikey=%s&asset_name=%s"), *HostName, *APIKey, *TaskData.JobName);

		AddAuthenticationHeader(request);
		request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
		request->SetURL(url);
		request->SetVerb(TEXT("POST"));
		request->SetContent(UploadParts[0].Data);

		request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::UploadAsset_Response);


		uint32 bufferSize = UploadParts[0].Data.Num();

		FHttpModule::Get().SetMaxReadBufferSize(bufferSize);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

		if (!request->ProcessRequest())
		{
			SetState(SRS_FAILED);
			UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
		}
		else
		{
			DebugHttpRequestCounter.Increment();
			PendingRequests.Add(request, request->GetURL());
			SetState(SRS_ASSETUPLOADED_PENDING);
		}
	}


}

void FSimplygonSwarmTask::UploadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();

	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!bWasSuccessful)
	{
		SetState(SRS_FAILED);
		if (Response.IsValid())
		{
			UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Upload asset Response: %i"), Response->GetResponseCode());
		}
		else
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload asset response failed."));
	}
	else
	{
		if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			FString msg = Response->GetContentAsString();

			FSwarmJsonResponse Data;
			if (ParseJsonMessage(msg, Data))
			{
				if (!Data.AssetId.IsEmpty())
				{
					InputAssetId = Data.AssetId;
					UploadParts[0].PartUploaded = true;
					SetState(SRS_ASSETUPLOADED);
				}
				else
					UE_LOG(LogSimplygonRESTClient, Display, TEXT("Could not parse Input asset Id for job: %s"), *Data.JobId);
			}
			else
			{
				SetState(SRS_FAILED);
			}
		}
		else
		{
			UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
		}

	}

}

void FSimplygonSwarmTask::DownloadAsset()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	if (OutputAssetId.IsEmpty())
	{
		SetState(SRS_FAILED);
	}

	FString url = FString::Printf(TEXT("%s/2.3/asset/%s/download?apikey=%s"), *HostName, *OutputAssetId, *APIKey);
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("GET"));
	FHttpModule::Get().

		FHttpModule::Get().SetHttpTimeout(300);
	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::DownloadAsset_Response);

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());

		UE_LOG(LogSimplygonRESTClient, Log, TEXT("Downloading Job with Id %s"), *JobId);
		SetState(SRS_ASSETDOWNLOADED_PENDING);
	}
}

void FSimplygonSwarmTask::DownloadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{

	DebugHttpRequestCounter.Decrement();

	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!Response.IsValid())
	{
		return;
	}

	if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		if (this == nullptr || JobId.IsEmpty())
		{
			UE_LOG(LogSimplygonRESTClient, Display, TEXT("Object has already been destoryed or job id was empty"));
			return;
		}
		TArray<uint8> data = Response->GetContent();
		/*FString msg = Response->GetContentAsString();
		FSwarmJsonResponse Data;*/

		if (data.Num() > 0)
		{
			if (!TaskData.OutputZipFilePath.IsEmpty() && !FFileHelper::SaveArrayToFile(data, *TaskData.OutputZipFilePath))
			{
				UE_LOG(LogSimplygonRESTClient, Display, TEXT("Unable to save file %s"), *TaskData.OutputZipFilePath);
				SetState(SRS_FAILED);

			}
			else
			{
				if (!this->IsCompleted &&  OnAssetDownloaded().IsBound())
				{

					UE_LOG(LogSimplygonRESTClient, Display, TEXT("Asset downloaded"));
					OnAssetDownloaded().Execute(*this);
					SetState(SRS_ASSETDOWNLOADED);
				}
				else
					UE_LOG(LogSimplygonRESTClient, Display, TEXT("OnAssetDownloaded delegatge not bound to any objects"));
			}
		}
	}
	else
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
	}
}

void FSimplygonSwarmTask::MultiPartUploadBegin()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/asset/uploadpart?apikey=%s&asset_name=%s"), *HostName, *APIKey, *TaskData.JobName);


	FArrayWriter Writer;
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::MultiPartUploadBegin_Response);
	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());
	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());

		SetState(SRS_ASSETUPLOADED_PENDING);
	}
}

void FSimplygonSwarmTask::MultiPartUploadBegin_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();

	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!bWasSuccessful)
	{
		SetState(SRS_FAILED);
		if (Response.IsValid())
		{
			UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Upload asset Response: %i"), Response->GetResponseCode());
		}
		else
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload asset response failed."));
	}
	else
	{
		if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			FString msg = Response->GetContentAsString();
			FSwarmJsonResponse Data;

			if (ParseJsonMessage(msg, Data))
			{
				if (!Data.UploadId.IsEmpty())
				{
					UploadId = Data.UploadId;
					bMultiPartUploadInitialized = true;
				}
			}
			else
			{

				SetState(SRS_FAILED);

			}
		}
		else
			UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());

	}
}

void FSimplygonSwarmTask::MultiPartUploadPart(const uint32 InPartNumber)
{

	//should bailout if part has already been uploaded
	int32 PartIndex = InPartNumber - 1;
	if (UploadParts[PartIndex].PartUploaded)
		return;

	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/asset/uploadpart/%s/upload?apikey=%s&part_number=%d"), *HostName, *UploadId, *APIKey, UploadParts[PartIndex].PartNumber);

	FArrayWriter Writer;
	AddAuthenticationHeader(request);
	request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	request->SetURL(url);
	request->SetVerb(TEXT("PUT"));
	request->SetContent(UploadParts[PartIndex].Data);
	FHttpModule::Get().SetMaxReadBufferSize(UploadParts[PartIndex].Data.Num());

	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::MultiPartUploadPart_Response);

	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());
	}

}

void FSimplygonSwarmTask::MultiPartUploadPart_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{

	DebugHttpRequestCounter.Decrement();
	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!bWasSuccessful)
	{
		SetState(SRS_FAILED);
		if (Response.IsValid())
		{
			UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Upload asset Response: %i"), Response->GetResponseCode());
		}
		else
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload asset response failed."));
	}
	else
	{
		if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			FString msg = Response->GetContentAsString();

			//get part_number from query string
			FString PartNumStr = Request->GetURLParameter("part_number");
			if (!PartNumStr.IsEmpty())
			{
				int32 PartNum = FCString::Atoi(*PartNumStr);
				PartNum -= 1;
				if (!UploadParts[PartNum].PartUploaded)
				{
					RemainingPartsToUpload.Decrement();
					UploadParts[PartNum].PartUploaded = true;
				}
			}
		}
		else
		{
			UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
		}

	}
}

void FSimplygonSwarmTask::MultiPartUploadEnd()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/asset/uploadpart/%s/Complete?apikey=%s&part_count=%d&upload_size=%d"), *HostName, *UploadId, *APIKey, TotalParts, UploadSize);

	FArrayWriter Writer;
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("POST"));
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::MultiPartUploadEnd_Response);

	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());
	}
}

void FSimplygonSwarmTask::MultiPartUploadEnd_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();

	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!bWasSuccessful)
	{
		SetState(SRS_FAILED);
		if (Response.IsValid())
		{
			UE_LOG(LogSimplygonRESTClient, Warning, TEXT("Upload asset Response: %i"), Response->GetResponseCode());
		}
		else
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload asset response failed."));
	}
	else
	{
		if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			FString msg = Response->GetContentAsString();

			FSwarmJsonResponse Data;
			if (ParseJsonMessage(msg, Data))
			{
				if (!Data.UploadId.IsEmpty())
				{
					this->TaskData.TaskUploadComplete = true;
				}

			}
			else
			{
				SetState(SRS_FAILED);
			}
		}
		else
			UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());

	}
}

void FSimplygonSwarmTask::MultiPartUploadGet()
{
	TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();

	FString url = FString::Printf(TEXT("%s/2.3/asset/uploadpart/%s?apikey=%s"), *HostName, *UploadId, *APIKey);

	FArrayWriter Writer;
	AddAuthenticationHeader(request);
	request->SetURL(url);
	request->SetVerb(TEXT("GET"));
	request->OnProcessRequestComplete().BindRaw(this, &FSimplygonSwarmTask::MultiPartUploadGet_Response);

	UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("%s"), *request->GetURL());

	if (!request->ProcessRequest())
	{
		SetState(SRS_FAILED);
		UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Failed to process Request %s"), *request->GetURL());
	}
	else
	{
		DebugHttpRequestCounter.Increment();
		PendingRequests.Add(request, request->GetURL());
	}
}

void FSimplygonSwarmTask::MultiPartUploadGet_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	DebugHttpRequestCounter.Decrement();
	FString OutUrl = PendingRequests.FindRef(Request);

	if (!OutUrl.IsEmpty())
	{
		PendingRequests.Remove(Request);
	}

	if (!bWasSuccessful)
	{
		SetState(SRS_FAILED);
		if (Response.IsValid())
		{
			UE_LOG(LogSimplygonRESTClient, Warning, TEXT("%s: %i"), __FUNCTION__, Response->GetResponseCode());
		}
		else
			UE_LOG(LogSimplygonRESTClient, Error, TEXT("Upload asset response failed."));
	}
	else
	{
		if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			FString msg = Response->GetContentAsString();

			FSwarmJsonResponse Data;

			if (ParseJsonMessage(msg, Data))
			{
				if (!Data.AssetId.IsEmpty())
				{
					InputAssetId = Data.AssetId;
					SetState(SRS_ASSETUPLOADED);
					UploadParts.Empty();
				}
			}
		}
		else
			UE_CLOG(bEnableDebugLogging, LogSimplygonRESTClient, VeryVerbose, TEXT("Response failed %s Code %d"), *Request->GetURL(), Response->GetResponseCode());
	}
}


void FSimplygonSwarmTask::AddAuthenticationHeader(TSharedRef<IHttpRequest> request)
{
	//Basic User:User
	request->SetHeader("Authorization", "Basic dXNlcjp1c2Vy");
}

FSimplygonRESTClient* FSimplygonRESTClient::Runnable = nullptr;

FSimplygonRESTClient::FSimplygonRESTClient()
	:
	Thread(nullptr),
	HostName(TEXT(HOSTNAME)),
	APIKey(TEXT("LOCAL")),
	bEnableDebugging(false)
{
	FString IP = GetDefault<UEditorPerProjectUserSettings>()->SimplygonServerIP;
	bEnableDebugging = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
	if (IP != "")
	{
		if (!IP.Contains("http://"))
		{
			HostName = "http://" + IP;
		}
		else
		{
			HostName = IP;
		}
	}
	else
	{
		HostName = TEXT(HOSTNAME);
	}

	HostName += TEXT(PORT);
	JobLimit = GetDefault<UEditorPerProjectUserSettings>()->SwarmNumOfConcurrentJobs;


	DelayBetweenRuns = GetDefault<UEditorPerProjectUserSettings>()->SimplygonSwarmDelay / 1000;
	DelayBetweenRuns = DelayBetweenRuns <= 5 ? 5 : DelayBetweenRuns;

	Thread = FRunnableThread::Create(this, TEXT("SimplygonRESTClient"));
}

FSimplygonRESTClient::~FSimplygonRESTClient()
{
	if (Thread != nullptr)
	{
		delete Thread;
		Thread = nullptr;
	}
}


bool FSimplygonRESTClient::Init()
{
	return true;
}


void FSimplygonRESTClient::Wait(const float InSeconds, const float InSleepTime /* = 0.1f */)
{
	// Instead of waiting the given amount of seconds doing nothing
	// check periodically if there's been any Stop requests.
	for (float TimeToWait = InSeconds; TimeToWait > 0.0f && ShouldStop() == false; TimeToWait -= InSleepTime)
	{
		FPlatformProcess::Sleep(FMath::Min(InSleepTime, TimeToWait));
	}
}

uint32 FSimplygonRESTClient::Run()
{
	Wait(5);
	//float SleepDelay = DelayBetweenRuns;
	do
	{

		MoveItemsToBoundedArray();

		UpdateTaskStates();

		Wait(DelayBetweenRuns);

	} while (ShouldStop() == false);


	return 0;
}

void FSimplygonRESTClient::UpdateTaskStates()
{
	TArray<TSharedPtr<class FSimplygonSwarmTask>> TasksMarkedForRemoval;
	//TasksMarkedForRemoval.AddUninitialized(JobLimit);

	FScopeLock Lock(&CriticalSectionData);

	for (int32 Index = 0; Index < JobsBuffer.Num(); Index++)
	{
		//
		TSharedPtr<FSimplygonSwarmTask>& SwarmTask = JobsBuffer[Index];
		switch (SwarmTask->GetState())
		{
		case SRS_UNKNOWN:
		case SRS_ASSETUPLOADED_PENDING:
			SwarmTask->UploadAsset();
			break;
		case SRS_FAILED:
			TasksMarkedForRemoval.Add(SwarmTask);
			break;
		case SRS_ASSETUPLOADED:
			SwarmTask->CreateJob();
			break;
		case SRS_JOBCREATED:
			SwarmTask->UploadJobSettings();
			break;
		case SRS_JOBSETTINGSUPLOADED:
			SwarmTask->ProcessJob();
			break;
		case SRS_JOBPROCESSING:
			SwarmTask->GetJob();
			break;
		case SRS_JOBPROCESSED:
			SwarmTask->DownloadAsset();
			break;
		case SRS_ASSETDOWNLOADED:
			TasksMarkedForRemoval.Add(SwarmTask);
			break;
		}
	}

	for (int32 Index = 0; Index < TasksMarkedForRemoval.Num(); Index++)
	{

		if (TasksMarkedForRemoval[Index]->GetState() == SRS_FAILED)
		{
			TasksMarkedForRemoval[Index]->OnSwarmTaskFailed().ExecuteIfBound(*TasksMarkedForRemoval[Index]);
		}
		JobsBuffer.Remove(TasksMarkedForRemoval[Index]);
	}

	TasksMarkedForRemoval.Empty();
}

void FSimplygonRESTClient::MoveItemsToBoundedArray()
{
	if (!PendingJobs.IsEmpty())
	{


		int32 ItemsToInsert = 0;
		if (JobsBuffer.Num() >= JobLimit)
		{
			ItemsToInsert = JobLimit;
		}
		else if (JobsBuffer.Num() < JobLimit)
		{
			JobsBuffer.Shrink();
			ItemsToInsert = JobLimit - JobsBuffer.Num();

			FScopeLock Lock(&CriticalSectionData);
			do
			{
				TSharedPtr<FSimplygonSwarmTask> OutItem;
				if (PendingJobs.Dequeue(OutItem))
				{
					OutItem->CreateUploadParts(MaxUploadSizeInBytes);
					JobsBuffer.Add(OutItem);
				}

			} while (!PendingJobs.IsEmpty() && JobsBuffer.Num() < JobLimit);
		}

	}
}

FSimplygonRESTClient* FSimplygonRESTClient::Get()
{
	if (!Runnable && FPlatformProcess::SupportsMultithreading())
	{
		Runnable = new FSimplygonRESTClient();
	}
	return Runnable;
}

void FSimplygonRESTClient::Shutdown()
{
	if (Runnable)
	{
		Runnable->EnusureCompletion();
		delete Runnable;
		Runnable = nullptr;
	}
}

void FSimplygonRESTClient::Stop()
{
	StopTaskCounter.Increment();
}



void FSimplygonRESTClient::EnusureCompletion()
{
	Stop();
	Thread->WaitForCompletion();
}

void FSimplygonRESTClient::AddSwarmTask(TSharedPtr<FSimplygonSwarmTask>& InTask)
{
	//FScopeLock Lock(&CriticalSectionData);
	InTask->SetHost(HostName);
	PendingJobs.Enqueue(InTask);

}

void FSimplygonRESTClient::SetMaxUploadSizeInBytes(int32 InMaxUploadSizeInBytes)
{
	MaxUploadSizeInBytes = InMaxUploadSizeInBytes;
}

void FSimplygonRESTClient::Exit()
{

}

