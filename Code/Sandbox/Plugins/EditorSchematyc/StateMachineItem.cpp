// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StateMachineItem.h"

#include "ScriptBrowserUtils.h"
#include "ObjectModel.h"

#include <Schematyc/Script/Elements/IScriptStateMachine.h>
#include <Schematyc/Script/Elements/IScriptState.h>
#include <Schematyc/SerializationUtils/ISerializationContext.h>

#include <StateItem.h>

#include <CryIcon.h>
#include <QtUtil.h>

namespace CrySchematycEditor {

	CryIcon CStateMachineItem::s_icon = CryIcon("icons:schematyc/script_state_machine.png");

CStateMachineItem::CStateMachineItem(Schematyc::IScriptStateMachine& scriptStateMachine, CAbstractObjectStructureModel& model)
	: CAbstractObjectStructureModelItem(model)
	, m_scriptStateMachine(scriptStateMachine)
	, m_pParentItem(nullptr)
{
	m_name = m_scriptStateMachine.GetName();
	LoadFromScriptElement();
}

CStateMachineItem::~CStateMachineItem()
{
	for (CStateItem* pItem : m_states)
		delete pItem;
}

void CStateMachineItem::SetName(QString name)
{
	Schematyc::CStackString uniqueName = QtUtil::ToString(name).c_str();
	Schematyc::ScriptBrowserUtils::MakeScriptElementNameUnique(uniqueName, m_model.GetScriptElement());

	m_scriptStateMachine.SetName(uniqueName);
	m_name = m_scriptStateMachine.GetName();
}

CAbstractObjectStructureModelItem* CStateMachineItem::GetChildItemByIndex(uint32 index) const
{
	if (index < m_states.size())
	{
		return m_states[index];
	}
	return nullptr;
}

uint32 CStateMachineItem::GetChildItemIndex(const CAbstractObjectStructureModelItem& item) const
{
	if (item.GetType() == eObjectItemType_State)
	{
		uint32 index = 0;
		for (const CStateItem* pItem : m_states)
		{
			if (pItem == &item)
				return index;
			++index;
		}
	}

	return 0xffffffff;
}

uint32 CStateMachineItem::GetIndex() const
{
	if (m_pParentItem)
	{
		return m_pParentItem->GetChildItemIndex(*this);
	}
	else
	{
		return GetModel().GetChildItemIndex(*this);
	}

	return 0xffffffff;
}

void CStateMachineItem::Serialize(Serialization::IArchive& archive)
{
	// TODO: This will only work for serialization to properties in inspector!
	Schematyc::SSerializationContextParams serParams(archive, Schematyc::ESerializationPass::Edit);
	Schematyc::ISerializationContextPtr pSerializationContext = gEnv->pSchematyc->CreateSerializationContext(serParams);
	// ~TODO

	m_scriptStateMachine.Serialize(archive);
}

bool CStateMachineItem::AllowsRenaming() const
{
	const bool allowsRenaming = !m_scriptStateMachine.GetElementFlags().Check(Schematyc::EScriptElementFlags::FixedName);
	return allowsRenaming;
}

CStateItem* CStateMachineItem::GetStateItemByIndex(uint32 index) const
{
	if (index < m_states.size())
	{
		return m_states[index];
	}
	return nullptr;
}

CStateItem* CStateMachineItem::CreateState()
{
	Schematyc::CStackString name = "State";
	Schematyc::ScriptBrowserUtils::MakeScriptElementNameUnique(name, m_model.GetScriptElement());

	Schematyc::IScriptState* pStateElement = gEnv->pSchematyc->GetScriptRegistry().AddState(name, m_model.GetScriptElement());
	if (pStateElement)
	{
		CStateItem* pStateItem = new CStateItem(*pStateElement, m_model);
		pStateItem->SetParentItem(this);

		m_states.push_back(pStateItem);
		GetModel().SignalObjectStructureItemAdded(*pStateItem);

		return pStateItem;
	}

	return nullptr;
}

bool CStateMachineItem::RemoveState()
{
	// TODO: !
	CRY_ASSERT_MESSAGE(false, "Missing impl.");
	return false;
}

Schematyc::SGUID CStateMachineItem::GetGUID() const
{
	return m_scriptStateMachine.GetGUID();
}

void CStateMachineItem::LoadFromScriptElement()
{
	m_name = m_scriptStateMachine.GetName();

	Schematyc::IScriptElement* pElement = m_scriptStateMachine.GetFirstChild();
	while (pElement)
	{
		const Schematyc::EScriptElementType elementType = pElement->GetElementType();
		switch (elementType)
		{
		case Schematyc::EScriptElementType::State:
			{
				Schematyc::IScriptState& scriptState = static_cast<Schematyc::IScriptState&>(*pElement);
				CStateItem* pStateItem = new CStateItem(scriptState, m_model);
				pStateItem->SetParentItem(this);
				m_states.push_back(pStateItem);
			}
			break;
		default:
			break;   // Something unexpected!
		}

		pElement = pElement->GetNextSibling();
	}
}

}
