// AccessControlFSM.cpp
#include "AccessControlFSM.h"

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