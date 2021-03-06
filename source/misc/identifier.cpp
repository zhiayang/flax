// identifier.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "ir/type.h"
#include "sst.h"


//* so how this works is, instead of having to do manual checks and error-posting, since everyone uses this TCResult system,
//* when we try to unwrap something we got from a typecheck and it's actually an error, we just post the error and quit.
//? in theory this should work like it used to, probably.
// TODO: investigate??

sst::Stmt* TCResult::stmt() const
{
	if(this->_kind == RK::Error)
		this->_pe->postAndQuit();

	switch(this->_kind)
	{
		case RK::Statement:     return this->_st;
		case RK::Expression:    return this->_ex;
		case RK::Definition:    return this->_df;
		default:                _error_and_exit("not stmt");
	}
}

sst::Expr* TCResult::expr() const
{
	if(this->_kind == RK::Error)
		this->_pe->postAndQuit();

	if(this->_kind != RK::Expression)
		_error_and_exit("not expr\n");

	return this->_ex;
}

sst::Defn* TCResult::defn() const
{
	if(this->_kind == RK::Error)
		this->_pe->postAndQuit();

	if(this->_kind != RK::Definition)
		_error_and_exit("not defn\n");

	return this->_df;
}


bool Identifier::operator == (const Identifier& other) const
{
	return (other.name == this->name) && (other.str() == this->str());
}
bool Identifier::operator != (const Identifier& other) const
{
	return !(other == *this);
}


std::string Identifier::str() const
{
	std::string ret;
	for(auto s : this->scope)
		ret += s + ".";

	ret += this->name;

	if(this->kind == IdKind::Function)
	{
		ret += "(";
		for(auto p : this->params)
			ret += p->str() + ", ";

		if(this->params.size() > 0)
			ret.pop_back(), ret.pop_back();

		ret += ")";
	}

	return ret;
}



static std::string mangleScopeOnly(const Identifier& id)
{
	bool first = true;
	std::string ret;
	for(auto s : id.scope)
	{
		ret += (!first ? std::to_string(s.length()) : "") + s;
		first = false;
	}

	return ret;
}

static inline std::string lentypestr(std::string s)
{
	return std::to_string(s.length()) + s;
}

static std::string mangleScopeName(const Identifier& id)
{
	return mangleScopeOnly(id) + lentypestr(id.name);
}

static std::string mangleType(fir::Type* t)
{
	if(t->isPrimitiveType())
	{
		return lentypestr(t->encodedStr());
	}
	if(t->isBoolType())
	{
		return lentypestr(t->encodedStr());
	}
	else if(t->isArrayType())
	{
		return "FA" + lentypestr(mangleType(t->getArrayElementType())) + std::to_string(t->toArrayType()->getArraySize());
	}
	else if(t->isDynamicArrayType())
	{
		return "DA" + lentypestr(mangleType(t->getArrayElementType()));
	}
	else if(t->isArraySliceType())
	{
		return "SL" + lentypestr(mangleType(t->getArrayElementType()));
	}
	else if(t->isVoidType())
	{
		return "v";
	}
	else if(t->isFunctionType())
	{
		std::string ret = "FN" + std::to_string(t->toFunctionType()->getArgumentTypes().size()) + "FA";
		for(auto a : t->toFunctionType()->getArgumentTypes())
		{
			ret += lentypestr(mangleType(a));
		}

		if(t->toFunctionType()->getArgumentTypes().empty())
			ret += "v";

		return ret;
	}
	else if(t->isStructType())
	{
		return lentypestr(mangleScopeName(t->toStructType()->getTypeName()));
	}
	else if(t->isClassType())
	{
		return lentypestr(mangleScopeName(t->toClassType()->getTypeName()));
	}
	else if(t->isTupleType())
	{
		std::string ret = "ST" + std::to_string(t->toTupleType()->getElementCount()) + "SM";
		for(auto m : t->toTupleType()->getElements())
			ret += lentypestr(mangleType(m));

		return ret;
	}
	else if(t->isPointerType())
	{
		return "PT" + lentypestr(mangleType(t->getPointerElementType()));
	}
	else if(t->isStringType())
	{
		return "SR";
	}
	else if(t->isCharType())
	{
		return "CH";
	}
	else if(t->isEnumType())
	{
		return "EN" + lentypestr(mangleType(t->toEnumType()->getCaseType())) + lentypestr(mangleScopeName(t->toEnumType()->getTypeName()));
	}
	else if(t->isAnyType())
	{
		return "AY";
	}
	else
	{
		_error_and_exit("unsupported ir type??? ('%s')", t);
	}
}

static std::string _doMangle(const Identifier& id, bool includeScope)
{
	if(id.kind == IdKind::Name || id.kind == IdKind::Type)
	{
		std::string scp;
		if(includeScope)
			scp += mangleScopeOnly(id);

		if(includeScope && id.scope.size() > 0)
			scp += std::to_string(id.name.length());

		return scp + id.name;
	}
	else if(!includeScope)
	{
		if(id.kind == IdKind::Function)
		{
			std::string ret = id.name + "(";

			if(id.params.empty())
			{
				ret += ")";
			}
			else
			{
				for(auto t : id.params)
					ret += t->str() + ",";

				ret = ret.substr(0, ret.length() - 1);
				ret += ")";
			}

			return ret;
		}
		else
		{
			_error_and_exit("invalid");
		}
	}
	else
	{
		std::string ret = "_F";

		if(id.kind == IdKind::Function)     ret += "F";
		else if(id.kind == IdKind::Type)    ret += "T";
		else                                ret += "U";

		if(includeScope)
			ret += mangleScopeOnly(id);

		ret += lentypestr(id.name);

		if(id.kind == IdKind::Function)
		{
			ret += "_FA";
			for(auto t : id.params)
				ret += "_" + mangleType(t);

			if(id.params.empty())
				ret += "v";
		}

		return ret;
	}
}


std::string Identifier::mangled() const
{
	return _doMangle(*this, true);
}

std::string Identifier::mangledName() const
{
	return _doMangle(*this, false);
}






namespace util
{
	std::string typeParamMapToString(const std::string& name, const TypeParamMap_t& map)
	{
		if(map.empty())
			return name;

		std::string ret;
		for(auto m : map)
			ret += (m.first + ":" + m.second->encodedStr()) + ",";

		// shouldn't be empty.
		iceAssert(ret.size() > 0);
		return strprintf("%s<%s>", name, ret.substr(0, ret.length() - 1));
	}
}


namespace tinyformat
{
	void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, const VisibilityLevel& vl)
	{
		switch(vl)
		{
			case VisibilityLevel::Invalid:	out << "invalid"; break;
			case VisibilityLevel::Public:	out << "public"; break;
			case VisibilityLevel::Private:	out << "private"; break;
			case VisibilityLevel::Internal:	out << "internal"; break;
		}
	}

	void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, const Identifier& id)
	{
		out << id.str();
	}
}





