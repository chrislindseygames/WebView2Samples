
#include "WebWindow.h"

#include <string>
#include <tchar.h>
#include <winrt/Base.h>
#include <wil/com.h>
#include <wrl.h>
#include <WebView2.h>

namespace localWebWindow
{
	//---------------------------------------------------------------------------------------------s
	// WebWindowImplementation class
	// This class is the implementation of the WebWindow class
	// It creates a window and a WebView2 control
	// The WebView2 control is used to display web content
	//---------------------------------------------------------------------------------------------
    class WebWindowImplementation : public WebWindow
    {
		std::wstring m_className;

		// Pointer to WebViewController
		winrt::com_ptr<ICoreWebView2Controller> webviewController;

		// Pointer to WebView window
		winrt::com_ptr<ICoreWebView2> webview;

    public:
		WebWindowImplementation(std::wstring_view className);
        virtual ~WebWindowImplementation() {}

        // Inherited via WebWindow
        int Show() override;
    };

	//---------------------------------------------------------------------------------------------
	// WndProc
	//---------------------------------------------------------------------------------------------
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_SIZE:
            /*if (webviewController != nullptr) {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                webviewController->put_Bounds(bounds);
            };*/
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
            break;
        }

        return 0;
    }

	WebWindowImplementation::WebWindowImplementation(std::wstring_view className)
		: m_className(className)
	{}

	//---------------------------------------------------------------------------------------------
	// Class		: WebWindowImplementation
	// Function		: Show
	//---------------------------------------------------------------------------------------------
    int WebWindowImplementation::Show()
	{
		//-----------------------------------------------------------------------------------------
		// Register the window class.
		//-----------------------------------------------------------------------------------------
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = nullptr;
		wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = m_className.c_str();
		wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

		if (!RegisterClassEx(&wcex))
		{
			MessageBox(nullptr, _T("Call to RegisterClassEx failed!"), m_className.c_str(), 0U);
			return 1;
		}

		//-----------------------------------------------------------------------------------------
		// Create the window.
		//-----------------------------------------------------------------------------------------
		HWND hWnd = CreateWindow(m_className.c_str(),			// szWindowClass 
			m_className.c_str(),			// szTitle
			WS_OVERLAPPEDWINDOW,			// the type of window to create
			CW_USEDEFAULT, CW_USEDEFAULT,	// initial position (x, y) 
			CW_USEDEFAULT, CW_USEDEFAULT,	// initial size (width, length)
			nullptr,						// the parent of this window
			nullptr,						// this application does not have a menu bar		
			nullptr,						// hInstance 
			nullptr);					// not used in this application

		if (!hWnd)
		{
			MessageBox(nullptr, _T("Call to CreateWindow failed!"), m_className.c_str(), 0U);
			return 1;
		}

		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

		//-----------------------------------------------------------------------------------------
		// Show the window and send a WM_PAINT message to the window procedure.
		//-----------------------------------------------------------------------------------------
		ShowWindow(hWnd, SW_SHOWNORMAL);
		UpdateWindow(hWnd);

		// <-- WebView2 sample code starts here -->
		// Step 3 - Create a single WebView within the parent window
		// Locate the browser and set up the environment for WebView
		CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
			Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
				[&](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {

					// Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
					env->CreateCoreWebView2Controller(hWnd, Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[&](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
							if (controller != nullptr) {
								webviewController.copy_from(controller);
								webviewController->get_CoreWebView2(webview.put());
							}

							// Add a few settings for the webview
							// The demo step is redundant since the values are the default settings
							winrt::com_ptr<ICoreWebView2Settings> settings;
							webview->get_Settings(settings.put());
							settings->put_IsScriptEnabled(TRUE);
							settings->put_AreDefaultScriptDialogsEnabled(TRUE);
							settings->put_IsWebMessageEnabled(TRUE);

							// Resize WebView to fit the bounds of the parent window
							RECT bounds;
							GetClientRect(hWnd, &bounds);
							webviewController->put_Bounds(bounds);

							// Schedule an async task to navigate to Bing
							webview->Navigate(L"https://google.com/");

							// <NavigationEvents>
							// Step 4 - Navigation events
							// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation
							EventRegistrationToken token;
							webview->add_NavigationStarting(Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
								[](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
									wil::unique_cotaskmem_string uri;
									args->get_Uri(&uri);
									std::wstring source(uri.get());
									if (source.substr(0, 5) != L"https") {
										args->put_Cancel(true);
									}
									return S_OK;
								}).Get(), &token);
							// </NavigationEvents>

							// <Scripting>
							// Step 5 - Scripting
							// Schedule an async task to add initialization script that freezes the Object object
							webview->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
							// Schedule an async task to get the document URL
							webview->ExecuteScript(L"window.document.URL;", Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
								[](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
									LPCWSTR URL = resultObjectAsJson;
									//doSomethingWithURL(URL);
									return S_OK;
								}).Get());
							// </Scripting>

							// <CommunicationHostWeb>
							// Step 6 - Communication between host and web content
							// Set an event handler for the host to return received message back to the web content
							webview->add_WebMessageReceived(Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
								[](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
									wil::unique_cotaskmem_string message;
									args->TryGetWebMessageAsString(&message);
									// processMessage(&message);
									webview->PostWebMessageAsString(message.get());
									return S_OK;
								}).Get(), &token);

							// Schedule an async task to add initialization script that
							// 1) Add an listener to print message from the host
							// 2) Post document URL to the host
							webview->AddScriptToExecuteOnDocumentCreated(
								L"window.chrome.webview.addEventListener(\'message\', event => alert(event.data));" \
								L"window.chrome.webview.postMessage(window.document.URL);",
								nullptr);
							// </CommunicationHostWeb>

							return S_OK;
						}).Get());
					return S_OK;
				}).Get());



		// <-- WebView2 sample code ends here -->

		//-----------------------------------------------------------------------------------------
		// Main message loop:
		//-----------------------------------------------------------------------------------------
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return (int)msg.wParam;
	}
};
using namespace localWebWindow;

//-------------------------------------------------------------------------------------------------
// Class		: WebWindow
// Function		: Create
// Description	: Create a new WebWindowImplementation object
//-------------------------------------------------------------------------------------------------
std::unique_ptr<WebWindow> WebWindow::Create(std::wstring_view className)
{
    return std::make_unique<WebWindowImplementation>(className);
}
