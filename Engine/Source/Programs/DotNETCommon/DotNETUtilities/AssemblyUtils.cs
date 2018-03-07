// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;

namespace Tools.DotNETCommon
{
	public static class AssemblyUtils
	{
        /// <summary>
        /// Gets the original location (path and filename) of an assembly.
        /// This method is using Assembly.CodeBase property to properly resolve original
        /// assembly path in case shadow copying is enabled.
        /// </summary>
        /// <returns>Absolute path and filename to the assembly.</returns>
        public static string GetOriginalLocation(this Assembly ThisAssembly)
        {
            return new Uri(ThisAssembly.CodeBase).LocalPath;
        }
    
        /// <summary>
        /// Version info of the executable which runs this code.
        /// </summary>
        public static FileVersionInfo ExecutableVersion
        {
            get
            {
                return FileVersionInfo.GetVersionInfo(Assembly.GetEntryAssembly().GetOriginalLocation());
            }
        }

        /// <summary>
        /// Installs an assembly resolver. Mostly used to get shared assemblies that we don't want copied around to various output locations as happens when "Copy Local" is set to true
        /// for an assembly reference (which is the default).
        /// </summary>
        public static void InstallAssemblyResolver(string PathToBinariesDotNET)
        {
			AppDomain.CurrentDomain.AssemblyResolve += (sender, args) =>
            {
			    // Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
			    string AssemblyName = args.Name.Split(',')[0];

                // Look for known assembly names we check into Binaries/DotNET/. Return null if we can't find it.
                return (
                    from KnownAssemblyName in new[] { "RPCUtility.exe", "Ionic.Zip.Reduced.dll" }
                    where AssemblyName.Equals(Path.GetFileNameWithoutExtension(KnownAssemblyName), StringComparison.InvariantCultureIgnoreCase)
                    let ResolvedAssemblyFilename = Path.Combine(PathToBinariesDotNET, KnownAssemblyName)
                    // check if the file exists first. If we just try to load it, we correctly throw an exception, but it's a generic
                    // FileNotFoundException, which is not informative. Better to return null.
                    select File.Exists(ResolvedAssemblyFilename) ? Assembly.LoadFile(ResolvedAssemblyFilename) : null
                    ).FirstOrDefault();
            };
        }
    }
}
