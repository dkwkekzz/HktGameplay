# HktPresentation 모듈

## 개요

HktPresentation은 순수 View 레이어 모듈입니다.
- 게임 로직 없음 (읽기 전용)
- HktRuntime의 `IHktModelProvider`를 통해 데이터 수신
- 클라이언트 전용 (서버에서 생성되지 않음)

## 파일 구조

```
HktPresentation/
├── HktPresentation.Build.cs
├── Public/
│   ├── IHktPresentationModule.h       # 모듈 인터페이스
│   ├── HktPresentationTypes.h         # 타입 정의
│   ├── HktPresentationSubsystem.h     # 메인 WorldSubsystem
│   ├── HktCameraPawn.h                # RTS 카메라
│   └── HktEntityHUDWidget.h           # 엔티티 HUD 위젯
├── Private/
│   ├── HktPresentationModule.cpp
│   ├── HktPresentationTypes.cpp
│   ├── HktPresentationSubsystem.cpp
│   ├── HktCameraPawn.cpp
│   ├── HktEntityHUDWidget.cpp
│   └── Managers/                      # 분리된 Manager 클래스들
│       ├── HktEntityVisualManager.h/cpp    # 엔티티 Spawn/Destroy
│       ├── HktSelectionVisualManager.h/cpp # 선택 데칼 표시
│       ├── HktInteractionFXManager.h/cpp   # 인터랙션 이펙트
│       └── HktEntityHUDManager.h/cpp       # 체력바/ID HUD
└── RuntimeAdditions/                  # HktRuntime에 추가할 파일들
    ├── HktModelProvider.h
    └── HktModelProvider.cpp
```

## 아키텍처

```
┌─────────────────────────────────────────────────────────────────┐
│                      HktRuntime (Model Layer)                    │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ AHktPlayerController : IHktModelProvider                     ││
│  │  ├─ UHktIntentBuilderComponent (입력 상태)                   ││
│  │  ├─ UHktVisibleStashComponent (엔티티 데이터)                ││
│  │  └─ Delegates (입력 이벤트)                                  ││
│  └─────────────────────────────────────────────────────────────┘│
└──────────────────────────────┬──────────────────────────────────┘
                               │ IHktModelProvider
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                   HktPresentation (View Layer)                   │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ UHktPresentationSubsystem (WorldSubsystem)                   ││
│  │  ├─ FHktEntityVisualManager   (Spawn/Destroy)               ││
│  │  ├─ FHktSelectionVisualManager (선택 표시)                  ││
│  │  ├─ FHktInteractionFXManager  (인터랙션 이펙트)             ││
│  │  └─ FHktEntityHUDManager      (체력바, ID 등)               ││
│  └─────────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ AHktCameraPawn                                               ││
│  │  └─ 휠 줌, 이동 등 카메라 제어                               ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

## Manager 클래스 설계

### FHktEntityVisualManager
- **책임**: Stash의 엔티티 생성/파괴에 따라 AHktCharacter Spawn/Destroy
- **주요 메서드**:
  - `OnEntityCreated()`: Character 스폰
  - `OnEntityDestroyed()`: Character 파괴
  - `Tick()`: 위치/상태 동기화
  - `GetCharacter()`: EntityId → Character 조회

### FHktSelectionVisualManager
- **책임**: Subject/Target 선택 시 데칼 표시
- **주요 메서드**:
  - `SetSelectedSubject()`: Subject 선택 데칼
  - `SetSelectedTarget()`: Target 선택 데칼
  - `ClearAll()`: 선택 해제

### FHktInteractionFXManager
- **책임**: 클릭 위치 이펙트, 타겟 범위 표시
- **주요 메서드**:
  - `PlayIntentFX()`: Intent 제출 시 이펙트
  - `ShowTargetIndicator()`: 타겟 위치 표시
  - `HideTargetIndicator()`: 타겟 인디케이터 숨기기

### FHktEntityHUDManager
- **책임**: 엔티티별 HUD 관리 (체력바, 마나바, ID)
- **주요 메서드**:
  - `AddEntityHUD()`: HUD 추가
  - `RemoveEntityHUD()`: HUD 제거
  - `Tick()`: HUD 데이터 업데이트

---

## HktRuntime 통합 가이드

### 1. HktModelProvider 인터페이스 추가

`RuntimeAdditions/` 폴더의 파일들을 HktRuntime에 복사:
- `HktModelProvider.h` → `HktRuntime/Public/`
- `HktModelProvider.cpp` → `HktRuntime/Private/`

### 2. AHktPlayerController 수정

```cpp
// HktPlayerController.h
#include "HktModelProvider.h"

UCLASS()
class HKTRUNTIME_API AHktPlayerController : public APlayerController, public IHktModelProvider
{
    GENERATED_BODY()

public:
    // === IHktModelProvider 구현 ===
    virtual IHktStashInterface* GetStashInterface() const override
    {
        return VisibleStashComponent ? VisibleStashComponent->GetStashInterface() : nullptr;
    }

    virtual FHktEntityId GetSelectedSubject() const override
    {
        return IntentBuilderComponent ? IntentBuilderComponent->GetSubjectEntityId() : InvalidEntityId;
    }

    virtual FHktEntityId GetSelectedTarget() const override
    {
        return IntentBuilderComponent ? IntentBuilderComponent->GetTargetEntityId() : InvalidEntityId;
    }

    virtual FVector GetTargetLocation() const override
    {
        return IntentBuilderComponent ? IntentBuilderComponent->GetTargetLocation() : FVector::ZeroVector;
    }

    virtual FGameplayTag GetSelectedCommand() const override
    {
        return IntentBuilderComponent ? IntentBuilderComponent->GetEventTag() : FGameplayTag();
    }

    virtual bool IsIntentValid() const override
    {
        return IntentBuilderComponent && IntentBuilderComponent->IsValid();
    }

    virtual FOnHktSubjectChanged& OnSubjectChanged() override { return SubjectChangedDelegate; }
    virtual FOnHktTargetChanged& OnTargetChanged() override { return TargetChangedDelegate; }
    virtual FOnHktCommandChanged& OnCommandChanged() override { return CommandChangedDelegate; }
    virtual FOnHktIntentSubmitted& OnIntentSubmitted() override { return IntentSubmittedDelegate; }
    virtual FOnHktWheelInput& OnWheelInput() override { return WheelInputDelegate; }
    virtual FOnHktEntityCreated& OnEntityCreated() override { return EntityCreatedDelegate; }
    virtual FOnHktEntityDestroyed& OnEntityDestroyed() override { return EntityDestroyedDelegate; }

protected:
    // 델리게이트 인스턴스
    FOnHktSubjectChanged SubjectChangedDelegate;
    FOnHktTargetChanged TargetChangedDelegate;
    FOnHktCommandChanged CommandChangedDelegate;
    FOnHktIntentSubmitted IntentSubmittedDelegate;
    FOnHktWheelInput WheelInputDelegate;
    FOnHktEntityCreated EntityCreatedDelegate;
    FOnHktEntityDestroyed EntityDestroyedDelegate;
};
```

### 3. IntentBuilderComponent Getter 추가

```cpp
// HktIntentBuilderComponent.h
UFUNCTION(BlueprintPure, Category = "Hkt|IntentBuilder")
FHktEntityId GetSubjectEntityId() const { return SubjectEntityId; }

UFUNCTION(BlueprintPure, Category = "Hkt|IntentBuilder")
FHktEntityId GetTargetEntityId() const { return TargetEntityId; }

UFUNCTION(BlueprintPure, Category = "Hkt|IntentBuilder")
FVector GetTargetLocation() const { return TargetLocation; }

UFUNCTION(BlueprintPure, Category = "Hkt|IntentBuilder")
FGameplayTag GetEventTag() const { return EventTag; }
```

### 4. 델리게이트 브로드캐스트 추가

```cpp
// AHktPlayerController 입력 핸들러에서

void AHktPlayerController::OnSubjectAction(const FInputActionValue& Value)
{
    if (IntentBuilderComponent)
    {
        IntentBuilderComponent->CreateSubjectAction();
        SubjectChangedDelegate.Broadcast(IntentBuilderComponent->GetSubjectEntityId());
    }
}

void AHktPlayerController::OnTargetAction(const FInputActionValue& Value)
{
    if (IntentBuilderComponent)
    {
        IntentBuilderComponent->CreateTargetAction();
        TargetChangedDelegate.Broadcast(IntentBuilderComponent->GetTargetEntityId());
    }
}

void AHktPlayerController::OnZoom(const FInputActionValue& Value)
{
    float Delta = Value.Get<float>();
    WheelInputDelegate.Broadcast(Delta);
}

// Client_ReceiveBatch_Implementation()에서
void AHktPlayerController::Client_ReceiveBatch_Implementation(const FHktFrameBatch& Batch)
{
    // ... 기존 코드 ...
    
    // 제거된 엔티티
    for (FHktEntityId EntityId : Batch.RemovedEntities)
    {
        VisibleStashComponent->FreeEntity(EntityId);
        EntityDestroyedDelegate.Broadcast(EntityId);
    }

    // 새 스냅샷 적용
    for (const FHktEntitySnapshot& Snapshot : Batch.Snapshots)
    {
        VisibleStashComponent->ApplyEntitySnapshot(Snapshot);
        EntityCreatedDelegate.Broadcast(Snapshot.EntityId);
    }
    
    // ... 나머지 코드 ...
}
```

### 5. Presentation 바인딩

```cpp
void AHktPlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    // ... 기존 코드 ...
    
    // Presentation 바인딩 (클라이언트에서만)
    if (!HasAuthority())
    {
        if (UHktPresentationSubsystem* Presentation = GetWorld()->GetSubsystem<UHktPresentationSubsystem>())
        {
            Presentation->BindModelProvider(this);
        }
    }
}
```

---

## 사용법

### 기본 설정

캐릭터 클래스, 선택 머티리얼, 인터랙션 FX, HUD 위젯 등은 **Project Settings → Game → Hkt Presentation Settings**에서 설정합니다.

```cpp
UHktPresentationSubsystem* Presentation = GetWorld()->GetSubsystem<UHktPresentationSubsystem>();
if (Presentation)
{
    // 카메라 설정 (런타임)
    Presentation->SetCameraPawn(CameraPawn);
}
```

### Manager 직접 접근

```cpp
// EntityVisualManager
if (FHktEntityVisualManager* Manager = Presentation->GetEntityVisualManager())
{
    AHktCharacter* Character = Manager->GetCharacter(EntityId);
    Manager->ForEachCharacter([](FHktEntityId Id, AHktCharacter* Char) {
        // ...
    });
}

// SelectionVisualManager (Subject/Target 색상은 Project Settings → Hkt Presentation Settings에서 설정)

// EntityHUDManager
if (FHktEntityHUDManager* Manager = Presentation->GetEntityHUDManager())
{
    Manager->SetShowEntityId(true);  // 디버그용
    Manager->SetShowManaBar(true);
}
```

---

## 데이터 흐름

```
1. 엔티티 생성
   Server Batch → VisibleStash.ApplySnapshot() 
                → EntityCreatedDelegate.Broadcast()
                → PresentationSubsystem.HandleEntityCreated()
                → EntityVisualManager.OnEntityCreated()
                → EntityHUDManager.AddEntityHUD()

2. 위치 동기화
   매 Tick → EntityVisualManager.Tick()
           → Stash.GetProperty(PosX/Y/Z) 
           → Character.SetActorLocation()

3. 선택 시각화
   Input → IntentBuilder.CreateSubjectAction() 
         → SubjectChangedDelegate.Broadcast()
         → PresentationSubsystem.HandleSubjectChanged()
         → SelectionVisualManager.SetSelectedSubject()

4. 카메라 줌
   Input → WheelInputDelegate.Broadcast()
         → PresentationSubsystem.HandleWheelInput()
         → CameraPawn.HandleWheelInput()

5. 엔티티 파괴
   Server Batch → VisibleStash.FreeEntity()
                → EntityDestroyedDelegate.Broadcast()
                → PresentationSubsystem.HandleEntityDestroyed()
                → SelectionVisualManager.Clear (if selected)
                → EntityHUDManager.RemoveEntityHUD()
                → EntityVisualManager.OnEntityDestroyed()
```

---

## 설계 원칙

| 원칙 | 적용 |
|------|------|
| **단일 책임** | 각 Manager가 하나의 시각화 책임만 담당 |
| **View만** | 데이터 변경 없음, 읽기 전용 |
| **HktRuntime 의존성** | HktCore 직접 참조 최소화 (인터페이스 통해) |
| **델리게이트 기반** | Provider → Presentation 단방향 통신 |
| **클라이언트 전용** | 서버 코드 없음 |
| **Manager 분리** | 테스트 용이, 확장 가능 |
