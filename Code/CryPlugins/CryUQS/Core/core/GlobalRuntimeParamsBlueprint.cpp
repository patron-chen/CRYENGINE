// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GlobalRuntimeParamsBlueprint.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// CTextualGlobalRuntimeParamsBlueprint
		//
		//===================================================================================

		CTextualGlobalRuntimeParamsBlueprint::CTextualGlobalRuntimeParamsBlueprint()
		{
			// nothing
		}

		void CTextualGlobalRuntimeParamsBlueprint::AddParameter(const char* name, const char* type, datasource::SyntaxErrorCollectorUniquePtr syntaxErrorCollector)
		{
			m_parameters.emplace_back(name, type, std::move(syntaxErrorCollector));
		}

		size_t CTextualGlobalRuntimeParamsBlueprint::GetParameterCount() const
		{
			return m_parameters.size();
		}

		ITextualGlobalRuntimeParamsBlueprint::SParameterInfo CTextualGlobalRuntimeParamsBlueprint::GetParameter(size_t index) const
		{
			assert(index < m_parameters.size());
			const SStoredParameterInfo& pi = m_parameters[index];
			return SParameterInfo(pi.name.c_str(), pi.type.c_str(), pi.pSyntaxErrorCollector.get());
		}

		//===================================================================================
		//
		// CGlobalRuntimeParamsBlueprint
		//
		//===================================================================================

		CGlobalRuntimeParamsBlueprint::CGlobalRuntimeParamsBlueprint()
		{
			// nothing
		}

		bool CGlobalRuntimeParamsBlueprint::Resolve(const ITextualGlobalRuntimeParamsBlueprint& source, const CQueryBlueprint* pParentQueryBlueprint)
		{
			bool bResolveSucceeded = true;

			//
			// - parse the runtime-params from given source
			// - we do this before inheriting from given parent because we wanna detect duplicates in the source, and not mistake an inherited from the parent for being a duplicate
			//

			const size_t numParams = source.GetParameterCount();

			for (size_t i = 0; i < numParams; ++i)
			{
				const ITextualGlobalRuntimeParamsBlueprint::SParameterInfo p = source.GetParameter(i);

				// ensure each parameter exists only once
				if (m_runtimeParameters.find(p.name) != m_runtimeParameters.cend())
				{
					if (datasource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
					{
						pSE->AddErrorMessage("Duplicate parameter: '%s'", p.name);
					}
					bResolveSucceeded = false;
					continue;
				}

				// find the item factory
				client::IItemFactory* pItemFactory = g_hubImpl->GetItemFactoryDatabase().FindFactoryByName(p.type);
				if (!pItemFactory)
				{
					if (datasource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
					{
						pSE->AddErrorMessage("Unknown item type: '%s'", p.type);
					}
					bResolveSucceeded = false;
					continue;
				}

				// - if the same parameter exists already in a parent query, ensure that both have the same data type (name clashes are fine, but type clashes are not!)
				// - we do these checks not only for the user's convenience to notify him of mistakes while he's editing (at runtime, the issue would get detected anyway, as one
				//   particular runtime-parameter can only have one particular type), but also for CQueryBlueprint::VisitRuntimeParams() to guarantee that "duplicate" parameters
				//   are still of the same item type
				if (pParentQueryBlueprint)
				{
					/* an example:

						Parent query:
							runtime param "A": int
							runtime param "B": float

						Child query:
							runtime param "A": int
							runtime param "B": Vec3

						=> A is fine, but B is not as the types differ in the queries
					*/

					if (const client::IItemFactory* pParentItemFactory = FindItemFactoryByParamNameInParentRecursively(p.name, *pParentQueryBlueprint))
					{
						// name clash detected (this is totally fine, though)
						// -> now check for type clash
						if (pParentItemFactory != pItemFactory)
						{
							// type clash detected
							if (datasource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
							{
								pSE->AddErrorMessage("Type mismatch: expected '%s' (since the parent's parameter is of that type), but got a '%s'", pParentItemFactory->GetItemType().name(), pItemFactory->GetItemType().name());
							}
							bResolveSucceeded = false;
							continue;
						}
					}
				}

				m_runtimeParameters[p.name] = pItemFactory;
			}

			return bResolveSucceeded;
		}

		const std::map<string, client::IItemFactory*>& CGlobalRuntimeParamsBlueprint::GetParams() const
		{
			return m_runtimeParameters;
		}

		void CGlobalRuntimeParamsBlueprint::PrintToConsole(CLogger& logger) const
		{
			if (m_runtimeParameters.empty())
			{
				logger.Printf("Global runtime params: (none)");
			}
			else
			{
				logger.Printf("Global runtime params:");
				CLoggerIndentation _indent;
				for (const auto& entry : m_runtimeParameters)
				{
					const char* paramName = entry.first.c_str();
					const client::IItemFactory* pItemFactory = entry.second;
					logger.Printf("\"%s\" [%s]", paramName, pItemFactory->GetName());
				}
			}
		}

		const client::IItemFactory* CGlobalRuntimeParamsBlueprint::FindItemFactoryByParamNameInParentRecursively(const char* paramNameToSearchFor, const CQueryBlueprint& parentalQueryBlueprint) const
		{
			const CGlobalRuntimeParamsBlueprint& parentGlobalRuntimeParamsBP = parentalQueryBlueprint.GetGlobalRuntimeParamsBlueprint();
			const std::map<string, client::IItemFactory*>& params = parentGlobalRuntimeParamsBP.GetParams();
			auto it = params.find(paramNameToSearchFor);

			if (it != params.end())
			{
				return it->second;
			}

			if (const CQueryBlueprint* pGrandParent = parentalQueryBlueprint.GetParent())
			{
				// recurse up
				return pGrandParent->GetGlobalRuntimeParamsBlueprint().FindItemFactoryByParamNameInParentRecursively(paramNameToSearchFor, *pGrandParent);
			}

			return nullptr;
		}

	}
}
