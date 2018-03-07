/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace MemoryProfiler2
{
	/// <summary> A panel control that doesn't flicker when repainted. </summary>
    public class DoubleBufferedPanel : Panel
    {
        public DoubleBufferedPanel()
        {
            // Set the value of the double-buffering style bits to true.
            SetStyle(ControlStyles.DoubleBuffer | ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint, true);

            UpdateStyles();
        }
    }
}
