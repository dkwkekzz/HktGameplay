// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * HktInsights 모듈 인터페이스
 * 런타임 디버깅 기능을 제공하는 모듈
 */
class HKTINSIGHTS_API IHktInsightsModule : public IModuleInterface
{
public:
    /**
     * 모듈 인스턴스 반환
     */
    static IHktInsightsModule& Get()
    {
        return FModuleManager::LoadModuleChecked<IHktInsightsModule>("HktInsights");
    }

    /**
     * 모듈이 로드되었는지 확인
     */
    static bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("HktInsights");
    }

    /**
     * 디버그 윈도우 토글
     */
    virtual void ToggleDebugWindow() = 0;

    /**
     * 디버그 윈도우 열기
     */
    virtual void OpenDebugWindow() = 0;

    /**
     * 디버그 윈도우 닫기
     */
    virtual void CloseDebugWindow() = 0;

    /**
     * 디버그 윈도우가 열려있는지 확인
     */
    virtual bool IsDebugWindowOpen() const = 0;

    /**
     * 데이터 수집 활성화/비활성화
     */
    virtual void SetDataCollectionEnabled(bool bEnabled) = 0;

    /**
     * 데이터 수집이 활성화되어 있는지 확인
     */
    virtual bool IsDataCollectionEnabled() const = 0;

    /**
     * 수집된 모든 데이터 클리어
     */
    virtual void ClearAllData() = 0;
};
