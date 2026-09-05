# Ambient Encounter Director

Region · Traversal · History를 바탕으로 Encounter를 선택·배치하고,
실행부터 정리까지 관리하는 **Unreal Engine 5.7 C++ 개인 프로젝트**입니다.

[플레이 영상 · 기술 설명](https://www.youtube.com/watch?v=4ZeVlZY_rtM) · [C++ 소스](Source/Ambient_UE5)

## 핵심 구현

- Region · 도보/탑승 상태 · History 기반 후보 필터링과 최고 점수 선택
- Authored Point / EQS 배치, 페이싱, 공통 Runtime 생명주기 관리
- 외부 말 시스템 연동, 야생동물 도주, Director 상태 저장·복원

## 담당 및 공개 범위

Director와 Encounter 로직, 외부 말 시스템 연동 코드를 C++로 직접 구현했습니다.
말 이동·탑승은 Horse Starter Kit을,
캐릭터·야생동물·환경 표현은 외부 에셋을 사용했습니다.

일부 외부 리소스는 저장소에 포함하지 않으므로,
Clone만으로 영상과 동일한 실행 환경을 재현할 수 없습니다.
