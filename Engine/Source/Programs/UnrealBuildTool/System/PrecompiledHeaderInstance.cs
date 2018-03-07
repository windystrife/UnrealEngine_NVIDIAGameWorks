using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Information about a PCH instance
	/// </summary>
	class PrecompiledHeaderInstance
	{
		/// <summary>
		/// The file to include to use this shared PCH
		/// </summary>
		public FileItem HeaderFile;

		/// <summary>
		/// Whether optimization is enabled
		/// </summary>
		public bool bOptimizeCode;

		/// <summary>
		/// Whether to enable RTTI
		/// </summary>
		public bool bUseRTTI;

		/// <summary>
		/// The output files for the shared PCH
		/// </summary>
		public CPPOutput Output;

		/// <summary>
		/// Constructor
		/// </summary>
		public PrecompiledHeaderInstance(FileItem HeaderFile, bool bOptimizeCode, bool bUseRTTI, CPPOutput Output)
		{
			this.HeaderFile = HeaderFile;
			this.bOptimizeCode = bOptimizeCode;
			this.bUseRTTI = bUseRTTI;
			this.Output = Output;
		}

		/// <summary>
		/// Return a string representation of this object for debugging
		/// </summary>
		/// <returns>String representation of the object</returns>
		public override string ToString()
		{
			return String.Format("{0} (Optimized={1}, RTTI={2})", HeaderFile.Reference.GetFileName(), bOptimizeCode, bUseRTTI);
		}
	}
}
