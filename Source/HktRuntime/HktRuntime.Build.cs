// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HktRuntime : ModuleRules
{
	public HktRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);
				
		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);
			
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
                "NetCore",
                "EnhancedInput",
                "InputCore",
                "HktCore",
				"HktAsset",
				"Json",
				"JsonUtilities"
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DeveloperSettings"
			}
		);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PrivateDependencyModuleNames.Add("HktInsights");
            PrivateDefinitions.Add("WITH_HKT_INSIGHTS=1");
        }
        else
        {
            PrivateDefinitions.Add("WITH_HKT_INSIGHTS=0");
        }
    }
}
