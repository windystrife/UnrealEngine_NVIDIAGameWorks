// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Windows.Forms;

namespace MemoryProfiler2
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
			Application.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;

            if (args.Length == 0)
            {
                Application.Run(new MainWindow());
            }
            else
            {
                Application.Run(new MainWindow(args[0]));
            }
        }
 
    }
}