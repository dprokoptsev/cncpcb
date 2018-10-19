#include "errors.h"
#include "utility.h"
#include "geom.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <memory>
#include <utility>

#include <readline/readline.h>
#include <readline/history.h>


template<class F, class... Args>
decltype(std::declval<F>()(std::declval<Args>()...))
check_syscall(const std::string& msg, F f, Args... args)
{
    auto ret = f(args...);
    if (ret == -1)
        throw std::runtime_error("cannot " + msg + ": " + strerror(errno));
    return ret;
}


class serial_streambuf: public std::streambuf {
public:
    explicit serial_streambuf(const std::string& device)
    {
        fd_ = check_syscall("open serial port", &::open, device.c_str(), O_RDWR);
        
        struct termios t;
        check_syscall("configure serial port", &::tcgetattr, fd_, &t);
        cfmakeraw(&t);
        cfsetispeed(&t, B115200);
        cfsetospeed(&t, B115200);
        check_syscall("configure serial port", &::tcsetattr, fd_, TCSANOW, &t);
    }
    
    ~serial_streambuf() { ::close(fd_); }
    
    serial_streambuf(const serial_streambuf&) = delete;
    serial_streambuf& operator = (const serial_streambuf&) = delete;
    
    int_type underflow() override
    {
        rdbuf_.resize(BUFSIZE);
        ssize_t len = check_syscall("read from serial port", &::read, fd_, rdbuf_.data(), rdbuf_.size());
        if (len == 0)
            return traits_type::eof();
        setg(rdbuf_.data(), rdbuf_.data(), rdbuf_.data() + len);
        return (unsigned char) rdbuf_[0];
    }
    
    int_type overflow(int_type c) override
    {
        if (pptr() != pbase()) {
            ssize_t len = check_syscall("write to serial port", &::write, fd_, pbase(), pptr() - pbase());
            if (len != pptr() - pbase())
                throw std::runtime_error("cannot write to serial port: EOF");
        }
        
        wrbuf_.resize(BUFSIZE);
        char* pptr = wrbuf_.data();
        if (c != traits_type::eof())
            *pptr++ = c;
        
        setp(pptr, wrbuf_.data() + wrbuf_.size());
        return 0;
    }
    
    int_type sync() override { return overflow(traits_type::eof()); }
    
    int fd() { return fd_; }
    
private:
    int fd_;
    std::vector<char> rdbuf_, wrbuf_;
    
    static const size_t BUFSIZE = 4096;
};

class serial_port: public std::iostream {
public:
    serial_port() {}
    explicit serial_port(const std::string& device): buf_(new serial_streambuf(device)) { rdbuf(buf_.get()); }
    serial_port& operator = (serial_port&& s) { buf_ = std::move(s.buf_); rdbuf(buf_.get()); s.rdbuf(0); return *this; }
    serial_port(serial_port&& s) { *this = std::move(s); }
    
    int fd() { return buf_ ? buf_->fd() : -1; }
    
private:
    std::unique_ptr<serial_streambuf> buf_;
};



class cnc_machine {
public:
    explicit cnc_machine(std::iostream& s): s_(&s)
    {
        for (;;) {
            std::string line;
            if (!std::getline(*s_, line))
                throw grbl_error::protocol_violation();
            if (line.substr(0, 5) == "Grbl ")
                break;
        }
        
        talk("?");
        read_wco();
    }
    
    void reset() { talk("\x18"); }
    
    point position()
    {        
        std::string status;
        while (status.empty()) {
            auto st = talk("?");
            if (!st.empty())
                status = st[0];            
        }
        
        point wpos;
        bool found = false;
        for (const std::string& field: split<std::string>(status, "|")) {
            if (field.substr(0, 5) == "MPos:") {
                return point(field.substr(5));
            } else if (field.substr(0, 5) == "WPos:") {
                wpos = point(field.substr(5));
                found = true;
            } else if (field.substr(0, 4) == "WCO:") {
                wco_ = vector(field.substr(4));
            }
        }
        if (found)
            return wpos + wco_;
        else
            throw grbl_error::protocol_violation();
    }
    
    void redefine_position(point newpos)
    {
        talk("G10 P0 L20 " + newpos.grbl());
        read_wco();
    }
    
private /*methods*/:
    
public:
    std::vector<std::string> talk(const std::string& cmd)
    {
        std::cerr << "\033[33m... send: " << cmd << "\033[0m" << std::endl;
        
        *s_ << cmd << std::endl;
        
        std::vector<std::string> resp;
        std::string line;
        while (std::getline(*s_, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.pop_back();
            
            std::cerr << "\033[32m... recv: " << line << "\033[0m" << std::endl;
            
            if (line == "ok") {
                return resp;
            } else if (line.substr(0, 6) == "error:") {
                throw grbl_error(lexical_cast<int>(line.substr(6)));
            } else if (line.substr(0, 6) == "ALARM:") {
                throw grbl_alarm(lexical_cast<int>(line.substr(6)));
            } else if (line.size() >= 2 && line[0] == '<' && line[line.size() - 1] == '>') {
                resp.push_back(line.substr(1, line.size() - 2));
            } else if (line.size() >= 2 && line[0] == '[' && line[line.size() - 1] == ']') {
                resp.push_back(line.substr(1, line.size() - 2));
            } else {
                continue;
            }
        }
        
        throw grbl_error::protocol_violation();
    }
    
    void read_wco()
    {
        for (const std::string& line: talk("$#")) {
            auto fields = split<std::string>(line, ":");
            if (fields.size() >= 2 && fields[0] == "G54") {
                wco_ = vector(fields[1]);
                break;
            }
        }
    }
    
private /*fields*/:
    std::iostream* s_;
    
    vector wco_;
};



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
