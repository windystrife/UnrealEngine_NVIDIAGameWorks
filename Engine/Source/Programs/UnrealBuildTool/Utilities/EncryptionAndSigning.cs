using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Helper functions for dealing with encryption and pak signing
	/// </summary>
	public static class EncryptionAndSigning
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="InDirectory"></param>
		/// <param name="InTargetPlatform"></param>
		/// <param name="OutRSAKeys"></param>
		/// <param name="OutAESKey"></param>
		public static void ParseEncryptionIni(DirectoryReference InDirectory, UnrealTargetPlatform InTargetPlatform, out String[] OutRSAKeys, out String OutAESKey)
		{
			OutAESKey = String.Empty;
			OutRSAKeys = null;

			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Encryption, InDirectory, InTargetPlatform);

			bool bSigningEnabled;
			Ini.GetBool("Core.Encryption", "SignPak", out bSigningEnabled);

			if (bSigningEnabled)
			{
				OutRSAKeys = new string[3];
				Ini.GetString("Core.Encryption", "rsa.privateexp", out OutRSAKeys[0]);
				Ini.GetString("Core.Encryption", "rsa.modulus", out OutRSAKeys[1]);
				Ini.GetString("Core.Encryption", "rsa.publicexp", out OutRSAKeys[2]);

				if (String.IsNullOrEmpty(OutRSAKeys[0]) || String.IsNullOrEmpty(OutRSAKeys[1]) || String.IsNullOrEmpty(OutRSAKeys[2]))
				{
					OutRSAKeys = null;
				}
			}

			bool bEncryptionEnabled;
			Ini.GetBool("Core.Encryption", "EncryptPak", out bEncryptionEnabled);

			if (bEncryptionEnabled)
			{
				Ini.GetString("Core.Encryption", "aes.key", out OutAESKey);
			}
		}
	}
}
