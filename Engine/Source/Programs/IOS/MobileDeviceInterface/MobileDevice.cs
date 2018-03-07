// Software License Agreement (BSD License)
// 
// Copyright (c) 2007, Peter Dennis Bartok <PeterDennisBartok@gmail.com>
// All rights reserved.
// 
// Redistribution and use of this software in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above
//   copyright notice, this list of conditions and the
//   following disclaimer.
// 
// * Redistributions in binary form must reproduce the above
//   copyright notice, this list of conditions and the
//   following disclaimer in the documentation and/or other
//   materials provided with the distribution.
// 
// * Neither the name of Peter Dennis Bartok nor the names of its
//   contributors may be used to endorse or promote products
//   derived from this software without specific prior
//   written permission of Yahoo! Inc.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 

//
// Based on code developed by geohot, ixtli, nightwatch, warren
// See http://iphone.fiveforty.net/wiki/index.php?title=Main_Page
//

using System;
using System.IO;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32;
using MobileDeviceInterface;

namespace Manzana
{
    /// <summary>
    /// Structures with unknown innards.  They should only ever be used as the target of a TypedPtr, never inspected.
    /// </summary>
    public class AppleMobileDeviceConnection { }

    /// <summary>
    /// Structures with unknown innards.  They should only ever be used as the target of a TypedPtr, never inspected.
    /// </summary>
    public class AFCCommConnection { }

    public class AFCDictionary { }

    /// <summary>
    /// Provides the fields representing the type of notification
    /// </summary>
    public enum NotificationMessage
    {
        /// <summary>The iPhone was connected to the computer.</summary>
        Connected = 1,

        /// <summary>The iPhone was disconnected from the computer.</summary>
        Disconnected = 2,

        /// <summary>Notification from the iPhone occurred, but the type is unknown.</summary>
        Unknown = 3,
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    internal struct AMDeviceNotificationCallbackInfo
    {
        public IntPtr dev
        {
            get
            {
                return dev_ptr;
            }
        }
        internal IntPtr dev_ptr;
        public NotificationMessage msg;
    }


    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    internal struct AMRecoveryDevice
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public byte[] unknown0;			/* 0 */
        public DeviceRestoreNotificationCallback callback;		/* 8 */
        public IntPtr user_info;			/* 12 */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 12)]
        public byte[] unknown1;			/* 16 */
        public uint readwrite_pipe;		/* 28 */
        public byte read_pipe;          /* 32 */
        public byte write_ctrl_pipe;    /* 33 */
        public byte read_unknown_pipe;  /* 34 */
        public byte write_file_pipe;    /* 35 */
        public byte write_input_pipe;   /* 36 */
    };

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void DeviceNotificationCallback(ref AMDeviceNotificationCallbackInfo callback_info);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void DeviceRestoreNotificationCallback(ref AMRecoveryDevice callback_info);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void DeviceInstallationCallback(IntPtr/*CFDictionary*/ UntypedStatusDict, IntPtr UserData);

	public interface ICoreFoundationRunLoop
	{
		int RunLoopInMode(IntPtr mode, double seconds, int returnAfterSourceHandled);
		IntPtr DefaultMode();
	}

	internal class CoreFoundationRunLoopOSX : ICoreFoundationRunLoop
	{
		public int RunLoopInMode(IntPtr mode, double seconds, int returnAfterSourceHandled)
		{
			return CFRunLoopRunInMode(mode, seconds, returnAfterSourceHandled);
		}

		public IntPtr DefaultMode()
		{
			return kCFRunLoopDefaultMode;
		}

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static IntPtr CFStringCreateWithCString(IntPtr allocator, string value, int encoding);

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static void CFRunLoopRun();

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static IntPtr CFRunLoopGetMain();

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static IntPtr CFRunLoopGetCurrent();

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static int CFRunLoopRunInMode(IntPtr mode, double seconds, int returnAfterSourceHandled);

		public static IntPtr kCFRunLoopDefaultMode = CFStringCreateWithCString(IntPtr.Zero, "kCFRunLoopDefaultMode", 0);
	}

	internal class CoreFoundationRunLoopWin32 : ICoreFoundationRunLoop
	{
		static CoreFoundationRunLoopWin32()
		{
		}

		public int RunLoopInMode(IntPtr mode, double seconds, int returnAfterSourceHandled)
		{
			return CFRunLoopRunInMode(mode, seconds, returnAfterSourceHandled);
		}

		public IntPtr DefaultMode()
		{
			return kCFRunLoopDefaultMode;
		}

		[DllImport("CoreFoundation.dll")]
		public extern static IntPtr CFStringCreateWithCString(IntPtr allocator, string value, int encoding);

		[DllImport("CoreFoundation.dll")]
		public extern static void CFRunLoopRun();

		[DllImport("CoreFoundation.dll")]
		public extern static IntPtr CFRunLoopGetMain();

		[DllImport("CoreFoundation.dll")]
		public extern static IntPtr CFRunLoopGetCurrent();

		[DllImport("CoreFoundation.dll")]
		public extern static int CFRunLoopRunInMode(IntPtr mode, double seconds, int returnAfterSourceHandled);

		public static IntPtr kCFRunLoopDefaultMode = CFStringCreateWithCString(IntPtr.Zero, "kCFRunLoopDefaultMode", 0);
	}
	
	public class CoreFoundationRunLoop
	{
		static public ICoreFoundationRunLoop CoreImpl;

		static CoreFoundationRunLoop()
		{
			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix)
			{
				CoreImpl = new CoreFoundationRunLoopOSX();
			}
			else
			{
				List<string> PathBits = new List<string>();
				PathBits.Add(Environment.GetEnvironmentVariable("Path"));

				// Try to add the paths from the registry (they aren't always available on newer iTunes installs though)
				object RegistrySupportDir = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Apple Inc.\Apple Application Support", "InstallDir", Environment.CurrentDirectory);
				if (RegistrySupportDir != null)
				{
					DirectoryInfo ApplicationSupportDirectory = new DirectoryInfo(RegistrySupportDir.ToString());

					if (ApplicationSupportDirectory.Exists)
					{
						PathBits.Add(ApplicationSupportDirectory.FullName);
					}
				}

				// Add some guesses as well
				DirectoryInfo AppleMobileDeviceSupport = new DirectoryInfo(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFiles) + @"\Apple\Mobile Device Support");
				if (AppleMobileDeviceSupport.Exists)
				{
					PathBits.Add(AppleMobileDeviceSupport.FullName);
				}

				DirectoryInfo AppleMobileDeviceSupportX86 = new DirectoryInfo(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFilesX86) + @"\Apple\Mobile Device Support");
				if ((AppleMobileDeviceSupport != AppleMobileDeviceSupportX86) && (AppleMobileDeviceSupportX86.Exists))
				{
					PathBits.Add(AppleMobileDeviceSupportX86.FullName);
				}

				// Set the path from all the individual bits
				Environment.SetEnvironmentVariable("Path", string.Join(";", PathBits));
				CoreImpl = new CoreFoundationRunLoopWin32();
			}
		}

		public static int RunLoopRunInMode(IntPtr mode, double seconds, int returnAfterSourceHandled)
		{
			return CoreImpl.RunLoopInMode(mode, seconds, returnAfterSourceHandled);
		}

		public static IntPtr kCFRunLoopDefaultMode()
		{
			return CoreImpl.DefaultMode();
		}
	}

	internal interface MobileDeviceImpl
	{
		int NotificationSubscribe(DeviceNotificationCallback DeviceCallbackHandle);
		IntPtr CopyDeviceIdentifier(TypedPtr<AppleMobileDeviceConnection> device);
		int Connect(TypedPtr<AppleMobileDeviceConnection> device);
		IntPtr CopyValue(TypedPtr<AppleMobileDeviceConnection> device, uint unknown, TypedPtr<CFString> cfstring);
		int Disconnect(TypedPtr<AppleMobileDeviceConnection> device);
		int GetConnectionID(TypedPtr<AppleMobileDeviceConnection> device);
		int IsPaired(TypedPtr<AppleMobileDeviceConnection> device);
		int Pair(TypedPtr<AppleMobileDeviceConnection> device);
		int Unpair(TypedPtr<AppleMobileDeviceConnection> device);
		int ValidatePairing(TypedPtr<AppleMobileDeviceConnection> device);
		int LookupApplications(TypedPtr<AppleMobileDeviceConnection> device, IntPtr options, out Dictionary<string, object> AppBundles);
		int StartSession(TypedPtr<AppleMobileDeviceConnection> device);
		int StopSession(TypedPtr<AppleMobileDeviceConnection> device);
		int StartHouseArrestService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2);
		int StartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, ref IntPtr handle, IntPtr unknown);
		int SecureStartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, int flagsPassInZero, ref IntPtr handle);
		string ServiceConnectionReceive(IntPtr handle);
		int RestoreRegisterForDeviceNotifications(
			DeviceRestoreNotificationCallback dfu_connect,
			DeviceRestoreNotificationCallback recovery_connect,
			DeviceRestoreNotificationCallback dfu_disconnect,
			DeviceRestoreNotificationCallback recovery_disconnect,
			uint unknown0,
			IntPtr user_info);
		int SecureUninstallApplication(
			IntPtr serviceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFString> ApplicationIdentifer,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData);
		int SecureInstallApplication(
			IntPtr serviceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFURL> UrlPath,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData);
		int SecureUpgradeApplication(
			IntPtr ServiceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFURL> UrlPath,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData);

		int ConnectionClose(TypedPtr<AFCCommConnection> conn);
		int ConnectionInvalidate(TypedPtr<AFCCommConnection> conn);
		int ConnectionIsValid(TypedPtr<AFCCommConnection> conn);
		int ConnectionOpen(IntPtr handle, uint io_timeout, out TypedPtr<AFCCommConnection> OutConn);
		int DeviceInfoOpen(IntPtr handle, ref IntPtr dict);
		int DirectoryClose(TypedPtr<AFCCommConnection> conn, IntPtr dir);
		int DirectoryCreate(TypedPtr<AFCCommConnection> conn, string path);
		int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref IntPtr dirent);
		int FileInfoOpen(TypedPtr<AFCCommConnection> conn, string path, out TypedPtr<AFCDictionary> OutDict);
		int FileRefClose(TypedPtr<AFCCommConnection> conn, Int64 handle);
		int FileRefOpen(TypedPtr<AFCCommConnection> conn, string path, Int64 mode, out Int64 handle);
		int FileRefRead(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, ref uint len);
		int FileRefSeek(TypedPtr<AFCCommConnection> conn, Int64 handle, Int64 pos, Int64 origin);
		int FileRefSetFileSize(TypedPtr<AFCCommConnection> conn, Int64 handle, uint size);
		int FileRefTell(TypedPtr<AFCCommConnection> conn, Int64 handle, ref uint position);
		int FileRefWrite(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, uint len);
		int FlushData(TypedPtr<AFCCommConnection> conn, Int64 handle);
		int KeyValueClose(TypedPtr<AFCDictionary> dict);
		int KeyValueRead(TypedPtr<AFCDictionary> dict, out IntPtr key, out IntPtr val);
		int RemovePath(TypedPtr<AFCCommConnection> conn, string path);
		int RenamePath(TypedPtr<AFCCommConnection> conn, string OldPath, string NewPath);
		int DirectoryOpen(TypedPtr<AFCCommConnection> conn, string path, ref IntPtr dir);
		int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref string buffer);
	}

	internal class MobileDeviceOSX : MobileDeviceImpl
	{
		//		static readonly int ForceStaticInit = 42;
		const string DLLName = "/System/Library/PrivateFrameworks/MobileDevice.framework/Versions/A/MobileDevice";

		public int NotificationSubscribe(DeviceNotificationCallback DeviceCallbackHandle)
		{
			return AMDeviceMethods.NotificationSubscribe(DeviceCallbackHandle);
		}

		public IntPtr CopyDeviceIdentifier(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.CopyDeviceIdentifier(device);
		}

		public int Connect(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Connect(device);
		}

		public IntPtr CopyValue(TypedPtr<AppleMobileDeviceConnection> device, uint unknown, TypedPtr<CFString> cfstring)
		{
			return AMDeviceMethods.CopyValue(device, unknown, cfstring);
		}

		public int Disconnect(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Disconnect(device);
		}

		public int GetConnectionID(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.GetConnectionID(device);
		}

		public int IsPaired(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.IsPaired(device);
		}

		public int Pair(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Pair(device);
		}

		public int Unpair(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Unpair(device);
		}

		public int ValidatePairing(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.ValidatePairing(device);
		}

		public int LookupApplications(TypedPtr<AppleMobileDeviceConnection> device, IntPtr options, out Dictionary<string, object> AppBundles)
		{
			int Result = AMDeviceMethods.LookupApplications(device, options, out AppBundles);
			return Result;
		}

		public int StartSession(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.StartSession((IntPtr)device);
		}

		public int StopSession(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.StopSession((IntPtr)device);
		}

		public int StartHouseArrestService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2)
		{
			return AMDeviceMethods.StartHouseArrestService((IntPtr)device, (IntPtr)bundleName, unknown1, ref handle, unknown2);
		}

		public int StartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, ref IntPtr handle, IntPtr unknown)
		{
			return AMDeviceMethods.StartService((IntPtr)device, (IntPtr)serviceName, ref handle, unknown);
		}

		public int SecureStartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, int flagsPassInZero, ref IntPtr handle)
		{
			return AMDeviceMethods.SecureStartService((IntPtr)device, (IntPtr)serviceName, flagsPassInZero, ref handle);
		}
		
		public string ServiceConnectionReceive(IntPtr handle)
		{
			return AMDeviceMethods.ServiceConnectionReceive(handle);
		}

		public int RestoreRegisterForDeviceNotifications(
			DeviceRestoreNotificationCallback dfu_connect,
			DeviceRestoreNotificationCallback recovery_connect,
			DeviceRestoreNotificationCallback dfu_disconnect,
			DeviceRestoreNotificationCallback recovery_disconnect,
			uint unknown0,
			IntPtr user_info)
		{
			return AMDeviceMethods.AMRestoreRegisterForDeviceNotifications(dfu_connect, recovery_connect, dfu_disconnect, recovery_disconnect, unknown0, user_info);
		}

		public int SecureUninstallApplication(
			IntPtr serviceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFString> ApplicationIdentifer,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureUninstallApplication(serviceConnection, DeviceIfConnIsNull, ApplicationIdentifer, ClientOptions, ProgressCallback, UserData);
		}

		public int SecureInstallApplication(
			IntPtr serviceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFURL> UrlPath,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureInstallApplication(serviceConnection, DeviceIfConnIsNull, UrlPath, ClientOptions, ProgressCallback, UserData);
		}

		public int SecureUpgradeApplication(
			IntPtr ServiceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFURL> UrlPath,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureUpgradeApplication(ServiceConnection, DeviceIfConnIsNull, UrlPath, ClientOptions, ProgressCallback, UserData);
		}

		public int ConnectionClose(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionClose(conn);
		}

		public int ConnectionInvalidate(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionInvalidate(conn);
		}

		public int ConnectionIsValid(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionIsValid(conn);
		}

		public int ConnectionOpen(IntPtr handle, uint io_timeout, out TypedPtr<AFCCommConnection> OutConn)
		{
			return AFC.ConnectionOpen(handle, io_timeout, out OutConn);
		}

		public int DeviceInfoOpen(IntPtr handle, ref IntPtr dict)
		{
			return AFC.DeviceInfoOpen(handle, ref dict);
		}

		public int DirectoryClose(TypedPtr<AFCCommConnection> conn, IntPtr dir)
		{
			return AFC.DirectoryClose(conn, dir);
		}

		public int DirectoryCreate(TypedPtr<AFCCommConnection> conn, string path)
		{
			return AFC.DirectoryCreate(conn, path);
		}

		public int DirectoryOpen(TypedPtr<AFCCommConnection> conn, string path, ref IntPtr dir)
		{
			return AFC.DirectoryOpen(conn, path, ref dir);
		}

		public int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref IntPtr dirent)
		{
			return AFC.DirectoryRead(conn, dir, ref dirent);
		}

		public int FileInfoOpen(TypedPtr<AFCCommConnection> conn, string path, out TypedPtr<AFCDictionary> OutDict)
		{
			return AFC.FileInfoOpen(conn, path, out OutDict);
		}

		public int FileRefClose(TypedPtr<AFCCommConnection> conn, Int64 handle)
		{
			return AFC.FileRefClose(conn, handle);
		}

		public int FileRefOpen(TypedPtr<AFCCommConnection> conn, string path, Int64 mode, out Int64 handle)
		{
			return AFC.FileRefOpen(conn, path, mode, out handle);
		}

		public int FileRefRead(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, ref uint len)
		{
			return AFC.FileRefRead(conn, handle, buffer, ref len);
		}

		public int FileRefSeek(TypedPtr<AFCCommConnection> conn, Int64 handle, Int64 pos, Int64 origin)
		{
			return AFC.FileRefSeek(conn, handle, pos, origin);
		}

		public int FileRefSetFileSize(TypedPtr<AFCCommConnection> conn, Int64 handle, uint size)
		{
			return AFC.FileRefSetFileSize(conn, handle, size);
		}

		public int FileRefTell(TypedPtr<AFCCommConnection> conn, Int64 handle, ref uint position)
		{
			return AFC.FileRefTell(conn, handle, ref position);
		}

		public int FileRefWrite(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, uint len)
		{
			return AFC.FileRefWrite(conn, handle, buffer, len);
		}

		public int FlushData(TypedPtr<AFCCommConnection> conn, Int64 handle)
		{
			return AFC.FlushData(conn, handle);
		}

		public int KeyValueClose(TypedPtr<AFCDictionary> dict)
		{
			return AFC.KeyValueClose(dict);
		}

		public int KeyValueRead(TypedPtr<AFCDictionary> dict, out IntPtr key, out IntPtr val)
		{
			return AFC.KeyValueRead(dict, out key, out val);
		}

		public int RemovePath(TypedPtr<AFCCommConnection> conn, string path)
		{
			return AFC.RemovePath(conn, path);
		}

		public int RenamePath(TypedPtr<AFCCommConnection> conn, string OldPath, string NewPath)
		{
			return AFC.RenamePath(conn, OldPath, NewPath);
		}

#region Application Methods
		public class AMDeviceMethods
		{
			// Need to have the path manipulation code in static MobileDevice() to run
			//			static int ForceMobileDeviceConstructorToRun;
			static AMDeviceMethods()
			{
				//				ForceMobileDeviceConstructorToRun = ForceStaticInit;
			}

			public static IntPtr CopyDeviceIdentifier(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceCopyDeviceIdentifier((IntPtr)device);
			}

			public static int Connect(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceConnect((IntPtr)device);
			}

			public static IntPtr CopyValue(TypedPtr<AppleMobileDeviceConnection> device, uint unknown, TypedPtr<CFString> cfstring)
			{
				return AMDeviceCopyValue((IntPtr)device, unknown, (IntPtr)cfstring);
			}

			public static int Disconnect(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceDisconnect((IntPtr)device);
			}

			public static int GetConnectionID(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceGetConnectionID((IntPtr)device);
			}

			public static int IsPaired(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceIsPaired((IntPtr)device);
			}

			public static int Pair(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDevicePair((IntPtr)device);
			}

			public static int Unpair(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceUnpair((IntPtr)device);
			}

			public static int ValidatePairing(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceValidatePairing((IntPtr)device);
			}

			public static int LookupApplications(TypedPtr<AppleMobileDeviceConnection> device, IntPtr options, out Dictionary<string, object> AppBundles)
			{
				IntPtr UntypedDict = IntPtr.Zero;
				int Result = AMDeviceLookupApplications((IntPtr)device, options, ref UntypedDict);

				AppBundles = MobileDevice.ConvertCFDictionaryToDictionaryString(new TypedPtr<CFDictionary>(UntypedDict));

				return Result;
			}

			public static int StartSession(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceStartSession((IntPtr)device);
			}

			public static int StopSession(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceStopSession((IntPtr)device);
			}

			public static int StartHouseArrestService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2)
			{
				return AMDeviceStartHouseArrestService((IntPtr)device, (IntPtr)bundleName, unknown1, ref handle, unknown2);
			}

			public static int StartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, ref IntPtr handle, IntPtr unknown)
			{
				return AMDeviceStartService((IntPtr)device, (IntPtr)serviceName, ref handle, unknown);
			}

			public static int SecureStartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, int flagsPassInZero, ref IntPtr handle)
			{
				return AMDeviceSecureStartService((IntPtr)device, (IntPtr)serviceName, flagsPassInZero, ref handle);
			}
		
			public static string ServiceConnectionReceive(IntPtr handle)
			{
				StringBuilder sb = new StringBuilder("", 1024);
				AMDServiceConnectionReceive(handle, sb, sb.Capacity, 0);
				return sb.ToString();
			}
			
			public static int UninstallApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> ApplicationIdentifier,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceUninstallApplication((IntPtr)device, (IntPtr)ApplicationIdentifier, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int SecureUninstallApplication(
				IntPtr serviceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFString> ApplicationIdentifer,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureUninstallApplication(serviceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)ApplicationIdentifer, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int SecureInstallApplication(
				IntPtr serviceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFURL> UrlPath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureInstallApplication(serviceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)UrlPath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int TransferApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> InPackagePath,
				IntPtr UnknownButUnused,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceTransferApplication((IntPtr)device, (IntPtr)InPackagePath, UnknownButUnused, ProgressCallback, UserData);
			}

			public static int SecureUpgradeApplication(
				IntPtr ServiceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFURL> UrlPath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureUpgradeApplication(ServiceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)UrlPath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int InstallApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> FilePath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceInstallApplication((IntPtr)device, (IntPtr)FilePath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int NotificationSubscribe(DeviceNotificationCallback DeviceCallbackHandle)
			{
				IntPtr notification;
				return AMDeviceNotificationSubscribe(DeviceCallbackHandle, 0, 0, 0, out notification);
			}

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDeviceCopyDeviceIdentifier(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceConnect(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDeviceCopyValue(IntPtr/*AppleMobileDeviceConnection*/ device, uint unknown, IntPtr/*CFString*/ cfstring);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceDisconnect(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceGetConnectionID(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceIsPaired(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDevicePair(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceUnpair(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceValidatePairing(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceLookupApplications(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr options, ref IntPtr appBundles);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceNotificationSubscribe(DeviceNotificationCallback callback, uint unused1, uint unused2, uint unused3, out IntPtr am_device_notification_ptr);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartHouseArrestService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ serviceName, ref IntPtr handle, IntPtr unknown);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureStartService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ serviceName, int flagsPassInZero, ref IntPtr handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDServiceConnectionReceive(IntPtr handle, StringBuilder buffer, int bufferLen, int unknown);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartSession(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStopSession(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMRestoreModeDeviceCreate(uint unknown0, int connection_id, uint unknown1);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			public extern static int AMRestoreRegisterForDeviceNotifications(
				DeviceRestoreNotificationCallback dfu_connect,
				DeviceRestoreNotificationCallback recovery_connect,
				DeviceRestoreNotificationCallback dfu_disconnect,
				DeviceRestoreNotificationCallback recovery_disconnect,
				uint unknown0,
				IntPtr user_info);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceUninstallApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ ApplicationIdentifier,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureUninstallApplication(
				IntPtr serviceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFString*/ ApplicationIdentifer,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureInstallApplication(
				IntPtr serviceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFURL*/ UrlPath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceTransferApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ InPackagePath,
				IntPtr UnknownButUnused,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureUpgradeApplication(
				IntPtr ServiceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFURL*/ UrlPath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceInstallApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ FilePath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			// the unknown goes into the 4th dword in the connection struct.  if non-zero, winsock send() won't get called in AMDServiceConnectionSend AFAIK
			// the only option is "CloseOnInvalidate" (CFBoolean type, defaults to false)
			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDServiceConnectionCreate(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr UnknownTypically0, IntPtr OptionsDictionary);


			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMSBeginSync(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr OuterIn8, IntPtr OuterIn12, IntPtr OuterIn16);
		}
		#endregion

#region AFC Operations

		public int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref string buffer)
		{
			int ret;

			IntPtr ptr = IntPtr.Zero;
			ret = AFC.DirectoryRead((IntPtr)conn, dir, ref ptr);
			if ((ret == 0) && (ptr != IntPtr.Zero))
			{
				buffer = Marshal.PtrToStringAnsi(ptr);
			}
			else
			{
				buffer = null;
			}
			return ret;
		}

		public class AFC
		{
			public static int ConnectionClose(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionClose((IntPtr)conn);
			}

			public static int ConnectionInvalidate(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionInvalidate((IntPtr)conn);
			}

			public static int ConnectionIsValid(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionIsValid((IntPtr)conn);
			}

			public static int ConnectionOpen(IntPtr handle, uint io_timeout, out TypedPtr<AFCCommConnection> OutConn)
			{
				IntPtr Conn;
				int Result = AFCConnectionOpen(handle, io_timeout, out Conn);
				OutConn = Conn;
				return Result;
			}

			public static int DeviceInfoOpen(IntPtr handle, ref IntPtr dict)
			{
				return AFCDeviceInfoOpen(handle, ref dict);
			}

			public static int DirectoryClose(TypedPtr<AFCCommConnection> conn, IntPtr dir)
			{
				return AFCDirectoryClose((IntPtr)conn, dir);
			}

			public static int DirectoryCreate(TypedPtr<AFCCommConnection> conn, string path)
			{
				return AFCDirectoryCreate((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path));
			}

			public static int DirectoryOpen(TypedPtr<AFCCommConnection> conn, string path, ref IntPtr dir)
			{
				return AFCDirectoryOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), ref dir);
			}

			public static int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref IntPtr dirent)
			{
				return AFCDirectoryRead((IntPtr)conn, dir, ref dirent);
			}

			public static int FileInfoOpen(TypedPtr<AFCCommConnection> conn, string path, out TypedPtr<AFCDictionary> OutDict)
			{
				IntPtr UntypedDict;
				int Result = AFCFileInfoOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), out UntypedDict);
				OutDict = UntypedDict;

				return Result;
			}

			public static int FileRefClose(TypedPtr<AFCCommConnection> conn, Int64 handle)
			{
				return AFCFileRefClose((IntPtr)conn, handle);
			}

			public static int FileRefOpen(TypedPtr<AFCCommConnection> conn, string path, Int64 mode, out Int64 handle)
			{
				return AFCFileRefOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), mode, out handle);
			}

			public static int FileRefRead(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, ref uint len)
			{
				return AFCFileRefRead((IntPtr)conn, handle, buffer, ref len);
			}

			public static int FileRefSetFileSize(TypedPtr<AFCCommConnection> conn, Int64 handle, uint size)
			{
				return AFCFileRefSetFileSize((IntPtr)conn, handle, size);
			}

			public static int FileRefSeek(TypedPtr<AFCCommConnection> conn, Int64 handle, Int64 pos, Int64 origin)
			{
				return AFCFileRefSeek((IntPtr)conn, handle, pos, origin);
			}

			public static int FileRefTell(TypedPtr<AFCCommConnection> conn, Int64 handle, ref uint position)
			{
				return AFCFileRefTell((IntPtr)conn, handle, ref position);
			}

			public static int FileRefWrite(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, uint len)
			{
				return AFCFileRefWrite((IntPtr)conn, handle, buffer, len);
			}

			public static int FlushData(TypedPtr<AFCCommConnection> conn, Int64 handle)
			{
				return AFCFlushData((IntPtr)conn, handle);
			}

			public static int KeyValueClose(TypedPtr<AFCDictionary> dict)
			{
				return AFCKeyValueClose((IntPtr)dict);
			}

			public static int KeyValueRead(TypedPtr<AFCDictionary> dict, out IntPtr key, out IntPtr val)
			{
				return AFCKeyValueRead((IntPtr)dict, out key, out val);
			}

			public static int RemovePath(TypedPtr<AFCCommConnection> conn, string path)
			{
				return AFCRemovePath((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path));
			}

			public static int RenamePath(TypedPtr<AFCCommConnection> conn, string OldPath, string NewPath)
			{
				return AFCRenamePath((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(OldPath), MobileDevice.StringToFileSystemRepresentation(NewPath));
			}

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionClose(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionInvalidate(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionIsValid(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionOpen(IntPtr handle, uint io_timeout, out IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDeviceInfoOpen(IntPtr handle, ref IntPtr dict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryClose(IntPtr/*AFCCommConnection*/ conn, IntPtr dir);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryCreate(IntPtr/*AFCCommConnection*/ conn, byte[] path);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, ref IntPtr dir);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryRead(IntPtr/*AFCCommConnection*/ conn, IntPtr dir, ref IntPtr dirent);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileInfoOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, out IntPtr OutDict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefClose(IntPtr/*AFCCommConnection*/ conn, Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, Int64 mode, out Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefRead(IntPtr/*AFCCommConnection*/ conn, Int64 handle, byte[] buffer, ref uint len);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefSeek(IntPtr/*AFCCommConnection*/ conn, Int64 handle, Int64 pos, Int64 origin);

			// FIXME - not working, arguments?
			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			public extern static int AFCFileRefSetFileSize(IntPtr/*AFCCommConnection*/ conn, Int64 handle, uint size);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefTell(IntPtr/*AFCCommConnection*/ conn, Int64 handle, ref uint position);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefWrite(IntPtr/*AFCCommConnection*/ conn, Int64 handle, byte[] buffer, uint len);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFlushData(IntPtr/*AFCCommConnection*/ conn, Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCKeyValueClose(IntPtr dict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCKeyValueRead(IntPtr dict, out IntPtr key, out IntPtr val);


			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCRemovePath(IntPtr/*AFCCommConnection*/ conn, byte[] path);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCRenamePath(IntPtr/*AFCCommConnection*/ conn, byte[] old_path, byte[] new_path);
		}

		#endregion
	}

	internal class MobileDeviceWiniTunes11 : MobileDeviceImpl
	{
		//		static readonly int ForceStaticInit = 42;
		const string DLLName = "iTunesMobileDevice.dll";

		static MobileDeviceWiniTunes11()
		{
			List<string> PathBits = new List<string>();

			// Try to add the paths from the registry (they aren't always available on newer iTunes installs though)
			object RegistryDllPath = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Apple Inc.\Apple Mobile Device Support\Shared", "iTunesMobileDeviceDLL", DLLName);
			if (RegistryDllPath != null)
			{
				FileInfo iTunesMobileDeviceFile = new FileInfo(RegistryDllPath.ToString());

				if (iTunesMobileDeviceFile.Exists)
				{
					PathBits.Add(iTunesMobileDeviceFile.DirectoryName);
				}
			}

			object RegistrySupportDir = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Apple Inc.\Apple Application Support", "InstallDir", Environment.CurrentDirectory);
			if (RegistrySupportDir != null)
			{
				DirectoryInfo ApplicationSupportDirectory = new DirectoryInfo(RegistrySupportDir.ToString());

				if (ApplicationSupportDirectory.Exists)
				{
					PathBits.Add(ApplicationSupportDirectory.FullName);
				}
			}

			// Add some guesses as well
			DirectoryInfo AppleMobileDeviceSupport = new DirectoryInfo(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFiles) + @"\Apple\Mobile Device Support");
			if (AppleMobileDeviceSupport.Exists)
			{
				PathBits.Add(AppleMobileDeviceSupport.FullName);
			}

			DirectoryInfo AppleMobileDeviceSupportX86 = new DirectoryInfo(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFilesX86) + @"\Apple\Mobile Device Support");
			if ((AppleMobileDeviceSupport != AppleMobileDeviceSupportX86) && (AppleMobileDeviceSupportX86.Exists))
			{
				PathBits.Add(AppleMobileDeviceSupportX86.FullName);
			}

			// add the rest of the path
			PathBits.Add(Environment.GetEnvironmentVariable("Path"));

			// Set the path from all the individual bits
			Environment.SetEnvironmentVariable("Path", string.Join(";", PathBits));
		}

		public int NotificationSubscribe(DeviceNotificationCallback DeviceCallbackHandle)
		{
			return AMDeviceMethods.NotificationSubscribe(DeviceCallbackHandle);
		}

		public IntPtr CopyDeviceIdentifier(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.CopyDeviceIdentifier(device);
		}

		public int Connect(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Connect(device);
		}

		public IntPtr CopyValue(TypedPtr<AppleMobileDeviceConnection> device, uint unknown, TypedPtr<CFString> cfstring)
		{
			return AMDeviceMethods.CopyValue(device, unknown, cfstring);
		}

		public int Disconnect(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Disconnect(device);
		}

		public int GetConnectionID(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.GetConnectionID(device);
		}

		public int IsPaired(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.IsPaired(device);
		}

		public int Pair(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Pair(device);
		}

		public int Unpair(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Unpair(device);
		}

		public int ValidatePairing(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.ValidatePairing(device);
		}

		public int LookupApplications(TypedPtr<AppleMobileDeviceConnection> device, IntPtr options, out Dictionary<string, object> AppBundles)
		{
			int Result = AMDeviceMethods.LookupApplications(device, options, out AppBundles);
			return Result;
		}

		public int StartSession(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.StartSession((IntPtr)device);
		}

		public int StopSession(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.StopSession((IntPtr)device);
		}

		public int StartHouseArrestService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2)
		{
			return AMDeviceMethods.StartHouseArrestService((IntPtr)device, (IntPtr)bundleName, unknown1, ref handle, unknown2);
		}

		public int StartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, ref IntPtr handle, IntPtr unknown)
		{
			return AMDeviceMethods.StartService((IntPtr)device, (IntPtr)serviceName, ref handle, unknown);
		}

		public int SecureStartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, int flagsPassInZero, ref IntPtr handle)
		{
			return AMDeviceMethods.SecureStartService((IntPtr)device, (IntPtr)serviceName, flagsPassInZero, ref handle);
		}
		
		public string ServiceConnectionReceive(IntPtr handle)
		{
			return AMDeviceMethods.ServiceConnectionReceive(handle);
		}

		public int RestoreRegisterForDeviceNotifications(
			DeviceRestoreNotificationCallback dfu_connect,
			DeviceRestoreNotificationCallback recovery_connect,
			DeviceRestoreNotificationCallback dfu_disconnect,
			DeviceRestoreNotificationCallback recovery_disconnect,
			uint unknown0,
			IntPtr user_info)
		{
			return AMDeviceMethods.AMRestoreRegisterForDeviceNotifications(dfu_connect, recovery_connect, dfu_disconnect, recovery_disconnect, unknown0, user_info);
		}

		public int SecureUninstallApplication(
			IntPtr serviceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFString> ApplicationIdentifer,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureUninstallApplication(serviceConnection, DeviceIfConnIsNull, ApplicationIdentifer, ClientOptions, ProgressCallback, UserData);
		}

		public int SecureInstallApplication(
			IntPtr serviceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFURL> UrlPath,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureInstallApplication(serviceConnection, DeviceIfConnIsNull, UrlPath, ClientOptions, ProgressCallback, UserData);
		}

		public int SecureUpgradeApplication(
			IntPtr ServiceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFURL> UrlPath,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureUpgradeApplication(ServiceConnection, DeviceIfConnIsNull, UrlPath, ClientOptions, ProgressCallback, UserData);
		}

		public int ConnectionClose(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionClose(conn);
		}

		public int ConnectionInvalidate(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionInvalidate(conn);
		}

		public int ConnectionIsValid(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionIsValid(conn);
		}

		public int ConnectionOpen(IntPtr handle, uint io_timeout, out TypedPtr<AFCCommConnection> OutConn)
		{
			return AFC.ConnectionOpen(handle, io_timeout, out OutConn);
		}

		public int DeviceInfoOpen(IntPtr handle, ref IntPtr dict)
		{
			return AFC.DeviceInfoOpen(handle, ref dict);
		}

		public int DirectoryClose(TypedPtr<AFCCommConnection> conn, IntPtr dir)
		{
			return AFC.DirectoryClose(conn, dir);
		}

		public int DirectoryCreate(TypedPtr<AFCCommConnection> conn, string path)
		{
			return AFC.DirectoryCreate(conn, path);
		}

		public int DirectoryOpen(TypedPtr<AFCCommConnection> conn, string path, ref IntPtr dir)
		{
			return AFC.DirectoryOpen(conn, path, ref dir);
		}

		public int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref IntPtr dirent)
		{
			return AFC.DirectoryRead(conn, dir, ref dirent);
		}

		public int FileInfoOpen(TypedPtr<AFCCommConnection> conn, string path, out TypedPtr<AFCDictionary> OutDict)
		{
			return AFC.FileInfoOpen(conn, path, out OutDict);
		}

		public int FileRefClose(TypedPtr<AFCCommConnection> conn, Int64 handle)
		{
			return AFC.FileRefClose(conn, handle);
		}

		public int FileRefOpen(TypedPtr<AFCCommConnection> conn, string path, Int64 mode, out Int64 handle)
		{
			return AFC.FileRefOpen(conn, path, mode, out handle);
		}

		public int FileRefRead(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, ref uint len)
		{
			return AFC.FileRefRead(conn, handle, buffer, ref len);
		}

		public int FileRefSeek(TypedPtr<AFCCommConnection> conn, Int64 handle, Int64 pos, Int64 origin)
		{
			return AFC.FileRefSeek(conn, handle, pos, origin);
		}

		public int FileRefSetFileSize(TypedPtr<AFCCommConnection> conn, Int64 handle, uint size)
		{
			return AFC.FileRefSetFileSize(conn, handle, size);
		}

		public int FileRefTell(TypedPtr<AFCCommConnection> conn, Int64 handle, ref uint position)
		{
			return AFC.FileRefTell(conn, handle, ref position);
		}

		public int FileRefWrite(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, uint len)
		{
			return AFC.FileRefWrite(conn, handle, buffer, len);
		}

		public int FlushData(TypedPtr<AFCCommConnection> conn, Int64 handle)
		{
			return AFC.FlushData(conn, handle);
		}

		public int KeyValueClose(TypedPtr<AFCDictionary> dict)
		{
			return AFC.KeyValueClose(dict);
		}

		public int KeyValueRead(TypedPtr<AFCDictionary> dict, out IntPtr key, out IntPtr val)
		{
			return AFC.KeyValueRead(dict, out key, out val);
		}

		public int RemovePath(TypedPtr<AFCCommConnection> conn, string path)
		{
			return AFC.RemovePath(conn, path);
		}

		public int RenamePath(TypedPtr<AFCCommConnection> conn, string OldPath, string NewPath)
		{
			return AFC.RenamePath(conn, OldPath, NewPath);
		}

		#region Application Methods
		public class AMDeviceMethods
		{
			// Need to have the path manipulation code in static MobileDevice() to run
			//			static int ForceMobileDeviceConstructorToRun;
			static AMDeviceMethods()
			{
				//				ForceMobileDeviceConstructorToRun = ForceStaticInit;
			}

			public static IntPtr CopyDeviceIdentifier(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceCopyDeviceIdentifier((IntPtr)device);
			}

			public static int Connect(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceConnect((IntPtr)device);
			}

			public static IntPtr CopyValue(TypedPtr<AppleMobileDeviceConnection> device, uint unknown, TypedPtr<CFString> cfstring)
			{
				return AMDeviceCopyValue((IntPtr)device, unknown, (IntPtr)cfstring);
			}

			public static int Disconnect(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceDisconnect((IntPtr)device);
			}

			public static int GetConnectionID(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceGetConnectionID((IntPtr)device);
			}

			public static int IsPaired(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceIsPaired((IntPtr)device);
			}

			public static int Pair(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDevicePair((IntPtr)device);
			}

			public static int Unpair(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceUnpair((IntPtr)device);
			}

			public static int ValidatePairing(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceValidatePairing((IntPtr)device);
			}

			public static int LookupApplications(TypedPtr<AppleMobileDeviceConnection> device, IntPtr options, out Dictionary<string, object> AppBundles)
			{
				IntPtr UntypedDict = IntPtr.Zero;
				int Result = AMDeviceLookupApplications((IntPtr)device, options, ref UntypedDict);

				AppBundles = MobileDevice.ConvertCFDictionaryToDictionaryString(new TypedPtr<CFDictionary>(UntypedDict));

				return Result;
			}

			public static int StartSession(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceStartSession((IntPtr)device);
			}

			public static int StopSession(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceStopSession((IntPtr)device);
			}

			public static int StartHouseArrestService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2)
			{
				return AMDeviceStartHouseArrestService((IntPtr)device, (IntPtr)bundleName, unknown1, ref handle, unknown2);
			}

			public static int StartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, ref IntPtr handle, IntPtr unknown)
			{
				return AMDeviceStartService((IntPtr)device, (IntPtr)serviceName, ref handle, unknown);
			}

			public static int SecureStartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, int flagsPassInZero, ref IntPtr handle)
			{
				return AMDeviceSecureStartService((IntPtr)device, (IntPtr)serviceName, flagsPassInZero, ref handle);
			}
		
			public static string ServiceConnectionReceive(IntPtr handle)
			{
				StringBuilder sb = new StringBuilder(1024);
				AMDServiceConnectionReceive(handle, sb, sb.Capacity, 0);
				return sb.ToString();
			}

			public static int UninstallApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> ApplicationIdentifier,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceUninstallApplication((IntPtr)device, (IntPtr)ApplicationIdentifier, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int SecureUninstallApplication(
				IntPtr serviceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFString> ApplicationIdentifer,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureUninstallApplication(serviceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)ApplicationIdentifer, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int SecureInstallApplication(
				IntPtr serviceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFURL> UrlPath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureInstallApplication(serviceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)UrlPath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int TransferApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> InPackagePath,
				IntPtr UnknownButUnused,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceTransferApplication((IntPtr)device, (IntPtr)InPackagePath, UnknownButUnused, ProgressCallback, UserData);
			}

			public static int SecureUpgradeApplication(
				IntPtr ServiceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFURL> UrlPath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureUpgradeApplication(ServiceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)UrlPath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int InstallApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> FilePath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceInstallApplication((IntPtr)device, (IntPtr)FilePath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int NotificationSubscribe(DeviceNotificationCallback DeviceCallbackHandle)
			{
				IntPtr notification;
				return AMDeviceNotificationSubscribe(DeviceCallbackHandle, 0, 0, 0, out notification);
			}

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDeviceCopyDeviceIdentifier(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceConnect(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDeviceCopyValue(IntPtr/*AppleMobileDeviceConnection*/ device, uint unknown, IntPtr/*CFString*/ cfstring);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceDisconnect(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceGetConnectionID(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceIsPaired(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDevicePair(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceUnpair(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceValidatePairing(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceLookupApplications(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr options, ref IntPtr appBundles);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceNotificationSubscribe(DeviceNotificationCallback callback, uint unused1, uint unused2, uint unused3, out IntPtr am_device_notification_ptr);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartHouseArrestService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ serviceName, ref IntPtr handle, IntPtr unknown);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureStartService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ serviceName, int flagsPassInZero, ref IntPtr handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDServiceConnectionReceive(IntPtr handle, StringBuilder buffer, int bufferLen, int unknown);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartSession(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStopSession(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMRestoreModeDeviceCreate(uint unknown0, int connection_id, uint unknown1);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			public extern static int AMRestoreRegisterForDeviceNotifications(
				DeviceRestoreNotificationCallback dfu_connect,
				DeviceRestoreNotificationCallback recovery_connect,
				DeviceRestoreNotificationCallback dfu_disconnect,
				DeviceRestoreNotificationCallback recovery_disconnect,
				uint unknown0,
				IntPtr user_info);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceUninstallApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ ApplicationIdentifier,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureUninstallApplication(
				IntPtr serviceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFString*/ ApplicationIdentifer,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureInstallApplication(
				IntPtr serviceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFURL*/ UrlPath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceTransferApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ InPackagePath,
				IntPtr UnknownButUnused,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureUpgradeApplication(
				IntPtr ServiceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFURL*/ UrlPath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceInstallApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ FilePath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			// the unknown goes into the 4th dword in the connection struct.  if non-zero, winsock send() won't get called in AMDServiceConnectionSend AFAIK
			// the only option is "CloseOnInvalidate" (CFBoolean type, defaults to false)
			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDServiceConnectionCreate(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr UnknownTypically0, IntPtr OptionsDictionary);


			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMSBeginSync(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr OuterIn8, IntPtr OuterIn12, IntPtr OuterIn16);
		}
		#endregion

		#region AFC Operations

		public int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref string buffer)
		{
			int ret;

			IntPtr ptr = IntPtr.Zero;
			ret = AFC.DirectoryRead((IntPtr)conn, dir, ref ptr);
			if ((ret == 0) && (ptr != IntPtr.Zero))
			{
				buffer = Marshal.PtrToStringAnsi(ptr);
			}
			else
			{
				buffer = null;
			}
			return ret;
		}

		public class AFC
		{
			public static int ConnectionClose(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionClose((IntPtr)conn);
			}

			public static int ConnectionInvalidate(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionInvalidate((IntPtr)conn);
			}

			public static int ConnectionIsValid(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionIsValid((IntPtr)conn);
			}

			public static int ConnectionOpen(IntPtr handle, uint io_timeout, out TypedPtr<AFCCommConnection> OutConn)
			{
				IntPtr Conn;
				int Result = AFCConnectionOpen(handle, io_timeout, out Conn);
				OutConn = Conn;
				return Result;
			}

			public static int DeviceInfoOpen(IntPtr handle, ref IntPtr dict)
			{
				return AFCDeviceInfoOpen(handle, ref dict);
			}

			public static int DirectoryClose(TypedPtr<AFCCommConnection> conn, IntPtr dir)
			{
				return AFCDirectoryClose((IntPtr)conn, dir);
			}

			public static int DirectoryCreate(TypedPtr<AFCCommConnection> conn, string path)
			{
				return AFCDirectoryCreate((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path));
			}

			public static int DirectoryOpen(TypedPtr<AFCCommConnection> conn, string path, ref IntPtr dir)
			{
				return AFCDirectoryOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), ref dir);
			}

			public static int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref IntPtr dirent)
			{
				return AFCDirectoryRead((IntPtr)conn, dir, ref dirent);
			}

			public static int FileInfoOpen(TypedPtr<AFCCommConnection> conn, string path, out TypedPtr<AFCDictionary> OutDict)
			{
				IntPtr UntypedDict;
				int Result = AFCFileInfoOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), out UntypedDict);
				OutDict = UntypedDict;

				return Result;
			}

			public static int FileRefClose(TypedPtr<AFCCommConnection> conn, Int64 handle)
			{
				return AFCFileRefClose((IntPtr)conn, handle);
			}

			public static int FileRefOpen(TypedPtr<AFCCommConnection> conn, string path, Int64 mode, out Int64 handle)
			{
				return AFCFileRefOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), mode, out handle);
			}

			public static int FileRefRead(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, ref uint len)
			{
				return AFCFileRefRead((IntPtr)conn, handle, buffer, ref len);
			}

			public static int FileRefSetFileSize(TypedPtr<AFCCommConnection> conn, Int64 handle, uint size)
			{
				return AFCFileRefSetFileSize((IntPtr)conn, handle, size);
			}

			public static int FileRefSeek(TypedPtr<AFCCommConnection> conn, Int64 handle, Int64 pos, Int64 origin)
			{
				return AFCFileRefSeek((IntPtr)conn, handle, pos, origin);
			}

			public static int FileRefTell(TypedPtr<AFCCommConnection> conn, Int64 handle, ref uint position)
			{
				return AFCFileRefTell((IntPtr)conn, handle, ref position);
			}

			public static int FileRefWrite(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, uint len)
			{
				return AFCFileRefWrite((IntPtr)conn, handle, buffer, len);
			}

			public static int FlushData(TypedPtr<AFCCommConnection> conn, Int64 handle)
			{
				return AFCFlushData((IntPtr)conn, handle);
			}

			public static int KeyValueClose(TypedPtr<AFCDictionary> dict)
			{
				return AFCKeyValueClose((IntPtr)dict);
			}

			public static int KeyValueRead(TypedPtr<AFCDictionary> dict, out IntPtr key, out IntPtr val)
			{
				return AFCKeyValueRead((IntPtr)dict, out key, out val);
			}

			public static int RemovePath(TypedPtr<AFCCommConnection> conn, string path)
			{
				return AFCRemovePath((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path));
			}

			public static int RenamePath(TypedPtr<AFCCommConnection> conn, string OldPath, string NewPath)
			{
				return AFCRenamePath((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(OldPath), MobileDevice.StringToFileSystemRepresentation(NewPath));
			}

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionClose(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionInvalidate(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionIsValid(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionOpen(IntPtr handle, uint io_timeout, out IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDeviceInfoOpen(IntPtr handle, ref IntPtr dict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryClose(IntPtr/*AFCCommConnection*/ conn, IntPtr dir);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryCreate(IntPtr/*AFCCommConnection*/ conn, byte[] path);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, ref IntPtr dir);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryRead(IntPtr/*AFCCommConnection*/ conn, IntPtr dir, ref IntPtr dirent);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileInfoOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, out IntPtr OutDict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefClose(IntPtr/*AFCCommConnection*/ conn, Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, Int64 mode, out Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefRead(IntPtr/*AFCCommConnection*/ conn, Int64 handle, byte[] buffer, ref uint len);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefSeek(IntPtr/*AFCCommConnection*/ conn, Int64 handle, Int64 pos, Int64 origin);

			// FIXME - not working, arguments?
			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			public extern static int AFCFileRefSetFileSize(IntPtr/*AFCCommConnection*/ conn, Int64 handle, uint size);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefTell(IntPtr/*AFCCommConnection*/ conn, Int64 handle, ref uint position);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefWrite(IntPtr/*AFCCommConnection*/ conn, Int64 handle, byte[] buffer, uint len);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFlushData(IntPtr/*AFCCommConnection*/ conn, Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCKeyValueClose(IntPtr dict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCKeyValueRead(IntPtr dict, out IntPtr key, out IntPtr val);


			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCRemovePath(IntPtr/*AFCCommConnection*/ conn, byte[] path);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCRenamePath(IntPtr/*AFCCommConnection*/ conn, byte[] old_path, byte[] new_path);
		}

		#endregion
	}

	internal class MobileDeviceWiniTunes12 : MobileDeviceImpl
	{
		//		static readonly int ForceStaticInit = 42;
		const string DLLName = @"MobileDevice.dll";

		static MobileDeviceWiniTunes12()
		{
			List<string> PathBits = new List<string>();

			// Try to add the paths from the registry (they aren't always available on newer iTunes installs though)
			object RegistryDllPath = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Apple Inc.\Apple Mobile Device Support\Shared", "MobileDeviceDLL", DLLName);
			if (RegistryDllPath != null)
			{
				FileInfo iTunesMobileDeviceFile = new FileInfo(RegistryDllPath.ToString());

				if (iTunesMobileDeviceFile.Exists)
				{
					PathBits.Add(iTunesMobileDeviceFile.DirectoryName);
				}
			}

			object RegistrySupportDir = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Apple Inc.\Apple Application Support", "InstallDir", Environment.CurrentDirectory);
			if (RegistrySupportDir != null)
			{
				DirectoryInfo ApplicationSupportDirectory = new DirectoryInfo(RegistrySupportDir.ToString());

				if (ApplicationSupportDirectory.Exists)
				{
					PathBits.Add(ApplicationSupportDirectory.FullName);
				}
			}

			// Add some guesses as well
			DirectoryInfo AppleMobileDeviceSupport = new DirectoryInfo(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFiles) + @"\Apple\Mobile Device Support");
			if (AppleMobileDeviceSupport.Exists)
			{
				PathBits.Add(AppleMobileDeviceSupport.FullName);
			}

			DirectoryInfo AppleMobileDeviceSupportX86 = new DirectoryInfo(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFilesX86) + @"\Apple\Mobile Device Support");
			if ((AppleMobileDeviceSupport != AppleMobileDeviceSupportX86) && (AppleMobileDeviceSupportX86.Exists))
			{
				PathBits.Add(AppleMobileDeviceSupportX86.FullName);
			}

			// add the rest of the path
			PathBits.Add(Environment.GetEnvironmentVariable("Path"));

			// Set the path from all the individual bits
			Environment.SetEnvironmentVariable("Path", string.Join(";", PathBits));
		}

		public int NotificationSubscribe(DeviceNotificationCallback DeviceCallbackHandle)
		{
			return AMDeviceMethods.NotificationSubscribe(DeviceCallbackHandle);
		}

		public IntPtr CopyDeviceIdentifier(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.CopyDeviceIdentifier(device);
		}

		public int Connect(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Connect(device);
		}

		public IntPtr CopyValue(TypedPtr<AppleMobileDeviceConnection> device, uint unknown, TypedPtr<CFString> cfstring)
		{
			return AMDeviceMethods.CopyValue(device, unknown, cfstring);
		}

		public int Disconnect(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Disconnect(device);
		}

		public int GetConnectionID(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.GetConnectionID(device);
		}

		public int IsPaired(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.IsPaired(device);
		}

		public int Pair(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Pair(device);
		}

		public int Unpair(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.Unpair(device);
		}

		public int ValidatePairing(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.ValidatePairing(device);
		}

		public int LookupApplications(TypedPtr<AppleMobileDeviceConnection> device, IntPtr options, out Dictionary<string, object> AppBundles)
		{
			int Result = AMDeviceMethods.LookupApplications(device, options, out AppBundles);
			return Result;
		}

		public int StartSession(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.StartSession((IntPtr)device);
		}

		public int StopSession(TypedPtr<AppleMobileDeviceConnection> device)
		{
			return AMDeviceMethods.StopSession((IntPtr)device);
		}

		public int StartHouseArrestService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2)
		{
			return AMDeviceMethods.StartHouseArrestService((IntPtr)device, (IntPtr)bundleName, unknown1, ref handle, unknown2);
		}

		public int StartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, ref IntPtr handle, IntPtr unknown)
		{
			return AMDeviceMethods.StartService((IntPtr)device, (IntPtr)serviceName, ref handle, unknown);
		}

		public int SecureStartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, int flagsPassInZero, ref IntPtr handle)
		{
			return AMDeviceMethods.SecureStartService((IntPtr)device, (IntPtr)serviceName, flagsPassInZero, ref handle);
		}
		
		public string ServiceConnectionReceive(IntPtr handle)
		{
			return AMDeviceMethods.ServiceConnectionReceive(handle);
		}

		public int RestoreRegisterForDeviceNotifications(
			DeviceRestoreNotificationCallback dfu_connect,
			DeviceRestoreNotificationCallback recovery_connect,
			DeviceRestoreNotificationCallback dfu_disconnect,
			DeviceRestoreNotificationCallback recovery_disconnect,
			uint unknown0,
			IntPtr user_info)
		{
			return AMDeviceMethods.AMRestoreRegisterForDeviceNotifications(dfu_connect, recovery_connect, dfu_disconnect, recovery_disconnect, unknown0, user_info);
		}

		public int SecureUninstallApplication(
			IntPtr serviceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFString> ApplicationIdentifer,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureUninstallApplication(serviceConnection, DeviceIfConnIsNull, ApplicationIdentifer, ClientOptions, ProgressCallback, UserData);
		}

		public int SecureInstallApplication(
			IntPtr serviceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFURL> UrlPath,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureInstallApplication(serviceConnection, DeviceIfConnIsNull, UrlPath, ClientOptions, ProgressCallback, UserData);
		}

		public int SecureUpgradeApplication(
			IntPtr ServiceConnection,
			TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
			TypedPtr<CFURL> UrlPath,
			TypedPtr<CFDictionary> ClientOptions,
			DeviceInstallationCallback ProgressCallback,
			IntPtr UserData)
		{
			return AMDeviceMethods.SecureUpgradeApplication(ServiceConnection, DeviceIfConnIsNull, UrlPath, ClientOptions, ProgressCallback, UserData);
		}

		public int ConnectionClose(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionClose(conn);
		}

		public int ConnectionInvalidate(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionInvalidate(conn);
		}

		public int ConnectionIsValid(TypedPtr<AFCCommConnection> conn)
		{
			return AFC.ConnectionIsValid(conn);
		}

		public int ConnectionOpen(IntPtr handle, uint io_timeout, out TypedPtr<AFCCommConnection> OutConn)
		{
			return AFC.ConnectionOpen(handle, io_timeout, out OutConn);
		}

		public int DeviceInfoOpen(IntPtr handle, ref IntPtr dict)
		{
			return AFC.DeviceInfoOpen(handle, ref dict);
		}

		public int DirectoryClose(TypedPtr<AFCCommConnection> conn, IntPtr dir)
		{
			return AFC.DirectoryClose(conn, dir);
		}

		public int DirectoryCreate(TypedPtr<AFCCommConnection> conn, string path)
		{
			return AFC.DirectoryCreate(conn, path);
		}

		public int DirectoryOpen(TypedPtr<AFCCommConnection> conn, string path, ref IntPtr dir)
		{
			return AFC.DirectoryOpen(conn, path, ref dir);
		}

		public int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref IntPtr dirent)
		{
			return AFC.DirectoryRead(conn, dir, ref dirent);
		}

		public int FileInfoOpen(TypedPtr<AFCCommConnection> conn, string path, out TypedPtr<AFCDictionary> OutDict)
		{
			return AFC.FileInfoOpen(conn, path, out OutDict);
		}

		public int FileRefClose(TypedPtr<AFCCommConnection> conn, Int64 handle)
		{
			return AFC.FileRefClose(conn, handle);
		}

		public int FileRefOpen(TypedPtr<AFCCommConnection> conn, string path, Int64 mode, out Int64 handle)
		{
			return AFC.FileRefOpen(conn, path, mode, out handle);
		}

		public int FileRefRead(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, ref uint len)
		{
			return AFC.FileRefRead(conn, handle, buffer, ref len);
		}

		public int FileRefSeek(TypedPtr<AFCCommConnection> conn, Int64 handle, Int64 pos, Int64 origin)
		{
			return AFC.FileRefSeek(conn, handle, pos, origin);
		}

		public int FileRefSetFileSize(TypedPtr<AFCCommConnection> conn, Int64 handle, uint size)
		{
			return AFC.FileRefSetFileSize(conn, handle, size);
		}

		public int FileRefTell(TypedPtr<AFCCommConnection> conn, Int64 handle, ref uint position)
		{
			return AFC.FileRefTell(conn, handle, ref position);
		}

		public int FileRefWrite(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, uint len)
		{
			return AFC.FileRefWrite(conn, handle, buffer, len);
		}

		public int FlushData(TypedPtr<AFCCommConnection> conn, Int64 handle)
		{
			return AFC.FlushData(conn, handle);
		}

		public int KeyValueClose(TypedPtr<AFCDictionary> dict)
		{
			return AFC.KeyValueClose(dict);
		}

		public int KeyValueRead(TypedPtr<AFCDictionary> dict, out IntPtr key, out IntPtr val)
		{
			return AFC.KeyValueRead(dict, out key, out val);
		}

		public int RemovePath(TypedPtr<AFCCommConnection> conn, string path)
		{
			return AFC.RemovePath(conn, path);
		}

		public int RenamePath(TypedPtr<AFCCommConnection> conn, string OldPath, string NewPath)
		{
			return AFC.RenamePath(conn, OldPath, NewPath);
		}

		#region Application Methods
		public class AMDeviceMethods
		{
			// Need to have the path manipulation code in static MobileDevice() to run
			//			static int ForceMobileDeviceConstructorToRun;
			static AMDeviceMethods()
			{
				//				ForceMobileDeviceConstructorToRun = ForceStaticInit;
			}

			public static IntPtr CopyDeviceIdentifier(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceCopyDeviceIdentifier((IntPtr)device);
			}

			public static int Connect(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceConnect((IntPtr)device);
			}

			public static IntPtr CopyValue(TypedPtr<AppleMobileDeviceConnection> device, uint unknown, TypedPtr<CFString> cfstring)
			{
				return AMDeviceCopyValue((IntPtr)device, unknown, (IntPtr)cfstring);
			}

			public static int Disconnect(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceDisconnect((IntPtr)device);
			}

			public static int GetConnectionID(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceGetConnectionID((IntPtr)device);
			}

			public static int IsPaired(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceIsPaired((IntPtr)device);
			}

			public static int Pair(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDevicePair((IntPtr)device);
			}

			public static int Unpair(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceUnpair((IntPtr)device);
			}

			public static int ValidatePairing(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceValidatePairing((IntPtr)device);
			}

			public static int LookupApplications(TypedPtr<AppleMobileDeviceConnection> device, IntPtr options, out Dictionary<string, object> AppBundles)
			{
				IntPtr UntypedDict = IntPtr.Zero;
				int Result = AMDeviceLookupApplications((IntPtr)device, options, ref UntypedDict);

				AppBundles = MobileDevice.ConvertCFDictionaryToDictionaryString(new TypedPtr<CFDictionary>(UntypedDict));

				return Result;
			}

			public static int StartSession(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceStartSession((IntPtr)device);
			}

			public static int StopSession(TypedPtr<AppleMobileDeviceConnection> device)
			{
				return AMDeviceStopSession((IntPtr)device);
			}

			public static int StartHouseArrestService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2)
			{
				return AMDeviceStartHouseArrestService((IntPtr)device, (IntPtr)bundleName, unknown1, ref handle, unknown2);
			}

			public static int StartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, ref IntPtr handle, IntPtr unknown)
			{
				return AMDeviceStartService((IntPtr)device, (IntPtr)serviceName, ref handle, unknown);
			}

			public static int SecureStartService(TypedPtr<AppleMobileDeviceConnection> device, TypedPtr<CFString> serviceName, int flagsPassInZero, ref IntPtr handle)
			{
				return AMDeviceSecureStartService((IntPtr)device, (IntPtr)serviceName, flagsPassInZero, ref handle);
			}
		
			public static string ServiceConnectionReceive(IntPtr handle)
			{
				StringBuilder sb = new StringBuilder(1024);
				AMDServiceConnectionReceive(handle, sb, sb.Capacity, 0);
				return sb.ToString();
			}

			public static int UninstallApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> ApplicationIdentifier,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceUninstallApplication((IntPtr)device, (IntPtr)ApplicationIdentifier, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int SecureUninstallApplication(
				IntPtr serviceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFString> ApplicationIdentifer,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureUninstallApplication(serviceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)ApplicationIdentifer, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int SecureInstallApplication(
				IntPtr serviceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFURL> UrlPath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureInstallApplication(serviceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)UrlPath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int TransferApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> InPackagePath,
				IntPtr UnknownButUnused,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceTransferApplication((IntPtr)device, (IntPtr)InPackagePath, UnknownButUnused, ProgressCallback, UserData);
			}

			public static int SecureUpgradeApplication(
				IntPtr ServiceConnection,
				TypedPtr<AppleMobileDeviceConnection> DeviceIfConnIsNull,
				TypedPtr<CFURL> UrlPath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceSecureUpgradeApplication(ServiceConnection, (IntPtr)DeviceIfConnIsNull, (IntPtr)UrlPath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int InstallApplication(
				TypedPtr<AppleMobileDeviceConnection> device,
				TypedPtr<CFString> FilePath,
				TypedPtr<CFDictionary> ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData)
			{
				return AMDeviceInstallApplication((IntPtr)device, (IntPtr)FilePath, (IntPtr)ClientOptions, ProgressCallback, UserData);
			}

			public static int NotificationSubscribe(DeviceNotificationCallback DeviceCallbackHandle)
			{
				IntPtr notification;
				return AMDeviceNotificationSubscribe(DeviceCallbackHandle, 0, 0, 0, out notification);
			}

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDeviceCopyDeviceIdentifier(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceConnect(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDeviceCopyValue(IntPtr/*AppleMobileDeviceConnection*/ device, uint unknown, IntPtr/*CFString*/ cfstring);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceDisconnect(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceGetConnectionID(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceIsPaired(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDevicePair(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceUnpair(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceValidatePairing(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceLookupApplications(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr options, ref IntPtr appBundles);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceNotificationSubscribe(DeviceNotificationCallback callback, uint unused1, uint unused2, uint unused3, out IntPtr am_device_notification_ptr);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartHouseArrestService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ bundleName, IntPtr unknown1, ref IntPtr handle, int unknown2);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ serviceName, ref IntPtr handle, IntPtr unknown);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureStartService(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr/*CFString*/ serviceName, int flagsPassInZero, ref IntPtr handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDServiceConnectionReceive(IntPtr handle, StringBuilder buffer, int bufferLen, int unknown);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStartSession(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceStopSession(IntPtr/*AppleMobileDeviceConnection*/ device);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMRestoreModeDeviceCreate(uint unknown0, int connection_id, uint unknown1);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			public extern static int AMRestoreRegisterForDeviceNotifications(
				DeviceRestoreNotificationCallback dfu_connect,
				DeviceRestoreNotificationCallback recovery_connect,
				DeviceRestoreNotificationCallback dfu_disconnect,
				DeviceRestoreNotificationCallback recovery_disconnect,
				uint unknown0,
				IntPtr user_info);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceUninstallApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ ApplicationIdentifier,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureUninstallApplication(
				IntPtr serviceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFString*/ ApplicationIdentifer,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureInstallApplication(
				IntPtr serviceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFURL*/ UrlPath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceTransferApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ InPackagePath,
				IntPtr UnknownButUnused,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceSecureUpgradeApplication(
				IntPtr ServiceConnection,
				IntPtr/*AppleMobileDeviceConnection*/ DeviceIfConnIsNull,
				IntPtr/*CFURL*/ UrlPath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMDeviceInstallApplication(
				IntPtr/*AppleMobileDeviceConnection*/ device,
				IntPtr/*CFString*/ FilePath,
				IntPtr/*CFDictionary*/ ClientOptions,
				DeviceInstallationCallback ProgressCallback,
				IntPtr UserData);

			// the unknown goes into the 4th dword in the connection struct.  if non-zero, winsock send() won't get called in AMDServiceConnectionSend AFAIK
			// the only option is "CloseOnInvalidate" (CFBoolean type, defaults to false)
			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static IntPtr AMDServiceConnectionCreate(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr UnknownTypically0, IntPtr OptionsDictionary);


			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AMSBeginSync(IntPtr/*AppleMobileDeviceConnection*/ device, IntPtr OuterIn8, IntPtr OuterIn12, IntPtr OuterIn16);
		}
		#endregion

		#region AFC Operations

		public int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref string buffer)
		{
			int ret;

			IntPtr ptr = IntPtr.Zero;
			ret = AFC.DirectoryRead((IntPtr)conn, dir, ref ptr);
			if ((ret == 0) && (ptr != IntPtr.Zero))
			{
				buffer = Marshal.PtrToStringAnsi(ptr);
			}
			else
			{
				buffer = null;
			}
			return ret;
		}

		public class AFC
		{
			public static int ConnectionClose(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionClose((IntPtr)conn);
			}

			public static int ConnectionInvalidate(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionInvalidate((IntPtr)conn);
			}

			public static int ConnectionIsValid(TypedPtr<AFCCommConnection> conn)
			{
				return AFCConnectionIsValid((IntPtr)conn);
			}

			public static int ConnectionOpen(IntPtr handle, uint io_timeout, out TypedPtr<AFCCommConnection> OutConn)
			{
				IntPtr Conn;
				int Result = AFCConnectionOpen(handle, io_timeout, out Conn);
				OutConn = Conn;
				return Result;
			}

			public static int DeviceInfoOpen(IntPtr handle, ref IntPtr dict)
			{
				return AFCDeviceInfoOpen(handle, ref dict);
			}

			public static int DirectoryClose(TypedPtr<AFCCommConnection> conn, IntPtr dir)
			{
				return AFCDirectoryClose((IntPtr)conn, dir);
			}

			public static int DirectoryCreate(TypedPtr<AFCCommConnection> conn, string path)
			{
				return AFCDirectoryCreate((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path));
			}

			public static int DirectoryOpen(TypedPtr<AFCCommConnection> conn, string path, ref IntPtr dir)
			{
				return AFCDirectoryOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), ref dir);
			}

			public static int DirectoryRead(TypedPtr<AFCCommConnection> conn, IntPtr dir, ref IntPtr dirent)
			{
				return AFCDirectoryRead((IntPtr)conn, dir, ref dirent);
			}

			public static int FileInfoOpen(TypedPtr<AFCCommConnection> conn, string path, out TypedPtr<AFCDictionary> OutDict)
			{
				IntPtr UntypedDict;
				int Result = AFCFileInfoOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), out UntypedDict);
				OutDict = UntypedDict;

				return Result;
			}

			public static int FileRefClose(TypedPtr<AFCCommConnection> conn, Int64 handle)
			{
				return AFCFileRefClose((IntPtr)conn, handle);
			}

			public static int FileRefOpen(TypedPtr<AFCCommConnection> conn, string path, Int64 mode, out Int64 handle)
			{
				return AFCFileRefOpen((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path), mode, out handle);
			}

			public static int FileRefRead(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, ref uint len)
			{
				return AFCFileRefRead((IntPtr)conn, handle, buffer, ref len);
			}

			public static int FileRefSetFileSize(TypedPtr<AFCCommConnection> conn, Int64 handle, uint size)
			{
				return AFCFileRefSetFileSize((IntPtr)conn, handle, size);
			}

			public static int FileRefSeek(TypedPtr<AFCCommConnection> conn, Int64 handle, Int64 pos, Int64 origin)
			{
				return AFCFileRefSeek((IntPtr)conn, handle, pos, origin);
			}

			public static int FileRefTell(TypedPtr<AFCCommConnection> conn, Int64 handle, ref uint position)
			{
				return AFCFileRefTell((IntPtr)conn, handle, ref position);
			}

			public static int FileRefWrite(TypedPtr<AFCCommConnection> conn, Int64 handle, byte[] buffer, uint len)
			{
				return AFCFileRefWrite((IntPtr)conn, handle, buffer, len);
			}

			public static int FlushData(TypedPtr<AFCCommConnection> conn, Int64 handle)
			{
				return AFCFlushData((IntPtr)conn, handle);
			}

			public static int KeyValueClose(TypedPtr<AFCDictionary> dict)
			{
				return AFCKeyValueClose((IntPtr)dict);
			}

			public static int KeyValueRead(TypedPtr<AFCDictionary> dict, out IntPtr key, out IntPtr val)
			{
				return AFCKeyValueRead((IntPtr)dict, out key, out val);
			}

			public static int RemovePath(TypedPtr<AFCCommConnection> conn, string path)
			{
				return AFCRemovePath((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(path));
			}

			public static int RenamePath(TypedPtr<AFCCommConnection> conn, string OldPath, string NewPath)
			{
				return AFCRenamePath((IntPtr)conn, MobileDevice.StringToFileSystemRepresentation(OldPath), MobileDevice.StringToFileSystemRepresentation(NewPath));
			}

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionClose(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionInvalidate(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionIsValid(IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCConnectionOpen(IntPtr handle, uint io_timeout, out IntPtr/*AFCCommConnection*/ conn);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDeviceInfoOpen(IntPtr handle, ref IntPtr dict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryClose(IntPtr/*AFCCommConnection*/ conn, IntPtr dir);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryCreate(IntPtr/*AFCCommConnection*/ conn, byte[] path);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, ref IntPtr dir);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCDirectoryRead(IntPtr/*AFCCommConnection*/ conn, IntPtr dir, ref IntPtr dirent);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileInfoOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, out IntPtr OutDict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefClose(IntPtr/*AFCCommConnection*/ conn, Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefOpen(IntPtr/*AFCCommConnection*/ conn, byte[] path, Int64 mode, out Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefRead(IntPtr/*AFCCommConnection*/ conn, Int64 handle, byte[] buffer, ref uint len);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefSeek(IntPtr/*AFCCommConnection*/ conn, Int64 handle, Int64 pos, Int64 origin);

			// FIXME - not working, arguments?
			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			public extern static int AFCFileRefSetFileSize(IntPtr/*AFCCommConnection*/ conn, Int64 handle, uint size);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefTell(IntPtr/*AFCCommConnection*/ conn, Int64 handle, ref uint position);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFileRefWrite(IntPtr/*AFCCommConnection*/ conn, Int64 handle, byte[] buffer, uint len);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCFlushData(IntPtr/*AFCCommConnection*/ conn, Int64 handle);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCKeyValueClose(IntPtr dict);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCKeyValueRead(IntPtr dict, out IntPtr key, out IntPtr val);


			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCRemovePath(IntPtr/*AFCCommConnection*/ conn, byte[] path);

			[DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
			private extern static int AFCRenamePath(IntPtr/*AFCCommConnection*/ conn, byte[] old_path, byte[] new_path);
		}

		#endregion
	}

    internal partial class MobileDevice
    {
		static public MobileDeviceImpl DeviceImpl = null;

        static MobileDevice()
        {
			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix)
			{
				DeviceImpl = new MobileDeviceOSX();
			}
			else
			{
				string dllPath11 = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "iTunesMobileDeviceDLL", null) as string;
				string dllPath12 = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "MobileDeviceDLL", null) as string;
				if (!String.IsNullOrEmpty(dllPath12) && File.Exists(dllPath12))
				{
					DeviceImpl = new MobileDeviceWiniTunes12();
				}
				else if (!String.IsNullOrEmpty(dllPath11) && File.Exists(dllPath11))
				{
					DeviceImpl = new MobileDeviceWiniTunes11();
				}
			}

            // Initialize the CoreFoundation bindings
            InitializeCoreFoundation();
        }

        /*
    Valid Value Names:
            ActivationState
            ActivationStateAcknowledged
            BasebandBootloaderVersion
            BasebandVersion
            BluetoothAddress
            BuildVersion
            DeviceCertificate
            DeviceClass
            DeviceName
            DevicePublicKey
            FirmwareVersion
            HostAttached
            IntegratedCircuitCardIdentity
            InternationalMobileEquipmentIdentity
            InternationalMobileSubscriberIdentity
            ModelNumber
            PhoneNumber
            ProductType
            ProductVersion
            ProtocolVersion
            RegionInfo
            SBLockdownEverRegisteredKey
            SIMStatus
            SerialNumber
            SomebodySetTimeZone
            TimeIntervalSince1970
            TimeZone
            TimeZoneOffsetFromUTC
            TrustedHostAttached
            UniqueDeviceID
            Uses24HourClock
            WiFiAddress
            iTunesHasConnected
 */

        public static string AMDeviceCopyValue(TypedPtr<AppleMobileDeviceConnection> device, string name)
        {
            IntPtr result = DeviceImpl.CopyValue(device, 0, CFStringMakeConstantString(name));
            if (result != IntPtr.Zero)
            {
                return MobileDevice.CFStringGetCString(result);
            }
            else
            {
                Console.WriteLine("Error: Call to AMDeviceCopyValue failed");
            }
            return String.Empty;
        }

        /// <summary>
        /// Returns an error string
        /// </summary>
        public static string GetErrorString(int ErrorValue)
        {
            string Result = GetSpecificErrorString(ErrorValue);

            if (Result == null)
            {
                Result = String.Format("Unknown error 0x{0:X}", ErrorValue);
            }

            return Result;
        }

        /// <summary>
        /// Returns a human readable description of the error from the mobile device or null if there isn't one
        /// </summary>
        public static string GetSpecificErrorString(int ErrorValue)
        {
            switch ((uint)ErrorValue)
            {
                case 0:
                    return "Success.";

                case 0xE8000002:
                    return "Bad Header Error";

                case 0xE8000003:
                    return "No Resources Error";

                case 0xE8000004:
                    return "Read Error";

                case 0xE8000005:
                    return "Write Error";

                case 0xE8000006:
                    return "Unknown Packet Error";

                case 0xE8000007:
                    return "Invalid Argument Error";

                case 0xE8000008:
                    return "Not Found Error";

                case 0xE8000009:
                    return "Is Directory Error";

                case 0xE800000A:
                    return "Permission Error";

                case 0xE800000B:
                    return "Not Connected Error";

                case 0xE800000C:
                    return "Time Out Error";

                case 0xE800000D:
                    return "Overrun Error";

                case 0xE800000E:
                    return "EOF Error";

                case 0xE800000F:
                    return "Unsupported Error";

                case 0xE8000010:
                    return "File Exists Error";

                case 0xE8000011:
                    return "Busy Error";

                case 0xE8000012:
                    return "Crypto Error";

                case 0xE8000013:
                    return "Invalid Response Error";

                case 0xE8000014:
                    return "Missing Key Error";

                case 0xE8000015:
                    return "Missing Value Error";

                case 0xE8000016:
                    return "Get Prohibited Error";

                case 0xE8000017:
                    return "Set Prohibited Error";

                case 0xE8000018:
                    return "Time Out Error";

                case 0xE8000019:
                    return "Immutable Value Error";

                case 0xE800001A:
                    return "Password Protected Error";

                case 0xE800001B:
                    return "Missing Host ID Error";

                case 0xE800001C:
                    return "Invalid Host ID Error";

                case 0xE800001D:
                    return "Session Active Error";

                case 0xE800001E:
                    return "Session Inactive Error";

                case 0xE800001F:
                    return "Missing Session ID Error";

                case 0xE8000020:
                    return "Invalid Session ID Error";

                case 0xE8000021:
                    return "Missing Service Error";

                case 0xE8000022:
                    return "Invalid Service Error";

                case 0xE8000023:
                    return "Invalid Checkin Error";

                case 0xE8000024:
                    return "Checkin Timeout Error";

                case 0xE8000025:
                    return "Missing Pair Record Error";

                case 0xE8000026:
                    return "Invalid Activation Record Error";

                case 0xE8000027:
                    return "Missing Activation Error";

                case 0xE8000028:
                    return "Wrong Droid Error";

                case 0xE8000029:
                    return "SU Verification Error";

                case 0xE800002A:
                    return "SU Patch Error";

                case 0xE800002B:
                    return "SU Firmware Error";

                case 0xE800002C:
                    return "Provisioning Profile Not Valid";

                case 0xE800002D:
                    return "Send Message Error";

                case 0xE800002E:
                    return "Receive Message Error";

                case 0xE800002F:
                    return "Missing Options Error";

                case 0xE8000030:
                    return "Missing Image Type Error";

                case 0xE8000031:
                    return "Digest Failed Error";

                case 0xE8000032:
                    return "Start Service Error";

                case 0xE8000033:
                    return "Invalid Disk Image Error";

                case 0xE8000034:
                    return "Missing Digest Error";

                case 0xE8000035:
                    return "Mux Error";

                case 0xE8000036:
                    return "Application Already Installed Error";

                case 0xE8000037:
                    return "Application Move Failed Error";

                case 0xE8000038:
                    return "Application SINF Capture Failed Error";

                case 0xE8000039:
                    return "Application Sandbox Failed Error";

                case 0xE800003A:
                    return "Application Verification Failed Error";

                case 0xE800003B:
                    return "Archive Destructor Failed Error";

                case 0xE800003C:
                    return "Bundle Verification Failed Error";

                case 0xE800003D:
                    return "Carrier Bundle Copy Failed Error";

                case 0xE800003E:
                    return "Carrier Bundle Directory Creation Failed Error";

                case 0xE800003F:
                    return "Carrier Bundle Missing Supported SIMs Error";

                case 0xE8000040:
                    return "Comm Center Notification Failed Error";

                case 0xE8000041:
                    return "COntainer Creation Failed Error";

                case 0xE8000042:
                    return "Container P0wn Failed Error";

                case 0xE8000043:
                    return "Container Removal Failed Error";

                case 0xE8000044:
                    return "Embedded Profile Install Failed Error";

                case 0xE8000045:
                    return "Error Error";

                case 0xE8000046:
                    return "Executable Twiddle Failed Error";

                case 0xE8000047:
                    return "Existence Check Failed Error";

                case 0xE8000048:
                    return "Install Map Upfate Failed Error";

                case 0xE8000049:
                    return "Manifest Capture Failed Error";

                case 0xE800004A:
                    return "Map Generation Failed Error";

                case 0xE800004B:
                    return "Missing Bundle Executable Error";

                case 0xE800004C:
                    return "Missing Bundle Identifier Error";

                case 0xE800004D:
                    return "Missing Bundle Path Error";

                case 0xE800004E:
                    return "Missing Container Error";

                case 0xE800004F:
                    return "Notification Failed Error";

                case 0xE8000050:
                    return "Package Extraction Failed Error";

                case 0xE8000051:
                    return "Package Inspection Failed Error";

                case 0xE8000052:
                    return "Package Move Failed Error";

                case 0xE8000053:
                    return "Path Conversion Failed Error";

                case 0xE8000054:
                    return "Restore Container Failed Error";

                case 0xE8000055:
                    return "Seatbelt Profile Removal Failed Error";

                case 0xE8000056:
                    return "Stage Creation Failed Error";

                case 0xE8000057:
                    return "Symlink Failed Error";

                case 0xE8000058:
                    return "iTunes Artwork Capture Failed Error";

                case 0xE8000059:
                    return "iTunes Metadata Capture Failed Error";

                case 0xE800005A:
                    return "Already Archived Error";

				case 0xE8008001:
					return "Unknown code signature related error (0xE8008001); your executable may be missing or misnamed (e.g., underscore issue)";
				
				case 0xE8008015:
                    return "No matching mobile provision for this application (0xE8008015).  Please ensure the mobile provision used includes the target device.";

                case 0xE8008016:
                    return "There is a problem with the entitlements for this application (0xE8008016).";

                case 0xE8008017:
                    return "Unable to validate signature (there is a problem with a signed code resource) (0xE8008017)";
					
                case 0xE8008018:
                    return "The identity used to sign the executable is no longer valid (0xE8008018)";

                case 0xE8008019:
                    return "Missing or invalid code signature (0xE8008019)";

                default:
                    return null;
            }
        }
    }

}
