/*
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef GFSDK_VXGI_MATHTYPES_H_
#define GFSDK_VXGI_MATHTYPES_H_

#include <limits>
#include <stdint.h>
#include <string.h>
#include <algorithm>

//8 is the default msvc packing
#ifdef _MSC_VER
#define GI_BEGIN_PACKING __pragma(pack(push, 8))
#define GI_END_PACKING __pragma(pack(pop))
#define GI_BEGIN_TIGHT_PACKING __pragma(pack(push, 1))
#define GI_END_TIGHT_PACKING __pragma(pack(pop))
#else //_MSC_VER
#define GI_BEGIN_PACKING _Pragma("pack(push, 8)")
#define GI_END_PACKING _Pragma("pack(pop)")
#define GI_BEGIN_TIGHT_PACKING _Pragma("pack(push, 1)")
#define GI_END_TIGHT_PACKING _Pragma("pack(pop)")
#endif

//These are stored in CBs so we want the most predictable layout
GI_BEGIN_TIGHT_PACKING

namespace VXGI
{
    // Suppress warnings "nonstandard extension used : nameless struct/union"
#ifdef _MSC_VER
#pragma warning(disable : 4201)
#endif //_MSC_VER

    template <typename T>
    struct Vector2;

    template <typename T>
    struct Vector3;

    template <typename T>
    struct Vector4;


    template <typename T>
    struct Vector2
    {
        union
        {
            struct
            {
                T x, y;
            };
            T vector[2];
        };

        Vector2() : x(0), y(0) { }
        Vector2(const Vector2 &v) : x(v.x), y(v.y) { }
        Vector2(const Vector3<T> &v) : x(v.x), y(v.y) { }
        Vector2(const Vector4<T> &v) : x(v.x), y(v.y) { }
        Vector2(T _scalar) : x(_scalar), y(_scalar) { }
        Vector2(T _x, T _y) : x(_x), y(_y) { }
        Vector2(const T *v) : x(v[0]), y(v[1]) { }
        T &operator [] (int i) { return vector[i]; }
        const T &operator [] (int i) const { return vector[i]; }

        // Useful functions
        T vmin() const { return std::min(x, y); }
        T vmax() const { return std::max(x, y); }
        Vector2 vfloor() const { return Vector2(floor(x), floor(y)); }
        Vector2 vceil() const { return Vector2(ceil(x), ceil(y)); }
        T lengthSq() const { return x * x + y * y; }
        T area() const { return x * y; }

        Vector2 normalize() const
        {
            float length = sqrt(lengthSq());
            if (length == 0)
                return *this;
            else
                return *this / length;
        }

        // Element-wise vector ops
        Vector2 operator +(const Vector2& b) const { return Vector2(x + b.x, y + b.y); }
        Vector2 operator -(const Vector2& b) const { return Vector2(x - b.x, y - b.y); }
        Vector2 operator *(const Vector2& b) const { return Vector2(x * b.x, y * b.y); }
        Vector2 operator /(const Vector2& b) const { return Vector2(x / b.x, y / b.y); }
        Vector2 operator >>(const Vector2& b) const { return Vector2(x >> b.x, y >> b.y); }
        Vector2 operator <<(const Vector2& b) const { return Vector2(x << b.x, y << b.y); }

        void operator +=(const Vector2& b) { x += b.x; y += b.y; }
        void operator -=(const Vector2& b) { x -= b.x; y -= b.y; }
        void operator *=(const Vector2& b) { x *= b.x; y *= b.y; }
        void operator /=(const Vector2& b) { x /= b.x; y /= b.y; }
        void operator >>=(const Vector2& b) { x >>= b.x; y >>= b.y; }
        void operator <<=(const Vector2& b) { x <<= b.x; y <<= b.y; }

        // Vector-scalar ops
        Vector2 operator +(T b) const { return Vector2(x + b, y + b); }
        Vector2 operator -(T b) const { return Vector2(x - b, y - b); }
        Vector2 operator *(T b) const { return Vector2(x * b, y * b); }
        Vector2 operator /(T b) const { return Vector2(x / b, y / b); }
        Vector2 operator >>(T b) const { return Vector2(x >> b, y >> b); }
        Vector2 operator <<(T b) const { return Vector2(x << b, y << b); }

        bool operator > (const Vector2& b) const { return x > b.x && y > b.y; }
        bool operator < (const Vector2& b) const { return x < b.x && y < b.y; }
        bool operator >=(const Vector2& b) const { return x >= b.x && y >= b.y; }
        bool operator <=(const Vector2& b) const { return x <= b.x && y <= b.y; }

        bool operator ==(const Vector2& b) const { return x == b.x && y == b.y; }
        bool operator !=(const Vector2& b) const { return x != b.x || y != b.y; }

        Vector2& operator = (const Vector2<T>& b) { x = b.x; y = b.y; return *this; }
        Vector2& operator = (const Vector3<T>& b) { x = b.x; y = b.y; return *this; }
        Vector2& operator = (const Vector4<T>& b) { x = b.x; y = b.y; return *this; }
    };

    typedef Vector2<int32_t>    Vector2i;
    typedef Vector2<uint32_t>   Vector2u;
    typedef Vector2<float>  Vector2f;

    template <typename T> T DotProduct(const Vector2<T> &a, const Vector2<T> &b)
    {
        return a.x * b.x + a.y * b.y;
    }

    template <typename T> T CrossProduct(const Vector2<T> &a, const Vector2<T> &b)
    {
        return a.x * b.y - a.y * b.x;
    }

    template <typename T, typename U> Vector2<T> CastVector2(const Vector2<U> &a)
    {
        return Vector2<T>((T)a.x, (T)a.y);
    }


    template <typename T>
    struct Vector3
    {
        union
        {
            struct
            {
                T x, y, z;
            };
            T vector[3];
        };

        Vector3() : x(0), y(0), z(0) { }
        Vector3(const Vector3 &v) : x(v.x), y(v.y), z(v.z) { }
        Vector3(const Vector4<T> &v) : x(v.x), y(v.y), z(v.z) { }
        Vector3(T _scalar) : x(_scalar), y(_scalar), z(_scalar) { }
        Vector3(T _x, T _y, T _z) : x(_x), y(_y), z(_z) { }
        Vector3(const T *v) : x(v[0]), y(v[1]), z(v[2]) { }
        T &operator [] (int i) { return vector[i]; }
        const T &operator [] (int i) const { return vector[i]; }

        // Useful functions
        T vmin() const { return std::min(x, std::min(y, z)); }
        T vmax() const { return std::max(x, std::max(y, z)); }
        Vector3 vfloor() const { return Vector3(floor(x), floor(y), floor(z)); }
        Vector3 vceil() const { return Vector3(ceil(x), ceil(y), ceil(z)); }
        T lengthSq() const { return x * x + y * y + z * z; }
        T volume() const { return x * y * z; }

        Vector3 normalize() const
        {
            float length = sqrt(lengthSq());
            if (length == 0)
                return *this;
            else
                return *this / length;
        }

        // Element-wise vector ops
        Vector3 operator +(const Vector3& b) const { return Vector3(x + b.x, y + b.y, z + b.z); }
        Vector3 operator -(const Vector3& b) const { return Vector3(x - b.x, y - b.y, z - b.z); }
        Vector3 operator *(const Vector3& b) const { return Vector3(x * b.x, y * b.y, z * b.z); }
        Vector3 operator /(const Vector3& b) const { return Vector3(x / b.x, y / b.y, z / b.z); }
        Vector3 operator >>(const Vector3& b) const { return Vector3(x >> b.x, y >> b.y, z >> b.z); }
        Vector3 operator <<(const Vector3& b) const { return Vector3(x << b.x, y << b.y, z << b.z); }

        void operator +=(const Vector3& b) { x += b.x; y += b.y; z += b.z; }
        void operator -=(const Vector3& b) { x -= b.x; y -= b.y; z -= b.z; }
        void operator *=(const Vector3& b) { x *= b.x; y *= b.y; z *= b.z; }
        void operator /=(const Vector3& b) { x /= b.x; y /= b.y; z /= b.z; }
        void operator >>=(const Vector3& b) { x >>= b.x; y >>= b.y; z >>= b.z; }
        void operator <<=(const Vector3& b) { x <<= b.x; y <<= b.y; z <<= b.z; }

        // Vector-scalar ops
        Vector3 operator +(T b) const { return Vector3(x + b, y + b, z + b); }
        Vector3 operator -(T b) const { return Vector3(x - b, y - b, z - b); }
        Vector3 operator *(T b) const { return Vector3(x * b, y * b, z * b); }
        Vector3 operator /(T b) const { return Vector3(x / b, y / b, z / b); }
        Vector3 operator >>(T b) const { return Vector3(x >> b, y >> b, z >> b); }
        Vector3 operator <<(T b) const { return Vector3(x << b, y << b, z << b); }

        bool operator > (const Vector3& b) const { return x > b.x && y > b.y && z > b.z; }
        bool operator < (const Vector3& b) const { return x < b.x && y < b.y && z < b.z; }
        bool operator >=(const Vector3& b) const { return x >= b.x && y >= b.y && z >= b.z; }
        bool operator <=(const Vector3& b) const { return x <= b.x && y <= b.y && z <= b.z; }

        bool operator ==(const Vector3& b) const { return x == b.x && y == b.y && z == b.z; }
        bool operator !=(const Vector3& b) const { return x != b.x || y != b.y || z != b.z; }

        Vector3& operator = (const Vector4<T>& b) { x = b.x; y = b.y; z = b.z; return *this; }
    };

    typedef Vector3<int32_t>    Vector3i;
    typedef Vector3<uint32_t>   Vector3u;
    typedef Vector3<float>  Vector3f;

    template <typename T> T DotProduct(const Vector3<T> &a, const Vector3<T> &b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    template <typename T> Vector3<T> CrossProduct(const Vector3<T> &a, const Vector3<T> &b)
    {
        return Vector3<T>(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }

    template <typename T, typename U> Vector3<T> CastVector3(const Vector3<U> &a)
    {
        return Vector3<T>((T)a.x, (T)a.y, (T)a.z);
    }

    template <typename T>
    struct Vector4
    {
        union
        {
            struct
            {
                T x, y, z, w;
            };
            T vector[4];
        };

        Vector4() : x(0), y(0), z(0), w(0) { }
        Vector4(const Vector4 &v) : x(v.x), y(v.y), z(v.z), w(v.w) { }
        Vector4(T _scalar) : x(_scalar), y(_scalar), z(_scalar), w(_scalar) { }
        Vector4(const Vector3<T> &v, T _w) : x(v.x), y(v.y), z(v.z), w(_w) { }
        Vector4(T _x, T _y, T _z, T _w) : x(_x), y(_y), z(_z), w(_w) { }
        Vector4(const T *v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) { }
        T &operator [] (int i) { return vector[i]; }
        const T &operator [] (int i) const { return vector[i]; }

        Vector4& operator = (const Vector3<T>& _V) { x = _V.x; y = _V.y; z = _V.z; w = 0; return *this; }
        Vector4 operator *(T b) const { return Vector4(x * b, y * b, z * b, w * b); }

        bool operator ==(const Vector4& b) const { return x == b.x && y == b.y && z == b.z && w == b.w; }
        bool operator !=(const Vector4& b) const { return x != b.x || y != b.y || z != b.z || w != b.w; }
    };

    typedef Vector4<int32_t>    Vector4i;
    typedef Vector4<uint32_t>   Vector4u;
    typedef Vector4<float>  Vector4f;

    template <typename T>
    struct Matrix4
    {
        union
        {
            struct
            {
                Vector4<T> row_0, row_1, row_2, row_3;
            };
            T m[16];
            struct
            {
                T rows[4][4];
            };
        };

        Matrix4() : row_0(T(1), T(0), T(0), T(0)), row_1(T(0), T(1), T(0), T(0)), row_2(T(0), T(0), T(1), T(0)), row_3(T(0), T(0), T(0), T(1)) { }
        Matrix4(const Matrix4 &_m) : row_0(_m.row_0), row_1(_m.row_1), row_2(_m.row_2), row_3(_m.row_3) { }
        Matrix4(const T* src_m) {
            memcpy(m, src_m, 16 * sizeof(m[0]));
        }
        Matrix4(T a00, T a10, T a20, T a30,
            T a01, T a11, T a21, T a31,
            T a02, T a12, T a22, T a32,
            T a03, T a13, T a23, T a33){
            rows[0][0] = a00;  rows[1][0] = a10;  rows[2][0] = a20;  rows[3][0] = a30;
            rows[0][1] = a01;  rows[1][1] = a11;  rows[2][1] = a21;  rows[3][1] = a31;
            rows[0][2] = a02;  rows[1][2] = a12;  rows[2][2] = a22;  rows[3][2] = a32;
            rows[0][3] = a03;  rows[1][3] = a13;  rows[2][3] = a23;  rows[3][3] = a33;
        }
        T* operator [] (int i) { return rows[i]; }
        const T* operator [] (int i) const { return rows[i]; }

        bool operator ==(const Matrix4& b) const {
            for (int i = 0; i < 16; i++)
                if (m[i] != b.m[i])
                    return false;
            return true;
        }

        bool operator !=(const Matrix4& b) const {
            return !(*this == b);
        }

        Matrix4<T> invert() const {
            T f = (T(1.0) /
                (rows[0][0] * rows[1][1] * rows[2][2] * rows[3][3] +
                rows[0][0] * rows[1][2] * rows[2][3] * rows[3][1] +
                rows[0][0] * rows[1][3] * rows[2][1] * rows[3][2] +
                rows[0][1] * rows[1][0] * rows[2][3] * rows[3][2] +
                rows[0][1] * rows[1][2] * rows[2][0] * rows[3][3] +
                rows[0][1] * rows[1][3] * rows[2][2] * rows[3][0] +
                rows[0][2] * rows[1][0] * rows[2][1] * rows[3][3] +
                rows[0][2] * rows[1][1] * rows[2][3] * rows[3][0] +
                rows[0][2] * rows[1][3] * rows[2][0] * rows[3][1] +
                rows[0][3] * rows[1][0] * rows[2][2] * rows[3][1] +
                rows[0][3] * rows[1][1] * rows[2][0] * rows[3][2] +
                rows[0][3] * rows[1][2] * rows[2][1] * rows[3][0] +
                -rows[0][0] * rows[1][1] * rows[2][3] * rows[3][2] +
                -rows[0][0] * rows[1][2] * rows[2][1] * rows[3][3] +
                -rows[0][0] * rows[1][3] * rows[2][2] * rows[3][1] +
                -rows[0][1] * rows[1][0] * rows[2][2] * rows[3][3] +
                -rows[0][1] * rows[1][2] * rows[2][3] * rows[3][0] +
                -rows[0][1] * rows[1][3] * rows[2][0] * rows[3][2] +
                -rows[0][2] * rows[1][0] * rows[2][3] * rows[3][1] +
                -rows[0][2] * rows[1][1] * rows[2][0] * rows[3][3] +
                -rows[0][2] * rows[1][3] * rows[2][1] * rows[3][0] +
                -rows[0][3] * rows[1][0] * rows[2][1] * rows[3][2] +
                -rows[0][3] * rows[1][1] * rows[2][2] * rows[3][0] +
                -rows[0][3] * rows[1][2] * rows[2][0] * rows[3][1]));

            T a00 = (rows[1][1] * rows[2][2] * rows[3][3] +
                rows[1][2] * rows[2][3] * rows[3][1] +
                rows[1][3] * rows[2][1] * rows[3][2] +
                -rows[1][1] * rows[2][3] * rows[3][2] +
                -rows[1][2] * rows[2][1] * rows[3][3] +
                -rows[1][3] * rows[2][2] * rows[3][1]);

            T a10 = (rows[0][1] * rows[2][3] * rows[3][2] +
                rows[0][2] * rows[2][1] * rows[3][3] +
                rows[0][3] * rows[2][2] * rows[3][1] +
                -rows[0][1] * rows[2][2] * rows[3][3] +
                -rows[0][2] * rows[2][3] * rows[3][1] +
                -rows[0][3] * rows[2][1] * rows[3][2]);

            T a20 = (rows[0][1] * rows[1][2] * rows[3][3] +
                rows[0][2] * rows[1][3] * rows[3][1] +
                rows[0][3] * rows[1][1] * rows[3][2] +
                -rows[0][1] * rows[1][3] * rows[3][2] +
                -rows[0][2] * rows[1][1] * rows[3][3] +
                -rows[0][3] * rows[1][2] * rows[3][1]);

            T a30 = (rows[0][1] * rows[1][3] * rows[2][2] +
                rows[0][2] * rows[1][1] * rows[2][3] +
                rows[0][3] * rows[1][2] * rows[2][1] +
                -rows[0][1] * rows[1][2] * rows[2][3] +
                -rows[0][2] * rows[1][3] * rows[2][1] +
                -rows[0][3] * rows[1][1] * rows[2][2]);

            T a01 = (rows[1][0] * rows[2][3] * rows[3][2] +
                rows[1][2] * rows[2][0] * rows[3][3] +
                rows[1][3] * rows[2][2] * rows[3][0] +
                -rows[1][0] * rows[2][2] * rows[3][3] +
                -rows[1][2] * rows[2][3] * rows[3][0] +
                -rows[1][3] * rows[2][0] * rows[3][2]);

            T a11 = (rows[0][0] * rows[2][2] * rows[3][3] +
                rows[0][2] * rows[2][3] * rows[3][0] +
                rows[0][3] * rows[2][0] * rows[3][2] +
                -rows[0][0] * rows[2][3] * rows[3][2] +
                -rows[0][2] * rows[2][0] * rows[3][3] +
                -rows[0][3] * rows[2][2] * rows[3][0]);

            T a21 = (rows[0][0] * rows[1][3] * rows[3][2] +
                rows[0][2] * rows[1][0] * rows[3][3] +
                rows[0][3] * rows[1][2] * rows[3][0] +
                -rows[0][0] * rows[1][2] * rows[3][3] +
                -rows[0][2] * rows[1][3] * rows[3][0] +
                -rows[0][3] * rows[1][0] * rows[3][2]);

            T a31 = (rows[0][0] * rows[1][2] * rows[2][3] +
                rows[0][2] * rows[1][3] * rows[2][0] +
                rows[0][3] * rows[1][0] * rows[2][2] +
                -rows[0][0] * rows[1][3] * rows[2][2] +
                -rows[0][2] * rows[1][0] * rows[2][3] +
                -rows[0][3] * rows[1][2] * rows[2][0]);

            T a02 = (rows[1][0] * rows[2][1] * rows[3][3] +
                rows[1][1] * rows[2][3] * rows[3][0] +
                rows[1][3] * rows[2][0] * rows[3][1] +
                -rows[1][0] * rows[2][3] * rows[3][1] +
                -rows[1][1] * rows[2][0] * rows[3][3] +
                -rows[1][3] * rows[2][1] * rows[3][0]);

            T a12 = (-rows[0][0] * rows[2][1] * rows[3][3] +
                -rows[0][1] * rows[2][3] * rows[3][0] +
                -rows[0][3] * rows[2][0] * rows[3][1] +
                rows[0][0] * rows[2][3] * rows[3][1] +
                rows[0][1] * rows[2][0] * rows[3][3] +
                rows[0][3] * rows[2][1] * rows[3][0]);

            T a22 = (rows[0][0] * rows[1][1] * rows[3][3] +
                rows[0][1] * rows[1][3] * rows[3][0] +
                rows[0][3] * rows[1][0] * rows[3][1] +
                -rows[0][0] * rows[1][3] * rows[3][1] +
                -rows[0][1] * rows[1][0] * rows[3][3] +
                -rows[0][3] * rows[1][1] * rows[3][0]);

            T a32 = (rows[0][0] * rows[1][3] * rows[2][1] +
                rows[0][1] * rows[1][0] * rows[2][3] +
                rows[0][3] * rows[1][1] * rows[2][0] +
                -rows[0][1] * rows[1][3] * rows[2][0] +
                -rows[0][3] * rows[1][0] * rows[2][1] +
                -rows[0][0] * rows[1][1] * rows[2][3]);

            T a03 = (rows[1][0] * rows[2][2] * rows[3][1] +
                rows[1][1] * rows[2][0] * rows[3][2] +
                rows[1][2] * rows[2][1] * rows[3][0] +
                -rows[1][0] * rows[2][1] * rows[3][2] +
                -rows[1][1] * rows[2][2] * rows[3][0] +
                -rows[1][2] * rows[2][0] * rows[3][1]);

            T a13 = (rows[0][0] * rows[2][1] * rows[3][2] +
                rows[0][1] * rows[2][2] * rows[3][0] +
                rows[0][2] * rows[2][0] * rows[3][1] +
                -rows[0][0] * rows[2][2] * rows[3][1] +
                -rows[0][1] * rows[2][0] * rows[3][2] +
                -rows[0][2] * rows[2][1] * rows[3][0]);

            T a23 = (rows[0][0] * rows[1][2] * rows[3][1] +
                rows[0][1] * rows[1][0] * rows[3][2] +
                rows[0][2] * rows[1][1] * rows[3][0] +
                -rows[0][0] * rows[1][1] * rows[3][2] +
                -rows[0][1] * rows[1][2] * rows[3][0] +
                -rows[0][2] * rows[1][0] * rows[3][1]);

            T a33 = (rows[0][0] * rows[1][1] * rows[2][2] +
                rows[0][1] * rows[1][2] * rows[2][0] +
                rows[0][2] * rows[1][0] * rows[2][1] +
                -rows[0][0] * rows[1][2] * rows[2][1] +
                -rows[0][1] * rows[1][0] * rows[2][2] +
                -rows[0][2] * rows[1][1] * rows[2][0]);

            return (Matrix4<T>(a00*f, a01*f, a02*f, a03*f,
                a10*f, a11*f, a12*f, a13*f,
                a20*f, a21*f, a22*f, a23*f,
                a30*f, a31*f, a32*f, a33*f));

        }

        Matrix4<T> transpose() const {
            return Matrix4<T>(rows[0][0], rows[0][1], rows[0][2], rows[0][3],
                rows[1][0], rows[1][1], rows[1][2], rows[1][3],
                rows[2][0], rows[2][1], rows[2][2], rows[2][3],
                rows[3][0], rows[3][1], rows[3][2], rows[3][3]);
        }

        Vector4<T> vecTransform(const Vector4<T> &v) const {
            return Vector4<T>(
                rows[0][0] * v[0] + rows[1][0] * v[1] + rows[2][0] * v[2] + rows[3][0] * v[3],
                rows[0][1] * v[0] + rows[1][1] * v[1] + rows[2][1] * v[2] + rows[3][1] * v[3],
                rows[0][2] * v[0] + rows[1][2] * v[1] + rows[2][2] * v[2] + rows[3][2] * v[3],
                rows[0][3] * v[0] + rows[1][3] * v[1] + rows[2][3] * v[2] + rows[3][3] * v[3]);
        }

        Vector3<T> vecTransform(const Vector3<T> &v) const {
            T w = rows[0][3] * v[0] + rows[1][3] * v[1] + rows[2][3] * v[2] + rows[3][3];
            T invW = T(1) / w;
            return Vector3<T>(
                (rows[0][0] * v[0] + rows[1][0] * v[1] + rows[2][0] * v[2]) * invW,
                (rows[0][1] * v[0] + rows[1][1] * v[1] + rows[2][1] * v[2]) * invW,
                (rows[0][2] * v[0] + rows[1][2] * v[1] + rows[2][2] * v[2]) * invW
                );
        }

        Vector3<T> pntTransform(const Vector3<T> &v) const {
            T w = rows[0][3] * v[0] + rows[1][3] * v[1] + rows[2][3] * v[2] + rows[3][3];
            T invW = T(1) / w;
            return Vector3<T>(
                (rows[0][0] * v[0] + rows[1][0] * v[1] + rows[2][0] * v[2] + rows[3][0]) * invW,
                (rows[0][1] * v[0] + rows[1][1] * v[1] + rows[2][1] * v[2] + rows[3][1]) * invW,
                (rows[0][2] * v[0] + rows[1][2] * v[1] + rows[2][2] * v[2] + rows[3][2]) * invW
                );
        }
    };

    template <typename T>
    Matrix4<T> operator*(Matrix4<T> const &a, Matrix4<T> const &b) {
        return Matrix4<T>((a[0][0] * b[0][0] + a[0][1] * b[1][0] +
            a[0][2] * b[2][0] + a[0][3] * b[3][0]),
            (a[1][0] * b[0][0] + a[1][1] * b[1][0] +
            a[1][2] * b[2][0] + a[1][3] * b[3][0]),
            (a[2][0] * b[0][0] + a[2][1] * b[1][0] +
            a[2][2] * b[2][0] + a[2][3] * b[3][0]),
            (a[3][0] * b[0][0] + a[3][1] * b[1][0] +
            a[3][2] * b[2][0] + a[3][3] * b[3][0]),

            (a[0][0] * b[0][1] + a[0][1] * b[1][1] +
            a[0][2] * b[2][1] + a[0][3] * b[3][1]),
            (a[1][0] * b[0][1] + a[1][1] * b[1][1] +
            a[1][2] * b[2][1] + a[1][3] * b[3][1]),
            (a[2][0] * b[0][1] + a[2][1] * b[1][1] +
            a[2][2] * b[2][1] + a[2][3] * b[3][1]),
            (a[3][0] * b[0][1] + a[3][1] * b[1][1] +
            a[3][2] * b[2][1] + a[3][3] * b[3][1]),

            (a[0][0] * b[0][2] + a[0][1] * b[1][2] +
            a[0][2] * b[2][2] + a[0][3] * b[3][2]),
            (a[1][0] * b[0][2] + a[1][1] * b[1][2] +
            a[1][2] * b[2][2] + a[1][3] * b[3][2]),
            (a[2][0] * b[0][2] + a[2][1] * b[1][2] +
            a[2][2] * b[2][2] + a[2][3] * b[3][2]),
            (a[3][0] * b[0][2] + a[3][1] * b[1][2] +
            a[3][2] * b[2][2] + a[3][3] * b[3][2]),

            (a[0][0] * b[0][3] + a[0][1] * b[1][3] +
            a[0][2] * b[2][3] + a[0][3] * b[3][3]),
            (a[1][0] * b[0][3] + a[1][1] * b[1][3] +
            a[1][2] * b[2][3] + a[1][3] * b[3][3]),
            (a[2][0] * b[0][3] + a[2][1] * b[1][3] +
            a[2][2] * b[2][3] + a[2][3] * b[3][3]),
            (a[3][0] * b[0][3] + a[3][1] * b[1][3] +
            a[3][2] * b[2][3] + a[3][3] * b[3][3]));
    }

    typedef Matrix4<float> Matrix4f;


    template <typename T>
    struct Box2
    {
        Vector2<T> lower, upper;

        Box2() : lower(), upper() { }
        Box2(const Box2 &a) : lower(a.lower), upper(a.upper) { }
        Box2(Vector2<T> _lower, Vector2<T> _upper) : lower(_lower), upper(_upper) { }

        Vector2<T> size() const { return Vector2<T>(upper.x - lower.x, upper.y - lower.y); }
        bool intersectsWith(const Box2 &other) const { return other.lower <= upper && other.upper >= lower; }
        bool contains(Vector2<T> v) const { return v >= lower && v <= upper; }
        bool contains(const Box2 &other) const { return contains(other.lower) && contains(other.upper); }

        Box2 intersection(const Box2 &other) const
        {
            return Box2(
                Vector3<T>(std::max(lower.x, other.lower.x), std::max(lower.y, other.lower.y), std::max(lower.z, other.lower.z)),
                Vector3<T>(std::min(upper.x, other.upper.x), std::min(upper.y, other.upper.y), std::min(upper.z, other.upper.z))
                );
        }

        Box2 unionWith(const Box2 &other) const
        {
            return Box2(
                Vector2<T>(std::min(lower.x, other.lower.x), std::min(lower.y, other.lower.y)),
                Vector2<T>(std::max(upper.x, other.upper.x), std::max(upper.y, other.upper.y))
                );
        }

        T area() const
        {
            return std::max(size().area(), 0);
        }

        Box2 operator *(const Vector2<T> &b) const { return Box2(lower * b, upper * b); }
        Box2 operator +(const Vector2<T> &b) const { return Box2(lower + b, upper + b); }
        Box2 operator -(const Vector2<T> &b) const { return Box2(lower - b, upper - b); }
        Box2 operator /(const Vector2<T> &b) const { return Box2(lower / b, upper / b); }
        Box2 operator *(T b) const { return Box2(lower * b, upper * b); }
        Box2 operator +(T b) const { return Box2(lower + b, upper + b); }
        Box2 operator -(T b) const { return Box2(lower - b, upper - b); }
        Box2 operator /(T b) const { return Box2(lower / b, upper / b); }
    };

    typedef Box2<float> Box2f;
    typedef Box2<int32_t>   Box2i;

    template <typename T, typename U> Box2<T> CastBox3(const Box2<U> &a)
    {
        return Box2<T>(CastVector2<T>(a.lower), CastVector2<T>(a.upper));
    }

    template <typename T>
    struct Box3
    {
        Vector3<T> lower, upper;

        Box3() : lower(), upper() { }
        Box3(const Box3 &a) : lower(a.lower), upper(a.upper) { }
        Box3(Vector3<T> _lower, Vector3<T> _upper) : lower(_lower), upper(_upper) { }

        Vector3<T> size() const { return Vector3<T>(upper.x - lower.x, upper.y - lower.y, upper.z - lower.z); }
        bool intersectsWith(const Box3 &other) const { return other.lower <= upper && other.upper >= lower; }
        bool contains(Vector3<T> v) const { return v >= lower && v <= upper; }
        bool contains(const Box3 &other) const { return contains(other.lower) && contains(other.upper); }

        Box3 intersection(const Box3 &other) const
        {
            return Box3(
                Vector3<T>(std::max(lower.x, other.lower.x), std::max(lower.y, other.lower.y), std::max(lower.z, other.lower.z)),
                Vector3<T>(std::min(upper.x, other.upper.x), std::min(upper.y, other.upper.y), std::min(upper.z, other.upper.z))
                );
        }

        Box3 unionWith(const Box3 &other) const
        {
            if (size().vmax() == 0)       return other;
            if (other.size().vmax() == 0) return *this;

            return Box3(
                Vector3<T>(std::min(lower.x, other.lower.x), std::min(lower.y, other.lower.y), std::min(lower.z, other.lower.z)),
                Vector3<T>(std::max(upper.x, other.upper.x), std::max(upper.y, other.upper.y), std::max(upper.z, other.upper.z))
                );
        }

        T volume() const
        {
            return std::max<T>(size().volume(), 0);
        }

        Box3 operator *(const Vector3<T> &b) const { return Box3(lower * b, upper * b); }
        Box3 operator +(const Vector3<T> &b) const { return Box3(lower + b, upper + b); }
        Box3 operator -(const Vector3<T> &b) const { return Box3(lower - b, upper - b); }
        Box3 operator /(const Vector3<T> &b) const { return Box3(lower / b, upper / b); }
        Box3 operator *(T b) const { return Box3(lower * b, upper * b); }
        Box3 operator +(T b) const { return Box3(lower + b, upper + b); }
        Box3 operator -(T b) const { return Box3(lower - b, upper - b); }
        Box3 operator /(T b) const { return Box3(lower / b, upper / b); }
    };

    typedef Box3<float> Box3f;
    typedef Box3<int32_t>   Box3i;

    template <typename T, typename U> Box3<T> CastBox3(const Box3<U> &a)
    {
        return Box3<T>(CastVector3<T>(a.lower), CastVector3<T>(a.upper));
    }

    template <typename T>
    struct Box4
    {
        Vector4<T> lower, upper;

        Box4() : lower(), upper() { }
        Box4(const Box4 &a) : lower(a.lower), upper(a.upper) { }
        Box4(const Box3<T> &a) : lower(Vector4<T>(a.lower, 0)), upper(Vector4<T>(a.upper, 0)) { }
        Box4(Vector4<T> _lower, Vector4<T> _upper) : lower(_lower), upper(_upper) { }
    };

    typedef Box4<float> Box4f;
    typedef Box4<int32_t>   Box4i;

    struct Plane
    {
        Vector3<float> normal;
        float distance;

        Plane() : normal(), distance(0.f) { }
        Plane(const Plane &p) : normal(p.normal), distance(p.distance) { }
        Plane(float x, float y, float z, float d) : normal(x, y, z), distance(d) { }

        Vector4<float> plane() const { return Vector4<float>(normal, distance); }

        void normalize()
        {
            float lengthSq = normal.lengthSq();
            float scale = (lengthSq > std::numeric_limits<float>::epsilon() ? (1.0f / sqrtf(lengthSq)) : 0);
            normal *= scale;
            distance *= scale;
        }
    };

    struct Frustum
    {
        enum FrustumPlanes
        {
            NEAR_PLANE = 0,
            FAR_PLANE,
            LEFT_PLANE,
            RIGHT_PLANE,
            TOP_PLANE,
            BOTTOM_PLANE,
            PLANES_COUNT
        };

        Plane planes[PLANES_COUNT];

        Frustum() { }
        Frustum(const Frustum &f)
        {
            planes[0] = f.planes[0];
            planes[1] = f.planes[1];
            planes[2] = f.planes[2];
            planes[3] = f.planes[3];
            planes[4] = f.planes[4];
            planes[5] = f.planes[5];
        }
        Frustum(const Matrix4<float> &viewProjMatrix)
        {
            planes[NEAR_PLANE] = Plane(-viewProjMatrix.row_0.z, -viewProjMatrix.row_1.z, -viewProjMatrix.row_2.z, viewProjMatrix.row_3.z);
            planes[FAR_PLANE] = Plane(-viewProjMatrix.row_0.w + viewProjMatrix.row_0.z, -viewProjMatrix.row_1.w + viewProjMatrix.row_1.z, -viewProjMatrix.row_2.w + viewProjMatrix.row_2.z, viewProjMatrix.row_3.w - viewProjMatrix.row_3.z);

            planes[LEFT_PLANE] = Plane(-viewProjMatrix.row_0.w - viewProjMatrix.row_0.x, -viewProjMatrix.row_1.w - viewProjMatrix.row_1.x, -viewProjMatrix.row_2.w - viewProjMatrix.row_2.x, viewProjMatrix.row_3.w + viewProjMatrix.row_3.x);
            planes[RIGHT_PLANE] = Plane(-viewProjMatrix.row_0.w + viewProjMatrix.row_0.x, -viewProjMatrix.row_1.w + viewProjMatrix.row_1.x, -viewProjMatrix.row_2.w + viewProjMatrix.row_2.x, viewProjMatrix.row_3.w - viewProjMatrix.row_3.x);

            planes[TOP_PLANE] = Plane(-viewProjMatrix.row_0.w + viewProjMatrix.row_0.y, -viewProjMatrix.row_1.w + viewProjMatrix.row_1.y, -viewProjMatrix.row_2.w + viewProjMatrix.row_2.y, viewProjMatrix.row_3.w - viewProjMatrix.row_3.y);
            planes[BOTTOM_PLANE] = Plane(-viewProjMatrix.row_0.w - viewProjMatrix.row_0.y, -viewProjMatrix.row_1.w - viewProjMatrix.row_1.y, -viewProjMatrix.row_2.w - viewProjMatrix.row_2.y, viewProjMatrix.row_3.w + viewProjMatrix.row_3.y);

            planes[0].normalize();
            planes[1].normalize();
            planes[2].normalize();
            planes[3].normalize();
            planes[4].normalize();
            planes[5].normalize();
        }

        bool intersectsWith(const Vector3<float> &point) const
        {
            for (int i = 0; i<PLANES_COUNT; ++i)
            {
                float distance = planes[i].normal.x * point.x + planes[i].normal.y * point.y + planes[i].normal.z * point.z;
                if (distance > planes[i].distance) return false;
            }

            return true;
        }

        bool intersectsWith(const Box3<float> &box) const
        {
            Vector3f minPt;

            for (int i = 0; i < PLANES_COUNT; ++i)
            {
                minPt.x = planes[i].normal.x > 0 ? box.lower.x : box.upper.x;
                minPt.y = planes[i].normal.y > 0 ? box.lower.y : box.upper.y;
                minPt.z = planes[i].normal.z > 0 ? box.lower.z : box.upper.z;

                float distance = planes[i].normal.x * minPt.x + planes[i].normal.y * minPt.y + planes[i].normal.z * minPt.z;
                if (distance > planes[i].distance) return false;
            }

            return true;
        }

        void extendForConservativeVoxelization(float voxelSize)
        {
            // Move every plane outwards in the direction of its normal to get conservative voxelization with regular sampling

            for (int plane = 0; plane < Frustum::PLANES_COUNT; plane++)
            {
                Plane& p = planes[plane];
                p.distance += (abs(p.normal.x) + abs(p.normal.y) + abs(p.normal.z)) * voxelSize * 0.5f;
            }
        }
    };


} //nvgi
GI_END_TIGHT_PACKING

#endif // GFSDK_VXGI_MATHTYPES_H_
