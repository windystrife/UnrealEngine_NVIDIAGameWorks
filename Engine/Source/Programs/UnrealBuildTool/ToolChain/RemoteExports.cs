using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public Remote toolchain functions exposed to UAT
	/// </summary>
	public class RemoteExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public static string ConvertPath(string Path)
		{
			return RemoteToolChain.ConvertPath(Path);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="RemotePath"></param>
		/// <returns></returns>
		public static string UnconvertPath(string RemotePath)
		{
			return RemoteToolChain.UnconvertPath(RemotePath);
		}
	}
}
