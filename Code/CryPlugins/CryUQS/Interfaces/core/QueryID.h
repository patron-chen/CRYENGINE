// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// CQueryID
		//
		// - represents a handle to a query instance inside the IQueryManager
		// - 0 means "invalid" query
		//
		//===================================================================================

		class CQueryID
		{
		public:
			static CQueryID     CreateInvalid();

			bool                IsValid() const;
			CQueryID&           operator++();
			bool                operator==(const CQueryID& rhs) const;
			bool                operator!=(const CQueryID& rhs) const;
			bool                operator<(const CQueryID& rhs) const;
			void                ToString(shared::IUqsString& out) const;
			void                Serialize(Serialization::IArchive& ar);

		private:
			explicit            CQueryID();
			uint32              m_id;
		};

		inline CQueryID::CQueryID()
			: m_id(0)
		{}

		inline CQueryID CQueryID::CreateInvalid()
		{
			return CQueryID();
		}

		inline bool CQueryID::IsValid() const
		{
			return m_id != 0;
		}

		inline bool CQueryID::operator==(const CQueryID& rhs) const
		{
			return m_id == rhs.m_id;
		}

		inline bool CQueryID::operator!=(const CQueryID& rhs) const
		{
			return m_id != rhs.m_id;
		}

		inline CQueryID& CQueryID::operator++()
		{
			++m_id;
			return *this;
		}

		inline bool CQueryID::operator<(const CQueryID& rhs) const
		{
			return m_id < rhs.m_id;
		}

		inline void CQueryID::ToString(shared::IUqsString& out) const
		{
			out.Format("%i", (int)m_id);
		}

		inline void CQueryID::Serialize(Serialization::IArchive& ar)
		{
			ar(m_id, "m_id");
		}

	}
}
