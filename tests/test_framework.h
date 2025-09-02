// tests/test_framework.h
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <iostream>
#include <vector>
#include <functional>
#include <string>

class TestRunner {
private:
    std::vector<std::pair<std::string, std::function<bool()>>> tests;
    
public:
    auto addTest(const std::string& name, std::function<bool()> test) -> void {
        tests.emplace_back(name, std::move(test));
    }
    
    [[nodiscard]] auto runAll() -> int {
        int passed = 0;
        int failed = 0;
        
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "RUNNING TESTS\n";
        std::cout << std::string(50, '=') << "\n";
        
        for (const auto& [name, test] : tests) {
            std::cout << "• " << name << "... ";
            std::cout.flush();
            
            try {
                if (test()) {
                    std::cout << "✅ PASSED\n";
                    passed++;
                } else {
                    std::cout << "❌ FAILED\n";
                    failed++;
                }
            } catch (const std::exception& e) {
                std::cout << "💥 EXCEPTION: " << e.what() << "\n";
                failed++;
            }
        }
        
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "RESULTS: " << passed << " passed, " << failed << " failed\n";
        std::cout << std::string(50, '=') << "\n";
        
        return failed;
    }
};

#endif // TEST_FRAMEWORK_H
