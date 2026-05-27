#include "AccessControlFSM.h"
#include <iostream>

int main() {
    AccessControlFSM fsm;
    
    std::cout << "--- 正常流程:刷卡 → 验证通过 → 5秒后关门 ---\n";
    fsm.onEvent(Event::CARD_SWIPED);
    fsm.onEvent(Event::VERIFY_OK);
    fsm.onEvent(Event::TIMEOUT_5S);
    
    std::cout << "\n--- 异常流程:验证中再刷卡(应被忽略)---\n";
    fsm.onEvent(Event::CARD_SWIPED);
    fsm.onEvent(Event::CARD_SWIPED);  // 应该看到"被忽略"日志
    fsm.onEvent(Event::VERIFY_FAIL);
    fsm.onEvent(Event::TIMEOUT_3S);
    
    std::cout << "\n--- 非法流程:IDLE 时收到 VERIFY_OK ---\n";
    fsm.onEvent(Event::VERIFY_OK);  // 应该看到 ERROR 日志
    
    return 0;
}