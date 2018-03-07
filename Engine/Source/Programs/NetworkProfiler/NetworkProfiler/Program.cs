// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.



using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;

namespace NetworkProfiler
{
	static class Program
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main( string[] Args )
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);
			Application.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;
			Application.Run(new MainWindow());
		}
	}
}
