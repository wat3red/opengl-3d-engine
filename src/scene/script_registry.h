#pragma once

#include "scriptable_entity.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

class ScriptRegistry {
public:
	using ScriptCreator = std::function<ScriptableEntity*()>;

	template<typename T>
	static void Register(const std::string& name)
	{
		auto& creators = GetCreators();
		if (creators.find(name) == creators.end())
		{
			creators[name] = []() -> ScriptableEntity* { return static_cast<ScriptableEntity*>(new T()); };
			GetRegisteredNamesList().push_back(name);
		}
	}

	static ScriptableEntity* Instantiate(const std::string& name)
	{
		auto& creators = GetCreators();
		auto it = creators.find(name);
		if (it != creators.end())
		{
			return it->second();
		}
		return nullptr;
	}

	static bool Has(const std::string& name)
	{
		auto& creators = GetCreators();
		return creators.find(name) != creators.end();
	}

	static const std::vector<std::string>& GetRegisteredNames()
	{
		return GetRegisteredNamesList();
	}

private:
	static std::unordered_map<std::string, ScriptCreator>& GetCreators()
	{
		static std::unordered_map<std::string, ScriptCreator> s_Creators;
		return s_Creators;
	}

	static std::vector<std::string>& GetRegisteredNamesList()
	{
		static std::vector<std::string> s_Names;
		return s_Names;
	}
};

#define REGISTER_SCRIPT(Type) \
	namespace { \
		struct Type##_ScriptRegistrar { \
			Type##_ScriptRegistrar() { \
				::ScriptRegistry::Register<Type>(#Type); \
			} \
		}; \
		static Type##_ScriptRegistrar s_##Type##_Registrar; \
	}
