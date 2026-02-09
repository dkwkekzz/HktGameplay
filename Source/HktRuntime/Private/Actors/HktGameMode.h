#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HktRuntimeTypes.h"
#include "HktDatabaseTypes.h"
#include "HktGameMode.generated.h"

class UHktMasterStashComponent;
class UHktGridRelevancyComponent;
class UHktVMProcessorComponent;
class UHktPlayerDatabaseComponent;
class UHktPersistentFrameComponent;
class UHktIntentCollectorComponent;
class UHktBatchBuilderComponent;
class AHktInGamePlayerController;
class IHktServerRule;
class IHktStashInterface;

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
 *   RequestLogin   → Rule->OnReceived_Authentication()
 */
UCLASS()
class HKTRUNTIME_API AHktGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AHktGameMode();

    // === 외부 접근 ===

    /** Intent를 IntentCollector에 푸시 (PlayerController에서 호출) */
    void PushIntent(int64 PlayerUid, const FHktIntentEvent& Event);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    // === Rule 조회 ===
    IHktServerRule* GetServerRule() const;

protected:
    // === 인터페이스 구현 컴포넌트들 (Rule의 파라미터로 전달됨) ===

    /** IHktPersistentFrame 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktPersistentFrameComponent> PersistentFrameComponent;

    /** MasterStash (IHktSimulator의 백엔드로 사용) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktMasterStashComponent> MasterStashComponent;

    /** IHktRelevancyGraph 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktGridRelevancyComponent> GridRelevancyComponent;

    /** IHktWorldDatabase 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktPlayerDatabaseComponent> PlayerDatabaseComponent;

    /** IHktIntentCollector 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktIntentCollectorComponent> IntentCollectorComponent;

    /** IHktBatchBuilder 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktBatchBuilderComponent> BatchBuilderComponent;

    /** VM 프로세서 (서버 시뮬레이션) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktVMProcessorComponent> VMProcessorComponent;
};
