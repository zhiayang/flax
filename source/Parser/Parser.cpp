// Parser.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <deque>
#include <cfloat>
#include <fstream>
#include <cassert>
#include "../include/ast.h"
#include "../include/parser.h"

using namespace Ast;

namespace Parser
{
	PosInfo pos;
	Root* rootNode;
	Token* curtok;

	void error(const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		char* alloc = nullptr;
		vasprintf(&alloc, msg, ap);

		fprintf(stderr, "Error (%s:%lld): %s\n\n", curtok->posinfo->file->c_str(), curtok->posinfo->line, alloc);

		va_end(ap);
		exit(1);
	}


	// woah shit it's forward declarations
	// note: all these are expected to pop at least one token from the front of the list.
	void parseAll(std::deque<Token*>& tokens);
	Func* parseFunc(std::deque<Token*>& tokens);
	Expr* parseExpr(std::deque<Token*>& tokens);
	Expr* parseIdExpr(std::deque<Token*>& tokens);
	Expr* parsePrimary(std::deque<Token*>& tokens);
	Import* parseImport(std::deque<Token*>& tokens);
	Return* parseReturn(std::deque<Token*>& tokens);
	Number* parseNumber(std::deque<Token*>& tokens);
	VarDecl* parseVarDecl(std::deque<Token*>& tokens);
	Func* parseTopLevelExpr(std::deque<Token*>& tokens);
	FuncDecl* parseFuncDecl(std::deque<Token*>& tokens);
	Expr* parseParenthesised(std::deque<Token*>& tokens);
	ForeignFuncDecl* parseForeignFunc(std::deque<Token*>& tokens);
	Expr* parseRhs(std::deque<Token*>& tokens, Expr* expr, int prio);
	Expr* parseFunctionCall(std::deque<Token*>& tokens, std::string id);

	Root* Parse(std::string filename, std::string str)
	{
		Token* t = nullptr;
		pos.file = new std::string(filename);
		pos.line = 1;

		std::deque<Token*> tokens;

		while((t = getNextToken(str, pos)) != nullptr)
			tokens.push_back(t);

		rootNode = new Root();
		parseAll(tokens);

		return rootNode;
	}

	// helpers

	static void skipNewline(std::deque<Token*>& tokens)
	{
		while(tokens.size() > 0 && tokens.front()->type == TType::NewLine)
			tokens.pop_front();
	}

	static Token* eat(std::deque<Token*>& tokens)
	{
		// returns the current front, then pops front.
		if(tokens.size() == 0)
			error("Unexpected end of input");

		Token* t = tokens.front();
		tokens.pop_front();
		skipNewline(tokens);

		curtok = t;
		return t;
	}

	static bool checkHasMore(std::deque<Token*>& tokens)
	{
		return tokens.size() > 0;
	}

	static int getOpPrec(Token* tok)
	{
		switch(tok->type)
		{
			case TType::DoublePlus:
			case TType::DoubleMinus:
				return 50;

			case TType::Asterisk:
			case TType::Divide:
			case TType::Percent:
				return 40;

			case TType::Plus:
			case TType::Minus:
				return 20;

			case TType::ShiftLeft:
			case TType::ShiftRight:
				return 10;

			case TType::LAngle:
			case TType::RAngle:
			case TType::LessThanEquals:
			case TType::GreaterEquals:
				return 5;

			case TType::EqualsTo:
			case TType::NotEquals:
				return 4;

			case TType::Equal:
				return 1;

			default:
				return -1;
		}
	}

	static VarType determineVarType(Token* tok)
	{
		// kinda hardcoded
		if(tok->text == "Int8")			return VarType::Int8;
		else if(tok->text == "Int16")	return VarType::Int16;
		else if(tok->text == "Int32")	return VarType::Int32;
		else if(tok->text == "Int64")	return VarType::Int64;
		else if(tok->text == "Uint8")	return VarType::Uint8;
		else if(tok->text == "Uint16")	return VarType::Uint16;
		else if(tok->text == "Uint32")	return VarType::Uint32;
		else if(tok->text == "Uint64")	return VarType::Uint64;
		else if(tok->text == "Float32")	return VarType::Float32;
		else if(tok->text == "Float64")	return VarType::Float64;
		else if(tok->text == "Bool")	return VarType::Bool;
		else							return VarType::UserDefined;
	}













	void parseAll(std::deque<Token*>& tokens)
	{
		if(tokens.size() == 0)
			return;

		while(Token* tok = tokens.front())
		{
			assert(tok != nullptr);
			switch(tok->type)
			{
				// so-called 'top-level' things that need to manually recurse back into this function
				// may be dangerous -- look into goto or smth instead of recursing
				case TType::Func:
					rootNode->functions.push_back(parseFunc(tokens));
					break;

				case TType::Import:
					rootNode->imports.push_back(parseImport(tokens));
					break;

				case TType::ForeignFunc:
					rootNode->foreignfuncs.push_back(parseForeignFunc(tokens));
					break;

				// shit you just skip
				case TType::NewLine:
				case TType::Comment:
				case TType::Semicolon:
					tokens.pop_front();
					break;


				default:	// wip: skip shit we don't know/care about for now
					parseTopLevelExpr(tokens);
					break;
			}
		}
	}


















	Expr* parsePrimary(std::deque<Token*>& tokens)
	{
		if(tokens.size() == 0)
			return nullptr;

		while(Token* tok = tokens.front())
		{
			assert(tok != nullptr);
			switch(tok->type)
			{
				case TType::Var:
				case TType::Val:
					return parseVarDecl(tokens);

				case TType::LParen:
					return parseParenthesised(tokens);

				case TType::Identifier:
					return parseIdExpr(tokens);

				case TType::Integer:
				case TType::Decimal:
					return parseNumber(tokens);

				case TType::Return:
					return parseReturn(tokens);

				// shit you just skip
				case TType::NewLine:
				case TType::Comment:
				case TType::Semicolon:
					tokens.pop_front();
					return parsePrimary(tokens);

				default:	// wip: skip shit we don't know/care about for now
					fprintf(stderr, "Unknown token '%s', skipping\n", tok->text.c_str());
					tokens.pop_front();
					break;
			}
		}

		return nullptr;
	}

	FuncDecl* parseFuncDecl(std::deque<Token*>& tokens)
	{
		assert(eat(tokens)->type == TType::Func);
		if(tokens.front()->type != TType::Identifier)
			error("Expected identifier, but got token of type %d", tokens.front()->type);

		std::string id = tokens.front()->text;
		eat(tokens);

		// expect a left bracket
		if(eat(tokens)->type != TType::LParen)
			error("Expected '(' in function declaration");

		// get the parameter list
		// expect an identifer, colon, type
		std::deque<VarDecl*> params;
		std::map<std::string, VarDecl*> nameCheck;
		while(tokens.size() > 0 && tokens.front()->type != TType::RParen)
		{
			Token* tok_id;
			if((tok_id = eat(tokens))->type != TType::Identifier)
				error("Expected identifier");

			std::string id = tok_id->text;
			VarDecl* v = new VarDecl(id, true);

			// expect a colon
			if(eat(tokens)->type != TType::Colon)
				error("Expected ':' followed by a type");

			Token* tok_type;
			if((tok_type = eat(tokens))->type != TType::Identifier)
				error("Expected type after parameter");

			v->type = tok_type->text;
			v->varType = determineVarType(tok_type);

			if(!nameCheck[v->name])
			{
				params.push_back(v);
				nameCheck[v->name] = v;
			}
			else
			{
				error("Redeclared variable '%s' in argument list", v->name.c_str());
			}
		}

		// consume the closing paren
		eat(tokens);

		// get return type.
		std::string ret;
		Token* tok_type = nullptr;
		if(checkHasMore(tokens) && tokens.front()->type == TType::Arrow)
		{
			eat(tokens);
			if((tok_type = eat(tokens))->type != TType::Identifier)
				error("Expected type after parameter");

			ret = tok_type->text;
		}
		else
		{
			ret = "void";
		}

		skipNewline(tokens);
		FuncDecl* f = new FuncDecl(id, params, ret);
		f->varType = tok_type == nullptr ? VarType::Void : determineVarType(tok_type);

		return f;
	}

	ForeignFuncDecl* parseForeignFunc(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::ForeignFunc);
		eat(tokens);

		FuncDecl* decl = parseFuncDecl(tokens);
		return new ForeignFuncDecl(decl);
	}

	Func* parseFunc(std::deque<Token*>& tokens)
	{
		FuncDecl* decl = parseFuncDecl(tokens);

		Func* c = new Func(decl);

		// make sure the first token is a left brace.
		if(eat(tokens)->type != TType::LBrace)
			error("Expected '{'");

		skipNewline(tokens);

		// get the stuff inside.
		while(tokens.size() > 0 && tokens.front()->type != TType::RBrace)
		{
			c->statements.push_back(parseExpr(tokens));
			skipNewline(tokens);
		}

		if(eat(tokens)->type != TType::RBrace)
			error("Expected '}'");

		return c;
	}

	VarDecl* parseVarDecl(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::Var || tokens.front()->type == TType::Val);

		bool immutable = tokens.front()->type == TType::Val;
		eat(tokens);

		// get the identifier.
		Token* tok_id;
		if((tok_id = eat(tokens))->type != TType::Identifier)
			error("Expected identifier for variable declaration.");

		std::string id = tok_id->text;
		VarDecl* v = new VarDecl(id, immutable);

		// check the type.
		// todo: type inference
		if(eat(tokens)->type != TType::Colon)
			error("Expected colon to indicate type for variable declaration");

		Token* tok_type;
		if((tok_type = eat(tokens))->type != TType::Identifier)
			error("Expected type for variable declaration");

		v->type = tok_type->text;
		v->varType = determineVarType(tok_type);

		// TODO:
		// check if we have a default value
		v->initVal = nullptr;
		if(tokens.front()->type == TType::Equal)
		{
			// we do
			eat(tokens);

			v->initVal = parseExpr(tokens);
			if(!v->initVal)
				error("Invalid initialiser for variable '%s'", v->name.c_str());
		}

		return v;
	}

	Expr* parseParenthesised(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::LParen);
		eat(tokens);

		Expr* within = parseExpr(tokens);

		if(eat(tokens)->type != TType::RParen)
			error("Expected ')'");

		return within;
	}

	Expr* parseExpr(std::deque<Token*>& tokens)
	{
		Expr* lhs = parsePrimary(tokens);
		if(!lhs)
			return nullptr;

		return parseRhs(tokens, lhs, 0);
	}

	Expr* parseRhs(std::deque<Token*>& tokens, Expr* lhs, int prio)
	{
		while(true)
		{
			int prec = getOpPrec(tokens.front());
			if(prec < prio)
				return lhs;

			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything
			Token* tok_op = eat(tokens);

			Expr* rhs = parsePrimary(tokens);
			if(!rhs)
				return nullptr;

			int next = getOpPrec(tokens.front());
			if(prec < next)
			{
				rhs = parseRhs(tokens, rhs, prec + 1);
				if(!rhs)
					return nullptr;
			}

			ArithmeticOp op;
			switch(tok_op->type)
			{
				case TType::Plus:			op = ArithmeticOp::Add;			break;
				case TType::Minus:			op = ArithmeticOp::Subtract;	break;
				case TType::Asterisk:		op = ArithmeticOp::Multiply;	break;
				case TType::Divide:			op = ArithmeticOp::Divide;		break;
				case TType::Percent:		op = ArithmeticOp::Modulo;		break;
				case TType::ShiftLeft:		op = ArithmeticOp::ShiftLeft;	break;
				case TType::ShiftRight:		op = ArithmeticOp::ShiftRight;	break;
				case TType::Equal:			op = ArithmeticOp::Assign;		break;
				default:					error("Unknown operator '%s'", tok_op->text.c_str());
			}

			lhs = new BinOp(lhs, op, rhs);
		}
	}

	Expr* parseIdExpr(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::Identifier);
		std::string id = eat(tokens)->text;

		// todo: handle function calling
		skipNewline(tokens);

		if(tokens.front()->type != TType::LParen)
			return new VarRef(id);
		else
			return parseFunctionCall(tokens, id);
	}

	Number* parseNumber(std::deque<Token*>& tokens)
	{
		Number* n;
		if(tokens.front()->type == TType::Integer)
		{
			Token* tok = eat(tokens);
			n = new Number((int64_t) std::stoll(tok->text));

			// set the type.
			// always used signed
			n->varType = VarType::Int64;
		}
		else if(tokens.front()->type == TType::Decimal)
		{
			Token* tok = eat(tokens);
			n = new Number(std::stod(tok->text));

			if(n->dval < FLT_MAX)	n->varType = VarType::Float32;
			else					n->varType = VarType::Float64;
		}
		else
		{
			error("What!????");
			assert(false);
			return nullptr;
		}

		return n;
	}

	Expr* parseFunctionCall(std::deque<Token*>& tokens, std::string id)
	{
		assert(eat(tokens)->type == TType::LParen);


		std::deque<Expr*> args;
		if(tokens.front()->type != TType::RParen)
		{
			while(true)
			{
				Expr* arg = parseExpr(tokens);
				if(arg == nullptr)
					return nullptr;

				args.push_back(arg);

				if(tokens.front()->type == TType::RParen)
				{
					eat(tokens);
					break;
				}

				Token* t;
				if((t = eat(tokens))->type != TType::Comma)
					error("Expected either ',' or ')' in parameter list, got '%s'", t->text.c_str());
			}
		}
		else
		{
			eat(tokens);
		}

		return new FuncCall(id, args);
	}

	Return* parseReturn(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::Return);
		eat(tokens);

		return new Return(parseExpr(tokens));
	}

	Func* parseTopLevelExpr(std::deque<Token*>& tokens)
	{
		Expr* expr = parseExpr(tokens);
		FuncDecl* fakedecl = new FuncDecl("__anonymous_toplevel", std::deque<VarDecl*>(), "");
		Func* fakefunc = new Func(fakedecl);

		fakefunc->statements.push_back(expr);
		rootNode->functions.push_back(fakefunc);

		return fakefunc;
	}


	Import* parseImport(std::deque<Token*>& tokens)
	{
		assert(eat(tokens)->type == TType::Import);

		Token* tok_mod;
		if((tok_mod = eat(tokens))->type != TType::Identifier)
			error("Expected module name after 'import' statement.");

		return new Import(tok_mod->text);
	}
}

























