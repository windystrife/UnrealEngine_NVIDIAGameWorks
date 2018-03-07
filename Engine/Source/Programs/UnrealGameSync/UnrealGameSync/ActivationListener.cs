// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class ActivationListener : IDisposable
	{
		Thread BackgroundThread;
		EventWaitHandle ActivateEventHandle;
		EventWaitHandle QuitEventHandle;

		public event Action OnActivate;

		public ActivationListener(EventWaitHandle InActivateEventHandle)
		{
			ActivateEventHandle = InActivateEventHandle;
			QuitEventHandle = new EventWaitHandle(false, EventResetMode.ManualReset);
		}

		public void Start()
		{
			if(BackgroundThread == null)
			{
				BackgroundThread = new Thread(x => ThreadProc());
				BackgroundThread.Start();
			}
		}

		public void Stop()
		{
			if(BackgroundThread != null)
			{
				QuitEventHandle.Set();

				BackgroundThread.Join();
				BackgroundThread = null;
			}
		}

		public void Dispose()
		{
			Stop();

			if(QuitEventHandle != null)
			{
				QuitEventHandle.Dispose();
				QuitEventHandle = null;
			}
			if(ActivateEventHandle != null)
			{
				ActivateEventHandle.Dispose();
				ActivateEventHandle = null;
			}
		}

		void ThreadProc()
		{
			for(;;)
			{
				int Index = EventWaitHandle.WaitAny(new WaitHandle[]{ ActivateEventHandle, QuitEventHandle }, Timeout.Infinite);
				if(Index == 0)
				{
					OnActivate();
				}
				else
				{
					break;
				}
			}
		}
	}
}
