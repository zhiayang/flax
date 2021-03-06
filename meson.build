project('flax', ['c', 'cpp'])

the_compiler = meson.get_compiler('c')

# buildKind = get_option('buildtype')
# message('build type is ' + buildKind)

# libKind = 'DebugNoSyms'

if get_option('buildtype') == 'debug'
	libKind = 'DebugNoSyms'
else
	libKind = 'Release'
endif

mpir_root_dir = 'D:/Projects/lib/mpir'
mpfr_root_dir = 'D:/Projects/lib/mpfr'
llvm_root_dir = 'D:/Projects/lib/llvm'

mpir_hdr_dir = mpir_root_dir + '/' + libKind + '/include/'
mpfr_hdr_dir = mpfr_root_dir + '/' + libKind + '/include/'
llvm_hdr_dir = llvm_root_dir + '/' + libKind + '/include/'

mpir_lib_dir = mpir_root_dir + '/' + libKind + '/lib/'
mpfr_lib_dir = mpfr_root_dir + '/' + libKind + '/lib/'
llvm_lib_dir = llvm_root_dir + '/' + libKind + '/lib/'

if the_compiler.get_id() == 'msvc'
	add_project_arguments('/std:c++17', language: 'cpp')
	add_project_arguments('/permissive-', language: 'cpp')
	add_project_arguments('-D_SCL_SECURE_NO_WARNINGS', language: 'cpp')
 	add_project_arguments('-D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING', language: 'cpp')

	add_project_link_arguments('/machine:X64', language: 'cpp')
	# add_project_link_arguments('/incremental', language: 'cpp')
	add_project_link_arguments('/debug:fastlink', language: ['c', 'cpp'])
	add_project_link_arguments('/nodefaultlib:libcmt.lib', language: 'cpp')
	add_project_link_arguments('/nodefaultlib:libcmtd.lib', language: 'cpp')
else
	# add_project_arguments('--std=c++17', language: 'cpp')
	# add_project_arguments('--std=c11', language: 'c')

	error('You fool, use the makefile for non-windows builds')
endif


source_files = files([
	'source/main.cpp',

	'source/misc/mpool.cpp',
	'source/misc/allocator.cpp',
	'source/misc/identifier.cpp',
	'source/misc/destructors.cpp',

	'source/frontend/pts.cpp',
	'source/frontend/file.cpp',
	'source/frontend/lexer.cpp',
	'source/frontend/errors.cpp',
	'source/frontend/import.cpp',
	'source/frontend/frontend.cpp',
	'source/frontend/platform.cpp',
	'source/frontend/collector.cpp',
	'source/frontend/dependencies.cpp',

	'source/frontend/parser/misc.cpp',
	'source/frontend/parser/expr.cpp',
	'source/frontend/parser/type.cpp',
	'source/frontend/parser/literal.cpp',
	'source/frontend/parser/variable.cpp',
	'source/frontend/parser/function.cpp',
	'source/frontend/parser/toplevel.cpp',
	'source/frontend/parser/operators.cpp',
	'source/frontend/parser/controlflow.cpp',

	'source/backend/backend.cpp',
	'source/backend/x64AsmBackend.cpp',

	'source/backend/llvm/jit.cpp',
	'source/backend/llvm/linker.cpp',
	'source/backend/llvm/translator.cpp',

	'source/typecheck/call.cpp',
	'source/typecheck/type.cpp',
	'source/typecheck/using.cpp',
	'source/typecheck/slice.cpp',
	'source/typecheck/defer.cpp',
	'source/typecheck/loops.cpp',
	'source/typecheck/dotop.cpp',
	'source/typecheck/enums.cpp',
	'source/typecheck/alloc.cpp',
	'source/typecheck/assign.cpp',
	'source/typecheck/ranges.cpp',
	'source/typecheck/sizeof.cpp',
	'source/typecheck/unions.cpp',
	'source/typecheck/special.cpp',
	'source/typecheck/structs.cpp',
	'source/typecheck/classes.cpp',
	'source/typecheck/function.cpp',
	'source/typecheck/variable.cpp',
	'source/typecheck/literals.cpp',
	'source/typecheck/toplevel.cpp',
	'source/typecheck/subscript.cpp',
	'source/typecheck/operators.cpp',
	'source/typecheck/arithmetic.cpp',
	'source/typecheck/destructure.cpp',
	'source/typecheck/controlflow.cpp',
	'source/typecheck/typecheckstate.cpp',

	'source/typecheck/polymorph/misc.cpp',
	'source/typecheck/polymorph/driver.cpp',
	'source/typecheck/polymorph/solver.cpp',
	'source/typecheck/polymorph/transforms.cpp',
	'source/typecheck/polymorph/instantiator.cpp',

	'source/typecheck/resolver/misc.cpp',
	'source/typecheck/resolver/driver.cpp',
	'source/typecheck/resolver/resolver.cpp',

	'source/codegen/misc.cpp',
	'source/codegen/call.cpp',
	'source/codegen/loops.cpp',
	'source/codegen/slice.cpp',
	'source/codegen/alloc.cpp',
	'source/codegen/enums.cpp',
	'source/codegen/ranges.cpp',
	'source/codegen/sizeof.cpp',
	'source/codegen/assign.cpp',
	'source/codegen/dotops.cpp',
	'source/codegen/unions.cpp',
	'source/codegen/structs.cpp',
	'source/codegen/classes.cpp',
	'source/codegen/logical.cpp',
	'source/codegen/builtin.cpp',
	'source/codegen/variable.cpp',
	'source/codegen/function.cpp',
	'source/codegen/toplevel.cpp',
	'source/codegen/literals.cpp',
	'source/codegen/operators.cpp',
	'source/codegen/subscript.cpp',
	'source/codegen/arithmetic.cpp',
	'source/codegen/destructure.cpp',
	'source/codegen/refcounting.cpp',
	'source/codegen/controlflow.cpp',
	'source/codegen/constructor.cpp',
	'source/codegen/autocasting.cpp',
	'source/codegen/codegenstate.cpp',

	'source/codegen/glue/any.cpp',
	'source/codegen/glue/misc.cpp',
	'source/codegen/glue/arrays.cpp',
	'source/codegen/glue/strings.cpp',
	'source/codegen/glue/saa_common.cpp',

	'source/fir/ConstantValue.cpp',
	'source/fir/GlobalValue.cpp',
	'source/fir/Instruction.cpp',
	'source/fir/IRBuilder.cpp',
	'source/fir/Function.cpp',
	'source/fir/IRBlock.cpp',
	'source/fir/Module.cpp',
	'source/fir/Value.cpp',

	'source/fir/Types/DynamicArrayType.cpp',
	'source/fir/Types/ArraySliceType.cpp',
	'source/fir/Types/PrimitiveType.cpp',
	'source/fir/Types/FunctionType.cpp',
	'source/fir/Types/PointerType.cpp',
	'source/fir/Types/SingleTypes.cpp',
	'source/fir/Types/StructType.cpp',
	'source/fir/Types/ArrayType.cpp',
	'source/fir/Types/ClassType.cpp',
	'source/fir/Types/TupleType.cpp',
	'source/fir/Types/UnionType.cpp',
	'source/fir/Types/EnumType.cpp',
	'source/fir/Types/Type.cpp'
])

utf8rewind_dep = declare_dependency(include_directories: include_directories('external/utf8rewind/include'),
	sources: [
				'external/utf8rewind/source/utf8rewind.c',
				'external/utf8rewind/source/unicodedatabase.c',

				'external/utf8rewind/source/internal/seeking.c',
				'external/utf8rewind/source/internal/database.c',
				'external/utf8rewind/source/internal/streaming.c',
				'external/utf8rewind/source/internal/codepoint.c',
				'external/utf8rewind/source/internal/composition.c',
				'external/utf8rewind/source/internal/casemapping.c',
				'external/utf8rewind/source/internal/decomposition.c'
			]
)


tinyproclib_dep = declare_dependency(include_directories: include_directories('external/tinyprocesslib'),
	sources: [
				'external/tinyprocesslib/process.cpp',
				'external/tinyprocesslib/process_os.cpp'
			]
)



mpir_dep = declare_dependency(version: '3.0.0', include_directories: include_directories(mpir_hdr_dir),
	dependencies: the_compiler.find_library('mpir', dirs: mpir_lib_dir))

mpfr_dep = declare_dependency(version: '4.0.0', include_directories: include_directories(mpfr_hdr_dir),
	dependencies: the_compiler.find_library('mpfr', dirs: mpfr_lib_dir))

llvm_dep = declare_dependency(version: '6.0.0', include_directories: include_directories(llvm_hdr_dir),
	dependencies: [ mpfr_dep, mpir_dep,
		# the_compiler.find_library('LLVM_all', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMMC', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMipo', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMCore', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMMCJIT', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMOrcJIT', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMLinker', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMTarget', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMObject', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMPasses', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMX86Info', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMCodegen', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMX86Desc', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMSupport', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMMCParser', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMX86Utils', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMAnalysis', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMTablegen', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMVectorize', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMBitReader', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMLibDriver', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMBitWriter', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMAsmPrinter', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMGlobalISel', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMScalarOpts', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMX86CodeGen', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMProfileData', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMInstCombine', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMInterpreter', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMRuntimeDyld', dirs: llvm_lib_dir),
		# the_compiler.find_library('LLVMDebugInfoPDB', dirs: llvm_lib_dir),
		# the_compiler.find_library('LLVMDebugInfoMSF', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMSelectionDAG', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMBinaryFormat', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMX86AsmParser', dirs: llvm_lib_dir),
		# the_compiler.find_library('LLVMDlltoolDriver', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMX86AsmPrinter', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMTransformUtils', dirs: llvm_lib_dir),
		# the_compiler.find_library('LLVMDebugInfoDWARF', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMMCDisassembler', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMExecutionEngine', dirs: llvm_lib_dir),
		# the_compiler.find_library('LLVMInstrumentation', dirs: llvm_lib_dir),
		the_compiler.find_library('LLVMDebugInfoCodeView', dirs: llvm_lib_dir)
	]
)


include_dirs = include_directories([ 'source/include', 'external', mpir_hdr_dir, mpfr_hdr_dir, llvm_hdr_dir ])

executable('flaxc', source_files,
	include_directories: include_dirs,
	dependencies: [ llvm_dep, tinyproclib_dep, utf8rewind_dep, mpfr_dep, mpir_dep ]
)

	# cpp_pch: [ 'source/include/precompile.h', 'source/include/precompile.cpp' ]




