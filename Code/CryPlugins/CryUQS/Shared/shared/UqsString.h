// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace shared
	{

		//===================================================================================
		//
		// CUqsString
		//
		//===================================================================================

		class CUqsString : public IUqsString
		{
		public:
			virtual void                  Set(const char* szString) override;
			virtual void                  Format(const char* fmt, ...) override PRINTF_PARAMS(2, 3);
			virtual const char*           c_str() const override;

		private:
			string                        m_message;
		};

	}
}
