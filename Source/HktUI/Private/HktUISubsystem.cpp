// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUISubsystem.h"
#include "HktUIElement.h"
#include "IHktUIView.h"
#include "HktUIAnchorStrategy.h"
#include "IHktPlayerInteractionInterface.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Visibility.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SWidget.h"

UHktUISubsystem* UHktUISubsystem::Get(APlayerController* PC)
{
	if (PC && PC->GetLocalPlayer())
	{
		return PC->GetLocalPlayer()->GetSubsystem<UHktUISubsystem>();
	}
	return nullptr;
}

void UHktUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	check(!bInitialized);
	bInitialized = true;
	SetTickableTickType(GetTickableTickType());

	RootElement = NewObject<UHktUIElement>(this);
	RootElement->InitializeElement(nullptr, nullptr);

	MainCanvasWidget = SNew(SConstraintCanvas);

	// 뷰포트에 캔버스 추가 (LocalPlayer의 World/Viewport 사용)
	UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
	if (World && World->GetGameViewport())
	{
		World->GetGameViewport()->AddViewportWidgetContent(MainCanvasWidget.ToSharedRef(), 0);
	}
}

void UHktUISubsystem::Deinitialize()
{
	UnbindPlayerInteraction();

	if (UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr)
	{
		if (World->GetGameViewport() && MainCanvasWidget.IsValid())
		{
			World->GetGameViewport()->RemoveViewportWidgetContent(MainCanvasWidget.ToSharedRef());
		}
	}
	MainCanvasWidget.Reset();
	RootElement = nullptr;
	EntityUIMap.Empty();

	check(bInitialized);
	bInitialized = false;
	SetTickableTickType(ETickableTickType::Never);

	Super::Deinitialize();
}

bool UHktUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) return false;

	ULocalPlayer* LP = Cast<ULocalPlayer>(Outer);
	if (!LP) return false;

	UWorld* World = LP->GetWorld();
	return !(World && World->GetNetMode() == NM_DedicatedServer);
}

void UHktUISubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	if (NewPlayerController)
	{
		BindPlayerInteraction(NewPlayerController);
	}
	else
	{
		UnbindPlayerInteraction();
	}
}

UWorld* UHktUISubsystem::GetTickableGameObjectWorld() const
{
	return GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
}

ETickableTickType UHktUISubsystem::GetTickableTickType() const
{
	return (IsTemplate() || !bInitialized) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UHktUISubsystem::IsAllowedToTick() const
{
	return bInitialized;
}

void UHktUISubsystem::Tick(float DeltaTime)
{
	checkf(bInitialized, TEXT("UHktUISubsystem::Tick called while not initialized"));
	TickAllElements(DeltaTime);
	UpdateCanvasSlots();
}

TStatId UHktUISubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHktUISubsystem, STATGROUP_Tickables);
}

void UHktUISubsystem::BindPlayerInteraction(APlayerController* PC)
{
	if (!PC) return;

	PlayerInteraction = Cast<IHktPlayerInteractionInterface>(PC);
	CachedPlayerController = PC;

	if (!PlayerInteraction)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[HktUISubsystem] PlayerController does not implement IHktPlayerInteractionInterface"));
	}
}

void UHktUISubsystem::UnbindPlayerInteraction()
{
	PlayerInteraction = nullptr;
	CachedPlayerController = nullptr;
}

UHktUIElement* UHktUISubsystem::CreateElement(TSharedPtr<IHktUIView> InView, UHktUIAnchorStrategy* InStrategy, UHktUIElement* Parent)
{
	if (!InView.IsValid() || !InStrategy) return nullptr;

	UHktUIElement* ParentElement = Parent ? Parent : RootElement.Get();
	if (!ParentElement) return nullptr;

	UHktUIElement* Element = NewObject<UHktUIElement>(this);
	Element->InitializeElement(InView, InStrategy);
	Element->SetParent(ParentElement);

	AddElementToCanvas(Element);
	return Element;
}

UHktUIElement* UHktUISubsystem::GetOrAddEntityElement(int32 EntityID)
{
	if (UHktUIElement* Existing = FindEntityElement(EntityID))
	{
		return Existing;
	}
	UHktUIElement* Element = NewObject<UHktUIElement>(this);
	Element->OwnerEntityID = EntityID;
	Element->SetParent(RootElement);
	EntityUIMap.Add(EntityID, Element);
	return Element;
}

void UHktUISubsystem::RemoveEntityElement(int32 EntityID)
{
	EntityUIMap.Remove(EntityID);
}

UHktUIElement* UHktUISubsystem::FindEntityElement(int32 EntityID) const
{
	if (const TObjectPtr<UHktUIElement>* Ptr = EntityUIMap.Find(EntityID))
	{
		return *Ptr;
	}
	return nullptr;
}

void UHktUISubsystem::AddElementToCanvas(UHktUIElement* Element)
{
	if (!MainCanvasWidget.IsValid() || !Element || !Element->View.IsValid()) return;

	TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
	MainCanvasWidget->AddSlot()
		.Offset(FMargin(Element->CachedScreenPosition.X, Element->CachedScreenPosition.Y, 0.f, 0.f))
		.Anchors(FAnchors(0.f, 0.f, 0.f, 0.f))
		.Alignment(FVector2D::ZeroVector)
		[
			SlateWidget
		];
}

void UHktUISubsystem::TickAllElements(float DeltaTime)
{
	if (!RootElement) return;

	TArray<UHktUIElement*> ToTick;
	for (UHktUIElement* Child : RootElement->GetChildren())
	{
		if (Child) ToTick.Add(Child);
	}
	for (const auto& Pair : EntityUIMap)
	{
		if (Pair.Value && !RootElement->GetChildren().Contains(Pair.Value))
		{
			ToTick.Add(Pair.Value);
		}
	}
	for (UHktUIElement* Element : ToTick)
	{
		if (Element) Element->TickElement(DeltaTime);
	}
}

void UHktUISubsystem::UpdateCanvasSlots()
{
	// SConstraintCanvas Slot 위치 갱신은 필요 시 Element별 Slot 핸들 저장 후 구현
}
