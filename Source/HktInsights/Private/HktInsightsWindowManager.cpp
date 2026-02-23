// Copyright HKT. All Rights Reserved.

#include "HktInsightsWindowManager.h"
#include "HktInsightsLog.h"
#include "HktInsightsPanelFactory.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"

FHktInsightsWindowManager& FHktInsightsWindowManager::Get()
{
    static FHktInsightsWindowManager Instance;
    return Instance;
}

FHktInsightsWindowManager::~FHktInsightsWindowManager()
{
    CloseWindow();
}

void FHktInsightsWindowManager::OpenWindow()
{
    if (IsWindowOpen())
    {
        // 이미 열려있으면 포커스만
        if (DebugWindow.IsValid())
        {
            DebugWindow->BringToFront();
        }
        return;
    }

    CreateWindow();
}

void FHktInsightsWindowManager::CloseWindow()
{
    if (DebugWindow.IsValid())
    {
        // 위치 저장
        LastWindowPosition = DebugWindow->GetPositionInScreen();
        bHasLastPosition = true;

        DebugWindow->RequestDestroyWindow();
        DebugWindow.Reset();
    }

    DebugPanel.Reset();
}

void FHktInsightsWindowManager::ToggleWindow()
{
    if (IsWindowOpen())
    {
        CloseWindow();
    }
    else
    {
        OpenWindow();
    }
}

bool FHktInsightsWindowManager::IsWindowOpen() const
{
    return DebugWindow.IsValid() && DebugWindow->IsVisible();
}

void FHktInsightsWindowManager::SetWindowSize(FVector2D NewSize)
{
    DefaultWindowSize = NewSize;
    
    if (DebugWindow.IsValid())
    {
        DebugWindow->Resize(NewSize);
    }
}

void FHktInsightsWindowManager::SetWindowPosition(FVector2D NewPosition)
{
    if (DebugWindow.IsValid())
    {
        DebugWindow->MoveWindowTo(NewPosition);
    }
}

void FHktInsightsWindowManager::CreateWindow()
{
    // 통합 패널 생성 (Intent/VM + Runtime State + World State 탭)
    TSharedRef<SWidget> CombinedPanel = FHktInsightsPanelFactory::CreateCombinedPanel();

    // 윈도우 생성
    DebugWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("HKT Insights")))
        .ClientSize(DefaultWindowSize)
        .SupportsMinimize(true)
        .SupportsMaximize(true)
        .IsTopmostWindow(false)
        .SizingRule(ESizingRule::UserSized)
        .AutoCenter(bHasLastPosition ? EAutoCenter::None : EAutoCenter::PreferredWorkArea)
        .ScreenPosition(bHasLastPosition ? LastWindowPosition : FVector2D::ZeroVector)
        [
            CombinedPanel
        ];

    // 윈도우 닫힘 콜백 등록
    DebugWindow->SetOnWindowClosed(
        FOnWindowClosed::CreateRaw(this, &FHktInsightsWindowManager::OnWindowClosed));

    // Slate 앱에 윈도우 추가
    FSlateApplication::Get().AddWindow(DebugWindow.ToSharedRef());

    UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Debug window opened"));
}

void FHktInsightsWindowManager::OnWindowClosed(const TSharedRef<SWindow>& Window)
{
    if (DebugWindow == Window)
    {
        // 위치 저장
        LastWindowPosition = Window->GetPositionInScreen();
        bHasLastPosition = true;

        DebugWindow.Reset();
        DebugPanel.Reset();

        UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Debug window closed"));
    }
}
