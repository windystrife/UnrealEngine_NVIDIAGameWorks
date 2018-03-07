/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoGfsdkMath.h"

namespace nvidia {
namespace Common {
namespace GfsdkMath {


Vec3 rotate(const Quaternion& q, const Vec3& v)
{
	const float vx = 2.0f * v.x;
	const float vy = 2.0f * v.y;
	const float vz = 2.0f * v.z;
	const float w2 = q.w * q.w - 0.5f;
	const float dot2 = (q.x * vx + q.y * vy + q.z * vz);

	Vec3 ret;

	ret.x = vx * w2 + (q.y * vz - q.z * vy) * q.w + q.x * dot2;
	ret.y = vy * w2 + (q.z * vx - q.x * vz) * q.w + q.y * dot2;
	ret.z = vz * w2 + (q.x * vy - q.y * vx) * q.w + q.z * dot2;

	return ret;
}

Vec3 rotateInv(const Quaternion& q, const Vec3& v)
{
	const float vx = 2.0f * v.x;
	const float vy = 2.0f * v.y;
	const float vz = 2.0f * v.z;
	const float w2 = q.w * q.w - 0.5f;
	const float dot2 = (q.x * vx + q.y * vy + q.z * vz);

	Vec3 ret;

	ret.x = vx * w2 - (q.y * vz - q.z * vy) * q.w + q.x * dot2;
	ret.y = vy * w2 - (q.z * vx - q.x * vz) * q.w + q.y * dot2;
	ret.z = vz * w2 - (q.x * vy - q.y * vx) * q.w + q.z * dot2;

	return ret;
}

Vec3 getBasisX(const Quaternion& q)
{
	const float x2 = q.x * 2.0f;
	const float w2 = q.w * 2.0f;
	return makeVec3(
		(q.w * w2) - 1.0f + q.x * x2,
		(q.z * w2)        + q.y * x2,
		(-q.y * w2)       + q.z * x2);
}

Vec3 getBasisY(const Quaternion& q)
{
	const float y2 = q.y * 2.0f;
	const float w2 = q.w * 2.0f;
	return makeVec3(
		(-q.z * w2)			+ q.x * y2,
		( q.w * w2) - 1.0f	+ q.y * y2,
		( q.x * w2)			+ q.z * y2);
}

Vec3 getBasisZ(const Quaternion& q)
{
	const float z2 = q.z * 2.0f;
	const float w2 = q.w * 2.0f;
	
	return makeVec3(	
		( q.y * w2)			+ q.x * z2,
		(-q.x * w2)			+ q.y * z2,
		( q.w * w2) - 1.0f	+ q.z * z2);
}

Quaternion getConjugate(const Quaternion& in)
{
	return makeQuaternion(-in.x, -in.y, -in.z, in.w);
}

void setIdentity(Quaternion& q)
{
	q = makeQuaternion(0, 0, 0, 1);
}

Quaternion makeRotation(const Vec3& axis, float radian)
{
	float a = radian * 0.5f;
	float s = sinf(a);
	
	Quaternion q;
	q.w = cosf(a);
	q.x = axis.x * s;
	q.y = axis.y * s;
	q.z = axis.z * s;

	return q;
}

Quaternion quatFromAxis(const Vec3& n, const Vec3& t)
{
	Vec3 az = n;
	Vec3 ay = calcNormalized(cross(az, t));
	Vec3 ax = calcNormalized(cross(ay,az));

	Quaternion q;
	setRotation(ax, ay, az, q);
	return q;
}

Quaternion rotateBetween(const Vec3& from, const Vec3& to)
{
	Vec3 axis = cross(from, to);

	float axisLength = calcLength(axis);
	if (axisLength < 1e-2)
		return makeQuaternion(0,0,0,1);

	axis = (1.0f / axisLength) * axis;

	float dotT = Math::clamp(dot(from, to), -1.0f, 1.0f);
	float angle = acosf( dotT );

	return makeRotation(axis, angle);
}

void setRotation(const Vec3& ax, const Vec3& ay, const Vec3& az, Quaternion& q)
{
	float tr = ax.x + ay.y + az.z;
	float h;
	if(tr >= 0)
	{
		h = sqrtf(tr +1);
		q.w = float(0.5) * h;
		h = float(0.5) / h;

		q.x = (ay.z - az.y) * h;
		q.y = (az.x - ax.z) * h;
		q.z = (ax.y - ay.x) * h;
	}
	else
	{
		float max = ax.x;
		int i = 0; 
		if (ay.y > max)
		{
			i = 1; 
			max = ay.y;
		}
		if (az.z > max)
			i = 2; 

		switch (i)
		{
		case 0:
			h = Math::sqrt((ax.x - (ay.y + az.z)) + 1);
			q.x = float(0.5) * h;
			h = float(0.5) / h;

			q.y = (ay.x + ax.y) * h; 
			q.z = (ax.z + az.x) * h;
			q.w = (ay.z - az.y) * h;
			break;
		case 1:
			h = Math::sqrt((ay.y - (az.z + ax.x)) + 1);
			q.y = float(0.5) * h;
			h = float(0.5) / h;

			q.z = (az.y + ay.z) * h;
			q.x = (ay.x + ax.y) * h;
			q.w = (az.x - ax.z) * h;
			break;
		case 2:
			h = Math::sqrt((az.z - (ax.x + ay.y)) + 1);
			q.z = float(0.5) * h;
			h = float(0.5) / h;

			q.x = (ax.z + az.x) * h;
			q.y = (az.y + ay.z) * h;
			q.w = (ax.y - ay.x) * h;
			break;
		default: // Make compiler happy
			q.x = q.y = q.z = q.w = 0;
			break;
		}
	}	

}

Quaternion quaternionMultiply(const Quaternion& q0, const Quaternion& q1)
{
	Quaternion q;

	const float tx = q0.w * q1.x + q0.x * q1.w + q0.y * q1.z - q0.z * q1.y;
	const float ty = q0.w * q1.y + q0.y * q1.w + q0.z * q1.x - q0.x * q1.z;
	const float tz = q0.w * q1.z + q0.z * q1.w + q0.x * q1.y - q0.y * q1.x;

	q.w = q0.w * q1.w - q0.x * q1.x - q0.y * q1.y - q0.z  * q1.z;
	q.x = tx;
	q.y = ty;
	q.z = tz;

	return q;
}

Quaternion slerp(const Quaternion& q0, const Quaternion& q1, float t)
{
	const float quatEpsilon = (float(1.0e-8f));

	float cosine = dot(asVec4(q0), asVec4(q1));
	float sign = float(1);
	if (cosine < 0)
	{
		cosine = -cosine;
		sign = float(-1);
	}

	float sine = float(1) - cosine*cosine;

	if(sine>=quatEpsilon*quatEpsilon)	
	{
		sine = Math::sqrt(sine);
		const float angle = atan2f(sine, cosine);
		const float i_sin_angle = float(1) / sine;

		const float leftw	= sinf(angle * (float(1)-t)) * i_sin_angle;
		const float rightw	= sinf(angle * t) * i_sin_angle * sign;

		return asQuaternion(asVec4(q0) * leftw + asVec4(q1) * rightw);
	}

	return q0;
}

Quaternion calcRotation(const Mat4x4& sm)
{
	Mat4x4 m = sm;

	orthonormalize(m);

	float x,y,z,w;

	float tr = m._11 + m._22 + m._33, h;
	if(tr >= 0)
	{
		h = Math::sqrt(tr +1);
		w = 0.5f * h;
		h = 0.5f / h;

		x = (m._23 - m._32) * h;
		y = (m._31 - m._13) * h;
		z = (m._12 - m._21) * h;
	}
	else
	{
		float max = m._11;
		int i = 0; 
		if (m._22 > m._11)
		{
			i = 1; 
			max = m._22;
		}
		if (m._33 > max)
			i = 2; 
		switch (i)
		{
		case 0:
			h = Math::sqrt((m._11 - (m._22 + m._33)) + 1);
			x = 0.5f * h;
			h = 0.5f / h;

			y = (m._21 + m._12) * h; 
			z = (m._13 + m._31) * h;
			w = (m._23 - m._32) * h;
			break;
		case 1:
			h = Math::sqrt((m._22 - (m._33 + m._11)) + 1);
			y = 0.5f * h;
			h = 0.5f / h;

			z = (m._32 + m._23) * h;
			x = (m._21 + m._12) * h;
			w = (m._31 - m._13) * h;
			break;
		case 2:
			h = Math::sqrt((m._33 - (m._11 + m._22)) + 1);
			z = 0.5f * h;
			h = 0.5f / h;

			x = (m._13 + m._31) * h;
			y = (m._32 + m._23) * h;
			w = (m._12 - m._21) * h;
			break;
		default: // Make compiler happy
			x = y = z = w = 0.0f;
			break;
		}
	}	

	return makeQuaternion(x,y,z,w);
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!! DualQuaternion !!!!!!!!!!!!!!!!!!!!!!!!!!!! */

DualQuaternion makeDualQuaternion(const Quaternion& q, const Vec3& t)
{
	DualQuaternion dq;

	dq.q0 = calcNormalized(q);
	dq.q1 = quaternionMultiply(makeQuaternion(t.x, t.y, t.z, 0), dq.q0) * 0.5f;

	return dq;
}

DualQuaternion calcNormalized(const DualQuaternion& dq)
{
	float mag = dot( asVec4(dq.q0), asVec4(dq.q0));
	float deLen = 1.0f / Math::sqrt(mag+FLT_EPSILON);
	DualQuaternion dqOut;
	dqOut.q0 = dq.q0 * deLen;
	dqOut.q1 = dq.q1 * deLen;
	return dqOut;
}

DualQuaternion operator*(float s, const DualQuaternion& dq)
{
	DualQuaternion ndq = { s * dq.q0, s * dq.q1 };
	return ndq;
}

DualQuaternion operator*(const DualQuaternion& dq, float s)
{
	DualQuaternion ndq = {s * dq.q0, s * dq.q1};
	return ndq;
}

DualQuaternion& operator+=(DualQuaternion& dq, const DualQuaternion& dq2)
{
	// hemi-spherization
	float sign = (dot(dq.q0, dq2.q0) < -FLT_EPSILON) ? -1.0f: 1.0f;

	dq.q0 += sign * dq2.q0;
	dq.q1 += sign * dq2.q1;

	return dq;
}

DualQuaternion lerp(const DualQuaternion& dq1, const DualQuaternion& dq2, float t)
{
	DualQuaternion dq = dq1 * (1.0f - t);
	float sign = (dot(dq1.q0, dq2.q0) < -FLT_EPSILON) ? -1.0f: 1.0f;
	dq += (t * sign) * dq2;
	return calcNormalized(dq);
}

Vec3 transformCoord(const DualQuaternion& dq, const Vec3& vecIn) 
{
	Vec3 d0 = makeVec3(dq.q0.x, dq.q0.y, dq.q0.z);
	Vec3 de = makeVec3(dq.q1.x, dq.q1.y, dq.q1.z);
	float a0 = dq.q0.w;
	float ae = dq.q1.w;

	Vec3 temp = cross( d0, vecIn ) + a0 * vecIn;
	Vec3 temp2 = 2.0f * (a0 * de - ae * d0 + cross(d0, de));

	return vecIn + temp2 + 2.0f * cross( d0, temp);
}

Vec3 transformVector(const DualQuaternion& dq, const Vec3& vecIn) 
{
	Vec3 d0 = makeVec3(dq.q0.x, dq.q0.y, dq.q0.z);
	float a0 = dq.q0.w;

	Vec3 temp = cross( d0, vecIn ) + a0 * vecIn;
	return vecIn + 2.0f * cross( d0, temp);
}

Quaternion calcRotation(const DualQuaternion& dq)
{
	return dq.q0;
}

Vec3 getTranslation(const DualQuaternion& dq)
{
	Quaternion dual = 2.0f * dq.q1;
	Quaternion t = quaternionMultiply(dual, getConjugate( dq.q0 ));

	return makeVec3(t.x, t.y, t.z);
}

DualQuaternion makeDualQuaternion(const Mat4x4& m)
{
	Quaternion q = calcRotation(m);
	Vec3 t = getTranslation(m);

	return makeDualQuaternion(q, t);
}

void setIdentity(DualQuaternion& dq)
{
	dq.q0 = makeQuaternion(0.0f, 0.0f, 0.0f, 1.0f);
	dq.q1 = makeQuaternion(0.0f, 0.0f, 0.0f, 0.0f);
}

void setZero(DualQuaternion& dq)
{
	dq.q0 = makeQuaternion(0.0f, 0.0f, 0.0f, 0.0f);
	dq.q1 = makeQuaternion(0.0f, 0.0f, 0.0f, 0.0f);
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Mat4x4 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

void setMat4x4(const float* p, Mat4x4& m)
{
	memcpy(&m, p, sizeof(float) * 16);
}

void setIdentity(Mat4x4& transform)
{
	memset(&transform, 0, sizeof(Mat4x4));
	transform._11 = 1.0f;
	transform._22 = 1.0f;
	transform._33 = 1.0f;
	transform._44 = 1.0f;
}

void setDiagonal(const Vec3& d, Mat4x4& transform)
{
	memset(&transform, 0, sizeof(Mat4x4));
	transform._11 = d.x;
	transform._22 = d.y;
	transform._33 = d.z;
	transform._44 = 1.0f;
}

void setTranslation(const Vec3& t, Mat4x4& m)
{
	setIdentity(m);
	m._41 = t.x;
	m._42 = t.y;
	m._43 = t.z;
}

void setScale(const Vec3& s, Mat4x4& m)
{
	setIdentity(m);
	m._11 = s.x;
	m._22 = s.y;
	m._33 = s.z;
}

void setRotation(const Quaternion& q, Mat4x4& m)
{
	setIdentity(m);

	const float x = q.x;
	const float y = q.y;
	const float z = q.z;
	const float w = q.w;

	const float x2 = x + x;
	const float y2 = y + y;
	const float z2 = z + z;

	const float xx = x2*x;
	const float yy = y2*y;
	const float zz = z2*z;

	const float xy = x2*y;
	const float xz = x2*z;
	const float xw = x2*w;

	const float yz = y2*z;
	const float yw = y2*w;
	const float zw = z2*w;

	m._11 = 1.0f - yy - zz;
	m._12 = xy + zw;
	m._13 = xz - yw;

	m._21 = xy - zw;
	m._22 = 1.0f - xx - zz;
	m._23 = yz + xw;

	m._31 = xz + yw;
	m._32 = yz - xw;
	m._33 = 1.0f - xx - yy;
}

void setRotation(const Vec3& xaxis, const Vec3& yaxis, const Vec3& zaxis, Mat4x4& m)
{
	setIdentity(m);

	m._11 = xaxis.x;  m._12 = xaxis.y; m._13 = xaxis.z;
	m._21 = yaxis.x;  m._22 = yaxis.y; m._23 = yaxis.z;
	m._31 = zaxis.x;  m._32 = zaxis.y; m._33 = zaxis.z;
}

void setRotation(const Vec3& from, const Vec3& to, Mat4x4& R)
{
	setIdentity(R);

	// Early exit if to = from
	if( calcLengthSquared(from - to) < 1e-4f )
	{
		return;
	}

	// Early exit if to = -from
	if( calcLengthSquared(from + to) < 1e-4f )
	{
		setDiagonal(makeVec3(1.0f, -1.0f, -1.0f), R);
		return;
	}

	Vec3 n = cross(from, to);

	float C = dot(from, to);
	float S = Math::sqrt(1.0f - C * C);
	float CC = 1.0f - C;

	float xx = n.x * n.x;
	float yy = n.y * n.y;
	float zz = n.z * n.z;
	float xy = n.x * n.y;
	float yz = n.y * n.z;
	float xz = n.x * n.z;

	R._11 =  1 + CC * (xx - 1);
	R._21 = -n.z * S + CC * xy;
	R._31 =  n.y * S + CC * xz;

	R._12 =  n.z * S + CC * xy;
	R._22 =  1 + CC * (yy - 1);
	R._32 = -n.x * S + CC * yz;

	R._13 = -n.y * S + CC * xz;
	R._23 =  n.x * S + CC * yz;
	R._33 =  1 + CC * (zz - 1);
}

Mat4x4 makeTransform(const Quaternion& q, const Vec3& t, const Vec3& s)
{
	Mat4x4 m;
	setRotation(q, m);

	m._11 *= s.x; m._12 *= s.x; m._13 *= s.x;
	m._21 *= s.y; m._22 *= s.y; m._23 *= s.y;
	m._31 *= s.z; m._32 *= s.z; m._33 *= s.z;

	m._41 = t.x; 
	m._42 = t.y;
	m._43 = t.z;

	return m;
}

bool inverseProjection(Mat4x4& out, const Mat4x4& in)
{
	// inverse of a projection matrix

	setIdentity(out);

	// perspective projection requires separate treatment
	// | A 0 0 0 |
	// | 0 B 0 0 |
	// | 0 0 C D |
	// | 0 0 E F |

	// RH: A = xScale, B = yScale, C = zf/(zn-zf),  D = -1, E = zn*zf/(zn-zf)
	// LH: A = xScale, B = yScale, C = -zf/(zn-zf), D = 1,  E = zn*zf/(zn-zf)

	float A = in._11;
	float B = in._22;
	float C = in._33;
	float D = in._34;
	float E = in._43;
	float F = in._44;

	if (F != 0.0f) // non-perspective, non-degenerate
	{
		out = inverse(in);
		return true;
	}

	if ( (D == 0.0f) || (E == 0.0f)) // can't be zero in normal perspective projection matrix;
		return false;

	/* UE4 test matrix
	Mat4x4 in;
	in._11 = 1.0f, in._12 = 0.0f,			in._13 = 0.0f,	in._14 = 0.0f;
	in._21 = 0.0f, in._22 = 1.12275445f,	in._23 = 0.0f,	in._24 = 0.0f;
	in._31 = 0.0f, in._32 = 0.0f,			in._33 = 0.0f,	in._34 = 1.0f;
	in._41 = 0.0f, in._42 = 0.0f,			in._43 = 10.0f, in._44 = 0.0f;
	*/

	// x' = Ax, y' = By, z' = Cz + Ew, w' = D * z
	// x = x' / A
	// y = y' / B, 
	// z = w' / D
	// w = (z' - Cz) / E = (z' - C * (w' / D) ) / E
	//  = (1/E) * z' - C / (D * E) * w'

	// Inverse = 
	// | 1/A 0		0		0		|
	// | 0   1/B	0		0		|
	// | 0   0		0		1/E		|
	// | 0   0		1/D		-C/(D*E)|

	out._11 = 1.0f / A;		out._12 = 0.0f;			out._13 = 0.0f;		out._14 = 0.0f;
	out._21 = 0.0f;			out._22 = 1.0f / B;		out._23 = 0.0f;		out._24 = 0.0f;
	out._31 = 0.0f;			out._32 = 0.0f;			out._33 = 0.0f;		out._34 = 1.0f / E;
	out._41 = 0.0f;			out._42 = 0.0f;			out._43 = 1.0f / D; out._44 = -1.0f * C / (D * E);

	return true;
}

Mat4x4 lerp(const Mat4x4& start, const Mat4x4& end, float t)
{
	Quaternion sq = calcRotation(start);
	Quaternion eq = calcRotation(end);
	Vec3 st = getTranslation(start);
	Vec3 et = getTranslation(end);

	Vec3 ss = getScale(start);
	Vec3 es = getScale(end);
	Vec3 s = lerp(ss, es, t);

	DualQuaternion sdq = makeDualQuaternion(sq, st);
	DualQuaternion edq = makeDualQuaternion(eq, et);

	DualQuaternion dq = lerp(sdq, edq, t);

	Quaternion gr = calcRotation(dq);
	Vec3 gt = getTranslation(dq);

	return makeTransform(gr, gt, s);
}

Mat4x4 operator*(const Mat4x4& in1, const Mat4x4& in2)
{
#define MATRIX_SUM(OUT, IN1, IN2, ROW, COL) OUT._##ROW##COL = IN1._##ROW##1 * IN2._1##COL + IN1._##ROW##2 * IN2._2##COL + IN1._##ROW##3 * IN2._3##COL + IN1._##ROW##4 * IN2._4##COL;

	Mat4x4 out;

	MATRIX_SUM(out, in1, in2, 1, 1);
	MATRIX_SUM(out, in1, in2, 1, 2);
	MATRIX_SUM(out, in1, in2, 1, 3);
	MATRIX_SUM(out, in1, in2, 1, 4);

	MATRIX_SUM(out, in1, in2, 2, 1);
	MATRIX_SUM(out, in1, in2, 2, 2);
	MATRIX_SUM(out, in1, in2, 2, 3);
	MATRIX_SUM(out, in1, in2, 2, 4);

	MATRIX_SUM(out, in1, in2, 3, 1);
	MATRIX_SUM(out, in1, in2, 3, 2);
	MATRIX_SUM(out, in1, in2, 3, 3);
	MATRIX_SUM(out, in1, in2, 3, 4);

	MATRIX_SUM(out, in1, in2, 4, 1);
	MATRIX_SUM(out, in1, in2, 4, 2);
	MATRIX_SUM(out, in1, in2, 4, 3);
	MATRIX_SUM(out, in1, in2, 4, 4);

#undef MATRIX_SUM

	return out;
}

Vec3 getScale(const Mat4x4& m)
{
	Vec3 ax = makeVec3(m._11, m._12, m._13);
	Vec3 ay = makeVec3(m._21, m._22, m._23);
	Vec3 az = makeVec3(m._31, m._32, m._33);

	return makeVec3(calcLength(ax), calcLength(ay), calcLength(az));
}

Vec3 getTranslation(const Mat4x4& m)
{
	return makeVec3(m._41, m._42, m._43);
}

void setTranslation(Mat4x4& m, const Vec3 &v)
{
	m._41 = v.x;
	m._42 = v.y;
	m._43 = v.z;
}

void orthonormalize(Mat4x4& m)
{
	Vec3 ax = calcNormalized(makeVec3(m._11, m._12, m._13));
	Vec3 ay = calcNormalized(makeVec3(m._21, m._22, m._23));
	Vec3 az = calcNormalized(makeVec3(m._31, m._32, m._33));

	m._11 = ax.x; m._12 = ax.y; m._13 = ax.z;
	m._21 = ay.x; m._22 = ay.y; m._23 = ay.z;
	m._31 = az.x; m._32 = az.y; m._33 = az.z;

}

Mat4x4& operator*=(Mat4x4& m, float s)
{
	float* data = (float*)&m;
	for (int i = 0; i < 16; i++)
		data[i] *= s;

	return m;
}

Mat4x4& operator+=(Mat4x4& m1, const Mat4x4& m2)
{
	float* data1 = (float*)&m1;
	float* data2 = (float*)&m2;

	for (int i = 0; i < 16; i++)
		data1[i] += data2[i];

	return m1;
}

float getDeterminant(const Mat4x4& m)
{
	const float* matrix = (const float*)&m;

	Vec3 p0 = makeVec3(matrix[0 * 4 + 0], matrix[0 * 4 + 1], matrix[0 * 4 + 2]);
	Vec3 p1 = makeVec3(matrix[1 * 4 + 0], matrix[1 * 4 + 1], matrix[1 * 4 + 2]);
	Vec3 p2 = makeVec3(matrix[2 * 4 + 0], matrix[2 * 4 + 1], matrix[2 * 4 + 2]);

	Vec3 tempv = cross(p1, p2);

	return dot(p0, tempv);
}

Vec3 transformCoord(const Mat4x4& m, Vec3 op)
{
	Vec3 p;

	p.x = op.x * m._11 + op.y * m._21 + op.z * m._31 + m._41;
	p.y = op.x * m._12 + op.y * m._22 + op.z * m._32 + m._42;
	p.z = op.x * m._13 + op.y * m._23 + op.z * m._33 + m._43;

	return p;
}

Vec3 transformVector(const Mat4x4& m, Vec3 op)
{
	Vec3 p;

	p.x = op.x * m._11 + op.y * m._21 + op.z * m._31;
	p.y = op.x * m._12 + op.y * m._22 + op.z * m._32;
	p.z = op.x * m._13 + op.y * m._23 + op.z * m._33;

	return p;
}

Mat4x4 getSubMatrix(int ki, int kj, const Mat4x4& m)
{
	Mat4x4 out;
	setIdentity(out);

	float* pDst = (float*)&out;
	const float* matrix = (const float*)&m;

	int row, col;
	int dstCol = 0, dstRow = 0;

	for (col = 0; col < 4; col++)
	{
		if (col == kj)
		{
			continue;
		}
		for (dstRow = 0, row = 0; row < 4; row++)
		{
			if (row == ki)
			{
				continue;
			}
			pDst[dstCol * 4 + dstRow] = matrix[col * 4 + row];
			dstRow++;
		}
		dstCol++;
	}

	return out;
}

Mat4x4 inverse(const Mat4x4& m)
{
	Mat4x4 im;

	float* inverse_matrix = (float*)&im;

	float det = getDeterminant(m);
	det = 1.0f / det;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			int sign = 1 - ((i + j) % 2) * 2;

			Mat4x4 subMat = getSubMatrix(i, j, m);
			float subDeterminant = getDeterminant(subMat);

			inverse_matrix[i * 4 + j] = (subDeterminant * sign) * det;
		}
	}

	return im;
}


Mat4x4	makeTransform(const DualQuaternion& dq)
{
	Vec3 t = getTranslation(dq);
	Vec3 s = makeVec3(1.0f, 1.0f, 1.0f);
	return makeTransform(dq.q0, t, s);
}

Vec3 getColumn(const Mat4x4& m, int col)
{
	float* base = (float*)(&m);
	return makeVec3( base[col], base[ 4 + col], base [ 8 + col] );
}

Vec3 getRow(const Mat4x4& m, int row)
{
	float* base = (float*)(&m) + row * 4;
	return makeVec3( base[0], base[1], base[2] );
}

void setColumn(const Mat4x4& m, int col, const Vec3& v)
{
	float* base = (float*)(&m);
	base[0 + col] = v.x;
	base[4 + col] = v.y;
	base[8 + col] = v.z;

}

void setRow(const Mat4x4& m, int row, const Vec3& v)
{
	float* base = (float*)(&m) + row * 4;
	base[0] = v.x;
	base[1] = v.y;
	base[2] = v.z;
}

void calcJacobiRotate(const Mat4x4& A, int p, int q, Mat4x4& R)
{
	float Apq = getElement(A, p, q);
	float Aqp = getElement(A, q, p);
	float App = getElement(A, p, p);
	float Aqq = getElement(A, q, q);

	// rotates A through phi in pq-plane to set A(p,q) = 0
	// rotation stored in R whose columns are eigenvectors of A
	if (Apq == 0.0f)
		return;

	float d = (App - Aqq) / (2.0f * Apq);
	float t = 1.0f / (Math::abs(d) + Math::sqrt(d*d + 1.0f));
	if (d < 0.0f) 
		t = -t;

	float c = 1.0f / Math::sqrt(t*t + 1);
	float s = t*c;

	App += t * Apq;
	Aqq -= t * Apq;
	Apq = Aqp = 0.0f;

	// transform A
	int k;
	for (k = 0; k < 3; k++) 
	{
		if (k != p && k != q) 
		{
			float Akp = getElement(A, k, p);
			float Akq = getElement(A, k, q);
			float Apk = getElement(A, p, k);
			float Aqk = getElement(A, q, k);

			float R1 = c * Akp + s * Akq;
			float R2 =-s * Akp + c * Akq;
			Akp = Apk = R1;
			Akq = Aqk = R2;
		}
	}

	// store rotation in R
	for (k = 0; k < 3; k++) 
	{
		float& Rkp = getElement(R, k, p);
		float& Rkq = getElement(R, k, q);

		float R1 =  c * Rkp + s * Rkq;
		float R2 = -s * Rkp + c * Rkq;
		Rkp = R1;
		Rkq = R2;
	}
}

void calcEigenDecomposition(Mat4x4& A, Mat4x4& R)
{
	const int numJacobiIterations = 10;
	const float epsilon = 1e-15f;

	// only for symmetric matrices!
	setIdentity(R);

	int iter = 0;
	while (iter < numJacobiIterations) {	// 3 off diagonal elements
		// find off diagonal element with maximum modulus
		
		float max = Math::abs(getElement(A, 0, 1));
		int p = 0; 
		int q = 1;

		float a = Math::abs(getElement(A, 0, 2));
		if (a > max) { 
			p = 0; q = 2; max = a; 
		}
		
		a = Math::abs(getElement(A, 1, 2));
		if (a > max) { 
			p = 1; q = 2; max = a; 
		}

		// all small enough -> done
		if (max < epsilon) 
			break;

		// rotate matrix with respect to that element
		calcJacobiRotate(A, p, q, R);
		iter++;
	}
}

void calcPolarDecomposition(const Mat4x4& A, Mat4x4& R)
{
	// A = SR, where S is symmetric and R is orthonormal
	// -> S = (A A^T)^(1/2)

	Mat4x4 AAT;

	AAT._11 = A._11 * A._11 + A._12 * A._12 + A._13 * A._13;
	AAT._22 = A._21 * A._21 + A._22 * A._22 + A._23 * A._23;
	AAT._33 = A._31 * A._31 + A._32 * A._32 + A._33 * A._33;

	AAT._12 = A._11 * A._21 + A._12 * A._22 + A._13 * A._23;
	AAT._13 = A._11 * A._31 + A._12 * A._32 + A._13 * A._33;
	AAT._23 = A._21 * A._31 + A._22 * A._32 + A._23 * A._33;

	AAT._21 = AAT._12;
	AAT._31 = AAT._13;
	AAT._32 = AAT._23;

	Mat4x4 U;

	setIdentity(R);

	calcEigenDecomposition(AAT, U);

	const float eps = 1e-15f;

	float l0 = AAT._11; if (l0 <= eps) l0 = 0.0f; else l0 = 1.0f / Math::sqrt(l0);
	float l1 = AAT._22; if (l1 <= eps) l1 = 0.0f; else l1 = 1.0f / Math::sqrt(l1);
	float l2 = AAT._33; if (l2 <= eps) l2 = 0.0f; else l2 = 1.0f / Math::sqrt(l2);

	Mat4x4 S1;

	S1._11 = l0 * U._11 * U._11 + l1 * U._12 * U._12 + l2 * U._13 * U._13;
	S1._22 = l0 * U._21 * U._21 + l1 * U._22 * U._22 + l2 * U._23 * U._23;
	S1._33 = l0 * U._31 * U._31 + l1 * U._32 * U._32 + l2 * U._33 * U._33;

	S1._12 = l0 * U._11 * U._21 + l1 * U._12 * U._22 + l2 * U._13 * U._23;
	S1._13 = l0 * U._11 * U._31 + l1 * U._12 * U._32 + l2 * U._13 * U._33;
	S1._23 = l0 * U._21 * U._31 + l1 * U._22 * U._32 + l2 * U._23 * U._33;

	S1._21 = S1._12;
	S1._31 = S1._13;
	S1._32 = S1._23;

	R = S1 * A;

	// stabilize
	Vec3 c0 = getColumn(R, 0);
	Vec3 c1 = getColumn(R, 1);
	Vec3 c2 = getColumn(R, 2);

	if (calcLengthSquared(c0) < eps)
		c0 = cross(c1,c2);
	else if (calcLengthSquared(c1) < eps)
		c1 = cross(c2,c0);
	else 
		c2 = cross(c0,c1);

	setColumn(R, 0, c0);
	setColumn(R, 1, c1);
	setColumn(R, 2, c2);
}

bool isLeftHanded(const Mat4x4& m)
{
	Vec3 x = getColumn(m, 0);
	Vec3 y = getColumn(m, 1);
	Vec3 z = getColumn(m, 2);

	Vec3 xcrossy = cross(x,y);
	float d = dot(xcrossy, z);
	return d < 0;
}

Mat4x4 makeOrthoLh(float orthoW, float orthoH, float zNear, float zFar)
{
	Mat4x4 out;
	setIdentity(out);

	out._11 = 2.0f / orthoW;
	out._22 = 2.0f / orthoH;
	out._33 = 1.0f / (zFar - zNear);
	out._43 = zNear / (zNear - zFar);

	return out;
}

Mat4x4 makeOrthoRh(float orthoW, float orthoH, float zNear, float zFar)
{
	Mat4x4 out;
	setIdentity(out);

	out._11 = 2.0f / orthoW;
	out._22 = 2.0f / orthoH;
	out._33 = 1.0f / (zNear - zFar);
	out._43 = zNear / (zNear - zFar);

	return out;
}

bool equal(const Mat4x4& inA, const Mat4x4& inB)
{
	if (&inA == &inB)
	{
		return true;
	}
	const int size = int(sizeof(Mat4x4) / sizeof(float));
	const float* a = &inA._11;
	const float* b = &inB._11;
	for (int i = 0; i < size; i++)
	{
		if (a[i] != b[i])
		{
			return false;
		}
	}
	return true;
}

} // namespace GfsdkMath
} // namespace Common  
} // namespace nvidia 