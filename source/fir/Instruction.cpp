// Instruction.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/block.h"
#include "ir/function.h"
#include "ir/constant.h"
#include "ir/instruction.h"

#include "mpool.h"

namespace fir
{
	static util::MemoryPool<Value> value_pool(2048);


	Instruction::Instruction(OpKind kind, bool sideeff, IRBlock* parent, Type* out, const std::vector<Value*>& vals)
		: Instruction(kind, sideeff, parent, out, vals, Value::Kind::rvalue) { }

	Instruction::Instruction(OpKind kind, bool sideeff, IRBlock* parent, Type* out, const std::vector<Value*>& vals, Value::Kind k) : Value(out)
	{
		this->opKind = kind;
		this->operands = vals;
		this->sideEffects = sideeff;
		this->parentBlock = parent;
		this->realOutput = value_pool.construct(out, k);
	}

	Value* Instruction::getResult()
	{
		if(this->realOutput) return this->realOutput;
		error("Calling getActualValue() when not in function! (no real value)");
	}

	bool Instruction::hasSideEffects()
	{
		return this->sideEffects;
	}

	void Instruction::setValue(Value* v)
	{
		this->realOutput = v;
	}

	void Instruction::clearValue()
	{
		this->realOutput = 0;
	}

	std::string Instruction::str()
	{
		std::string instrname;
		switch(this->opKind)
		{
			case OpKind::Signed_Add:                        instrname = "sadd"; break;
			case OpKind::Signed_Sub:                        instrname = "ssub"; break;
			case OpKind::Signed_Mul:                        instrname = "smul"; break;
			case OpKind::Signed_Div:                        instrname = "sdiv"; break;
			case OpKind::Signed_Mod:                        instrname = "srem"; break;
			case OpKind::Signed_Neg:                        instrname = "neg"; break;
			case OpKind::Unsigned_Add:                      instrname = "uadd"; break;
			case OpKind::Unsigned_Sub:                      instrname = "usub"; break;
			case OpKind::Unsigned_Mul:                      instrname = "umul"; break;
			case OpKind::Unsigned_Div:                      instrname = "udiv"; break;
			case OpKind::Unsigned_Mod:                      instrname = "urem"; break;
			case OpKind::Floating_Add:                      instrname = "fadd"; break;
			case OpKind::Floating_Sub:                      instrname = "fsub"; break;
			case OpKind::Floating_Mul:                      instrname = "fmul"; break;
			case OpKind::Floating_Div:                      instrname = "fdiv"; break;
			case OpKind::Floating_Mod:                      instrname = "frem"; break;
			case OpKind::Floating_Neg:                      instrname = "fneg"; break;
			case OpKind::Floating_Truncate:                 instrname = "ftrunc"; break;
			case OpKind::Floating_Extend:                   instrname = "fext"; break;
			case OpKind::ICompare_Equal:                    instrname = "icmp eq"; break;
			case OpKind::ICompare_NotEqual:                 instrname = "icmp ne"; break;
			case OpKind::ICompare_Greater:                  instrname = "icmp gt"; break;
			case OpKind::ICompare_Less:                     instrname = "icmp lt"; break;
			case OpKind::ICompare_GreaterEqual:             instrname = "icmp ge"; break;
			case OpKind::ICompare_LessEqual:                instrname = "icmp le"; break;
			case OpKind::FCompare_Equal_ORD:                instrname = "fcmp ord eq"; break;
			case OpKind::FCompare_Equal_UNORD:              instrname = "fcmp unord eq"; break;
			case OpKind::FCompare_NotEqual_ORD:             instrname = "fcmp ord ne"; break;
			case OpKind::FCompare_NotEqual_UNORD:           instrname = "fcmp unord ne"; break;
			case OpKind::FCompare_Greater_ORD:              instrname = "fcmp ord gt"; break;
			case OpKind::FCompare_Greater_UNORD:            instrname = "fcmp unord gt"; break;
			case OpKind::FCompare_Less_ORD:                 instrname = "fcmp ord lt"; break;
			case OpKind::FCompare_Less_UNORD:               instrname = "fcmp unord lt"; break;
			case OpKind::FCompare_GreaterEqual_ORD:         instrname = "fcmp ord ge"; break;
			case OpKind::FCompare_GreaterEqual_UNORD:       instrname = "fcmp unord ge"; break;
			case OpKind::FCompare_LessEqual_ORD:            instrname = "fcmp ord le"; break;
			case OpKind::FCompare_LessEqual_UNORD:          instrname = "fcmp unord le"; break;
			case OpKind::ICompare_Multi:                    instrname = "icmp multi"; break;
			case OpKind::FCompare_Multi:                    instrname = "fcmp multi"; break;
			case OpKind::Bitwise_Not:                       instrname = "not"; break;
			case OpKind::Bitwise_Xor:                       instrname = "xor"; break;
			case OpKind::Bitwise_Arithmetic_Shr:            instrname = "ashr"; break;
			case OpKind::Bitwise_Logical_Shr:               instrname = "lshr"; break;
			case OpKind::Bitwise_Shl:                       instrname = "shl"; break;
			case OpKind::Bitwise_And:                       instrname = "and"; break;
			case OpKind::Bitwise_Or:                        instrname = "or"; break;
			case OpKind::Cast_Bitcast:                      instrname = "bitcast"; break;
			case OpKind::Cast_IntSize:                      instrname = "intszcast"; break;
			case OpKind::Cast_Signedness:                   instrname = "signedcast"; break;
			case OpKind::Cast_FloatToInt:                   instrname = "fptoint"; break;
			case OpKind::Cast_IntToFloat:                   instrname = "inttofp"; break;
			case OpKind::Cast_PointerType:                  instrname = "ptrcast"; break;
			case OpKind::Cast_PointerToInt:                 instrname = "ptrtoint"; break;
			case OpKind::Cast_IntToPointer:                 instrname = "inttoptr"; break;
			case OpKind::Cast_IntSignedness:                instrname = "signcast"; break;
			case OpKind::Integer_ZeroExt:                   instrname = "izeroext"; break;
			case OpKind::Integer_Truncate:                  instrname = "itrunc"; break;
			case OpKind::Value_WritePtr:                    instrname = "writemem"; break;
			case OpKind::Logical_Not:                       instrname = "logicalNot"; break;
			case OpKind::Value_ReadPtr:                     instrname = "readmem"; break;
			case OpKind::Value_StackAlloc:                  instrname = "stackAlloc"; break;
			case OpKind::Value_CallFunction:                instrname = "call"; break;
			case OpKind::Value_CallFunctionPointer:         instrname = "callfp"; break;
			case OpKind::Value_CallVirtualMethod:           instrname = "callvirtual"; break;
			case OpKind::Value_Return:                      instrname = "ret"; break;
			case OpKind::Value_GetPointerToStructMember:    instrname = "gep"; break;
			case OpKind::Value_GetStructMember:             instrname = "gep"; break;
			case OpKind::Value_GetPointer:                  instrname = "gep"; break;
			case OpKind::Value_GetGEP2:                     instrname = "gep"; break;
			case OpKind::Value_InsertValue:                 instrname = "insertval"; break;
			case OpKind::Value_ExtractValue:                instrname = "extractval"; break;
			case OpKind::Value_Select:                      instrname = "select"; break;
			case OpKind::Misc_Sizeof:                       instrname = "sizeof"; break;
			case OpKind::Branch_UnCond:                     instrname = "jump"; break;
			case OpKind::Branch_Cond:                       instrname = "branch"; break;
			case OpKind::Value_PointerAddition:             instrname = "ptradd"; break;
			case OpKind::Value_PointerSubtraction:          instrname = "ptrsub"; break;

			case OpKind::Value_CreatePHI:                   instrname = "phi"; break;

			case OpKind::SAA_GetData:                       instrname = "get_saa.data"; break;
			case OpKind::SAA_SetData:                       instrname = "set_saa.data"; break;
			case OpKind::SAA_GetLength:                     instrname = "get_saa.len"; break;
			case OpKind::SAA_SetLength:                     instrname = "set_saa.len"; break;
			case OpKind::SAA_GetCapacity:                   instrname = "get_saa.cap"; break;
			case OpKind::SAA_SetCapacity:                   instrname = "set_saa.cap"; break;
			case OpKind::SAA_GetRefCountPtr:                instrname = "get_saa.rc_ptr"; break;
			case OpKind::SAA_SetRefCountPtr:                instrname = "set_saa.rc_ptr"; break;


			case OpKind::ArraySlice_GetData:                instrname = "get_slice.data"; break;
			case OpKind::ArraySlice_SetData:                instrname = "set_slice.data"; break;
			case OpKind::ArraySlice_GetLength:              instrname = "get_slice.len"; break;
			case OpKind::ArraySlice_SetLength:              instrname = "set_slice.len"; break;

			case OpKind::Any_GetData:                       instrname = "get_any.data"; break;
			case OpKind::Any_SetData:                       instrname = "set_any.data"; break;
			case OpKind::Any_GetTypeID:                     instrname = "get_any.typeid"; break;
			case OpKind::Any_SetTypeID:                     instrname = "set_any.typeid"; break;
			case OpKind::Any_GetRefCountPtr:                instrname = "get_any.rc_ptr"; break;
			case OpKind::Any_SetRefCountPtr:                instrname = "set_any.rc_ptr"; break;

			case OpKind::Range_GetLower:                    instrname = "get_range.lower"; break;
			case OpKind::Range_SetLower:                    instrname = "set_range.lower"; break;
			case OpKind::Range_GetUpper:                    instrname = "get_range.upper"; break;
			case OpKind::Range_SetUpper:                    instrname = "set_range.upper"; break;
			case OpKind::Range_GetStep:                     instrname = "get_range.step"; break;
			case OpKind::Range_SetStep:                     instrname = "set_range.step"; break;

			case OpKind::Enum_GetIndex:                     instrname = "get_enum.index"; break;
			case OpKind::Enum_SetIndex:                     instrname = "set_enum.index"; break;
			case OpKind::Enum_GetValue:                     instrname = "get_enum.value"; break;
			case OpKind::Enum_SetValue:                     instrname = "set_enum.value"; break;


			case OpKind::Union_SetValue:                    instrname = "set_union.value"; break;
			case OpKind::Union_GetValue:                    instrname = "get_union.value"; break;
			case OpKind::Union_GetVariantID:                instrname = "get_union.id"; break;
			case OpKind::Union_SetVariantID:                instrname = "set_union.id"; break;

			case OpKind::Value_AddressOf:                   instrname = "addrof"; break;
			case OpKind::Value_Store:                       instrname = "store"; break;
			case OpKind::Value_Dereference:                 instrname = "dereferece"; break;
			case OpKind::Value_CreateLVal:                  instrname = "make_lval"; break;

			case OpKind::Unreachable:                       instrname = "<unreachable>"; break;
			case OpKind::Invalid:                           instrname = "<unknown>"; break;
		}


		std::string ops;
		bool endswithfn = false;

		if(this->opKind == OpKind::Value_CreatePHI)
		{
			auto phi = dynamic_cast<PHINode*>(this->realOutput);
			iceAssert(phi);
			std::string nodes;

			for(auto i : phi->getValues())
				nodes += strprintf("[$%s -> %%%zu], ", i.first->getName().name, i.second->id);

			ops += nodes;
		}
		else
		{
			for(auto op : this->operands)
			{
				bool didfn = false;
				if(op->getType()->isFunctionType())
				{
					ops += "@" + op->getName().str();
					if(this->opKind == OpKind::Value_CallFunction)
					{
						ops += ", (";
						didfn = true;
					}
				}
				else if(ConstantInt* ci = dynamic_cast<ConstantInt*>(op))
				{
					ops += std::to_string(ci->getSignedValue());
				}
				else if(ConstantFP* cf = dynamic_cast<ConstantFP*>(op))
				{
					ops += std::to_string(cf->getValue());
				}
				else if(ConstantBool* cb = dynamic_cast<ConstantBool*>(op))
				{
					ops += cb->getValue() ? "true" : "false";
				}
				else if(ConstantArraySlice* cas = dynamic_cast<ConstantArraySlice*>(op))
				{
					ops += "(const slice %" + std::to_string(op->id) + ", %" + std::to_string(cas->getData()->id) + ", %"
						+ std::to_string(cas->getLength()->id) + " :: " + op->getType()->str();
				}
				else if(dynamic_cast<ConstantValue*>(op))
				{
					ops += "(const %" + std::to_string(op->id) + " :: " + op->getType()->str() + ")";
				}
				else if(IRBlock* ib = dynamic_cast<IRBlock*>(op))
				{
					ops += "$" + ib->getName().str();
				}
				else
				{
					auto name = op->getName().str();
					ops += name + (name.empty() ? "" : " ") + "(%" + std::to_string(op->id) + ") :: " + op->getType()->str();
				}

				if(!didfn)
					ops += ", ";

				endswithfn = didfn;
			}
		}



		if(ops.length() > 0 && !endswithfn)
			ops = ops.substr(0, ops.length() - 2);


		if(this->opKind == OpKind::Value_CallFunction)
			ops += ")";


		std::string ret = "";
		if(this->realOutput->getType()->isVoidType())
		{
			ret = instrname + " " + ops;
		}
		else
		{
			auto name = this->realOutput->getName().str();
			ret = name + (name.empty() ? "" : " ") + "(%" + std::to_string(this->realOutput->id) + ") :: " + this->realOutput->getType()->str() + " = " + instrname + " " + ops;
		}

		// return strprintf("!%d ", this->id) + ret;
		return ret;
	}
}










































