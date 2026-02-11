// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktInsightProvider.h"
#include "HktInsightsRuntimeTypes.h"

DECLARE_MULTICAST_DELEGATE(FOnHktRuntimeInsightsUpdated);

/**
 * FHktRuntimeInsightsCollector - 런타임 인사이트 데이터 중앙 수집기
 *
 * 두 가지 채널로 데이터를 수집합니다:
 *
 * 1) Provider 기반 (Pull 모델):
 *    - IHktInsightProvider를 구현한 오브젝트를 RegisterProvider()로 등록
 *    - CollectAll()이 호출되면 모든 Provider의 CollectInsightData()를 호출
 *    - GameMode, PlayerController, Component들이 자신의 상태를 Push
 *
 * 2) Packet 기록 (Push 모델):
 *    - RecordPacket()으로 RPC 송수신 시점에 기록
 *    - Ring buffer 기반으로 최근 N개의 패킷 기록 유지
 *    - 1초 윈도우 통계(패킷/초, 바이트/초) 자동 계산
 *
 * 패킷 수집 전략:
 *    기존 코드에 RecordPacket() 1줄을 추가하는 방식입니다.
 *    - Server_ReceiveIntent_Implementation → RecordPacket(C2S, Intent)
 *    - Client_ReceiveFrameBatch_Implementation → RecordPacket(S2C, FrameBatch)
 *    - SendFrameBatch/SendInitialSimulationState → RecordPacket(S2C, ...)
 *    이 방식은 기존 인터페이스를 변경하지 않으며,
 *    WITH_HKT_INSIGHTS 매크로로 Shipping에서 제거됩니다.
 */
class HKTINSIGHTS_API FHktRuntimeInsightsCollector
{
public:
    static FHktRuntimeInsightsCollector& Get();

    // ========== Provider 관리 ==========

    /** Provider 등록 (UObject 기반, weak reference) */
    void RegisterProvider(UObject* Provider);

    /** Provider 해제 */
    void UnregisterProvider(UObject* Provider);

    /** 등록된 모든 Provider에서 데이터 수집 */
    void CollectAll();

    /** 수집된 스냅샷 전체 조회 */
    const TArray<FHktInsightSnapshot>& GetSnapshots() const { return CachedSnapshots; }

    /** 특정 카테고리의 Entry만 필터링 */
    TArray<FHktInsightEntry> GetEntriesByCategory(const FString& Category) const;

    // ========== Packet 기록 ==========

    /**
     * 패킷 기록 (RPC 호출 지점에서 사용)
     *
     * 사용 예:
     *   HKT_INSIGHTS_RECORD_PACKET(EHktPacketDirection::ClientToServer,
     *       EHktPacketType::Intent, PlayerUid, 0, 1, sizeof(FHktRuntimeEvent), TEXT("Intent RPC"));
     */
    void RecordPacket(const FHktPacketRecord& Record);

    /** 최근 패킷 기록 조회 */
    TArray<FHktPacketRecord> GetRecentPackets(int32 MaxCount = 100) const;

    /** 1초 윈도우 통계 조회 */
    FHktPacketStats GetPacketStats() const;

    // ========== 설정 ==========

    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
    bool IsEnabled() const { return bEnabled; }

    void SetMaxPacketHistory(int32 Size) { MaxPacketHistory = FMath::Max(10, Size); }

    void Clear();

    // ========== Delegate ==========

    FOnHktRuntimeInsightsUpdated OnUpdated;

private:
    FHktRuntimeInsightsCollector();
    ~FHktRuntimeInsightsCollector() = default;

    FHktRuntimeInsightsCollector(const FHktRuntimeInsightsCollector&) = delete;
    FHktRuntimeInsightsCollector& operator=(const FHktRuntimeInsightsCollector&) = delete;

    /** 등록된 Provider들 (Weak reference) */
    TArray<TWeakObjectPtr<UObject>> Providers;

    /** 최근 수집된 스냅샷 */
    TArray<FHktInsightSnapshot> CachedSnapshots;

    /** 패킷 기록 (Ring buffer) */
    mutable FCriticalSection PacketLock;
    TArray<FHktPacketRecord> PacketHistory;
    int32 MaxPacketHistory = 500;

    /** 통계 캐시 */
    mutable FHktPacketStats CachedStats;
    mutable double LastStatsComputeTime = 0.0;

    bool bEnabled = true;

    /** 통계 재계산 */
    void RecomputeStats() const;

    double GetCurrentTime() const;
};

// ========== 패킷 기록 매크로 ==========

#if WITH_HKT_INSIGHTS

    #define HKT_INSIGHTS_RECORD_PACKET(InDirection, InType, InPlayerUid, InFrameNum, InEventCount, InSizeBytes, InDesc) \
        do { \
            FHktPacketRecord __Record; \
            __Record.Timestamp = FPlatformTime::Seconds(); \
            __Record.Direction = InDirection; \
            __Record.Type = InType; \
            __Record.PlayerUid = InPlayerUid; \
            __Record.FrameNumber = InFrameNum; \
            __Record.EventCount = InEventCount; \
            __Record.EstimatedSizeBytes = InSizeBytes; \
            __Record.Description = InDesc; \
            FHktRuntimeInsightsCollector::Get().RecordPacket(__Record); \
        } while(0)

    #define HKT_INSIGHTS_REGISTER_PROVIDER(Obj) \
        FHktRuntimeInsightsCollector::Get().RegisterProvider(Obj)

    #define HKT_INSIGHTS_UNREGISTER_PROVIDER(Obj) \
        FHktRuntimeInsightsCollector::Get().UnregisterProvider(Obj)

#else

    #define HKT_INSIGHTS_RECORD_PACKET(InDirection, InType, InPlayerUid, InFrameNum, InEventCount, InSizeBytes, InDesc)
    #define HKT_INSIGHTS_REGISTER_PROVIDER(Obj)
    #define HKT_INSIGHTS_UNREGISTER_PROVIDER(Obj)

#endif
