// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationTest.h"
#include "Tests/TestHelpers.h"
#include "Tests/Fake/HttpManager.fake.h"
#include "Tests/Mock/FileSystem.mock.h"
#include "Tests/Mock/DownloadServiceStat.mock.h"
#include "Tests/Mock/InstallerAnalytics.mock.h"
#include "Installer/DownloadService.h"
#include "Containers/Ticker.h"
#include "BuildPatchHash.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDownloadServiceSpec, "BuildPatchServices.Unit", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
// Unit.
TUniquePtr<BuildPatchServices::IDownloadService> DownloadService;
// Mock/Fake.
FTicker Ticker;
TUniquePtr<BuildPatchServices::FFakeHttpManager> FakeHttpModule;
TUniquePtr<BuildPatchServices::FMockFileSystem> MockFileSystem;
TUniquePtr<BuildPatchServices::FMockDownloadServiceStat> MockDownloadServiceStat;
TUniquePtr<BuildPatchServices::FMockInstallerAnalytics> MockInstallerAnalytics;
// Data.
BuildPatchServices::FDownloadProgressDelegate DownloadProgress;
BuildPatchServices::FDownloadCompleteDelegate DownloadComplete;
TArray<TTuple<double, int32, int32>> RxDownloadProgress;
TArray<TTuple<double, int32, BuildPatchServices::FDownloadRef>> RxDownloadComplete;
FString HttpFileUrl;
FString HttpsFileUrl;
FString NetworkFileUrl;
int32 MadeRequestId;
// Ticker helpers.
void DoTick();
void DoTicksUntilCreated(int32 TickCount = 50, int32 CreateCount = 1, float Sleep = 0.0f);
void DoTicksUntilComplete(int32 TickCount = 50, int32 CompleteCount = 1, float Sleep = 0.0f);
END_DEFINE_SPEC(FDownloadServiceSpec)

void FDownloadServiceSpec::Define()
{
	using namespace BuildPatchServices;

	// Data setup.
	FRollingHashConst::Init();
	DownloadProgress.BindLambda([this](int32 RequestId, int32 BytesSoFar)
	{
		RxDownloadProgress.Emplace(FStatsCollector::GetSeconds(), RequestId, BytesSoFar);
	});
	DownloadComplete.BindLambda([this](int32 RequestId, const FDownloadRef& Download)
	{
		RxDownloadComplete.Emplace(FStatsCollector::GetSeconds(), RequestId, Download);
	});
	HttpFileUrl = TEXT("http://download.tests.com/file.dat");
	HttpsFileUrl = TEXT("https://download.tests.com/file.dat");
	NetworkFileUrl = TEXT("\\\\somenetwork\\somefolder\\file.dat");

	// Specs.
	BeforeEach([this]()
	{
		Ticker = FTicker();
		FakeHttpModule.Reset(new FFakeHttpManager(Ticker));
		MockFileSystem.Reset(new FMockFileSystem());
		MockDownloadServiceStat.Reset(new FMockDownloadServiceStat());
		MockInstallerAnalytics.Reset(new FMockInstallerAnalytics());
		DownloadService.Reset(FDownloadServiceFactory::Create(
			Ticker,
			FakeHttpModule.Get(),
			MockFileSystem.Get(),
			MockDownloadServiceStat.Get(),
			MockInstallerAnalytics.Get()));
	});

	Describe("DownloadService", [this]()
	{
		Describe("RequestFile", [this]()
		{
			Describe("when given a FileUri starting with http", [this]()
			{
				It("should use the http module to process the request.", [this]()
				{
					DownloadService->RequestFile(HttpFileUrl, DownloadComplete, DownloadProgress);
					DoTicksUntilComplete();
					TEST_EQUAL(FakeHttpModule->RxCreateRequest, 1);
					TEST_EQUAL(MockFileSystem->RxCreateFileReader.Num(), 0);
				});
			});

			Describe("when given a FileUri starting with https", [this]()
			{
				It("should use the http module to process the request.", [this]()
				{
					DownloadService->RequestFile(HttpsFileUrl, DownloadComplete, DownloadProgress);
					DoTicksUntilComplete();
					TEST_EQUAL(FakeHttpModule->RxCreateRequest, 1);
					TEST_EQUAL(MockFileSystem->RxCreateFileReader.Num(), 0);
				});
			});

			Describe("when given a FileUri not starting with http", [this]()
			{
				It("should use the file manager to process the request.", [this]()
				{
					DownloadService->RequestFile(NetworkFileUrl, DownloadComplete, DownloadProgress);
					DoTicksUntilComplete(50, 1);
					TEST_EQUAL(FakeHttpModule->RxCreateRequest, 0);
					TEST_EQUAL(MockFileSystem->RxCreateFileReader.Num(), 1);
				});
			});

			Describe("when the http request results in success", [this]()
			{
				BeforeEach([this]()
				{
					FakeHttpModule->DataServed.Add(HttpFileUrl, {1,2,3,4,5,6,7,8,9,10});
				});

				It("should provide an IDownload with access to success status.", [this]()
				{
					DownloadService->RequestFile(HttpFileUrl, DownloadComplete, DownloadProgress);
					DoTicksUntilComplete();
					TEST_EQUAL(RxDownloadComplete.Num(), 1);
					if (RxDownloadComplete.Num() == 1)
					{
						const FDownloadRef& Download = RxDownloadComplete[0].Get<2>();
						TEST_TRUE(Download->WasSuccessful());
						TEST_EQUAL(Download->GetResponseCode(), EHttpResponseCodes::Ok);
						TEST_EQUAL(Download->GetData(), FakeHttpModule->DataServed[HttpFileUrl]);
					}
				});
			});

			Describe("when the file request results in success", [this]()
			{
				BeforeEach([this]()
				{
					MockFileSystem->ReadFile = {1,2,3,4,5,6,7,8,9,10};
				});

				It("should provide an IDownload with access to success status.", [this]()
				{
					DownloadService->RequestFile(NetworkFileUrl, DownloadComplete, DownloadProgress);
					DoTicksUntilComplete();
					TEST_EQUAL(RxDownloadComplete.Num(), 1);
					if (RxDownloadComplete.Num() == 1)
					{
						const FDownloadRef& Download = RxDownloadComplete[0].Get<2>();
						TEST_TRUE(Download->WasSuccessful());
						TEST_EQUAL(Download->GetResponseCode(), EHttpResponseCodes::Ok);
						TEST_EQUAL(Download->GetData(), MockFileSystem->ReadFile);
					}
				});
			});
		});

		Describe("RequestCancel", [this]()
		{
			Describe("when a file request was made", [this]()
			{
				BeforeEach([this]()
				{
					MockFileSystem->ReadFile.AddUninitialized(1024 * 1024 * 50);
					MadeRequestId = DownloadService->RequestFile(NetworkFileUrl, DownloadComplete, DownloadProgress);
				});

				It("should cancel if it was not started yet.", [this]()
				{
					DownloadService->RequestCancel(MadeRequestId);
					DoTicksUntilComplete();
					TEST_EQUAL(FakeHttpModule->RxCreateRequest, 0);
					TEST_EQUAL(MockFileSystem->RxCreateFileReader.Num(), 0);
					TEST_EQUAL(RxDownloadComplete.Num(), 1);
					if (RxDownloadComplete.Num() == 1)
					{
						const FDownloadRef& Download = RxDownloadComplete[0].Get<2>();
						TEST_FALSE(Download->WasSuccessful());
					}
				});

				It("should cancel if it had already started.", [this]()
				{
					DoTicksUntilCreated();
					DownloadService->RequestCancel(MadeRequestId);
					DoTicksUntilComplete(50, 1, 0.1f);
					TEST_EQUAL(FakeHttpModule->RxCreateRequest, 0);
					TEST_EQUAL(MockFileSystem->RxCreateFileReader.Num(), 1);
					TEST_EQUAL(RxDownloadComplete.Num(), 1);
					if (RxDownloadComplete.Num() == 1)
					{
						const FDownloadRef& Download = RxDownloadComplete[0].Get<2>();
						TEST_FALSE(Download->WasSuccessful());
					}
				});
			});

			Describe("when a HTTP request was made", [this]()
			{
				BeforeEach([this]()
				{
					FakeHttpModule->DataServed.FindOrAdd(HttpFileUrl).AddUninitialized(1024*1024*50);
					MadeRequestId = DownloadService->RequestFile(HttpFileUrl, DownloadComplete, DownloadProgress);
				});

				It("should cancel if it was not started yet.", [this]()
				{
					DownloadService->RequestCancel(MadeRequestId);
					DoTicksUntilComplete();
					TEST_EQUAL(FakeHttpModule->RxCreateRequest, 0);
					TEST_EQUAL(MockFileSystem->RxCreateFileReader.Num(), 0);
					TEST_EQUAL(RxDownloadComplete.Num(), 1);
					if (RxDownloadComplete.Num() == 1)
					{
						const FDownloadRef& Download = RxDownloadComplete[0].Get<2>();
						TEST_FALSE(Download->WasSuccessful());
					}
				});

				It("should cancel if it had already started.", [this]()
				{
					DoTicksUntilCreated();
					DownloadService->RequestCancel(MadeRequestId);
					DoTicksUntilComplete();
					TEST_EQUAL(FakeHttpModule->RxCreateRequest, 1);
					TEST_EQUAL(MockFileSystem->RxCreateFileReader.Num(), 0);
					TEST_EQUAL(RxDownloadComplete.Num(), 1);
					if (RxDownloadComplete.Num() == 1)
					{
						const FDownloadRef& Download = RxDownloadComplete[0].Get<2>();
						TEST_FALSE(Download->WasSuccessful());
					}
				});
			});
		});

		Describe("Destructor", [this]()
		{
			Describe("when there are currently active http requests", [this]()
			{
				It("should cancel the request.", [this]()
				{
					int32 RequestId = DownloadService->RequestFile(HttpFileUrl, DownloadComplete, DownloadProgress);
					DoTicksUntilCreated();
					DownloadService.Reset();
					DoTicksUntilComplete();
					TEST_EQUAL(FakeHttpModule->RxCreateRequest, 1);
					TEST_EQUAL(MockFileSystem->RxCreateFileReader.Num(), 0);
					TEST_EQUAL(RxDownloadComplete.Num(), 1);
					if (RxDownloadComplete.Num() == 1)
					{
						const FDownloadRef& Download = RxDownloadComplete[0].Get<2>();
						TEST_FALSE(Download->WasSuccessful());
					}
				});
			});
		});
	});

	AfterEach([this]()
	{
		DownloadService.Reset();
		RxDownloadProgress.Reset();
		RxDownloadComplete.Reset();
		FakeHttpModule.Reset();
		MockFileSystem.Reset();
		MockDownloadServiceStat.Reset();
		MockInstallerAnalytics.Reset();
	});
}

void FDownloadServiceSpec::DoTick()
{
	Ticker.Tick(0.1f);
}

void FDownloadServiceSpec::DoTicksUntilComplete(int32 TickCount, int32 CompleteCount, float Sleep)
{
	while (TickCount > 0 && RxDownloadComplete.Num() < CompleteCount)
	{
		DoTick();
		--TickCount;
		FPlatformProcess::Sleep(Sleep);
	}
}

void FDownloadServiceSpec::DoTicksUntilCreated(int32 TickCount, int32 CreateCount, float Sleep)
{
	MockFileSystem->ThreadLock.Lock();
	while (TickCount > 0 && (FakeHttpModule->RxCreateRequest + MockFileSystem->RxCreateFileReader.Num()) < CreateCount)
	{
		MockFileSystem->ThreadLock.Unlock();
		DoTick();
		--TickCount;
		FPlatformProcess::Sleep(Sleep);
		MockFileSystem->ThreadLock.Lock();
	}
	MockFileSystem->ThreadLock.Unlock();
}

#endif //WITH_DEV_AUTOMATION_TESTS
