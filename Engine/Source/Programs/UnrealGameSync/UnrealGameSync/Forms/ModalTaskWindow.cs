// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	interface IModalTask
	{
		bool Run(out string ErrorMessage);
	}

	partial class ModalTaskWindow : Form
	{
		string Title;
		string Message;
		Thread BackgroundThread;
		IModalTask Task;
		string ErrorMessage;
		bool bSucceeded;

		private ModalTaskWindow(IModalTask InTask, string InTitle, string InMessage)
		{
			Task = InTask;
			Title = InTitle;
			Message = InMessage;
			InitializeComponent();
		}

		public static bool Execute(IWin32Window Owner, IModalTask Task, string InTitle, string InMessage, out string ErrorMessage)
		{
			ModalTaskWindow Window = new ModalTaskWindow(Task, InTitle, InMessage);
			Window.ShowDialog(Owner);
			ErrorMessage = Window.ErrorMessage;
			return Window.bSucceeded;
		}

		private void OpenProjectWindow_Load(object sender, EventArgs e)
		{
			Text = Title;
			MessageLabel.Text = Message;

			BackgroundThread = new Thread(x => ThreadProc());
			BackgroundThread.Start();
		}

		private void OpenProjectWindow_FormClosing(object sender, FormClosingEventArgs e)
		{
			BackgroundThread.Abort();
			BackgroundThread.Join();
		}

		private void ThreadProc()
		{
			bSucceeded = Task.Run(out ErrorMessage);
			BeginInvoke(new MethodInvoker(Close));
		}
	}
}
