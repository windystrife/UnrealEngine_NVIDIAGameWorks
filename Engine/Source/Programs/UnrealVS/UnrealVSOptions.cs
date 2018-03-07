// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Shell;

namespace UnrealVS
{
	[Guid(GuidList.UnrealVSOptionsString)]
	public class UnrealVsOptions : DialogPage
	{
		public event EventHandler OnOptionsChanged;

		private bool _HideNonGameStartupProjects;

		[Category("General")]
		[DisplayName("Hide Non-Game Startup Projects")]
		[Description("Shows only game projects in the startup project and batch-builder lists")]
		public bool HideNonGameStartupProjects
		{
			get { return _HideNonGameStartupProjects; }
			set { _HideNonGameStartupProjects = value; }
		}

		protected override void OnApply(PageApplyEventArgs e)
		{
			base.OnApply(e);

			if (e.ApplyBehavior == ApplyKind.Apply && OnOptionsChanged != null)
			{
				OnOptionsChanged(this, EventArgs.Empty);
			}
		}
	}
}
