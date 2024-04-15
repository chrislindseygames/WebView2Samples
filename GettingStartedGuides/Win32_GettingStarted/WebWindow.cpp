
#include "WebWindow.h"

#include <string>
#include <winrt/Base.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

namespace localWebWindow
{
    //---------------------------------------------------------------------------------------------
    // Function     : NormalizeUrl()
    // Description  : Returns a normalized string for the specified url.
    //---------------------------------------------------------------------------------------------
    static std::wstring NormalizeUrl(std::wstring url)
    {
        if (url.find(L"://") < 0)
        {
            if (url.length() > 1 && url[1] == ':')
                url = L"file://" + url;
            else
                url = L"http://" + url;
        }

        return url;
    }

    //---------------------------------------------------------------------------------------------
    // WebWindowImplementation class
    // This class is the implementation of the WebWindow class
    // It creates a window and a WebView2 control
    // The WebView2 control is used to display web content
    //---------------------------------------------------------------------------------------------
    class WebWindowImplementation : public WebWindow
    {
    public:
        WebWindowImplementation(std::wstring_view className);
        virtual ~WebWindowImplementation() {}

        // Inherited via WebWindow
        int Show() override;

    private:
        std::wstring                            m_className;
        HWND                                    m_hWnd = nullptr;

        winrt::com_ptr<ICoreWebView2Controller> m_pWebviewController;
        winrt::com_ptr<ICoreWebView2Settings>   m_pWebviewSettings;
        winrt::com_ptr<ICoreWebView2>           m_pWebview;

        CallbackFunc                            m_onCreationComplete = nullptr;
        CallbackFunc                            m_onNavigationComplete = nullptr;
        CallbackFunc                            m_onWebViewReady = nullptr;
        MessageReceivedFunc                     m_onMessageReceived = nullptr;
        FunctionReceivedFunc                    m_onFunctionReceived = nullptr;

        EventRegistrationToken                  m_navigationCompletedToken = {};
        EventRegistrationToken                  m_webMessageReceivedToken = {};

        void Close();
        void RegisterEventHandlers();
        void Initialize();

        HRESULT OnCreateEnvironmentCompleted(HRESULT result, ICoreWebView2Environment* pEnvironment);
        HRESULT OnCreateWebViewControllerCompleted(HRESULT result, ICoreWebView2Controller* pController);
        HRESULT OnNavigationCompleted(ICoreWebView2* pSender, ICoreWebView2NavigationCompletedEventArgs* pArgs);
        HRESULT OnWebMessageReceived(ICoreWebView2* pSender, ICoreWebView2WebMessageReceivedEventArgs* pArgs);

        void NavigateTo(std::wstring url);
        void ResizeToClientArea();
    };

    //---------------------------------------------------------------------------------------------
    // WndProc
    //---------------------------------------------------------------------------------------------
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
    // Class        : WebWindowImplementation
    // Function     : Show
    //---------------------------------------------------------------------------------------------
    int WebWindowImplementation::Show()
    {
        //-----------------------------------------------------------------------------------------
        // Register the window class.
        //-----------------------------------------------------------------------------------------
        WNDCLASSEX wcex{};

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
            MessageBox(nullptr, L"Call to RegisterClassEx failed!", m_className.c_str(), 0U);
            return 1;
        }

        //-----------------------------------------------------------------------------------------
        // Create the window.
        //-----------------------------------------------------------------------------------------
        m_hWnd = CreateWindow(m_className.c_str(),            // szWindowClass 
            m_className.c_str(),            // szTitle
            WS_OVERLAPPEDWINDOW,            // the type of window to create
            CW_USEDEFAULT, CW_USEDEFAULT,    // initial position (x, y) 
            CW_USEDEFAULT, CW_USEDEFAULT,    // initial size (width, length)
            nullptr,                        // the parent of this window
            nullptr,                        // this application does not have a menu bar        
            nullptr,                        // hInstance 
            nullptr);                         // not used in this application

        if (!m_hWnd)
        {
            MessageBox(nullptr, L"Call to CreateWindow failed!", m_className.c_str(), 0U);
            return 1;
        }

        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

        //-----------------------------------------------------------------------------------------
        // Show the window and send a WM_PAINT message to the window procedure.
        //-----------------------------------------------------------------------------------------
        ShowWindow(m_hWnd, SW_SHOWNORMAL);
        UpdateWindow(m_hWnd);

        Initialize();

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

    void WebWindowImplementation::Close()
    {
        if (m_pWebview != nullptr)
        {
            m_pWebview->remove_NavigationCompleted(m_navigationCompletedToken);
            m_pWebview->remove_WebMessageReceived(m_webMessageReceivedToken);

            m_pWebview = nullptr;
        }
        if (m_pWebviewController != nullptr)
        {
            m_pWebviewController->Close();

            m_pWebviewController = nullptr;
        }

        //Nothing to clean up for WebSettings
        m_pWebviewSettings = nullptr;
    }

    void WebWindowImplementation::RegisterEventHandlers()
    {
        m_pWebview->add_NavigationCompleted(
            Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
                this, &WebWindowImplementation::OnNavigationCompleted).Get(),
            &m_navigationCompletedToken);

        m_pWebview->add_WebMessageReceived(
            Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                this, &WebWindowImplementation::OnWebMessageReceived).Get(),
            &m_webMessageReceivedToken);
    }

    void WebWindowImplementation::Initialize()
    {
        Close();

        CreateCoreWebView2Environment(
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                this, &WebWindowImplementation::OnCreateEnvironmentCompleted).Get());
    }
    HRESULT WebWindowImplementation::OnCreateEnvironmentCompleted(HRESULT result,
        ICoreWebView2Environment* pEnvironment)
    {
        if (pEnvironment != nullptr)
        {
            pEnvironment->CreateCoreWebView2Controller(
                m_hWnd,
                Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    this, &WebWindowImplementation::OnCreateWebViewControllerCompleted).Get());
        }

        return S_OK;
    }
    HRESULT WebWindowImplementation::OnCreateWebViewControllerCompleted(HRESULT result,
        ICoreWebView2Controller* pController)
    {
        if (pController != nullptr)
        {
            m_pWebviewController.copy_from(pController);
            m_pWebviewController->get_CoreWebView2(m_pWebview.put());

            if (m_pWebview == nullptr)
            {
                return E_FAIL;
            }

            m_pWebview->get_Settings(m_pWebviewSettings.put());

            if (m_pWebviewSettings != nullptr)
            {
                m_pWebviewSettings->put_AreDefaultContextMenusEnabled(FALSE);
            }

            m_pWebview->Navigate(L"https://bing.com/");

            ResizeToClientArea();
            RegisterEventHandlers();
        }

        if (m_onCreationComplete != nullptr)
        {
            m_onCreationComplete();
        }

        return S_OK;
    }
    HRESULT WebWindowImplementation::OnNavigationCompleted(ICoreWebView2* pSender,
        ICoreWebView2NavigationCompletedEventArgs* pArgs)
    {
        if (pArgs == nullptr)
        {
            return E_FAIL;
        }

        BOOL success;
        pArgs->get_IsSuccess(&success);

        if (!success)
        {
            COREWEBVIEW2_WEB_ERROR_STATUS webErrorStatus{};
            pArgs->get_WebErrorStatus(&webErrorStatus);
            return E_FAIL;
        }

        if (m_onNavigationComplete != nullptr)
        {
            m_onNavigationComplete();
        }

        return S_OK;
    }
    HRESULT WebWindowImplementation::OnWebMessageReceived(ICoreWebView2* pSender, ICoreWebView2WebMessageReceivedEventArgs* pArgs)
    {
        LPWSTR pwStr;
        HRESULT result = pArgs->TryGetWebMessageAsString(&pwStr);
        std::wstring message = pwStr;
        if (message == L"WebViewReady")
        {
            if (m_onWebViewReady != nullptr)
            {
                m_onWebViewReady();
            }
        }
        else if (message._Starts_with(L"{{") && m_onFunctionReceived != nullptr)
        {
            size_t iEndFunctionName = message.find(L"}}", 2);
            if (iEndFunctionName != std::wstring::npos)
            {
                std::wstring function = message.substr(2, iEndFunctionName - 2);
                std::wstring arguments = message.substr(iEndFunctionName + 2);

                m_onFunctionReceived(function, arguments);
            }
        }
        else if (m_onMessageReceived != nullptr)
        {
            m_onMessageReceived(message);
        }

        return result;
    }
    void WebWindowImplementation::NavigateTo(std::wstring url)
    {
        if (m_pWebview != nullptr)
        {
            m_pWebview->Navigate(NormalizeUrl(url).c_str());
        }
    }
    void WebWindowImplementation::ResizeToClientArea()
    {
        if (m_pWebviewController != nullptr)
        {
            // Resize WebView to fit the bounds of the parent window
            RECT bounds;
            GetClientRect(m_hWnd, &bounds);
            m_pWebviewController->put_Bounds(bounds);
        }
    }
};
using namespace localWebWindow;

//-------------------------------------------------------------------------------------------------
// Class        : WebWindow
// Function     : Create
// Description  : Create a new WebWindowImplementation object
//-------------------------------------------------------------------------------------------------
std::unique_ptr<WebWindow> WebWindow::Create(std::wstring_view className)
{
    return std::make_unique<WebWindowImplementation>(className);
}
