// AccessControlFSM.cpp
#include "AccessControlFSM.h"

// void AccessControlFSM::onEvent(Event e) {
//     if (state_ == State::IDLE) {
//         if (e == Event::CARD_SWIPED) {
//             // 行 #1: IDLE + CARD_SWIPED → VERIFYING
//             std::cout << "[ACTION] 启动后台验证请求\n";
//             state_ = State::VERIFYING;
//         } else {
//             // 默认:非法事件
//             std::cerr << "[ERROR] 非法事件:state=IDLE, event=" 
//                       << static_cast<int>(e) << "\n";
//         }
//     }
//     else if (state_ == State::VERIFYING) {
//         // ←——— 你来填这里 ———
//         if (e == Event::VERIFY_OK) {
//             std::cout << "[ACTION] 验证通过 启动5秒计时\n";
//             state_ = State::OPEN;
//         }
//         else if (e == Event::VERIFY_FAIL) {
//             std::cout << "[ACTION] 验证失败 开启蜂鸣器3秒\n";
//             state_ = State::ALARM;
//         }
//         else if (e == Event::CARD_SWIPED) {
//             std::cout << "[INFO] 验证中,重复刷卡事件被忽略\n";
//         }
//         else {
//             std::cerr << "[ERROR] 非法事件:state=VERIFYING, event=" 
//                       << static_cast<int>(e) << "\n";
//         }
//     }
//     else if (state_ == State::OPEN) {
//         // ←——— 你来填这里 ———
//         if (e == Event::TIMEOUT_5S) {
//             std::cout << "[ACTION] 开门已5秒 关门\n";
//             state_ = State::IDLE;
//         }
//         else {
//             std::cerr << "[ERROR] 非法事件:state=OPEN, event=" 
//                       << static_cast<int>(e) << "\n";
//         }
//     }
//     else if (state_ == State::ALARM) {
//         // ←——— 你来填这里 ———
//         if (e == Event::TIMEOUT_3S) {
//             std::cout << "[ACTION] 警报已三秒 关闭警报\n";
//             state_ = State::IDLE;
//         }
//         else {
//             std::cerr << "[ERROR] 非法事件:state=ALARM, event=" 
//                       << static_cast<int>(e) << "\n";
//         }
//     }
// }

void AccessControlFSM::onEvent(Event e) {
    switch (state_)
    {
    case State::IDLE:
        switch (e)
        {
        case Event::CARD_SWIPED:
            std::cout << "[ACTION] 启动后台验证请求\n";
            state_ = State::VERIFYING;
            break;
        
        default:
            std::cerr << "[ERROR] 非法事件:state=IDLE, event=" << static_cast<int>(e) << "\n";
            break;
        }
        break;
    
    case State::VERIFYING:
        switch (e)
        {
        case Event::VERIFY_OK:
            std::cout << "[ACTION] 验证通过 启动5秒计时\n";
            state_ = State::OPEN;
            break;

        case Event::VERIFY_FAIL:
            std::cout << "[ACTION] 验证失败 开启蜂鸣器3秒\n";
            state_ = State::ALARM;
            break;
        
        case Event::CARD_SWIPED:
            std::cout << "[INFO] 验证中,重复刷卡事件被忽略\n";
            break;
        
        default:
            std::cerr << "[ERROR] 非法事件:state=VERIFYING, event=" << static_cast<int>(e) << "\n";
            break;
        }

        break;

    case State::OPEN:
        switch (e)
        {
        case Event::TIMEOUT_5S:
            std::cout << "[ACTION] 开门已5秒 关门\n";
            state_ = State::IDLE;
            break;
        
        default:
            std::cerr << "[ERROR] 非法事件:state=OPEN, event=" << static_cast<int>(e) << "\n";
            break;
        }
        break;
        

    case State::ALARM:
        switch (e)
        {
        case Event::TIMEOUT_3S:
            std::cout << "[ACTION] 警报已三秒 关闭警报\n";
            state_ = State::IDLE;
            break;
        
        default:
            std::cerr << "[ERROR] 非法事件:state=ALARM, event=" << static_cast<int>(e) << "\n";
            break;
        }
        break;
    }
}