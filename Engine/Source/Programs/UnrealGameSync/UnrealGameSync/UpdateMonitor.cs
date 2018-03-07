// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class UpdateMonitor : IDisposable
	{
		PerforceConnection Perforce;
		string WatchPath;
		Thread WorkerThread;
		ManualResetEvent QuitEvent;

		public event Action OnUpdateAvailable;

		public UpdateMonitor(PerforceConnection InPerforce, string InWatchPath)
		{
			Perforce = InPerforce;
			WatchPath = InWatchPath;

			QuitEvent = new ManualResetEvent(false);

			if(WatchPath != null)
			{
				WorkerThread = new Thread(() => PollForUpdates());
				WorkerThread.Start();
			}
		}

		public void Close()
		{
			QuitEvent.Set();

			if(WorkerThread != null)
			{
				if(!WorkerThread.Join(30))
				{
					WorkerThread.Abort();
					WorkerThread.Join();
				}
				WorkerThread = null;
			}
		}

		public void Dispose()
		{
			Close();
		}

		public bool IsUpdateAvailable
		{
			get;
			private set;
		}

		void PollForUpdates()
		{
			while(!QuitEvent.WaitOne(5 * 60 * 1000))
			{
				StringWriter Log = new StringWriter();

				List<PerforceChangeSummary> Changes;
				if(Perforce.FindChanges(WatchPath, 1, out Changes, Log) && Changes.Count > 0)
				{
					TriggerUpdate();
				}
			}
		}

		public void TriggerUpdate()
		{
			IsUpdateAvailable = true;
			if(OnUpdateAvailable != null)
			{
				OnUpdateAvailable();
			}
		}
	}
}
