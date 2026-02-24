// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HktVFXEditor : ModuleRules
{
	public HktVFXEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "..", "HktVFX", "Public"),
				System.IO.Path.Combine(ModuleDirectory, "..", "HktCore", "Public"),
				System.IO.Path.Combine(ModuleDirectory, "..", "HktRuntime", "Public"),
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
				"Slate",
				"SlateCore",
				"HktVFX",
				"HktCore",
				"HktRuntime",
				"HktAsset"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorStyle",
				"InputCore",
				"Niagara",
				"DeveloperSettings",
				"GameplayTags",
				"AssetRegistry"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
