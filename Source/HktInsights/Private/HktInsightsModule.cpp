// Copyright HKT. All Rights Reserved.

#include "IHktInsightsModule.h"
#include "HktInsightsDataCollector.h"
#include "HktInsightsLog.h"
#include "HktInsightsWindowManager.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogHktInsights);

#define LOCTEXT_NAMESPACE "HktInsights"

/**
 * HktInsights 모듈 구현
 */
class FHktInsightsModule : public IHktInsightsModule
{
public:
    // IModuleInterface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    // IHktInsightsModule
    virtual void ToggleDebugWindow() override;
    virtual void OpenDebugWindow() override;
    virtual void CloseDebugWindow() override;
    virtual bool IsDebugWindowOpen() const override;
    virtual void SetDataCollectionEnabled(bool bEnabled) override;
    virtual bool IsDataCollectionEnabled() const override;
    virtual void ClearAllData() override;

private:
    /** 콘솔 명령어 등록 */
    void RegisterConsoleCommands();

    /** 콘솔 명령어 해제 */
    void UnregisterConsoleCommands();

    /** 콘솔 명령어 핸들들 */
    TArray<IConsoleObject*> ConsoleCommands;
};

IMPLEMENT_MODULE(FHktInsightsModule, HktInsights)

void FHktInsightsModule::StartupModule()
{
    UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Module starting up..."));

    // 콘솔 명령어 등록
    RegisterConsoleCommands();

    UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Module startup complete. Use 'hkt.insights.toggle' to open debug window."));
}

void FHktInsightsModule::ShutdownModule()
{
    UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Module shutting down..."));

    // 윈도우 닫기
    FHktInsightsWindowManager::Get().CloseWindow();

    // 콘솔 명령어 해제
    UnregisterConsoleCommands();

    // 데이터 클리어
    FHktInsightsDataCollector::Get().Clear();

    UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Module shutdown complete."));
}

void FHktInsightsModule::ToggleDebugWindow()
{
    FHktInsightsWindowManager::Get().ToggleWindow();
}

void FHktInsightsModule::OpenDebugWindow()
{
    FHktInsightsWindowManager::Get().OpenWindow();
}

void FHktInsightsModule::CloseDebugWindow()
{
    FHktInsightsWindowManager::Get().CloseWindow();
}

bool FHktInsightsModule::IsDebugWindowOpen() const
{
    return FHktInsightsWindowManager::Get().IsWindowOpen();
}

void FHktInsightsModule::SetDataCollectionEnabled(bool bEnabled)
{
    FHktInsightsDataCollector::Get().SetEnabled(bEnabled);
}

bool FHktInsightsModule::IsDataCollectionEnabled() const
{
    return FHktInsightsDataCollector::Get().IsEnabled();
}

void FHktInsightsModule::ClearAllData()
{
    FHktInsightsDataCollector::Get().Clear();
}

void FHktInsightsModule::RegisterConsoleCommands()
{
    // 디버그 윈도우 토글
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.toggle"),
        TEXT("Toggle HKT Insights debug window"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            FHktInsightsWindowManager::Get().ToggleWindow();
        }),
        ECVF_Default
    ));

    // 디버그 윈도우 열기
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.open"),
        TEXT("Open HKT Insights debug window"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            FHktInsightsWindowManager::Get().OpenWindow();
        }),
        ECVF_Default
    ));

    // 디버그 윈도우 닫기
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.close"),
        TEXT("Close HKT Insights debug window"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            FHktInsightsWindowManager::Get().CloseWindow();
        }),
        ECVF_Default
    ));

    // 데이터 클리어
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.clear"),
        TEXT("Clear all HKT Insights debug data"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            FHktInsightsDataCollector::Get().Clear();
            UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Debug data cleared."));
        }),
        ECVF_Default
    ));

    // 데이터 수집 활성화/비활성화
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.enable"),
        TEXT("Enable HKT Insights data collection"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            FHktInsightsDataCollector::Get().SetEnabled(true);
        }),
        ECVF_Default
    ));

    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.disable"),
        TEXT("Disable HKT Insights data collection"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            FHktInsightsDataCollector::Get().SetEnabled(false);
        }),
        ECVF_Default
    ));

    // 통계 출력
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.stats"),
        TEXT("Print HKT Insights statistics"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            FHktInsightsStats Stats = FHktInsightsDataCollector::Get().GetStats();
            UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] === Statistics ==="));
            UE_LOG(LogHktInsights, Log, TEXT("  Total Events: %d"), Stats.TotalEventCount);
            UE_LOG(LogHktInsights, Log, TEXT("  Pending Events: %d"), Stats.PendingEventCount);
            UE_LOG(LogHktInsights, Log, TEXT("  Failed Events: %d"), Stats.FailedEventCount);
            UE_LOG(LogHktInsights, Log, TEXT("  Active VMs: %d"), Stats.ActiveVMCount);
            UE_LOG(LogHktInsights, Log, TEXT("  Completed VMs: %d"), Stats.CompletedVMCount);
            UE_LOG(LogHktInsights, Log, TEXT("  Avg VM Time: %.3fms"), Stats.AverageVMExecutionTime * 1000.0f);
        }),
        ECVF_Default
    ));

    // 히스토리 크기 설정
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.maxhistory"),
        TEXT("Set maximum history size (usage: hkt.insights.maxhistory <size>)"),
        FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
        {
            if (Args.Num() > 0)
            {
                int32 Size = FCString::Atoi(*Args[0]);
                if (Size > 0)
                {
                    FHktInsightsDataCollector::Get().SetMaxHistorySize(Size);
                    UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Max history size set to %d"), Size);
                }
                else
                {
                    UE_LOG(LogHktInsights, Warning, TEXT("[HktInsights] Invalid size. Must be positive integer."));
                }
            }
            else
            {
                UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Current max history size: %d"), 
                    FHktInsightsDataCollector::Get().GetMaxHistorySize());
            }
        }),
        ECVF_Default
    ));

    // 상세 로그 토글
    static bool bVerboseLogging = false;
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("hkt.insights.verbose"),
        TEXT("Toggle verbose logging"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            bVerboseLogging = !bVerboseLogging;
            // LogHktInsights의 Verbosity를 조정
            UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Verbose logging %s"), 
                bVerboseLogging ? TEXT("enabled") : TEXT("disabled"));
        }),
        ECVF_Default
    ));
}

void FHktInsightsModule::UnregisterConsoleCommands()
{
    for (IConsoleObject* Cmd : ConsoleCommands)
    {
        if (Cmd)
        {
            IConsoleManager::Get().UnregisterConsoleObject(Cmd);
        }
    }
    ConsoleCommands.Empty();
}

#undef LOCTEXT_NAMESPACE
