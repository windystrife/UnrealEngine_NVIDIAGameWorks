// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPortalPackageInstaller.h"
#include "IMessageRpcClient.h"
#include "PortalPackageInstallerMessages.h"


class FPortalPackageInstallerProxy
	: public IPortalPackageInstaller
{
public:

	FPortalPackageInstallerProxy(const TSharedRef<IMessageRpcClient>& InRpc)
		: RpcClient(InRpc)
	{ }

	virtual ~FPortalPackageInstallerProxy() { }

public:

	virtual TAsyncResult<bool> Install(const FString& Path, const FString& AppName, const FString& BuildLabel) override
	{
		return RpcClient->Call<FPortalPackageInstallerInstall>(Path, AppName, BuildLabel);
	}

	virtual TAsyncResult<bool> Uninstall(const FString& Path, const FString& AppName, const FString& BuildLabel, bool RemoveUserFiles) override
	{
		return RpcClient->Call<FPortalPackageInstallerUninstall>(Path, AppName, BuildLabel, RemoveUserFiles);
	}

private:

	TSharedRef<IMessageRpcClient> RpcClient;
};
