#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "SteamAchievementPlatform.h"

/** Registers the Steam platform with the core; the core never links against Steam. */
class FAchievementsSteamModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(IAchievementPlatformFactory::GetModularFeatureName(), &Factory);
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(IAchievementPlatformFactory::GetModularFeatureName(), &Factory);
	}

private:
	FSteamAchievementPlatformFactory Factory;
};

IMPLEMENT_MODULE(FAchievementsSteamModule, AchievementsSteam)
