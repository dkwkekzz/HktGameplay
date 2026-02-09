// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMProcessorComponent.h"
#include "HktCoreInterfaces.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#endif

UHktVMProcessorComponent::UHktVMProcessorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

UHktVMProcessorComponent::~UHktVMProcessorComponent()
{
}

void UHktVMProcessorComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // VMProcessor는 Initialize() 호출 시 생성됨
}

void UHktVMProcessorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    VMProcessor.Reset();
    
    Super::EndPlay(EndPlayReason);
}

void UHktVMProcessorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (VMProcessor)
    {
        VMProcessor->Tick(SyncFrameNumber++, DeltaTime);
    }
}

void UHktVMProcessorComponent::Initialize(IHktStashInterface* InStash)
{
    if (!InStash)
    {
        UE_LOG(LogTemp, Warning, TEXT("VMProcessorComponent: Failed to initialize - invalid Stash"));
        return;
    }
    
    // VMProcessor 생성 (Stash와 함께 초기화)
    VMProcessor = CreateVMProcessor(InStash);

    if (VMProcessor.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("VMProcessorComponent: Initialized with Stash"));
    }
}

void UHktVMProcessorComponent::NotifyIntentEvent(int32 InFrameNumber, const FHktIntentEvent& Event)
{
    if (!VMProcessor)
    {
        UE_LOG(LogTemp, Warning, TEXT("VMProcessorComponent: Cannot notify event - not initialized"));
        return;
    }

    SyncFrameNumber = InFrameNumber;

    VMProcessor->NotifyIntentEvent(Event);
}

void UHktVMProcessorComponent::NotifyIntentEvents(int32 InFrameNumber, const TArray<FHktIntentEvent>& Events)
{
    if (!VMProcessor)
    {
        UE_LOG(LogTemp, Warning, TEXT("VMProcessorComponent: Cannot notify events - not initialized"));
        return;
    }

    SyncFrameNumber = InFrameNumber;

    for (const FHktIntentEvent& Event : Events)
    {
        VMProcessor->NotifyIntentEvent(Event);
    }
}

void UHktVMProcessorComponent::NotifyCollision(FHktEntityId WatchedEntity, FHktEntityId HitEntity)
{
    if (VMProcessor)
    {
        VMProcessor->NotifyCollision(WatchedEntity, HitEntity);
    }
}
