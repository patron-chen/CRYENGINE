// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace client
	{
		namespace internal
		{

			//===================================================================================
			//
			// CInputParameterRegistry
			//
			// - used by various factories to store the kind of parameter they expect
			// - used by various blueprints to check for type consistency at load time
			//
			//===================================================================================

			class CInputParameterRegistry final : public IInputParameterRegistry
			{
			private:
				struct SStoredParameterInfo
				{
					explicit                        SStoredParameterInfo(const char* _name, const shared::CTypeInfo& _type, size_t _offset);

					string                          name;
					const shared::CTypeInfo&        type;
					size_t                          offset;
				};

			public:
				virtual size_t                      GetParameterCount() const override;
				virtual SParameterInfo              GetParameter(size_t index) const override;

				void                                RegisterParameterType(const char* paramName, const shared::CTypeInfo& typeInfo, size_t offset);

			private:
				std::vector<SStoredParameterInfo>   m_parametersInOrder;
			};

		}
	}
}
