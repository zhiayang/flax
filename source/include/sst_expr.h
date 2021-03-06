// sst_expr.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"

namespace cgn
{
	struct CodegenState;
}

namespace sst
{
	struct Stmt : Locatable
	{
		Stmt(const Location& l) : Locatable(l, "statement") { }
		virtual ~Stmt() { }

		virtual CGResult codegen(cgn::CodegenState* cs, fir::Type* inferred = 0)
		{
			if(didCodegen)
			{
				return cachedResult;
			}
			else
			{
				this->didCodegen = true;
				return (this->cachedResult = this->_codegen(cs, inferred));
			}
		}

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) = 0;

		bool didCodegen = false;
		CGResult cachedResult = CGResult(0);
	};

	struct Expr : Stmt
	{
		Expr(const Location& l, fir::Type* t) : Stmt(l), type(t) { this->readableName = "expression"; }
		~Expr() { }

		fir::Type* type = 0;
	};


	struct Defn : Stmt
	{
		Defn(const Location& l) : Stmt(l) { this->readableName = "definition"; }
		~Defn() { }

		Identifier id;
		fir::Type* type = 0;
		bool global = false;
		VisibilityLevel visibility = VisibilityLevel::Internal;

		virtual std::string getKind() = 0;
	};
}



