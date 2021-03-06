// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "frontend.h"
#include "parser_internal.h"

#include "mpool.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	using TT = lexer::TokenType;
	ImportStmt* parseImport(State& st)
	{
		iceAssert(st.front() == TT::Import);
		st.eat();

		if(st.frontAfterWS() != TT::StringLiteral)
			expectedAfter(st, "string literal", "'import' for module specifier", st.frontAfterWS().str());

		{
			auto ret = util::pool<ImportStmt>(st.loc(), st.frontAfterWS().str());
			ret->resolvedModule = frontend::resolveImport(ret->path, ret->loc, st.currentFilePath);

			st.eat();

			// check for 'import as foo'
			if(st.frontAfterWS() == TT::As)
			{
				st.eat();
				auto t = st.eat();
				if(t == TT::Identifier)
					ret->importAs = util::to_string(t.text);

				else
					expectedAfter(st.ploc(), "identifier", "'import-as'", st.prev().str());
			}

			return ret;
		}
	}

	UsingStmt* parseUsingStmt(State& st)
	{
		iceAssert(st.front() == TT::Using);
		st.eat();

		auto ret = util::pool<UsingStmt>(st.ploc());

		//* so the deal is, we can't use 'parseExpr' here to parse the thing we want to 'use', because "X as Y" parses
		//* as a cast instead.

		//* so, we parse it manually, including handling the dot operator stuff.
		// TODO: fix this maybe? more robustness would be good.

		if(st.front() != TT::Identifier)
			expectedAfter(st.loc(), "identifier", "'using'", st.front().str());

		std::vector<std::pair<std::string, Location>> names;
		while(st.front() == TT::Identifier)
		{
			names.push_back({ st.eat().str(), st.ploc() });
			if(st.front() == TT::Period)
			{
				st.eat();
				continue;
			}
			else
			{
				break;
			}
		}

		// ok, convert the names into a series of dot-op + identifier nesting nonsenses.
		if(names.size() == 1)
		{
			ret->expr = util::pool<Ident>(names[0].second, names[0].first);
		}
		else
		{
			auto a = util::pool<Ident>(names[0].second, names[0].first);
			auto b = util::pool<Ident>(names[1].second, names[1].first);

			auto left = util::pool<DotOperator>(names[0].second, a, b);
			for(size_t i = 2; i < names.size(); i++)
				left = util::pool<DotOperator>(names[i].second, left, util::pool<Ident>(names[i].second, names[i].first));

			ret->expr = left;
		}

		if(st.front() != TT::As)
			expectedAfter(st.loc(), "'as'", "scope in 'using'", st.front().str());

		st.eat();
		if(st.front() != TT::Identifier)
			expectedAfter(st.loc(), "identifier", "'as' in 'using' declaration", st.front().str());

		ret->useAs = st.eat().str();
		return ret;
	}
}

void expected(const Location& loc, std::string a, std::string b)
{
	error(loc, "expected %s, found '%s' instead", a, b);
}

void expectedAfter(const Location& loc, std::string a, std::string b, std::string c)
{
	error(loc, "expected %s after %s, found '%s' instead", a, b, c);
}

void unexpected(const Location& loc, std::string a)
{
	error(loc, "unexpected %s", a);
}













