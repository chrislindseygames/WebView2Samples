#pragma once
#include <memory>
#include <string>
#include <functional>

class WebWindow
{
public:
    using CallbackFunc = std::function<void()>;
    using MessageReceivedFunc = std::function<void(std::wstring)>;
    using FunctionReceivedFunc = std::function<void(std::wstring, std::wstring)>;
public:
    virtual ~WebWindow() {}
    static std::unique_ptr<WebWindow> Create(std::wstring_view className);

    virtual int Show() = 0;

protected:
    WebWindow() {}
};

