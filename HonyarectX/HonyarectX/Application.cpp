#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"

/// <summary>ウィンドウ定数</summary>
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

/// <summary>面倒だけど書かなあかんやつ</summary>
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		// ウィンドウが破棄されたら呼ばれる
		// OSに対して「もうこのアプリは終わる」と伝える
		PostQuitMessage(0);
		return 0;
	}
	// 規定の処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	HINSTANCE hInst = GetModuleHandle(nullptr);

	// ウィンドウクラス生成＆登録
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;		// コールバック関数の指定
	windowClass.lpszClassName = _T("HonyarectX");			// アプリケーションクラス名（適当でよい）
	windowClass.hInstance = GetModuleHandle(0);				// ハンドルの取得
	RegisterClassEx(&_windowClass);							// アプリケーションクラス（ウィンドウクラスの指定をOSに伝える）

	RECT wrc = { 0, 0, window_width, window_height };		// ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);		// 関数を使ってウィンドウのサイズを補正する

	// ウィンドウオブジェクトの生成
	hwnd = CreateWindow(
		windowClass.lpszClassName,		// クラス名指定
		_T("HonyarectX"),				// タイトルバーの文字
		WS_OVERLAPPEDWINDOW,			// タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT,					// 表示x座標はOSにお任せ
		CW_USEDEFAULT,					// 表示y座標はOSにお任せ
		wrc.right - wrc.left,			// ウィンドウ幅
		wrc.bottom - wrc.top,			// ウィンドウ高
		nullptr,						// 親ウィンドウハンドル
		nullptr,						// メニューハンドル
		windowClass.hInstance,			// 呼び出しアプリケーションハンドル
		nullptr);						// 追加パラメーター
}

SIZE Application::GetWindowSize() const
{
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW);			// ウィンドウ表示
	float angle = 0.0f;
	MSG msg = {};
	UINT frame = 0;
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {
			break;
		}

		// 全体の描画準備
		_dx12->BeginDraw();

		// PMD用の描画パイプラインに合わせる
		_dx12->CommandList()->SetPipelineState(_pmdRenderer->GetPipelineState());
		// ルートシグネチャもPMD用に合わせる
		_dx12->CommandList()->SetGraphicsRootSignature(_pmdRenderer->GetRootSignature());

		_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_dx12->SetScene();

		_pmdActor->Update();
		_pmdActor->Draw();

		_dx12->EndDraw();

		// フリップ
		_dx12->Swapchain()->Present(1, 0);
	}
}

bool Application::Init()
{
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	CreateGameWindow(_hwnd, _windowClass);

	// DirectX12ラッパー生成＆初期化
	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));
	_pmdActor.reset(new PMDActor("Model/初音ミク.pmd", *_pmdRenderer));
	//_pmdActor->LoadVMDFile("motion/motion.vmd", "pose");
	_pmdActor->LoadVMDFile("motion/squat2.vmd", "pose");
	_pmdActor->PlayAnimation();

	return true;
}

void Application::Terminate()
{
	// もうクラスは使わないので登録解除する
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

Application& Application::Instance()
{
	static Application instance;
	return instance;
}

Application::Application()
{
}

Application::~Application()
{
}
