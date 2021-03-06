// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

#include "gluecode.h"

namespace pts
{
	std::string unwrapPointerType(const std::string&, int*);
}

namespace fir
{
	static TypeCache tc;
	TypeCache& TypeCache::get()
	{
		return tc;
	}

	int getCastDistance(Type* from, Type* to)
	{
		if(from == to) return 0;

		if(from->isConstantNumberType() && to->isPrimitiveType())
		{
			auto cty = from->toConstantNumberType();
			if(!cty->isFloating() && to->isIntegerType())
				return 0;

			else if(!cty->isFloating() && to->isFloatingPointType())
				return 1;

			else if(cty->isFloating())      // not isint means isfloat, so if we're doing float -> float the cost is 0
				return 0;

			else                            // if we reach here, we're trying to do float -> int, which is a no-go.
				return -1;
		}
		else if(from->isIntegerType() && to->isIntegerType())
		{
			if(from->isSignedIntType() == to->isSignedIntType())
			{
				auto bitdiff = abs((int) from->toPrimitiveType()->getIntegerBitWidth() - (int) to->toPrimitiveType()->getIntegerBitWidth());
				switch(bitdiff)
				{
					case 0:		return 0;	// same
					case 8:		return 1;	// i16 - i8
					case 16:	return 1;	// i32 - i16
					case 32:	return 1;	// i64 - i32

					case 24:	return 2;	// i32 - i8
					case 48:	return 2;	// i64 - i16

					case 56:	return 3;	// i64 - i8
					default:	iceAssert(0);
				}
			}
			else
			{
				// only allow casting unsigned things to signed things... maybe??
				// TODO: investigate whether we want such loose casting.

				//? for now, no.
				return -1;
			}
		}
		else if(from->isDynamicArrayType() && to->isArraySliceType() && from->getArrayElementType() == to->getArrayElementType())
		{
			return 2;
		}
		else if(from->isDynamicArrayType() && from->getArrayElementType()->isVoidType() && (to->isDynamicArrayType() || to->isArraySliceType() || to->isArrayType()))
		{
			return 2;
		}
		else if(from->isFloatingPointType() && to->isFloatingPointType())
		{
			return 1;
		}
		else if(from->isStringType() && to == fir::Type::getInt8Ptr())
		{
			return 5;
		}
		else if(from->isStringType() && to->isCharSliceType())
		{
			return 3;
		}
		else if(from->isCharSliceType() && to == fir::Type::getInt8Ptr())
		{
			return 3;
		}
		else if(from->isMutablePointer() && to->isImmutablePointer() && from->getPointerElementType() == to->getPointerElementType())
		{
			// cast from a mutable pointer type to an immutable one can be implicit.
			return 1;
		}
		else if(from->isVariadicArrayType() && to->isArraySliceType() && from->getArrayElementType() == to->getArrayElementType())
		{
			// allow implicit casting from variadic slices to their normal counterparts.
			return 4;
		}
		else if(from->isArraySliceType() && to->isArraySliceType() && (from->getArrayElementType() == to->getArrayElementType())
			&& from->toArraySliceType()->isMutable() && !to->toArraySliceType()->isMutable() && !from->isVariadicArrayType() && !to->isVariadicArrayType())
		{
			// same with slices -- cast from mutable slice to immut slice can be implicit.
			return 1;
		}
		//* note: we don't need to check that 'to' is a class type, because if it's not then the parent check will fail anyway.
		else if(from->isPointerType() && to->isPointerType() && from->getPointerElementType()->isClassType()
			&& from->getPointerElementType()->toClassType()->isInParentHierarchy(to->getPointerElementType()))
		{
			// cast from a derived class pointer to a base class pointer
			return 2;
		}
		else if(from->isNullType() && to->isPointerType())
		{
			return 1;
		}
		else if(from->isTupleType() && to->isTupleType() && from->toTupleType()->getElementCount() == to->toTupleType()->getElementCount())
		{
			int sum = 0;

			auto ftt = from->toTupleType();
			auto ttt = to->toTupleType();

			for(size_t i = 0; i < ttt->getElementCount(); i++)
			{
				if(int k = fir::getCastDistance(ftt->getElementN(i), ttt->getElementN(i)); k < 0)
					return -1;

				else
					sum += k;
			}

			return sum;
		}
		else if(to->isAnyType())
		{
			// lol. completely arbitrary.
			return 15;
		}

		return -1;
	}














	std::string Type::typeListToString(const std::initializer_list<Type*>& types, bool includeBraces)
	{
		return typeListToString(std::vector<Type*>(types.begin(), types.end()), includeBraces);
	}

	std::string Type::typeListToString(const std::vector<Type*>& types, bool braces)
	{
		// print types
		std::string str = (braces ? "{ " : "");
		for(auto t : types)
			str += t->str() + ", ";

		if(str.length() > 2)
			str = str.substr(0, str.length() - 2);

		return str + (braces ? " }" : "");
	}


	bool Type::areTypeListsEqual(const std::vector<Type*>& a, const std::vector<Type*>& b)
	{
		if(a.size() != b.size()) return false;
		if(a.size() == 0 && b.size() == 0) return true;

		for(size_t i = 0; i < a.size(); i++)
		{
			if(a[i] != b[i])
				return false;
		}

		return true;
	}

	bool Type::areTypeListsEqual(const std::initializer_list<Type*>& a, const std::initializer_list<Type*>& b)
	{
		return areTypeListsEqual(std::vector<Type*>(a.begin(), a.end()), std::vector<Type*>(b.begin(), b.end()));
	}



	Type* Type::getPointerTo()
	{
		// cache the pointer internally
		if(!this->pointerTo)
		{
			PointerType* newType = new PointerType(this, false);
			this->pointerTo = newType;
		}

		return this->pointerTo;
	}

	Type* Type::getMutablePointerTo()
	{
		// cache the pointer internally
		if(!this->mutablePointerTo)
		{
			PointerType* newType = new PointerType(this, true);
			this->mutablePointerTo = newType;
		}

		return this->mutablePointerTo;
	}


	Type* Type::getMutablePointerVersion()
	{
		iceAssert(this->isPointerType() && "not pointer type");
		return this->toPointerType()->getMutable();
	}

	Type* Type::getImmutablePointerVersion()
	{
		iceAssert(this->isPointerType() && "not pointer type");
		return this->toPointerType()->getImmutable();
	}


	Type* Type::getPointerElementType()
	{
		if(!this->isPointerType())
			error("type is not a pointer ('%s')", this);

		PointerType* ptrthis = this->toPointerType();
		iceAssert(ptrthis);

		// ptrthis could only have been obtained by calling getPointerTo
		// on an already normalised type, so this should not be needed
		// newType = tc->normaliseType(newType);

		return ptrthis->baseType;
	}


	Type* Type::getIndirectedType(int times)
	{
		Type* ret = this;
		if(times > 0)
		{
			for(int i = 0; i < times; i++)
				ret = ret->getPointerTo();
		}
		else if(times < 0)
		{
			for(int i = 0; i < -times; i++)
				ret = ret->getPointerElementType();
		}
		// both getPointerTo and getPointerElementType should already
		// return normalised types
		// ret = tc->normaliseType(ret);
		return ret;
	}


	Type* Type::fromBuiltin(const std::string& builtin)
	{

		int indirections = 0;
		auto copy = pts::unwrapPointerType(builtin, &indirections);

		Type* real = 0;

		if(copy == INT8_TYPE_STRING)                    real = Type::getInt8();
		else if(copy == INT16_TYPE_STRING)              real = Type::getInt16();
		else if(copy == INT32_TYPE_STRING)              real = Type::getInt32();
		else if(copy == INT64_TYPE_STRING)              real = Type::getInt64();
		else if(copy == INT128_TYPE_STRING)             real = Type::getInt128();

		else if(copy == UINT8_TYPE_STRING)              real = Type::getUint8();
		else if(copy == UINT16_TYPE_STRING)             real = Type::getUint16();
		else if(copy == UINT32_TYPE_STRING)             real = Type::getUint32();
		else if(copy == UINT64_TYPE_STRING)             real = Type::getUint64();
		else if(copy == UINT128_TYPE_STRING)            real = Type::getUint128();

		else if(copy == FLOAT32_TYPE_STRING)            real = Type::getFloat32();
		else if(copy == FLOAT64_TYPE_STRING)            real = Type::getFloat64();
		else if(copy == FLOAT128_TYPE_STRING)           real = Type::getFloat128();

		else if(copy == STRING_TYPE_STRING)             real = Type::getString();

		else if(copy == CHARACTER_SLICE_TYPE_STRING)    real = ArraySliceType::get(Type::getInt8(), false);

		else if(copy == BOOL_TYPE_STRING)               real = Type::getBool();
		else if(copy == VOID_TYPE_STRING)               real = Type::getVoid();

		// unspecified things
		else if(copy == INTUNSPEC_TYPE_STRING)          real = Type::getInt64();
		else if(copy == UINTUNSPEC_TYPE_STRING)         real = Type::getUint64();

		else if(copy == FLOAT_TYPE_STRING)              real = Type::getFloat32();
		else if(copy == DOUBLE_TYPE_STRING)             real = Type::getFloat64();

		else if(copy == ANY_TYPE_STRING)                real = Type::getAny();

		else return 0;

		iceAssert(real);

		real = real->getIndirectedType(indirections);
		return real;
	}



	Type* Type::getArrayElementType()
	{
		if(this->isDynamicArrayType())		return this->toDynamicArrayType()->getElementType();
		else if(this->isArrayType())		return this->toArrayType()->getElementType();
		else if(this->isArraySliceType())	return this->toArraySliceType()->getElementType();
		else								error("'%s' is not an array type", this);
	}



	size_t Type::getBitWidth()
	{
		if(this->isIntegerType())
			return this->toPrimitiveType()->getIntegerBitWidth();

		else if(this->isFloatingPointType())
			return this->toPrimitiveType()->getFloatingPointBitWidth();

		else if(this->isPointerType())
			return sizeof(void*) * CHAR_BIT;

		else
			return 0;
	}


	static bool _containsPlaceholders(fir::Type* ty, std::unordered_set<fir::Type*>& seen, std::vector<PolyPlaceholderType*>* found)
	{
		if(seen.find(ty) != seen.end())
			return false;

		seen.insert(ty);

		if(ty->isPolyPlaceholderType())
		{
			if(found) found->push_back(ty->toPolyPlaceholderType());
			return true;
		}
		else if(ty->isPointerType())        return _containsPlaceholders(ty->getPointerElementType(), seen, found);
		else if(ty->isArrayType())          return _containsPlaceholders(ty->getArrayElementType(), seen, found);
		else if(ty->isArraySliceType())     return _containsPlaceholders(ty->getArrayElementType(), seen, found);
		else if(ty->isDynamicArrayType())   return _containsPlaceholders(ty->getArrayElementType(), seen, found);
		else if(ty->isArrayType())          return _containsPlaceholders(ty->getArrayElementType(), seen, found);
		else if(ty->isUnionVariantType())   return _containsPlaceholders(ty->toUnionVariantType()->getInteriorType(), seen, found);
		else if(ty->isTupleType())
		{
			bool res = false;
			for(auto t : ty->toTupleType()->getElements())
				res |= _containsPlaceholders(t, seen, found);

			return res;
		}
		else if(ty->isClassType())
		{
			bool res = false;
			for(auto t : ty->toClassType()->getElements())
				res |= _containsPlaceholders(t, seen, found);

			return res;
		}
		else if(ty->isStructType())
		{
			bool res = false;
			for(auto t : ty->toStructType()->getElements())
				res |= _containsPlaceholders(t, seen, found);

			return res;
		}
		else if(ty->isFunctionType())
		{
			bool res = ty->toFunctionType()->getReturnType()->containsPlaceholders();
			for(auto t : ty->toFunctionType()->getArgumentTypes())
				res |= _containsPlaceholders(t, seen, found);

			return res;
		}
		else if(ty->isUnionType())
		{
			bool res = false;
			for(auto t : ty->toUnionType()->getVariants())
				res |= _containsPlaceholders(t.second, seen, found);

			return res;
		}
		else
		{
			return false;
		}
	}


	// better to just handle this centrally i guess.
	bool Type::containsPlaceholders()
	{
		std::unordered_set<fir::Type*> seen;
		return _containsPlaceholders(this, seen, nullptr);
	}

	std::vector<PolyPlaceholderType*> Type::getContainedPlaceholders()
	{
		std::unordered_set<fir::Type*> seen;
		std::vector<PolyPlaceholderType*> found;

		_containsPlaceholders(this, seen, &found);
		return found;
	}



	bool Type::isPointerTo(Type* other)
	{
		return other->getPointerTo() == this;
	}

	bool Type::isPointerElementOf(Type* other)
	{
		return this->getPointerTo() == other;
	}

	PrimitiveType* Type::toPrimitiveType()
	{
		if(this->kind != TypeKind::Primitive) error("not primitive type");
		return static_cast<PrimitiveType*>(this);
	}

	FunctionType* Type::toFunctionType()
	{
		if(this->kind != TypeKind::Function) error("not function type");
		return static_cast<FunctionType*>(this);
	}

	PointerType* Type::toPointerType()
	{
		if(this->kind != TypeKind::Pointer) error("not pointer type");
		return static_cast<PointerType*>(this);
	}

	StructType* Type::toStructType()
	{
		if(this->kind != TypeKind::Struct) error("not struct type");
		return static_cast<StructType*>(this);
	}

	ClassType* Type::toClassType()
	{
		if(this->kind != TypeKind::Class) error("not class type");
		return static_cast<ClassType*>(this);
	}

	TupleType* Type::toTupleType()
	{
		if(this->kind != TypeKind::Tuple) error("not tuple type");
		return static_cast<TupleType*>(this);
	}

	ArrayType* Type::toArrayType()
	{
		if(this->kind != TypeKind::Array) error("not array type");
		return static_cast<ArrayType*>(this);
	}

	DynamicArrayType* Type::toDynamicArrayType()
	{
		if(this->kind != TypeKind::DynamicArray) error("not dynamic array type");
		return static_cast<DynamicArrayType*>(this);
	}

	ArraySliceType* Type::toArraySliceType()
	{
		if(this->kind != TypeKind::ArraySlice) error("not array slice type");
		return static_cast<ArraySliceType*>(this);
	}

	RangeType* Type::toRangeType()
	{
		if(this->kind != TypeKind::Range) error("not range type");
		return static_cast<RangeType*>(this);
	}

	StringType* Type::toStringType()
	{
		if(this->kind != TypeKind::String) error("not string type");
		return static_cast<StringType*>(this);
	}

	EnumType* Type::toEnumType()
	{
		if(this->kind != TypeKind::Enum) error("not enum type");
		return static_cast<EnumType*>(this);
	}

	UnionType* Type::toUnionType()
	{
		if(this->kind != TypeKind::Union) error("not union type");
		return static_cast<UnionType*>(this);
	}

	AnyType* Type::toAnyType()
	{
		if(this->kind != TypeKind::Any) error("not any type");
		return static_cast<AnyType*>(this);
	}

	NullType* Type::toNullType()
	{
		if(this->kind != TypeKind::Null) error("not null type");
		return static_cast<NullType*>(this);
	}

	ConstantNumberType* Type::toConstantNumberType()
	{
		if(this->kind != TypeKind::ConstantNumber) error("not constant number type");
		return static_cast<ConstantNumberType*>(this);
	}

	PolyPlaceholderType* Type::toPolyPlaceholderType()
	{
		if(this->kind != TypeKind::PolyPlaceholder) error("not poly placeholder type");
		return static_cast<PolyPlaceholderType*>(this);
	}

	UnionVariantType* Type::toUnionVariantType()
	{
		if(this->kind != TypeKind::UnionVariant) error("not union variant type");
		return static_cast<UnionVariantType*>(this);
	}









	bool Type::isConstantNumberType()
	{
		return this->kind == TypeKind::ConstantNumber;
	}

	bool Type::isStructType()
	{
		return this->kind == TypeKind::Struct;
	}

	bool Type::isTupleType()
	{
		return this->kind == TypeKind::Tuple;
	}

	bool Type::isClassType()
	{
		return this->kind == TypeKind::Class;
	}

	bool Type::isPackedStruct()
	{
		return this->isStructType() && (this->toStructType()->isTypePacked);
	}

	bool Type::isArrayType()
	{
		return this->kind == TypeKind::Array;
	}

	bool Type::isFloatingPointType()
	{
		return this->kind == TypeKind::Primitive && (this->toPrimitiveType()->primKind == PrimitiveType::Kind::Floating);
	}

	bool Type::isIntegerType()
	{
		return this->kind == TypeKind::Primitive && (this->toPrimitiveType()->primKind == PrimitiveType::Kind::Integer);
	}

	bool Type::isSignedIntType()
	{
		return this->isIntegerType() && this->toPrimitiveType()->isSigned();
	}

	bool Type::isFunctionType()
	{
		return this->kind == TypeKind::Function;
	}

	bool Type::isPrimitiveType()
	{
		return this->kind == TypeKind::Primitive;
	}

	bool Type::isPointerType()
	{
		return this->kind == TypeKind::Pointer;
	}

	bool Type::isVoidType()
	{
		return this->kind == TypeKind::Void;
	}

	bool Type::isDynamicArrayType()
	{
		return this->kind == TypeKind::DynamicArray;
	}

	bool Type::isVariadicArrayType()
	{
		return this->isArraySliceType() && this->toArraySliceType()->isVariadicType();
	}

	bool Type::isArraySliceType()
	{
		return this->kind == TypeKind::ArraySlice;
	}

	bool Type::isRangeType()
	{
		return this->kind == TypeKind::Range;
	}

	bool Type::isStringType()
	{
		return this->kind == TypeKind::String;
	}

	bool Type::isCharType()
	{
		return this == fir::Type::getInt8();
	}

	bool Type::isEnumType()
	{
		return this->kind == TypeKind::Enum;
	}

	bool Type::isUnionType()
	{
		return this->kind == TypeKind::Union;
	}

	bool Type::isAnyType()
	{
		return this->kind == TypeKind::Any;
	}

	bool Type::isNullType()
	{
		return this->kind == TypeKind::Null;
	}

	bool Type::isBoolType()
	{
		return this == fir::Type::getBool();
	}

	bool Type::isMutablePointer()
	{
		return this->isPointerType() && this->toPointerType()->isMutable();
	}

	bool Type::isImmutablePointer()
	{
		return this->isPointerType() && !this->toPointerType()->isMutable();
	}

	bool Type::isCharSliceType()
	{
		return this->isArraySliceType() && this->getArrayElementType() == fir::Type::getInt8();
	}

	bool Type::isPolyPlaceholderType()
	{
		return this->kind == TypeKind::PolyPlaceholder;
	}

	bool Type::isUnionVariantType()
	{
		return this->kind == TypeKind::UnionVariant;
	}



	// static getting functions
	VoidType* Type::getVoid()
	{
		return VoidType::get();
	}

	NullType* Type::getNull()
	{
		return NullType::get();
	}

	Type* Type::getVoidPtr()
	{
		return VoidType::get()->getPointerTo();
	}

	BoolType* Type::getBool()
	{
		return BoolType::get();
	}

	PrimitiveType* Type::getInt8()
	{
		return PrimitiveType::getInt8();
	}

	PrimitiveType* Type::getInt16()
	{
		return PrimitiveType::getInt16();
	}

	PrimitiveType* Type::getInt32()
	{
		return PrimitiveType::getInt32();
	}

	PrimitiveType* Type::getInt64()
	{
		return PrimitiveType::getInt64();
	}

	PrimitiveType* Type::getInt128()
	{
		return PrimitiveType::getInt128();
	}

	PrimitiveType* Type::getUint8()
	{
		return PrimitiveType::getUint8();
	}

	PrimitiveType* Type::getUint16()
	{
		return PrimitiveType::getUint16();
	}

	PrimitiveType* Type::getUint32()
	{
		return PrimitiveType::getUint32();
	}

	PrimitiveType* Type::getUint64()
	{
		return PrimitiveType::getUint64();
	}

	PrimitiveType* Type::getUint128()
	{
		return PrimitiveType::getUint128();
	}

	PrimitiveType* Type::getFloat32()
	{
		return PrimitiveType::getFloat32();
	}

	PrimitiveType* Type::getFloat64()
	{
		return PrimitiveType::getFloat64();
	}

	PrimitiveType* Type::getFloat128()
	{
		return PrimitiveType::getFloat128();
	}


	PointerType* Type::getInt8Ptr()
	{
		return PointerType::getInt8Ptr();
	}

	PointerType* Type::getInt16Ptr()
	{
		return PointerType::getInt16Ptr();
	}

	PointerType* Type::getInt32Ptr()
	{
		return PointerType::getInt32Ptr();
	}

	PointerType* Type::getInt64Ptr()
	{
		return PointerType::getInt64Ptr();
	}

	PointerType* Type::getInt128Ptr()
	{
		return PointerType::getInt128Ptr();
	}

	PointerType* Type::getUint8Ptr()
	{
		return PointerType::getUint8Ptr();
	}

	PointerType* Type::getUint16Ptr()
	{
		return PointerType::getUint16Ptr();
	}

	PointerType* Type::getUint32Ptr()
	{
		return PointerType::getUint32Ptr();
	}

	PointerType* Type::getUint64Ptr()
	{
		return PointerType::getUint64Ptr();
	}

	PointerType* Type::getUint128Ptr()
	{
		return PointerType::getUint128Ptr();
	}




	PointerType* Type::getMutInt8Ptr()
	{
		return PointerType::getInt8Ptr()->getMutable();
	}

	PointerType* Type::getMutInt16Ptr()
	{
		return PointerType::getInt16Ptr()->getMutable();
	}

	PointerType* Type::getMutInt32Ptr()
	{
		return PointerType::getInt32Ptr()->getMutable();
	}

	PointerType* Type::getMutInt64Ptr()
	{
		return PointerType::getInt64Ptr()->getMutable();
	}

	PointerType* Type::getMutInt128Ptr()
	{
		return PointerType::getInt128Ptr()->getMutable();
	}

	PointerType* Type::getMutUint8Ptr()
	{
		return PointerType::getUint8Ptr()->getMutable();
	}

	PointerType* Type::getMutUint16Ptr()
	{
		return PointerType::getUint16Ptr()->getMutable();
	}

	PointerType* Type::getMutUint32Ptr()
	{
		return PointerType::getUint32Ptr()->getMutable();
	}

	PointerType* Type::getMutUint64Ptr()
	{
		return PointerType::getUint64Ptr()->getMutable();
	}

	PointerType* Type::getMutUint128Ptr()
	{
		return PointerType::getUint128Ptr()->getMutable();
	}





	ArraySliceType* Type::getCharSlice(bool mut)
	{
		return ArraySliceType::get(fir::Type::getInt8(), mut);
	}

	RangeType* Type::getRange()
	{
		return RangeType::get();
	}

	StringType* Type::getString()
	{
		return StringType::get();
	}

	AnyType* Type::getAny()
	{
		return AnyType::get();
	}

















	static size_t getAggregateSize(const std::vector<Type*>& tys)
	{
		size_t ptr = 0;
		size_t aln = 0;

		for(auto ty : tys)
		{
			auto a = getAlignmentOfType(ty);
			if(ptr % a > 0)
				ptr += (a - (ptr % a));

			ptr += getSizeOfType(ty);
			aln = std::max(aln, a);
		}

		if(ptr % aln > 0)
			ptr += (aln - (ptr % aln));

		return ptr;
	}

	size_t getSizeOfType(Type* type)
	{
		auto ptrt = fir::Type::getInt8Ptr();
		auto i64t = fir::Type::getInt64();

		if(type->isVoidType())                                      return 0;
		else if(type->isBoolType())                                 return 1;
		else if(type->isPointerType() || type->isFunctionType())    return sizeof(void*);
		else if(type->isPrimitiveType())                            return type->getBitWidth() / 8;
		else if(type->isArraySliceType())                           return getAggregateSize({ ptrt, i64t });
		else if(type->isStringType() || type->isDynamicArrayType()) return getAggregateSize({ ptrt, i64t, i64t, ptrt });
		else if(type->isRangeType())                                return getAggregateSize({ i64t, i64t, i64t });
		else if(type->isArrayType())
		{
			return type->toArrayType()->getArraySize() * getSizeOfType(type->getArrayElementType());
		}
		else if(type->isEnumType())
		{
			return getAggregateSize({ i64t, type->toEnumType()->getCaseType() });
		}
		else if(type->isAnyType())
		{
			return getAggregateSize({ i64t, ptrt, fir::ArrayType::get(fir::Type::getInt8(), BUILTIN_ANY_DATA_BYTECOUNT) });
		}
		else if(type->isClassType() || type->isStructType() || type->isTupleType())
		{
			std::vector<Type*> tys;

			if(type->isClassType())
			{
				auto c = type->toClassType();
				auto base = c;
				while(base)
				{
					tys.insert(tys.begin(), base->getElements().begin(), base->getElements().end());
					base = base->getBaseClass();
				}

				tys.insert(tys.begin(), fir::Type::getInt8Ptr());
			}
			else if(type->isStructType())
			{
				tys = type->toStructType()->getElements();
			}
			else
			{
				tys = type->toTupleType()->getElements();
			}

			return getAggregateSize(tys);
		}
		else
		{
			error("cannot get size of unsupported type '%s'", type);
		}
	}

	size_t getAlignmentOfType(Type* type)
	{
		if(type->isArrayType())     return getAlignmentOfType(type->getArrayElementType());
		else                        return getSizeOfType(type);
	}
}





namespace tinyformat
{
	void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, fir::Type* ty)
	{
		out << (ty ? ty->str() : "(null)");
	}
}












