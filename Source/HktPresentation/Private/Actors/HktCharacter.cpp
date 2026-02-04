// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCharacter.h"
#include "Components/CapsuleComponent.h"

//=============================================================================
// AHktCharacter
//=============================================================================

AHktCharacter::AHktCharacter()
	: EntityId(InvalidEntityId)
	, bSelectable(true)
{
	// 기본 충돌 설정 - 커서 트레이스에서 감지되도록
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

//-----------------------------------------------------------------------------
// IHktSelectable Interface
//-----------------------------------------------------------------------------

FHktEntityId AHktCharacter::GetEntityId() const
{
	return EntityId;
}

bool AHktCharacter::IsSelectable() const
{
	return bSelectable && EntityId != InvalidEntityId;
}

//-----------------------------------------------------------------------------
// Entity Management
//-----------------------------------------------------------------------------

void AHktCharacter::SetEntityId(FHktEntityId InEntityId)
{
	EntityId = InEntityId;
	
	UE_LOG(LogTemp, Log, TEXT("[HktCharacter] %s: EntityId set to %d"), 
		*GetName(), EntityId.RawValue);
}

void AHktCharacter::SetSelectable(bool bInSelectable)
{
	bSelectable = bInSelectable;
}
