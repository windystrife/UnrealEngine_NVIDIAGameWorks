using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace AutomationTool
{
	/// <summary>
	/// Allows queuing a large number of tasks to a thread pool and waiting for them all to complete.
	/// </summary>
	public class ThreadPoolWorkQueue : IDisposable
	{
		/// <summary>
		/// Number of jobs remaining in the queue. This is updated in an atomic way.
		/// </summary>
		int NumOutstandingJobs;

		/// <summary>
		/// Event which indicates whether the queue is empty.
		/// </summary>
		ManualResetEvent EmptyEvent = new ManualResetEvent(true);

		/// <summary>
		/// Default constructor
		/// </summary>
		public ThreadPoolWorkQueue()
		{
		}

		/// <summary>
		/// Waits for the queue to drain, and disposes of it
		/// </summary>
		public void Dispose()
		{
			if(EmptyEvent != null)
			{
				Wait();

				EmptyEvent.Dispose();
				EmptyEvent = null;
			}
		}

		/// <summary>
		/// Returns the number of items remaining in the queue
		/// </summary>
		public int NumRemaining
		{
			get { return NumOutstandingJobs; }
		}

		/// <summary>
		/// Adds an item to the queue
		/// </summary>
		/// <param name="ActionToExecute">The action to add</param>
		public void Enqueue(Action ActionToExecute)
		{
			if(Interlocked.Increment(ref NumOutstandingJobs) == 1)
			{
				EmptyEvent.Reset();
			}
			ThreadPool.QueueUserWorkItem(Execute, ActionToExecute);
		}

		/// <summary>
		/// Internal method to execute an action
		/// </summary>
		/// <param name="ActionToExecute">The action to execute</param>
		void Execute(object ActionToExecute)
		{
			((Action)ActionToExecute)();
			if(Interlocked.Decrement(ref NumOutstandingJobs) == 0)
			{
				EmptyEvent.Set();
			}
		}

		/// <summary>
		/// Waits for all queued tasks to finish
		/// </summary>
		public void Wait()
		{
			EmptyEvent.WaitOne();
		}

		/// <summary>
		/// Waits for all queued tasks to finish, or the timeout to elapse
		/// </summary>
		/// <param name="MillisecondsTimeout">Maximum time to wait</param>
		/// <returns>True if the queue completed, false if the timeout elapsed</returns>
		public bool Wait(int MillisecondsTimeout)
		{
			return EmptyEvent.WaitOne(MillisecondsTimeout);
		}
	}
}
