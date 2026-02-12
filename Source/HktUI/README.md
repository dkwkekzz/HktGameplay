# HktUI 모듈

## 개요

HktPresentation에서 UI를 완전 분리한 독립 모듈.
- **HktPresentation 의존성 없음**
- `IHktUserEventDispatcher` 인터페이스로 PlayerController와 통신
- 위젯 부착 대상 추상화: **Viewport / Widget / Entity**

## 위젯 부착 추상화

```
EHktUIAttachTarget
├── Viewport  → AddToViewport (인자 없음). 로그인, 인벤토리 등.
├── Widget    → 기존 관리 위젯의 자식으로 부착. ParentWidgetTag로 대상 지정.
└── Entity    → HktEntity 위에 부착. Actor/MassEntity/대상 없음 모두 지원.
               IHktUserEventDispatcher::GetEntityLocationInfo(EntityId)로 위치 조회.
               WidgetComponent(Screen Space)를 임시 Actor에 부착, 매 Tick 위치 갱신.
               엔티티 사라지면(bIsValid=false) 자동 제거.
```

Entity 부착이 Character에 의존하지 않는 이유:
- `GetEntityLocationInfo()`가 위치만 반환 → Actor든 MassEntity든 Stash 기반이든 무관
- PlayerController가 내부적으로 어떤 구현체를 쓰든 UI는 모름

## 핵심 이벤트 흐름

```
HUD 위젯 (버튼 클릭 등)
  │ FHktUIEvent { EventTag, Params, EntityId }
  ▼
UHktUISubsystem::HandleUIEvent()
  │ HktAssetSubsystem::LoadAssetAsync(EventTag)
  ▼
UHktUIActionDataAsset
  ├── ActionType: CreateWidget / DestroyWidget / DispatchEvent / CreateWidgetAndDispatch
  ├── AttachTarget: Viewport / Widget / Entity
  └── 관련 설정 (WidgetClass, ParentWidgetTag, EntityAttachOffset 등)
  │
  ▼ 액션 수행
  ├── CreateWidget → AttachToViewport / AttachToWidget / AttachToEntity
  ├── DestroyWidget → DestroyManagedWidget(TargetWidgetTag)
  └── DispatchEvent → IHktUserEventDispatcher::DispatchUserEvent(Tag, UHktEventParam*)
```

## UObject 콜백 패턴 (로그인 예시)

```
1. 로그인 버튼 클릭
2. UHktLoginEventParam 생성 { UserId, Password }
   └─ OnCompleted 콜백 바인딩
3. UISubsystem → IHktUserEventDispatcher::DispatchUserEvent(Event_Login, Param)
4. PlayerController: Tag 매칭 → 서버 패킷 전달
5. 서버 응답 → Param->Complete(bSuccess)
6. OnCompleted 브로드캐스트 → UI 업데이트 (위젯 제거 등)
```

## IHktUserEventDispatcher 구현 가이드

```cpp
// PlayerController에서 구현
class AHktPlayerController : public APlayerController, public IHktUserEventDispatcher
{
public:
    virtual void DispatchUserEvent(const FGameplayTag& Tag, UHktEventParam* Param) override;

    virtual FOnHktEntityCreated& OnEntityCreated() override;
    virtual FOnHktEntityDestroyed& OnEntityDestroyed() override;
    virtual FOnHktEntityEvent& OnEntityEvent() override;

    // Actor 기반 엔티티
    virtual FHktEntityLocationInfo GetEntityLocationInfo(FHktEntityId Id) const override
    {
        FHktEntityLocationInfo Info;
        if (AActor* Actor = FindEntityActor(Id))
        {
            Info.bIsValid = true;
            Info.WorldLocation = Actor->GetActorLocation();
            Info.AttachOffset = FVector(0, 0, 120);
        }
        return Info;
    }

    // MassEntity 기반일 때
    // virtual FHktEntityLocationInfo GetEntityLocationInfo(FHktEntityId Id) const override
    // {
    //     FHktEntityLocationInfo Info;
    //     FVector Pos;
    //     if (MassEntitySubsystem->GetEntityPosition(Id, Pos))
    //     {
    //         Info.bIsValid = true;
    //         Info.WorldLocation = Pos;
    //     }
    //     return Info;
    // }

    // Stash 직접 읽기
    virtual IHktStashInterface* GetStashInterface() const override;
};
```

## 파일 구조

```
HktUI/
├── HktUI.Build.cs
├── Public/
│   ├── IHktUIModule.h
│   ├── IHktUserEventDispatcher.h   ← HktRuntime에 배치 권장
│   ├── HktEventParam.h             ← UObject 콜백 패턴
│   ├── HktUITypes.h                ← EHktUIAttachTarget, FHktUIEvent, FHktManagedWidgetEntry
│   └── HktUISubsystem.h
├── Private/
│   ├── HktUIModule.cpp
│   ├── HktUISubsystem.cpp
│   ├── HktEventParam.cpp
│   ├── Actors/HktLoginHud.h/cpp
│   ├── Actors/HktRtsHud.h/cpp
│   ├── DataAssets/
│   │   ├── HktUIActionDataAsset.h  ← 태그→액션+부착대상 매핑
│   │   └── HktWidgetLoginHudDataAsset.h
│   ├── Settings/HktUIGlobalSetting.h
│   ├── Slates/SHktLoginHudWidget.h/cpp
│   └── Widgets/
│       ├── HktRtsHudWidget.h/cpp
│       └── HktRtsMinimapWidget.h/cpp
└── README.md
```

## HktPresentation 변경

삭제: 모든 UI 관련 파일 (Actors/HktLoginHud, HktRtsHud, Widgets/*, Slates/*, DataAssets/HktWidgetLoginHudDataAsset, Managers/HktEntityHUDManager)

수정:
- `Build.cs` → UMG, Slate, SlateCore, MediaAssets 제거
- `PresentationSubsystem` → EntityHUDManager 코드 전부 제거
- `PresentationGlobalSetting` → HUDWidgetClass 제거
- `PresentationTypes` → FHktEntityHUDData 제거 (HktUITypes로 이동)
