#define IN_TESTS

#include <catch.hpp>
#include "../dispatch.h"
#include "../utility.h"

using impl::command_list;
#define TEST_CMD(name, ...) cmdlist << ::impl::command_builder(name) % [&](__VA_ARGS__)

std::vector<std::string> split(const std::string& s) { return split<std::string>(s, " "); }

TEST_CASE("dispatch_basic", "[dispatch]")
{
    command_list cmdlist;
    
    int x = 10;
    TEST_CMD("set x", int new_x) { x = new_x; };
    
    cmdlist.call(split("set x 15"));
    CHECK(x == 15);
}

TEST_CASE("dispatch_overload", "[dispatch][overload]")
{
    command_list cmdlist;
    
    int x = 0;
    TEST_CMD("cmd", int new_x, const std::string&) { x = new_x; };
    TEST_CMD("cmd", const std::string&, int new_x) { x = new_x + 100; };
    
    cmdlist.call(split("cmd 15 aaa"));
    CHECK(x == 15);
    cmdlist.call(split("cmd aaa 15"));
    CHECK(x == 115);
    
    CHECK_THROWS(cmdlist.call(split("cmd aaa aaa")));
}

TEST_CASE("dispatch_defaults", "[dispatch][defaults]")
{
    command_list cmdlist;
    
    int x = 0;
    TEST_CMD("cmd", int a, int b = 100, int c = 10) { x = a + b + c; };
    
    CHECK_THROWS(cmdlist.call(split("cmd")));
    
    cmdlist.call(split("cmd 1"));
    CHECK(x == 111);
    
    cmdlist.call(split("cmd 1 2"));
    CHECK(x == 13);
    
    cmdlist.call(split("cmd 1 2 3"));
    CHECK(x == 6);
    
    CHECK_THROWS(cmdlist.call(split("cmd 1 2 3 4")));
}
