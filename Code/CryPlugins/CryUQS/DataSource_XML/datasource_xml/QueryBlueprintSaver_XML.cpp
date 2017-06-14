// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryBlueprintSaver_XML.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace datasource_xml
	{

		CQueryBlueprintSaver_XML::CQueryBlueprintSaver_XML(const char* xmlFileNameToSaveTo)
			: m_xmlFileNameToSaveTo(xmlFileNameToSaveTo)
			, m_queryElementToSaveTo()
			, m_query(nullptr)
		{
			// nothing
		}

		bool CQueryBlueprintSaver_XML::SaveTextualQueryBlueprint(const core::ITextualQueryBlueprint& queryBlueprintToSave, shared::IUqsString& error)
		{
			m_queryElementToSaveTo = gEnv->pSystem->CreateXmlNode("Query");

			m_query = &queryBlueprintToSave;
			SaveQueryElement(m_queryElementToSaveTo);
			m_query = nullptr;

			if (m_queryElementToSaveTo->saveToFile(m_xmlFileNameToSaveTo.c_str()))
			{
				return true;
			}
			else
			{
				error.Format("Could not save the query-blueprint to file '%s' (file might be read-only)", m_xmlFileNameToSaveTo.c_str());
				return false;
			}
		}

		void CQueryBlueprintSaver_XML::SaveQueryElement(const XmlNodeRef& queryElementToSaveTo)
		{
			assert(queryElementToSaveTo->isTag("Query"));

			// notice: we don't save the name of the query blueprint here, as the caller has a better understanding of how to deal with that
			//         (e. g. the name might be that of the file in which the query blueprint is stored)

			// "factory" attribute
			queryElementToSaveTo->setAttr("factory", m_query->GetQueryFactoryName());

			// "maxItemsToKeepInResultSet" attribute
			queryElementToSaveTo->setAttr("maxItemsToKeepInResultSet", m_query->GetMaxItemsToKeepInResultSet());

			// "shuttleType" attribute
			if (const shared::IUqsString* shuttleType = m_query->GetExpectedShuttleType())
			{
				queryElementToSaveTo->setAttr("expectedShuttleType", shuttleType->c_str());
			}

			// <GlobalParams>
			SaveGlobalParamsElement(queryElementToSaveTo->newChild("GlobalParams"));

			// <Generator>
			if (m_query->GetGenerator() != nullptr)
			{
				SaveGeneratorElement(queryElementToSaveTo->newChild("Generator"));
			}

			// all <InstantEvaluator>s
			for (size_t i = 0; i < m_query->GetInstantEvaluatorCount(); ++i)
			{
				const core::ITextualInstantEvaluatorBlueprint& textualInstantEvaluatorBP = m_query->GetInstantEvaluator(i);
				XmlNodeRef instantEvaluatorElement = queryElementToSaveTo->newChild("InstantEvaluator");
				SaveInstantEvaluatorElement(instantEvaluatorElement, textualInstantEvaluatorBP);
			}

			// all <DeferredEvaluator>s
			for (size_t i = 0; i < m_query->GetDeferredEvaluatorCount(); ++i)
			{
				const core::ITextualDeferredEvaluatorBlueprint& textualDeferredEvaluatorBP = m_query->GetDeferredEvaluator(i);
				XmlNodeRef deferredEvaluatorElement = queryElementToSaveTo->newChild("DeferredEvaluator");
				SaveDeferredEvaluatorElement(deferredEvaluatorElement, textualDeferredEvaluatorBP);
			}

			// all child <Query>s
			const core::ITextualQueryBlueprint* parent = m_query;
			for (size_t i = 0; i < parent->GetChildCount(); ++i)
			{
				const core::ITextualQueryBlueprint& childQueryBP = parent->GetChild(i);
				m_query = &childQueryBP;
				XmlNodeRef childQueryElement = queryElementToSaveTo->newChild("Query");
				SaveQueryElement(childQueryElement);
			}
			m_query = parent;

			// TODO: secondary-generator(s)
		}

		void CQueryBlueprintSaver_XML::SaveGlobalParamsElement(const XmlNodeRef& globalParamsElementToSaveTo)
		{
			assert(globalParamsElementToSaveTo->isTag("GlobalParams"));

			// <ConstantParam>s
			{
				const core::ITextualGlobalConstantParamsBlueprint& constantParamsBP = m_query->GetGlobalConstantParams();
				for (size_t i = 0; i < constantParamsBP.GetParameterCount(); ++i)
				{
					const core::ITextualGlobalConstantParamsBlueprint::SParameterInfo pi = constantParamsBP.GetParameter(i);
					XmlNodeRef constantParamElement = globalParamsElementToSaveTo->newChild("ConstantParam");
					constantParamElement->setAttr("name", pi.name);
					constantParamElement->setAttr("type", pi.type);
					constantParamElement->setAttr("value", pi.value);
				}
			}

			// <RuntimeParam>s
			{
				const core::ITextualGlobalRuntimeParamsBlueprint& runtimeParamsBP = m_query->GetGlobalRuntimeParams();
				for (size_t i = 0; i < runtimeParamsBP.GetParameterCount(); ++i)
				{
					const core::ITextualGlobalRuntimeParamsBlueprint::SParameterInfo pi = runtimeParamsBP.GetParameter(i);
					XmlNodeRef runtimeParamElement = globalParamsElementToSaveTo->newChild("RuntimeParam");
					runtimeParamElement->setAttr("name", pi.name);
					runtimeParamElement->setAttr("type", pi.type);
				}
			}
		}

		void CQueryBlueprintSaver_XML::SaveGeneratorElement(const XmlNodeRef& generatorElementToSaveTo)
		{
			assert(generatorElementToSaveTo->isTag("Generator"));
			assert(m_query->GetGenerator() != nullptr);

			const core::ITextualGeneratorBlueprint* pGeneratorBP = m_query->GetGenerator();

			// "name"
			generatorElementToSaveTo->setAttr("name", pGeneratorBP->GetGeneratorName());

			// all <Input>s
			{
				const core::ITextualInputBlueprint& inputRootBP = pGeneratorBP->GetInputRoot();
				for (size_t i = 0; i < inputRootBP.GetChildCount(); ++i)
				{
					const core::ITextualInputBlueprint& inputBP = inputRootBP.GetChild(i);
					XmlNodeRef inputElement = generatorElementToSaveTo->newChild("Input");
					SaveInputElement(inputElement, inputBP);
				}
			}
		}

		void CQueryBlueprintSaver_XML::SaveInstantEvaluatorElement(const XmlNodeRef& instantEvaluatorElementToSaveTo, const core::ITextualInstantEvaluatorBlueprint& instantEvaluatorBP)
		{
			assert(instantEvaluatorElementToSaveTo->isTag("InstantEvaluator"));

			// "name" attribute
			instantEvaluatorElementToSaveTo->setAttr("name", instantEvaluatorBP.GetEvaluatorName());

			// "weight" attribute
			instantEvaluatorElementToSaveTo->setAttr("weight", instantEvaluatorBP.GetWeight());

			// all <Input>s
			{
				const core::ITextualInputBlueprint& inputRootBP = instantEvaluatorBP.GetInputRoot();
				for (size_t i = 0; i < inputRootBP.GetChildCount(); ++i)
				{
					const core::ITextualInputBlueprint& inputBP = inputRootBP.GetChild(i);
					XmlNodeRef inputElement = instantEvaluatorElementToSaveTo->newChild("Input");
					SaveInputElement(inputElement, inputBP);
				}
			}
		}

		void CQueryBlueprintSaver_XML::SaveDeferredEvaluatorElement(const XmlNodeRef& deferredEvaluatorElementToSaveTo, const core::ITextualDeferredEvaluatorBlueprint& deferredEvaluatorBP)
		{
			assert(deferredEvaluatorElementToSaveTo->isTag("DeferredEvaluator"));

			// "name" attribute
			deferredEvaluatorElementToSaveTo->setAttr("name", deferredEvaluatorBP.GetEvaluatorName());

			// "weight" attribute
			deferredEvaluatorElementToSaveTo->setAttr("weight", deferredEvaluatorBP.GetWeight());

			// all <Input>s
			{
				const core::ITextualInputBlueprint& inputRootBP = deferredEvaluatorBP.GetInputRoot();
				for (size_t i = 0; i < inputRootBP.GetChildCount(); ++i)
				{
					const core::ITextualInputBlueprint& inputBP = inputRootBP.GetChild(i);
					XmlNodeRef inputElement = deferredEvaluatorElementToSaveTo->newChild("Input");
					SaveInputElement(inputElement, inputBP);
				}
			}
		}

		void CQueryBlueprintSaver_XML::SaveFunctionElement(const XmlNodeRef& functionElementToSaveTo, const core::ITextualInputBlueprint& parentInput)
		{
			assert(functionElementToSaveTo->isTag("Function"));

			// <Function>'s "name" attribute (name of the function)
			functionElementToSaveTo->setAttr("name", parentInput.GetFuncName());

			// <Function>'s "addReturnValueToDebugRenderWorldUponExecution" attribute ("true" / "false")
			functionElementToSaveTo->setAttr("addReturnValueToDebugRenderWorldUponExecution", parentInput.GetAddReturnValueToDebugRenderWorldUponExecution());

			// <Function>'s "returnValue" attribute (only if we are a leaf-function!)
			if (parentInput.GetChildCount() == 0)
			{
				functionElementToSaveTo->setAttr("returnValue", parentInput.GetFuncReturnValueLiteral());
			}
			else
			{
				// all <Input>s (we're a non-leaf function!)
				for (size_t i = 0; i < parentInput.GetChildCount(); ++i)
				{
					const core::ITextualInputBlueprint& inputBP = parentInput.GetChild(i);
					XmlNodeRef inputElement = functionElementToSaveTo->newChild("Input");
					SaveInputElement(inputElement, inputBP);
				}
			}
		}

		void CQueryBlueprintSaver_XML::SaveInputElement(const XmlNodeRef& inputElementToSaveTo, const core::ITextualInputBlueprint& inputBP)
		{
			assert(inputElementToSaveTo->isTag("Input"));

			// "name" attribute (name of the parameter)
			inputElementToSaveTo->setAttr("name", inputBP.GetParamName());

			// <Function> (value of the parameter)
			XmlNodeRef functionElement = inputElementToSaveTo->newChild("Function");

			SaveFunctionElement(functionElement, inputBP);
		}

	}
}
