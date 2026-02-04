// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IHktPersistentTickProvider - 영구 프레임 번호 배치 할당 인터페이스
 *
 * Hi-Lo 알고리즘 기반으로 DB/스토리지에서 프레임 번호 범위를 예약받습니다.
 * 구현체: 파일(개발용), Redis, SQL 등 - 교체 가능하도록 인터페이스로 분리.
 *
 * 연결 실패 시 서비스를 운영할 수 없음 (콜백 미호출 = 초기화 실패).
 */
class HKTRUNTIME_API IHktPersistentTickProvider
{
public:
    virtual ~IHktPersistentTickProvider() = default;

    /**
     * DB/스토리지에서 BatchSize만큼의 프레임 번호 범위를 예약합니다.
     * 콜백은 GameThread에서 호출됩니다.
     *
     * @param BatchSize 예약할 프레임 수 (예: 36000)
     * @param Callback 성공 시 NewMaxFrame(예약된 범위의 끝)으로 호출. 실패 시 호출되지 않음.
     */
    virtual void ReserveBatch(int64 BatchSize, TFunction<void(int64 NewMaxFrame)> Callback) = 0;
};
