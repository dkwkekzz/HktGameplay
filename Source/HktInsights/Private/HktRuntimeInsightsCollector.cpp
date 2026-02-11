// Copyright HKT. All Rights Reserved.

#include "HktRuntimeInsightsCollector.h"
#include "HktInsightProvider.h"
#include "HktInsightsLog.h"
#include "Engine/Engine.h"

FHktRuntimeInsightsCollector& FHktRuntimeInsightsCollector::Get()
{
    static FHktRuntimeInsightsCollector Instance;
    return Instance;
}

FHktRuntimeInsightsCollector::FHktRuntimeInsightsCollector()
{
    PacketHistory.Reserve(MaxPacketHistory);
}

// ============================================================================
// Provider 관리
// ============================================================================

void FHktRuntimeInsightsCollector::RegisterProvider(UObject* Provider)
{
    if (!Provider) return;
    if (!Provider->GetClass()->ImplementsInterface(UHktInsightProvider::StaticClass()))
    {
        UE_LOG(LogHktInsights, Warning, TEXT("[RuntimeInsights] %s does not implement IHktInsightProvider"), *Provider->GetName());
        return;
    }

    // 중복 방지
    for (const TWeakObjectPtr<UObject>& Existing : Providers)
    {
        if (Existing.Get() == Provider) return;
    }

    Providers.Add(Provider);
    UE_LOG(LogHktInsights, Verbose, TEXT("[RuntimeInsights] Provider registered: %s"), *Provider->GetName());
}

void FHktRuntimeInsightsCollector::UnregisterProvider(UObject* Provider)
{
    Providers.RemoveAll([Provider](const TWeakObjectPtr<UObject>& Weak)
    {
        return !Weak.IsValid() || Weak.Get() == Provider;
    });
}

void FHktRuntimeInsightsCollector::CollectAll()
{
    if (!bEnabled) return;

    CachedSnapshots.Reset();

    // 만료된 Provider 정리
    Providers.RemoveAll([](const TWeakObjectPtr<UObject>& Weak) { return !Weak.IsValid(); });

    for (const TWeakObjectPtr<UObject>& Weak : Providers)
    {
        UObject* Obj = Weak.Get();
        if (!Obj) continue;

        IHktInsightProvider* Provider = Cast<IHktInsightProvider>(Obj);
        if (!Provider) continue;

        FHktInsightSnapshot Snapshot;
        Snapshot.ProviderName = Provider->GetInsightProviderName();
        Provider->CollectInsightData(Snapshot);

        if (Snapshot.Entries.Num() > 0)
        {
            CachedSnapshots.Add(MoveTemp(Snapshot));
        }
    }

    OnUpdated.Broadcast();
}

TArray<FHktInsightEntry> FHktRuntimeInsightsCollector::GetEntriesByCategory(const FString& Category) const
{
    TArray<FHktInsightEntry> Result;
    for (const FHktInsightSnapshot& Snapshot : CachedSnapshots)
    {
        for (const FHktInsightEntry& Entry : Snapshot.Entries)
        {
            if (Entry.Category == Category)
            {
                Result.Add(Entry);
            }
        }
    }
    return Result;
}

// ============================================================================
// Packet 기록
// ============================================================================

void FHktRuntimeInsightsCollector::RecordPacket(const FHktPacketRecord& Record)
{
    if (!bEnabled) return;

    FScopeLock Lock(&PacketLock);

    if (PacketHistory.Num() >= MaxPacketHistory)
    {
        // Ring buffer: 가장 오래된 것 제거
        PacketHistory.RemoveAt(0, PacketHistory.Num() - MaxPacketHistory + 1);
    }

    PacketHistory.Add(Record);
}

TArray<FHktPacketRecord> FHktRuntimeInsightsCollector::GetRecentPackets(int32 MaxCount) const
{
    FScopeLock Lock(&PacketLock);

    TArray<FHktPacketRecord> Result;
    int32 StartIndex = FMath::Max(0, PacketHistory.Num() - MaxCount);
    for (int32 i = PacketHistory.Num() - 1; i >= StartIndex; --i)
    {
        Result.Add(PacketHistory[i]);
    }
    return Result;
}

FHktPacketStats FHktRuntimeInsightsCollector::GetPacketStats() const
{
    double Now = GetCurrentTime();
    if (Now - LastStatsComputeTime < 0.25) // 캐시: 250ms 이내면 재사용
    {
        return CachedStats;
    }

    RecomputeStats();
    return CachedStats;
}

void FHktRuntimeInsightsCollector::RecomputeStats() const
{
    FScopeLock Lock(&PacketLock);

    double Now = GetCurrentTime();
    LastStatsComputeTime = Now;

    FHktPacketStats Stats;
    Stats.TotalPackets = PacketHistory.Num();

    const double WindowSeconds = 1.0;
    double WindowStart = Now - WindowSeconds;
    int32 WindowPackets = 0;
    int64 WindowBytes = 0;

    for (const FHktPacketRecord& Record : PacketHistory)
    {
        // 전체 통계
        if (Record.Direction == EHktPacketDirection::ClientToServer) Stats.C2S_Count++;
        else Stats.S2C_Count++;

        switch (Record.Type)
        {
        case EHktPacketType::Intent:       Stats.IntentCount++; break;
        case EHktPacketType::FrameBatch:   Stats.FrameBatchCount++; break;
        case EHktPacketType::InitialState: Stats.InitialStateCount++; break;
        default: break;
        }

        Stats.TotalBytes += Record.EstimatedSizeBytes;

        // 윈도우 통계
        if (Record.Timestamp >= WindowStart)
        {
            WindowPackets++;
            WindowBytes += Record.EstimatedSizeBytes;
        }
    }

    Stats.PacketsPerSecond = static_cast<float>(WindowPackets) / static_cast<float>(WindowSeconds);
    Stats.BytesPerSecond = static_cast<float>(WindowBytes) / static_cast<float>(WindowSeconds);

    CachedStats = Stats;
}

// ============================================================================
// 유틸리티
// ============================================================================

void FHktRuntimeInsightsCollector::Clear()
{
    CachedSnapshots.Reset();

    {
        FScopeLock Lock(&PacketLock);
        PacketHistory.Reset();
    }

    CachedStats = FHktPacketStats();
    OnUpdated.Broadcast();
}

double FHktRuntimeInsightsCollector::GetCurrentTime() const
{
    return FPlatformTime::Seconds();
}
