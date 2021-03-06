// resolver.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include "resolver.h"
#include "polymorph.h"

namespace sst {
namespace resolver
{
	std::pair<int, ErrorMsg*> computeOverloadDistance(const Location& fnLoc, const std::vector<fir::LocatedType>& target,
		const std::vector<fir::LocatedType>& _args, bool cvararg)
	{
		std::vector<fir::LocatedType> input;
		if(cvararg) input = util::take(_args, target.size());
		else        input = _args;

		auto [ soln, err ] = poly::solveTypeList(target, input, poly::Solution_t(), /* isFnCall: */ true);

		if(err != nullptr)  return { -1, err };
		else                return { soln.distance, nullptr };
	}



	std::pair<int, ErrorMsg*> computeNamedOverloadDistance(const Location& fnLoc, const std::vector<FnParam>& target,
		const std::vector<FnCallArgument>& _args, bool cvararg)
	{
		std::vector<FnCallArgument> input;
		if(cvararg) input = util::take(_args, target.size());
		else        input = _args;

		ErrorMsg* err = 0;
		auto arguments = resolver::misc::canonicaliseCallArguments(fnLoc, target, input, &err);
		if(err != nullptr) return { -1, err };

		auto [ soln, err1 ] = poly::solveTypeList(util::map(target, [](const FnParam& p) -> fir::LocatedType {
			return fir::LocatedType(p.type, p.loc);
		}), arguments, poly::Solution_t(), /* isFnCall: */ true);


		if(err1 != nullptr) return { -1, err1 };
		else                return { soln.distance, nullptr };
	}




	namespace internal
	{
		std::pair<TCResult, std::vector<FnCallArgument>> resolveFunctionCallFromCandidates(TypecheckState* fs, const Location& callLoc,
			const std::vector<std::pair<Defn*, std::vector<FnCallArgument>>>& _cands, const TypeParamMap_t& gmaps, bool allowImplicitSelf,
			fir::Type* return_infer)
		{
			if(_cands.empty())
				return { TCResult(BareError::make("no candidates")), { } };

			int bestDist = INT_MAX;
			std::map<Defn*, ErrorMsg*> fails;
			std::vector<std::tuple<Defn*, std::vector<FnCallArgument>, int>> finals;

			auto cands = _cands;

			for(const auto& [ _cand, _args ] : cands)
			{
				int dist = -1;
				Defn* curcandidate = _cand;
				std::vector<FnCallArgument> replacementArgs = _args;

				if(auto fn = dcast(FunctionDecl, curcandidate))
				{
					// check for placeholders -- means that we should attempt to infer the type of the parent if its a static method.
					//* there are some assumptions we can make -- primarily that this will always be a static method of a type.
					//? (a): namespaces cannot be generic.
					//? (b): instance methods must have an associated 'self', and you can't have a variable of generic type
					if(fn->type->containsPlaceholders())
					{
						if(auto fd = dcast(FunctionDefn, fn); !fd)
						{
							error("invalid non-definition of a function with placeholder types");
						}
						else
						{
							// ok, i guess.
							iceAssert(fd);
							iceAssert(fd->original);

							// do an inference -- with the arguments that we have.
							auto [ res, soln ] = poly::attemptToInstantiatePolymorph(fs, fd->original, fn->id.name, gmaps, /* return_infer: */ return_infer,
								/* type_infer: */ nullptr, /* isFnCall: */ true, &replacementArgs, /* fillPlacholders: */ false,
								/* problem_infer: */ fn->type);

							if(!res.isDefn())
							{
								fails[fn] = res.error();
								dist = -1;
							}
							else
							{
								curcandidate = res.defn();
								std::tie(dist, fails[fn]) = std::make_tuple(soln.distance, nullptr);
							}
						}
					}
					else
					{
						std::tie(dist, fails[fn]) = computeNamedOverloadDistance(fn->loc, fn->params, replacementArgs, fn->isVarArg);
					}
				}
				else if(auto vr = dcast(VarDefn, curcandidate))
				{
					iceAssert(vr->type->isFunctionType());
					auto ft = vr->type->toFunctionType();

					// check if have any names
					for(auto p : replacementArgs)
					{
						if(p.name != "")
						{
							return { TCResult(SimpleError::make(p.loc, "function values cannot be called with named arguments")->append(
								SimpleError::make(vr->loc, "'%s' was defined here:", vr->id.name))
							), { } };
						}
					}

					auto prms = ft->getArgumentTypes();
					std::tie(dist, fails[vr]) = computeOverloadDistance(curcandidate->loc, util::map(prms, [](fir::Type* t) -> fir::LocatedType {
						return fir::LocatedType(t, Location());
					}), util::map(replacementArgs, [](const FnCallArgument& p) -> fir::LocatedType {
						return fir::LocatedType(p.value->type, Location());
					}), false);
				}

				if(dist == -1)
					continue;

				else if(dist < bestDist)
					finals.clear(), finals.push_back({ curcandidate, replacementArgs, dist }), bestDist = dist;

				else if(dist == bestDist)
					finals.push_back({ curcandidate, replacementArgs, dist });
			}



			if(finals.empty())
			{
				iceAssert(cands.size() == fails.size());
				std::vector<fir::Type*> tmp = util::map(cands[0].second, [](const FnCallArgument& p) -> auto { return p.value->type; });

				auto errs = OverloadError::make(SimpleError::make(callLoc, "no overload in call to '%s(%s)' amongst %zu %s",
					cands[0].first->id.name, fir::Type::typeListToString(tmp), fails.size(), util::plural("candidate", fails.size())));

				for(auto f : fails)
					errs->addCand(f.first, f.second);

				return { TCResult(errs), { } };
			}
			else if(finals.size() > 1)
			{
				// check if all of the targets we found are virtual, and that they belong to the same class.

				bool virt = true;
				fir::ClassType* self = 0;

				Defn* ret = std::get<0>(finals[0]);

				for(auto def : finals)
				{
					if(auto fd = dcast(sst::FunctionDefn, std::get<0>(def)); fd && fd->isVirtual)
					{
						iceAssert(fd->parentTypeForMethod);
						iceAssert(fd->parentTypeForMethod->isClassType());

						if(!self)
						{
							self = fd->parentTypeForMethod->toClassType();
						}
						else
						{
							// check if they're co/contra variant
							auto ty = fd->parentTypeForMethod->toClassType();

							//* here we're just checking that 'ty' and 'self' are part of the same class hierarchy -- we don't really care about the method
							//* that we resolve being at the lowest or highest level of that hierarchy.

							if(!ty->isInParentHierarchy(self) && !self->isInParentHierarchy(ty))
							{
								virt = false;
								break;
							}
						}
					}
					else
					{
						virt = false;
						break;
					}
				}

				if(virt)
				{
					return { TCResult(ret), std::get<1>(finals[0]) };
				}
				else
				{
					auto err = SimpleError::make(callLoc, "ambiguous call to function '%s', have %zu candidates:",
						cands[0].first->id.name, finals.size());

					for(auto f : finals)
					{
						err->append(SimpleError::make(MsgType::Note, std::get<0>(f)->loc, "possible target (overload distance %d):", std::get<2>(f)));
					}

					return { TCResult(err), { } };
				}
			}
			else
			{
				return { TCResult(std::get<0>(finals[0])), std::get<1>(finals[0]) };
			}
		}
	}
}
}





























