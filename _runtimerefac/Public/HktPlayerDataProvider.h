// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktDatabaseTypes.h"
#include "Misc/Optional.h"

/**
 * IHktPlayerDataProvider - 플레이어 영구 데이터 저장소 인터페이스
 *
 * 플레이어별 로드/저장을 추상화합니다.
 * 구현체: 파일(개발용), Redis, SQL 등 - 교체 가능하도록 인터페이스로 분리.
 *
 * 연결 실패 시 콜백 미호출 = 서비스 불가.
 */
class HKTRUNTIME_API IHktPlayerDataProvider
{
public:
    virtual ~IHktPlayerDataProvider() = default;

    /**
     * 저장소에서 해당 플레이어 레코드를 로드합니다.
     * 콜백은 GameThread에서 호출됩니다.
     *
     * @param PlayerId 플레이어 ID
     * @param Callback 성공 시 레코드(없으면 TOptional::Empty). 실패 시 호출되지 않음.
     */
    virtual void Load(const FString& PlayerId, TFunction<void(TOptional<FHktPlayerRecord>)> Callback) = 0;

    /**
     * 해당 플레이어 레코드를 저장소에 저장합니다.
     * 콜백은 GameThread에서 호출됩니다.
     *
     * @param PlayerId 플레이어 ID
     * @param Record 저장할 레코드
     * @param Callback 완료 시 bSuccess로 호출. 실패 시 false.
     */
    virtual void Save(const FString& PlayerId, const FHktPlayerRecord& Record, TFunction<void(bool bSuccess)> Callback) = 0;
};
