using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public Mac functions exposed to UAT
	/// </summary>
	public class MacExports
	{
		/// <summary>
		/// Strips symbols from a file
		/// </summary>
		/// <param name="SourceFile">The input file</param>
		/// <param name="TargetFile">The output file</param>
		public static void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			MacToolChain ToolChain = new MacToolChain(null, MacToolChainOptions.None);
			ToolChain.StripSymbols(SourceFile, TargetFile);
		}
	}
}
