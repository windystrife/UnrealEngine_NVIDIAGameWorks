// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
	/// <summary>
	/// Used to pass information to tasks about the currently running job.
	/// </summary>
	public class JobContext
	{
		/// <summary>
		/// The command that is running the current job.
		/// </summary>
		public readonly BuildCommand OwnerCommand;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InOwnerCommand">The command running the current job</param>
		public JobContext(BuildCommand InOwnerCommand)
		{
			OwnerCommand = InOwnerCommand;
		}
	}
}
