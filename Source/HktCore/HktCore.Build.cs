// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HktCore : ModuleRules
{
	public HktCore(ReadOnlyTargetRules Target) : base(Target)
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
				"GameplayTags"
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		// HktInsights - 개발/에디터 빌드에서만 활성화
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
