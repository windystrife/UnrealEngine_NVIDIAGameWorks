// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.

#ifndef NV_CO_KEY_CODE_H
#define NV_CO_KEY_CODE_H

namespace nvidia {
namespace Common {

struct MouseButtonFlag { enum Enum
{
	NONE = 0,
	LEFT = 1,
	RIGHT = 2,
	MIDDLE = 4,
	X1 = 8,
	X2 = 16,
}; }; 
typedef MouseButtonFlag::Enum EMouseButtonFlag;

struct KeyCode { enum Enum 
{
	NONE = 0,
	ESCAPE = 1,
	NUM_1 = 2,
	NUM_2 = 3,
	NUM_3 = 4,
	NUM_4 = 5,
	NUM_5 = 6,
	NUM_6 = 7,
	NUM_7 = 8,
	NUM_8 = 9,
	NUM_9 = 10,
	NUM_0 = 11,
	MINUS = 12,
	EQUALS = 13,
	BACK = 14,
	TAB = 15,
	Q = 16,
	W = 17,
	E = 18,
	R = 19,
	T = 20,
	Y = 21,
	U = 22,
	I = 23,
	O = 24,
	P = 25,
	LEFT_BRACKET = 26,
	RIGHT_BRACKET = 27,
	RETURN = 28,
	LEFT_CONTROL = 29,
	A = 30,
	S = 31,
	D = 32,
	F = 33,
	G = 34,
	H = 35,
	J = 36,
	K = 37,
	L = 38,
	SEMICOLON = 39,
	APOSTROPHE = 40,
	GRAVE = 41,
	LEFT_SHIFT = 42,
	BACKSLASH = 43,
	Z = 44,
	X = 45,
	C = 46,
	V = 47,
	B = 48,
	N = 49,
	M = 50,
	COMMA = 51,
	PERIOD = 52,
	SLASH = 53,
	RIGHT_SHIFT = 54,
	MULTIPLY = 55,
	LEFT_MENU = 56,
	SPACE = 57,
	CAPITAL = 58,
	F1 = 59,
	F2 = 60,
	F3 = 61,
	F4 = 62,
	F5 = 63,
	F6 = 64,
	F7 = 65,
	F8 = 66,
	F9 = 67,
	F10 = 68,
	NUM_LOCK = 69,
	SCROLL = 70,
	NUM_PAD_7 = 71,
	NUM_PAD_8 = 72,
	NUM_PAD_9 = 73,
	SUBTRACT = 74,
	NUM_PAD_4 = 75,
	NUM_PAD_5 = 76,
	NUM_PAD_6 = 77,
	ADD = 78,
	NUM_PAD_1 = 79,
	NUM_PAD_2 = 80,
	NUM_PAD_3 = 81,
	NUM_PAD_0 = 82,
	DECIMAL = 83,
	OEM_102 = 86,
	F11 = 87,
	F12 = 88,
	F13 = 100,
	F14 = 101,
	F15 = 102,
	KANA = 112,
	ABNT_C1 = 115,
	CONVERT = 121,
	NO_CONVERT = 123,
	YEN = 125,
	ABNT_C2 = 126,
	NUM_PAD_EQUALS = 141,
	PREV_TRACK = 144,
	AT = 145,
	COLON = 146,
	UNDERLINE = 147,
	KANJI = 148,
	STOP = 149,
	AX = 150,
	UNLABELED = 151,
	NEXT_TRACK = 153,
	NUM_PAD_ENTER = 156,
	RIGHT_CONTROL = 157,
	MUTE = 160,
	CALCULATOR = 161,
	PLAY_PAUSE = 162,
	MEDIA_STOP = 164,
	VOLUME_DOWN = 174,
	VOLUME_UP = 176,
	WEB_HOME = 178,
	NUM_PAD_COMMA = 179,
	DIVIDE = 181,
	SYS_REQ = 183,
	RIGHT_MENU = 184,
	PAUSE = 197,
	HOME = 199,
	UP = 200,
	PRIOR = 201,
	LEFT = 203,
	RIGHT = 205,
	END = 207,
	DOWN = 208,
	NEXT = 209,
	INSERT = 210,
	DEL = 211,
	LEFT_WIN = 219,
	RIGHT_WIN = 220,
	APPS = 221,
	POWER = 222,
	SLEEP = 223,
	WAKE = 227,
	WEB_SEARCH = 229,
	WEB_FAVORITES = 230,
	WEB_REFRESH = 231,
	WEB_STOP = 232,
	WEB_FORWARD = 233,
	WEB_BACK = 234,
	MY_COMPUTER = 235,
	MAIL = 236,
	MEDIA_SELECT = 237,
	COUNT_OF = 238,
}; };
typedef KeyCode::Enum EKeyCode;

struct VirtualKey { enum Enum
{
	UNKNOWN = 0,
	PRIOR = 1,
	NEXT = 2,
	END = 3,
	HOME = 4,
	LEFT = 5,
	UP = 6,
	RIGHT = 7,
	DOWN = 8,
	SELECT = 9,
	BACK = 10,
	TAB = 11,
	CLEAR = 12,
	RETURN = 13,
	ESCAPE = 14,
	PRINT = 15,
	EXECUTE = 16,
	SNAPSHOT = 17,
	INSERT = 18,
	DEL = 19,
	HELP = 20,
	F1 = 21,
	F2 = 22,
	F3 = 23,
	F4 = 24,
	F5 = 25,
	F6 = 26,
	F7 = 27,
	F8 = 28,
	F9 = 29,
	F10 = 30,
	F11 = 31,
	F12 = 32,
	F13 = 33,
	F14 = 34,
	F15 = 35,
	F16 = 36,
	F17 = 37,
	F18 = 38,
	F19 = 39,
	F20 = 40,
	F21 = 41,
	F22 = 42,
	F23 = 43,
	F24 = 44,
	COUNT_OF,
}; };
typedef VirtualKey::Enum EVirtualKey;

} // namespace Common
} // namespace nvidia

#endif // NV_KEY_CODE_H