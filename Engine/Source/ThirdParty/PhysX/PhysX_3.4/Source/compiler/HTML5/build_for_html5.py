import subprocess, os, shutil, sys

# This script builds PhysX+Apex for HTML5 using Esmcripten.
# Usage:
#  1. Activate emsdk in the current command prompt
#  2. Run "python path/to/build_for_html5.py"

if not os.getenv('EMSCRIPTEN'):
	print >> sys.stderr, 'Environment variable EMSCRIPTEN not set! Please first install and activate emsdk in current cmd prompt.'
	sys.exit(1)

def bat_file(cmd):
	return (cmd + '.bat') if os.name == 'nt' else cmd

def run(cmd):
	print str(cmd)
	env=os.environ.copy()
	env['GW_DEPS_ROOT'] = src_directory.replace('\\', '/')
	env['CMAKE_MODULE_PATH'] = os.path.join(src_directory, 'Externals', 'CMakeModules').replace('\\', '/')
	proc = subprocess.Popen(cmd, env=env)
	proc.communicate()
	if proc.returncode != 0: raise Exception('Command failed!')

build_modes = [('', 'Debug'),
               ('-O2', 'Release'),
               ('-O3', 'Release'),
               ('-Oz', 'MinSizeRel')]

src_directory = os.path.normpath(os.path.join(os.path.realpath(os.path.dirname(__file__)), '..', '..', '..', '..'))

output_lib_directory = os.path.normpath(os.path.join(src_directory, 'Lib', 'HTML5'))
if 'rebuild' in sys.argv:
	try:
		for f in [os.path.join(output_lib_directory, f) for f in os.listdir(output_lib_directory) if f.endswith(".bc")]:
			print 'Clearing ' + f
			os.remove(f)
	except Exception, e:
		pass

cmake_src_directory = os.path.join(src_directory, 'PhysX_3.4', 'Source', 'compiler', 'cmake', 'html5')
print 'Build source directory: ' + cmake_src_directory

print 'Output libraries to directory: ' + output_lib_directory

for (mode, cmake_build_type) in build_modes:
	print 'Building ' + mode + ', ' + cmake_build_type + ':'

	# Create and move to the build directory for this configuration.
	build_dir = os.path.join(cmake_src_directory, 'html5_build' + mode.replace('-', '_'))
	print 'Artifacts directory for build: ' + build_dir
	if os.path.exists(build_dir) and 'rebuild' in sys.argv: shutil.rmtree(build_dir)
	if not os.path.exists(build_dir): os.mkdir(build_dir)
	os.chdir(build_dir)

	# C & C++ compiler flags to use for the build.
	compile_flags = mode + ' -Wno-double-promotion -Wno-comma -Wno-expansion-to-defined -Wno-undefined-func-template -Wno-disabled-macro-expansion -Wno-missing-noreturn '
	compile_flags += ' -D_DEBUG ' if cmake_build_type == 'Debug' else ' -DNDEBUG '
	compile_flags = ['-DCMAKE_C_FLAGS_' + cmake_build_type.upper() + '=' + compile_flags,
		'-DCMAKE_CXX_FLAGS_' + cmake_build_type.upper() + '=' + compile_flags]

# BUG: Would like to enable the following flags, but that gives missing symbols when linking:
#     "UATHelper: Packaging (HTML5): UnrealBuildTool: warning: unresolved symbol: _ZN5physx6shdfnd14NamedAllocatorD1Ev, _ZN5physx6shdfnd14NamedAllocatorD2Ev, _ZN5physx6shdfnd14NamedAllocatorC1EPKc, _ZN5physx6shdfnd14NamedAllocator10deallocateEPv, _ZN5physx6shdfnd14NamedAllocatorC2ERKS1_, _ZN5physx6shdfnd14NamedAllocator8allocateEjPKci"
# Not really sure why that occurs and whether it is an Emscripten or an UE4 issue, but for now, disable linker optimization flags (below).

#		'-DCMAKE_STATIC_LINKER_FLAGS_' + cmake_build_type.upper() + '=' + mode,
#		'-DCMAKE_SHARED_LINKER_FLAGS_' + cmake_build_type.upper() + '=' + mode,
#		'-DCMAKE_MODULE_LINKER_FLAGS_' + cmake_build_type.upper() + '=' + mode,
#		'-DCMAKE_EXE_LINKER_FLAGS_' + cmake_build_type.upper() + '=' + mode]

	# Configure the build via CMake
	run([os.path.join(os.getenv('EMSCRIPTEN'), bat_file('emcmake')), 'cmake',
		'-DAPEX_ENABLE_UE4=1', '-DTARGET_BUILD_PLATFORM=HTML5',
		'-DPHYSX_ROOT_DIR=' + os.path.join(src_directory, 'PhysX_3.4').replace('\\', '/'),
		'-DPXSHARED_ROOT_DIR=' + os.path.join(src_directory, 'PxShared').replace('\\', '/'),
		'-DNVSIMD_INCLUDE_DIR=' + os.path.join(src_directory, 'PxShared', 'src', 'NvSimd').replace('\\', '/'),
		'-DNVTOOLSEXT_INCLUDE_DIRS=' + os.path.join(src_directory, 'PhysX_3.4', 'externals', 'nvToolsExt', 'include').replace('\\', '/'),
		'-DEMSCRIPTEN_GENERATE_BITCODE_STATIC_LIBRARIES=ON', '-DCMAKE_BUILD_TYPE=' + cmake_build_type] + compile_flags + ['..'])

	# Run the build
	cmd = ['cmake', '--build', '.', '--', '-j']
	if 'verbose' in sys.argv: cmd += ['VERBOSE=1']
	run(cmd)

	# Deploy the output to the libraries directory.
	output_lib_directories = [build_dir, os.path.join(build_dir, 'pxshared_bin')]
	for d in output_lib_directories:
		for output_file in os.listdir(d):
			if output_file.endswith('.bc'):
				deployed_output_file = os.path.join(output_lib_directory, output_file.replace('.bc', mode.replace('-', '_') + '.bc'))
				print 'Deploying ' + output_file + ' to ' + deployed_output_file
				shutil.copyfile(os.path.join(d, output_file), deployed_output_file)
