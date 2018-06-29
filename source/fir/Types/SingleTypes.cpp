// SingleTypes.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	static AnyType* singleAny = 0;
	AnyType::AnyType() : Type(TypeKind::Any)            { }
	std::string AnyType::str()                          { return "any"; }
	std::string AnyType::encodedStr()                   { return "any"; }
	bool AnyType::isTypeEqual(Type* other)              { return other && other->isAnyType(); }
	AnyType* AnyType::get()                             { return singleAny = (singleAny ? singleAny : new AnyType()); }
	fir::Type* AnyType::substitutePlaceholders(const std::unordered_map<fir::Type*, fir::Type*>& subst)     { return this; }


	static BoolType* singleBool = 0;
	BoolType::BoolType() : Type(TypeKind::Bool)         { }
	std::string BoolType::str()                         { return "bool"; }
	std::string BoolType::encodedStr()                  { return "bool"; }
	bool BoolType::isTypeEqual(Type* other)             { return other && other->isBoolType(); }
	BoolType* BoolType::get()                           { return singleBool = (singleBool ? singleBool : new BoolType()); }
	fir::Type* BoolType::substitutePlaceholders(const std::unordered_map<fir::Type*, fir::Type*>& subst)    { return this; }


	static VoidType* singleVoid = 0;
	VoidType::VoidType() : Type(TypeKind::Void)         { }
	std::string VoidType::str()                         { return "void"; }
	std::string VoidType::encodedStr()                  { return "void"; }
	bool VoidType::isTypeEqual(Type* other)             { return other && other->isVoidType(); }
	VoidType* VoidType::get()                           { return singleVoid = (singleVoid ? singleVoid : new VoidType()); }
	fir::Type* VoidType::substitutePlaceholders(const std::unordered_map<fir::Type*, fir::Type*>& subst)    { return this; }


	static NullType* singleNull = 0;
	NullType::NullType() : Type(TypeKind::Null)         { }
	std::string NullType::str()                         { return "nulltype"; }
	std::string NullType::encodedStr()                  { return "nulltype"; }
	bool NullType::isTypeEqual(Type* other)             { return other && other->isNullType(); }
	NullType* NullType::get()                           { return singleNull = (singleNull ? singleNull : new NullType()); }
	fir::Type* NullType::substitutePlaceholders(const std::unordered_map<fir::Type*, fir::Type*>& subst)    { return this; }


	static RangeType* singleRange = 0;
	RangeType::RangeType() : Type(TypeKind::Range)      { }
	std::string RangeType::str()                        { return "range"; }
	std::string RangeType::encodedStr()                 { return "range"; }
	bool RangeType::isTypeEqual(Type* other)            { return other && other->isRangeType(); }
	RangeType* RangeType::get()                         { return singleRange = (singleRange ? singleRange : new RangeType()); }
	fir::Type* RangeType::substitutePlaceholders(const std::unordered_map<fir::Type*, fir::Type*>& subst)   { return this; }


	static StringType* singleString = 0;
	StringType::StringType() : Type(TypeKind::String)   { }
	std::string StringType::str()                       { return "string"; }
	std::string StringType::encodedStr()                { return "string"; }
	bool StringType::isTypeEqual(Type* other)           { return other && other->isStringType(); }
	StringType* StringType::get()                       { return singleString = (singleString ? singleString : new StringType()); }
	fir::Type* StringType::substitutePlaceholders(const std::unordered_map<fir::Type*, fir::Type*>& subst)  { return this; }



	std::string ConstantNumberType::str()                       { return "number"; }
	std::string ConstantNumberType::encodedStr()                { return "number"; }
	bool ConstantNumberType::isSigned()                         { return this->_signed; }
	bool ConstantNumberType::isFloating()                       { return this->_floating; }
	size_t ConstantNumberType::getMinBits()                     { return this->_bits; }
	bool ConstantNumberType::isTypeEqual(Type* other)           { return other && other->isConstantNumberType(); }
	ConstantNumberType* ConstantNumberType::get(bool neg, bool flt, size_t bits)
	{
		return new ConstantNumberType(neg, flt, bits);
	}
	ConstantNumberType::ConstantNumberType(bool neg, bool flt, size_t bits) : Type(TypeKind::ConstantNumber)
	{
		this->_bits = bits;
		this->_signed = neg;
		this->_floating = flt;
	}
	fir::Type* ConstantNumberType::substitutePlaceholders(const std::unordered_map<fir::Type*, fir::Type*>& subst)
	{
		return this;
	}


	ConstantNumberType* unifyConstantTypes(ConstantNumberType* a, ConstantNumberType* b)
	{
		auto sgn = a->isSigned() || b->isSigned();
		auto flt = a->isFloating() || b->isFloating();
		auto bit = std::max(a->getMinBits(), b->getMinBits());

		return ConstantNumberType::get(sgn, flt, bit);
	}



	std::string PolyPlaceholderType::str()          { return strprintf("$%s", this->name); }
	std::string PolyPlaceholderType::encodedStr()   { return strprintf("$%s", this->name); }

	std::string PolyPlaceholderType::getName()      { return this->name; }
	int PolyPlaceholderType::getGroup()             { return this->group; }

	PolyPlaceholderType* PolyPlaceholderType::get(const std::string& n, int group)
	{
		return TypeCache::get().getOrAddCachedType(new PolyPlaceholderType(n, group));
	}

	bool PolyPlaceholderType::isTypeEqual(Type* other)
	{
		return other && other->isPolyPlaceholderType() && other->toPolyPlaceholderType()->name == this->name
			&& other->toPolyPlaceholderType()->group == this->group;
	}

	PolyPlaceholderType::PolyPlaceholderType(const std::string& n, int g) : Type(TypeKind::PolyPlaceholder)
	{
		this->name = n;
		this->group = g;
	}


	static fir::Type* _substitute(const std::unordered_map<fir::Type*, fir::Type*>& subst, fir::Type* t)
	{
		if(auto it = subst.find(t); it != subst.end())
			return it->second->substitutePlaceholders(subst);

		return t;
	}

	fir::Type* PolyPlaceholderType::substitutePlaceholders(const std::unordered_map<fir::Type*, fir::Type*>& subst)
	{
		return _substitute(subst, this);
	}


}





















