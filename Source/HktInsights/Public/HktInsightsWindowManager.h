// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SHktInsightsPanel;
class SWindow;

/**
 * HKT 디버그 윈도우 관리자
 * 
 * 런타임에서 독립적인 디버그 윈도우를 생성하고 관리합니다.
 * 에디터/PIE 모두에서 사용 가능합니다.
 */
class HKTINSIGHTS_API FHktInsightsWindowManager
{
public:
    /** 싱글톤 인스턴스 반환 */
    static FHktInsightsWindowManager& Get();

    /** 디버그 윈도우 열기 */
    void OpenWindow();

    /** 디버그 윈도우 닫기 */
    void CloseWindow();

    /** 디버그 윈도우 토글 */
    void ToggleWindow();

    /** 디버그 윈도우가 열려있는지 확인 */
    bool IsWindowOpen() const;

    /** 디버그 패널 포인터 반환 (UI 테스트용) */
    TSharedPtr<SHktInsightsPanel> GetDebugPanel() const { return DebugPanel; }

    /** 윈도우 크기 설정 */
    void SetWindowSize(FVector2D NewSize);

    /** 윈도우 위치 설정 */
    void SetWindowPosition(FVector2D NewPosition);

private:
    FHktInsightsWindowManager() = default;
    ~FHktInsightsWindowManager();

    // Non-copyable
    FHktInsightsWindowManager(const FHktInsightsWindowManager&) = delete;
    FHktInsightsWindowManager& operator=(const FHktInsightsWindowManager&) = delete;

    /** 윈도우 생성 */
    void CreateWindow();

    /** 윈도우가 닫힐 때 콜백 */
    void OnWindowClosed(const TSharedRef<SWindow>& Window);

    /** 디버그 윈도우 */
    TSharedPtr<SWindow> DebugWindow;

    /** 디버그 패널 위젯 */
    TSharedPtr<SHktInsightsPanel> DebugPanel;

    /** 기본 윈도우 크기 */
    FVector2D DefaultWindowSize = FVector2D(900, 650);

    /** 마지막 윈도우 위치 (재사용) */
    FVector2D LastWindowPosition = FVector2D::ZeroVector;
    bool bHasLastPosition = false;
};
