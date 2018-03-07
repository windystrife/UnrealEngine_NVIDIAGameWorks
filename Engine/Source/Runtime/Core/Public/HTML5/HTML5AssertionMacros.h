// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// scratch pad for HTML5 assertion macros macros.

extern "C" {
void emscripten_log(int flags, ...);
}

#if DO_CHECK

// For the asm.js builds, use emscripten-specific versions of these macros

#undef checkCode
#undef check
#undef checkf
#undef checkNoEntry
#undef checkNoReentry
#undef checkNoRecursion
#undef verify
#undef verifyf

#define checkCode(...)
#define checkNoEntry(...)
#define checkNoReentry(...)
#define checkNoRecursion(...)

inline void html5_break_msg(const char* msg, const char* file, int line) {
	EM_ASM_ARGS(
	{
		var InMsg = Pointer_stringify($0);
		var InFile = Pointer_stringify($1);
		alert('Expression ('+InMsg+') failed in '+InFile+':'+$2+'!\nCheck console for details.\n');
		var callstack = new Error;
		throw callstack.stack;
	}, msg, file, line);
}

#define check(expr)			{ if (!(expr)) { \
		emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
}
#define checkf(expr, ...)	{ if (!(expr)) { \
		emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); \
		emscripten_log(255, ##__VA_ARGS__); \
		FDebug::AssertFailed( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
	CA_ASSUME(expr); \
}
#define verify(expr)		{ if(!(expr)) {\
		emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
}
#define verifyf(expr, ...)	{ if(!(expr)) { \
		emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); \
		emscripten_log(255, ##__VA_ARGS__); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
}

#endif

#if DO_GUARD_SLOW

#undef checkSlow
#undef checkfSlow
#undef verifySlow

#define checkSlow(expr, ...)   {if(!(expr)) { \
		emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); \
		emscripten_log(255, ##__VA_ARGS__); FDebug::AssertFailed( #expr, __FILE__, __LINE__ ); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
	CA_ASSUME(expr); \
}
#define checkfSlow(expr, ...)	{ if(!(expr)) { \
		emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); \
		emscripten_log(255, ##__VA_ARGS__); FDebug::AssertFailed( #expr, __FILE__, __LINE__, __VA_ARGS__ ); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
	CA_ASSUME(expr); \
}
#define verifySlow(expr)  {if(!(expr)) { \
		emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); \
		FDebug::AssertFailed( #expr, __FILE__, __LINE__ ); \
		html5_break_msg(#expr, __FILE__, __LINE__); \
	} \
}

#endif
