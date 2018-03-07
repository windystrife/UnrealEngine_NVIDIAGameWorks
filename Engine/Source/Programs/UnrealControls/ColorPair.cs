// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;

namespace UnrealControls
{
	/// <summary>
	/// A pair of colors.
	/// </summary>
	public struct ColorPair
	{
		/// <summary>
		/// The foreground color.
		/// </summary>
		public Color? BackColor;
		/// <summary>
		/// The background color
		/// </summary>
		public Color? ForeColor;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="BackColor">The background color.</param>
		/// <param name="ForeColor">The foreground color.</param>
		public ColorPair(Color? BackColor, Color? ForeColor)
		{
			this.BackColor = BackColor;
			this.ForeColor = ForeColor;
		}
	}

}
