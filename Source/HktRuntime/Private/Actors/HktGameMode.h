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
class UHktPersistentTickComponent;
class AHktPlayerController;
class IHktStashInterface;

/**
 * AHktGameMode
 * 
 * 서버 GameMode
 * - PlayerDatabase로 플레이어 데이터 관리
 * - PostLogin 시 엔티티 로드 및 Spawn 이벤트 발행
 * - 클라이언트 단위 병렬 처리
 */
UCLASS()
class HKTRUNTIME_API AHktGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AHktGameMode();

    UFUNCTION(BlueprintPure, Category = "Hkt")
    int32 GetFrameNumber() const;
    IHktStashInterface* GetStashInterface() const;

    void PushIntent(const FHktIntentEvent& Event);
    
    UFUNCTION(BlueprintCallable, Category = "Hkt")
    int32 GenerateEventId();

    /** 플레이어의 엔티티들을 MasterStash에 로드하고 Spawn 이벤트 발행 */
    void LoadPlayerEntities(AHktPlayerController* PC, FHktPlayerRecord& Record);
    
    /** 로그아웃 시 엔티티 상태를 DB에 저장 */
    void SavePlayerEntities(AHktPlayerController* PC);
    
    UFUNCTION(BlueprintNativeEvent, Category = "Hkt")
    FVector GetSpawnLocationForPlayer(AHktPlayerController* PC);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    void ProcessFrame();
    void ProcessFrameEventCell();
    void ProcessFrameClientBatch(AHktPlayerController*& PC, FHktFrameBatch& Batch);

    virtual FString GetPlayerId(APlayerController* PC) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt")
    UHktMasterStashComponent* MasterStash;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt")
    UHktGridRelevancyComponent* GridRelevancy;

    /** VM 프로세서 컴포넌트 (서버 시뮬레이션 실행) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt")
    UHktVMProcessorComponent* VMProcessor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt")
    UHktPlayerDatabaseComponent* PlayerDatabase;

    /** 영구 프레임 번호 (DB/파일 배치 할당) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt")
    UHktPersistentTickComponent* PersistentTick;

private:
    int32 NextEventId = 1;

    // Intent 수집 (락 보호)
    TArray<FHktIntentEvent> CollectedIntents;
    FCriticalSection IntentLock;

    // 프레임 처리용 (매 프레임 재사용)
    TArray<FHktIntentEvent> FrameIntents;
    
    // 이벤트별 셀 인덱스 캐시 (병렬 접근용)
    struct FEventCellInfo
    {
        FIntPoint Cell;
        bool bIsGlobal;
        bool bHasValidLocation;
    };
    TArray<FEventCellInfo> EventCellCache;
};
