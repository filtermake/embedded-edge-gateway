// AccessControlFSM.h
#pragma once

#include <iostream>

// ① 列出所有状态
enum class State {
    IDLE,
    VERIFYING,
    OPEN,
    ALARM
};

// ② 列出所有事件
enum class Event {
    CARD_SWIPED,
    VERIFY_OK,
    VERIFY_FAIL,
    TIMEOUT_5S,
    TIMEOUT_3S
};

class AccessControlFSM {
public:
    AccessControlFSM() : state_(State::IDLE) {}      // 初始状态

    void onEvent(Event e);                            // 处理事件
    State current() const noexcept { return state_; } // 给外部看

private:
    State state_;                                     // 当前状态
};