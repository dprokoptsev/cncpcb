#pragma once

enum class key {
    UP,
    DOWN, 
    LEFT,
    RIGHT,
    PAGE_UP,
    PAGE_DOWN,
    PLUS,
    MINUS
};


class interactive {
public:
    interactive();
    ~interactive();
    key getkey();
};
