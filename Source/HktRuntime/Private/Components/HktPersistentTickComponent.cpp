// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPersistentTickComponent.h"
#include "HktFilePersistentTickProvider.h"

UHktPersistentTickComponent::UHktPersistentTickComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    Provider = MakeUnique<FHktFilePersistentTickProvider>();
}

void UHktPersistentTickComponent::BeginPlay()
{
    Super::BeginPlay();
    ReserveNextBatch();
}

int64 UHktPersistentTickComponent::AdvanceFrame()
{
    if (!bIsInitialized)
    {
        return -1;
    }

    if (CurrentFrame >= ReservedMaxFrame)
    {
        UE_LOG(LogTemp, Error, TEXT("[PersistentTick] CRITICAL: Frame range exhausted (Current=%lld, Max=%lld). Waiting for next batch."),
            CurrentFrame, ReservedMaxFrame);
        return -1;  // 대기 - 다음 틱에 재시도
    }

    CurrentFrame++;

    // 80% 소진 시 미리 다음 배치 예약
    if (!bIsReservePending && (ReservedMaxFrame - CurrentFrame) < (BatchSize / 5))
    {
        ReserveNextBatch();
    }

    return CurrentFrame;
}

void UHktPersistentTickComponent::ReserveNextBatch()
{
    if (bIsReservePending)
    {
        return;
    }
    bIsReservePending = true;

    IHktPersistentTickProvider* P = Provider.Get();
    P->ReserveBatch(BatchSize, [this](int64 NewMaxFrame)
    {
        // 콜백은 이미 GameThread (파일 구현은 동기)
        ReservedMaxFrame = NewMaxFrame;

        if (!bIsInitialized)
        {
            CurrentFrame = NewMaxFrame - BatchSize;
            bIsInitialized = true;
            UE_LOG(LogTemp, Log, TEXT("[PersistentTick] Initialized: CurrentFrame=%lld, ReservedMaxFrame=%lld"),
                CurrentFrame, ReservedMaxFrame);
        }

        bIsReservePending = false;
    });

    // 파일 구현은 동기이므로 콜백이 즉시 호출됨. 비동기 구현체는 bIsReservePending이 콜백에서 해제됨.
}
