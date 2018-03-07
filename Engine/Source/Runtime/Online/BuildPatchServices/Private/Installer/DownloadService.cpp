// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/DownloadService.h"
#include "Core/AsyncHelpers.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Common/StatsCollector.h"
#include "Installer/InstallerAnalytics.h"
#include "Common/HttpManager.h"
#include "Common/FileSystem.h"
#include "ThreadSafeBool.h"
#include "Misc/ScopeLock.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDownloadService, Warning, All);
DEFINE_LOG_CATEGORY(LogDownloadService);

namespace BuildPatchServices
{
	// 2MB buffer for reading from disk/network.
	const int32 FileReaderBufferSize = 2097152;
	// Empty array representation for no data.
	const TArray<uint8> NoData;

	struct FFileRequest
	{
	public:
		FFileRequest();

	public:
		TFuture<void> Future;
		FThreadSafeBool ShouldCancel;
	};

	FFileRequest::FFileRequest()
		: Future()
		, ShouldCancel(false)
	{
	}

	class FHttpDownload
		: public IDownload
	{
	public:
		FHttpDownload(const FHttpResponsePtr& InHttpResponse, bool bInSuccess);
		~FHttpDownload();

		// IDownload interface begin.
		virtual bool WasSuccessful() const override;
		virtual int32 GetResponseCode() const override;
		virtual const TArray<uint8>& GetData() const override;
		// IDownload interface end.

	private:
		FHttpResponsePtr HttpResponse;
		bool bSuccess;
	};

	FHttpDownload::FHttpDownload(const FHttpResponsePtr& InHttpResponse, bool bInSuccess)
		: HttpResponse(InHttpResponse)
		, bSuccess(bInSuccess)
	{
	}

	FHttpDownload::~FHttpDownload()
	{
	}

	bool FHttpDownload::WasSuccessful() const
	{
		return bSuccess;
	}

	int32 FHttpDownload::GetResponseCode() const
	{
		return HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : INDEX_NONE;
	}

	const TArray<uint8>& FHttpDownload::GetData() const
	{
		return HttpResponse.IsValid() ? HttpResponse->GetContent() : NoData;
	}

	class FFileDownload
		: public IDownload
	{
	public:
		FFileDownload(TArray<uint8> InDataArray, bool bInSuccess);
		~FFileDownload();

		// IDownload interface begin.
		virtual bool WasSuccessful() const override;
		virtual int32 GetResponseCode() const override;
		virtual const TArray<uint8>& GetData() const override;
		// IDownload interface end.

	private:
		TArray<uint8> DataArray;
		bool bSuccess;
	};

	FFileDownload::FFileDownload(TArray<uint8> InDataArray, bool bInSuccess)
		: DataArray(MoveTemp(InDataArray))
		, bSuccess(bInSuccess)
	{
	}

	FFileDownload::~FFileDownload()
	{
	}

	bool FFileDownload::WasSuccessful() const
	{
		return bSuccess;
	}

	int32 FFileDownload::GetResponseCode() const
	{
		return WasSuccessful() ? EHttpResponseCodes::Ok : EHttpResponseCodes::NotFound;
	}

	const TArray<uint8>& FFileDownload::GetData() const
	{
		return DataArray;
	}

	struct FDownloadDelegates
	{
	public:
		FDownloadDelegates();
		FDownloadDelegates(const FDownloadCompleteDelegate& InOnCompleteDelegate, const FDownloadProgressDelegate& InOnProgressDelegate);

	public:
		FDownloadCompleteDelegate OnCompleteDelegate;
		FDownloadProgressDelegate OnProgressDelegate;
	};

	FDownloadDelegates::FDownloadDelegates()
		: OnCompleteDelegate()
		, OnProgressDelegate()
	{
	}

	FDownloadDelegates::FDownloadDelegates(const FDownloadCompleteDelegate& InOnCompleteDelegate, const FDownloadProgressDelegate& InOnProgressDelegate)
		: OnCompleteDelegate(InOnCompleteDelegate)
		, OnProgressDelegate(InOnProgressDelegate)
	{
	}

	namespace DownloadServiceHelpers
	{
		void ExecuteCancelled(int32 RequestId, const FDownloadDelegates& DownloadDelegates)
		{
			DownloadDelegates.OnCompleteDelegate.ExecuteIfBound(RequestId, MakeShareable(new FFileDownload(TArray<uint8>(), false)));
		}

		void ExecuteCancelledAndReset(int32 RequestId, FDownloadDelegates& DownloadDelegates)
		{
			ExecuteCancelled(RequestId, DownloadDelegates);
			DownloadDelegates = FDownloadDelegates();
		}
	}

	class FDownloadService
		: public IDownloadService
	{
	private:
		/**
		 * This is a wrapper class for binding thread safe shared ptr delegates for the http module, without having to enforce that
		 * this service should be made using TShared* reference controllers.
		 */
		class FHttpDelegates
		{
		public:
			FHttpDelegates(FDownloadService& InDownloadService);

			void HttpRequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived, int32 RequestId);
			void HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, IDownloadServiceStat::FDownloadRecord DownloadRecord);

		private:
			FDownloadService& DownloadService;
		};

	public:
		FDownloadService(FTicker& Ticker, IHttpManager* HttpManager, IFileSystem* FileSystem, IDownloadServiceStat* DownloadServiceStat, IInstallerAnalytics* InstallerAnalytics);
		~FDownloadService();

		// IDownloadService interface begin.
		virtual int32 RequestFile(const FString& FileUri, const FDownloadCompleteDelegate& OnCompleteDelegate, const FDownloadProgressDelegate& OnProgressDelegate) override;
		virtual void RequestCancel(int32 RequestId) override;
		// IDownloadService interface end.

	private:
		bool Tick(float DeltaTime);
		void ProcessCancelRequests();
		void ProcessNewRequests();
		void ProcessProgressUpdates();
		void ProcessCompletedRequests();

		int32 MakeRequestId();
		IDownloadServiceStat::FDownloadRecord MakeDownloadRecord(int32 RequestId, FString Uri);
		TFunction<void()> MakeFileLoadTask(const int32& RequestId, const FString& FileUri, FFileRequest* FileRequest);

		void RegisterRequest(int32 RequestId, TSharedRef<IHttpRequest> HttpRequest);
		void RegisterRequest(int32 RequestId, TUniquePtr<FFileRequest> FileRequest);
		void UnregisterRequest(int32 RequestId);
		void HttpRequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived, int32 RequestId);
		void HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, IDownloadServiceStat::FDownloadRecord DownloadRecord);
		void SetRequestProgress(int32 RequestId, int32 BytesSoFar);
		void SetFileRequestComplete(int32 RequestId, bool bSuccess, TArray<uint8> FileDataArray);
		void SetHttpRequestComplete(int32 RequestId, bool bSuccess, FHttpResponsePtr Response);

	private:
		TSharedRef<FHttpDelegates, ESPMode::ThreadSafe> HttpDelegates;
		FTicker& Ticker;
		IHttpManager* HttpManager;
		IFileSystem* FileSystem;
		IDownloadServiceStat* DownloadServiceStat;
		IInstallerAnalytics* InstallerAnalytics;
		TSharedRef<FThreadSafeBool, ESPMode::ThreadSafe> SharedShouldRunState;
		FThreadSafeCounter RequestIdCounter;

		FCriticalSection RequestDelegatesCS;
		TMap<int32, FDownloadDelegates> RequestDelegates;

		FCriticalSection NewRequestsCS;
		TMap<int32, FString> NewRequests;

		FCriticalSection CancelRequestsCS;
		TArray<int32> CancelRequests;

		FCriticalSection ActiveRequestsCS;
		TMap<int32, TSharedRef<IHttpRequest>> ActiveHttpRequests;
		TMap<int32, TUniquePtr<FFileRequest>> ActiveFileRequests;

		FCriticalSection ProgressUpdatesCS;
		TMap<int32, int32> ProgressUpdates;

		FCriticalSection CompletedRequestsCS;
		TMap<int32, FDownloadRef> CompletedRequests;

		FDelegateHandle TickerHandle;
	};

	FDownloadService::FHttpDelegates::FHttpDelegates(FDownloadService& InDownloadService)
		: DownloadService(InDownloadService)
	{
	}

	void FDownloadService::FHttpDelegates::HttpRequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived, int32 RequestId)
	{
		DownloadService.HttpRequestProgress(MoveTemp(Request), BytesSent, BytesReceived, RequestId);
	}

	void FDownloadService::FHttpDelegates::HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, IDownloadServiceStat::FDownloadRecord DownloadRecord)
	{
		DownloadService.HttpRequestComplete(MoveTemp(Request), MoveTemp(Response), bSucceeded, MoveTemp(DownloadRecord));
	}

	FDownloadService::FDownloadService(FTicker& InTicker, IHttpManager* InHttpManager, IFileSystem* InFileSystem, IDownloadServiceStat* InDownloadServiceStat, IInstallerAnalytics* InInstallerAnalytics)
		: HttpDelegates(MakeShareable(new FHttpDelegates(*this)))
		, Ticker(InTicker)
		, HttpManager(InHttpManager)
		, FileSystem(InFileSystem)
		, DownloadServiceStat(InDownloadServiceStat)
		, InstallerAnalytics(InInstallerAnalytics)
		, SharedShouldRunState(MakeShareable(new FThreadSafeBool(true)))
		, RequestIdCounter(0)
		, RequestDelegatesCS()
		, RequestDelegates()
		, NewRequestsCS()
		, NewRequests()
		, CancelRequestsCS()
		, CancelRequests()
		, ActiveRequestsCS()
		, ActiveHttpRequests()
		, ActiveFileRequests()
		, ProgressUpdatesCS()
		, ProgressUpdates()
		, CompletedRequestsCS()
		, CompletedRequests()
	{
		check(IsInGameThread());
		TickerHandle = Ticker.AddTicker(FTickerDelegate::CreateRaw(this, &FDownloadService::Tick));
	}

	FDownloadService::~FDownloadService()
	{
		check(IsInGameThread());
		// Remove ticker.
		Ticker.RemoveTicker(TickerHandle);
		// Make sure our file threads will exit if they continue.
		SharedShouldRunState->AtomicSet(false);
		// Cancel all HTTP requests and wait for all file downloads threads to exit.
		ActiveRequestsCS.Lock();
		for (const TPair<int32, TSharedRef<IHttpRequest>>& ActiveHttpRequest : ActiveHttpRequests)
		{
			ActiveHttpRequest.Value->OnRequestProgress().Unbind();
			ActiveHttpRequest.Value->OnProcessRequestComplete().Unbind();
			ActiveHttpRequest.Value->CancelRequest();
		}
		for (const TPair<int32, TUniquePtr<FFileRequest>>& ActiveFileRequest : ActiveFileRequests)
		{
			ActiveFileRequest.Value->Future.Wait();
		}
		ActiveHttpRequests.Empty();
		ActiveFileRequests.Empty();
		ActiveRequestsCS.Unlock();
		// Kick off any remaining delegates next frame.
		RequestDelegatesCS.Lock();
		for (const TPair<int32, FDownloadDelegates>& RequestDelegate : RequestDelegates)
		{
			DownloadServiceHelpers::ExecuteCancelled(RequestDelegate.Key, RequestDelegate.Value);
		}
		RequestDelegates.Empty();
		RequestDelegatesCS.Unlock();
		// By this point all other references to SharedShouldRunState should have destructed.
		check(SharedShouldRunState.IsUnique());
	}

	int32 FDownloadService::RequestFile(const FString& FileUri, const FDownloadCompleteDelegate& OnCompleteDelegate, const FDownloadProgressDelegate& OnProgressDelegate)
	{
		const int32 NewRequestId = MakeRequestId();

		// Save the delegates.
		RequestDelegatesCS.Lock();
		RequestDelegates.Emplace(NewRequestId, FDownloadDelegates(OnCompleteDelegate, OnProgressDelegate));
		RequestDelegatesCS.Unlock();

		// Add the request.
		NewRequestsCS.Lock();
		NewRequests.Emplace(NewRequestId, FileUri);
		NewRequestsCS.Unlock();

		return NewRequestId;
	}

	void FDownloadService::RequestCancel(int32 RequestId)
	{
		// Add the request.
		FScopeLock ScopeLock(&CancelRequestsCS);
		CancelRequests.Add(RequestId);
	}

	bool FDownloadService::Tick(float DeltaTime)
	{
		ProcessCancelRequests();

		ProcessNewRequests();

		ProcessProgressUpdates();

		ProcessCompletedRequests();

		return true;
	}

	void FDownloadService::ProcessCancelRequests()
	{
		// Grab new cancel requests for this frame.
		TArray<int32> FrameCancelRequests;
		CancelRequestsCS.Lock();
		FrameCancelRequests = MoveTemp(CancelRequests);
		CancelRequestsCS.Unlock();

		// Cancel new requests that were not processed yet.
		TSet<int32> UnstartedRequests;
		NewRequestsCS.Lock();
		for (const int32& RequestId : FrameCancelRequests)
		{
			if (NewRequests.Remove(RequestId) > 0)
			{
				UnstartedRequests.Add(RequestId);
			}
		}
		NewRequestsCS.Unlock();

		// Cancel ongoing requests.
		TSet<int32> CancelledRequests;
		ActiveRequestsCS.Lock();
		for (const int32& RequestId : FrameCancelRequests)
		{
			if (ActiveHttpRequests.Contains(RequestId))
			{
				ActiveHttpRequests[RequestId]->CancelRequest();
				CancelledRequests.Add(RequestId);
			}
			if (ActiveFileRequests.Contains(RequestId))
			{
				ActiveFileRequests[RequestId]->ShouldCancel = true;
				CancelledRequests.Add(RequestId);
			}
		}
		ActiveRequestsCS.Unlock();

		// Call and remove delegates for new requests that were canceled.
		RequestDelegatesCS.Lock();
		for (const int32& RequestId : UnstartedRequests.Union(CancelledRequests))
		{
			if (ensureMsgf(RequestDelegates.Contains(RequestId), TEXT("Missing request delegates for %d"), RequestId))
			{
				DownloadServiceHelpers::ExecuteCancelledAndReset(RequestId, RequestDelegates[RequestId]);
				if (UnstartedRequests.Contains(RequestId))
				{
					RequestDelegates.Remove(RequestId);
				}
			}
		}
		RequestDelegatesCS.Unlock();
	}

	void FDownloadService::ProcessNewRequests()
	{
		// Grab new http requests for this frame.
		TMap<int32, FString> FrameNewRequests;
		NewRequestsCS.Lock();
		FrameNewRequests = MoveTemp(NewRequests);
		NewRequestsCS.Unlock();

		// Start new requests.
		for (const TPair<int32, FString>& NewRequest : FrameNewRequests)
		{
			const bool bIsHttpRequest = NewRequest.Value.StartsWith(TEXT("http"), ESearchCase::IgnoreCase);
			if (bIsHttpRequest)
			{
				// Kick off http request.
				TSharedRef<IHttpRequest> HttpRequest = HttpManager->CreateRequest();
				HttpRequest->OnRequestProgress().BindThreadSafeSP(HttpDelegates, &FHttpDelegates::HttpRequestProgress, NewRequest.Key);
				HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(HttpDelegates, &FHttpDelegates::HttpRequestComplete, MakeDownloadRecord(NewRequest.Key, NewRequest.Value));
				HttpRequest->SetURL(NewRequest.Value);
				HttpRequest->SetVerb(TEXT("GET"));
				HttpRequest->ProcessRequest();
				RegisterRequest(NewRequest.Key, MoveTemp(HttpRequest));
			}
			else
			{
				// Load file from drive/network.
				TUniquePtr<FFileRequest> FileRequest(new FFileRequest());
				FFileRequest* FileRequestPtr = FileRequest.Get();
				FileRequest->Future = Async(EAsyncExecution::ThreadPool, MakeFileLoadTask(NewRequest.Key, NewRequest.Value, FileRequestPtr));
				RegisterRequest(NewRequest.Key, MoveTemp(FileRequest));
			}
		}
	}

	void FDownloadService::ProcessProgressUpdates()
	{
		// Grab the progress updates for this frame.
		TMap<int32, int32> FrameProgressUpdates;
		ProgressUpdatesCS.Lock();
		FrameProgressUpdates = MoveTemp(ProgressUpdates);
		ProgressUpdatesCS.Unlock();

		// Process progress updates.
		RequestDelegatesCS.Lock();
		for (const TPair<int32, int32>& FrameProgressUpdate : FrameProgressUpdates)
		{
			if (ensureMsgf(RequestDelegates.Contains(FrameProgressUpdate.Key), TEXT("Missing request delegates for %d"), FrameProgressUpdate.Key))
			{
				RequestDelegates[FrameProgressUpdate.Key].OnProgressDelegate.ExecuteIfBound(FrameProgressUpdate.Key, FrameProgressUpdate.Value);
			}
		}
		RequestDelegatesCS.Unlock();
	}

	void FDownloadService::ProcessCompletedRequests()
	{
		// Grab the completed requests for this frame.
		TMap<int32, FDownloadRef> FrameCompletedRequests;
		CompletedRequestsCS.Lock();
		FrameCompletedRequests = MoveTemp(CompletedRequests);
		CompletedRequestsCS.Unlock();

		// Process completed requests.
		RequestDelegatesCS.Lock();
		for (const TPair<int32, FDownloadRef>& FrameCompletedRequest : FrameCompletedRequests)
		{
			if (ensureMsgf(RequestDelegates.Contains(FrameCompletedRequest.Key), TEXT("Missing request delegates for %d"), FrameCompletedRequest.Key))
			{
				RequestDelegates[FrameCompletedRequest.Key].OnCompleteDelegate.ExecuteIfBound(FrameCompletedRequest.Key, FrameCompletedRequest.Value);
				UnregisterRequest(FrameCompletedRequest.Key);
				RequestDelegates.Remove(FrameCompletedRequest.Key);
			}
		}
		RequestDelegatesCS.Unlock();
	}

	int32 FDownloadService::MakeRequestId()
	{
		return RequestIdCounter.Increment();
	}

	IDownloadServiceStat::FDownloadRecord FDownloadService::MakeDownloadRecord(int32 RequestId, FString Uri)
	{
		IDownloadServiceStat::FDownloadRecord DownloadRecord;
		DownloadRecord.RequestId = RequestId;
		DownloadRecord.Uri = MoveTemp(Uri);
		DownloadRecord.bSuccess = false;
		DownloadRecord.ResponseCode = INDEX_NONE;
		DownloadRecord.StartedAt = FStatsCollector::GetSeconds();
		DownloadRecord.CompletedAt = DownloadRecord.StartedAt;
		DownloadRecord.BytesReceived = 0;
		return DownloadRecord;
	}

	TFunction<void()> FDownloadService::MakeFileLoadTask(const int32& RequestId, const FString& FileUri, FFileRequest* FileRequest)
	{
		TWeakPtr<FThreadSafeBool, ESPMode::ThreadSafe> WeakShouldRun = SharedShouldRunState;
		return [this, RequestId, FileUri, FileRequest, WeakShouldRun]()
		{
			TSharedPtr<FThreadSafeBool, ESPMode::ThreadSafe> LambdaShouldRun = WeakShouldRun.Pin();
			TArray<uint8> FileDataArray;
			bool bSuccess = LambdaShouldRun.IsValid() && *LambdaShouldRun.Get() && !FileRequest->ShouldCancel;
			if (bSuccess)
			{
				IDownloadServiceStat::FDownloadRecord DownloadRecord = MakeDownloadRecord(RequestId, FileUri);
				TUniquePtr<FArchive> Reader = FileSystem->CreateFileReader(*FileUri);
				bSuccess = Reader.IsValid();
				if (bSuccess)
				{
					const int64 FileSize = Reader->TotalSize();
					FileDataArray.Reset();
					FileDataArray.AddUninitialized(FileSize);
					int64 BytesRead = 0;
					while (BytesRead < FileSize && *LambdaShouldRun.Get() && !FileRequest->ShouldCancel)
					{
						const int64 ReadLen = FMath::Min<int64>(FileReaderBufferSize, FileSize - BytesRead);
						Reader->Serialize(FileDataArray.GetData() + BytesRead, ReadLen);
						BytesRead += ReadLen;
						SetRequestProgress(RequestId, BytesRead);
					}
					DownloadRecord.BytesReceived = BytesRead;
					bSuccess = Reader->Close() && BytesRead == FileSize;
				}
				DownloadRecord.CompletedAt = FStatsCollector::GetSeconds();
				DownloadRecord.bSuccess = bSuccess;
				DownloadServiceStat->OnDownloadComplete(MoveTemp(DownloadRecord));
			}
			SetFileRequestComplete(RequestId, bSuccess, MoveTemp(FileDataArray));
		};
	}

	void FDownloadService::RegisterRequest(int32 RequestId, TUniquePtr<FFileRequest> FileRequest)
	{
		FScopeLock ScopeLock(&ActiveRequestsCS);
		ActiveFileRequests.Add(RequestId, MoveTemp(FileRequest));
	}

	void FDownloadService::RegisterRequest(int32 RequestId, TSharedRef<IHttpRequest> HttpRequest)
	{
		FScopeLock ScopeLock(&ActiveRequestsCS);
		ActiveHttpRequests.Add(RequestId, MoveTemp(HttpRequest));
	}

	void FDownloadService::UnregisterRequest(int32 RequestId)
	{
		FScopeLock ScopeLock(&ActiveRequestsCS);
		ActiveHttpRequests.Remove(RequestId);
		ActiveFileRequests.Remove(RequestId);
	}

	void FDownloadService::HttpRequestProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived, int32 RequestId)
	{
		SetRequestProgress(RequestId, BytesReceived);
	}

	void FDownloadService::HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, IDownloadServiceStat::FDownloadRecord DownloadRecord)
	{
		InstallerAnalytics->TrackRequest(Request);
		DownloadRecord.bSuccess = bSuccess;
		DownloadRecord.ResponseCode = Response.IsValid() ? Response->GetResponseCode() : INDEX_NONE;
		DownloadRecord.CompletedAt = FStatsCollector::GetSeconds();
		DownloadRecord.BytesReceived = Response.IsValid() ? Response->GetContent().Num() : 0;
		SetHttpRequestComplete(DownloadRecord.RequestId, bSuccess, MoveTemp(Response));
		DownloadServiceStat->OnDownloadComplete(MoveTemp(DownloadRecord));
	}

	void FDownloadService::SetRequestProgress(int32 RequestId, int32 BytesSoFar)
	{
		FScopeLock ScopeLock(&ProgressUpdatesCS);
		ProgressUpdates.Add(RequestId, BytesSoFar);
	}

	void FDownloadService::SetFileRequestComplete(int32 RequestId, bool bSuccess, TArray<uint8> FileDataArray)
	{
		FScopeLock ScopeLock(&CompletedRequestsCS);
		CompletedRequests.Emplace(RequestId, MakeShareable(new FFileDownload(MoveTemp(FileDataArray), bSuccess)));
	}

	void FDownloadService::SetHttpRequestComplete(int32 RequestId, bool bSuccess, FHttpResponsePtr Response)
	{
		FScopeLock ScopeLock(&CompletedRequestsCS);
		CompletedRequests.Emplace(RequestId, MakeShareable(new FHttpDownload(MoveTemp(Response), bSuccess)));
	}

	IDownloadService* FDownloadServiceFactory::Create(FTicker& Ticker, IHttpManager* HttpManager, IFileSystem* FileSystem, IDownloadServiceStat* DownloadServiceStat, IInstallerAnalytics* InstallerAnalytics)
	{
		check(HttpManager != nullptr);
		check(FileSystem != nullptr);
		check(DownloadServiceStat != nullptr);
		check(InstallerAnalytics != nullptr);
		return new FDownloadService(Ticker, HttpManager, FileSystem, DownloadServiceStat, InstallerAnalytics);
	}
}