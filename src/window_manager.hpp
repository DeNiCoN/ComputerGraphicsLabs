#pragma once

class WindowManager
{
public:
    WindowManager& Instance()
    {
        static WindowManager instance;
        return instance;
    }
private:
    WindowManager();
    ~WindowManager();
};
