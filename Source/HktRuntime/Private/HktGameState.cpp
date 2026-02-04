#include "HktGameState.h"
#include "Net/UnrealNetwork.h"

AHktGameState::AHktGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	ServerFrame = 0;
	ClientEstimatedFrame = 0;
	FrameAccumulator = 0.0f;
}

void AHktGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHktGameState, ServerFrame);
}

void AHktGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetLocalRole() == ROLE_Authority)
	{
		// Server: Frame count is driven by GameMode (or manually set)
		// We just ensure ClientEstimatedFrame tracks it for server-side queries
		ClientEstimatedFrame = ServerFrame;
	}
	else
	{
		// Client: Predict frame advancement
		FrameAccumulator += DeltaSeconds;
		while (FrameAccumulator >= FixedFrameTime)
		{
			FrameAccumulator -= FixedFrameTime;
			ClientEstimatedFrame++;
		}
	}
}

void AHktGameState::OnRep_ServerFrame()
{
	// Simple reconciliation: Snap to server frame if drift is too large
	int32 Diff = ClientEstimatedFrame - ServerFrame;
	if (FMath::Abs(Diff) > 5) 
	{
		ClientEstimatedFrame = ServerFrame;
	}
}

int32 AHktGameState::GetCurrentFrame() const
{
	return ClientEstimatedFrame;
}
