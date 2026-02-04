// Copyright HKT. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ToolMenus.h"
#include "HktInsightsLog.h"
#include "IHktInsightsModule.h"
#include "HktInsightsPanelFactory.h"

#define LOCTEXT_NAMESPACE "HktInsightsEditor"

DEFINE_LOG_CATEGORY_STATIC(LogHktInsightsEditor, Log, All);

static const FName HktInsightsTabName(TEXT("HktInsightsTab"));

/**
 * HktInsightsEditor 모듈 구현
 * 에디터 전용 기능 (도킹 탭, 메뉴 통합)
 */
class FHktInsightsEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    /** 도킹 탭 스폰 */
    TSharedRef<SDockTab> SpawnDebugTab(const FSpawnTabArgs& Args);

    /** 메뉴 확장 등록 */
    void RegisterMenuExtensions();

    /** 메뉴 확장 해제 */
    void UnregisterMenuExtensions();

    /** 탭 스포너 등록됨 */
    bool bTabSpawnerRegistered = false;
};

IMPLEMENT_MODULE(FHktInsightsEditorModule, HktInsightsEditor)

void FHktInsightsEditorModule::StartupModule()
{
    UE_LOG(LogHktInsightsEditor, Log, TEXT("[HktInsightsEditor] Module starting up..."));

    // 탭 스포너 등록
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        HktInsightsTabName,
        FOnSpawnTab::CreateRaw(this, &FHktInsightsEditorModule::SpawnDebugTab))
        .SetDisplayName(LOCTEXT("HktInsightsTabTitle", "HKT Insights"))
        .SetTooltipText(LOCTEXT("HktInsightsTabTooltip", "Open the HKT Insights debug panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Debug"));

    bTabSpawnerRegistered = true;

    // 메뉴 확장 등록
    RegisterMenuExtensions();

    UE_LOG(LogHktInsightsEditor, Log, TEXT("[HktInsightsEditor] Module startup complete."));
}

void FHktInsightsEditorModule::ShutdownModule()
{
    UE_LOG(LogHktInsightsEditor, Log, TEXT("[HktInsightsEditor] Module shutting down..."));

    // 메뉴 확장 해제
    UnregisterMenuExtensions();

    // 탭 스포너 해제
    if (bTabSpawnerRegistered)
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(HktInsightsTabName);
        bTabSpawnerRegistered = false;
    }

    UE_LOG(LogHktInsightsEditor, Log, TEXT("[HktInsightsEditor] Module shutdown complete."));
}

TSharedRef<SDockTab> FHktInsightsEditorModule::SpawnDebugTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("HktInsightsTabLabel", "HKT Insights"))
        [
            FHktInsightsPanelFactory::CreatePanel()
        ];
}

void FHktInsightsEditorModule::RegisterMenuExtensions()
{
    // ToolMenus가 초기화될 때까지 대기
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
    {
        // Window 메뉴에 항목 추가
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
        if (Menu)
        {
            FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
            
            Section.AddMenuEntry(
                "HktInsights",
                LOCTEXT("HktInsightsMenuEntry", "HKT Insights"),
                LOCTEXT("HktInsightsMenuTooltip", "Open the HKT Insights debug panel"),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "Debug"),
                FUIAction(FExecuteAction::CreateLambda([]()
                {
                    FGlobalTabmanager::Get()->TryInvokeTab(HktInsightsTabName);
                }))
            );
        }

        // Tools 메뉴에도 추가
        UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
        if (ToolsMenu)
        {
            FToolMenuSection& Section = ToolsMenu->FindOrAddSection("Instrumentation");

            Section.AddMenuEntry(
                "HktInsightsTools",
                LOCTEXT("HktInsightsToolsMenuEntry", "HKT Insights"),
                LOCTEXT("HktInsightsToolsMenuTooltip", "Open the HKT Insights debug panel"),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "Debug"),
                FUIAction(FExecuteAction::CreateLambda([]()
                {
                    FGlobalTabmanager::Get()->TryInvokeTab(HktInsightsTabName);
                }))
            );
        }
    }));
}

void FHktInsightsEditorModule::UnregisterMenuExtensions()
{
    // ToolMenus 해제는 자동으로 처리됨
    UToolMenus::UnRegisterStartupCallback(this);
}

#undef LOCTEXT_NAMESPACE
