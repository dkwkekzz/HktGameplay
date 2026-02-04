// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPresentationSubsystem.h"
#include "HktModelProvider.h"
#include "Actors/HktRtsCameraPawn.h"
#include "Actors/HktCharacter.h"

#include "Managers/HktEntityVisualManager.h"
#include "Managers/HktSelectionVisualManager.h"
#include "Managers/HktInteractionFXManager.h"
#include "Managers/HktEntityHUDManager.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

// ============================================================================
// Construction
// ============================================================================

UHktPresentationSubsystem::UHktPresentationSubsystem()
	: ULocalPlayerSubsystem()
	, FTickableGameObject()
{
}

UHktPresentationSubsystem::~UHktPresentationSubsystem()
{
}

// ============================================================================
// USubsystem Interface
// ============================================================================

void UHktPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	check(!bInitialized);
	bInitialized = true;
	
	CreateManagers();
	
	// FTickableGameObject: 틱 등록
	SetTickableTickType(GetTickableTickType());
	
	UE_LOG(LogTemp, Log, TEXT("[HktPresentationSubsystem] Initialized"));
}

void UHktPresentationSubsystem::Deinitialize()
{
	UnbindModelProvider();
	DestroyManagers();
	
	check(bInitialized);
	bInitialized = false;
	
	// FTickableGameObject: 틱 해제
	SetTickableTickType(ETickableTickType::Never);
	
	Super::Deinitialize();
	
	UE_LOG(LogTemp, Log, TEXT("[HktPresentationSubsystem] Deinitialized"));
}

UHktPresentationSubsystem* UHktPresentationSubsystem::Get(APlayerController* PC)
{
	if (PC && PC->GetLocalPlayer())
	{
		return PC->GetLocalPlayer()->GetSubsystem<UHktPresentationSubsystem>();
	}
	return nullptr;
}

void UHktPresentationSubsystem::BeginDestroy()
{
	Super::BeginDestroy();
	
	ensureMsgf(!bInitialized, TEXT("UHktPresentationSubsystem was destroyed while still initialized! Check for missing Super call"));
}

// ============================================================================
// FTickableGameObject Interface
// ============================================================================

UWorld* UHktPresentationSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

ETickableTickType UHktPresentationSubsystem::GetTickableTickType() const
{
	// 초기화 전이거나 템플릿이면 틱하지 않음
	if (IsTemplate() || !bInitialized)
	{
		return ETickableTickType::Never;
	}
	return ETickableTickType::Conditional;
}

bool UHktPresentationSubsystem::IsAllowedToTick() const
{
	ensureMsgf(bInitialized, TEXT("Tickable subsystem %s tried to tick when not initialized! Check for missing Super call"), *GetFullName());
	return bInitialized;
}

void UHktPresentationSubsystem::Tick(float DeltaTime)
{
	checkf(bInitialized, TEXT("Ticking should have been disabled for an uninitialized subsystem!"));
	
	if (!bIsBound || !ModelProvider)
	{
		return;
	}
	
	IHktModelProvider* Provider = ModelProvider.GetInterface();
	if (!Provider)
	{
		return;
	}
	
	IHktStashInterface* Stash = Provider->GetStashInterface();
	
	// 각 Manager Tick
	if (EntityVisualManager)
	{
		EntityVisualManager->Tick(DeltaTime, Stash);
	}
	
	if (SelectionVisualManager)
	{
		SelectionVisualManager->Tick(DeltaTime);
	}
	
	if (InteractionFXManager)
	{
		InteractionFXManager->Tick(DeltaTime);
	}
	
	if (EntityHUDManager)
	{
		EntityHUDManager->Tick(DeltaTime, Stash);
	}
}

TStatId UHktPresentationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHktPresentationSubsystem, STATGROUP_Tickables);
}

// ============================================================================
// ULocalPlayerSubsystem
// ============================================================================

void UHktPresentationSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	
	if (NewPlayerController)
	{
		TScriptInterface<IHktModelProvider> Provider(NewPlayerController);
		if (Provider)
		{
			BindModelProvider(Provider);
		}
	}
	else
	{
		UnbindModelProvider();
	}
}

bool UHktPresentationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Outer);
	if (!LocalPlayer)
	{
		return false;
	}
	
	UWorld* World = LocalPlayer->GetWorld();
	if (World && World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	
	return true;
}

// ============================================================================
// Manager 생성/파괴
// ============================================================================

void UHktPresentationSubsystem::CreateManagers()
{
	UWorld* World = GetWorld();

	EntityVisualManager = new FHktEntityVisualManager(World);
	SelectionVisualManager = new FHktSelectionVisualManager(World);
	InteractionFXManager = new FHktInteractionFXManager(World);
	EntityHUDManager = new FHktEntityHUDManager(World);
}

void UHktPresentationSubsystem::DestroyManagers()
{
	if (EntityHUDManager) { delete EntityHUDManager; EntityHUDManager = nullptr; }
	if (InteractionFXManager) { delete InteractionFXManager; InteractionFXManager = nullptr; }
	if (SelectionVisualManager) { delete SelectionVisualManager; SelectionVisualManager = nullptr; }
	if (EntityVisualManager) { delete EntityVisualManager; EntityVisualManager = nullptr; }
}

// ============================================================================
// Model Provider 바인딩
// ============================================================================

void UHktPresentationSubsystem::BindModelProvider(TScriptInterface<IHktModelProvider> InProvider)
{
	if (bIsBound)
	{
		UnbindModelProvider();
	}
	
	ModelProvider = InProvider;
	
	if (!ModelProvider)
	{
		return;
	}
	
	IHktModelProvider* Provider = ModelProvider.GetInterface();
	if (!Provider)
	{
		return;
	}
	
	// 델리게이트 바인딩
	SubjectChangedHandle = Provider->OnSubjectChanged().AddUObject(
		this, &UHktPresentationSubsystem::HandleSubjectChanged);
	
	TargetChangedHandle = Provider->OnTargetChanged().AddUObject(
		this, &UHktPresentationSubsystem::HandleTargetChanged);
	
	CommandChangedHandle = Provider->OnCommandChanged().AddUObject(
		this, &UHktPresentationSubsystem::HandleCommandChanged);
	
	IntentSubmittedHandle = Provider->OnIntentSubmitted().AddUObject(
		this, &UHktPresentationSubsystem::HandleIntentSubmitted);
	
	WheelInputHandle = Provider->OnWheelInput().AddUObject(
		this, &UHktPresentationSubsystem::HandleWheelInput);
	
	bIsBound = true;
	
	// 기존 엔티티 동기화
	SyncEntitiesFromStash();
	
	UE_LOG(LogTemp, Log, TEXT("[HktPresentationSubsystem] Bound to ModelProvider"));
}

void UHktPresentationSubsystem::UnbindModelProvider()
{
	if (!bIsBound || !ModelProvider)
	{
		return;
	}
	
	IHktModelProvider* Provider = ModelProvider.GetInterface();
	if (Provider)
	{
		Provider->OnSubjectChanged().Remove(SubjectChangedHandle);
		Provider->OnTargetChanged().Remove(TargetChangedHandle);
		Provider->OnCommandChanged().Remove(CommandChangedHandle);
		Provider->OnIntentSubmitted().Remove(IntentSubmittedHandle);
		Provider->OnWheelInput().Remove(WheelInputHandle);
	}
	
	// Manager 상태 초기화
	if (SelectionVisualManager)
	{
		SelectionVisualManager->ClearAll();
	}
	
	ModelProvider = nullptr;
	bIsBound = false;
	
	UE_LOG(LogTemp, Log, TEXT("[HktPresentationSubsystem] Unbound from ModelProvider"));
}

// ============================================================================
// 엔티티 조회 (EntityVisualManager 위임)
// ============================================================================

AActor* UHktPresentationSubsystem::GetEntityActor(FHktEntityId EntityId) const
{
	if (EntityVisualManager)
	{
		return EntityVisualManager->GetActor(EntityId);
	}
	return nullptr;
}

TArray<AActor*> UHktPresentationSubsystem::GetAllEntityActors() const
{
	TArray<AActor*> Result;
	
	if (EntityVisualManager)
	{
		EntityVisualManager->ForEachCharacter([&Result](FHktEntityId EntityId, AHktCharacter* Character)
		{
			Result.Add(Character);
		});
	}
	
	return Result;
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

void UHktPresentationSubsystem::HandleSubjectChanged(FHktEntityId EntityId)
{
	if (!SelectionVisualManager || !EntityVisualManager)
	{
		return;
	}
	
	AHktCharacter* Character = EntityVisualManager->GetCharacter(EntityId);
	SelectionVisualManager->SetSelectedSubject(EntityId, Character);
	
	UE_LOG(LogTemp, Verbose, TEXT("[HktPresentationSubsystem] Subject changed: %d"), EntityId.RawValue);
}

void UHktPresentationSubsystem::HandleTargetChanged(FHktEntityId EntityId)
{
	if (!SelectionVisualManager || !EntityVisualManager)
	{
		return;
	}
	
	AHktCharacter* Character = EntityVisualManager->GetCharacter(EntityId);
	SelectionVisualManager->SetSelectedTarget(EntityId, Character);
	
	UE_LOG(LogTemp, Verbose, TEXT("[HktPresentationSubsystem] Target changed: %d"), EntityId.RawValue);
}

void UHktPresentationSubsystem::HandleCommandChanged(FGameplayTag Command)
{
	// 필요시 커맨드 미리보기 UI 등 구현
	UE_LOG(LogTemp, Verbose, TEXT("[HktPresentationSubsystem] Command changed: %s"), *Command.ToString());
}

void UHktPresentationSubsystem::HandleIntentSubmitted(const FHktIntentEvent& Event)
{
	if (InteractionFXManager)
	{
		InteractionFXManager->PlayIntentFX(Event);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[HktPresentationSubsystem] Intent submitted: %s"), *Event.EventTag.ToString());
}

void UHktPresentationSubsystem::HandleWheelInput(float Delta)
{
	OnWheelInput.Broadcast(Delta);
}

void UHktPresentationSubsystem::HandleEntityCreated(FHktEntityId EntityId)
{
	if (!ModelProvider)
	{
		return;
	}
	
	IHktModelProvider* Provider = ModelProvider.GetInterface();
	if (!Provider)
	{
		return;
	}
	
	IHktStashInterface* Stash = Provider->GetStashInterface();
	
	// EntityVisualManager에서 Character 스폰
	if (EntityVisualManager)
	{
		EntityVisualManager->OnEntityCreated(EntityId, Stash);
		
		// HUD 추가
		if (EntityHUDManager)
		{
			AHktCharacter* Character = EntityVisualManager->GetCharacter(EntityId);
			if (Character)
			{
				EntityHUDManager->AddEntityHUD(EntityId, Character);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[HktPresentationSubsystem] Entity created: %d"), EntityId.RawValue);
}

void UHktPresentationSubsystem::HandleEntityDestroyed(FHktEntityId EntityId)
{
	// 선택 상태 해제
	if (SelectionVisualManager)
	{
		if (SelectionVisualManager->GetSelectedSubject() == EntityId)
		{
			SelectionVisualManager->ClearSubjectSelection();
		}
		if (SelectionVisualManager->GetSelectedTarget() == EntityId)
		{
			SelectionVisualManager->ClearTargetSelection();
		}
	}
	
	// HUD 제거
	if (EntityHUDManager)
	{
		EntityHUDManager->RemoveEntityHUD(EntityId);
	}
	
	// Character 파괴
	if (EntityVisualManager)
	{
		EntityVisualManager->OnEntityDestroyed(EntityId);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[HktPresentationSubsystem] Entity destroyed: %d"), EntityId.RawValue);
}

// ============================================================================
// 내부 헬퍼
// ============================================================================

void UHktPresentationSubsystem::SyncEntitiesFromStash()
{
	if (!ModelProvider || !EntityVisualManager)
	{
		return;
	}
	
	IHktModelProvider* Provider = ModelProvider.GetInterface();
	if (!Provider)
	{
		return;
	}
	
	IHktStashInterface* Stash = Provider->GetStashInterface();
	if (!Stash)
	{
		return;
	}
	
	// Stash의 모든 엔티티에 대해 Actor 생성
	Stash->ForEachEntity([this, Stash](FHktEntityId EntityId)
	{
		EntityVisualManager->OnEntityCreated(EntityId, Stash);
		
		// HUD 추가
		if (EntityHUDManager)
		{
			AHktCharacter* Character = EntityVisualManager->GetCharacter(EntityId);
			if (Character)
			{
				EntityHUDManager->AddEntityHUD(EntityId, Character);
			}
		}
	});
	
	UE_LOG(LogTemp, Log, TEXT("[HktPresentationSubsystem] Synced %d entities from Stash"), 
		EntityVisualManager->GetEntityCount());
}
