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
// 엔티티 리스트
// ============================================================================

void FHktRuntimeInsightsCollector::SyncEntityList(const FString& Source, TArray<FHktEntityListEntry>&& NewEntries)
{
    if (!bEnabled) return;

    FScopeLock Lock(&EntityListLock);

    TArray<FHktEntityListEntry>& Existing = EntityListBySource.FindOrAdd(Source);

    // 빠른 변경 감지: 개수가 다르면 무조건 변경
    if (Existing.Num() != NewEntries.Num())
    {
        Existing = MoveTemp(NewEntries);
        ++EntityListVersion;
        return;
    }

    // 개수 동일 → EntityId 집합 비교 (정렬 없이 O(N) 비교)
    bool bChanged = false;
    for (int32 i = 0; i < Existing.Num(); ++i)
    {
        if (Existing[i].EntityId != NewEntries[i].EntityId)
        {
            bChanged = true;
            break;
        }
    }

    if (bChanged)
    {
        Existing = MoveTemp(NewEntries);
        ++EntityListVersion;
    }
}

TArray<FHktEntityListEntry> FHktRuntimeInsightsCollector::GetEntityList() const
{
    FScopeLock Lock(&EntityListLock);

    TArray<FHktEntityListEntry> Result;
    for (const auto& Pair : EntityListBySource)
    {
        Result.Append(Pair.Value);
    }
    return Result;
}

int32 FHktRuntimeInsightsCollector::GetEntityListVersion() const
{
    FScopeLock Lock(&EntityListLock);
    return EntityListVersion;
}

// ============================================================================
// 선택 엔티티
// ============================================================================

void FHktRuntimeInsightsCollector::SetSelectedEntity(const FString& Source, int32 EntityId)
{
    FScopeLock Lock(&SelectionLock);
    SelectedEntity.Source = Source;
    SelectedEntity.EntityId = EntityId;
    CachedDetail = FHktSelectedEntityDetail(); // 리셋
}

FHktEntitySelection FHktRuntimeInsightsCollector::GetSelectedEntity() const
{
    FScopeLock Lock(&SelectionLock);
    return SelectedEntity;
}

void FHktRuntimeInsightsCollector::PushSelectedEntityDetail(FHktSelectedEntityDetail&& Detail)
{
    if (!bEnabled) return;

    FScopeLock Lock(&SelectionLock);
    CachedDetail = MoveTemp(Detail);
}

const FHktSelectedEntityDetail& FHktRuntimeInsightsCollector::GetSelectedEntityDetail() const
{
    // Note: caller should be on game thread; no lock needed for read-only access
    return CachedDetail;
}

// ============================================================================
// 다중 엔티티 상세
// ============================================================================

void FHktRuntimeInsightsCollector::PushAllEntityDetails(const FString& Source, TArray<FHktSelectedEntityDetail>&& Details)
{
    if (!bEnabled) return;

    FScopeLock Lock(&DetailLock);
    EntityDetailsBySource.FindOrAdd(Source) = MoveTemp(Details);
}

TArray<FHktSelectedEntityDetail> FHktRuntimeInsightsCollector::GetAllEntityDetails(const FString& Source) const
{
    FScopeLock Lock(&DetailLock);
    if (const TArray<FHktSelectedEntityDetail>* Found = EntityDetailsBySource.Find(Source))
    {
        return *Found;
    }
    return {};
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

    {
        FScopeLock Lock(&EntityListLock);
        EntityListBySource.Empty();
        ++EntityListVersion;
    }

    {
        FScopeLock Lock(&SelectionLock);
        SelectedEntity.Reset();
        CachedDetail = FHktSelectedEntityDetail();
    }

    {
        FScopeLock Lock(&DetailLock);
        EntityDetailsBySource.Empty();
    }

    CachedStats = FHktPacketStats();
    OnUpdated.Broadcast();
}

double FHktRuntimeInsightsCollector::GetCurrentTime() const
{
    return FPlatformTime::Seconds();
}
