// logical.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

namespace cgn
{
	static CGResult doLogicalOr(CodegenState* cs, sst::BinaryOp* b)
	{
		// use a phi thing.
		auto cb = cs->irb.getCurrentBlock();


		auto pass = cs->irb.addNewBlockAfter("pass", cb);
		auto check = cs->irb.addNewBlockAfter("check", pass);
		auto merge = cs->irb.addNewBlockAfter("merge", check);

		// ok.
		// always generate the first thing.

		fir::Value* left = b->left->codegen(cs, fir::Type::getBool()).value;
		if(!left->getType()->isBoolType())
			error(b->left, "Non-boolean type '%s' cannot be used as a conditional", left->getType());

		// ok, compare first.
		fir::Value* cmpres = cs->irb.ICmpEQ(left, fir::ConstantBool::get(true));
		cs->irb.CondBranch(cmpres, pass, check);

		cs->irb.setCurrentBlock(check);
		{
			// ok, check the second
			fir::Value* right = b->right->codegen(cs, fir::Type::getBool()).value;
			if(!right->getType()->isBoolType())
				error(b->right, "Non-boolean type '%s' cannot be used as a conditional", right->getType());

			fir::Value* cmpres = cs->irb.ICmpEQ(right, fir::ConstantBool::get(true));
			cs->irb.CondBranch(cmpres, pass, merge);
		}

		cs->irb.setCurrentBlock(pass);
		{
			cs->irb.UnCondBranch(merge);
		}

		cs->irb.setCurrentBlock(merge);

		auto phi = cs->irb.CreatePHINode(fir::Type::getBool());
		phi->addIncoming(fir::ConstantBool::get(true), pass);
		phi->addIncoming(fir::ConstantBool::get(false), check);

		return CGResult(phi);
	}


	static CGResult doLogicalAnd(CodegenState* cs, sst::BinaryOp* b)
	{
		// use a phi thing.
		auto cb = cs->irb.getCurrentBlock();

		auto fail = cs->irb.addNewBlockAfter("fail", cb);
		auto check = cs->irb.addNewBlockAfter("check", fail);
		auto merge = cs->irb.addNewBlockAfter("merge", check);

		// ok.
		// always generate the first thing.

		fir::Value* left = b->left->codegen(cs, fir::Type::getBool()).value;
		if(!left->getType()->isBoolType())
			error(b->left, "Non-boolean type '%s' cannot be used as a conditional", left->getType());

		// ok, compare first.
		fir::Value* cmpres = cs->irb.ICmpEQ(left, fir::ConstantBool::get(true));
		cs->irb.CondBranch(cmpres, check, fail);

		cs->irb.setCurrentBlock(fail);
		{
			// break straight to merge
			cs->irb.UnCondBranch(merge);
		}

		cs->irb.setCurrentBlock(check);
		{
			// ok, check the second
			fir::Value* right = b->right->codegen(cs, fir::Type::getBool()).value;
			if(!right->getType()->isBoolType())
				error(b->right, "Non-boolean type '%s' cannot be used as a conditional", right->getType());

			fir::Value* cmpres = cs->irb.ICmpEQ(right, fir::ConstantBool::get(true));
			cs->irb.CondBranch(cmpres, merge, fail);
		}

		cs->irb.setCurrentBlock(merge);

		auto phi = cs->irb.CreatePHINode(fir::Type::getBool());
		phi->addIncoming(fir::ConstantBool::get(true), check);
		phi->addIncoming(fir::ConstantBool::get(false), fail);

		return CGResult(phi);
	}






	CGResult CodegenState::performLogicalBinaryOperation(sst::BinaryOp* bo)
	{
		if(bo->op == "&&")
			return doLogicalAnd(this, bo);

		else
			return doLogicalOr(this, bo);
	}

}


