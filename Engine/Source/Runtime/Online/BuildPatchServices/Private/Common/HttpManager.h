// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IHttpRequest.h"

namespace BuildPatchServices
{
	/**
	 * The HTTP manager is used for classes which require HTTP access. Using this wrapper
	 * allows those classes to be easily testable.
	 */
	class IHttpManager
	{
	public:
		virtual ~IHttpManager() {}

		/**
		 * Instantiates a new Http request for the current platform
		 *
		 * @return new Http request instance
		 */
		virtual TSharedRef<IHttpRequest> CreateRequest() = 0;
	};

	/**
	 * A factory for creating an IHttpManager instance.
	 */
	class FHttpManagerFactory
	{
	public:
		/**
		 * Creates an implementation which wraps use of FHttpModule.
		 * @return the new IHttpManager instance created.
		 */
		static IHttpManager* Create();
	};
}