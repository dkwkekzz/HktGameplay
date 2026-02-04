// Copyright HKT. All Rights Reserved.

using UnrealBuildTool;

public class HktInsights : ModuleRules
{
    public HktInsights(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 이 모듈을 사용하는 측에서 WITH_HKT_INSIGHTS 매크로 사용 가능
        PublicDefinitions.Add("WITH_HKT_INSIGHTS=1");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "InputCore",
            "GameplayTags"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // 프로젝트의 실제 모듈명에 맞게 수정 필요
            // "HktCore",
            // "HktRuntime",
            // "HktService"
        });

        // 에디터 빌드에서만 Slate 윈도우 기능 활성화
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
