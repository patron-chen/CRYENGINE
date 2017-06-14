// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryBlueprintLoader_XML.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace datasource_xml
	{

		static void DeleteErrorProvider(datasource::ISyntaxErrorCollector* pSyntaxErrorCollectorToDelete)
		{
			delete pSyntaxErrorCollectorToDelete;
		}

		static datasource::SyntaxErrorCollectorUniquePtr MakeNewSyntaxErrorCollectorUniquePtr(int lineNumber, const std::shared_ptr<CXMLDataErrorCollector>& dataErrorCollector)
		{
			return datasource::SyntaxErrorCollectorUniquePtr(new CSyntaxErrorCollector_XML(lineNumber, dataErrorCollector), datasource::CSyntaxErrorCollectorDeleter(&DeleteErrorProvider));
		}

		CQueryBlueprintLoader_XML::CQueryBlueprintLoader_XML(const char* queryName, const char* xmlFilePath, const std::shared_ptr<CXMLDataErrorCollector>& dataErrorCollector)
			: m_queryName(queryName)
			, m_xmlFilePath(xmlFilePath)
			, m_queryElement()
			, m_dataErrorCollector(dataErrorCollector)
			, m_query(nullptr)
		{
			// nothing
		}

		bool CQueryBlueprintLoader_XML::LoadTextualQueryBlueprint(core::ITextualQueryBlueprint& out, shared::IUqsString& error)
		{
			// load the XML file
			m_queryElement = gEnv->pSystem->LoadXmlFromFile(m_xmlFilePath.c_str());
			if (!m_queryElement)
			{
				// file not found or invalid xml
				error.Format("XML file '%s' not found or it contains invalid xml", m_xmlFilePath.c_str());
				return false;
			}

			// expect <Query> root node
			if (!m_queryElement->isTag("Query"))
			{
				error.Format("XML file '%s': root node must be <Query>, but is <%s>", m_xmlFilePath.c_str(), m_queryElement->getTag());
				return false;
			}

			bool success;

			m_query = &out;
			m_query->SetName(m_queryName.c_str());
			success = ParseQueryElement(m_queryElement, error);
			m_query = nullptr;

			return success;
		}

		bool CQueryBlueprintLoader_XML::ParseQueryElement(const XmlNodeRef& queryElement, shared::IUqsString& error)
		{
			assert(queryElement->isTag("Query"));

			m_query->SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(queryElement->getLine(), m_dataErrorCollector));

			// notice: the name of the query blueprint came in from somewhere outside (e. g. file name) and has already been set by LoadTextualQueryBlueprint()

			// "factory" attribute
			const char* factory = queryElement->getAttr("factory");
			m_query->SetQueryFactoryName(factory);

			// "maxItemsToKeepInResultSet" attribute
			const char* maxItemsToKeepInResultSet = queryElement->getAttr("maxItemsToKeepInResultSet");
			m_query->SetMaxItemsToKeepInResultSet(maxItemsToKeepInResultSet);

			// "expectedShuttleType" attribute (optional attribute)
			const char* expectedShuttleType;
			if (queryElement->getAttr("expectedShuttleType", &expectedShuttleType))
			{
				m_query->SetExpectedShuttleType(expectedShuttleType);
			}

			for (int i = 0; i < queryElement->getChildCount(); ++i)
			{
				XmlNodeRef child = queryElement->getChild(i);

				if (child->isTag("GlobalParams"))
				{
					if (!ParseGlobalParamsElement(child, error))
						return false;
				}
				else if (child->isTag("Generator"))
				{
					if (!ParseGeneratorElement(child, error))
						return false;
				}
				else if (child->isTag("InstantEvaluator"))
				{
					if (!ParseInstantEvaluatorElement(child, error))
						return false;
				}
				else if (child->isTag("DeferredEvaluator"))
				{
					if (!ParseDeferredEvaluatorElement(child, error))
						return false;
				}
				else if (child->isTag("Query"))
				{
					// child queries

					core::ITextualQueryBlueprint* parentQuery = m_query;
					m_query = &m_query->AddChild();
					const bool succeeded = ParseQueryElement(child, error);
					m_query = parentQuery;

					if (!succeeded)
						return false;
				}
				// TODO: <SecondaryGenerator> element(s) ?
				else
				{
					error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
					return false;
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseGlobalParamsElement(const XmlNodeRef& globalParamsElement, shared::IUqsString& error)
		{
			assert(globalParamsElement->isTag("GlobalParams"));

			// <ConstantParam> and <RuntimeParam> elements
			for (int i = 0; i < globalParamsElement->getChildCount(); ++i)
			{
				XmlNodeRef child = globalParamsElement->getChild(i);

				if (child->isTag("ConstantParam"))
				{
					// "name" attribute
					const char* name;
					if (!child->getAttr("name", &name))
					{
						error.Format("line #%i: <ConstantParam> is missing the attribute 'name'", child->getLine());
						return false;
					}

					// "type" attribute
					const char* type;
					if (!child->getAttr("type", &type))
					{
						error.Format("line #%i: <ConstantParam> is missing the attribute 'type'", child->getLine());
						return false;
					}

					// "value" attribute
					const char* value;
					if (!child->getAttr("value", &value))
					{
						error.Format("line #%i: <ConstantParam> is missing the attribute 'value'", child->getLine());
						return false;
					}

					m_query->GetGlobalConstantParams().AddParameter(name, type, value, MakeNewSyntaxErrorCollectorUniquePtr(child->getLine(), m_dataErrorCollector));
				}
				else if (child->isTag("RuntimeParam"))
				{
					// "name" attribute
					const char* name;
					if (!child->getAttr("name", &name))
					{
						error.Format("line #%i: <RuntimeParam> is missing the attribute 'name'", child->getLine());
						return false;
					}

					// "type" attribute
					const char* type;
					if (!child->getAttr("type", &type))
					{
						error.Format("line #%i: <RuntimeParam> is missing the attribute 'type'", child->getLine());
						return false;
					}

					m_query->GetGlobalRuntimeParams().AddParameter(name, type, MakeNewSyntaxErrorCollectorUniquePtr(child->getLine(), m_dataErrorCollector));
				}
				else
				{
					error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
					return false;
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseGeneratorElement(const XmlNodeRef& generatorElement, shared::IUqsString& error)
		{
			assert(generatorElement->isTag("Generator"));

			core::ITextualGeneratorBlueprint& textualGeneratorBP = m_query->SetGenerator();
			textualGeneratorBP.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(generatorElement->getLine(), m_dataErrorCollector));

			// "name" attribute
			const char* name;
			if (!generatorElement->getAttr("name", &name))
			{
				error.Format("line #%i: <Generator> is missing the attribute 'name'", generatorElement->getLine());
				return false;
			}
			textualGeneratorBP.SetGeneratorName(name);

			// parse <Input> elements (expect only these elements)
			core::ITextualInputBlueprint& textualInputRoot = textualGeneratorBP.GetInputRoot();
			textualInputRoot.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(generatorElement->getLine(), m_dataErrorCollector));	// same as its parent (the textual-generator-blueprint)
			for (int i = 0; i < generatorElement->getChildCount(); ++i)
			{
				XmlNodeRef child = generatorElement->getChild(i);

				if (child->isTag("Input"))
				{
					if (!ParseInputElement(child, textualInputRoot, error))
						return false;
				}
				else
				{
					error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
					return false;
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseInstantEvaluatorElement(const XmlNodeRef& instantEvaluatorElement, shared::IUqsString& error)
		{
			assert(instantEvaluatorElement->isTag("InstantEvaluator"));

			core::ITextualInstantEvaluatorBlueprint& textualInstantEvaluatorBP = m_query->AddInstantEvaluator();
			textualInstantEvaluatorBP.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(instantEvaluatorElement->getLine(), m_dataErrorCollector));

			// "name" attribute
			const char* evaluatorName;
			if (!instantEvaluatorElement->getAttr("name", &evaluatorName))
			{
				error.Format("line #%i: <InstantEvaluator> is missing the attribute 'name'", instantEvaluatorElement->getLine());
				return false;
			}
			textualInstantEvaluatorBP.SetEvaluatorName(evaluatorName);

			// "weight" attribute
			const char* weight;
			if (!instantEvaluatorElement->getAttr("weight", &weight))
			{
				error.Format("line #%i: <InstantEvaluator> is missing the attribute 'weight'", instantEvaluatorElement->getLine());
				return false;
			}
			textualInstantEvaluatorBP.SetWeight(weight);

			// parse <Input> elements (expect only these elements)
			core::ITextualInputBlueprint& textualInputRoot = textualInstantEvaluatorBP.GetInputRoot();
			textualInputRoot.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(instantEvaluatorElement->getLine(), m_dataErrorCollector));	// same as its parent (the textual-instant-evaluator-blueprint)
			for (int i = 0; i < instantEvaluatorElement->getChildCount(); ++i)
			{
				XmlNodeRef child = instantEvaluatorElement->getChild(i);

				if (child->isTag("Input"))
				{
					if (!ParseInputElement(child, textualInputRoot, error))
						return false;
				}
				else
				{
					error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
					return false;
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseDeferredEvaluatorElement(const XmlNodeRef& deferredEvaluatorElement, shared::IUqsString& error)
		{
			assert(deferredEvaluatorElement->isTag("DeferredEvaluator"));

			core::ITextualDeferredEvaluatorBlueprint& textualDeferredEvaluatorBP = m_query->AddDeferredEvaluator();
			textualDeferredEvaluatorBP.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(deferredEvaluatorElement->getLine(), m_dataErrorCollector));

			// "name" attribute
			const char* evaluatorName;
			if (!deferredEvaluatorElement->getAttr("name", &evaluatorName))
			{
				error.Format("line #%i: <DeferredEvaluator> is missing the attribute 'name'", deferredEvaluatorElement->getLine());
				return false;
			}
			textualDeferredEvaluatorBP.SetEvaluatorName(evaluatorName);

			// "weight" attribute
			const char* weight;
			if (!deferredEvaluatorElement->getAttr("weight", &weight))
			{
				error.Format("line #%i: <DeferredEvaluator> is missing the attribute 'weight'", deferredEvaluatorElement->getLine());
				return false;
			}
			textualDeferredEvaluatorBP.SetWeight(weight);

			// parse <Input> elements (expect only these elements)
			core::ITextualInputBlueprint& textualInputRoot = textualDeferredEvaluatorBP.GetInputRoot();
			textualInputRoot.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(deferredEvaluatorElement->getLine(), m_dataErrorCollector));	// same as its parent (the textual-deferred-evaluator-blueprint)
			for (int i = 0; i < deferredEvaluatorElement->getChildCount(); ++i)
			{
				XmlNodeRef child = deferredEvaluatorElement->getChild(i);

				if (child->isTag("Input"))
				{
					if (!ParseInputElement(child, textualInputRoot, error))
						return false;
				}
				else
				{
					error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
					return false;
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseFunctionElement(const XmlNodeRef& functionElement, core::ITextualInputBlueprint& parentInput, shared::IUqsString& error)
		{
			assert(functionElement->isTag("Function"));

			// whether the "returnValue" attribute is present specifies whether it's a leaf- or non-leaf function

			if (functionElement->haveAttr("returnValue"))
			{
				// leaf function
				// TODO: do something with that "returnValue" attribute
			}
			else
			{
				// non-leaf function => recurse further down (expect <Input> elements)
				for (int i = 0; i < functionElement->getChildCount(); ++i)
				{
					XmlNodeRef child = functionElement->getChild(i);

					if (child->isTag("Input"))
					{
						if (!ParseInputElement(child, parentInput, error))
							return false;
					}
					else
					{
						error.Format("line #%i: unknown tag: <%s>", child->getLine(), child->getTag());
						return false;
					}
				}
			}

			return true;
		}

		bool CQueryBlueprintLoader_XML::ParseInputElement(const XmlNodeRef& inputElement, core::ITextualInputBlueprint& parentInput, shared::IUqsString& error)
		{
			assert(inputElement->isTag("Input"));

			// "name" attribute (name of the parameter)
			const char* paramName;
			if (!inputElement->getAttr("name", &paramName))
			{
				error.Format("line #%i: <Input> is missing the attribute 'name'", inputElement->getLine());
				return false;
			}

			// <Function> element
			XmlNodeRef functionElement = inputElement->findChild("Function");
			if (!functionElement)
			{
				error.Format("line #%i: <Input> is missing the <Function> element", inputElement->getLine());
				return false;
			}

			// <Function>'s "name" attribute (name of the function)
			const char* functionName;
			if (!functionElement->getAttr("name", &functionName))
			{
				error.Format("line #%i: <Function> is missing the attribute 'name'", functionElement->getLine());
				return false;
			}
			const char* functionReturnValue = functionElement->getAttr("returnValue");   // optional attribute; defaults to ""
			const char* addReturnValueToDebugRenderWorldUponExecution = functionElement->getAttr("addReturnValueToDebugRenderWorldUponExecution"); // optional attribute; default is "false"
			if (addReturnValueToDebugRenderWorldUponExecution[0] == '\0')	// FIXME: detect properly whether this attribute was really missing (and only then default it to "false")
				addReturnValueToDebugRenderWorldUponExecution = "false";

			core::ITextualInputBlueprint& newChild = parentInput.AddChild(paramName, functionName, functionReturnValue, addReturnValueToDebugRenderWorldUponExecution);
			newChild.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(functionElement->getLine(), m_dataErrorCollector));

			return ParseFunctionElement(functionElement, newChild, error);
		}

	}
}
