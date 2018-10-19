#include "cnc.h"
#include "serial.h"
#include "geom.h"
#include <string>

#include <readline/readline.h>
#include <readline/history.h>


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

int main()
{
    try {
        serial_port tty("/dev/ttyUSB0");
        cnc_machine cnc(tty);
        
        std::string cmd;
        while (readline("> ", cmd)) {
            if (cmd == "reset") {
                cnc.reset();
            } else if (cmd == "pos") {
                std::cout << cnc.position() << std::endl;
            } else if (cmd == "setzero") {
                cnc.set_zero();
            } else if (cmd.substr(0, 3) == "xy ") {
                cnc.move_xy(point(cmd.substr(3)));
            } else if (cmd.substr(0, 2) == "z ") {
                cnc.move_z(lexical_cast<double>(cmd.substr(2)));
            } else if (cmd == "probe") {
                std::cout << cnc.probe() << std::endl;
            } else if (!cmd.empty() && cmd[0] == '.') {
                cnc.talk(cmd.substr(1));
            } else {
                std::cout << "unknown command" << std::endl;
            }
        }
        
        std::cout << std::endl;
        return 0;
    }
    catch (std::runtime_error& e) {
        std::cerr << "cnc: " << e.what() << std::endl;
        return 1;
    }
}
