using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Class to display an incrementing progress percentage. Handles progress markup and direct console output.
	/// </summary>
	public class ProgressWriter : IDisposable
	{
		/// <summary>
		/// Global setting controlling whether to output markup
		/// </summary>
		public static bool bWriteMarkup = false;

		/// <summary>
		/// Whether to write messages to the console
		/// </summary>
		bool bWriteToConsole;
		string Message;
		int NumCharsToBackspaceOver;
		string CurrentProgressString;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InMessage">The message to display before the progress percentage</param>
		/// <param name="bInWriteToConsole">Whether to write progress message to the console</param>
		public ProgressWriter(string InMessage, bool bInWriteToConsole)
		{
			Message = InMessage;
			bWriteToConsole = bInWriteToConsole;
			if (!bWriteMarkup && bWriteToConsole)
			{
				Console.Write(Message + " ");
			}
			Write(0, 100);
		}

		/// <summary>
		/// Write the terminating newline
		/// </summary>
		public void Dispose()
		{
			if (!bWriteMarkup && bWriteToConsole)
			{
				Console.WriteLine();
			}
		}

		/// <summary>
		/// Writes the current progress
		/// </summary>
		/// <param name="Numerator">Numerator for the progress fraction</param>
		/// <param name="Denominator">Denominator for the progress fraction</param>
		public void Write(int Numerator, int Denominator)
		{
			float ProgressValue = Denominator > 0 ? ((float)Numerator / (float)Denominator) : 1.0f;
			string ProgressString = String.Format("{0}%", Math.Round(ProgressValue * 100.0f));
			if (ProgressString != CurrentProgressString)
			{
				CurrentProgressString = ProgressString;
				if (bWriteMarkup)
				{
					Log.WriteLine(LogEventType.Console, "@progress '{0}' {1}", Message, ProgressString);
				}
				else if (bWriteToConsole)
				{
					// Backspace over previous progress value
					while (NumCharsToBackspaceOver-- > 0)
					{
						Console.Write("\b");
					}

					// Display updated progress string and keep track of how long it was
					NumCharsToBackspaceOver = ProgressString.Length;
					Console.Write(ProgressString);
				}
			}
		}
	}
}
