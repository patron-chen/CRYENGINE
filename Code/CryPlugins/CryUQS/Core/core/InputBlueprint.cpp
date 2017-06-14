// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InputBlueprint.h"
#include "ItemSerializationSupport.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// CTextualInputBlueprint
		//
		//===================================================================================

		CTextualInputBlueprint::CTextualInputBlueprint()
		{
		}

		CTextualInputBlueprint::CTextualInputBlueprint(const char* paramName, const char* funcName, const char* funcReturnValueLiteral, const char* addReturnValueToDebugRenderWorldUponExecution)
			: m_paramName(paramName)
			, m_funcName(funcName)
			, m_funcReturnValueLiteral(funcReturnValueLiteral)
			, m_addReturnValueToDebugRenderWorldUponExecution(addReturnValueToDebugRenderWorldUponExecution)
		{
		}

		CTextualInputBlueprint::~CTextualInputBlueprint()
		{
			for(CTextualInputBlueprint* pChild : m_children)
			{
				delete pChild;
			}
		}

		const char* CTextualInputBlueprint::GetParamName() const
		{
			return m_paramName.c_str();
		}

		const char* CTextualInputBlueprint::GetFuncName() const
		{
			return m_funcName.c_str();
		}

		const char* CTextualInputBlueprint::GetFuncReturnValueLiteral() const
		{
			return m_funcReturnValueLiteral.c_str();
		}

		const char* CTextualInputBlueprint::GetAddReturnValueToDebugRenderWorldUponExecution() const
		{
			return m_addReturnValueToDebugRenderWorldUponExecution.c_str();
		}

		void CTextualInputBlueprint::SetParamName(const char* szParamName)
		{
			m_paramName = szParamName;
		}

		void CTextualInputBlueprint::SetFuncName(const char* szFuncName)
		{
			m_funcName = szFuncName;
		}

		void CTextualInputBlueprint::SetFuncReturnValueLiteral(const char* szValue)
		{
			m_funcReturnValueLiteral = szValue;
		}

		void CTextualInputBlueprint::SetAddReturnValueToDebugRenderWorldUponExecution(const char* szAddReturnValueToDebugRenderWorldUponExecution)
		{
			m_addReturnValueToDebugRenderWorldUponExecution = szAddReturnValueToDebugRenderWorldUponExecution;
		}

		ITextualInputBlueprint& CTextualInputBlueprint::AddChild(const char* paramName, const char* funcName, const char* funcReturnValueLiteral, const char* addReturnValueToDebugRenderWorldUponExecution)
		{
			CTextualInputBlueprint* b = new CTextualInputBlueprint(paramName, funcName, funcReturnValueLiteral, addReturnValueToDebugRenderWorldUponExecution);
			m_children.push_back(b);
			return *b;
		}

		size_t CTextualInputBlueprint::GetChildCount() const
		{
			return m_children.size();
		}

		const ITextualInputBlueprint& CTextualInputBlueprint::GetChild(size_t index) const
		{
			assert(index < m_children.size());
			return *m_children[index];
		}

		const ITextualInputBlueprint* CTextualInputBlueprint::FindChildByParamName(const char* paramName) const
		{
			for(const CTextualInputBlueprint* child : m_children)
			{
				if(strcmp(child->m_paramName.c_str(), paramName) == 0)
					return child;
			}
			return nullptr;
		}

		void CTextualInputBlueprint::SetSyntaxErrorCollector(datasource::SyntaxErrorCollectorUniquePtr ptr)
		{
			m_pSyntaxErrorCollector = std::move(ptr);
		}

		datasource::ISyntaxErrorCollector* CTextualInputBlueprint::GetSyntaxErrorCollector() const
		{
			return m_pSyntaxErrorCollector.get();
		}

		//===================================================================================
		//
		// CInputBlueprint
		//
		//===================================================================================

		CInputBlueprint::CInputBlueprint()
			: m_pFunctionFactory(nullptr)
			, m_bAddReturnValueToDebugRenderWorldUponExecution(false)
		{
		}

		CInputBlueprint::CInputBlueprint(client::IFunctionFactory& functionFactory, const char* functionReturnValueLiteral, bool bAddReturnValueToDebugRenderWorldUponExecution)
			: m_pFunctionFactory(&functionFactory)
			, m_functionReturnValueLiteral(functionReturnValueLiteral)
			, m_bAddReturnValueToDebugRenderWorldUponExecution(bAddReturnValueToDebugRenderWorldUponExecution)
		{
		}

		CInputBlueprint::~CInputBlueprint()
		{
			for(CInputBlueprint* b : m_children)
			{
				delete b;
			}
		}

		bool CInputBlueprint::Resolve(const ITextualInputBlueprint& sourceParent, const client::IInputParameterRegistry& inputParamsReg, const CQueryBlueprint& queryBlueprintForGlobalParamChecking, bool bResolvingForAGenerator)
		{
			bool bResolveSucceeded = true;

			const size_t numParamsExpected = inputParamsReg.GetParameterCount();
			const size_t numParamsProvided = sourceParent.GetChildCount();

			//
			// ensure the correct number of parameters has been provided
			//

			if (numParamsProvided != numParamsExpected)
			{
				if (datasource::ISyntaxErrorCollector* pSE = sourceParent.GetSyntaxErrorCollector())
				{
					pSE->AddErrorMessage("Incorrect number of parameters provided: expected %i, got %i", (int)numParamsExpected, (int)numParamsProvided);
				}
				bResolveSucceeded = false;
			}

			//
			// parse one parameter after another
			//

			for (size_t i = 0; i < numParamsExpected; ++i)
			{
				const client::IInputParameterRegistry::SParameterInfo& pi = inputParamsReg.GetParameter(i);

				//
				// look up the child by the name of the parameter it's being represented by
				//

				const ITextualInputBlueprint* pSourceChild = sourceParent.FindChildByParamName(pi.name);
				if (!pSourceChild)
				{
					if (datasource::ISyntaxErrorCollector* pSE = sourceParent.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Missing parameter: '%s'", pi.name);
					}
					bResolveSucceeded = false;
					continue;
				}

				CInputBlueprint *pNewChild = new CInputBlueprint;
				m_children.push_back(pNewChild);

				//
				// look up the function of that new child
				//

				const char* funcName = pSourceChild->GetFuncName();
				pNewChild->m_pFunctionFactory = g_hubImpl->GetFunctionFactoryDatabase().FindFactoryByName(funcName);
				if (!pNewChild->m_pFunctionFactory)
				{
					if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Unknown function: '%s'", funcName);
					}
					bResolveSucceeded = false;
					continue;   // without a function, we cannot continue parsing this child and also cannot go down deeper the call hierarchy
				}

				pNewChild->m_functionReturnValueLiteral = pSourceChild->GetFuncReturnValueLiteral();

				//
				// ensure that the function's return type matches the parameter's type
				//

				const shared::CTypeInfo& childReturnType = pNewChild->m_pFunctionFactory->GetReturnType();
				if (childReturnType != pi.type)
				{
					if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Parameter '%s' is of type '%s', but Function '%s' returns a '%s'", pi.name, pi.type.name(), funcName, childReturnType.name());
					}
					bResolveSucceeded = false;
				}

				//
				// see if the function is configured to output debug stuff into the debug-render-world every time it will be called at runtime
				//

				const char* szAddReturnValueToDebugRenderWorldUponExecution = pSourceChild->GetAddReturnValueToDebugRenderWorldUponExecution();
				if (strcmp(szAddReturnValueToDebugRenderWorldUponExecution, "true") == 0)
				{
					pNewChild->m_bAddReturnValueToDebugRenderWorldUponExecution = true;
				}
				else if (strcmp(szAddReturnValueToDebugRenderWorldUponExecution, "false") == 0)
				{
					pNewChild->m_bAddReturnValueToDebugRenderWorldUponExecution = false;
				}
				else
				{
					if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("The value of the property AddReturnValueToDebugRenderWorldUponExecution could not be parsed into a bool from its textual value: '%s' (only 'true' and 'false' are allowed)", szAddReturnValueToDebugRenderWorldUponExecution);
					}
					bResolveSucceeded = false;
				}

				//
				// - if this function returns the iterated item, ensure that the function is not part of a generator
				// - reason: at runtime, there's no iteration going on during the generator phase, only during the evaluator phase (of course)
				//

				if (bResolvingForAGenerator && pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == client::IFunctionFactory::ELeafFunctionKind::IteratedItem)
				{
					if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Generators cannot use functions that return the iterated item (this is only possible for evaluators)");
					}
					bResolveSucceeded = false;
				}

				//
				// if the function returns the iterated item, ensure that this item type matches the type of items to generate
				//

				if (pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == client::IFunctionFactory::ELeafFunctionKind::IteratedItem)
				{
					// just in case the generator had already problems getting resolved and we ended up without one so far...
					if (const CGeneratorBlueprint* pGeneratorBP = queryBlueprintForGlobalParamChecking.GetGeneratorBlueprint())
					{
						const shared::CTypeInfo& returnType = pNewChild->m_pFunctionFactory->GetReturnType();
						const shared::CTypeInfo& typeOfItemsToGenerate = pGeneratorBP->GetTypeOfItemsToGenerate();

						if (returnType != typeOfItemsToGenerate)
						{
							if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
							{
								pSE->AddErrorMessage("This function returns items of type '%s', which mismatches the type of items to generate: '%s'", returnType.name(), typeOfItemsToGenerate.name());
							}
							bResolveSucceeded = false;
						}
					}
				}

				//
				// if the function returns a global param (ELeafFunctionKind::GlobalParam), ensure that this global param exists and that its type matches
				//

				if (pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == client::IFunctionFactory::ELeafFunctionKind::GlobalParam)
				{
					const char* nameOfGlobalParam = pNewChild->GetFunctionReturnValueLiteral();
					const client::IItemFactory* pItemFactoryOfThatGlobalParam = nullptr;

					// search among the global constant-params
					{
						const CGlobalConstantParamsBlueprint& constantParamsBP = queryBlueprintForGlobalParamChecking.GetGlobalConstantParamsBlueprint();
						const shared::CVariantDict& params = constantParamsBP.GetParams();
						pItemFactoryOfThatGlobalParam = params.FindItemFactory(nameOfGlobalParam);	// may still be a nullptr
					}

					// search among the global runtime-params
					if(!pItemFactoryOfThatGlobalParam)
					{
						const CGlobalRuntimeParamsBlueprint& runtimeParamsBP = queryBlueprintForGlobalParamChecking.GetGlobalRuntimeParamsBlueprint();
						const std::map<string, client::IItemFactory*>& params = runtimeParamsBP.GetParams();
						auto it = params.find(nameOfGlobalParam);
						if (it != params.cend())
						{
							pItemFactoryOfThatGlobalParam = it->second;
						}
					}

					// if the global param exists, then check for type mismatches
					if (pItemFactoryOfThatGlobalParam)
					{
						const shared::CTypeInfo& typeOfGlobalParam = pItemFactoryOfThatGlobalParam->GetItemType();
						const shared::CTypeInfo& returnTypeOfFunction = pNewChild->m_pFunctionFactory->GetReturnType();

						// types mismatch?
						if (typeOfGlobalParam != returnTypeOfFunction)
						{
							if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
							{
								pSE->AddErrorMessage("Return type of function '%s' (%s) mismatches the type of the global param '%s' (%s)", pNewChild->m_pFunctionFactory->GetName(), returnTypeOfFunction.name(), nameOfGlobalParam, typeOfGlobalParam.name());
							}
							bResolveSucceeded = false;
						}
					}
					else
					{
						// the referenced global param doesn't exist -> syntax error
						if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Function '%s' returns an unknown global param: '%s'", pNewChild->m_pFunctionFactory->GetName(), nameOfGlobalParam);
						}
						bResolveSucceeded = false;
					}
				}

				//
				// if the function returns a literal (ELeafFunctionKind::Literal), then make sure the textual representation of that literal can be parsed
				//

				if (pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == client::IFunctionFactory::ELeafFunctionKind::Literal)
				{
					const shared::CTypeInfo& returnType = pNewChild->m_pFunctionFactory->GetReturnType();
					client::IItemFactory* pItemFactoryOfReturnType = g_hubImpl->GetUtils().FindItemFactoryByType(returnType);

					// notice: if no item-factory for the function's return type was found, then the function's return type hasn't been registered in the item-factory-database
					//         (in fact, the StartupConsistencyChecker should have detected this already, but still, we handle it gracefully here)
					if (pItemFactoryOfReturnType)
					{
						if (pItemFactoryOfReturnType->CanBePersistantlySerialized())
						{ 
							// try to deserialize the literal from its textual representation
							IItemSerializationSupport& itemSerializationSupport = uqs::core::IHubPlugin::GetHub().GetItemSerializationSupport();
							const char* textualRepresentationOfThatLiteral = pNewChild->m_functionReturnValueLiteral.c_str();
							shared::CVariantDict tempDict;
							shared::CUqsString errorMessage;
							shared::IUqsString* pErrorMessage = (pSourceChild->GetSyntaxErrorCollector()) ? &errorMessage : nullptr;
							if (!itemSerializationSupport.DeserializeItemIntoDictFromCStringLiteral(tempDict, "key_does_not_matter", *pItemFactoryOfReturnType, textualRepresentationOfThatLiteral, pErrorMessage))
							{
								if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
								{
									pSE->AddErrorMessage("Function '%s' returns a literal of type %s, but the literal could not be parsed from its archive representation: '%s'. Reason:\n%s", pNewChild->m_pFunctionFactory->GetName(), returnType.name(), textualRepresentationOfThatLiteral, errorMessage.c_str());
								}
								bResolveSucceeded = false;
							}
						}
						else
						{
							// - the type of the literal has no textual representation (e. g. it could be a pointer or some complex struct)
							// - in fact, we should never get here, unless a client has forcefully hacked around or accidentally
							//   registered a wrong function as ELeafFunctionKind::Literal
							if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
							{
								pSE->AddErrorMessage("Function '%s' is of kind ELeafFunctionKind::Literal but its return type (%s) cannot be represented in textual form", pNewChild->m_pFunctionFactory->GetName(), returnType.name());
							}
							bResolveSucceeded = false;
						}
					}
				}

				//
				// if the function returns the shuttled items (ELeafFunctionKind::ShuttledItems), then make sure that the query has been provided with an expected type of these items and that this type matches the function's return value
				//

				if (pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == client::IFunctionFactory::ELeafFunctionKind::ShuttledItems)
				{
					if (const shared::CTypeInfo* pExpectedShuttleType = queryBlueprintForGlobalParamChecking.GetExpectedShuttleType())
					{
						// notice: the type of the shuttle actually specifies the *container* type of items, hence we need access to the *contained* type
						const shared::CTypeInfo* pContainedType = pNewChild->m_pFunctionFactory->GetContainedType();

						// if this assert fails, then something must have become inconsistent between client::internal::CFunc_ShuttledItems<> and client::internal::SContainedTypeRetriever<>
						assert(pContainedType);

						if (*pContainedType != *pExpectedShuttleType)
						{
							if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
							{
								pSE->AddErrorMessage("Function '%s' is of kind ELeafFunctionKind::ShuttledItems and expects the shuttled items to be of type '%s', but they are actually of type '%s'", pNewChild->m_pFunctionFactory->GetName(), pExpectedShuttleType->name(), pContainedType->name());
							}
							bResolveSucceeded = false;
						}
					}
					else
					{
						if (datasource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Function '%s' is of kind ELeafFunctionKind::ShuttledItems, but the query has not specified what type the shuttled items are expected to be", pNewChild->m_pFunctionFactory->GetName());
						}
						bResolveSucceeded = false;
					}
				}

				//
				// recurse down the function call hierarchy
				//

				if (!pNewChild->Resolve(*pSourceChild, pNewChild->m_pFunctionFactory->GetInputParameterRegistry(), queryBlueprintForGlobalParamChecking, bResolvingForAGenerator))
				{
					bResolveSucceeded = false;
				}
			}

			return bResolveSucceeded;
		}

		size_t CInputBlueprint::GetChildCount() const
		{
			return m_children.size();
		}

		const CInputBlueprint& CInputBlueprint::GetChild(size_t index) const
		{
			assert(index < m_children.size());
			return *m_children[index];
		}

		client::IFunctionFactory* CInputBlueprint::GetFunctionFactory() const
		{
			return m_pFunctionFactory;
		}

		const char* CInputBlueprint::GetFunctionReturnValueLiteral() const
		{
			return m_functionReturnValueLiteral.c_str();
		}

		bool CInputBlueprint::GetAddReturnValueToDebugRenderWorldUponExecution() const
		{
			return m_bAddReturnValueToDebugRenderWorldUponExecution;
		}

	}
}
