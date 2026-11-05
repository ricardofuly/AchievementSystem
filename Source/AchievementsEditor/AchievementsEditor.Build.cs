using UnrealBuildTool;

public class AchievementsEditor : ModuleRules
{
	public AchievementsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AchievementsCore",
			"AssetRegistry",
			"GameplayTags",
			"Projects",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd",
			"WebBrowser"
		});
	}
}
