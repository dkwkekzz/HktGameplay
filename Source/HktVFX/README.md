# HktVFX 모듈

## 개요

HktVFX는 VFX 전용 레이어 모듈입니다.
- 게임 로직 없음 (읽기 전용)
- HktRuntime의 `IHktModelProvider`를 통해 데이터 수신
- 클라이언트 전용 (서버에서 생성되지 않음)

## 파일 구조

```
HktVFX/
├── HktVFX.Build.cs
├── Public/
│   └── IHktVFXModule.h       # 모듈 인터페이스
└── Private/
    └── HktVFXModule.cpp
```
