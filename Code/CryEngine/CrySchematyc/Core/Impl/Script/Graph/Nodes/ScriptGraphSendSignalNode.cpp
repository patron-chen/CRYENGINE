// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphSendSignalNode.h"

#include <CrySerialization/Decorators/ActionButton.h>
#include <Schematyc/IObject.h>
#include <Schematyc/Compiler/CompilerContext.h>
#include <Schematyc/Compiler/IGraphNodeCompiler.h>
#include <Schematyc/Env/IEnvRegistry.h>
#include <Schematyc/Script/IScriptRegistry.h>
#include <Schematyc/Script/Elements/IScriptSignal.h>
#include <Schematyc/Utils/Any.h>
#include <Schematyc/Utils/IGUIDRemapper.h>
#include <Schematyc/Utils/StackString.h>

#include "Runtime/RuntimeClass.h"
#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"
#include "SerializationUtils/SerializationContext.h"

SERIALIZATION_ENUM_BEGIN_NESTED2(Schematyc, CScriptGraphSendSignalNode, ETarget, "Schematyc Script Graph Send Signal Node Target")
SERIALIZATION_ENUM(Schematyc::CScriptGraphSendSignalNode::ETarget::Self, "Self", "Send To Self")
SERIALIZATION_ENUM(Schematyc::CScriptGraphSendSignalNode::ETarget::Object, "Object", "Send To Object")
SERIALIZATION_ENUM(Schematyc::CScriptGraphSendSignalNode::ETarget::Broadcast, "Broadcast", "Broadcast To All Objects")
SERIALIZATION_ENUM_END()

namespace Schematyc
{
CScriptGraphSendSignalNode::SRuntimeData::SRuntimeData(const SGUID& _signalGUID)
	: signalGUID(_signalGUID)
{}

CScriptGraphSendSignalNode::SRuntimeData::SRuntimeData(const SRuntimeData& rhs)
	: signalGUID(rhs.signalGUID)
{}

SGUID CScriptGraphSendSignalNode::SRuntimeData::ReflectSchematycType(CTypeInfo<CScriptGraphSendSignalNode::SRuntimeData>& typeInfo)
{
	return "a88a4c08-22df-493b-ab27-973a893acefb"_schematyc_guid;
}

CScriptGraphSendSignalNode::CScriptGraphSendSignalNode()
	: m_target(ETarget::Self)
{}

CScriptGraphSendSignalNode::CScriptGraphSendSignalNode(const SGUID& signalGUID)
	: m_signalGUID(signalGUID)
	, m_target(ETarget::Self)
{}

SGUID CScriptGraphSendSignalNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphSendSignalNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetStyleId("Core::SendSignal");

	const char* szSubject = nullptr;
	if (!GUID::IsEmpty(m_signalGUID))
	{
		layout.AddInput("In", SGUID(), { EScriptGraphPortFlags::Flow, EScriptGraphPortFlags::MultiLink });
		layout.AddOutput("Out", SGUID(), EScriptGraphPortFlags::Flow);

		if (m_target == ETarget::Object)
		{
			layout.AddInputWithData("ObjectId", GetTypeInfo<ObjectId>().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::Persistent, EScriptGraphPortFlags::Editable }, ObjectId());
		}

		const IScriptSignal* pScriptSignal = DynamicCast<IScriptSignal>(gEnv->pSchematyc->GetScriptRegistry().GetElement(m_signalGUID));
		if (pScriptSignal)
		{
			szSubject = pScriptSignal->GetName();

			for (uint32 inputIdx = 0, inputCount = pScriptSignal->GetInputCount(); inputIdx < inputCount; ++inputIdx)
			{
				CAnyConstPtr pData = pScriptSignal->GetInputData(inputIdx);
				if (pData)
				{
					layout.AddInputWithData(CGraphPortId::FromGUID(pScriptSignal->GetInputGUID(inputIdx)), pScriptSignal->GetInputName(inputIdx), pScriptSignal->GetInputTypeId(inputIdx).guid, { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::Persistent, EScriptGraphPortFlags::Editable }, *pData);
				}
			}
		}
	}
	layout.SetName(nullptr, szSubject);
}

void CScriptGraphSendSignalNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	CRuntimeClass* pClass = context.interfaces.Query<CRuntimeClass>();
	if (pClass)
	{
		if (!GUID::IsEmpty(m_signalGUID))
		{
			const IScriptSignal* pScriptSignal = DynamicCast<IScriptSignal>(gEnv->pSchematyc->GetScriptRegistry().GetElement(m_signalGUID));
			if (pScriptSignal)
			{
				switch (m_target)
				{
				case ETarget::Self:
					{
						compiler.BindCallback(&ExecuteSendToSelf);
						break;
					}
				case ETarget::Object:
					{
						compiler.BindCallback(&ExecuteSendToObject);
						break;
					}
				case ETarget::Broadcast:
					{
						compiler.BindCallback(&ExecuteBroadcast);
						break;
					}
				}

				compiler.BindData(SRuntimeData(m_signalGUID));
			}
		}
	}
}

void CScriptGraphSendSignalNode::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_signalGUID, "signalGUID");
	archive(m_target, "target");
}

void CScriptGraphSendSignalNode::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_signalGUID, "signalGUID");
	archive(m_target, "target");
}

void CScriptGraphSendSignalNode::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_target, "target", "Target");

	archive(Serialization::ActionButton(functor(*this, &CScriptGraphSendSignalNode::GoToSignal)), "goToSignal", "^Go To Signal");

	Validate(archive, context);
}

void CScriptGraphSendSignalNode::Validate(Serialization::IArchive& archive, const ISerializationContext& context)
{
	if (!GUID::IsEmpty(m_signalGUID))
	{
		const IScriptSignal* pScriptSignal = DynamicCast<IScriptSignal>(gEnv->pSchematyc->GetScriptRegistry().GetElement(m_signalGUID));
		if (!pScriptSignal)
		{
			archive.error(*this, "Unable to retrieve script signal!");
		}
	}
}

void CScriptGraphSendSignalNode::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	m_signalGUID = guidRemapper.Remap(m_signalGUID);
}

void CScriptGraphSendSignalNode::Register(CScriptGraphNodeFactory& factory)
{
	class CCreator : public IScriptGraphNodeCreator
	{
	private:

		class CCreationCommand : public IScriptGraphNodeCreationCommand
		{
		public:

			CCreationCommand(const char* szSubject, const SGUID& signalGUID)
				: m_subject(szSubject)
				, m_signalGUID(signalGUID)
			{}

			// IScriptGraphNodeCreationCommand

			virtual const char* GetBehavior() const override
			{
				return "Signal::Send";
			}

			virtual const char* GetSubject() const override
			{
				return m_subject.c_str();
			}

			virtual const char* GetDescription() const override
			{
				return "Send signal";
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::SendSignal";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphSendSignalNode>(m_signalGUID), pos);
			}

			// ~IScriptGraphNodeCreationCommand

		private:

			string m_subject;
			SGUID  m_signalGUID;
		};

	public:

		// IScriptGraphNodeCreator

		virtual SGUID GetTypeGUID() const override
		{
			return CScriptGraphSendSignalNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const SGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphSendSignalNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override
		{
			switch (graph.GetType())
			{
			case EScriptGraphType::Signal:
			case EScriptGraphType::Function:
				{
					auto visitScriptSignal = [&nodeCreationMenu, &scriptView](const IScriptSignal& scriptSignal)
					{
						CStackString subject;
						scriptView.QualifyName(scriptSignal, EDomainQualifier::Global, subject);
						nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(subject.c_str(), scriptSignal.GetGUID()));
					};
					scriptView.VisitAccesibleSignals(ScriptSignalConstVisitor::FromLambda(visitScriptSignal));
					break;
				}
			}
		}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

void CScriptGraphSendSignalNode::GoToSignal()
{
	CryLinkUtils::ExecuteCommand(CryLinkUtils::ECommand::Show, m_signalGUID);
}

SRuntimeResult CScriptGraphSendSignalNode::ExecuteSendToSelf(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	struct EInputIdx
	{
		enum : uint32
		{
			In = 0,
			FirstParam
		};
	};

	const SRuntimeData& data = DynamicCast<SRuntimeData>(*context.node.GetData());

	CRuntimeParams params;
	for (uint8 inputIdx = EInputIdx::FirstParam, inputCount = context.node.GetInputCount(); inputIdx < inputCount; ++inputIdx)
	{
		if (context.node.IsDataInput(inputIdx))
		{
			params.SetInput(inputIdx - EInputIdx::FirstParam, *context.node.GetInputData(inputIdx));
		}
	}

	static_cast<IObject*>(context.pObject)->ProcessSignal(data.signalGUID, params);

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

SRuntimeResult CScriptGraphSendSignalNode::ExecuteSendToObject(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	struct EInputIdx
	{
		enum : uint32
		{
			In = 0,
			ObjectId,
			FirstParam
		};
	};

	const SRuntimeData& data = DynamicCast<SRuntimeData>(*context.node.GetData());
	const ObjectId objectId = DynamicCast<ObjectId>(*context.node.GetInputData(EInputIdx::ObjectId));

	CRuntimeParams params;
	for (uint8 inputIdx = EInputIdx::FirstParam, inputCount = context.node.GetInputCount(); inputIdx < inputCount; ++inputIdx)
	{
		if (context.node.IsDataInput(inputIdx))
		{
			params.SetInput(inputIdx - EInputIdx::FirstParam, *context.node.GetInputData(inputIdx));
		}
	}

	gEnv->pSchematyc->SendSignal(objectId, data.signalGUID, params);

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

SRuntimeResult CScriptGraphSendSignalNode::ExecuteBroadcast(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	struct EInputIdx
	{
		enum : uint32
		{
			In = 0,
			FirstParam
		};
	};

	const SRuntimeData& data = DynamicCast<SRuntimeData>(*context.node.GetData());

	CRuntimeParams params;
	for (uint8 inputIdx = EInputIdx::FirstParam, inputCount = context.node.GetInputCount(); inputIdx < inputCount; ++inputIdx)
	{
		if (context.node.IsDataInput(inputIdx))
		{
			params.SetInput(inputIdx - EInputIdx::FirstParam, *context.node.GetInputData(inputIdx));
		}
	}

	gEnv->pSchematyc->BroadcastSignal(data.signalGUID, params);

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

const SGUID CScriptGraphSendSignalNode::ms_typeGUID = "bfcebe12-b479-4cd4-90e2-5ceab24ea12e"_schematyc_guid;
} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphSendSignalNode::Register)
