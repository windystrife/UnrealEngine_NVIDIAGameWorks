HLSLCC - An HLSL Cross Compiler

This library compiles HLSL shader source code into a high-level IR, performs device-independent optimizations, and produces GLSL compatible source code. The library is largely based on the GLSL compiler from Mesa. The frontend has been heavily rewritten to parse HLSL and generate Mesa IR from the HLSL AST. The library leverages Mesa's IR optimization to simplify the code and finally generates GLSL source code from the Mesa IR. The GLSL generation is based on the work in glsl-optimizer.

The library has a single interface function: HlslCrossCompile. Some sample code:

	const char* HlslVertexSource =
		"float4x4 LocalToWorld;\n"
		"float4x4 WorldToClip;\n\n"
		"struct FVertexInput\n"
		"{\n"
		"\tfloat4 Position : POSITION;\n"
		"\tfloat4 TexCoords : TEXCOORD0;\n"
		"};\n\n"
		"struct FVertexOutput\n"
		"{\n"
		"\tfloat4 Position : SV_Position;\n"
		"\tfloat4 WorldPosition : WORLDPOS;\n"
		"\tfloat2 TexCoords : TEXCOORD0;\n"
		"};\n\n"
		"void Main(in FVertexInput In, out FVertexOutput Out)\n"
		"{\n"
		"\tOut.WorldPosition = mul(LocalToWorld, In.Position);\n"
		"\tOut.Position = mul(WorldToClip, Out.WorldPosition);\n"
		"\tOut.TexCoords = In.TexCoords;\n"
		"}\n\n";
	char* GlslVertexSource = 0;
	char* ErrorLog = 0;

	HlslCrossCompile(
		/*InSourceFilename=*/ "VertexShader.hlsl",
		/*InShaderSource=*/ HlslVertexSource,
		/*InEntryPoint=*/ "Main",
		/*InShaderFrequency=*/ HLSLCC_VertexShader,
		/*InFlags=*/ 0,
		/*OutShaderSource=*/ &GlslVertexSource,
		/*OutErrorLog=*/ &ErrorLog
		);

	printf("HLSL Shader Source --------------------------------------------------------------\n");
	printf(HlslVertexSource);
	printf("---------------------------------------------------------------------------------\n\n");
	if (ErrorLog)
	{
		printf("Error Log -----------------------------------------------------------------------\n");
		printf(ErrorLog);
		printf("---------------------------------------------------------------------------------\n\n");
		free(ErrorLog);
	}
	if (GlslVertexSource)
	{
		printf("GLSL Shader Source --------------------------------------------------------------\n");
		printf(GlslVertexSource);
		printf("---------------------------------------------------------------------------------\n\n");
		free(GlslVertexSource);
	}

And the output:

HLSL Shader Source --------------------------------------------------------------
float4x4 LocalToWorld;
float4x4 WorldToClip;

struct FVertexInput
{
	float4 Position : POSITION;
	float4 TexCoords : TEXCOORD0;
};

struct FVertexOutput
{
	float4 Position : SV_Position;
	float4 WorldPosition : WORLDPOS;
	float2 TexCoords : TEXCOORD0;
};

void Main(in FVertexInput In, out FVertexOutput Out)
{
	Out.WorldPosition = mul(LocalToWorld, In.Position);
	Out.Position = mul(WorldToClip, Out.WorldPosition);
	Out.TexCoords = In.TexCoords;
}

---------------------------------------------------------------------------------

Error Log -----------------------------------------------------------------------
VertexShader.hlsl(21): warning: implicit trunction from `vec4' to `vec2'
---------------------------------------------------------------------------------

GLSL Shader Source --------------------------------------------------------------
// Inputs: f4:in_POSITION,f4:in_TEXCOORD0
// Outputs: f4:var_WORLDPOS,f2:var_TEXCOORD0
// Uniforms: f4x4:WorldToClip,f4x4:LocalToWorld
uniform mat4 WorldToClip;
uniform mat4 LocalToWorld;
in vec4 in_POSITION;
in vec4 in_TEXCOORD0;
out vec4 var_WORLDPOS;
out vec2 var_TEXCOORD0;
void main()
{
	vec4 t0;
	t0.x = dot(LocalToWorld[0],in_POSITION);
	t0.y = dot(LocalToWorld[1],in_POSITION);
	t0.z = dot(LocalToWorld[2],in_POSITION);
	t0.w = dot(LocalToWorld[3],in_POSITION);
	vec4 t1;
	t1.x = dot(WorldToClip[0],t0);
	t1.y = dot(WorldToClip[1],t0);
	t1.z = dot(WorldToClip[2],t0);
	t1.w = dot(WorldToClip[3],t0);
	gl_Position.xyzw = t1;
	var_WORLDPOS.xyzw = t0;
	var_TEXCOORD0.xy = in_TEXCOORD0.xy;
}

---------------------------------------------------------------------------------

If attempting to modify this library, you will need FLEX and BISON to generate the necessary lexers and parsers. The batch file GenerateParsers.bat exists to automate the process. The versions I used are FLEX 2.5.35 and BISON 2.5 (Packaged as win_flex_bison-1.2.zip)

Binaries for windows are available here: http://sourceforge.net/projects/winflexbison/files/old_versions/1.0/

This library is licensed under the Unreal Engine License. This library is based on third party software: Mesa3D and GLSL Optimizer. Links and licenses for that software are reproduced below.

http://www.mesa3d.org/

The Mesa3D code upon which this library is based is covered by the following license:

Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


https://github.com/aras-p/glsl-optimizer

GLSL Optimizer is licensed according to the terms of the MIT license:

Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
Copyright (C) 2010-2011  Unity Technologies All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
