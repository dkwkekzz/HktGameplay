// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFlowRegistry.h"

TArray<FHktFlowRegistry::FFlowRegisterFunc>& FHktFlowRegistry::GetRegistry()
{
    static TArray<FFlowRegisterFunc> Registry;
    return Registry;
}

void FHktFlowRegistry::AddFlowRegistration(FFlowRegisterFunc InitFunc)
{
    GetRegistry().Add(InitFunc);
}

void FHktFlowRegistry::InitializeAllFlows()
{
    // 등록된 모든 Flow 생성 로직 실행
    for (const auto& RegisterFunc : GetRegistry())
    {
        if (RegisterFunc)
        {
            RegisterFunc();
        }
    }

    // 메모리 절약을 위해 실행 후 비움 (필요에 따라 유지 가능)
    GetRegistry().Empty();
    
    UE_LOG(LogTemp, Log, TEXT("HktFlowRegistry: All Flows have been initialized."));
}