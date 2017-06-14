// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectPool.h"

#include <Schematyc/Runtime/IRuntimeClass.h>

#include "Core.h"
#include "Object.h"
#include "Runtime/RuntimeRegistry.h"

namespace Schematyc
{
IObject* CObjectPool::CreateObject(const SObjectParams& params)
{
	CRuntimeClassConstPtr pClass = CCore::GetInstance().GetRuntimeRegistryImpl().GetClassImpl(params.classGUID);
	if (pClass)
	{
		if (m_freeSlots.empty() && !AllocateSlots(m_slots.size() + 1))
		{
			return nullptr;
		}

		SSlot& slot = m_slots[m_freeSlots.back()];
		CObjectPtr pObject = std::make_shared<CObject>(slot.objectId);
		if (pObject->Init(pClass, params.pCustomData, params.pProperties, params.simulationMode))
		{
			m_freeSlots.pop_back();
			slot.pObject = pObject;
			return pObject.get();
		}
	}
	return nullptr;
}

IObject* CObjectPool::GetObject(ObjectId objectId)
{
	uint32 slotIdx;
	uint32 salt;
	if (ExpandObjectId(objectId, slotIdx, salt))
	{
		return m_slots[slotIdx].pObject.get();
	}
	return nullptr;
}

void CObjectPool::DestroyObject(ObjectId objectId)
{
	uint32 slotIdx;
	uint32 salt;
	if (ExpandObjectId(objectId, slotIdx, salt))
	{
		SSlot& slot = m_slots[slotIdx];
		slot.pObject.reset();
		slot.objectId = CreateObjectId(slotIdx, IncrementSalt(salt));
		m_freeSlots.push_back(slotIdx);
	}
}

void CObjectPool::SendSignal(ObjectId objectId, const SGUID& signalGUID, CRuntimeParams& params)
{
	IObject* pObject = GetObject(objectId);
	if (pObject)
	{
		pObject->ProcessSignal(signalGUID, params);
	}
}

void CObjectPool::BroadcastSignal(const SGUID& signalGUID, CRuntimeParams& params)
{
	for (SSlot& slot : m_slots)
	{
		if (slot.pObject)
		{
			slot.pObject->ProcessSignal(signalGUID, params);
		}
	}
}

bool CObjectPool::AllocateSlots(uint32 slotCount)
{
	const uint32 prevSlotCount = m_slots.size();
	if (slotCount > prevSlotCount)
	{
		const uint32 maxSlotCount = 0xffff;
		if (slotCount > maxSlotCount)
		{
			return false;
		}

		const uint32 minSlotCount = 100;
		slotCount = max(max(slotCount, min(prevSlotCount * 2, maxSlotCount)), minSlotCount);

		m_slots.resize(slotCount);
		m_freeSlots.reserve(slotCount);

		for (uint32 slotIdx = prevSlotCount; slotIdx < slotCount; ++slotIdx)
		{
			m_slots[slotIdx].objectId = CreateObjectId(slotIdx, 0);
			m_freeSlots.push_back(slotIdx);
		}
	}
	return true;
}

ObjectId CObjectPool::CreateObjectId(uint32 slotIdx, uint32 salt) const
{
	return static_cast<ObjectId>((slotIdx << 16) | salt);
}

bool CObjectPool::ExpandObjectId(ObjectId objectId, uint32& slotIdx, uint32& salt) const
{
	const uint32 value = static_cast<uint32>(objectId);
	slotIdx = value >> 16;
	salt = value & 0xffff;
	return (slotIdx < m_slots.size()) && (m_slots[slotIdx].objectId == objectId);
}

uint32 CObjectPool::IncrementSalt(uint32 salt) const
{
	const uint32 maxSalt = 0x7fff;
	if (++salt > maxSalt)
	{
		salt = 0;
	}
	return salt;
}
} // Schematyc
