// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"

namespace BuildPatchServices
{
	/**
	 * Helper functions for wrapping async functionality.
	 */
	namespace AsyncHelpers
	{
		template<typename ResultType, typename... Args>
		static TFunction<void()> MakePromiseKeeper(const TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe>& Promise, const TFunction<ResultType(Args...)>& Function, Args... FuncArgs)
		{
			return [Promise, Function, FuncArgs...]()
			{
				Promise->SetValue(Function(FuncArgs...));
			};
		}

		template<typename... Args>
		static TFunction<void()> MakePromiseKeeper(const TSharedRef<TPromise<void>, ESPMode::ThreadSafe>& Promise, const TFunction<void(Args...)>& Function, Args... FuncArgs)
		{
			return [Promise, Function, FuncArgs...]()
			{
				Function(FuncArgs...);
				Promise->SetValue();
			};
		}

		template<typename ResultType, typename... Args>
		static TFuture<ResultType> ExecuteOnGameThread(const TFunction<ResultType(Args...)>& Function, Args... FuncArgs)
		{
			TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<ResultType>());
			TFunction<void()> PromiseKeeper = MakePromiseKeeper(Promise, Function, FuncArgs...);
			if (!IsInGameThread())
			{
				AsyncTask(ENamedThreads::GameThread, MoveTemp(PromiseKeeper));
			}
			else
			{
				PromiseKeeper();
			}
			return Promise->GetFuture();
		}

		template<typename ResultType>
		static TFuture<ResultType> ExecuteOnGameThread(const TFunction<ResultType()>& Function)
		{
			TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<ResultType>());
			TFunction<void()> PromiseKeeper = MakePromiseKeeper(Promise, Function);
			if (!IsInGameThread())
			{
				AsyncTask(ENamedThreads::GameThread, MoveTemp(PromiseKeeper));
			}
			else
			{
				PromiseKeeper();
			}
			return Promise->GetFuture();
		}
	}
}
