/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;


namespace MobileDeviceInterface
{
    /// <summary>
    /// This is a simple generic to provide a limited form of type safety for IntPtr arguments to
    /// structs of unknown contents (so we can't just marshal them as unique types)
    /// </summary>
    public struct TypedPtr<T>
    {
        public IntPtr Handle;

        public TypedPtr(IntPtr Ptr)
        {
            Handle = Ptr;
        }

        public static implicit operator TypedPtr<T>(IntPtr Ptr)
        {
            return new TypedPtr<T>(Ptr);
        }

        public static explicit operator IntPtr(TypedPtr<T> Ptr)
        {
            return Ptr.Handle;
        }
    }
}
