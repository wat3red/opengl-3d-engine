#include "script_registry.h"
#include "components.h"

bool NativeScriptComponent::Bind(const std::string& name)
{
	ScriptName = name;
	if (ScriptRegistry::Has(name))
	{
		InstantiateScript = [name]() -> ScriptableEntity* {
			return ScriptRegistry::Instantiate(name);
		};
		DestroyScript = [](NativeScriptComponent* nsc) {
			delete nsc->Instance;
			nsc->Instance = nullptr;
		};
		return true;
	}
	return false;
}
