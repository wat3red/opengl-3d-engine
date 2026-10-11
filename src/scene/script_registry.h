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
		RegisterCustom(name, []() -> ScriptableEntity* { return static_cast<ScriptableEntity*>(new T()); });
	}

	static void RegisterCustom(const std::string& name, ScriptCreator creator)
	{
		auto& creators = GetCreators();
		creators[name] = creator;
		auto& names = GetRegisteredNamesList();
		if (std::find(names.begin(), names.end(), name) == names.end())
		{
			names.push_back(name);
		}
	}

	static void Clear()
	{
		GetCreators().clear();
		GetRegisteredNamesList().clear();
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
