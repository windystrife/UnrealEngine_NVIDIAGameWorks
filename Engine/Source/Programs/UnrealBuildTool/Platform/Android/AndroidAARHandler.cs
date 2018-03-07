// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.Xml.Linq;
using System.Diagnostics;
using System.IO;

using Ionic.Zip;

namespace UnrealBuildTool
{
	class AndroidAARHandler
	{
		public class AndroidAAREntry
		{
			public string BaseName;
			public string Version;
			public string Filename;
			public List<string> Dependencies;

			public AndroidAAREntry(string InBaseName, string InVersion, string InFilename)
			{
				BaseName = InBaseName;
				Version = InVersion;
				Filename = InFilename;
				Dependencies = new List<string>();
			}

			public void AddDependency(string InBaseName, string InVersion)
			{
				Dependencies.Add(InBaseName);  // + "-" + InVersion);  will replace version with latest
			}

			public void ClearDependencies()
			{
				Dependencies.Clear();
			}
		}

		public List<string> Repositories = null;
		public List<AndroidAAREntry> AARList = null;
		private List<AndroidAAREntry> JARList = null;

		/// <summary>
		/// Handler for AAR and JAR dependency determination and staging
		/// </summary>
		public AndroidAARHandler()
		{
			Repositories = new List<string>();
			AARList = new List<AndroidAAREntry>();
			JARList = new List<AndroidAAREntry>();
		}

		/// <summary>
		/// Add a new respository path to search for AAR and JAR files
		/// </summary>
		/// <param name="RepositoryPath">Directory containing the repository</param>
		public void AddRepository(string RepositoryPath)
		{
			if (Directory.Exists(RepositoryPath))
			{
				Log.TraceInformation("Added repository: {0}", RepositoryPath);
				Repositories.Add(RepositoryPath);
			}
			else
			{
				Log.TraceWarning("AddRepository: Directory {0} not found!", RepositoryPath);
			}
		}

		/// <summary>
		/// Add new respository paths to search for AAR and JAR files (recursive)
		/// </summary>
		/// <param name="RepositoryPath">Root directory containing the repository</param>
		/// <param name="SearchPattern">Search pattern to match</param>
		public void AddRepositories(string RepositoryPath, string SearchPattern)
		{
			List<string> ToCheck = new List<string>();
			ToCheck.Add(RepositoryPath);
			while (ToCheck.Count > 0)
			{
				int LastIndex = ToCheck.Count - 1;
				string CurrentDir = ToCheck[LastIndex];
				ToCheck.RemoveAt(LastIndex);
				foreach (string SearchPath in Directory.GetDirectories(CurrentDir))
				{
					if (SearchPath.Contains(SearchPattern))
					{
						Log.TraceInformation("Added repository: {0}", SearchPath);
						Repositories.Add(SearchPath);
					}
					else
					{
						ToCheck.Add(SearchPath);
					}
				}
			}
		}

		public void DumpAAR()
		{
			Log.TraceInformation("ALL DEPENDENCIES");
			foreach (AndroidAAREntry Entry in AARList)
			{
				Log.TraceInformation("{0}", Entry.Filename);
			}
			foreach (AndroidAAREntry Entry in JARList)
			{
				Log.TraceInformation("{0}", Entry.Filename);
			}
		}

		private string GetElementValue(XElement SourceElement, XName ElementName, string DefaultValue)
		{
			XElement Element = SourceElement.Element(ElementName);
			return (Element != null) ? Element.Value : DefaultValue;
		}

		private string FindPackageFile(string PackageName, string BaseName, string Version)
		{
			string[] Sections = PackageName.Split('.');
			string PackagePath = Path.Combine(Sections);

			foreach (string Repository in Repositories)
			{
				string PackageDirectory = Path.Combine(Repository, PackagePath, BaseName, Version);

                if (Directory.Exists(PackageDirectory))
				{
					return PackageDirectory;
				}
			}

			return null;
		}
		
		private bool HasAnyVersionCharacters(string InValue)
		{
			for (int Index = 0; Index < InValue.Length; Index++)
			{
				char c = InValue[Index];
				if (c == '.' || (c >= '0' && c <= '9'))
				{
					return true;
				}
			}
			return false;
		}

		private bool HasOnlyVersionCharacters(string InValue)
		{
			for (int Index = 0; Index < InValue.Length; Index++)
			{
				char c = InValue[Index];
				if (c != '.' && !(c >= '0' && c <= '9'))
				{
					return false;
				}
			}
			return true;
		}

		private uint GetVersionValue(string VersionString)
		{
			// read up to 4 sections (ie. 20.0.3.5), first section most significant
			// each section assumed to be 0 to 255 range
			uint Value = 0;
			try
			{
				string[] Sections = VersionString.Split(".".ToCharArray());
				Value |= (Sections.Length > 0) ? (uint.Parse(Sections[0]) << 24) : 0;
				Value |= (Sections.Length > 1) ? (uint.Parse(Sections[1]) << 16) : 0;
				Value |= (Sections.Length > 2) ? (uint.Parse(Sections[2]) << 8) : 0;
				Value |= (Sections.Length > 3) ? uint.Parse(Sections[3]) : 0;
			}
			catch (Exception)
			{
				// ignore poorly formed version
			}
			return Value;
		}

		// clean up the version (Maven version info here: https://docs.oracle.com/middleware/1212/core/MAVEN/maven_version.htm)
		// only going to handle a few cases, not proper ranges (keeps the rightmost valid version which should be highest)
		// will still return something but will include an error in log, but don't want to throw an exception
		private string CleanupVersion(string Filename, string InVersion)
		{
			string WorkVersion = InVersion;

			// if has commas, keep the rightmost part with actual numbers
			if (WorkVersion.Contains(","))
			{
				string[] CommaParts = WorkVersion.Split(',');
				WorkVersion = "";
				for (int Index = CommaParts.Length - 1; Index >= 0; Index--)
				{
					if (HasAnyVersionCharacters(CommaParts[Index]))
					{
						WorkVersion = CommaParts[Index];
						break;
					}
				}
			}

			// if not left with a possibly valid number, stop
			if (!HasAnyVersionCharacters(WorkVersion))
			{
				Log.TraceError("AAR Dependency file {0} version unknown! {1}", Filename, InVersion);
				return InVersion;
			}

			// just remove any parens or brackets left
			WorkVersion = WorkVersion.Replace("(", "").Replace(")", "").Replace("[", "").Replace("]", "");

			// just return it if now looks value
			if (HasOnlyVersionCharacters(WorkVersion))
			{
				return WorkVersion;
			}

			// give an error, not likely going to work, though
			Log.TraceError("AAR Dependency file {0} version unknown! {1}", Filename, InVersion);
			return InVersion;
		}

		/// <summary>
		/// Adds a new required JAR file and resolves dependencies
		/// </summary>
		/// <param name="PackageName">Name of the package the JAR belongs to in repository</param>
		/// <param name="BaseName">Directory in repository containing the JAR</param>
		/// <param name="Version">Version of the AAR to use</param>
		public void AddNewJAR(string PackageName, string BaseName, string Version)
		{
			string BasePath = FindPackageFile(PackageName, BaseName, Version);
			if (BasePath == null)
			{
				Log.TraceError("AAR: Unable to find package {0}!", PackageName + "/" + BaseName);
				return;
			}
			string BaseFilename = Path.Combine(BasePath, BaseName + "-" + Version);

			// Check if already added
			uint NewVersionValue = GetVersionValue(Version);
			for (int JARIndex = 0; JARIndex < JARList.Count; JARIndex++)
			{
				if (JARList[JARIndex].BaseName == BaseName)
				{
					// Is it the same version or older?  ignore if so
					uint EntryVersionValue = GetVersionValue(JARList[JARIndex].Version);
					if (NewVersionValue <= EntryVersionValue)
					{
						return;
					}

					Log.TraceInformation("AAR: {0}: {1} newer than {2}", JARList[JARIndex].BaseName, Version, JARList[JARIndex].Version);

					// This is a newer version; remove old one
					JARList.RemoveAt(JARIndex);
					break;
				}
			}

			//Log.TraceInformation("JAR: {0}", BaseName);
			AndroidAAREntry AAREntry = new AndroidAAREntry(BaseName, Version, BaseFilename);
			JARList.Add(AAREntry);

			// Check for dependencies
			XDocument DependsXML;
			string DependencyFilename = BaseFilename + ".pom";
			if (File.Exists(DependencyFilename))
			{
				try
				{
					DependsXML = XDocument.Load(DependencyFilename);
				}
				catch (Exception e)
				{
					Log.TraceError("AAR Dependency file {0} parsing error! {1}", DependencyFilename, e);
					return;
				}
			}
			else
			{
				Log.TraceError("AAR: Dependency file {0} missing!", DependencyFilename);
				return;
			}

			string NameSpace = DependsXML.Root.Name.NamespaceName;
			XName DependencyName = XName.Get("dependency", NameSpace);
			XName GroupIdName = XName.Get("groupId", NameSpace);
			XName ArtifactIdName = XName.Get("artifactId", NameSpace);
			XName VersionName = XName.Get("version", NameSpace);
			XName ScopeName = XName.Get("scope", NameSpace);
			XName TypeName = XName.Get("type", NameSpace);

			foreach (XElement DependNode in DependsXML.Descendants(DependencyName))
			{
				string DepGroupId = GetElementValue(DependNode, GroupIdName, "");
				string DepArtifactId = GetElementValue(DependNode, ArtifactIdName, "");
				string DepVersion = CleanupVersion(DependencyFilename + "." + DepGroupId + "." + DepArtifactId, GetElementValue(DependNode, VersionName, ""));
				string DepScope = GetElementValue(DependNode, ScopeName, "compile");
				string DepType = GetElementValue(DependNode, TypeName, "jar");

				//Log.TraceInformation("Dependency: {0} {1} {2} {3} {4}", DepGroupId, DepArtifactId, DepVersion, DepScope, DepType);

				// ignore test scope
				if (DepScope == "test")
				{
					continue;
				}

				if (DepType == "aar")
				{
					AddNewAAR(DepGroupId, DepArtifactId, DepVersion);
				}
				else if (DepType == "jar")
				{
					AddNewJAR(DepGroupId, DepArtifactId, DepVersion);
				}
			}
		}

		/// <summary>
		/// Adds a new required AAR file and resolves dependencies
		/// </summary>
		/// <param name="PackageName">Name of the package the AAR belongs to in repository</param>
		/// <param name="BaseName">Directory in repository containing the AAR</param>
		/// <param name="Version">Version of the AAR to use</param>
		/// <param name="HandleDependencies">Optionally process POM file for dependencies (default)</param>
		public void AddNewAAR(string PackageName, string BaseName, string Version, bool HandleDependencies = true)
		{
			if (!HandleDependencies)
			{
				AndroidAAREntry NewAAREntry = new AndroidAAREntry(BaseName, Version, PackageName);
				AARList.Add(NewAAREntry);
				return;
			}

			string BasePath = FindPackageFile(PackageName, BaseName, Version);
			if (BasePath == null)
			{
				Log.TraceError("AAR: Unable to find package {0}!", PackageName + "/" + BaseName);
				return;
			}
			string BaseFilename = Path.Combine(BasePath, BaseName + "-" + Version);

			// Check if already added
			uint NewVersionValue = GetVersionValue(Version);
			for (int AARIndex = 0; AARIndex < AARList.Count; AARIndex++)
			{
				if (AARList[AARIndex].BaseName == BaseName)
				{
					// Is it the same version or older?  ignore if so
					uint EntryVersionValue = GetVersionValue(AARList[AARIndex].Version);
					if (NewVersionValue <= EntryVersionValue)
					{
						return;
					}

					Log.TraceInformation("AAR: {0}: {1} newer than {2}", AARList[AARIndex].BaseName, Version, AARList[AARIndex].Version);

					// This is a newer version; remove old one
					// @TODO: be smarter about dependency cleanup (newer AAR might not need older dependencies)
					AARList.RemoveAt(AARIndex);
					break;
				}
			}

			//Log.TraceInformation("AAR: {0}", BaseName);
			AndroidAAREntry AAREntry = new AndroidAAREntry(BaseName, Version, BaseFilename);
			AARList.Add(AAREntry);

			if (!HandleDependencies)
			{
				return;
			}

			// Check for dependencies
			XDocument DependsXML;
			string DependencyFilename = BaseFilename + ".pom";
			if (File.Exists(DependencyFilename))
			{
				try
				{
					DependsXML = XDocument.Load(DependencyFilename);
				}
				catch (Exception e)
				{
					Log.TraceError("AAR Dependency file {0} parsing error! {1}", DependencyFilename, e);
					return;
				}
			}
			else
			{
				Log.TraceError("AAR: Dependency file {0} missing!", DependencyFilename);
				return;
			}

			string NameSpace = DependsXML.Root.Name.NamespaceName;
			XName DependencyName = XName.Get("dependency", NameSpace);
			XName GroupIdName = XName.Get("groupId", NameSpace);
			XName ArtifactIdName = XName.Get("artifactId", NameSpace);
			XName VersionName = XName.Get("version", NameSpace);
			XName ScopeName = XName.Get("scope", NameSpace);
			XName TypeName = XName.Get("type", NameSpace);

			foreach (XElement DependNode in DependsXML.Descendants(DependencyName))
			{
				string DepGroupId = GetElementValue(DependNode, GroupIdName, "");
				string DepArtifactId = GetElementValue(DependNode, ArtifactIdName, "");
				string DepVersion = CleanupVersion(DependencyFilename + "." + DepGroupId + "." + DepArtifactId, GetElementValue(DependNode, VersionName, ""));
				string DepScope = GetElementValue(DependNode, ScopeName, "compile");
				string DepType = GetElementValue(DependNode, TypeName, "jar");

				//Log.TraceInformation("Dependency: {0} {1} {2} {3} {4}", DepGroupId, DepArtifactId, DepVersion, DepScope, DepType);

				// ignore test scope
				if (DepScope == "test")
				{
					continue;
				}

				if (DepType == "aar")
				{
					// Add dependency
					AAREntry.AddDependency(DepArtifactId, DepVersion);

					AddNewAAR(DepGroupId, DepArtifactId, DepVersion);
				}
				else
				if (DepType == "jar")
				{
					AddNewJAR(DepGroupId, DepArtifactId, DepVersion);
				}
			}
		}

		private void MakeDirectoryIfRequiredForFile(string DestFilename)
		{
			string DestSubdir = Path.GetDirectoryName(DestFilename);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}
		}

		private void MakeDirectoryIfRequired(string DestDirectory)
		{
			if (!Directory.Exists(DestDirectory))
			{
				Directory.CreateDirectory(DestDirectory);
			}
		}

		/// <summary>
		/// Copies the required JAR files to the provided directory
		/// </summary>
		/// <param name="DestinationPath">Destination path for JAR files</param>
		public void CopyJARs(string DestinationPath)
		{
			MakeDirectoryIfRequired(DestinationPath);
			DestinationPath = Path.Combine(DestinationPath, "libs");
			MakeDirectoryIfRequired(DestinationPath);

			foreach (AndroidAAREntry Entry in JARList)
			{
				string Filename = Entry.Filename + ".jar";
				string BaseName = Path.GetFileName(Filename);
				string TargetPath = Path.Combine(DestinationPath, BaseName);
                //Log.TraceInformation("Attempting to copy JAR {0} {1} {2}", Filename, BaseName, TargetPath);

                if (!File.Exists(Filename))
                {
                    Log.TraceInformation("JAR doesn't exist! {0}", Filename);
                }
                if (!File.Exists(TargetPath))
				{
					Log.TraceInformation("Copying JAR {0}", BaseName);
					File.Copy(Filename, TargetPath);
				}
			}
		}

		/// <summary>
		/// Extracts the required AAR files to the provided directory
		/// </summary>
		/// <param name="DestinationPath">Destination path for AAR files</param>
		/// <param name="AppPackageName">Name of the package these AARs are being used with</param>
		public void ExtractAARs(string DestinationPath, string AppPackageName)
		{
			MakeDirectoryIfRequired(DestinationPath);
			DestinationPath = Path.Combine(DestinationPath, "JavaLibs");
			MakeDirectoryIfRequired(DestinationPath);

			Log.TraceInformation("Extracting AARs");
			foreach (AndroidAAREntry Entry in AARList)
			{
				string BaseName = Path.GetFileName(Entry.Filename);
				string TargetPath = Path.Combine(DestinationPath, BaseName);

				// Only extract if haven't before to prevent changing timestamps
				string TargetManifestFileName = Path.Combine(TargetPath, "AndroidManifest.xml");
				if (!File.Exists(TargetManifestFileName))
				{
					Log.TraceInformation("Extracting AAR {0}", BaseName);
					IEnumerable<string> FileNames = UnzipFiles(Entry.Filename + ".aar", TargetPath);

					// Must have a src directory (even if empty)
					string SrcDirectory = Path.Combine(TargetPath, "src");
					if (!Directory.Exists(SrcDirectory))
					{
						Directory.CreateDirectory(SrcDirectory);
						//FileStream Placeholder = new FileStream(Path.Combine(SrcDirectory, ".placeholder_" + BaseName), FileMode.Create, FileAccess.Write);
					}

					// Move the class.jar file
					string ClassSourceFile = Path.Combine(TargetPath, "classes.jar");
					if (File.Exists(ClassSourceFile))
					{
						string ClassDestFile = Path.Combine(TargetPath, "libs", BaseName + ".jar");
						MakeDirectoryIfRequiredForFile(ClassDestFile);
						File.Move(ClassSourceFile, ClassDestFile);
					}

					// Now create the project.properties file
					string PropertyFilename = Path.Combine(TargetPath, "project.properties");
					if (!File.Exists(PropertyFilename))
					{
						// Try to get the SDK target from the AndroidManifest.xml
						string MinSDK = "9";
						string ManifestFilename = Path.Combine(TargetPath, "AndroidManifest.xml");
						XDocument ManifestXML;
						if (File.Exists(ManifestFilename))
						{
							try
							{
								// Replace all instances of the application id with the packagename
								string Contents = File.ReadAllText(ManifestFilename);
								string NewContents = Contents.Replace("${applicationId}", AppPackageName);
								if (Contents != NewContents)
								{
									File.WriteAllText(ManifestFilename, NewContents);
								}

								ManifestXML = XDocument.Load(ManifestFilename);
								XElement UsesSdk = ManifestXML.Root.Element(XName.Get("uses-sdk", ManifestXML.Root.Name.NamespaceName));
								XAttribute Target = UsesSdk.Attribute(XName.Get("minSdkVersion", "http://schemas.android.com/apk/res/android"));
								MinSDK = Target.Value;
							}
							catch (Exception e)
							{
								Log.TraceError("AAR Manifest file {0} parsing error! {1}", ManifestFilename, e);
							}
						}

						// Project contents
						string ProjectPropertiesContents = "target=android-" + MinSDK + "\nandroid.library=true\n";

						// Add the dependencies
						int RefCount = 0;
						foreach (string DependencyName in Entry.Dependencies)
						{
							// Find the version
							foreach (AndroidAAREntry ScanEntry in AARList)
							{
								if (ScanEntry.BaseName == DependencyName)
								{
									RefCount++;
									ProjectPropertiesContents += "android.library.reference." + RefCount + "=../" + DependencyName + "-" + ScanEntry.Version + "\n";
									break;
								}
							}
						}

						File.WriteAllText(PropertyFilename, ProjectPropertiesContents);
					}
				}
			}
		}

		/// <summary>
		/// Extracts the contents of a zip file
		/// </summary>
		/// <param name="ZipFileName">Name of the zip file</param>
		/// <param name="BaseDirectory">Output directory</param>
		/// <returns>List of files written</returns>
		public static IEnumerable<string> UnzipFiles(string ZipFileName, string BaseDirectory)
		{
			// manually extract the files. There was a problem with the Ionic.Zip library that required this on non-PC at one point,
			// but that problem is now fixed. Leaving this code as is as we need to return the list of created files and fix up their permissions anyway.
			using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile(ZipFileName))
			{
				List<string> OutputFileNames = new List<string>();
				foreach (Ionic.Zip.ZipEntry Entry in Zip.Entries)
				{
					// support-v4 and support-v13 has the jar file named with "internal_impl-XX.X.X.jar"
					// this causes error "Found 2 versions of internal_impl-XX.X.X.jar"
					// following codes adds "support-v4-..." to the output jar file name to avoid the collision
					string OutputFileName = Path.Combine(BaseDirectory, Entry.FileName);
					if (Entry.FileName.Contains("internal_impl"))
					{
						string _ZipName = Path.GetFileNameWithoutExtension(ZipFileName);
						string NewOutputFileName = Path.Combine(Path.GetDirectoryName(OutputFileName),
							_ZipName + '-' + Path.GetFileNameWithoutExtension(OutputFileName) + '.' + Path.GetExtension(OutputFileName));
						Log.TraceInformation("Changed FileName {0} => {1}", Entry.FileName, NewOutputFileName);
						OutputFileName = NewOutputFileName;
					}

					Directory.CreateDirectory(Path.GetDirectoryName(OutputFileName));
					if (!Entry.IsDirectory)
					{
						using (FileStream OutputStream = new FileStream(OutputFileName, FileMode.Create, FileAccess.Write))
						{
							Entry.Extract(OutputStream);
						}
						OutputFileNames.Add(OutputFileName);
					}
				}
				return OutputFileNames;
			}
		}

	}
}
