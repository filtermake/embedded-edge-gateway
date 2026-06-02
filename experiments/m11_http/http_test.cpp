#include "HttpRequest.h"
#include <cassert>
#include <iostream>
#include <cstring>
using namespace m11;

int main() {
    // 测试1:一次性喂完整请求(带 body)
    {
        HttpRequest req;
        Buffer buf;
        const char* raw =
            "POST /submit HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "hello";
        buf.append(raw, strlen(raw));

        ParseResult r = req.parse(&buf);
        assert(r == ParseResult::kComplete);
        assert(req.method() == "POST");
        assert(req.path() == "/submit");
        assert(req.version() == "HTTP/1.1");
        assert(req.getHeader("Host") == "example.com");
        assert(req.body() == "hello");
        std::cout << "Test 1 passed\n";
    }

    // 测试2:半包 —— 分两次喂,验证 kIncomplete → kComplete
    {
        HttpRequest req;
        Buffer buf;
        // 第一次只喂到一半(请求行 + 半个 header)
        const char* part1 = "GET /index.html HTTP/1.1\r\nHo";
        buf.append(part1, strlen(part1));
        ParseResult r1 = req.parse(&buf);
        assert(r1 == ParseResult::kIncomplete);   // 还没完

        // 第二次喂剩下的
        const char* part2 = "st: a.com\r\n\r\n";
        buf.append(part2, strlen(part2));
        ParseResult r2 = req.parse(&buf);
        assert(r2 == ParseResult::kComplete);      // 这次齐了
        assert(req.path() == "/index.html");
        assert(req.getHeader("Host") == "a.com");
        std::cout << "Test 2 passed\n";
    }

    // 测试3:畸形 Content-Length,验证不崩、返回 kError
    {
        HttpRequest req;
        Buffer buf;
        const char* raw =
            "POST /x HTTP/1.1\r\n"
            "Content-Length: abc\r\n"      // ← 非法数字
            "\r\n";
        buf.append(raw, strlen(raw));
        ParseResult r = req.parse(&buf);
        assert(r == ParseResult::kError);   // 不崩,报错
        std::cout << "Test 3 passed\n";
    }

    {
        HttpRequest req;
        Buffer buf;
        const char* raw =
            "GET / HTTP/1.1\r\n"
            "Host: a.com\r\n"
            "\r\n";
        buf.append(raw, strlen(raw));
        ParseResult r = req.parse(&buf);
        assert(r == ParseResult::kComplete);
        assert(req.method() == "GET");
        assert(req.body() == "");           // 没有 body
        std::cout << "Test 4 passed\n";
    }

    // 测试5:大小写混乱的 header key
    {
        HttpRequest req;
        Buffer buf;
        const char* raw =
            "POST /x HTTP/1.1\r\n"
            "CONTENT-LENGTH: 3\r\n"          // ← 全大写
            "\r\n"
            "abc";
        buf.append(raw, strlen(raw));
        ParseResult r = req.parse(&buf);
        assert(r == ParseResult::kComplete);  // 归一化生效,正确识别出 body 长度
        assert(req.body() == "abc");
        std::cout << "Test 5 passed\n";
    }

    // 测试6:pipelining —— 一个 buffer 两条请求
    {
        Buffer buf;
        const char* raw =
            "GET /first HTTP/1.1\r\n\r\n"
            "GET /second HTTP/1.1\r\n\r\n";
        buf.append(raw, strlen(raw));

        HttpRequest req1;
        ParseResult r1 = req1.parse(&buf);
        assert(r1 == ParseResult::kComplete);
        assert(req1.path() == "/first");

        // 第二条:buffer 里还剩 /second,用新的 HttpRequest 解析
        HttpRequest req2;
        ParseResult r2 = req2.parse(&buf);
        assert(r2 == ParseResult::kComplete);
        assert(req2.path() == "/second");
        std::cout << "Test 6 passed\n";
    }

    // 测试7:同一个 HttpRequest 复用解析两条(长连接)
    {
        Buffer buf;
        const char* raw =
            "GET /first HTTP/1.1\r\n\r\n"
            "GET /second HTTP/1.1\r\n\r\n";
        buf.append(raw, strlen(raw));

        HttpRequest req;
        assert(req.parse(&buf) == ParseResult::kComplete);
        assert(req.path() == "/first");

        req.reset();                          // ← 复用前重置
        assert(req.parse(&buf) == ParseResult::kComplete);
        assert(req.path() == "/second");
        std::cout << "Test 7 passed\n";
    }

    std::cout << "All tests passed\n";
    return 0;
}