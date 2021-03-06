// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "pts.h"
#include "parser_internal.h"

#include "mpool.h"

using namespace ast;
using namespace lexer;

using TT = lexer::TokenType;
namespace parser
{
	// declared in parser/operators.cpp (because we use it there)
	std::tuple<std::vector<FuncDefn::Arg>, std::unordered_map<std::string, TypeConstraints_t>, pts::Type*, bool, Location> parseFunctionLookingDecl(State& st)
	{
		pts::Type* returnType = 0;
		std::vector<FuncDefn::Arg> args;
		std::unordered_map<std::string, TypeConstraints_t> generics;

		// check for generic function
		if(st.front() == TT::LAngle)
		{
			st.eat();
			// parse generic
			if(st.front() == TT::RAngle)
				error(st, "empty type parameter lists are not allowed");

			generics = parseGenericTypeList(st);
		}

		if(st.front() != TT::LParen)
			expectedAfter(st, "'('", "function declaration to begin argument list", st.front().str());

		st.eat();
		bool isvar = false;
		Location varloc;
		while(st.front() != TT::RParen)
		{
			if(isvar)
				error(st, "variadic arguments must be the last in the function parameter list");

			if(st.front() == TT::Ellipsis)
			{
				isvar = true;
				varloc = st.loc();
				st.pop();

				continue;
			}

			if(st.front() != TT::Identifier)
				expected(st, "identifier in function parameter list", st.front().str());

			std::string name = st.front().str();
			auto loc = st.loc();

			st.eat();

			if(st.front() != TT::Colon)
				expected(st, "':' after identifier to specify type", st.front().str());

			st.eat();
			auto type = parseType(st);

			args.push_back(FuncDefn::Arg { name, loc, type });

			if(st.front() == TT::Comma)
				st.eat();

			else if(st.front() != TT::RParen)
				expected(st, "')' or ',' in function parameter list", st.front().str());

			st.skipWS();
		}

		iceAssert(st.front() == TT::RParen);
		st.eat();

		if(st.front() == TT::RightArrow)
		{
			st.eat();
			returnType = parseType(st);
		}
		else
		{
			returnType = 0;
		}

		return std::make_tuple(args, generics, returnType, isvar, varloc);
	}


	static std::tuple<FuncDefn*, bool, Location> parseFunctionDecl(State& st)
	{
		iceAssert(st.front() == TT::Func);
		st.eat();

		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'fn'", st.front().str());

		FuncDefn* defn = util::pool<FuncDefn>(st.loc());
		defn->name = st.eat().str();

		Location loc;
		bool isvar = false;
		std::tie(defn->args, defn->generics, defn->returnType, isvar, loc) = parseFunctionLookingDecl(st);

		if(defn->returnType == 0)
			defn->returnType = pts::NamedType::create(VOID_TYPE_STRING);

		return std::make_tuple(defn, isvar, loc);
	}




	FuncDefn* parseFunction(State& st)
	{
		auto [ defn, isvar, varloc ] = parseFunctionDecl(st);
		if(isvar)
			error(varloc, "C-style variadic arguments are not supported on non-foreign functions");

		st.skipWS();
		if(st.front() != TT::LBrace && st.front() != TT::FatRightArrow)
			expected(st, "'{' to begin function body", st.front().str());

		st.enterFunctionBody();
		{
			defn->body = parseBracedBlock(st);
		}
		st.leaveFunctionBody();

		return defn;
	}





	ForeignFuncDefn* parseForeignFunction(State& st)
	{
		iceAssert(st.front() == TT::ForeignFunc);
		st.pop();

		if(st.front() != TT::Func)
			expectedAfter(st, "'fn'", "'ffi'", st.front().str());

		auto ffn = util::pool<ForeignFuncDefn>(st.loc());

		// copy the things over
		auto [ defn, isvar, varloc ] = parseFunctionDecl(st);
		(void) varloc;

		if(!defn->generics.empty())
			error(ffn->loc, "foreign functions cannot be generic");

		ffn->loc = defn->loc;
		ffn->isVarArg = isvar;
		ffn->args = defn->args;
		ffn->name = defn->name;
		ffn->visibility = defn->visibility;
		ffn->returnType = defn->returnType;

		return ffn;
	}


	InitFunctionDefn* parseInitFunction(State& st)
	{
		Token tok = st.pop();
		iceAssert(tok.str() == "init");

		auto [ args, generics, retty, isvar, varloc ] = parseFunctionLookingDecl(st);
		if(generics.size() > 0)
			error(st.ploc(), "class initialiser functions cannot be generic");

		else if(retty != 0)
			error(st.ploc(), "class initialisers cannot have a return type");

		else if(isvar)
			error(varloc, "C-style variadic arguments are not supported on non-foreign functions");

		// ok loh
		InitFunctionDefn* ret = util::pool<InitFunctionDefn>(tok.loc);
		ret->args = args;

		// check for super-class args.
		if(st.front() == TT::Colon)
		{
			st.eat();
			if(st.eat().str() != "super")
				expectedAfter(st.ploc(), "'super'", "':' in init function definition", st.prev().str());

			if(st.eat() != TT::LParen)
				expectedAfter(st.ploc(), "'('", "'super' in call to base-class initialiser", st.prev().str());

			ret->superArgs = parseCallArgumentList(st);
			ret->didCallSuper = true;
		}

		st.enterFunctionBody();
		{
			ret->body = parseBracedBlock(st);
		}
		st.leaveFunctionBody();

		return ret;
	}











	std::unordered_map<std::string, TypeConstraints_t> parseGenericTypeList(State& st)
	{
		std::unordered_map<std::string, TypeConstraints_t> ret;

		while(st.front().type != TT::RAngle)
		{
			if(st.front().type == TT::Identifier)
			{
				std::string gt = st.eat().str();
				TypeConstraints_t constrs;

				if(st.front().type == TT::Colon)
				{
					st.eat();
					if(st.front().type != TT::Identifier)
						error(st, "expected identifier after beginning of type constraint list");

					while(st.front().type == TT::Identifier)
					{
						constrs.protocols.push_back(st.eat().str());

						if(st.front().type == TT::Ampersand)
						{
							st.eat();
						}
						else if(st.front().type != TT::Comma && st.front().type != TT::RAngle)
						{
							error(st, "expected ',' or '>' to end type parameter list (1)");
						}
					}
				}
				else if(st.front().type != TT::Comma && st.front().type != TT::RAngle)
				{
					error(st, "expected ',' or '>' to end type parameter list (2)");
				}

				ret[gt] = constrs;
			}
			else if(st.front().type == TT::Comma)
			{
				st.eat();
			}
			else if(st.front().type != TT::RAngle)
			{
				error(st, "expected '>' to end type parameter list");
			}
		}

		iceAssert(st.front().type == TT::RAngle);
		st.eat();

		return ret;
	}













	Block* parseBracedBlock(State& st)
	{
		st.skipWS();

		if(st.front() == TT::LBrace)
		{
			Block* ret = util::pool<Block>(st.eat().loc);

			st.skipWS();
			while(st.front() != TT::RBrace)
			{
				auto stmt = parseStmt(st);
				if(auto defer = dynamic_cast<DeferredStmt*>(stmt))
					ret->deferredStatements.push_back(defer);

				else
					ret->statements.push_back(stmt);


				if(st.front() == TT::NewLine || st.front() == TT::Comment || st.front() == TT::Semicolon)
					st.pop();

				else if(st.frontAfterWS() == TT::RBrace)
					break;

				else
					expected(st, "newline or semicolon to terminate a statement", st.front().str());

				st.skipWS();
			}

			auto closing = st.eat();
			iceAssert(closing == TT::RBrace);
			ret->closingBrace = closing.loc;
			ret->isArrow = false;

			return ret;
		}
		else if(st.front() == TT::FatRightArrow)
		{
			Block* ret = util::pool<Block>(st.eat().loc);
			ret->statements.push_back(parseStmt(st));
			ret->closingBrace = st.loc();
			ret->isArrow = true;

			return ret;
		}
		else
		{
			expected(st, "'{' to begin braced block", st.front().str());
		}
	}
}







