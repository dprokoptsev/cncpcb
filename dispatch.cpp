#include "dispatch.h"
#include "utility.h"

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#include <readline/readline.h>
#include <readline/history.h>

namespace impl {

bool readline(const std::string& prompt, std::string& dest)
{
    char* s = ::readline(prompt.c_str());
    if (s) {
        if (*s)
            add_history(s);
        dest = std::string(s);
        free(s);
        return true;
    } else {
        return false;
    }
}

command_list& command_list::instance()
{
    static command_list obj;
    return obj;
}

command_list::command_list(): on_failure_([]{}) {}

void command_list::call(const std::vector<std::string>& args)
{
    std::vector<command::errmsg> failures;
    
    for (const auto& cmd: cmds_) {
        if (cmd->matches(args)) {
            command::errmsg err = cmd->call(args);
            if (err.ok())
                return;
            failures.push_back(std::move(err));
        }
    }
    if (failures.empty())
        throw std::runtime_error("unknown command");
    else
        failures.front().raise_as<std::runtime_error>();
}

void command_list::run()
{
    std::string line;
    while (readline("> ", line)) {
        while (!line.empty() && isspace(line.back()))
            line.pop_back();
        auto args = split<std::string>(line, " ");
        try {
            call(args);
        }
        catch (std::exception& e) {
            on_failure_();
            std::cerr << "\033[31;1m" << e.what() << "\033[0m" << std::endl;
        }        
    }
}

} // namespace impl
