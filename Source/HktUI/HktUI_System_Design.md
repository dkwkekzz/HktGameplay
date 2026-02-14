# HktUI 시스템 설계 명세서 (Architecture Specification)

---

## 1. 개요 (Overview)

HktUI는 UI의 생명주기, 계층 구조(Hierarchy), 그리고 배치(Layout)를 추상화하여 관리하는 중앙 집중식 UI 프레임워크입니다.

이 시스템의 핵심 목표는 **"무엇을(Widget)"**과 **"어디에(Anchor/Position)"**를 분리하여, 2D 뷰포트와 3D 월드 공간의 경계 없이 UI 요소를 제어하는 것입니다.

---

## 2. 핵심 아키텍처 (Core Architecture)

### 2.1 클래스 다이어그램 요약

- **UHktUISubsystem**: 시스템의 진입점. 전체 UI 트리의 루트를 관리하고 틱(Tick)을 주관합니다.
- **UHktUIElement** (Node): UI의 논리적 단위. 트리 구조를 형성하며 생명주기를 가집니다.
- **IHktUIView** (Visual): 실제 화면에 그려지는 구현체(Slate, UMG)를 감싸는 인터페이스입니다.
- **UHktUIAnchorStrategy** (Policy): 이 요소가 화면 어디에 위치할지, 혹은 누구를 따라다닐지를 결정하는 전략 클래스입니다.

---

## 3. 상세 설계 (Detailed Design)

### 3.1 UHktUIElement (The Abstract Node)

UI의 논리적 본체입니다. UObject를 상속받아 가비지 컬렉션의 관리를 받습니다.

```cpp
// HktUIElement.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HktUIElement.generated.h"

class IHktUIView;
class UHktUIAnchorStrategy;

/**
 * UI 요소를 추상화한 클래스.
 * 계층 구조(Tree) 관리 및 생명주기 담당.
 */
UCLASS(BlueprintType, Blueprintable)
class HKTUI_API UHktUIElement : public UObject
{
    GENERATED_BODY()

public:
    // --- Lifecycle ---
    virtual void InitializeElement(TSharedPtr<IHktUIView> InView, UHktUIAnchorStrategy* InAnchorStrategy);
    virtual void TickElement(float DeltaTime);
    virtual void DestroyElement();

    // --- Hierarchy ---
    void AddChild(UHktUIElement* Child);
    void RemoveChild(UHktUIElement* Child);
    void SetParent(UHktUIElement* InParent);

    // --- Getters ---
    TSharedPtr<IHktUIView> GetView() const { return View; }
    FVector2D GetScreenPosition() const { return CachedScreenPosition; }

protected:
    // 실제 렌더링을 담당하는 Slate/UMG 래퍼
    TSharedPtr<IHktUIView> View;

    // 위치 결정 전략 (뷰포트 고정, 액터 추적 등)
    UPROPERTY()
    UHktUIAnchorStrategy* AnchorStrategy;

    // 트리 구조
    UPROPERTY()
    UHktUIElement* Parent;

    UPROPERTY()
    TArray<UHktUIElement*> Children;

    FVector2D CachedScreenPosition;
};
```

---

### 3.2 IHktUIView (The Visual Abstraction)

Slate와 UUserWidget(UMG)을 동일한 인터페이스로 다루기 위한 추상화입니다. 주로 Slate로 작업할 예정이시므로, Slate Widget을 홀딩하는 것이 핵심입니다.

```cpp
// IHktUIView.h
class HKTUI_API IHktUIView
{
public:
    virtual ~IHktUIView() = default;

    // 실제 슬레이트 위젯 반환 (필수)
    virtual TSharedRef<SWidget> GetSlateWidget() const = 0;

    // 가시성, 크기 등의 공통 제어
    virtual void SetVisibility(EVisibility InVisibility) = 0;
    virtual void SetRenderOpacity(float InOpacity) = 0;
};

// Slate 구현체 예시
class FHktSlateView : public IHktUIView
{
public:
    FHktSlateView(TSharedRef<SWidget> InWidget) : SlateWidget(InWidget) {}
    
    virtual TSharedRef<SWidget> GetSlateWidget() const override { return SlateWidget; }
    virtual void SetVisibility(EVisibility InVisibility) override { SlateWidget->SetVisibility(InVisibility); }
    // ...
private:
    TSharedRef<SWidget> SlateWidget;
};
```

---

### 3.3 UHktUIAnchorStrategy (The Positioning Logic)

가장 중요한 부분입니다. UI가 **"무엇에 붙어있는가"**를 정의합니다. Strategy 패턴을 사용하여 확장성을 확보합니다.

- **FixedStrategy**: 화면(Viewport)의 특정 좌표나 앵커(Top-Left 등)에 고정.
- **WorldActorStrategy**: 특정 AActor를 추적하며 World -> Screen Project 수행.
- **WorldLocationStrategy**: 고정된 월드 좌표(FVector)를 추적.

```cpp
// HktUIAnchorStrategy.h
UCLASS(Abstract, BlueprintType)
class HKTUI_API UHktUIAnchorStrategy : public UObject
{
    GENERATED_BODY()
public:
    // 델타타임을 받아 현재 프레임의 화면 좌표를 계산하여 반환
    // 반환값: 화면 좌표 (FVector2D), 화면 밖인지 여부(bool)
    virtual bool CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos) { return false; }
};

// 액터를 따라다니는 전략 (예: 체력바, 데미지 플로터)
UCLASS()
class HKTUI_API UHktUIActorAnchorStrategy : public UHktUIAnchorStrategy
{
    GENERATED_BODY()
public:
    void SetTargetActor(AActor* InActor, FVector InOffset) 
    { 
        TargetActor = InActor; 
        WorldOffset = InOffset; 
    }

    virtual bool CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos) override
    {
        if (!TargetActor.IsValid()) return false;

        APlayerController* PC = WorldContext->GetWorld()->GetFirstPlayerController();
        if (!PC) return false;

        FVector TargetLoc = TargetActor->GetActorLocation() + WorldOffset;
        return PC->ProjectWorldLocationToScreen(TargetLoc, OutScreenPos);
    }

private:
    TWeakObjectPtr<AActor> TargetActor;
    FVector WorldOffset;
};
```

---

### 3.4 UHktUISubsystem (The Manager)

UWorldSubsystem을 사용하여 월드와 생명주기를 같이하도록 설계합니다.

```cpp
// HktUISubsystem.h
UCLASS()
class HKTUI_API UHktUISubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // --- Subsystem Interface ---
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- Tickable Interface ---
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { return TStatId(); }

    // --- Factory ---
    // 요소를 생성하고 루트 혹은 특정 부모 아래에 등록
    UHktUIElement* CreateElement(TSharedPtr<IHktUIView> InView, UHktUIAnchorStrategy* InStrategy, UHktUIElement* Parent = nullptr);

private:
    // 전체 UI 트리의 최상위 루트 (보이지 않는 컨테이너)
    UPROPERTY()
    UHktUIElement* RootElement;

    // Slate 레이어에서 모든 HktUI 요소를 담을 Canvas/Overlay
    TSharedPtr<class SConstraintCanvas> MainCanvasWidget;
};
```

---

## 4. 구현 로직 흐름 (Workflow)

### 4.1 요소 생성 및 등록

1. 클라이언트 코드가 `HktUISubsystem::CreateElement`를 호출합니다.
2. 이때 Slate Widget 인스턴스와 ActorAnchorStrategy(타겟 액터 지정)를 전달합니다.
3. Subsystem은 UHktUIElement를 생성하고 Strategy를 주입합니다.
4. Subsystem은 MainCanvasWidget (전체 화면 슬레이트)에 해당 위젯을 자식으로 추가합니다.

### 4.2 매 프레임 업데이트 (Tick)

1. `HktUISubsystem::Tick`이 실행됩니다.
2. 등록된 모든 UHktUIElement를 순회합니다 (혹은 Dirty 상태인 것만).
3. 각 Element는 자신의 `AnchorStrategy->CalculateScreenPosition()`을 호출합니다.
4. 액터가 움직였다면, 새로운 화면 좌표가 계산됩니다.
5. 계산된 좌표를 `SConstraintCanvas::Slot`의 Offset/Position에 반영합니다.

---

## 5. 확장성 고려 (Scalability)

- **Slate vs UMG**: IHktUIView만 상속받으면 되므로, 나중에 UMG로 만든 위젯(UUserWidget)을 `TakeWidget()` 하여 IHktUIView로 감싸서 전달하면 시스템 변경 없이 UMG도 3D 월드 좌표에 띄울 수 있습니다.

- **성능 최적화**: 수백 개의 데미지 텍스트가 뜰 경우, SConstraintCanvas의 레이아웃 갱신 비용이 큽니다. 이를 위해 HktUISubsystem 내에서 화면 밖(Off-screen)에 있는 요소는 틱을 끄거나 렌더링에서 제외하는 컬링(Culling) 로직을 추가할 수 있습니다.

- **계층 구조 활용**: UHktUIElement가 Tree 구조이므로, '캐릭터 머리 위(부모)' -> '상태 아이콘 리스트(자식)' -> '버프 아이콘(손자)' 형태의 상대 좌표 계산도 가능합니다. (부모의 ScreenPosition + 자식의 LocalOffset).
