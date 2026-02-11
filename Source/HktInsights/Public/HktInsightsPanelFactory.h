// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

/**
 * HKT Insights 패널 팩토리
 * 
 * 외부 모듈(HktInsightsEditor 등)에서 Insights 패널을 생성할 수 있도록
 * Public API를 제공합니다.
 */
class HKTINSIGHTS_API FHktInsightsPanelFactory
{
public:
    /**
     * HKT Insights 패널 위젯 생성
     * @return 새로 생성된 패널 위젯
     */
    static TSharedRef<SWidget> CreatePanel();

    /**
     * 자동 새로고침 간격을 지정하여 패널 생성
     * @param AutoRefreshInterval 자동 새로고침 간격 (초)
     * @return 새로 생성된 패널 위젯
     */
    static TSharedRef<SWidget> CreatePanel(float AutoRefreshInterval);

    /**
     * 런타임 인사이트 패널 생성 (Provider + Packet)
     * @return 새로 생성된 런타임 패널 위젯
     */
    static TSharedRef<SWidget> CreateRuntimePanel();

    /**
     * 런타임 인사이트 패널 (자동 새로고침 간격 지정)
     * @param AutoRefreshInterval 자동 새로고침 간격 (초)
     * @return 새로 생성된 런타임 패널 위젯
     */
    static TSharedRef<SWidget> CreateRuntimePanel(float AutoRefreshInterval);

    /**
     * 통합 패널 (기존 + 런타임 탭)
     * @return 새로 생성된 통합 패널 위젯
     */
    static TSharedRef<SWidget> CreateCombinedPanel();
};
