#pragma once

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include <vector>
#include <string>
#include <memory>


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
