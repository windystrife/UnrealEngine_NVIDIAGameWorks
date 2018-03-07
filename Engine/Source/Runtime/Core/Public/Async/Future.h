// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/Function.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "Misc/DateTime.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

template<typename ResultType> class TSharedFuture;

/* TFutureBase
*****************************************************************************/

/**
 * Base class for the internal state of asynchronous return values (futures).
 */
class FFutureState
{
public:

	/** Default constructor. */
	FFutureState()
		: CompletionEvent(FPlatformProcess::GetSynchEventFromPool(true))
		, Complete(false)
	{ }

	/**
	 * Create and initialize a new instance with a callback.
	 *
	 * @param InCompletionCallback A function that is called when the state is completed.
	 */
	FFutureState(TFunction<void()>&& InCompletionCallback)
		: CompletionCallback(InCompletionCallback)
		, CompletionEvent(FPlatformProcess::GetSynchEventFromPool(true))
		, Complete(false)
	{ }

	/** Destructor. */
	~FFutureState()
	{
		FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
		CompletionEvent = nullptr;
	}

public:

	/**
	 * Checks whether the asynchronous result has been set.
	 *
	 * @return true if the result has been set, false otherwise.
	 * @see WaitFor
	 */
	bool IsComplete() const
	{
		return Complete;
	}

	/**
	 * Blocks the calling thread until the future result is available.
	 *
	 * @param Duration The maximum time span to wait for the future result.
	 * @return true if the result is available, false otherwise.
	 * @see IsComplete
	 */
	bool WaitFor(const FTimespan& Duration) const
	{
		if (CompletionEvent->Wait(Duration))
		{
			return true;
		}

		return false;
	}

protected:

	/** Notifies any waiting threads that the result is available. */
	void MarkComplete()
	{
		Complete = true;
		CompletionEvent->Trigger();

		if (CompletionCallback)
		{
			CompletionCallback();
		}
	}

private:

	/** An optional callback function that is executed the state is completed. */
	TFunction<void()> CompletionCallback;

	/** Holds an event signaling that the result is available. */
	FEvent* CompletionEvent;

	/** Whether the asynchronous result is available. */
	bool Complete;
};


/**
 * Implements the internal state of asynchronous return values (futures).
 */
template<typename InternalResultType>
class TFutureState
	: public FFutureState
{
public:

	/** Default constructor. */
	TFutureState()
		: FFutureState()
	{ }

	/**
	 * Create and initialize a new instance with a callback.
	 *
	 * @param CompletionCallback A function that is called when the state is completed.
	 */
	TFutureState(TFunction<void()>&& CompletionCallback)
		: FFutureState(MoveTemp(CompletionCallback))
	{ }

public:

	/**
	 * Gets the result (will block the calling thread until the result is available).
	 *
	 * @return The result value.
	 * @see SetResult
	 */
	const InternalResultType& GetResult() const
	{
		while (!IsComplete())
		{
			WaitFor(FTimespan::MaxValue());
		}

		return Result;
	}

	/**
	 * Sets the result and notifies any waiting threads.
	 *
	 * @param InResult The result to set.
	 * @see GetResult
	 */
	void SetResult(const InternalResultType& InResult)
	{
		check(!IsComplete());

		Result = InResult;
		MarkComplete();
	}

	/**
	 * Sets the result and notifies any waiting threads (from rvalue).
	 *
	 * @param InResult The result to set.
	 * @see GetResult
	 */
	void SetResult(InternalResultType&& InResult)
	{
		check(!IsComplete());

		Result = InResult;
		MarkComplete();
	}

private:

	/** Holds the asynchronous result. */
	InternalResultType Result;
};


/**
 * Abstract base template for futures and shared futures.
 */
template<typename InternalResultType>
class TFutureBase
{
public:

	/**
	 * Checks whether this future object has its value set.
	 *
	 * @return true if this future has a shared state and the value has been set, false otherwise.
	 * @see IsValid
	 */
	bool IsReady() const
	{
		return State.IsValid() ? State->IsComplete() : false;
	}

	/**
	 * Checks whether this future object has a valid state.
	 *
	 * @return true if the state is valid, false otherwise.
	 * @see IsReady
	 */
	bool IsValid() const
	{
		return State.IsValid();
	}

	/**
	 * Blocks the calling thread until the future result is available.
	 *
	 * Note that this method may block forever if the result is never set. Use
	 * the WaitFor or WaitUntil methods to specify a maximum timeout for the wait.
	 *
	 * @see WaitFor, WaitUntil
	 */
	void Wait() const
	{
		while (!WaitFor(FTimespan::MaxValue()));
	}

	/**
	 * Blocks the calling thread until the future result is available or the specified duration is exceeded.
	 *
	 * @param Duration The maximum time span to wait for the future result.
	 * @return true if the result is available, false otherwise.
	 * @see Wait, WaitUntil
	 */
	bool WaitFor(const FTimespan& Duration) const
	{
		return State.IsValid() ? State->WaitFor(Duration) : false;
	}

	/**
	 * Blocks the calling thread until the future result is available or the specified time is hit.
	 *
	 * @param Time The time until to wait for the future result (in UTC).
	 * @return true if the result is available, false otherwise.
	 * @see Wait, WaitUntil
	 */
	bool WaitUntil(const FDateTime& Time) const
	{
		return WaitFor(Time - FDateTime::UtcNow());
	}

protected:

	typedef TSharedPtr<TFutureState<InternalResultType>, ESPMode::ThreadSafe> StateType;

	/** Default constructor. */
	TFutureBase() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InState The shared state to initialize with.
	 */
	TFutureBase(const StateType& InState)
		: State(InState)
	{ }

	/**
	 * Protected move construction
	 */
	TFutureBase(TFutureBase&&) = default;

	/**
	 * Protected move assignment
	 */
	TFutureBase& operator=(TFutureBase&&) = default;

	/**
	 * Protected copy construction
	 */
	TFutureBase(const TFutureBase&) = default;

	/**
	 * Protected copy assignment
	 */
	TFutureBase& operator=(const TFutureBase&) = default;

	/** Protected destructor. */
	~TFutureBase() { }

protected:

	/**
	 * Gets the shared state object.
	 *
	 * @return The shared state object.
	 */
	const StateType& GetState() const
	{
		// if you hit this assertion then your future has an invalid state.
		// this happens if you have an uninitialized future or if you moved
		// it to another instance.
		check(State.IsValid());

		return State;
	}

private:

	/** Holds the future's state. */
	StateType State;
};


/* TFuture
*****************************************************************************/

template<typename ResultType> class TSharedFuture;


/**
 * Template for unshared futures.
 */
template<typename ResultType>
class TFuture
	: public TFutureBase<ResultType>
{
	typedef TFutureBase<ResultType> BaseType;

public:

	/** Default constructor. */
	TFuture() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InState The shared state to initialize with.
	 */
	TFuture(const typename BaseType::StateType& InState)
		: BaseType(InState)
	{ }

	/**
	 * Move constructor.
	 */
	TFuture(TFuture&&) = default;

	/**
	 * Move assignment operator.
	 */
	TFuture& operator=(TFuture&& Other) = default;

	/** Destructor. */
	~TFuture() { }

public:

	/**
	 * Gets the future's result.
	 *
	 * @return The result.
	 */
	ResultType Get() const
	{
		return this->GetState()->GetResult();
	}

	/**
	 * Moves this future's state into a shared future.
	 *
	 * @return The shared future object.
	 */
	TSharedFuture<ResultType> Share()
	{
		return TSharedFuture<ResultType>(MoveTemp(*this));
	}

private:

	/** Hidden copy constructor (futures cannot be copied). */
	TFuture(const TFuture&);

	/** Hidden copy assignment (futures cannot be copied). */
	TFuture& operator=(const TFuture&);
};


/**
 * Template for unshared futures (specialization for reference types).
 */
template<typename ResultType>
class TFuture<ResultType&>
	: public TFutureBase<ResultType*>
{
	typedef TFutureBase<ResultType*> BaseType;

public:

	/** Default constructor. */
	TFuture() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InState The shared state to initialize with.
	 */
	TFuture(const typename BaseType::StateType& InState)
		: BaseType(InState)
	{ }

	/**
	 * Move constructor.
	 */
	TFuture(TFuture&&) = default;

	/**
	 * Move assignment operator.
	 */
	TFuture& operator=(TFuture&& Other) = default;

	/** Destructor. */
	~TFuture() { }

public:

	/**
	 * Gets the future's result.
	 *
	 * @return The result.
	 */
	ResultType& Get() const
	{
		return *this->GetState()->GetResult();
	}

	/**
	 * Moves this future's state into a shared future.
	 *
	 * @return The shared future object.
	 */
	TSharedFuture<ResultType&> Share()
	{
		return TSharedFuture<ResultType&>(MoveTemp(*this));
	}

private:

	/** Hidden copy constructor (futures cannot be copied). */
	TFuture(const TFuture&);

	/** Hidden copy assignment (futures cannot be copied). */
	TFuture& operator=(const TFuture&);
};


/**
 * Template for unshared futures (specialization for void).
 */
template<>
class TFuture<void>
	: public TFutureBase<int>
{
	typedef TFutureBase<int> BaseType;

public:

	/** Default constructor. */
	TFuture() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InState The shared state to initialize with.
	 */
	TFuture(const BaseType::StateType& InState)
		: BaseType(InState)
	{ }

	/**
	 * Move constructor.
	 */
	TFuture(TFuture&&) = default;

	/**
	 * Move assignment operator.
	 */
	TFuture& operator=(TFuture&& Other) = default;

	/** Destructor. */
	~TFuture() { }

public:

	/**
	 * Gets the future's result.
	 *
	 * @return The result.
	 */
	void Get() const
	{
		GetState()->GetResult();
	}

	/**
	 * Moves this future's state into a shared future.
	 *
	 * @return The shared future object.
	 */
	TSharedFuture<void> Share();

private:

	/** Hidden copy constructor (futures cannot be copied). */
	TFuture(const TFuture&);

	/** Hidden copy assignment (futures cannot be copied). */
	TFuture& operator=(const TFuture&);
};


/* TSharedFuture
*****************************************************************************/

/**
 * Template for shared futures.
 */
template<typename ResultType>
class TSharedFuture
	: public TFutureBase<ResultType>
{
	typedef TFutureBase<ResultType> BaseType;

public:

	/** Default constructor. */
	TSharedFuture() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InState The shared state to initialize from.
	 */
	TSharedFuture(const typename BaseType::StateType& InState)
		: BaseType(InState)
	{ }

	/**
	 * Creates and initializes a new instances from a future object.
	 *
	 * @param Future The future object to initialize from.
	 */
	TSharedFuture(TFuture<ResultType>&& Future)
		: BaseType(MoveTemp(Future))
	{ }

	/**
	 * Copy constructor.
	 */
	TSharedFuture(const TSharedFuture&) = default;

	/**
	 * Copy assignment operator.
	 */
	TSharedFuture& operator=(const TSharedFuture& Other) = default;

	/**
	 * Move constructor.
	 */
	TSharedFuture(TSharedFuture&&) = default;

	/**
	 * Move assignment operator.
	 */
	TSharedFuture& operator=(TSharedFuture&& Other) = default;

	/** Destructor. */
	~TSharedFuture() { }

public:

	/**
	 * Gets the future's result.
	 *
	 * @return The result.
	 */
	ResultType Get() const
	{
		return this->GetState()->GetResult();
	}
};


/**
 * Template for shared futures (specialization for reference types).
 */
template<typename ResultType>
class TSharedFuture<ResultType&>
	: public TFutureBase<ResultType*>
{
	typedef TFutureBase<ResultType*> BaseType;

public:

	/** Default constructor. */
	TSharedFuture() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InState The shared state to initialize from.
	 */
	TSharedFuture(const typename BaseType::StateType& InState)
		: BaseType(InState)
	{ }

	/**
	* Creates and initializes a new instances from a future object.
	*
	* @param Future The future object to initialize from.
	*/
	TSharedFuture(TFuture<ResultType>&& Future)
		: BaseType(MoveTemp(Future))
	{ }

	/**
	 * Copy constructor.
	 */
	TSharedFuture(const TSharedFuture&) = default;

	/** Copy assignment operator. */
	TSharedFuture& operator=(const TSharedFuture& Other) = default;

	/**
	 * Move constructor.
	 */
	TSharedFuture(TSharedFuture&&) = default;

	/**
	 * Move assignment operator.
	 */
	TSharedFuture& operator=(TSharedFuture&& Other) = default;

	/** Destructor. */
	~TSharedFuture() { }

public:

	/**
	 * Gets the future's result.
	 *
	 * @return The result.
	 */
	ResultType& Get() const
	{
		return *this->GetState()->GetResult();
	}
};


/**
 * Template for shared futures (specialization for void).
 */
template<>
class TSharedFuture<void>
	: public TFutureBase<int>
{
	typedef TFutureBase<int> BaseType;

public:

	/** Default constructor. */
	TSharedFuture() { }

	/**
	 * Creates and initializes a new instance from shared state.
	 *
	 * @param InState The shared state to initialize from.
	 */
	TSharedFuture(const BaseType::StateType& InState)
		: BaseType(InState)
	{ }

	/**
	 * Creates and initializes a new instances from a future object.
	 *
	 * @param Future The future object to initialize from.
	 */
	TSharedFuture(TFuture<void>&& Future)
		: BaseType(MoveTemp(Future))
	{ }

	/**
	 * Copy constructor.
	 */
	TSharedFuture(const TSharedFuture&) = default;

	/**
	 * Copy assignment operator.
	 */
	TSharedFuture& operator=(const TSharedFuture& Other) = default;

	/**
	 * Move constructor.
	 */
	TSharedFuture(TSharedFuture&&) = default;

	/**
	 * Move assignment operator.
	 */
	TSharedFuture& operator=(TSharedFuture&& Other) = default;

	/** Destructor. */
	~TSharedFuture() { }

public:

	/**
	 * Gets the future's result.
	 *
	 * @return The result.
	 */
	void Get() const
	{
		GetState()->GetResult();
	}
};


inline TSharedFuture<void> TFuture<void>::Share()
{
	return TSharedFuture<void>(MoveTemp(*this));
}


/* TPromise
*****************************************************************************/

template<typename InternalResultType>
class TPromiseBase
	: FNoncopyable
{
	typedef TSharedPtr<TFutureState<InternalResultType>, ESPMode::ThreadSafe> StateType;

public:

	/** Default constructor. */
	TPromiseBase()
		: State(MakeShareable(new TFutureState<InternalResultType>))
	{ }

	/**
	 * Move constructor.
	 *
	 * @param Other The promise holding the shared state to move.
	 */
	TPromiseBase(TPromiseBase&& Other)
		: State(MoveTemp(Other.State))
	{
		Other.State.Reset();
	}

	/**
	 * Create and initialize a new instance with a callback.
	 *
	 * @param CompletionCallback A function that is called when the future state is completed.
	 */
	TPromiseBase(TFunction<void()>&& CompletionCallback)
		: State(MakeShareable(new TFutureState<InternalResultType>(MoveTemp(CompletionCallback))))
	{ }

public:

	/** Move assignment operator. */
	TPromiseBase& operator=(TPromiseBase&& Other)
	{
		State = Other.State;
		Other.State.Reset();
		return *this;
	}

protected:

	/** Destructor. */
	~TPromiseBase()
	{
		if (State.IsValid())
		{
			// if you hit this assertion then your promise never had its result
			// value set. broken promises are considered programming errors.
			check(State->IsComplete());
		}
	}

	/**
	 * Gets the shared state object.
	 *
	 * @return The shared state object.
	 */
	const StateType& GetState()
	{
		// if you hit this assertion then your promise has an invalid state.
		// this happens if you move the promise to another instance.
		check(State.IsValid());

		return State;
	}

private:

	/** Holds the shared state object. */
	StateType State;
};


/**
 * Template for promises.
 */
template<typename ResultType>
class TPromise
	: public TPromiseBase<ResultType>
{
public:

	typedef TPromiseBase<ResultType> BaseType;

	/** Default constructor (creates a new shared state). */
	TPromise()
		: BaseType()
		, FutureRetrieved(false)
	{ }

	/**
	 * Move constructor.
	 *
	 * @param Other The promise holding the shared state to move.
	 */
	TPromise(TPromise&& Other)
		: BaseType(MoveTemp(Other))
		, FutureRetrieved(MoveTemp(Other.FutureRetrieved))
	{ }

	/**
	 * Create and initialize a new instance with a callback.
	 *
	 * @param CompletionCallback A function that is called when the future state is completed.
	 */
	TPromise(TFunction<void()>&& CompletionCallback)
		: BaseType(MoveTemp(CompletionCallback))
		, FutureRetrieved(false)
	{ }

public:

	/**
	 * Move assignment operator.
	 *
	 * @param Other The promise holding the shared state to move.
	 */
	TPromise& operator=(TPromise&& Other)
	{
		BaseType::operator=(MoveTemp(Other));
		FutureRetrieved = MoveTemp(Other.FutureRetrieved);

		return *this;
	}

public:

	/**
	 * Gets a TFuture object associated with the shared state of this promise.
	 *
	 * @return The TFuture object.
	 */
	TFuture<ResultType> GetFuture()
	{
		check(!FutureRetrieved);
		FutureRetrieved = true;

		return TFuture<ResultType>(this->GetState());
	}

	/**
	 * Sets the promised result.
	 *
	 * The result must be set only once. An assertion will
	 * be triggered if this method is called a second time.
	 *
	 * @param Result The result value to set.
	 */
	void SetValue(const ResultType& Result)
	{
		this->GetState()->SetResult(Result);
	}

	/**
	 * Sets the promised result (from rvalue).
	 *
	 * The result must be set only once. An assertion will
	 * be triggered if this method is called a second time.
	 *
	 * @param Result The result value to set.
	 */
	void SetValue(ResultType&& Result)
	{
		this->GetState()->SetResult(MoveTemp(Result));
	}

private:

	/** Whether a future has already been retrieved from this promise. */
	bool FutureRetrieved;
};


/**
 * Template for promises (specialization for reference types).
 */
template<typename ResultType>
class TPromise<ResultType&>
	: public TPromiseBase<ResultType*>
{
	typedef TPromiseBase<ResultType*> BaseType;

public:

	/** Default constructor (creates a new shared state). */
	TPromise()
		: BaseType()
		, FutureRetrieved(false)
	{ }

	/**
	 * Move constructor.
	 *
	 * @param Other The promise holding the shared state to move.
	 */
	TPromise(TPromise&& Other)
		: BaseType(MoveTemp(Other))
		, FutureRetrieved(MoveTemp(Other.FutureRetrieved))
	{ }

	/**
	 * Create and initialize a new instance with a callback.
	 *
	 * @param CompletionCallback A function that is called when the future state is completed.
	 */
	TPromise(TFunction<void()>&& CompletionCallback)
		: BaseType(MoveTemp(CompletionCallback))
		, FutureRetrieved(false)
	{ }

public:

	/**
	 * Move assignment operator.
	 *
	 * @param Other The promise holding the shared state to move.
	 */
	TPromise& operator=(TPromise&& Other)
	{
		BaseType::operator=(MoveTemp(Other));
		FutureRetrieved = MoveTemp(Other.FutureRetrieved);

		return this;
	}

public:

	/**
	 * Gets a TFuture object associated with the shared state of this promise.
	 *
	 * @return The TFuture object.
	 */
	TFuture<ResultType&> GetFuture()
	{
		check(!FutureRetrieved);
		FutureRetrieved = true;

		return TFuture<ResultType&>(this->GetState());
	}

	/**
	 * Sets the promised result.
	 *
	 * The result must be set only once. An assertion will
	 * be triggered if this method is called a second time.
	 *
	 * @param Result The result value to set.
	 */
	void SetValue(ResultType& Result)
	{
		this->GetState()->SetResult(Result);
	}

private:

	/** Whether a future has already been retrieved from this promise. */
	bool FutureRetrieved;
};


/**
 * Template for promises (specialization for void results).
 */
template<>
class TPromise<void>
	: public TPromiseBase<int>
{
	typedef TPromiseBase<int> BaseType;

public:

	/** Default constructor (creates a new shared state). */
	TPromise()
		: BaseType()
		, FutureRetrieved(false)
	{ }

	/**
	 * Move constructor.
	 *
	 * @param Other The promise holding the shared state to move.
	 */
	TPromise(TPromise&& Other)
		: BaseType(MoveTemp(Other))
		, FutureRetrieved(false)
	{ }

	/**
	 * Create and initialize a new instance with a callback.
	 *
	 * @param CompletionCallback A function that is called when the future state is completed.
	 */
	TPromise(TFunction<void()>&& CompletionCallback)
		: BaseType(MoveTemp(CompletionCallback))
		, FutureRetrieved(false)
	{ }

public:

	/**
	 * Move assignment operator.
	 *
	 * @param Other The promise holding the shared state to move.
	 */
	TPromise& operator=(TPromise&& Other)
	{
		BaseType::operator=(MoveTemp(Other));
		FutureRetrieved = MoveTemp(Other.FutureRetrieved);

		return *this;
	}

public:

	/**
	 * Gets a TFuture object associated with the shared state of this promise.
	 *
	 * @return The TFuture object.
	 */
	TFuture<void> GetFuture()
	{
		check(!FutureRetrieved);
		FutureRetrieved = true;

		return TFuture<void>(GetState());
	}

	/**
	 * Sets the promised result.
	 *
	 * The result must be set only once. An assertion will
	 * be triggered if this method is called a second time.
	 */
	void SetValue()
	{
		GetState()->SetResult(0);
	}

private:

	/** Whether a future has already been retrieved from this promise. */
	bool FutureRetrieved;
};
