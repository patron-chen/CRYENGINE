// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace shared
	{

		//===================================================================================
		//
		// IUqsString
		//
		//===================================================================================

		struct IUqsString
		{
			virtual                       ~IUqsString() {}
			virtual void                  Set(const char* szString) = 0;
			virtual void                  Format(const char* fmt, ...) PRINTF_PARAMS(2, 3) = 0;
			virtual const char*           c_str() const = 0;
		};

	}
}
