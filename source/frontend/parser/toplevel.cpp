// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "errors.h"
#include "parser.h"
#include "frontend.h"

#include "parser_internal.h"

#include "mpool.h"

using namespace lexer;
using namespace ast;

namespace parser
{

	static std::string parseModuleName(std::string fullpath)
	{
		using TT = lexer::TokenType;
		auto tokens = frontend::getFileTokens(fullpath);

		std::string ret;

		// basically, this is how it goes:
		// only allow comments to occur before the module name.
		for(size_t i = 0; i < tokens.size(); i++)
		{
			const Token& tok = tokens[i];
			if(tok.type == TT::Export)
			{
				i++;

				if(tokens[i].type == TT::Identifier)
				{
					ret = tokens[i].str();
					i++;
				}
				else
				{
					expectedAfter(tokens[i].loc, "identifier for export declaration", "'module'", tokens[i].str());
				}


				if(tokens[i].type != TT::NewLine && tokens[i].type != TT::Semicolon && tokens[i].type != TT::Comment)
				{
					expected(tokens[i].loc, "newline or semicolon to terminate export statement", tokens[i].str());
				}

				// i++ handled by loop
			}
			else if(tok.type == TT::Comment || tok.type == TT::NewLine)
			{
				// skipped
			}
			else
			{
				// stop
				break;
			}
		}

		if(ret == "")
			ret = frontend::removeExtensionFromFilename(frontend::getFilenameFromPath(fullpath));

		return ret;
	}



	TopLevelBlock* parseTopLevel(State& st, std::string name)
	{
		using TT = lexer::TokenType;
		TopLevelBlock* root = util::pool<TopLevelBlock>(st.loc(), name);

		// if it's not empty, then it's an actual user-defined namespace
		bool hadLBrace = false;
		if(name != "")
		{
			// expect "namespace FOO { ... }"

			hadLBrace = true;
			iceAssert(st.front() == TT::Identifier);
			st.eat();

			if(st.eat() != TT::LBrace)
				expected(st.ploc(), "'{' to start namespace declaration", st.prev().str());
		}

		bool isFirst = true;
		auto priv = VisibilityLevel::Invalid;
		size_t tix = (size_t) -1;


		// flags that determine whether or not 'import' and '@operator' things can still be done.
		bool importsStillValid = true;
		bool operatorsStillValid = true;


		while(st.hasTokens() && st.front() != TT::EndOfFile)
		{
			switch(st.front())
			{
				case TT::Import:
					if(name != "" || !importsStillValid)
						error(st, "import statements are not allowed here");

					root->statements.push_back(parseImport(st));
					break;

				case TT::Attr_Operator:
					if(name != "" || !operatorsStillValid)
						error(st, "custom operator declarations are not allowed here");

					// just skip it.
					st.setIndex(parseOperatorDecl(st.getTokenList(), st.getIndex(), 0, 0));

					importsStillValid = false;
					break;

				case TT::Namespace: {
					st.eat();
					Token tok = st.front();
					if(tok != TT::Identifier)
						expectedAfter(st, "identifier", "'namespace'", st.front().str());

					auto ns = parseTopLevel(st, tok.str());
					if(priv != VisibilityLevel::Invalid)
						ns->visibility = priv, priv = VisibilityLevel::Invalid, tix = -1;

					root->statements.push_back(ns);

					importsStillValid = false;
					operatorsStillValid = false;

				} break;

				case TT::Attr_NoMangle: {
					st.pop();
					auto stmt = parseStmt(st);
					if(!dynamic_cast<FuncDefn*>(stmt) && !dynamic_cast<VarDefn*>(stmt))
						error(st, "attribute '@nomangle' can only be applied on function and variable declarations");

					else if(dynamic_cast<ForeignFuncDefn*>(stmt))
						warn(st, "attribute '@nomangle' is redundant on 'ffi' functions");

					else if(auto fd = dynamic_cast<FuncDefn*>(stmt))
						fd->noMangle = true;

					else if(auto vd = dynamic_cast<VarDefn*>(stmt))
						vd->noMangle = true;

					root->statements.push_back(stmt);

					importsStillValid = false;
					operatorsStillValid = false;

				} break;

				case TT::Attr_EntryFn: {
					st.pop();
					auto stmt = parseStmt(st);
					if(auto fd = dynamic_cast<FuncDefn*>(stmt))
						fd->isEntry = true;

					else
						error(st, "'@entry' attribute is only applicable to function definitions");

					root->statements.push_back(stmt);

					importsStillValid = false;
					operatorsStillValid = false;

				} break;

				case TT::Public:
					priv = VisibilityLevel::Public;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Private:
					priv = VisibilityLevel::Private;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Internal:
					priv = VisibilityLevel::Internal;
					tix = st.getIndex();
					st.pop();
					break;

				case TT::Comment:
				case TT::NewLine:
					isFirst = true;
					st.skipWS();
					continue;

				case TT::RBrace:
					if(!hadLBrace) error(st, "Unexpected '}'");
					goto out;

				default:
					if(priv != VisibilityLevel::Invalid)
					{
						st.rewindTo(tix);

						tix = -1;
						priv = VisibilityLevel::Invalid;
					}

					if(st.front() == TT::Export)
					{
						if(!isFirst || name != "")
						{
							error(st, "export declaration not allowed here (%s / %d)", name, isFirst);
						}
						else
						{
							st.eat();
							st.eat();

							break;
						}
					}

					importsStillValid = false;
					operatorsStillValid = false;

					root->statements.push_back(parseStmt(st));
					break;
			}

			isFirst = false;
			st.skipWS();
		}

		out:
		if(name != "")
		{
			if(st.front() != TT::RBrace)
				expected(st, "'}' to close namespace declaration", st.front().str());

			st.eat();
		}

		return root;
	}

	ParsedFile parseFile(std::string filename, frontend::CollectorState& cs)
	{
		auto full = frontend::getFullPathOfFile(filename);
		const TokenList& tokens = frontend::getFileTokens(full);
		auto state = State(tokens);
		state.currentFilePath = full;

		// copy this stuff over.
		state.binaryOps = cs.binaryOps;
		state.prefixOps = cs.prefixOps;
		state.postfixOps = cs.postfixOps;

		auto modname = parseModuleName(full);
		auto toplevel = parseTopLevel(state, "");


		return ParsedFile {
			filename,
			modname,
			toplevel,
		};
	}
}





































