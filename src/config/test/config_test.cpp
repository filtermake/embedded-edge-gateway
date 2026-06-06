#include "Config.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
using namespace gateway;

// 写一个临时配置文件
static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream o(path);
    o << content;
}

int main() {
    const std::string path = "/tmp/test_gateway.conf";

    // ---- Test 1: 正常配置,init 成功,值正确 ----
    {
        writeFile(path,
            "# comment line\n"
            "log_level = 2\n"
            "serial_path = /tmp/ttyV9\n"
            "serial_baud = 9600\n"
            "mqtt_host = 192.168.1.5\n"
            "mqtt_port = 1884\n"
            "worker_count = 8\n");
        ConfigManager::init(path);
        auto c = ConfigManager::current();
        assert(c->log_level == 2);
        assert(c->serial_path == "/tmp/ttyV9");
        assert(c->serial_baud == 9600);
        assert(c->mqtt_host == "192.168.1.5");
        assert(c->mqtt_port == 1884);
        assert(c->worker_count == 8);
        std::cout << "Test 1 (parse + init) passed\n";
    }

    // ---- Test 2: reload 改 A/B 档,diff 正确 ----
    {
        // 上面 init 时 serial_path=/tmp/ttyV9。现在改它 + mqtt_host
        writeFile(path,
            "log_level = 0\n"
            "serial_path = /tmp/ttyVX\n"     // 变了 → serial_changed
            "serial_baud = 9600\n"            // 没变
            "mqtt_host = 192.168.1.5\n"       // 没变 → mqtt 不该 changed
            "mqtt_port = 1884\n"
            "worker_count = 8\n");
        auto r = ConfigManager::reload();
        assert(r.ok);
        assert(r.serial_changed);             // path 变了
        assert(!r.mqtt_changed);              // host/keepalive 都没变
        assert(!r.db_changed);
        assert(ConfigManager::current()->log_level == 0);          // A 档生效
        assert(ConfigManager::current()->serial_path == "/tmp/ttyVX");
        std::cout << "Test 2 (reload diff) passed\n";
    }

    // ---- Test 3: C 档改了被忽略(还原启动值)----
    {
        // 启动时 mqtt_port=1884。现在文件里改成 9999,应被忽略
        writeFile(path,
            "log_level = 0\n"
            "serial_path = /tmp/ttyVX\n"
            "serial_baud = 9600\n"
            "mqtt_host = 192.168.1.5\n"
            "mqtt_port = 9999\n"              // C 档,该被忽略
            "worker_count = 8\n");
        auto r = ConfigManager::reload();
        assert(r.ok);
        assert(ConfigManager::current()->mqtt_port == 1884);  // 还是启动值,没变成 9999
        std::cout << "Test 3 (C-tier ignored) passed\n";
    }

    // ---- Test 4: 非法值 → reload 失败,旧配置不动 ----
    {
        int old_baud = ConfigManager::current()->serial_baud;
        writeFile(path,
            "serial_baud = abc\n");          // 转换失败 → 抛异常 → reload 整体回滚
        auto r = ConfigManager::reload();
        assert(!r.ok);                       // 失败
        assert(ConfigManager::current()->serial_baud == old_baud);  // 旧值原封不动
        std::cout << "Test 4 (invalid value rollback) passed\n";
    }

    // ---- Test 5: validate 拦截非法端口 ----
    {
        writeFile(path,
            "serial_path = /tmp/ttyVX\n"
            "serial_baud = 9600\n"
            "mqtt_host = x\n"
            "http_port = 70000\n");          // 超范围,validate 该拦
        auto before = ConfigManager::current()->serial_path;
        auto r = ConfigManager::reload();
        assert(!r.ok);                       // validate 失败 → reload 失败
        assert(ConfigManager::current()->serial_path == before);  // 旧配置不动
        std::cout << "Test 5 (validate reject) passed\n";
    }

    std::remove(path.c_str());
    std::cout << "All config tests passed\n";
    return 0;
}