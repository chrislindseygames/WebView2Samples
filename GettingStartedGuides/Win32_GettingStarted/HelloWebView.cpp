
#include <Windows.h>
#include "WebWindow.h"

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR     lpCmdLine,
	_In_ int       nCmdShow)
{
	WebWindow::Create(L"HelloWebView")->Show();

};
