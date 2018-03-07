// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Common/HttpManager.h"
#include "HttpModule.h"

namespace BuildPatchServices
{
	class FHttpManager
		: public IHttpManager
	{
	public:
		FHttpManager();
		~FHttpManager();

		// IHttpManager interface begin.
		virtual TSharedRef<IHttpRequest> CreateRequest() override;
		// IHttpManager interface end.

	private:
		FHttpModule& HttpModule;
	};

	FHttpManager::FHttpManager()
		: HttpModule(FHttpModule::Get())
	{
	}

	FHttpManager::~FHttpManager()
	{
	}

	TSharedRef<IHttpRequest> FHttpManager::CreateRequest()
	{
		return HttpModule.CreateRequest();
	}

	IHttpManager* FHttpManagerFactory::Create()
	{
		return new FHttpManager();
	}
}