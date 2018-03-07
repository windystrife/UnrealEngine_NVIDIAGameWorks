using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// A template for creating a shared PCH. Instances of it are created depending on the configurations required.
	/// </summary>
	class PrecompiledHeaderTemplate
	{
		/// <summary>
		/// Module providing this PCH.
		/// </summary>
		public UEBuildModuleCPP Module;

		/// <summary>
		/// The base compile environment, including all the public compile environment that all consuming modules inherit.
		/// </summary>
		public CppCompileEnvironment BaseCompileEnvironment;

		/// <summary>
		/// The header file 
		/// </summary>
		public FileItem HeaderFile;

		/// <summary>
		/// Output directory for instances of this PCH.
		/// </summary>
		public DirectoryReference OutputDir;

		/// <summary>
		/// Instances of this PCH
		/// </summary>
		public List<PrecompiledHeaderInstance> Instances = new List<PrecompiledHeaderInstance>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Module">The module with a valid shared PCH</param>
		/// <param name="BaseCompileEnvironment">The compile environment to use</param>
		/// <param name="HeaderFile">The header file to generate a PCH from</param>
		/// <param name="OutputDir">Output directory for instances of this PCH</param>
		public PrecompiledHeaderTemplate(UEBuildModuleCPP Module, CppCompileEnvironment BaseCompileEnvironment, FileItem HeaderFile, DirectoryReference OutputDir)
		{
			this.Module = Module;
			this.BaseCompileEnvironment = BaseCompileEnvironment;
			this.HeaderFile = HeaderFile;
			this.OutputDir = OutputDir;
		}

		/// <summary>
		/// Checks whether this template is valid for the given compile environment
		/// </summary>
		/// <param name="CompileEnvironment">Compile environment to check with</param>
		/// <returns>True if the template is compatible with the given compile environment</returns>
		public bool IsValidFor(CppCompileEnvironment CompileEnvironment)
		{
			if(CompileEnvironment.bIsBuildingDLL != BaseCompileEnvironment.bIsBuildingDLL)
			{
				return false;
			}
			if(CompileEnvironment.bIsBuildingLibrary != BaseCompileEnvironment.bIsBuildingLibrary)
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Return a string representation of this object for debugging
		/// </summary>
		/// <returns>String representation of the object</returns>
		public override string ToString()
		{
			return HeaderFile.AbsolutePath;
		}
	}
}
