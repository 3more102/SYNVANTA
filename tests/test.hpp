#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
struct TestCase { std::string name; std::function<void()> function; };
inline std::vector<TestCase>& test_registry(){static std::vector<TestCase> tests;return tests;}
struct TestRegistration {
    TestRegistration(std::string n,std::function<void()> f){test_registry().push_back({std::move(n),std::move(f)});}
};
#define TEST_CASE(name) static void name(); static TestRegistration registration_##name(#name,name); static void name()
#define REQUIRE(condition) do { if(!(condition)) throw std::runtime_error(std::string("requirement failed: ")+#condition); } while(false)
