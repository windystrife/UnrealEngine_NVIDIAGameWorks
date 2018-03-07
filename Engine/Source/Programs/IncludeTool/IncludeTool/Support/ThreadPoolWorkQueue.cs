// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace IncludeTool.Support
{
	/// <summary>
	/// Interface for a work item that can be scheduled on a ThreadPoolWorkQueue
	/// </summary>
	interface IThreadPoolWorker
	{
		/// <summary>
		/// Execute the job. The given queue parameter can be used to schedule new jobs.
		/// </summary>
		/// <param name="Queue">The queue executing the job</param>
		void Run(ThreadPoolWorkQueue Queue);
	}

	/// <summary>
	/// Allows queuing a large number of tasks to a thread pool, and waiting for them all to complete.
	/// </summary>
	class ThreadPoolWorkQueue : IDisposable
	{
		/// <summary>
		/// The number of jobs which have been enqueued
		/// </summary>
		int NumOutstandingJobs;

		/// <summary>
		/// Event which is signaled when the queue is empty
		/// </summary>
		ManualResetEvent EmptyEvent = new ManualResetEvent(true);

		/// <summary>
		/// Default constructor
		/// </summary>
		public ThreadPoolWorkQueue()
		{
		}

		/// <summary>
		/// Dispose of the queue
		/// </summary>
		public void Dispose()
		{
			if(EmptyEvent != null)
			{
				WaitUntilComplete();
				EmptyEvent.Dispose();
				EmptyEvent = null;
			}
		}

		/// <summary>
		/// Enqueue an item of 
		/// </summary>
		/// <param name="Worker"></param>
		public void Enqueue(IThreadPoolWorker Worker)
		{
			if (Interlocked.Increment(ref NumOutstandingJobs) == 1)
			{
				EmptyEvent.Reset();
			}
			ThreadPool.QueueUserWorkItem(Execute, Worker);
		}

		/// <summary>
		/// Callback function to execute a single work item
		/// </summary>
		/// <param name="Worker">The IThreadPoolWorker object</param>
		void Execute(object Worker)
		{
			((IThreadPoolWorker)Worker).Run(this);

			if (Interlocked.Decrement(ref NumOutstandingJobs) == 0)
			{
				EmptyEvent.Set();
			}
		}

		/// <summary>
		/// Wait until the work queue is complete
		/// </summary>
		public void WaitUntilComplete()
		{
			EmptyEvent.WaitOne();
		}
	}
}
