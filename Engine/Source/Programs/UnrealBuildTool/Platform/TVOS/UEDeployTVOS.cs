// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.IO;
using System.Diagnostics;
using System.Xml.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class UEDeployTVOS : UEDeployIOS
	{

        public UEDeployTVOS()
        {
        }

		protected override string GetTargetPlatformName()
		{
			return "TVOS";
		}

		public static bool GenerateTVOSPList(string ProjectDirectory, bool bIsUE4Game, string GameName, string ProjectName, string InEngineDir, string AppDirectory, UEDeployTVOS InThis = null)
		{
			// @todo tvos: THIS!


			// generate the Info.plist for future use
			string BuildDirectory = ProjectDirectory + "/Build/TVOS";
			bool bSkipDefaultPNGs = false;
			string IntermediateDirectory = (bIsUE4Game ? InEngineDir : ProjectDirectory) + "/Intermediate/TVOS";
			string PListFile = IntermediateDirectory + "/" + GameName + "-Info.plist";
			// @todo tvos: This is really nasty - both IOS and TVOS are setting static vars
			VersionUtilities.BuildDirectory = BuildDirectory;
			VersionUtilities.GameName = GameName;

			// read the old file
			string OldPListData = File.Exists(PListFile) ? File.ReadAllText(PListFile) : "";

			// determine if there is a launch.xib
			string LaunchXib = InEngineDir + "/Build/IOS/Resources/Interface/LaunchScreen.xib";
			if (File.Exists(BuildDirectory + "/Resources/Interface/LaunchScreen.xib"))
			{
				LaunchXib = BuildDirectory + "/Resources/Interface/LaunchScreen.xib";
			}

			// get the settings from the ini file
			// plist replacements
			// @todo tvos: Are we going to make TVOS specific .ini files?
			DirectoryReference DirRef = bIsUE4Game ? (!string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()) ? new DirectoryReference(UnrealBuildTool.GetRemoteIniPath()) : null) : new DirectoryReference(ProjectDirectory);
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirRef, UnrealTargetPlatform.IOS);

			// bundle display name
			string BundleDisplayName;
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleDisplayName", out BundleDisplayName);

			// bundle identifier
			string BundleIdentifier;
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleIdentifier", out BundleIdentifier);

			// bundle name
			string BundleName;
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleName", out BundleName);

			// short version string
			string BundleShortVersion;
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "VersionInfo", out BundleShortVersion);

			// required capabilities
			string RequiredCaps = "\t\t<string>arm64</string>\n";

			// minimum iOS version
			string MinVersion;
			if (Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MinimumTVOSVersion", out MinVersion))
			{
				switch (MinVersion)
				{
					case "TVOS_9":
						MinVersion = "9.0";
						break;
				}
			}
			else
			{
				MinVersion = "9.0";
			}

			// extra plist data
			string ExtraData = "";
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalPlistData", out ExtraData);

            // create the final display name, including converting all entities for XML use
            string FinalDisplayName = BundleDisplayName.Replace("[PROJECT_NAME]", ProjectName).Replace("_", "");
            FinalDisplayName = FinalDisplayName.Replace("&", "&amp;");
            FinalDisplayName = FinalDisplayName.Replace("\"", "&quot;");
            FinalDisplayName = FinalDisplayName.Replace("\'", "&apos;");
            FinalDisplayName = FinalDisplayName.Replace("<", "&lt;");
            FinalDisplayName = FinalDisplayName.Replace(">", "&gt;");

            // generate the plist file
            StringBuilder Text = new StringBuilder();
			Text.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
			Text.AppendLine("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
			Text.AppendLine("<plist version=\"1.0\">");
			Text.AppendLine("<dict>");
			Text.AppendLine("\t<key>CFBundleDevelopmentRegion</key>");
			Text.AppendLine("\t<string>en</string>");
			Text.AppendLine("\t<key>CFBundleDisplayName</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", EncodeBundleName(BundleDisplayName, ProjectName)));
			Text.AppendLine("\t<key>CFBundleExecutable</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", bIsUE4Game ? "UE4Game" : GameName));
			Text.AppendLine("\t<key>CFBundleIdentifier</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", BundleIdentifier.Replace("[PROJECT_NAME]", ProjectName).Replace("_","")));
			Text.AppendLine("\t<key>CFBundleInfoDictionaryVersion</key>");
			Text.AppendLine("\t<string>6.0</string>");
			Text.AppendLine("\t<key>CFBundleName</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", EncodeBundleName(BundleName, ProjectName)));
			Text.AppendLine("\t<key>CFBundlePackageType</key>");
			Text.AppendLine("\t<string>APPL</string>");
			Text.AppendLine("\t<key>CFBundleSignature</key>");
			Text.AppendLine("\t<string>????</string>");
			Text.AppendLine("\t<key>CFBundleVersion</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", VersionUtilities.UpdateBundleVersion(OldPListData, InEngineDir)));
			Text.AppendLine("\t<key>CFBundleShortVersionString</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", BundleShortVersion));
			Text.AppendLine("\t<key>LSRequiresIPhoneOS</key>");
			Text.AppendLine("\t<true/>");
			Text.AppendLine("\t<key>UIRequiredDeviceCapabilities</key>");
			Text.AppendLine("\t<array>");
			foreach (string Line in RequiredCaps.Split("\r\n".ToCharArray()))
			{
				if (!string.IsNullOrWhiteSpace(Line))
				{
					Text.AppendLine(Line);
				}
			}
            Text.AppendLine("\t</array>");

            Text.AppendLine("\t<key>TVTopShelfImage</key>");
            Text.AppendLine("\t<dict>");
            Text.AppendLine("\t\t<key>TVTopShelfPrimaryImage</key>");
            Text.AppendLine("\t\t<string>TopShelf</string>");
            Text.AppendLine("\t</dict>");
            Text.AppendLine("\t<key>UILaunchImages</key>");
            Text.AppendLine("\t<array>");
            Text.AppendLine("\t\t<dict>");
            Text.AppendLine("\t\t\t<key>UILaunchImageSize</key>");
            Text.AppendLine("\t\t\t<string>{1920, 1080}</string>");
            Text.AppendLine("\t\t\t<key>UILaunchImageName</key>");
            Text.AppendLine("\t\t\t<string>LaunchImage</string>");
            Text.AppendLine("\t\t\t<key>UILaunchImageMinimumOSVersion</key>");
            Text.AppendLine("\t\t\t<string>9.0</string>");
            Text.AppendLine("\t\t\t<key>UILaunchImageOrientation</key>");
            Text.AppendLine("\t\t\t<string>Landscape</string>");
            Text.AppendLine("\t\t</dict>");
            Text.AppendLine("\t</array>");
            Text.AppendLine("\t<key>CFBundleIcons</key>");
            Text.AppendLine("\t<dict>");
            Text.AppendLine("\t\t<key>CFBundlePrimaryIcon</key>");
            Text.AppendLine("\t\t<string>AppIconSmall</string>");
            Text.AppendLine("\t</dict>");

            /*			Text.AppendLine("\t<key>CFBundleIcons</key>");
                        Text.AppendLine("\t<dict>");
                        Text.AppendLine("\t\t<key>CFBundlePrimaryIcon</key>");
                        Text.AppendLine("\t\t<dict>");
                        Text.AppendLine("\t\t\t<key>CFBundleIconFiles</key>");
                        Text.AppendLine("\t\t\t<array>");
                        Text.AppendLine("\t\t\t\t<string>Icon29.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon29@2x.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon40.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon40@2x.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon57.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon57@2x.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon60@2x.png</string>");
                        Text.AppendLine("\t\t\t</array>");
                        Text.AppendLine("\t\t\t<key>UIPrerenderedIcon</key>");
                        Text.AppendLine("\t\t\t<true/>");
                        Text.AppendLine("\t\t</dict>");
                        Text.AppendLine("\t</dict>");
                        Text.AppendLine("\t<key>CFBundleIcons~ipad</key>");
                        Text.AppendLine("\t<dict>");
                        Text.AppendLine("\t\t<key>CFBundlePrimaryIcon</key>");
                        Text.AppendLine("\t\t<dict>");
                        Text.AppendLine("\t\t\t<key>CFBundleIconFiles</key>");
                        Text.AppendLine("\t\t\t<array>");
                        Text.AppendLine("\t\t\t\t<string>Icon29.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon29@2x.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon40.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon40@2x.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon50.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon50@2x.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon72.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon72@2x.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon76.png</string>");
                        Text.AppendLine("\t\t\t\t<string>Icon76@2x.png</string>");
                        Text.AppendLine("\t\t\t</array>");
                        Text.AppendLine("\t\t\t<key>UIPrerenderedIcon</key>");
                        Text.AppendLine("\t\t\t<true/>");
                        Text.AppendLine("\t\t</dict>");
                        Text.AppendLine("\t</dict>");
                        if (File.Exists(LaunchXib))
                        {
                            // TODO: compile the xib via remote tool
                            Text.AppendLine("\t<key>UILaunchStoryboardName</key>");
                            Text.AppendLine("\t<string>LaunchScreen</string>");
                            bSkipDefaultPNGs = true;
                        }
                        else
                        {
                            // this is a temp way to inject the iphone 6 images without needing to upgrade everyone's plist
                            // eventually we want to generate this based on what the user has set in the project settings
                            string[] IPhoneConfigs =  
                                { 
                                    "Default-IPhone6", "Landscape", "{375, 667}", 
                                    "Default-IPhone6", "Portrait", "{375, 667}", 
                                    "Default-IPhone6Plus-Landscape", "Landscape", "{414, 736}", 
                                    "Default-IPhone6Plus-Portrait", "Portrait", "{414, 736}", 
                                    "Default", "Landscape", "{320, 480}",
                                    "Default", "Portrait", "{320, 480}",
                                    "Default-568h", "Landscape", "{320, 568}",
                                    "Default-568h", "Portrait", "{320, 568}",
                                };

                            Text.AppendLine("\t<key>UILaunchImages~iphone</key>");
                            Text.AppendLine("\t<array>");
                            for (int ConfigIndex = 0; ConfigIndex < IPhoneConfigs.Length; ConfigIndex += 3)
                            {
                                Text.AppendLine("\t\t<dict>");
                                Text.AppendLine("\t\t\t<key>UILaunchImageMinimumOSVersion</key>");
                                Text.AppendLine("\t\t\t<string>8.0</string>");
                                Text.AppendLine("\t\t\t<key>UILaunchImageName</key>");
                                Text.AppendLine(string.Format("\t\t\t<string>{0}</string>", IPhoneConfigs[ConfigIndex + 0]));
                                Text.AppendLine("\t\t\t<key>UILaunchImageOrientation</key>");
                                Text.AppendLine(string.Format("\t\t\t<string>{0}</string>", IPhoneConfigs[ConfigIndex + 1]));
                                Text.AppendLine("\t\t\t<key>UILaunchImageSize</key>");
                                Text.AppendLine(string.Format("\t\t\t<string>{0}</string>", IPhoneConfigs[ConfigIndex + 2]));
                                Text.AppendLine("\t\t</dict>");
                            }

                            // close it out
                            Text.AppendLine("\t</array>");
                        }
                        Text.AppendLine("\t<key>UILaunchImages~ipad</key>");
                        Text.AppendLine("\t<array>");
                        Text.AppendLine("\t\t<dict>");
                        Text.AppendLine("\t\t\t<key>UILaunchImageMinimumOSVersion</key>");
                        Text.AppendLine("\t\t\t<string>7.0</string>");
                        Text.AppendLine("\t\t\t<key>UILaunchImageName</key>");
                        Text.AppendLine("\t\t\t<string>Default-Landscape</string>");
                        Text.AppendLine("\t\t\t<key>UILaunchImageOrientation</key>");
                        Text.AppendLine("\t\t\t<string>Landscape</string>");
                        Text.AppendLine("\t\t\t<key>UILaunchImageSize</key>");
                        Text.AppendLine("\t\t\t<string>{768, 1024}</string>");
                        Text.AppendLine("\t\t</dict>");
                        Text.AppendLine("\t\t<dict>");
                        Text.AppendLine("\t\t\t<key>UILaunchImageMinimumOSVersion</key>");
                        Text.AppendLine("\t\t\t<string>7.0</string>");
                        Text.AppendLine("\t\t\t<key>UILaunchImageName</key>");
                        Text.AppendLine("\t\t\t<string>Default-Portrait</string>");
                        Text.AppendLine("\t\t\t<key>UILaunchImageOrientation</key>");
                        Text.AppendLine("\t\t\t<string>Portrait</string>");
                        Text.AppendLine("\t\t\t<key>UILaunchImageSize</key>");
                        Text.AppendLine("\t\t\t<string>{768, 1024}</string>");
                        Text.AppendLine("\t\t</dict>");
                        Text.AppendLine("\t</array>”);
                        Text.AppendLine("\t<key>CFBundleSupportedPlatforms</key>");
                        Text.AppendLine("\t<array>");
                        Text.AppendLine("\t\t<string>iPhoneOS</string>");
                        Text.AppendLine("\t</array>");
                        Text.AppendLine("\t<key>MinimumOSVersion</key>");
                        Text.AppendLine(string.Format("\t<string>{0}</string>", MinVersion));
                        if (!string.IsNullOrEmpty(ExtraData))
                        {
                            ExtraData = ExtraData.Replace("\\n", "\n");
                            foreach (string Line in ExtraData.Split("\r\n".ToCharArray()))
                            {
                                if (!string.IsNullOrWhiteSpace(Line))
                                {
                                    Text.AppendLine("\t" + Line);
                                }
                            }
                        }*/
            Text.AppendLine("</dict>");
			Text.AppendLine("</plist>");

			// Create the intermediate directory if needed
			if (!Directory.Exists(IntermediateDirectory))
			{
				Directory.CreateDirectory(IntermediateDirectory);
			}

			if(InThis != null && InThis.UPL != null)
			{
				// Allow UPL to modify the plist here
				XDocument XDoc;
				try
				{
					XDoc = XDocument.Parse(Text.ToString());
				}
				catch (Exception e)
				{
					throw new BuildException("plist is invalid {0}\n{1}", e, Text.ToString());
				}

				XDoc.DocumentType.InternalSubset = "";
				InThis.UPL.ProcessPluginNode("None", "iosPListUpdates", "", ref XDoc);
                string result = XDoc.Declaration.ToString() + "\n" + XDoc.ToString().Replace("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"[]>", "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
                File.WriteAllText(PListFile, result);
			}
			else
			{
				File.WriteAllText(PListFile, Text.ToString());
			}

			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				if (!Directory.Exists(AppDirectory))
				{
					Directory.CreateDirectory(AppDirectory);
				}
				File.WriteAllText(AppDirectory + "/Info.plist", Text.ToString());
			}

			return bSkipDefaultPNGs;
		}

		public override bool GeneratePList(FileReference ProjectFile, UnrealTargetConfiguration Config, string ProjectDirectory, bool bIsUE4Game, string GameName, string ProjectName, string InEngineDir, string AppDirectory, out bool bSupportsPortrait, out bool bSupportsLandscape, out bool bSkipIcons)
		{
            bSupportsLandscape = bSupportsPortrait = true;
            bSkipIcons = true;
			return GenerateTVOSPList(ProjectDirectory, bIsUE4Game, GameName, ProjectName, InEngineDir, AppDirectory, this);
		}

        protected override void CopyGraphicsResources(bool bSkipDefaultPNGs, bool bSkipIcons, string InEngineDir, string AppDirectory, string BuildDirectory, string IntermediateDir, bool bSupportsPortrait, bool bSupportsLandscape)
        {
            // do nothing on TVOS
        }
    }
}
