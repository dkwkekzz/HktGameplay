# HktUI 시스템 설계 명세서 (Architecture Specification)

---

## 1. 개요 (Overview)

HktUI는 UI의 생명주기, 계층 구조(Hierarchy), 그리고 배치(Layout)를 추상화하여 관리하는 중앙 집중식 UI 프레임워크입니다.

이 시스템의 핵심 목표는 **"무엇을(Widget)"**과 **"어디에(Anchor/Position)"**를 분리하여, 2D 뷰포트와 3D 월드 공간의 경계 없이 UI 요소를 제어하는 것입니다.

모든 UI 요소의 생성은 DataAsset을 기반으로 비동기 로딩되며, 데이터 주도적(Data-Driven)으로 관리됩니다.

---

## 2. 핵심 아키텍처 (Core Architecture)

### 2.1 클래스 다이어그램 요약

- **UHktUISubsystem**: 시스템의 백엔드 (ULocalPlayerSubsystem). 전체 UI 트리의 기술적 루트를 관리하고 렌더링 틱(Tick)을 주관합니다. 현재 PlayerController의 IHktPlayerInteractionInterface에 접근할 수 있습니다.
- **AHktHUD** (Root Manager): 뷰포트 UI의 논리적 관리자. 최초 레이아웃을 결정하고 UI 이벤트를 처리합니다.
- **FHktWorldView** (Data Bridge): HktRuntime 모듈의 인터페이스. 변경된 엔티티 속성을 순회하며 UI 시스템에 전파합니다.
- **UHktTagDataAsset** (Configuration): UI 구성에 필요한 리소스(텍스처, 폰트, 미디어 등)를 정의하는 데이터 에셋입니다. GameplayTag로 관리됩니다.
- **UHktUIElement** (Node): UI의 논리적 단위. 트리 구조를 형성하며 생명주기를 가집니다.
- **IHktUIView** (Visual): 실제 화면에 그려지는 구현체(Slate, UMG)를 감싸는 인터페이스입니다.
- **UHktUIAnchorStrategy** (Policy): 이 요소가 화면 어디에 위치할지, 혹은 누구를 따라다닐지를 결정하는 전략 클래스입니다.
- **IHktPlayerInteractionInterface** (New): UI가 PlayerController에게 이벤트를 전달하기 위한 통신 인터페이스입니다.

---

## 3. 상세 설계 (Detailed Design)

### 3.1 UHktTagDataAsset & Creation Pattern

모든 UI는 하드코딩된 생성이 아닌, DataAsset을 통한 설정을 따릅니다. 이를 통해 아티스트/기획자가 UI 구성을 에디터에서 제어할 수 있습니다.
또한, Factory Method 패턴을 적용하여 각 DataAsset 서브클래스가 자신의 View 생성 로직을 책임집니다.

#### 3.1.1 데이터 에셋 정의

```cpp
// UI 구성을 위한 기본 데이터 에셋 (Factory Method 포함)
UCLASS(Abstract, BlueprintType)
class HKTUI_API UHktTagDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category="Tags")
    FGameplayTag WidgetTag;

    // 기본 배치 전략 클래스 (에디터에서 설정 가능)
    UPROPERTY(EditDefaultsOnly, Category="Config")
    TSubclassOf<UHktUIAnchorStrategy> DefaultAnchorStrategyClass;

    // Factory Method: 구체적인 View 생성은 자식 클래스에서 구현
    virtual TSharedPtr<IHktUIView> CreateView() const PURE_VIRTUAL(UHktTagDataAsset::CreateView, return nullptr;);
    
    // Helper: 전략 객체 생성
    virtual UHktUIAnchorStrategy* CreateStrategy(UObject* Outer) const;
};

// 구체적인 로그인 HUD 데이터 예시
UCLASS(BlueprintType)
class HKTUI_API UHktWidgetLoginHudDataAsset : public UHktTagDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Login")
    TObjectPtr<UTexture2D> LoginBackgroundTexture;

    // ... 기타 속성 ...

    // 구체적인 Slate 위젯 생성 로직을 여기에 캡슐화
    virtual TSharedPtr<IHktUIView> CreateView() const override
    {
         // 필요한 리소스 가공
         TOptional<FSlateBrush> BgBrush;
         if (LoginBackgroundTexture) { /* Brush 설정 */ }

         // Slate 위젯 생성
         TSharedRef<SHktLoginHudWidget> Widget = SNew(SHktLoginHudWidget)
             .LoginWidgetDataAsset(this) // 자신(DataAsset)을 주입
             .BackgroundBrush(BgBrush);

         return MakeShared<FHktSlateView>(Widget);
    }
};
```

#### 3.1.2 Slate 위젯 구현 패턴

Slate는 UObject가 아니므로, DataAsset의 수명을 보장하기 위해 TStrongObjectPtr를 사용해야 합니다.

```cpp
class HKTUI_API SHktLoginHudWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktLoginHudWidget) {}
        SLATE_EVENT(FOnHktLoginRequested, OnLoginRequested)
        SLATE_ARGUMENT(TOptional<FSlateBrush>, BackgroundBrush)
        SLATE_ARGUMENT(const UHktWidgetLoginHudDataAsset*, LoginWidgetDataAsset) // const 데이터 주입
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    // UObject 리소스를 GC로부터 보호하기 위한 Strong Pointer
    TStrongObjectPtr<const UHktWidgetLoginHudDataAsset> DataAsset;
    
    // ... 기타 Slate 멤버 변수
};
```

---

### 3.2 AHktHUD (The Root Manager)

뷰포트 UI의 진입점입니다. AssetSubsystem을 통해 필요한 UI 데이터를 비동기 로드하고, 로드가 완료되면 위젯을 생성합니다.
HUD는 구체적인 위젯 타입(LoginHud 등)을 알 필요가 없으며, 추상화된 생성 인터페이스만 호출합니다.

**UHktUISubsystem 접근**: `GetOwningPlayerController()`를 통해 `UHktUISubsystem::Get(PC)`로 서브시스템을 획득합니다.

```cpp
// AHktHUD.h
UCLASS()
class HKTUI_API AHktHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

protected:
    // 내부적으로 AssetSubsystem 호출 -> DataAsset 로드 -> CreateView
    void LoadAndCreateWidget(FGameplayTag WidgetTag, TFunction<void(UHktUIElement*)> OnCreated = nullptr);

    // HktRuntime의 WorldView 참조 (DI 또는 Getter로 획득)
    TSharedPtr<class FHktWorldView> WorldView;
    
    // UI 업데이트 라우팅
    void UpdateEntityUI();
};
```

---

### 3.3 FHktWorldView (The Entity Observer Integration)

HktRuntime 모듈에서 제공하는 FHktWorldView를 사용하여 엔티티의 변경 사항을 UI에 반영합니다.
이 인터페이스는 Dirty Checking 기반으로 동작하므로, 매 프레임 변경된 속성에 대해서만 콜백이 호출됩니다.

```cpp
// FHktWorldView.h (Reference)
class HKTUI_API FHktWorldView
{
public:
    using FEntityCallback = TFunctionRef<void(int32 EntityID, const void* EntityData)>;

    // 이번 프레임에 특정 PropertyId가 변경된 엔티티만 순회
    void ForEachEntity(int32 PropertyId, FEntityCallback Callback);

    // 이번 프레임에 어떤 속성이든 변경이 있었던 모든 엔티티 순회
    void ForEachEntity(FEntityCallback Callback);
};
```

---

### 3.4 UHktUIElement (The Abstract Node)

UI의 논리적 본체입니다. UObject를 상속받아 가비지 컬렉션의 관리를 받습니다.

```cpp
// HktUIElement.h
#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HktUIElement.generated.h"

class IHktUIView;
class UHktUIAnchorStrategy;

UCLASS(BlueprintType)
class HKTUI_API UHktUIElement : public UObject
{
    GENERATED_BODY()
public:
    virtual void InitializeElement(TSharedPtr<IHktUIView> InView, UHktUIAnchorStrategy* InAnchorStrategy);
    virtual void TickElement(float DeltaTime);
    
    // 렌더링 래퍼 및 전략
    TSharedPtr<IHktUIView> View;
    
    UPROPERTY()
    UHktUIAnchorStrategy* AnchorStrategy;
    
    FVector2D CachedScreenPosition;
    
    // 이 UI가 추적 중인 Entity ID (Optional)
    int32 OwnerEntityID = -1;
};
```

---

### 3.5 IHktUIView & UHktUIAnchorStrategy

- **IHktUIView**: Slate 위젯을 홀딩하는 인터페이스. SHktLoginHudWidget 등의 인스턴스를 TSharedRef로 관리합니다.
- **UHktUIAnchorStrategy**: 위치 결정 로직. UHktUIActorAnchorStrategy 등.

---

### 3.6 UHktUISubsystem (The Manager)

시스템의 백엔드입니다. **ULocalPlayerSubsystem**을 상속받아 플레이어별로 인스턴스가 생성되며, 현재 PlayerController의 **IHktPlayerInteractionInterface**에 접근할 수 있습니다. 생성된 Element를 등록하고 Tick을 돌리며, Entity ID와 UI Element 간의 매핑을 관리합니다.

```cpp
// HktUISubsystem.h
UCLASS()
class HKTUI_API UHktUISubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    // 정적 접근: PlayerController로부터 서브시스템 획득
    static UHktUISubsystem* Get(APlayerController* PC);

    // ULocalPlayerSubsystem: PlayerController 변경 시 호출
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

    // IHktPlayerInteractionInterface 접근 (현재 PC가 인터페이스를 구현한 경우)
    IHktPlayerInteractionInterface* GetPlayerInteraction() const { return PlayerInteraction; }

    // Factory: 뷰와 전략을 받아 Element 생성
    UHktUIElement* CreateElement(TSharedPtr<IHktUIView> InView, UHktUIAnchorStrategy* InStrategy, UHktUIElement* Parent = nullptr);
    
    // Entity 관리
    UHktUIElement* GetOrAddEntityElement(int32 EntityID);
    void RemoveEntityElement(int32 EntityID);
    UHktUIElement* FindEntityElement(int32 EntityID) const;

    virtual void Tick(float DeltaTime) override;
    
private:
    void BindPlayerInteraction(APlayerController* PC);
    void UnbindPlayerInteraction();

    TWeakObjectPtr<APlayerController> CachedPlayerController;
    IHktPlayerInteractionInterface* PlayerInteraction = nullptr;

    UPROPERTY()
    UHktUIElement* RootElement;
    
    TSharedPtr<class SConstraintCanvas> MainCanvasWidget;

    // Entity ID -> UI Element 매핑 (빠른 검색용)
    UPROPERTY()
    TMap<int32, UHktUIElement*> EntityUIMap;
};
```

**주요 특징**:
- **ULocalPlayerSubsystem**: 플레이어별 인스턴스로, 멀티플레이어 환경에서 각 플레이어의 UI를 독립적으로 관리합니다.
- **IHktPlayerInteractionInterface 접근**: `PlayerControllerChanged`에서 현재 PC를 `Cast<IHktPlayerInteractionInterface>`하여 바인딩하고, `GetPlayerInteraction()`으로 접근합니다.
- **World 접근**: `GetLocalPlayer()->GetWorld()`를 통해 World를 획득합니다.

---

## 4. 구현 로직 흐름 (Workflow)

### 4.1 데이터 기반 UI 생성 파이프라인

1. **요청 (Request)**: AHktHUD 혹은 FHktWorldView가 특정 상황에서 HktGameplayTags를 사용해 UI 생성을 요청합니다.
2. **비동기 로드 (Async Load)**: AssetSubsystem이 태그에 매핑된 UHktTagDataAsset을 로드합니다.
3. **팩토리 생성 (Factory Create)**: 로드된 DataAsset의 `CreateView()` 가상 함수를 호출합니다.
4. **요소 등록 (Register)**: `HktUISubsystem::CreateElement`를 호출하여 UI 트리에 등록합니다.

### 4.2 FHktWorldView를 통한 엔티티 UI 자동 생성 및 업데이트

엔티티가 생성될 때, 해당 엔티티가 어떤 UI를 사용할지 결정하는 UITag 속성을 가집니다. 이를 감지하여 UI를 자동으로 생성합니다.

- **UI Tag 변경 감지**: PropertyID_UITag가 변경된 엔티티를 순회하여 UI를 생성/제거합니다.
- **데이터 업데이트**: PropertyID_Health 등의 값 변경을 순회하며 View에 전달합니다.

### 4.3 UI 이벤트 전파 (Event Propagation Workflow)

사용자가 UI(예: 로그인 버튼)를 조작했을 때의 흐름입니다.

1. **UI 입력 (Input)**: SHktLoginHudWidget에서 로그인 버튼 클릭.
2. **인터페이스 호출 (Interface Call)**:
   - 위젯은 `GetOwningPlayer()`를 통해 APlayerController를 가져옵니다.
   - 방법 A: 직접 `Cast<IHktPlayerInteractionInterface>(PC)` 후 `HandleUICommand` / `SendRuntimeEvent` 호출.
   - 방법 B: `UHktUISubsystem::Get(PC)->GetPlayerInteraction()`으로 인터페이스 획득 후 호출.
3. **컨트롤러 라우팅 (Routing)**:
   - APlayerController는 인터페이스 구현부에서 Tag를 확인합니다.
   - 로그인 관련 태그라면 `UHktLoginComponent::Server_RequestLogin`을 호출합니다.
   - 시뮬레이션 태그라면 HktRuntime 모듈로 이벤트를 전달합니다.

---

## 5. 이벤트 처리 시스템 (Event Handling System)

UI에서 발생한 입력을 서버 로직이나 시뮬레이션 시스템으로 전달하기 위한 구조입니다.

### 5.1 Player Interaction Interface

UI는 PlayerController의 구체적인 타입을 알지 못해도 이벤트를 보낼 수 있어야 합니다. 이를 위해 인터페이스를 사용합니다.

```cpp
// IHktPlayerInteractionInterface.h
UINTERFACE(MinimalAPI)
class UHktPlayerInteractionInterface : public UInterface
{
    GENERATED_BODY()
};

class HKTUI_API IHktPlayerInteractionInterface
{
    GENERATED_BODY()
public:
    // 1. 일반적인 게임플레이 관련 명령 전달 (Component로 라우팅)
    virtual void HandleUICommand(FGameplayTag CommandTag, const FString& Payload) = 0;

    // 2. 시뮬레이션 시스템으로 이벤트 전달
    virtual void SendRuntimeEvent(const struct FHktRuntimeEvent& Event) = 0;
};
```

---

### 5.2 Login Component (Actor Component)

로그인과 같은 기능 단위는 ActorComponent로 분리하여 PlayerController에 부착합니다.

```cpp
// UHktLoginComponent.h
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HKTUI_API UHktLoginComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // [서버] 로그인 요청 수신
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestLogin(const FString& ID, const FString& PW);

    // [클라이언트] 서버가 보낸 로그인 결과 수신
    UFUNCTION(Client, Reliable)
    void Client_ReceiveLoginResult(bool bSuccess, const FString& Token, const FString& InUserID);

    // 로그인 성공 시 처리 (GameInstance 저장 및 레벨 이동)
    UFUNCTION(BlueprintCallable, Category = "Hkt|Login")
    void OnLoginSuccess(const FString& Token, const FString& InUserID);
};
```
