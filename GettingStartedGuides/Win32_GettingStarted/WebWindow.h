#pragma once
#include <memory>
#include <string>

class WebWindow
{
public:
	virtual ~WebWindow() {}
	static std::unique_ptr<WebWindow> Create(std::wstring_view className);

	virtual int Show() = 0;

protected:
	WebWindow() {}
};

