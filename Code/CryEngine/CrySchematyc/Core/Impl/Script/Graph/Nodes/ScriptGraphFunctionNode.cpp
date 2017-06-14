// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphFunctionNode.h"

#include <CrySerialization/Decorators/ActionButton.h>
#include <Schematyc/Compiler/CompilerContext.h>
#include <Schematyc/Compiler/IGraphNodeCompiler.h>
#include <Schematyc/Env/IEnvRegistry.h>
#include <Schematyc/Env/Elements/IEnvComponent.h>
#include <Schematyc/Env/Elements/IEnvFunction.h>
#include <Schematyc/Script/IScriptRegistry.h>
#include <Schematyc/Script/Elements/IScriptComponentInstance.h>
#include <Schematyc/Script/Elements/IScriptFunction.h>
#include <Schematyc/Utils/Any.h>
#include <Schematyc/Utils/IGUIDRemapper.h>
#include <Schematyc/Utils/Properties.h>
#include <Schematyc/Utils/StackString.h>

#include "Object.h"
#include "Runtime/RuntimeClass.h"
#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"
#include "SerializationUtils/SerializationContext.h"

namespace Schematyc
{
CScriptGraphFunctionNode::SEnvGlobalFunctionRuntimeData::SEnvGlobalFunctionRuntimeData(const IEnvFunction* _pEnvFunction)
	: pEnvFunction(_pEnvFunction)
{}

CScriptGraphFunctionNode::SEnvGlobalFunctionRuntimeData::SEnvGlobalFunctionRuntimeData(const SEnvGlobalFunctionRuntimeData& rhs)
	: pEnvFunction(rhs.pEnvFunction)
{}

SGUID CScriptGraphFunctionNode::SEnvGlobalFunctionRuntimeData::ReflectSchematycType(CTypeInfo<CScriptGraphFunctionNode::SEnvGlobalFunctionRuntimeData>& typeInfo)
{
	return "90c48655-4a34-49cc-a618-44ae349c9c7b"_schematyc_guid;
}

CScriptGraphFunctionNode::SEnvComponentFunctionRuntimeData::SEnvComponentFunctionRuntimeData(const IEnvFunction* _pEnvFunction, uint32 _componentIdx)
	: pEnvFunction(_pEnvFunction)
	, componentIdx(_componentIdx)
{}

CScriptGraphFunctionNode::SEnvComponentFunctionRuntimeData::SEnvComponentFunctionRuntimeData(const SEnvComponentFunctionRuntimeData& rhs)
	: pEnvFunction(rhs.pEnvFunction)
	, componentIdx(rhs.componentIdx)
{}

SGUID CScriptGraphFunctionNode::SEnvComponentFunctionRuntimeData::ReflectSchematycType(CTypeInfo<CScriptGraphFunctionNode::SEnvComponentFunctionRuntimeData>& typeInfo)
{
	return "205a9972-3dc7-4d20-97f6-a322ae2d9e37"_schematyc_guid;
}

CScriptGraphFunctionNode::SScriptFunctionRuntimeData::SScriptFunctionRuntimeData(uint32 _functionIdx)
	: functionIdx(_functionIdx)
{}

CScriptGraphFunctionNode::SScriptFunctionRuntimeData::SScriptFunctionRuntimeData(const SScriptFunctionRuntimeData& rhs)
	: functionIdx(rhs.functionIdx)
{}

SGUID CScriptGraphFunctionNode::SScriptFunctionRuntimeData::ReflectSchematycType(CTypeInfo<CScriptGraphFunctionNode::SScriptFunctionRuntimeData>& typeInfo)
{
	return "e049b617-7e1e-4f61-aefc-b827e5d353f5"_schematyc_guid;
}

CScriptGraphFunctionNode::CScriptGraphFunctionNode() {}

CScriptGraphFunctionNode::CScriptGraphFunctionNode(const SElementId& functionId, const SGUID& objectGUID)
	: m_functionId(functionId)
	, m_objectGUID(objectGUID)
{}

SGUID CScriptGraphFunctionNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphFunctionNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetStyleId("Core::Function");

	stack_string subject;

	const IScriptElement* pScriptObject = gEnv->pSchematyc->GetScriptRegistry().GetElement(m_objectGUID);
	if (pScriptObject)
	{
		subject = pScriptObject->GetName();
		subject.append("::");
	}

	if (!GUID::IsEmpty(m_functionId.guid))
	{
		layout.AddInput("In", SGUID(), { EScriptGraphPortFlags::Flow, EScriptGraphPortFlags::MultiLink });
		layout.AddOutput("Out", SGUID(), EScriptGraphPortFlags::Flow);

		IEnvRegistry& envRegistry = gEnv->pSchematyc->GetEnvRegistry();
		switch (m_functionId.domain)
		{
		case EDomain::Env:
			{
				const IEnvFunction* pEnvFunction = envRegistry.GetFunction(m_functionId.guid); // #SchematycTODO : Should we be using a script view to retrieve this?
				if (pEnvFunction)
				{
					subject.append(pEnvFunction->GetName());

					CreateInputsAndOutputs(layout, *pEnvFunction);
				}
				break;
			}
		case EDomain::Script:
			{
				const IScriptElement* pScriptElement = gEnv->pSchematyc->GetScriptRegistry().GetElement(m_functionId.guid); // #SchematycTODO : Should we be using a script view to retrieve this?
				if (pScriptElement && (pScriptElement->GetElementType() == EScriptElementType::Function))
				{
					subject.append(pScriptElement->GetName());

					CreateInputsAndOutputs(layout, DynamicCast<IScriptFunction>(*pScriptElement));
				}
				break;
			}
		}
	}

	layout.SetName(nullptr, subject.c_str());
}

void CScriptGraphFunctionNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	CRuntimeClass* pClass = context.interfaces.Query<CRuntimeClass>();
	if (pClass)
	{
		if (!GUID::IsEmpty(m_functionId.guid))
		{
			switch (m_functionId.domain)
			{
			case EDomain::Env:
				{
					const IEnvFunction* pEnvFunction = gEnv->pSchematyc->GetEnvRegistry().GetFunction(m_functionId.guid);
					if (pEnvFunction)
					{
						if (GUID::IsEmpty(m_objectGUID))
						{
							if (!pEnvFunction->GetFlags().Check(EEnvFunctionFlags::Member))
							{
								compiler.BindCallback(&ExecuteEnvGlobalFunction);
								compiler.BindData(SEnvGlobalFunctionRuntimeData(pEnvFunction));
							}
							else
							{
								SCHEMATYC_COMPILER_ERROR("Unable to find object on which to call function!");
							}
						}
						else
						{
							const IScriptElement* pScriptObject = gEnv->pSchematyc->GetScriptRegistry().GetElement(m_objectGUID);
							if (pScriptObject)
							{
								switch (pScriptObject->GetElementType())
								{
								case EScriptElementType::ComponentInstance:
									{
										const IScriptComponentInstance& scriptComponentInstance = DynamicCast<IScriptComponentInstance>(*pScriptObject);
										if (scriptComponentInstance.GetTypeGUID() == pEnvFunction->GetObjectTypeInfo()->GetGUID()) // #SchematycTODO : Check type info before dereferencing?
										{
											compiler.BindCallback(&ExecuteEnvComponentFunction);

											const uint32 componentIdx = pClass->FindComponentInstance(m_objectGUID);
											compiler.BindData(SEnvComponentFunctionRuntimeData(pEnvFunction, componentIdx));
										}
										else
										{
											SCHEMATYC_COMPILER_ERROR("Unable to find object on which to call function!");
										}
										break;
									}
								}
							}
							else
							{
								SCHEMATYC_COMPILER_ERROR("Unable to find object on which to call function!");
							}
						}
					}
					else
					{
						SCHEMATYC_COMPILER_ERROR("Unable to find environment function!");
					}
					break;
				}
			case EDomain::Script:
				{
					const IScriptFunction* pScriptFunction = DynamicCast<IScriptFunction>(gEnv->pSchematyc->GetScriptRegistry().GetElement(m_functionId.guid));
					if (pScriptFunction)
					{
						const IScriptGraph* pScriptGraph = pScriptFunction->GetExtensions().QueryExtension<const IScriptGraph>();
						if (pScriptGraph)
						{
							context.tasks.CompileGraph(*pScriptGraph);
							compiler.BindCallback(&ExecuteScriptFunction);

							const uint32 functionIdx = pClass->FindOrReserveFunction(pScriptFunction->GetGUID());
							compiler.BindData(SScriptFunctionRuntimeData(functionIdx));
						}
						else
						{
							SCHEMATYC_COMPILER_ERROR("Unable to find script graph!");
						}
					}
					else
					{
						SCHEMATYC_COMPILER_ERROR("Unable to find script function!");
					}
					break;
				}
			}
		}
	}
}

void CScriptGraphFunctionNode::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_functionId, "functionId");
	archive(m_objectGUID, "objectGUID");
}

void CScriptGraphFunctionNode::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_functionId, "functionId");
	archive(m_objectGUID, "objectGUID");
}

void CScriptGraphFunctionNode::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	if (m_functionId.domain == EDomain::Script)
	{
		archive(Serialization::ActionButton(functor(*this, &CScriptGraphFunctionNode::GoToFunction)), "goToFunction", "^Go To Function");
	}

	Validate(archive, context);
}

void CScriptGraphFunctionNode::Validate(Serialization::IArchive& archive, const ISerializationContext& context)
{
	if (!GUID::IsEmpty(m_functionId.guid))
	{
		switch (m_functionId.domain)
		{
		case EDomain::Env:
			{
				const IEnvFunction* pEnvFunction = gEnv->pSchematyc->GetEnvRegistry().GetFunction(m_functionId.guid);
				if (pEnvFunction)
				{
					if (pEnvFunction->GetElementFlags().Check(EEnvElementFlags::Deprecated))
					{
						archive.warning(*this, "Function is deprecated!");
					}
				}
				else
				{
					archive.error(*this, "Unable to retrieve environment function!");
				}
				break;
			}
		}
	}
}

void CScriptGraphFunctionNode::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	if (m_functionId.domain == EDomain::Script)
	{
		m_functionId.guid = guidRemapper.Remap(m_functionId.guid);
	}
	m_objectGUID = guidRemapper.Remap(m_objectGUID);
}

void CScriptGraphFunctionNode::Register(CScriptGraphNodeFactory& factory)
{
	class CCreator : public IScriptGraphNodeCreator
	{
	private:

		class CCreationCommand : public IScriptGraphNodeCreationCommand
		{
		public:

			CCreationCommand(const char* szSubject, const char* szDescription, const SElementId& functionId, const SGUID& objectGUID = SGUID())
				: m_subject(szSubject)
				, m_description(szDescription)
				, m_functionId(functionId)
				, m_objectGUID(objectGUID)
			{}

			// IScriptGraphNodeCreationCommand

			virtual const char* GetBehavior() const override
			{
				return "Function";
			}

			virtual const char* GetSubject() const override
			{
				return m_subject.c_str();
			}

			virtual const char* GetDescription() const override
			{
				return m_description.c_str();
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::Function";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphFunctionNode>(m_functionId, m_objectGUID), pos);
			}

			// ~IScriptGraphNodeCreationCommand

		private:

			string     m_subject;
			string     m_description;
			SElementId m_functionId;
			SGUID      m_objectGUID;
		};

	public:

		// IScriptGraphNodeCreator

		virtual SGUID GetTypeGUID() const override
		{
			return CScriptGraphFunctionNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const SGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphFunctionNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override
		{
			struct SObject
			{
				inline SObject(const SGUID& _guid, const SGUID& _typeGUID, const char* szName)
					: guid(_guid)
					, typeGUID(_typeGUID)
					, name(szName)
				{}

				SGUID  guid;
				SGUID  typeGUID;
				string name;
			};

			const EScriptGraphType graphType = graph.GetType();
			if (graphType == EScriptGraphType::Transition)
			{
				return;
			}

			std::vector<SObject> objects;
			objects.reserve(20);

			auto visitScriptComponentInstance = [&scriptView, &objects](const IScriptComponentInstance& scriptComponentInstance) -> EVisitStatus
			{
				CStackString name;
				scriptView.QualifyName(scriptComponentInstance, EDomainQualifier::Global, name);
				objects.emplace_back(scriptComponentInstance.GetGUID(), scriptComponentInstance.GetTypeGUID(), name.c_str());
				return EVisitStatus::Continue;
			};
			scriptView.VisitScriptComponentInstances(ScriptComponentInstanceConstVisitor::FromLambda(visitScriptComponentInstance), EDomainScope::Derived);

			auto visitEnvFunction = [&nodeCreationMenu, &scriptView, graphType, &objects](const IEnvFunction& envFunction) -> EVisitStatus
			{
				if (envFunction.GetElementFlags().Check(EEnvElementFlags::Deprecated))
				{
					return EVisitStatus::Continue;
				}

				// #SchematycTODO : Create utility functions to determine which nodes are callable from which graphs?

				if ((graphType == EScriptGraphType::Construction) && !envFunction.GetFlags().Check(EEnvFunctionFlags::Construction))
				{
					return EVisitStatus::Continue;
				}

				if (envFunction.GetFlags().Check(EEnvFunctionFlags::Member))
				{
					const SGUID objectTypeGUID = envFunction.GetObjectTypeInfo()->GetGUID();
					for (SObject& object : objects)
					{
						if (object.typeGUID == objectTypeGUID)
						{
							CStackString subject = object.name.c_str();
							subject.append("::");
							subject.append(envFunction.GetName());
							nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(subject.c_str(), envFunction.GetDescription(), SElementId(EDomain::Env, envFunction.GetGUID()), object.guid));
						}
					}
				}
				else
				{
					CStackString subject;
					scriptView.QualifyName(envFunction, subject);
					nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(subject.c_str(), envFunction.GetDescription(), SElementId(EDomain::Env, envFunction.GetGUID())));
				}
				return EVisitStatus::Continue;
			};
			gEnv->pSchematyc->GetEnvRegistry().VisitFunctions(EnvFunctionConstVisitor::FromLambda(visitEnvFunction));

			if (graphType == EScriptGraphType::Construction)
			{
				return;
			}

			auto visitScriptFunction = [&nodeCreationMenu, &scriptView](const IScriptFunction& scriptFunction) -> EVisitStatus
			{
				CStackString subject;
				scriptView.QualifyName(scriptFunction, EDomainQualifier::Global, subject);
				nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(subject.c_str(), scriptFunction.GetDescription(), SElementId(EDomain::Script, scriptFunction.GetGUID())));
				return EVisitStatus::Continue;
			};
			scriptView.VisitScriptFunctions(ScriptFunctionConstVisitor::FromLambda(visitScriptFunction));
		}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

void CScriptGraphFunctionNode::CreateInputsAndOutputs(CScriptGraphNodeLayout& layout, const IEnvFunction& envFunction)
{
	for (uint32 inputIdx = 0, inputCount = envFunction.GetInputCount(); inputIdx < inputCount; ++inputIdx)
	{
		CAnyConstPtr pData = envFunction.GetInputData(inputIdx);
		if (pData)
		{
			layout.AddInputWithData(CGraphPortId::FromUniqueId(envFunction.GetInputId(inputIdx)), envFunction.GetInputName(inputIdx), pData->GetTypeInfo().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::Persistent, EScriptGraphPortFlags::Editable }, *pData);
		}
	}

	for (uint32 outputIdx = 0, outputCount = envFunction.GetOutputCount(); outputIdx < outputCount; ++outputIdx)
	{
		CAnyConstPtr pData = envFunction.GetOutputData(outputIdx);
		if (pData)
		{
			layout.AddOutputWithData(CGraphPortId::FromUniqueId(envFunction.GetOutputId(outputIdx)), envFunction.GetOutputName(outputIdx), pData->GetTypeInfo().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink }, *pData);
		}
	}
}

void CScriptGraphFunctionNode::CreateInputsAndOutputs(CScriptGraphNodeLayout& layout, const IScriptFunction& scriptFunction)
{
	for (uint32 inputIdx = 0, inputCount = scriptFunction.GetInputCount(); inputIdx < inputCount; ++inputIdx)
	{
		CAnyConstPtr pData = scriptFunction.GetInputData(inputIdx);
		if (pData)
		{
			layout.AddInputWithData(CGraphPortId::FromGUID(scriptFunction.GetInputGUID(inputIdx)), scriptFunction.GetInputName(inputIdx), pData->GetTypeInfo().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::Persistent, EScriptGraphPortFlags::Editable }, *pData);
		}
	}

	for (uint32 outputIdx = 0, outputCount = scriptFunction.GetOutputCount(); outputIdx < outputCount; ++outputIdx)
	{
		CAnyConstPtr pData = scriptFunction.GetOutputData(outputIdx);
		if (pData)
		{
			layout.AddOutputWithData(CGraphPortId::FromGUID(scriptFunction.GetInputGUID(outputIdx)), scriptFunction.GetOutputName(outputIdx), pData->GetTypeInfo().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink }, *pData);
		}
	}
}

void CScriptGraphFunctionNode::GoToFunction()
{
	CryLinkUtils::ExecuteCommand(CryLinkUtils::ECommand::Show, m_functionId.guid);
}

SRuntimeResult CScriptGraphFunctionNode::ExecuteEnvGlobalFunction(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	SEnvGlobalFunctionRuntimeData& data = DynamicCast<SEnvGlobalFunctionRuntimeData>(*context.node.GetData());

	data.pEnvFunction->Execute(context, nullptr);

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

SRuntimeResult CScriptGraphFunctionNode::ExecuteEnvComponentFunction(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	SEnvComponentFunctionRuntimeData& data = DynamicCast<SEnvComponentFunctionRuntimeData>(*context.node.GetData());
	CComponent* pEnvComponent = static_cast<CObject*>(context.pObject)->GetComponent(data.componentIdx);  // #SchematycTODO : How can we ensure this pointer is correct for the implementation, not just the interface?

	data.pEnvFunction->Execute(context, pEnvComponent);

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

SRuntimeResult CScriptGraphFunctionNode::ExecuteScriptFunction(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	SScriptFunctionRuntimeData& data = DynamicCast<SScriptFunctionRuntimeData>(*context.node.GetData());

	CRuntimeParams params;
	for (uint8 inputIdx = EInputIdx::FirstParam, inputCount = context.node.GetInputCount(); inputIdx < inputCount; ++inputIdx)
	{
		if (context.node.IsDataInput(inputIdx))
		{
			params.SetInput(inputIdx - EInputIdx::FirstParam, *context.node.GetInputData(inputIdx));
		}
	}

	static_cast<CObject*>(context.pObject)->ExecuteFunction(data.functionIdx, params);

	for (uint8 outputIdx = EOutputIdx::FirstParam, outputCount = context.node.GetOutputCount(); outputIdx < outputCount; ++outputIdx)
	{
		if (context.node.IsDataOutput(outputIdx))
		{
			CAnyConstPtr pSrcValue = params.GetOutput(outputIdx - EOutputIdx::FirstParam);
			if (pSrcValue)
			{
				Any::CopyAssign(*context.node.GetOutputData(outputIdx), *pSrcValue);
			}
		}
	}

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

const SGUID CScriptGraphFunctionNode::ms_typeGUID = "1bcfd811-b8b7-4032-a90c-311dfa4454c6"_schematyc_guid;
} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphFunctionNode::Register)
