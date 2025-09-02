#include <iostream>
#include <vector>
#include <functional>

// Simple testing framework
class TestRunner {
private:
    std::vector<std::pair<std::string, std::function<bool()>>> tests;
    
public:
    void addTest(const std::string& name, std::function<bool()> test) {
        tests.push_back({name, test});
    }
    
    int runAll() {
        int passed = 0;
        int failed = 0;
        
        for (const auto& [name, test] : tests) {
            std::cout << "Running " << name << "... ";
            try {
                if (test()) {
                    std::cout << "PASSED" << std::endl;
                    passed++;
                } else {
                    std::cout << "FAILED" << std::endl;
                    failed++;
                }
            } catch (const std::exception& e) {
                std::cout << "EXCEPTION: " << e.what() << std::endl;
                failed++;
            }
        }
        
        std::cout << "\nResults: " << passed << " passed, " << failed << " failed" << std::endl;
        return failed;
    }
};

// Test registration
extern void registerLexerTests(TestRunner& runner);

int main() {
    TestRunner runner;
    
    // Register all test suites
    registerLexerTests(runner);
    
    return runner.runAll();
}
