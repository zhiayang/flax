// collector.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <sys/stat.h>

#include <unordered_map>

#include "errors.h"
#include "codegen.h"
#include "frontend.h"
#include "typecheck.h"

namespace frontend
{
	void collectFiles(const std::string& _filename, CollectorState* state)
	{
		// first, collect and parse the first file
		std::string full = getFullPathOfFile(_filename);

		auto graph = new DependencyGraph();

		std::unordered_map<std::string, bool> visited;

		state->allFiles = checkForCycles(full, buildDependencyGraph(graph, full, visited));
		state->fullMainFile = full;
		state->graph = graph;


		// pre-lex everything.
		for(const auto& f : state->allFiles)
			frontend::getFileTokens(f);
	}


	void parseFiles(CollectorState* state)
	{
		// parse
		for(const auto& file : state->allFiles)
		{
			// parse it all
			auto opers = parser::parseOperators(frontend::getFileTokens(file));

			{
				// TODO: clean this up maybe.
				auto checkDupes = [](const std::unordered_map<std::string, parser::CustomOperatorDecl>& existing,
					const std::unordered_map<std::string, parser::CustomOperatorDecl>& newops, const std::string& kind) {

					for(const auto& op : newops)
					{
						if(auto it = existing.find(op.first); it != existing.end())
						{
							SimpleError::make(op.second.loc, "duplicate declaration for %s operator '%s'", kind, op.second.symbol)
								->append(SimpleError::make(MsgType::Note, it->second.loc, "previous declaration was here:"))
								->postAndQuit();
						}
					}
				};

				checkDupes(state->binaryOps, std::get<0>(opers), "infix");
				checkDupes(state->prefixOps, std::get<1>(opers), "prefix");
				checkDupes(state->postfixOps, std::get<2>(opers), "postfix");

				for(const auto& op : std::get<0>(opers))
					state->binaryOps[op.first] = op.second;

				for(const auto& op : std::get<1>(opers))
					state->prefixOps[op.first] = op.second;

				for(const auto& op : std::get<2>(opers))
					state->postfixOps[op.first] = op.second;
			}


			state->parsed[file] = parser::parseFile(file, *state);
		}
	}



	sst::DefinitionTree* typecheckFiles(CollectorState* state)
	{
		// typecheck
		for(const auto& file : state->allFiles)
		{
			// note that we're guaranteed (because that's the whole point)
			// that any module we encounter here will have had all of its dependencies checked already

			std::vector<std::pair<ImportThing, sst::StateTree*>> imports;
			for(auto d : state->graph->getDependenciesOf(file))
			{
				auto imported = d->to;

				auto stree = state->dtrees[imported->name]->base;
				iceAssert(stree);

				ImportThing ithing { imported->name, d->ithing.importAs, d->ithing.loc };
				if(auto it = std::find_if(imports.begin(), imports.end(), [&ithing](auto x) -> bool { return x.first.name == ithing.name; });
					it != imports.end())
				{
					SimpleError::make(ithing.loc, "importing previously imported module '%s'", ithing.name)
						->append(SimpleError::make(MsgType::Note, it->first.loc, "previous import was here:"))
						->postAndQuit();
				}

				imports.push_back({ ithing, stree });

				// debuglog("%s depends on %s\n", frontend::getFilenameFromPath(file).c_str(), frontend::getFilenameFromPath(d->name).c_str());
			}

			// debuglog("typecheck %s\n", file);
			state->dtrees[file] = sst::typecheck(state, state->parsed[file], imports);
		}

		return state->dtrees[state->fullMainFile];
	}


	fir::Module* generateFIRModule(CollectorState* state, sst::DefinitionTree* maintree)
	{
		iceAssert(maintree && maintree->topLevel);

		for(const auto& dt : state->dtrees)
		{
			for(const auto& def : dt.second->typeDefnMap)
			{
				if(auto it = maintree->typeDefnMap.find(def.first); it != maintree->typeDefnMap.end())
					iceAssert(it->second == def.second);

				maintree->typeDefnMap[def.first] = def.second;
			}
		}

		return cgn::codegen(maintree);
	}
}













































