// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptRegistry.h"

#include <CryCore/CryCrc32.h>
#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/File/ICryPak.h>
#include <Schematyc/Env/IEnvRegistry.h>
#include <Schematyc/Env/Elements/IEnvComponent.h>
#include <Schematyc/Services/ILog.h>
#include <Schematyc/Utils/Assert.h>
#include <Schematyc/Utils/StackString.h>
#include <Schematyc/Utils/StringUtils.h>

#include "CVars.h"
#include "Script/Script.h"
#include "Script/ScriptSerializers.h"
#include "Script/Elements/ScriptActionInstance.h"
#include "Script/Elements/ScriptBase.h"
#include "Script/Elements/ScriptClass.h"
#include "Script/Elements/ScriptComponentInstance.h"
#include "Script/Elements/ScriptConstructor.h"
#include "Script/Elements/ScriptEnum.h"
#include "Script/Elements/ScriptFunction.h"
#include "Script/Elements/ScriptInterface.h"
#include "Script/Elements/ScriptInterfaceFunction.h"
#include "Script/Elements/ScriptInterfaceImpl.h"
#include "Script/Elements/ScriptInterfaceTask.h"
#include "Script/Elements/ScriptModule.h"
#include "Script/Elements/ScriptRoot.h"
#include "Script/Elements/ScriptSignal.h"
#include "Script/Elements/ScriptSignalReceiver.h"
#include "Script/Elements/ScriptState.h"
#include "Script/Elements/ScriptStateMachine.h"
#include "Script/Elements/ScriptStruct.h"
#include "Script/Elements/ScriptTimer.h"
#include "Script/Elements/ScriptVariable.h"
#include "SerializationUtils/SerializationContext.h"
#include "Utils/FileUtils.h"
#include "Utils/GUIDRemapper.h"

namespace Schematyc
{
DECLARE_SHARED_POINTERS(CScriptActionInstance)
DECLARE_SHARED_POINTERS(CScriptBase)
DECLARE_SHARED_POINTERS(CScriptClass)
DECLARE_SHARED_POINTERS(CScriptComponentInstance)
DECLARE_SHARED_POINTERS(CScriptConstructor)
DECLARE_SHARED_POINTERS(CScriptEnum)
DECLARE_SHARED_POINTERS(CScriptFunction)
DECLARE_SHARED_POINTERS(CScriptInterface)
DECLARE_SHARED_POINTERS(CScriptInterfaceFunction)
DECLARE_SHARED_POINTERS(CScriptInterfaceImpl)
DECLARE_SHARED_POINTERS(CScriptInterfaceTask)
DECLARE_SHARED_POINTERS(CScriptModule)
DECLARE_SHARED_POINTERS(CScriptSignal)
DECLARE_SHARED_POINTERS(CScriptSignalReceiver)
DECLARE_SHARED_POINTERS(CScriptState)
DECLARE_SHARED_POINTERS(CScriptStateMachine)
DECLARE_SHARED_POINTERS(CScriptStruct)
DECLARE_SHARED_POINTERS(CScriptTimer)
DECLARE_SHARED_POINTERS(CScriptVariable)

namespace
{
void SaveAllScriptFilesCommand(IConsoleCmdArgs* pArgs)
{
	gEnv->pSchematyc->GetScriptRegistry().Save(true);
}

void ProcessEventRecursive(IScriptElement& element, const SScriptEvent& event)
{
	element.ProcessEvent(event);

	for (IScriptElement* pChildElement = element.GetFirstChild(); pChildElement; pChildElement = pChildElement->GetNextSibling())
	{
		ProcessEventRecursive(*pChildElement, event);
	}
}
} // Anonymous

CScriptRegistry::CScriptRegistry()
	: m_changeDepth(0)
{
	m_pRoot = std::make_shared<CScriptRoot>();
	REGISTER_COMMAND("sc_SaveAllScriptFiles", SaveAllScriptFilesCommand, VF_NULL, "Save all Schematyc script file regardless of whether they have been modified");
}

void CScriptRegistry::ProcessEvent(const SScriptEvent& event)
{
	ProcessEventRecursive(*m_pRoot, event);
}

bool CScriptRegistry::Load()
{
	LOADING_TIME_PROFILE_SECTION;

	// Configure file enumeration flags.

	FileUtils::FileEnumFlags fileEnumFlags = FileUtils::EFileEnumFlags::Recursive;
	if (CVars::sc_IgnoreUnderscoredFolders)
	{
		fileEnumFlags.Add(FileUtils::EFileEnumFlags::IgnoreUnderscoredFolders);
	}

	// Enumerate files and construct new elements.

	ScriptInputBlocks inputBlocks;
	const char* szScriptFolder = gEnv->pSchematyc->GetScriptsFolder();

	auto loadScript = [this, &inputBlocks](const char* szFileName, unsigned attributes)
	{
		SScriptInputBlock inputBlock;
		CScriptLoadSerializer serializer(inputBlock);
		Serialization::LoadXmlFile(serializer, szFileName);
		if (!GUID::IsEmpty(inputBlock.guid) && inputBlock.rootElement.ptr)
		{
			CScript* pScript = GetScript(inputBlock.guid);
			if (!pScript)
			{
				CStackString fileName = gEnv->pCryPak->GetGameFolder();
				fileName.append("/");
				fileName.append(szFileName);
				fileName.MakeLower();

				pScript = CreateScript(fileName.c_str(), inputBlock.guid);
				pScript->SetRoot(inputBlock.rootElement.ptr.get());
				inputBlock.rootElement.ptr->SetScript(pScript);
				inputBlocks.push_back(std::move(inputBlock));
			}
		}
	};
	FileUtils::EnumFilesInFolder(szScriptFolder, "*.sc_*", FileUtils::FileEnumCallback::FromLambda(loadScript), fileEnumFlags);

	ProcessInputBlocks(inputBlocks, *m_pRoot, EScriptEventId::FileLoad);
	return true;
}

void CScriptRegistry::Save(bool bAlwaysSave)
{
	// Save script files.
	for (Scripts::value_type& script : m_scripts)
	{
		SaveScript(*script.second);
	}
}

bool CScriptRegistry::IsValidScope(EScriptElementType elementType, IScriptElement* pScope) const
{
	const EScriptElementType scopeElementType = pScope ? pScope->GetElementType() : EScriptElementType::Root;
	switch (elementType)
	{
	case EScriptElementType::Module:
		{
			return scopeElementType == EScriptElementType::Root || scopeElementType == EScriptElementType::Module;
		}
	case EScriptElementType::Enum:
		{
			return scopeElementType == EScriptElementType::Root || scopeElementType == EScriptElementType::Module || scopeElementType == EScriptElementType::Class;
		}
	case EScriptElementType::Struct:
		{
			return scopeElementType == EScriptElementType::Root || scopeElementType == EScriptElementType::Module || scopeElementType == EScriptElementType::Class;
		}
	case EScriptElementType::Signal:
		{
			return scopeElementType == EScriptElementType::Root || scopeElementType == EScriptElementType::Module || scopeElementType == EScriptElementType::Class || scopeElementType == EScriptElementType::State;
		}
	case EScriptElementType::Constructor:
		{
			return scopeElementType == EScriptElementType::Class;
		}
	case EScriptElementType::Function:
		{
			return scopeElementType == EScriptElementType::Root || scopeElementType == EScriptElementType::Module || scopeElementType == EScriptElementType::Class;
		}
	case EScriptElementType::Interface:
		{
			return scopeElementType == EScriptElementType::Root || scopeElementType == EScriptElementType::Module;
		}
	case EScriptElementType::InterfaceFunction:
		{
			return scopeElementType == EScriptElementType::Interface;
		}
	case EScriptElementType::InterfaceTask:
		{
			return scopeElementType == EScriptElementType::Interface;
		}
	case EScriptElementType::Class:
		{
			return scopeElementType == EScriptElementType::Root || scopeElementType == EScriptElementType::Module;
		}
	case EScriptElementType::Base:
		{
			return scopeElementType == EScriptElementType::Class;
		}
	case EScriptElementType::StateMachine:
		{
			return scopeElementType == EScriptElementType::Class;
		}
	case EScriptElementType::State:
		{
			return scopeElementType == EScriptElementType::StateMachine || scopeElementType == EScriptElementType::State;
		}
	case EScriptElementType::Variable:
		{
			return scopeElementType == EScriptElementType::Class;
		}
	case EScriptElementType::Timer:
		{
			return scopeElementType == EScriptElementType::Class || scopeElementType == EScriptElementType::State;
		}
	case EScriptElementType::SignalReceiver:
		{
			return scopeElementType == EScriptElementType::Class || scopeElementType == EScriptElementType::State;
		}
	case EScriptElementType::InterfaceImpl:
		{
			return scopeElementType == EScriptElementType::Class;
		}
	case EScriptElementType::ComponentInstance:
		{
			switch (scopeElementType)
			{
			case EScriptElementType::Class:
			case EScriptElementType::Base:
				{
					return true;
				}
			case EScriptElementType::ComponentInstance:
				{
					const IScriptComponentInstance& componentInstance = DynamicCast<IScriptComponentInstance>(*pScope);
					const IEnvComponent* pEnvComponent = gEnv->pSchematyc->GetEnvRegistry().GetComponent(componentInstance.GetTypeGUID());
					if (pEnvComponent)
					{
						if (pEnvComponent->GetFlags().Check(EEnvComponentFlags::Socket))
						{
							return true;
						}
					}
					return false;
				}
			default:
				{
					return false;
				}
			}
		}
	case EScriptElementType::ActionInstance:
		{
			return scopeElementType == EScriptElementType::Class || scopeElementType == EScriptElementType::State;
		}
	default:
		{
			return false;
		}
	}
}

bool CScriptRegistry::IsValidName(const char* szName, IScriptElement* pScope, const char*& szErrorMessage) const
{
	if (StringUtils::IsValidElementName(szName, szErrorMessage))
	{
		if (IsUniqueElementName(szName, pScope))
		{
			return true;
		}
		else
		{
			szErrorMessage = "Name must be unique.";
		}
	}
	return false;
}

IScriptModule* CScriptRegistry::AddModule(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Module, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptModulePtr pModule = std::make_shared<CScriptModule>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pModule, *pScope);
				return pModule.get();
			}
		}
	}
	return nullptr;
}

IScriptEnum* CScriptRegistry::AddEnum(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Enum, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptEnumPtr pEnum = std::make_shared<CScriptEnum>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pEnum, *pScope);
				return pEnum.get();
			}
		}
	}
	return nullptr;
}

IScriptStruct* CScriptRegistry::AddStruct(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Struct, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptStructPtr pStruct = std::make_shared<CScriptStruct>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pStruct, *pScope);
				return pStruct.get();
			}
		}
	}
	return nullptr;
}

IScriptSignal* CScriptRegistry::AddSignal(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Signal, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptSignalPtr pSignal = std::make_shared<CScriptSignal>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pSignal, *pScope);
				return pSignal.get();
			}
		}
	}
	return nullptr;
}

IScriptConstructor* CScriptRegistry::AddConstructor(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Constructor, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptConstructorPtr pConstructor = std::make_shared<CScriptConstructor>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pConstructor, *pScope);
				return pConstructor.get();
			}
		}
	}
	return nullptr;
}

IScriptFunction* CScriptRegistry::AddFunction(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Function, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptFunctionPtr pFunction = std::make_shared<CScriptFunction>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pFunction, *pScope);
				return pFunction.get();
			}
		}
	}
	return nullptr;
}

IScriptInterface* CScriptRegistry::AddInterface(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Interface, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptInterfacePtr pInterface = std::make_shared<CScriptInterface>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pInterface, *pScope);
				return pInterface.get();
			}
		}
	}
	return nullptr;
}

IScriptInterfaceFunction* CScriptRegistry::AddInterfaceFunction(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::InterfaceFunction, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptInterfaceFunctionPtr pInterfaceFunction = std::make_shared<CScriptInterfaceFunction>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pInterfaceFunction, *pScope);
				return pInterfaceFunction.get();
			}
		}
	}
	return nullptr;
}

IScriptInterfaceTask* CScriptRegistry::AddInterfaceTask(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::InterfaceTask, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptInterfaceTaskPtr pInterfaceTask = std::make_shared<CScriptInterfaceTask>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pInterfaceTask, *pScope);
				return pInterfaceTask.get();
			}
		}
	}
	return nullptr;
}

IScriptClass* CScriptRegistry::AddClass(const char* szName, const SElementId& baseId, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Class, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptClassPtr pClass = std::make_shared<CScriptClass>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pClass, *pScope);
				AddBase(baseId, pClass.get());
				return pClass.get();
			}
		}
	}
	return nullptr;
}

IScriptBase* CScriptRegistry::AddBase(const SElementId& baseId, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor());
	if (gEnv->IsEditor())
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Base, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			CScriptBasePtr pBase = std::make_shared<CScriptBase>(gEnv->pSchematyc->CreateGUID(), baseId);
			AddElement(pBase, *pScope);
			return pBase.get();
		}
	}
	return nullptr;
}

IScriptStateMachine* CScriptRegistry::AddStateMachine(const char* szName, EScriptStateMachineLifetime lifetime, const SGUID& contextGUID, const SGUID& partnerGUID, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::StateMachine, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptStateMachinePtr pStateMachine = std::make_shared<CScriptStateMachine>(gEnv->pSchematyc->CreateGUID(), szName, lifetime, contextGUID, partnerGUID);
				AddElement(pStateMachine, *pScope);
				return pStateMachine.get();
			}
		}
	}
	return nullptr;
}

IScriptState* CScriptRegistry::AddState(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::State, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptStatePtr pState = std::make_shared<CScriptState>(gEnv->pSchematyc->CreateGUID(), szName);
				AddElement(pState, *pScope);
				return pState.get();
			}
		}
	}
	return nullptr;
}

IScriptVariable* CScriptRegistry::AddVariable(const char* szName, const SElementId& typeId, const SGUID& baseGUID, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Variable, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptVariablePtr pVariable = std::make_shared<CScriptVariable>(gEnv->pSchematyc->CreateGUID(), szName, typeId, baseGUID);
				AddElement(pVariable, *pScope);
				return pVariable.get();
			}
		}
	}
	return nullptr;
}

IScriptTimer* CScriptRegistry::AddTimer(const char* szName, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor());
	if (gEnv->IsEditor())
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::Timer, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			CScriptTimerPtr pTimer = std::make_shared<CScriptTimer>(gEnv->pSchematyc->CreateGUID(), szName);
			AddElement(pTimer, *pScope);
			return pTimer.get();
		}
	}
	return nullptr;
}

IScriptSignalReceiver* CScriptRegistry::AddSignalReceiver(const char* szName, EScriptSignalReceiverType type, const SGUID& signalGUID, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor());
	if (gEnv->IsEditor())
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::SignalReceiver, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			CScriptSignalReceiverPtr pSignalReceiver = std::make_shared<CScriptSignalReceiver>(gEnv->pSchematyc->CreateGUID(), szName, type, signalGUID);
			AddElement(pSignalReceiver, *pScope);
			return pSignalReceiver.get();
		}
	}
	return nullptr;
}

IScriptInterfaceImpl* CScriptRegistry::AddInterfaceImpl(EDomain domain, const SGUID& refGUID, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor());
	if (gEnv->IsEditor())
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::InterfaceImpl, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			CScriptInterfaceImplPtr pInterfaceImpl = std::make_shared<CScriptInterfaceImpl>(gEnv->pSchematyc->CreateGUID(), domain, refGUID);
			AddElement(pInterfaceImpl, *pScope);
			return pInterfaceImpl.get();
		}
	}
	return nullptr;
}

IScriptComponentInstance* CScriptRegistry::AddComponentInstance(const char* szName, const SGUID& typeGUID, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::ComponentInstance, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptComponentInstancePtr pComponentInstance = std::make_shared<CScriptComponentInstance>(gEnv->pSchematyc->CreateGUID(), szName, typeGUID);
				AddElement(pComponentInstance, *pScope);
				return pComponentInstance.get();
			}
		}
	}
	return nullptr;
}

IScriptActionInstance* CScriptRegistry::AddActionInstance(const char* szName, const SGUID& actionGUID, const SGUID& contextGUID, IScriptElement* pScope)
{
	SCHEMATYC_CORE_ASSERT(gEnv->IsEditor() && szName);
	if (gEnv->IsEditor() && szName)
	{
		const bool bIsValidScope = IsValidScope(EScriptElementType::ActionInstance, pScope);
		SCHEMATYC_CORE_ASSERT(bIsValidScope);
		if (bIsValidScope)
		{
			if (!pScope)
			{
				pScope = m_pRoot.get();
			}
			if (IsElementNameUnique(szName, pScope))
			{
				CScriptActionInstancePtr pActionInstance = std::make_shared<CScriptActionInstance>(gEnv->pSchematyc->CreateGUID(), szName, actionGUID, contextGUID);
				AddElement(pActionInstance, *pScope);
				return pActionInstance.get();
			}
		}
	}
	return nullptr;
}

void CScriptRegistry::RemoveElement(const SGUID& guid)
{
	Elements::iterator itElement = m_elements.find(guid);
	if (itElement != m_elements.end())
	{
		RemoveElement(*itElement->second);
	}
}

IScriptElement& CScriptRegistry::GetRootElement()
{
	return *m_pRoot;
}

const IScriptElement& CScriptRegistry::GetRootElement() const
{
	return *m_pRoot;
}

IScriptElement* CScriptRegistry::GetElement(const SGUID& guid)
{
	Elements::iterator itElement = m_elements.find(guid);
	return itElement != m_elements.end() ? itElement->second.get() : nullptr;
}

const IScriptElement* CScriptRegistry::GetElement(const SGUID& guid) const
{
	Elements::const_iterator itElement = m_elements.find(guid);
	return itElement != m_elements.end() ? itElement->second.get() : nullptr;
}

bool CScriptRegistry::CopyElementsToXml(XmlNodeRef& output, IScriptElement& scope) const
{
	// #SchematycTODO : Make sure elements don't have NotCopyable flag!!!
	output = Serialization::SaveXmlNode(CScriptCopySerializer(scope), "schematycScript");
	return !!output;
}

bool CScriptRegistry::PasteElementsFromXml(const XmlNodeRef& input, IScriptElement* pScope)
{
	ScriptInputBlocks inputBlocks(1);
	CScriptPasteSerializer serializer(inputBlocks.back());
	if (Serialization::LoadXmlNode(serializer, input))
	{
		ProcessInputBlocks(inputBlocks, pScope ? *pScope : *m_pRoot, EScriptEventId::EditorPaste);
	}
	return true;
}

bool CScriptRegistry::IsElementNameUnique(const char* szName, IScriptElement* pScope) const
{
	SCHEMATYC_CORE_ASSERT(szName);
	if (szName)
	{
		if (!pScope)
		{
			pScope = m_pRoot.get();
		}
		for (const IScriptElement* pElement = pScope->GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
		{
			if (strcmp(pElement->GetName(), szName) == 0)
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

void CScriptRegistry::MakeElementNameUnique(IString& name, IScriptElement* pScope) const
{
	CStackString uniqueName = name.c_str();
	if (!IsElementNameUnique(uniqueName.c_str(), pScope))
	{
		const CStackString::size_type length = uniqueName.length();
		char stringBuffer[32] = "";
		uint32 postfix = 1;
		do
		{
			uniqueName.resize(length);
			ltoa(postfix, stringBuffer, 10);
			uniqueName.append(stringBuffer);
			++postfix;
		}
		while (!IsElementNameUnique(uniqueName.c_str(), pScope));
		name.assign(uniqueName.c_str());
	}
}

void CScriptRegistry::ElementModified(IScriptElement& element)
{
	ProcessChange(SScriptRegistryChange(EScriptRegistryChangeType::ElementModified, element));
	ProcessChangeDependencies(EScriptRegistryChangeType::ElementModified, element.GetGUID());
}

ScriptRegistryChangeSignal::Slots& CScriptRegistry::GetChangeSignalSlots()
{
	return m_signals.change.GetSlots();
}

CScript* CScriptRegistry::CreateScript(const char* szFileName, const SGUID& guid)
{
	// #SchematycTODO : Should we also take steps to avoid name collisions?
	CScriptPtr pScript = std::make_shared<CScript>(guid, szFileName);
	m_scripts.insert(Scripts::value_type(guid, pScript));
	return pScript.get();
}

CScript* CScriptRegistry::CreateScript()
{
	const SGUID guid = gEnv->pSchematyc->CreateGUID();
	SCHEMATYC_CORE_ASSERT(!GUID::IsEmpty(guid));
	if (!GUID::IsEmpty(guid))
	{
		return CreateScript(nullptr, guid);
	}
	return nullptr;
}

CScript* CScriptRegistry::GetScript(const SGUID& guid)
{
	Scripts::iterator itScript = m_scripts.find(guid);
	return itScript != m_scripts.end() ? itScript->second.get() : nullptr;
}

void CScriptRegistry::ProcessInputBlocks(ScriptInputBlocks& inputBlocks, IScriptElement& scope, EScriptEventId eventId)
{
	// Unroll elements.
	ScriptInputElementPtrs elements;
	elements.reserve(100);
	for (SScriptInputBlock& inputBlock : inputBlocks)
	{
		UnrollScriptInputElementsRecursive(elements, inputBlock.rootElement);
	}
	// Pre-load elements.
	for (SScriptInputElement* pElement : elements)
	{
		CScriptInputElementSerializer elementSerializer(*pElement->ptr, ESerializationPass::LoadDependencies);
		Serialization::LoadBlackBox(elementSerializer, pElement->blackBox);
	}
	if (eventId == EScriptEventId::EditorPaste)
	{
		// Make sure root elements have unique names.
		for (SScriptInputBlock& inputBlock : inputBlocks)
		{
			CStackString elementName = inputBlock.rootElement.ptr->GetName();
			MakeElementNameUnique(elementName, &scope);
			inputBlock.rootElement.ptr->SetName(elementName.c_str());
		}
		// Re-map element dependencies.
		CGUIDRemapper guidRemapper;
		for (SScriptInputElement* pElement : elements)
		{
			guidRemapper.Bind(pElement->ptr->GetGUID(), gEnv->pSchematyc->CreateGUID());
		}
		for (SScriptInputElement* pElement : elements)
		{
			pElement->ptr->RemapDependencies(guidRemapper);
		}
	}
	// Sort elements in order of dependency.
	if (SortScriptInputElementsByDependency(elements))
	{
		// Load elements.
		for (SScriptInputElement* pElement : elements)
		{
			CScriptInputElementSerializer elementSerializer(*pElement->ptr, ESerializationPass::Load);
			Serialization::LoadBlackBox(elementSerializer, pElement->blackBox);
			m_elements.insert(Elements::value_type(pElement->ptr->GetGUID(), pElement->ptr));
		}
		// Attach elements.
		for (SScriptInputBlock& inputBlock : inputBlocks)
		{
			IScriptElement* pScope = !GUID::IsEmpty(inputBlock.scopeGUID) ? GetElement(inputBlock.scopeGUID) : &scope;
			IScriptElement& element = *inputBlock.rootElement.ptr;
			if (pScope)
			{
				pScope->AttachChild(element);
			}
			else
			{
				SCHEMATYC_CORE_CRITICAL_ERROR("Invalid scope for element %s!", element.GetName());
			}
		}
		// Post-load elements.
		for (SScriptInputElement* pElement : elements)
		{
			CScriptInputElementSerializer elementSerializer(*pElement->ptr, ESerializationPass::PostLoad);
			Serialization::LoadBlackBox(elementSerializer, pElement->blackBox);

			if (eventId == EScriptEventId::EditorPaste)
			{
				pElement->ptr->ProcessEvent(SScriptEvent(EScriptEventId::EditorPaste));
			}
			ProcessChange(SScriptRegistryChange(EScriptRegistryChangeType::ElementAdded, *pElement->ptr));
		}
		// Broadcast event to all elements.
		for (SScriptInputElement* pElement : elements)
		{
			pElement->ptr->ProcessEvent(SScriptEvent(eventId));
		}
	}
}

void CScriptRegistry::AddElement(const IScriptElementPtr& pElement, IScriptElement& scope)
{
	BeginChange();

	bool bCreateScript = false;
	if (CVars::sc_EnableScriptPartitioning)
	{
		bCreateScript = pElement->GetElementFlags().Check(EScriptElementFlags::CanOwnScript);
	}
	else
	{
		if (pElement->GetElementFlags().Check(EScriptElementFlags::MustOwnScript))
		{
			bCreateScript = true;
		}
		else if (pElement->GetElementFlags().Check(EScriptElementFlags::CanOwnScript))
		{
			bCreateScript = true;
			for (const IScriptElement* pScope = &scope; pScope; pScope = pScope->GetParent())
			{
				if (pScope->GetScript())
				{
					bCreateScript = false;
				}
			}
		}
	}

	if (bCreateScript)
	{
		// #SchematycTODO : We should do this when patching up script elements!!!
		CScript* pScript = CreateScript();
		pScript->SetRoot(pElement.get());
		pElement->SetScript(pScript);
	}

	scope.AttachChild(*pElement);

	m_elements.insert(Elements::value_type(pElement->GetGUID(), pElement));

	ProcessChange(SScriptRegistryChange(EScriptRegistryChangeType::ElementAdded, *pElement));

	pElement->ProcessEvent(SScriptEvent(EScriptEventId::EditorAdd));

	EndChange();
}

void CScriptRegistry::RemoveElement(IScriptElement& element)
{
	while (IScriptElement* pChildElement = element.GetFirstChild())
	{
		RemoveElement(*pChildElement);
	}

	ProcessChange(SScriptRegistryChange(EScriptRegistryChangeType::ElementRemoved, element));

	const SGUID guid = element.GetGUID();
	IScript* pScript = element.GetScript();
	if (pScript)
	{
		m_scripts.erase(guid);
	}

	m_elements.erase(guid);

	ProcessChangeDependencies(EScriptRegistryChangeType::ElementRemoved, guid);
}

void CScriptRegistry::SaveScript(CScript& script)
{
	CStackString fileName = script.GetName();
	if (fileName.empty())
	{
		fileName = script.SetNameFromRoot();
	}

	CStackString folder = fileName.substr(0, fileName.rfind('/'));
	if (!folder.empty())
	{
		gEnv->pCryPak->MakeDir(folder.c_str());
	}

	auto elementSerializeCallback = [this](IScriptElement& element)
	{
		ProcessChange(SScriptRegistryChange(EScriptRegistryChangeType::ElementSaved, element));
	};

	const bool bError = !Serialization::SaveXmlFile(fileName.c_str(), CScriptSaveSerializer(script, ScriptElementSerializeCallback::FromLambda(elementSerializeCallback)), "schematyc");
	if (bError)
	{
		SCHEMATYC_CORE_ERROR("Failed to save file '%s'!", fileName.c_str());
	}
}

void CScriptRegistry::BeginChange()
{
	++m_changeDepth;
}

void CScriptRegistry::EndChange()
{
	--m_changeDepth;
	if (m_changeDepth == 0)
	{
		for (const SScriptRegistryChange& change : m_changeQueue)
		{
			m_signals.change.Send(change);
		}
		m_changeQueue.clear();
	}
}

void CScriptRegistry::ProcessChange(const SScriptRegistryChange& change)
{
	if (m_changeDepth)
	{
		m_changeQueue.push_back(change);
	}
	else
	{
		m_signals.change.Send(change);
	}
}

void CScriptRegistry::ProcessChangeDependencies(EScriptRegistryChangeType changeType, const SGUID& elementGUID)
{
	EScriptEventId dependencyEventId = EScriptEventId::Invalid;
	EScriptRegistryChangeType dependencyChangeType = EScriptRegistryChangeType::Invalid;
	switch (changeType)
	{
	case EScriptRegistryChangeType::ElementModified:
		{
			dependencyEventId = EScriptEventId::EditorDependencyModified;
			dependencyChangeType = EScriptRegistryChangeType::ElementDependencyModified;
			break;
		}
	case EScriptRegistryChangeType::ElementRemoved:
		{
			dependencyEventId = EScriptEventId::EditorDependencyRemoved;
			dependencyChangeType = EScriptRegistryChangeType::ElementDependencyRemoved;
			break;
		}
	default:
		{
			return;
		}
	}

	const SScriptEvent event(dependencyEventId, elementGUID);
	for (Elements::value_type& dependencyElement : m_elements)
	{
		bool bIsDependency = false;
		auto enumerateDependency = [&elementGUID, &bIsDependency](const SGUID& guid)
		{
			if (guid == elementGUID)
			{
				bIsDependency = true;
			}
		};
		dependencyElement.second->EnumerateDependencies(ScriptDependencyEnumerator::FromLambda(enumerateDependency), EScriptDependencyType::Event);   // #SchematycTODO : Can we cache dependencies after every change?

		if (bIsDependency)
		{
			dependencyElement.second->ProcessEvent(event);

			m_signals.change.Send(SScriptRegistryChange(dependencyChangeType, *dependencyElement.second));   // #SchematycTODO : Queue these changes and process immediately after processing events?
		}
	}
}

bool CScriptRegistry::IsUniqueElementName(const char* szName, IScriptElement* pScope) const
{
	if (!pScope)
	{
		pScope = m_pRoot.get();
	}
	for (IScriptElement* pElement = pScope->GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
	{
		if (strcmp(szName, pElement->GetName()) == 0)
		{
			return false;
		}
	}
	return true;
}
} // Schematyc
