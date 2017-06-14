// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryBlueprint.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// CTextualQueryBlueprint
		//
		//===================================================================================

		CTextualQueryBlueprint::CTextualQueryBlueprint()
			: m_bExpectedShuttleTypeHasBeenProvided(false)
		{
			// nothing
		}

		CTextualQueryBlueprint::~CTextualQueryBlueprint()
		{
			for (CTextualInstantEvaluatorBlueprint* pIE : m_instantEvaluators)
			{
				delete pIE;
			}

			for (CTextualDeferredEvaluatorBlueprint* pDE : m_deferredEvaluators)
			{
				delete pDE;
			}

			for (CTextualQueryBlueprint* pChild : m_children)
			{
				delete pChild;
			}
		}

		void CTextualQueryBlueprint::SetName(const char* name)
		{
			m_name = name;
		}

		void CTextualQueryBlueprint::SetQueryFactoryName(const char* factoryName)
		{
			m_queryFactoryName = factoryName;
		}

		void CTextualQueryBlueprint::SetMaxItemsToKeepInResultSet(const char* maxItems)
		{
			m_maxItemsToKeepInResultSet = maxItems;
		}

		void CTextualQueryBlueprint::SetExpectedShuttleType(const char* shuttleTypeName)
		{
			m_expectedShuttleType.Format("%s", shuttleTypeName);
			m_bExpectedShuttleTypeHasBeenProvided = true;
		}

		ITextualGlobalConstantParamsBlueprint& CTextualQueryBlueprint::GetGlobalConstantParams()
		{
			return m_globalConstantParams;
		}

		ITextualGlobalRuntimeParamsBlueprint& CTextualQueryBlueprint::GetGlobalRuntimeParams()
		{
			return m_globalRuntimeParams;
		}

		ITextualGeneratorBlueprint& CTextualQueryBlueprint::SetGenerator()
		{
			m_pGenerator.reset(new CTextualGeneratorBlueprint);
			return *m_pGenerator;
		}

		ITextualInstantEvaluatorBlueprint& CTextualQueryBlueprint::AddInstantEvaluator()
		{
			CTextualInstantEvaluatorBlueprint* pNewInstantEvaluatorBP = new CTextualInstantEvaluatorBlueprint;
			m_instantEvaluators.push_back(pNewInstantEvaluatorBP);
			return *pNewInstantEvaluatorBP;
		}

		size_t CTextualQueryBlueprint::GetInstantEvaluatorCount() const
		{
			return m_instantEvaluators.size();
		}

		ITextualDeferredEvaluatorBlueprint& CTextualQueryBlueprint::AddDeferredEvaluator()
		{
			CTextualDeferredEvaluatorBlueprint* pNewDeferredEvaluatorBlueprint = new CTextualDeferredEvaluatorBlueprint;
			m_deferredEvaluators.push_back(pNewDeferredEvaluatorBlueprint);
			return *pNewDeferredEvaluatorBlueprint;
		}

		ITextualQueryBlueprint& CTextualQueryBlueprint::AddChild()
		{
			CTextualQueryBlueprint* pNewChild = new CTextualQueryBlueprint;
			m_children.push_back(pNewChild);
			return *pNewChild;
		}

		size_t CTextualQueryBlueprint::GetDeferredEvaluatorCount() const
		{
			return m_deferredEvaluators.size();
		}

		const char* CTextualQueryBlueprint::GetName() const
		{
			return m_name.c_str();
		}

		const char* CTextualQueryBlueprint::GetQueryFactoryName() const
		{
			return m_queryFactoryName.c_str();
		}

		const char* CTextualQueryBlueprint::GetMaxItemsToKeepInResultSet() const
		{
			return m_maxItemsToKeepInResultSet.c_str();
		}

		const shared::IUqsString* CTextualQueryBlueprint::GetExpectedShuttleType() const
		{
			return m_bExpectedShuttleTypeHasBeenProvided ? &m_expectedShuttleType : nullptr;
		}

		const ITextualGlobalConstantParamsBlueprint& CTextualQueryBlueprint::GetGlobalConstantParams() const
		{
			return m_globalConstantParams;
		}

		const ITextualGlobalRuntimeParamsBlueprint& CTextualQueryBlueprint::GetGlobalRuntimeParams() const
		{
			return m_globalRuntimeParams;
		}

		const ITextualGeneratorBlueprint* CTextualQueryBlueprint::GetGenerator() const
		{
			return m_pGenerator.get();
		}

		const ITextualInstantEvaluatorBlueprint& CTextualQueryBlueprint::GetInstantEvaluator(size_t index) const
		{
			assert(index < m_instantEvaluators.size());
			return *m_instantEvaluators[index];
		}

		const ITextualDeferredEvaluatorBlueprint& CTextualQueryBlueprint::GetDeferredEvaluator(size_t index) const
		{
			assert(index < m_deferredEvaluators.size());
			return *m_deferredEvaluators[index];
		}

		size_t CTextualQueryBlueprint::GetChildCount() const
		{
			return m_children.size();
		}

		const ITextualQueryBlueprint& CTextualQueryBlueprint::GetChild(size_t index) const
		{
			assert(index < m_children.size());
			return *m_children[index];
		}

		void CTextualQueryBlueprint::SetSyntaxErrorCollector(datasource::SyntaxErrorCollectorUniquePtr ptr)
		{
			m_pSyntaxErrorCollector = std::move(ptr);
		}

		datasource::ISyntaxErrorCollector* CTextualQueryBlueprint::GetSyntaxErrorCollector() const
		{
			return m_pSyntaxErrorCollector.get();
		}

		//===================================================================================
		//
		// CQueryBlueprint
		//
		//===================================================================================

		CQueryBlueprint::CQueryBlueprint()
			: m_pQueryFactory(nullptr)
			, m_maxItemsToKeepInResultSet(0)
			, m_pExpectedShuttleType(nullptr)
			, m_pParent(nullptr)
		{
			// nothing
		}

		CQueryBlueprint::~CQueryBlueprint()
		{
			for (CInstantEvaluatorBlueprint* ie : m_instantEvaluators)
			{
				delete ie;
			}

			for (CDeferredEvaluatorBlueprint* de : m_deferredEvaluators)
			{
				delete de;
			}
		}

		const char* CQueryBlueprint::GetName() const
		{
			return m_name.c_str();
		}

		void CQueryBlueprint::VisitRuntimeParams(client::IQueryBlueprintRuntimeParamVisitor& visitor) const
		{
			std::map<string, client::IItemFactory*> allRuntimeParamsInTheHierarchy;

			GrabRuntimeParamsRecursively(allRuntimeParamsInTheHierarchy);

			for (const auto& pair : allRuntimeParamsInTheHierarchy)
			{
				const char* paramName = pair.first.c_str();
				client::IItemFactory* pItemFactory = pair.second;
				assert(pItemFactory);
				visitor.OnRuntimeParamVisited(paramName, *pItemFactory);
			}
		}

		const shared::CTypeInfo& CQueryBlueprint::GetOutputType() const
		{
			assert(m_pQueryFactory);
			return m_pQueryFactory->GetQueryBlueprintType(*this);
		}

		bool CQueryBlueprint::Resolve(const ITextualQueryBlueprint& source)
		{
			bool bResolveSucceeded = true;

			// name
			m_name = source.GetName();

			// query factory
			{
				const char* queryFactoryName = source.GetQueryFactoryName();
				m_pQueryFactory = static_cast<CQueryFactoryBase*>(g_hubImpl->GetQueryFactoryDatabase().FindFactoryByName(queryFactoryName));  // the static_cast<> is kinda ok'ish here, since IQueryFactory and its derived class CQueryFactoryBase are _both_ defined in the core, so we definitely know about the inheritance hierarchy
				if (!m_pQueryFactory)
				{
					if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Query factory with name '%s' not found", queryFactoryName);
					}
					bResolveSucceeded = false;
				}
			}

			// max. items to keep in result set
			{
				const char* maxItemsAsString = source.GetMaxItemsToKeepInResultSet();
				if (sscanf(maxItemsAsString, "%d", &m_maxItemsToKeepInResultSet) != 1)	// using %d to allow only decimal representation (no hex or octal)
				{
					if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("The maximum number of items could not be parsed into an int (value = '%s')", maxItemsAsString);
					}
					bResolveSucceeded = false;
				}
			}

			// shuttle type (optional)
			if (const shared::IUqsString* pShuttleTypeAsString = source.GetExpectedShuttleType())
			{
				if (const client::IItemFactory* pItemFactory = g_hubImpl->GetItemFactoryDatabase().FindFactoryByName(pShuttleTypeAsString->c_str()))
				{
					m_pExpectedShuttleType = &pItemFactory->GetItemType();
				}
				else
				{
					if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Unknown item type for the shuttled items: '%s'", pShuttleTypeAsString->c_str());
					}
					bResolveSucceeded = false;
				}
			}

			// ensure no duplicates between constant-params and runtime-params
			for (size_t i1 = 0; i1 < source.GetGlobalConstantParams().GetParameterCount(); ++i1)
			{
				for (size_t i2 = 0; i2 < source.GetGlobalRuntimeParams().GetParameterCount(); ++i2)
				{
					const CTextualGlobalConstantParamsBlueprint::SParameterInfo p1 = source.GetGlobalConstantParams().GetParameter(i1);
					const CTextualGlobalRuntimeParamsBlueprint::SParameterInfo p2 = source.GetGlobalRuntimeParams().GetParameter(i2);

					if (strcmp(p1.name, p2.name) == 0)
					{
						// output error to constant-params
						if (datasource::ISyntaxErrorCollector* pSE = p1.pSyntaxErrorCollector)
						{
							pSE->AddErrorMessage("Global constant-parameter clashes with runtime-parameter of the same name: '%s'", p1.name);
						}

						// output error to runtime-params
						if (datasource::ISyntaxErrorCollector* pSE = p2.pSyntaxErrorCollector)
						{
							pSE->AddErrorMessage("Global runtime-parameter clashes with constant-parameter of the same name: '%s'", p2.name);
						}

						bResolveSucceeded = false;
					}
				}
			}

			// global constant-params
			if (!m_globalConstantParams.Resolve(source.GetGlobalConstantParams()))
			{
				bResolveSucceeded = false;
			}

			// global runtime-params
			if (!m_globalRuntimeParams.Resolve(source.GetGlobalRuntimeParams(), m_pParent))
			{
				bResolveSucceeded = false;
			}

			// generator (a generator is optional and typically never exists in composite queries)
			if (const ITextualGeneratorBlueprint* pGeneratorBlueprint = source.GetGenerator())
			{
				m_pGenerator.reset(new CGeneratorBlueprint);
				if (!m_pGenerator->Resolve(*pGeneratorBlueprint, *this))
				{
					m_pGenerator.release();    // nullify the generator so that CInputBlueprint::Resolve() won't be tempted to use this half-baked object for further checks (and crash becuase it might be lacking a generator-factory)
					bResolveSucceeded = false;
				}
			}

			// instant-evaluators
			for (size_t i = 0; i < source.GetInstantEvaluatorCount(); ++i)
			{
				CInstantEvaluatorBlueprint* pNewInstantEvaluatorBP = new CInstantEvaluatorBlueprint;
				m_instantEvaluators.push_back(pNewInstantEvaluatorBP);
				if (!pNewInstantEvaluatorBP->Resolve(source.GetInstantEvaluator(i), *this))
				{
					bResolveSucceeded = false;
				}
			}

			// deferred-evaluators
			for (size_t i = 0; i < source.GetDeferredEvaluatorCount(); ++i)
			{
				CDeferredEvaluatorBlueprint* pNewDeferredEvaluatorBP = new CDeferredEvaluatorBlueprint;
				m_deferredEvaluators.push_back(pNewDeferredEvaluatorBP);
				if (!pNewDeferredEvaluatorBP->Resolve(source.GetDeferredEvaluator(i), *this))
				{
					bResolveSucceeded = false;
				}
			}

			//
			// - ensure that the max. number of instant- and deferred-evaluators is not exceeded
			// - otherwise, CQuery cannot keep track of all evaluator's states as a bitfield during query execution
			//

			{
				const size_t numInstantEvaluators = m_instantEvaluators.size();
				if (numInstantEvaluators > UQS_MAX_EVALUATORS)
				{
					if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Exceeded the maximum number of instant-evaluators in the query blueprint (max %i supported, %i present in the blueprint)", UQS_MAX_EVALUATORS, (int)numInstantEvaluators);
					}
					bResolveSucceeded = false;
				}
			}

			{
				const size_t numDeferredEvaluators = m_deferredEvaluators.size();
				if (numDeferredEvaluators > UQS_MAX_EVALUATORS)
				{
					if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Exceeded the maximum number of deferred-evaluators in the query blueprint (max %i supported, %i present in the blueprint)", UQS_MAX_EVALUATORS, (int)numDeferredEvaluators);
					}
					bResolveSucceeded = false;
				}
			}

			// children (recurse down)
			for (size_t i = 0, n = source.GetChildCount(); i < n; ++i)
			{
				const ITextualQueryBlueprint& childSource = source.GetChild(i);
				std::shared_ptr<CQueryBlueprint> pChildTarget(new CQueryBlueprint);
				pChildTarget->m_pParent = this;
				m_children.push_back(pChildTarget);
				if (!pChildTarget->Resolve(childSource))
					bResolveSucceeded = false;
			}

			// if the query-factory expects to have a generator in the blueprint, then ensure that one was provided (and the other way around)
			if (m_pQueryFactory)  // might be a nullptr in case of unknown query factory (c. f. code further above)
			{
				if (m_pQueryFactory->RequiresGenerator() && !m_pGenerator)
				{
					if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Query factories of type '%s' require a generator, but none is provided", m_pQueryFactory->GetName());
					}
					bResolveSucceeded = false;
				}
				else if (!m_pQueryFactory->RequiresGenerator() && m_pGenerator)
				{
					if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Query factories of type '%s' don't require a generator, but one was provided", m_pQueryFactory->GetName());
					}
					bResolveSucceeded = false;
				}
			}

			// validate the min/max number of child queries
			if (m_pQueryFactory)  // might be a nullptr in case of unknown query factory (c. f. code further above)
			{
				const size_t actualNumChildQueries = m_children.size();

				// min required child queries
				{
					const size_t minRequiredChildQueries = m_pQueryFactory->GetMinRequiredChildren();
					if (actualNumChildQueries < minRequiredChildQueries)
					{
						if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Too few child queries: %i (expected at least %i)", (int)actualNumChildQueries, (int)minRequiredChildQueries);
						}
						bResolveSucceeded = false;
					}
				}

				// max allowed child queries
				{
					const size_t maxAllowedChildQueries = m_pQueryFactory->GetMaxAllowedChildren();
					if (actualNumChildQueries > maxAllowedChildQueries)
					{
						if (datasource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Too many child queries: %i (expected no more than %i)", (int)actualNumChildQueries, (int)maxAllowedChildQueries);
						}
						bResolveSucceeded = false;
					}
				}
			}

			// have the query-factory check that if it deals with child-queries that their output types are compatible among each other
			// note: we do this only if the blueprint could be resolved successfully so far (it's too cumbersome to deal with potential null-pointers in all related code)
			if (bResolveSucceeded)
			{
				string error;
				size_t indexOfChildCausingTheError = 0;

				if (!m_pQueryFactory->CheckOutputTypeCompatibilityAmongChildQueryBlueprints(*this, error, indexOfChildCausingTheError))
				{
					// write the error message into the child that caused the error
					const ITextualQueryBlueprint& childSource = source.GetChild(indexOfChildCausingTheError);
					if (datasource::ISyntaxErrorCollector* pSE = childSource.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("%s", error.c_str());
					}
					bResolveSucceeded = false;
				}
			}

			return bResolveSucceeded;
		}

		const CQueryBlueprint* CQueryBlueprint::GetParent() const
		{
			return m_pParent;
		}

		size_t CQueryBlueprint::GetChildCount() const
		{
			return m_children.size();
		}

		std::shared_ptr<const CQueryBlueprint> CQueryBlueprint::GetChild(size_t index) const
		{
			assert(index < m_children.size());
			return m_children[index];
		}

		QueryBaseUniquePtr CQueryBlueprint::CreateQuery(const CQueryBase::SCtorContext& ctorContext) const
		{
			assert(m_pQueryFactory);
			return m_pQueryFactory->CreateQuery(ctorContext);
		}

		int CQueryBlueprint::GetMaxItemsToKeepInResultSet() const
		{
			return m_maxItemsToKeepInResultSet;
		}

		const shared::CTypeInfo* CQueryBlueprint::GetExpectedShuttleType() const
		{
			return m_pExpectedShuttleType;
		}

		const CGlobalConstantParamsBlueprint& CQueryBlueprint::GetGlobalConstantParamsBlueprint() const
		{
			return m_globalConstantParams;
		}

		const CGlobalRuntimeParamsBlueprint& CQueryBlueprint::GetGlobalRuntimeParamsBlueprint() const
		{
			return m_globalRuntimeParams;
		}

		const CGeneratorBlueprint* CQueryBlueprint::GetGeneratorBlueprint() const
		{
			return m_pGenerator.get();
		}

		const std::vector<CInstantEvaluatorBlueprint*>& CQueryBlueprint::GetInstantEvaluatorBlueprints() const
		{
			return m_instantEvaluators;
		}

		const std::vector<CDeferredEvaluatorBlueprint*>& CQueryBlueprint::GetDeferredEvaluatorBlueprints() const
		{
			return m_deferredEvaluators;
		}

		bool CQueryBlueprint::CheckPresenceAndTypeOfGlobalRuntimeParamsRecursively(const shared::IVariantDict& runtimeParamsToValidate, shared::CUqsString& error) const
		{
			//
			// ensure that all required runtime-params have been passed in and that their data types match those in this query-blueprint
			//

			const std::map<string, client::IItemFactory*>& expectedRuntimeParams = m_globalRuntimeParams.GetParams();

			for (const auto& pair : expectedRuntimeParams)
			{
				const char* key = pair.first.c_str();
				const client::IItemFactory* pFoundItemFactory = runtimeParamsToValidate.FindItemFactory(key);

				if (pFoundItemFactory == nullptr)
				{
					error.Format("Missing runtime-param: '%s'", key);
					return false;
				}

				const client::IItemFactory* pExpectedItemFactory = pair.second;
				assert(pExpectedItemFactory);

				if (pFoundItemFactory != pExpectedItemFactory)
				{
					error.Format("Runtime-param '%s' mismatches the item-factory: expected '%s', but received '%s'", key, pExpectedItemFactory->GetName(), pFoundItemFactory->GetName());
					return false;
				}
			}

			// recurse down
			for (const std::shared_ptr<CQueryBlueprint>& pChild : m_children)
			{
				if (!pChild->CheckPresenceAndTypeOfGlobalRuntimeParamsRecursively(runtimeParamsToValidate, error))
				{
					return false;
				}
			}

			return true;
		}

		void CQueryBlueprint::PrintToConsole(CLogger& logger) const
		{
			//
			// name + output type + max items to generate
			//

			const shared::CTypeInfo& outputType = GetOutputType();
			logger.Printf("\"%s\" [%s] (max items in result set = %i)", m_name.c_str(), outputType.name(), m_maxItemsToKeepInResultSet);
			CLoggerIndentation _indent;

			//
			// factory
			//

			assert(m_pQueryFactory);
			logger.Printf("Query factory = %s", m_pQueryFactory->GetName());

			//
			// global constant params
			//

			m_globalConstantParams.PrintToConsole(logger);

			//
			// global runtime params
			//

			m_globalRuntimeParams.PrintToConsole(logger);

			//
			// generator (optional - typically never used by composite queries)
			//

			if (m_pGenerator)
			{
				m_pGenerator->PrintToConsole(logger);
			}
			else
			{
				logger.Printf("Generator: (none)");
			}

			//
			// instant-evaluators
			//

			if (m_instantEvaluators.empty())
			{
				logger.Printf("Instant Evaluators: (none)");
			}
			else
			{
				logger.Printf("Instant Evaluators:");
				CLoggerIndentation _indent;
				for (size_t i = 0, n = m_instantEvaluators.size(); i < n; ++i)
				{
					stack_string prefix;
					prefix.Format("#%i: ", (int)i);
					m_instantEvaluators[i]->PrintToConsole(logger, prefix.c_str());
				}
			}

			//
			// deferred-evaluators
			//

			if (m_deferredEvaluators.empty())
			{
				logger.Printf("Deferred Evaluators: (none)");
			}
			else
			{
				logger.Printf("Deferred Evaluators:");
				CLoggerIndentation _indent;
				for (size_t i = 0, n = m_deferredEvaluators.size(); i < n; ++i)
				{
					stack_string prefix;
					prefix.Format("#%i: ", (int)i);
					m_deferredEvaluators[i]->PrintToConsole(logger, prefix.c_str());
				}
			}

			//
			// children
			//

			if (m_children.empty())
			{
				logger.Printf("Children: (none)");
			}
			else
			{
				logger.Printf("Children: %i", (int)m_children.size());
				for (size_t i = 0, n = m_children.size(); i < n; ++i)
				{
					logger.Printf("Child %i:", (int)i);
					CLoggerIndentation _indent;
					m_children[i]->PrintToConsole(logger);
				}
			}
		}

		void CQueryBlueprint::GrabRuntimeParamsRecursively(std::map<string, client::IItemFactory*>& out) const
		{
			const std::map<string, client::IItemFactory*>& params = m_globalRuntimeParams.GetParams();

			for (const auto& pair : params)
			{
				const char* paramName = pair.first.c_str();
				client::IItemFactory* pItemFactory = pair.second;
				assert(pItemFactory);
				out[paramName] = pItemFactory;	// potentially overwrite the param from a parent; we presume here that both have the same item type (this is ensured by CGlobalRuntimeParamsBlueprint::Resolve())
			}

			// recurse down all children
			for (const std::shared_ptr<CQueryBlueprint>& pChild : m_children)
			{
				pChild->GrabRuntimeParamsRecursively(out);
			}
		}

	}
}
