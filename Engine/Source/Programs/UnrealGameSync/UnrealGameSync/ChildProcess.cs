// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32.SafeHandles;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class ChildProcess : IDisposable
	{
		const UInt32 JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000;

		[StructLayout(LayoutKind.Sequential)]
		class SECURITY_ATTRIBUTES
		{
			public int nLength;
			public IntPtr lpSecurityDescriptor;
			public int bInheritHandle;
		}

		[StructLayout(LayoutKind.Sequential)]
		class PROCESS_INFORMATION
		{
			public IntPtr hProcess;
			public IntPtr hThread;
			public uint dwProcessId;
			public uint dwThreadId;
		}

		[StructLayout(LayoutKind.Sequential)]
		class STARTUPINFO
		{
			public int cb;
			public string lpReserved;
			public string lpDesktop;
			public string lpTitle;
			public uint dwX;
			public uint dwY;
			public uint dwXSize;
			public uint dwYSize;
			public uint dwXCountChars;
			public uint dwYCountChars;
			public uint dwFillAttribute;
			public uint dwFlags;
			public short wShowWindow;
			public short cbReserved2;
			public IntPtr lpReserved2;
			public SafeHandle hStdInput;
			public SafeHandle hStdOutput;
			public SafeHandle hStdError;
		}

		[StructLayout(LayoutKind.Sequential)]
		struct JOBOBJECT_BASIC_LIMIT_INFORMATION
		{
			public Int64 PerProcessUserTimeLimit;
			public Int64 PerJobUserTimeLimit;
			public UInt32 LimitFlags;
			public UIntPtr MinimumWorkingSetSize;
			public UIntPtr MaximumWorkingSetSize;
			public UInt32 ActiveProcessLimit;
			public Int64 Affinity;
			public UInt32 PriorityClass;
			public UInt32 SchedulingClass;
		}

		[StructLayout(LayoutKind.Sequential)]
		struct IO_COUNTERS
		{
			public UInt64 ReadOperationCount;
			public UInt64 WriteOperationCount;
			public UInt64 OtherOperationCount;
			public UInt64 ReadTransferCount;
			public UInt64 WriteTransferCount;
			public UInt64 OtherTransferCount;
		}

		[StructLayout(LayoutKind.Sequential)]
		struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
		{
			public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
			public IO_COUNTERS IoInfo;
			public UIntPtr ProcessMemoryLimit;
			public UIntPtr JobMemoryLimit;
			public UIntPtr PeakProcessMemoryUsed;
			public UIntPtr PeakJobMemoryUsed;
		}

		[DllImport("kernel32.dll", SetLastError=true)]
		static extern SafeFileHandle CreateJobObject(IntPtr SecurityAttributes, IntPtr Name);

		const int JobObjectExtendedLimitInformation = 9;

        [DllImport("kernel32.dll", SetLastError=true)]
        static extern int SetInformationJobObject(SafeFileHandle hJob, int JobObjectInfoClass, IntPtr lpJobObjectInfo, int cbJobObjectInfoLength);

		[DllImport("kernel32.dll", SetLastError=true)]
		static extern int AssignProcessToJobObject(SafeFileHandle hJob, IntPtr hProcess);

		[DllImport("kernel32.dll", SetLastError=true)]
		static extern int CloseHandle(IntPtr hObject);

		[DllImport("kernel32.dll", SetLastError = true)]
		static extern int CreatePipe(out SafeFileHandle hReadPipe, out SafeFileHandle hWritePipe, SECURITY_ATTRIBUTES lpPipeAttributes, uint nSize);

		[DllImport("kernel32.dll", SetLastError = true)]
		static extern int SetHandleInformation(SafeFileHandle hObject, int dwMask, int dwFlags);

		const int HANDLE_FLAG_INHERIT = 1;

        const int STARTF_USESTDHANDLES = 0x00000100;

        const int CREATE_NO_WINDOW = 0x08000000;
        const int CREATE_SUSPENDED = 0x00000004;
		const int CREATE_BREAKAWAY_FROM_JOB = 0x01000000;

        [DllImport("kernel32.dll", SetLastError=true)]
        static extern int CreateProcess(/*[MarshalAs(UnmanagedType.LPTStr)]*/ string lpApplicationName, StringBuilder lpCommandLine, IntPtr lpProcessAttributes, IntPtr lpThreadAttributes, bool bInheritHandles, int dwCreationFlags, IntPtr lpEnvironment, /*[MarshalAs(UnmanagedType.LPTStr)]*/ string lpCurrentDirectory, STARTUPINFO lpStartupInfo, PROCESS_INFORMATION lpProcessInformation);

		[DllImport("kernel32.dll", SetLastError=true)]
		static extern int ResumeThread(IntPtr hThread);

		[DllImport("kernel32.dll", SetLastError=true)]
		static extern IntPtr GetCurrentProcess();

        const int DUPLICATE_SAME_ACCESS = 2;

        [DllImport("kernel32.dll", SetLastError=true)]    
        static extern int DuplicateHandle(
            IntPtr hSourceProcessHandle,
            SafeHandle hSourceHandle,
            IntPtr hTargetProcess,
            out SafeWaitHandle targetHandle,
            int dwDesiredAccess,
            bool bInheritHandle,
            int dwOptions
        );

		[DllImport("kernel32.dll", SetLastError = true)]
		static extern int GetExitCodeProcess(SafeFileHandle hProcess, out int lpExitCode);

		SafeFileHandle JobHandle;
		SafeFileHandle ProcessHandle;
		SafeFileHandle StdInWrite;
		SafeFileHandle StdOutRead;

		FileStream InnerStream;
		StreamReader ReadStream;

		public ChildProcess(string FileName, string CommandLine, string Input)
		{
			JobHandle = CreateJobObject(IntPtr.Zero, IntPtr.Zero);
			if(JobHandle == null)
			{
				throw new Win32Exception();
			}

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION LimitInformation = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
			LimitInformation.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

			int Length = Marshal.SizeOf(typeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
			IntPtr LimitInformationPtr = Marshal.AllocHGlobal(Length);
			Marshal.StructureToPtr(LimitInformation, LimitInformationPtr, false);

			if(SetInformationJobObject(JobHandle, JobObjectExtendedLimitInformation, LimitInformationPtr, Length) == 0)
			{
				throw new Win32Exception();
			}

			SafeFileHandle StdInRead = null;
			SafeFileHandle StdOutWrite = null;
			SafeWaitHandle StdErrWrite = null;
			try
			{
				SECURITY_ATTRIBUTES SecurityAttributes = new SECURITY_ATTRIBUTES();
				SecurityAttributes.bInheritHandle = 1;

				if(CreatePipe(out StdInRead, out StdInWrite, SecurityAttributes, 0) == 0 || SetHandleInformation(StdInWrite, HANDLE_FLAG_INHERIT, 0) == 0)
				{
					throw new Win32Exception();
				}
				if(CreatePipe(out StdOutRead, out StdOutWrite, SecurityAttributes, 0) == 0 || SetHandleInformation(StdOutRead, HANDLE_FLAG_INHERIT, 0) == 0)
				{
					throw new Win32Exception();
				}
				if(DuplicateHandle(GetCurrentProcess(), StdOutWrite, GetCurrentProcess(), out StdErrWrite, DUPLICATE_SAME_ACCESS, true, 0) == 0)
				{
					throw new Win32Exception();
				}

				STARTUPINFO StartupInfo = new STARTUPINFO();
                StartupInfo.cb = Marshal.SizeOf(StartupInfo);
				StartupInfo.hStdInput = StdInRead;
				StartupInfo.hStdOutput = StdOutWrite;
				StartupInfo.hStdError = StdErrWrite;
				StartupInfo.dwFlags = STARTF_USESTDHANDLES;

				PROCESS_INFORMATION ProcessInfo = new PROCESS_INFORMATION();
				try
				{
					if(CreateProcess(null, new StringBuilder("\"" + FileName + "\" " + CommandLine), IntPtr.Zero, IntPtr.Zero, true, CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB, new IntPtr(0), null, StartupInfo, ProcessInfo) == 0)
					{
						throw new Win32Exception();
					}
					if(AssignProcessToJobObject(JobHandle, ProcessInfo.hProcess) == 0)
					{
						throw new Win32Exception();
					}
					if(ResumeThread(ProcessInfo.hThread) == -1)
					{
						throw new Win32Exception();
					}

					using(StreamWriter StdInWriter = new StreamWriter(new FileStream(StdInWrite, FileAccess.Write, 4096, false), Console.InputEncoding, 4096))
					{
						if(!String.IsNullOrEmpty(Input))
						{
							StdInWriter.WriteLine(Input);
							StdInWriter.Flush();
						}
					}

					InnerStream = new FileStream(StdOutRead, FileAccess.Read, 4096, false);
					ReadStream = new StreamReader(InnerStream, Console.OutputEncoding);
					ProcessHandle = new SafeFileHandle(ProcessInfo.hProcess, true);
				}
				finally
				{
					if(ProcessInfo.hProcess != null && ProcessHandle == null)
					{
						CloseHandle(ProcessInfo.hProcess);
					}
					if(ProcessInfo.hThread != null)
					{
						CloseHandle(ProcessInfo.hThread);
					}
				}
			}
			finally
			{
				if(StdInRead != null)
				{
					StdInRead.Dispose();
					StdInRead = null;
				}
				if(StdOutWrite != null)
				{
					StdOutWrite.Dispose();
					StdOutWrite = null;
				}
				if(StdErrWrite != null)
				{
					StdErrWrite.Dispose();
					StdErrWrite = null;
				}
			}
		}

		public void Dispose()
		{
			if(JobHandle != null)
			{
				JobHandle.Dispose();
				JobHandle = null;
			}
			if(ProcessHandle != null)
			{
				ProcessHandle.Dispose();
				ProcessHandle = null;
			}
			if(StdInWrite != null)
			{
				StdInWrite.Dispose();
				StdInWrite = null;
			}
			if(StdOutRead != null)
			{
				StdOutRead.Dispose();
				StdOutRead = null;
			}
		}

		public List<string> ReadAllLines()
		{
			// Manually read all the output lines from the stream. Using ReadToEndAsync() or ReadAsync() on the StreamReader is abysmally slow, especially
			// for high-volume processes. Manually picking out the lines via a buffered ReadAsync() call was found to be 6x faster on 'p4 -ztag have' calls.
			List<string> OutputLines = new List<string>();
			byte[] Buffer = new byte[32 * 1024];
			byte LastCharacter = 0;
			int NumBytesInBuffer = 0;
			for(;;)
			{
				// Fill the buffer, reentering managed code every 20ms to allow thread abort exceptions to be thrown
				Task<int> ReadTask = InnerStream.ReadAsync(Buffer, NumBytesInBuffer, Buffer.Length - NumBytesInBuffer);
				while(!ReadTask.Wait(20))
				{
				}

				// Update the buffer size
				if(ReadTask.Result == 0)
				{
					break;
				}

				// Otherwise append to the existing buffer
				NumBytesInBuffer += ReadTask.Result;

				// Pull out all the complete output lines
				int LastStartIdx = 0;
				for(int Idx = 0; Idx < NumBytesInBuffer; Idx++)
				{
					if(Buffer[Idx] == '\r' || Buffer[Idx] == '\n')
					{
						if(Buffer[Idx] != '\n' || LastCharacter != '\r')
						{
							OutputLines.Add(Encoding.UTF8.GetString(Buffer, LastStartIdx, Idx - LastStartIdx));
						}
						LastStartIdx = Idx + 1;
					}
					LastCharacter = Buffer[Idx];
				}

				// Shuffle everything back to the start of the buffer
				Array.Copy(Buffer, LastStartIdx, Buffer, 0, Buffer.Length - LastStartIdx);
				NumBytesInBuffer -= LastStartIdx;
			}
			return OutputLines;
		}

		public bool TryReadLine(out string Line)
		{
			// Busy wait for the ReadLine call to finish, so we can get interrupted by thread abort exceptions
			Task<string> ReadLineTask = ReadStream.ReadLineAsync();
			for(;;)
			{
				if(ReadLineTask.Wait(20))
				{
					Line = ReadLineTask.IsCompleted? ReadLineTask.Result : null;
					return Line != null;
				}
			}
		}

		public int ExitCode
		{
			get
			{
				int Value;
				if(GetExitCodeProcess(ProcessHandle, out Value) == 0)
				{
					throw new Win32Exception();
				}
				return Value;
			}
		}
	}
}
