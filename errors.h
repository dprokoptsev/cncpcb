#pragma once

#include <map>
#include <stdexcept>

template<class Self>
class grbl_exception: public std::runtime_error {
public:
    explicit grbl_exception(int code):
        std::runtime_error(find_message(Self::MESSAGES, code)),
        code_(code)
    {}
    
    int code() const { return code_; }

protected:    
    typedef std::map<int, std::string> message_map;

private:
    int code_;
    
    static std::string find_message(const message_map& mm, int code)
    {
        auto i = mm.find(code);
        return (i != mm.end()) ? i->second : std::to_string(code);
    }
};

class grbl_error: public grbl_exception<grbl_error> {
public:
    using grbl_exception::grbl_exception;
    
    static grbl_error protocol_violation() { return grbl_error(0); }

private:
    static const message_map MESSAGES;
    friend class grbl_exception<grbl_error>;
};

class grbl_alarm: public grbl_exception<grbl_alarm> {
public:
    using grbl_exception::grbl_exception;
private:
    static const message_map MESSAGES;
    friend class grbl_exception<grbl_alarm>;
};

class grbl_reset: public std::runtime_error {
public:
    grbl_reset(): std::runtime_error("Emergency stop button pushed") {}
};
