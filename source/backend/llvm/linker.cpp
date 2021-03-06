// linker.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.



#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif


#include <fstream>

#ifdef _MSC_VER
	#pragma warning(push, 0)
#endif

#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Host.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ir/type.h"
#include "ir/value.h"
#include "ir/module.h"
#include "ir/irbuilder.h"

#include "frontend.h"
#include "backends/llvm.h"

#include "tinyprocesslib/process.h"


static llvm::LLVMContext globalContext;

template <typename T>
static void _printTiming(T ts, const std::string& thing)
{
	auto dur = std::chrono::high_resolution_clock::now() - ts;
	auto ms = (double) dur.count() / 1000.0 / 1000.0;
	fprintf(stderr, "%s took %.1f ms%s\n", thing.c_str(), ms, ms > 3000 ? strprintf("  (aka %.2f s)", ms / 1000.0).c_str() : "");
}

namespace backend
{
	llvm::LLVMContext& LLVMBackend::getLLVMContext()
	{
		return globalContext;
	}

	LLVMBackend::LLVMBackend(CompiledData& dat, std::vector<std::string> inputs, std::string output) : Backend(BackendCaps::EmitAssembly | BackendCaps::EmitObject | BackendCaps::EmitProgram | BackendCaps::JIT, dat, inputs, output)
	{
		if(inputs.size() != 1)
			error("Need exactly 1 input filename, have %zu", inputs.size());
	}

	std::string LLVMBackend::str()
	{
		return "LLVM";
	}

	void LLVMBackend::performCompilation()
	{
		auto ts = std::chrono::high_resolution_clock::now();

		llvm::InitializeNativeTarget();

		// fprintf(stderr, "%s\n\n\n", this->compiledData.module->print().c_str());

		auto mainModule = this->translateFIRtoLLVM(this->compiledData.module);
		auto s = frontend::getFilenameFromPath(this->inputFilenames[0]);

		if(this->compiledData.module->getEntryFunction())
			this->entryFunction = mainModule->getFunction(this->compiledData.module->getEntryFunction()->getName().mangled());


		this->linkedModule = std::shared_ptr<llvm::Module>(mainModule);
		this->finaliseGlobalConstructors();

		// ok, move some shit into here because llvm is fucking retarded
		this->setupTargetMachine();
		this->linkedModule->setDataLayout(this->targetMachine->createDataLayout());

		if(frontend::getPrintLLVMIR())
			this->linkedModule->print(llvm::outs(), 0);

		_printTiming(ts, "translation to llvm");
	}


	void LLVMBackend::optimiseProgram()
	{
		auto ts = std::chrono::high_resolution_clock::now();


		if(llvm::verifyModule(*this->linkedModule, &llvm::errs()))
		{
			fprintf(stderr, "\n\n");
			this->linkedModule->print(llvm::errs(), 0);

			BareError::make("\nLLVM Module verification failed\n")->postAndQuit();
		}

		llvm::legacy::PassManager fpm = llvm::legacy::PassManager();

		fpm.add(llvm::createDeadInstEliminationPass());
		fpm.add(llvm::createDeadCodeEliminationPass());

		if(frontend::getOptLevel() > OptimisationLevel::Debug)
		{
			// mem2reg is based, because it changes inefficient load-store branches into more efficient phi-nodes (at least more efficient
			// in terms of optimisation potential)
			fpm.add(llvm::createPromoteMemoryToRegisterPass());
			fpm.add(llvm::createMergedLoadStoreMotionPass());
			fpm.add(llvm::createInstructionCombiningPass());
			fpm.add(llvm::createConstantPropagationPass());
			fpm.add(llvm::createScalarizerPass());
		}

		if(frontend::getOptLevel() > OptimisationLevel::None)
		{
			fpm.add(llvm::createReassociatePass());
			fpm.add(llvm::createCFGSimplificationPass());

			// hmm.
			// fuck it, turn everything on.

			fpm.add(llvm::createConstantHoistingPass());
			fpm.add(llvm::createLICMPass());
			fpm.add(llvm::createDelinearizationPass());
			fpm.add(llvm::createFlattenCFGPass());
			fpm.add(llvm::createScalarizerPass());
			fpm.add(llvm::createSinkingPass());
			fpm.add(llvm::createInstructionSimplifierPass());
			fpm.add(llvm::createDeadStoreEliminationPass());
			fpm.add(llvm::createMemCpyOptPass());

			fpm.add(llvm::createSCCPPass());

			fpm.add(llvm::createTailCallEliminationPass());
		}

		if(frontend::getOptLevel() > OptimisationLevel::Minimal)
		{
			fpm.add(llvm::createAggressiveDCEPass());

			// module-level stuff
			fpm.add(llvm::createMergeFunctionsPass());
			fpm.add(llvm::createLoopSimplifyPass());
		}

		fpm.run(*this->linkedModule);

		_printTiming(ts, "llvm opt");
	}

	void LLVMBackend::writeOutput()
	{
		auto ts = std::chrono::high_resolution_clock::now();

		// verify the module.
		{
			if(llvm::verifyModule(*this->linkedModule, &llvm::errs()))
				error("\n\nLLVM Module verification failed");
		}

		std::string foldername;
		size_t sep = this->inputFilenames[0].find_last_of("\\/");
		if(sep != std::string::npos)
			foldername = this->inputFilenames[0].substr(0, sep);

		bool wasEmpty = this->outputFilename.empty();
		std::string oname = this->outputFilename.empty() ? (foldername + "/" + this->linkedModule->getModuleIdentifier())
			: this->outputFilename;

		if(frontend::getOutputMode() == ProgOutputMode::RunJit)
		{
			const char* argv = ("__JIT-" + this->linkedModule->getModuleIdentifier()).c_str();
			auto entry = this->getEntryFunctionFromJIT();

			_printTiming(ts, "llvm jit");
			fprintf(stderr, "\n");

			iceAssert(this->jitInstance);
			entry(1, &argv);

			delete this->jitInstance;
		}
		else if(frontend::getOutputMode() == ProgOutputMode::LLVMBitcode)
		{
			std::error_code e;
			llvm::sys::fs::OpenFlags of = (llvm::sys::fs::OpenFlags) 0;
			llvm::raw_fd_ostream rso(oname.c_str(), e, of);

			llvm::WriteBitcodeToFile(this->linkedModule.get(), rso);
			rso.close();

			_printTiming(ts, "writing bitcode file");
		}
		else
		{
			if(frontend::getOutputMode() != ProgOutputMode::ObjectFile && !this->compiledData.module->getEntryFunction())
			{
				error("No entry function marked, a program cannot be compiled");
			}

			auto buffer = this->initialiseLLVMStuff();

			if(frontend::getOutputMode() == ProgOutputMode::ObjectFile)
			{
				// now memoryBuffer should contain the .object file
				std::ofstream objectOutput(oname + (wasEmpty ? ".o" : ""), std::ios::binary | std::ios::out);
				objectOutput.write(buffer.data(), buffer.size_in_bytes());
				objectOutput.close();
			}
			else
			{
				std::string objname = platform::getTemporaryFilename(this->linkedModule->getModuleIdentifier());

				auto fd = platform::openFile(objname.c_str(), O_RDWR | O_CREAT, 0);
				if(fd == platform::InvalidFileHandle)
				{
					perror("open(2) error");
					BareError::make("Unable to create temporary file ('%s') for linking", objname)->postAndQuit();
				}

				platform::writeFile(fd, buffer.data(), buffer.size_in_bytes());
				platform::closeFile(fd);


				auto libs = frontend::getLibrariesToLink();
				auto libdirs = frontend::getLibrarySearchPaths();

				auto frames = frontend::getFrameworksToLink();
				auto framedirs = frontend::getFrameworkSearchPaths();


				// here, if we're doing a link, and we're not in freestanding mode, then we're going to add -lc and -lm
				if(!frontend::getIsFreestanding())
				{
					if(std::find(libs.begin(), libs.end(), "m") == libs.end())
						libs.push_back("m");

					if(std::find(libs.begin(), libs.end(), "c") == libs.end())
						libs.push_back("c");
				}


				size_t num_extra = 0;
				size_t s = 5 + num_extra + (2 * libs.size()) + (2 * libdirs.size()) + (2 * frames.size()) + (2 * framedirs.size());
				const char** argv = new const char*[s];
				memset(argv, 0, s * sizeof(const char*));

				argv[0] = "cc";
				argv[1] = "-o";
				argv[2] = oname.c_str();
				argv[3] = objname.c_str();

				size_t i = 4 + num_extra;


				// note: these need to be references
				// if they're not, then the std::string (along with its buffer) is destructed at the end of the loop body
				// so the pointer in argv[i] becomes invalid
				// thus we need to make sure the pointed thing is valid until we call execvp; the frames/libs/blabla deques up there
				// will live for the required duration, so we use a reference.

				for(auto& F : framedirs)
				{
					argv[i] = "-F";			i++;
					argv[i] = F.c_str();	i++;
				}

				for(auto& f : frames)
				{
					argv[i] = "-framework";	i++;
					argv[i] = f.c_str();	i++;
				}

				for(auto& L : libdirs)
				{
					argv[i] = "-L";			i++;
					argv[i] = L.c_str();	i++;
				}

				for(auto& l : libs)
				{
					argv[i] = "-l";			i++;
					argv[i] = l.c_str();	i++;
				}

				argv[s - 1] = 0;

				std::string output;
				int status = 0;


				std::string cmdline;
				for(size_t i = 0; i < s - 1; i++)
				{
					cmdline += argv[i];

					if(strcmp(argv[i], "-l") != 0 && strcmp(argv[i], "-L") != 0)
						cmdline += " ";
				}

				tinyproclib::Process proc(cmdline, "", [&output](const char* bytes, size_t n) {
					output = std::string(bytes, n);
				});


				delete[] argv;

				if(status != 0 || output.size() != 0)
				{
					fprintf(stderr, "%s\n", output.c_str());
					fprintf(stderr, "linker returned non-zero (status = %d), exiting\n", status);
					fprintf(stderr, "cmdline was: %s\n", cmdline.c_str());
					exit(status);
				}
			}

			_printTiming(ts, "outputting exe/obj");
		}
	}










	void LLVMBackend::setupTargetMachine()
	{
		#if 1
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmParser();
		llvm::InitializeNativeTargetAsmPrinter();

		llvm::Triple targetTriple;
		targetTriple.setTriple(frontend::getParameter("targetarch").empty() ? llvm::sys::getProcessTriple()
			: frontend::getParameter("targetarch"));



		std::string err_str;
		const llvm::Target* theTarget = llvm::TargetRegistry::lookupTarget("", targetTriple, err_str);
		if(!theTarget)
		{
			error("failed in creating target: (wanted: '%s'); llvm error: %s\n", targetTriple.str(), err_str);
		}


		// get the mcmodel
		llvm::CodeModel::Model codeModel;
		if(frontend::getParameter("mcmodel") == "kernel")
		{
			codeModel = llvm::CodeModel::Kernel;
		}
		else if(frontend::getParameter("mcmodel") == "small")
		{
			codeModel = llvm::CodeModel::Small;
		}
		else if(frontend::getParameter("mcmodel") == "medium")
		{
			codeModel = llvm::CodeModel::Medium;
		}
		else if(frontend::getParameter("mcmodel") == "large")
		{
			codeModel = llvm::CodeModel::Large;
		}
		else if(frontend::getParameter("mcmodel").empty())
		{
			codeModel = llvm::CodeModel::Large;
		}
		else
		{
			error("Invalid mcmodel '%s' (valid options: kernel, small, medium, or large)", frontend::getParameter("mcmodel"));
		}


		llvm::TargetOptions targetOptions;
		llvm::Reloc::Model relocModel = llvm::Reloc::Model::Static;

		// todo: use dynamic no pic for dylibs?? idk
		if(frontend::getIsPositionIndependent())
			relocModel = llvm::Reloc::Model::PIC_;

		this->targetMachine = theTarget->createTargetMachine(targetTriple.getTriple(), "", "",
			targetOptions, relocModel, codeModel, llvm::CodeGenOpt::Default);

		#else

		this->targetMachine = llvm::EngineBuilder().selectTarget();

		#endif
	}





	llvm::SmallVector<char, 0> LLVMBackend::initialiseLLVMStuff()
	{
		llvm::SmallVector<char, 0> memoryBuffer;
		auto bufferStream = llvm::make_unique<llvm::raw_svector_ostream>(memoryBuffer);
		llvm::raw_pwrite_stream* rawStream = bufferStream.get();

		{
			// auto p = prof::Profile(PROFGROUP_LLVM, "llvm_emit_object");
			llvm::legacy::PassManager pm = llvm::legacy::PassManager();
			targetMachine->addPassesToEmitFile(pm, *rawStream, llvm::TargetMachine::CodeGenFileType::CGFT_ObjectFile);
			pm.run(*this->linkedModule);
		}

		// flush and kill it.
		rawStream->flush();

		return memoryBuffer;
	}


















	EntryPoint_t LLVMBackend::getEntryFunctionFromJIT()
	{
		// all linked already.
		// dump here, before the output.


		// if JIT-ing, we need to load all the framework shit.
		// note(compat): i think -L -l ordering matters when resolving libraries
		// we don't support that right now.
		// now, just add all the paths immediately.

		std::string penv;
		std::string pfenv;

		char* e = std::getenv("LD_LIBRARY_PATH");

		std::string env;
		if(e) env = std::string(e);

		penv = env;

		if(!env.empty() && env.back() != ':')
			env += ":";

		// add all the paths.
		{
			for(auto L : frontend::getLibrarySearchPaths())
				env += L + ":";

			if(!env.empty() && env.back() == ':')
				env.pop_back();
		}



		std::string fenv;

		char* fe = std::getenv("DYLD_FRAMEWORK_PATH");
		if(fe) fenv = std::string(fe);

		pfenv = fenv;

		if(!fenv.empty() && fenv.back() != ':')
			fenv += ":";

		// add framework paths
		{
			for(auto F : frontend::getFrameworkSearchPaths())
				fenv += F + ":";

			if(!fenv.empty() && fenv.back() == ':')
				fenv.pop_back();
		}


		// set the things

		#ifndef _WIN32
		{
			setenv("LD_LIBRARY_PATH", env.c_str(), 1);
			setenv("DYLD_FRAMEWORK_PATH", fenv.c_str(), 1);
		}
		#else
		{
			_putenv_s("LD_LIBRARY_PATH", env.c_str());
			_putenv_s("DYLD_FRAMEWORK_PATH", fenv.c_str());
		}
		#endif




		auto tolink = frontend::getLibrariesToLink();

		// note: linux is stupid. to be safe, explicitly link libc and libm
		// note: will not affect freestanding implementations, since this is JIT mode
		// note2: the stupidity of linux extends further than i thought
		// apparently we cannot dlopen "libc.so", because that's not even a fucking ELF library.
		// note3: wow, this applies to libm as well.
		// fuck you torvalds
		// so basically just do nothing.

		for(auto l : tolink)
		{
			std::string ext;

			#if defined(__MACH__)
				ext = ".dylib";
			#elif defined(WIN32)
				ext = ".dll";
			#else
				ext = ".so";
			#endif

			std::string err;
			llvm::sys::DynamicLibrary dl = llvm::sys::DynamicLibrary::getPermanentLibrary(("lib" + l + ext).c_str(), &err);
			if(!dl.isValid())
				error("Failed to load library '%s', dlopen failed with error:\n%s", l, err);
		}


		for(auto l : frontend::getFrameworksToLink())
		{
			std::string name = l + ".framework/" + l;

			std::string err;
			llvm::sys::DynamicLibrary dl = llvm::sys::DynamicLibrary::getPermanentLibrary(name.c_str(), &err);
			if(!dl.isValid())
				error("Failed to load framework '%s', dlopen failed with error:\n%s", l, err);
		}

		EntryPoint_t ret = 0;
		if(this->entryFunction)
		{
			#if 1

			this->jitInstance = new LLVMJit(this->targetMachine);
			this->jitInstance->addModule(this->linkedModule);

			auto name = this->entryFunction->getName().str();
			auto entryaddr = this->jitInstance->getSymbolAddress(name);
			ret = (int (*)(int, const char**)) entryaddr;

			iceAssert(ret && "failed to resolve entry function address");

			#else
			llvm::ExecutionEngine* execEngine = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(this->linkedModule.get()))
				.create(this->targetMachine);

			// finalise the object, which does something.
			iceAssert(execEngine);
			execEngine->finalizeObject();

			void* func = execEngine->getPointerToFunction(this->entryFunction);
			iceAssert(func != 0);

			ret = (int (*)(int, const char**)) func;
			#endif
		}
		else
		{
			error("No entry function marked, cannot JIT");
		}


		// restore

		#ifndef _WIN32
		{
			setenv("LD_LIBRARY_PATH", penv.c_str(), 1);
			setenv("DYLD_FRAMEWORK_PATH", pfenv.c_str(), 1);
		}
		#else
		{
			_putenv_s("LD_LIBRARY_PATH", penv.c_str());
			_putenv_s("DYLD_FRAMEWORK_PATH", pfenv.c_str());
		}
		#endif

		// ret(1, (const char**) &"JIT");
		return ret;
	}















	void LLVMBackend::finaliseGlobalConstructors()
	{
		// first, if our entry function is named 'main', then all is well
		// if not, then we need to make our own main (checking for conflicts) and call the real entry
		// function there.

		// this->linkedModule->print(llvm::errs(), 0);


		llvm::IRBuilder<> builder(LLVMBackend::getLLVMContext());

		auto entryfunc = this->entryFunction;
		if(!entryfunc)
		{
			entryfunc = this->linkedModule->getFunction("main");
			if(entryfunc)
			{
				warn("No entry point marked with '@entry', defaulting to 'main'");
				this->entryFunction = entryfunc;
			}
			else
			{
				error("No entry point marked with '@entry', and no 'main' function; cannot compile program");
			}
		}

		if(entryfunc->getName() != "main")
		{
			// well.
			if(this->linkedModule->getFunction("main") != 0)
			{
				error("Conflicting 'main' function; entry function was '%s', but 'main' must be undefined in order to allow trampoline code to work (blame the linker)");
			}

			// ok.

			auto& c = LLVMBackend::getLLVMContext();
			llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(c),
				{ llvm::Type::getInt32Ty(c), llvm::Type::getInt8Ty(c)->getPointerTo()->getPointerTo() }, false);

			llvm::Function* mainf = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage, "main", this->linkedModule.get());
			llvm::BasicBlock* entry = llvm::BasicBlock::Create(c, "main_entry", mainf);
			builder.SetInsertPoint(entry);

			auto argc = mainf->arg_begin();
			auto argv = mainf->arg_begin() + 1;

			builder.SetInsertPoint(entry);
			llvm::Value* returnVal = 0;
			if(entryfunc->getFunctionType()->params() == ft->params())
			{
				// ok, can pass arguments
				returnVal = builder.CreateCall(entryfunc->getFunctionType(), entryfunc, { argc, argv });
			}
			else
			{
				returnVal = builder.CreateCall(entryfunc->getFunctionType(), entryfunc, { });
			}


			// ok, check return type
			iceAssert(returnVal);
			{
				if(entryfunc->getReturnType()->isIntegerTy())
				{
					returnVal = builder.CreateIntCast(returnVal, llvm::Type::getInt32Ty(c), true);
					builder.CreateRet(returnVal);
				}
				else
				{
					builder.CreateRet(llvm::ConstantInt::getSigned(llvm::Type::getInt32Ty(c), 0));
				}
			}
		}

		// return;
		{
			// insert a call at the beginning of main().
			if((frontend::getOutputMode() == ProgOutputMode::Program || frontend::getOutputMode() == ProgOutputMode::RunJit) && !entryfunc)
				error("No entry function defined");


			llvm::BasicBlock* entryblock = &entryfunc->getEntryBlock();
			llvm::BasicBlock* f = llvm::BasicBlock::Create(LLVMBackend::getLLVMContext(), "__entry_entry", entryfunc);

			f->moveBefore(entryblock);

			builder.SetInsertPoint(f);
			builder.CreateCall(this->linkedModule->getFunction("__global_init_function__"));

			// auto x = builder.CreateAlloca(llvm::Type::getInt64Ty(LLVMBackend::getLLVMContext()), nullptr, "x");
			// builder.CreateStore(llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(LLVMBackend::getLLVMContext()), 491), x, true);
			builder.CreateBr(entryblock);
		}
	}
}


















































































