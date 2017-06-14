// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoClass.h"

#include "MonoObject.h"
#include "MonoRuntime.h"

#include "RootMonoDomain.h"

#include <mono/metadata/class.h>
#include <mono/metadata/exception.h>

CMonoClass::CMonoClass(CMonoLibrary* pLibrary, const char *nameSpace, const char *name)
	: m_pLibrary(pLibrary)
	, m_namespace(nameSpace)
	, m_name(name)
{
	m_pClass = mono_class_from_name(pLibrary->GetImage(), nameSpace, name);
}

CMonoClass::CMonoClass(CMonoLibrary* pLibrary, MonoClass* pClass)
	: m_pLibrary(pLibrary)
	, m_pClass(pClass)
{
	m_namespace = mono_class_get_namespace(m_pClass);
	m_name = mono_class_get_name(m_pClass);
}

void CMonoClass::Serialize()
{
	for (auto it = m_objects.begin(); it != m_objects.end();)
	{
		if (auto pObject = it->lock())
		{
			static_cast<CMonoObject*>(pObject.get())->Serialize();

			++it;
		}
		else
		{
			// Clear out dead objects while we're at it
			it = m_objects.erase(it);
		}
	}
}

void CMonoClass::Deserialize()
{
	for (auto it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		if (auto pObject = it->lock())
		{
			static_cast<CMonoObject*>(pObject.get())->Deserialize();
		}
	}
}

void CMonoClass::ReloadClass()
{
	m_pClass = mono_class_from_name(m_pLibrary->GetImage(), m_namespace, m_name);
}

const char* CMonoClass::GetName() const
{
	return mono_class_get_name(m_pClass);
}

const char* CMonoClass::GetNamespace() const
{
	return mono_class_get_namespace(m_pClass);
}

IMonoAssembly* CMonoClass::GetAssembly() const
{
	return m_pLibrary;
}

std::shared_ptr<IMonoObject> CMonoClass::CreateUninitializedInstance()
{
	auto pWrappedObject = std::make_shared<CMonoObject>(mono_object_new((MonoDomain*)static_cast<CMonoDomain*>(m_pLibrary->GetDomain())->GetHandle(), m_pClass), m_pThis.lock());

	// Push back a weak pointer to the instance, so that we can serialize the object in case of app domain reload
	m_objects.push_back(pWrappedObject);

	return pWrappedObject;
}

std::shared_ptr<IMonoObject> CMonoClass::CreateInstance(void** pConstructorParams, int numParams)
{
	auto* pObject = mono_object_new((MonoDomain*)static_cast<CMonoDomain*>(m_pLibrary->GetDomain())->GetHandle(), m_pClass);

	if (pConstructorParams != nullptr)
	{
		MonoMethod* pConstructorMethod = mono_class_get_method_from_name(m_pClass, ".ctor", numParams);

		bool bException = false;
		InvokeMethod(pConstructorMethod, pObject, pConstructorParams, bException);

		if (bException)
		{
			return nullptr;
		}
	}
	else
	{
		// Execute the default constructor
		mono_runtime_object_init(pObject);
	}

	auto pWrappedObject = std::make_shared<CMonoObject>(pObject, m_pThis.lock());

	// Push back a weak pointer to the instance, so that we can serialize the object in case of app domain reload
	m_objects.push_back(pWrappedObject);

	return pWrappedObject;
}

std::shared_ptr<IMonoObject> CMonoClass::CreateInstanceWithDesc(const char* parameterDesc, void** pConstructorParams)
{
	auto* pObject = mono_object_new((MonoDomain*)static_cast<CMonoDomain*>(m_pLibrary->GetDomain())->GetHandle(), m_pClass);

	string sMethodDesc;
	sMethodDesc.Format(":.ctor(%s)", parameterDesc);

	auto* pMethodDesc = mono_method_desc_new(sMethodDesc, false);

	if (MonoMethod* pMethod = mono_method_desc_search_in_class(pMethodDesc, m_pClass))
	{
		mono_method_desc_free(pMethodDesc);

		bool bException = false;
		InvokeMethod(pMethod, pObject, pConstructorParams, bException);

		if (bException)
		{
			return nullptr;
		}

		auto pWrappedObject = std::make_shared<CMonoObject>(pObject, m_pThis.lock());

		// Push back a weak pointer to the instance, so that we can serialize the object in case of app domain reload
		m_objects.push_back(pWrappedObject);

		return pWrappedObject;
	}

	mono_method_desc_free(pMethodDesc);

	static_cast<CMonoRuntime*>(gEnv->pMonoRuntime)->HandleException((MonoObject*)mono_get_exception_missing_method(GetName(), sMethodDesc));
	return nullptr;
}

std::shared_ptr<IMonoObject> CMonoClass::InvokeMethod(const char *methodName, const IMonoObject* pObject, void **pParams, int numParams) const
{
	if (MonoMethod* pMethod = GetMethodFromNameRecursive(m_pClass, methodName, numParams))
	{
		MonoObject* pObjectHandle = pObject != nullptr ? (MonoObject*)pObject->GetHandle() : nullptr;

		bool bException = false;
		return InvokeMethod(pMethod, pObjectHandle, pParams, bException);
	}

	static_cast<CMonoRuntime*>(gEnv->pMonoRuntime)->HandleException((MonoObject*)mono_get_exception_missing_method(GetName(), methodName));
	return nullptr;
}

std::shared_ptr<IMonoObject> CMonoClass::InvokeMethodWithDesc(const char* methodDesc, const IMonoObject* pObject, void** pParams) const
{
	auto* pMethodDesc = mono_method_desc_new(methodDesc, true);

	if (MonoMethod* pMethod = mono_method_desc_search_in_class(pMethodDesc, m_pClass))
	{
		mono_method_desc_free(pMethodDesc);

		MonoObject* pObjectHandle = pObject != nullptr ? (MonoObject*)pObject->GetHandle() : nullptr;

		bool bException = false;
		return InvokeMethod(pMethod, pObjectHandle, pParams, bException);
	}

	mono_method_desc_free(pMethodDesc);

	static_cast<CMonoRuntime*>(gEnv->pMonoRuntime)->HandleException((MonoObject*)mono_get_exception_missing_method(GetName(), methodDesc));
	return nullptr;
}

MonoMethod* CMonoClass::GetMethodFromNameRecursive(MonoClass* pClass, const char* szName, int numParams) const
{
	MonoMethod* pMethod = nullptr;
	while (pClass != nullptr && pMethod == nullptr) 
	{
		pMethod = mono_class_get_method_from_name(pClass, szName, numParams);
		
		if (pMethod == nullptr)
		{
			pClass = mono_class_get_parent(pClass);
		}
	}
	
	return pMethod;
}

std::shared_ptr<IMonoObject> CMonoClass::InvokeMethod(MonoMethod* pMethod, MonoObject* pObjectHandle, void** pParams, bool bHadException) const
{
	MonoObject* pException = nullptr;

	MonoObject* pResult = mono_runtime_invoke(pMethod, pObjectHandle, pParams, &pException);
	if (pException == nullptr)
	{
		return std::make_shared<CMonoObject>(pResult, m_pThis.lock());
	}

	bHadException = true;
	static_cast<CMonoRuntime*>(gEnv->pMonoRuntime)->HandleException(pException);

	return nullptr;
}

bool CMonoClass::IsMethodImplemented(IMonoClass* pBaseClass, const char* methodDesc)
{
	void *pIterator = 0;

	MonoClass *pClass = m_pClass;
	
	while (pClass != nullptr)
	{
		string sMethodDesc;
		sMethodDesc.Format("%s:%s", GetName(), methodDesc);

		MonoMethodDesc* pDesiredDesc = mono_method_desc_new(sMethodDesc, false);
		
		if (mono_method_desc_search_in_class(pDesiredDesc, pClass) != nullptr)
		{
			mono_method_desc_free(pDesiredDesc);
			return true;
		}

		mono_method_desc_free(pDesiredDesc);
		pClass = mono_class_get_parent(pClass);
		if (pClass == mono_get_object_class() || pClass == pBaseClass->GetHandle())
			break;

		pIterator = 0;
	}

	return false;
}