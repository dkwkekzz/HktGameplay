#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HktRuntimeTypes.h"
#include "HktDatabaseTypes.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightProvider.h"
#endif

#include "HktGameMode.generated.h"

class UHktGridRelevancyComponent;
class UHktFileDatabaseComponent;
class UHktFilePersistentFrameComponent;
class UHktIntentCollectorComponent;
class UHktBatchBuilderComponent;
class UHktWorldPlayerComponent;
class AHktInGamePlayerController;
class IHktServerRule;

/**
 * AHktGameMode - 서버 오케스트레이터
 *
 * 아키텍처 원칙:
 *   - Actor는 "이벤트 발행"에 집중 (인터페이스를 직접 구현하지 않음)
 *   - 로직 흐름은 ServerRule이 인터페이스를 통해 결정
 *   - Component가 인터페이스 구현을 담당
 *
 * 이벤트 → Rule 매핑:
 *   PostLogin()    → Rule->OnLogin_EnterWorldPlayer()
 *   Logout()       → Rule->OnLogout_ExitWorldPlayer()
 *   Tick()         → Rule->OnTick_ProcessPendingConnections()
 *                  → Rule->OnTick_ExecuteFrame()
 *                  → Rule->OnTick_SendFrameBatch()
 *   ReceiveIntent  → Rule->OnReceived_FireIntentEvent()
 */
UCLASS()
class HKTRUNTIME_API AHktGameMode : public AGameModeBase
#if WITH_HKT_INSIGHTS
    , public IHktInsightProvider
#endif
{
    GENERATED_BODY()

public:
    AHktGameMode();

    /** Intent를 IntentCollector에 푸시 (PlayerController에서 호출) */
    void PushIntent(int64 PlayerUid, const FHktRuntimeEvent& Event);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    IHktServerRule* GetServerRule() const;

#if WITH_HKT_INSIGHTS
public:
    virtual void CollectInsightData(FHktInsightSnapshot& OutSnapshot) const override;
    virtual FString GetInsightProviderName() const override { return TEXT("GameMode"); }
#endif

protected:
    /** IHktPersistentFrame 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktFilePersistentFrameComponent> PersistentFrameComponent;

    /** IHktRelevancyGraph 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktGridRelevancyComponent> GridRelevancyComponent;

    /** IHktWorldDatabase 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktFileDatabaseComponent> PlayerDatabaseComponent;

    /** IHktIntentCollector 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktIntentCollectorComponent> IntentCollectorComponent;

    /** IHktBatchBuilder 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktBatchBuilderComponent> BatchBuilderComponent;

private:
    /** Insight 통계: 틱 당 처리 시간 추적 */
    float LastTickDurationMs = 0.0f;
};
