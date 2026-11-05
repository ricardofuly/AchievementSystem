using UnrealBuildTool;

public class AchievementsSteam : ModuleRules
{
	public AchievementsSteam(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "AchievementsCore", "SteamShared" });

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
