#pragma once

#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#include <wrl.h>
#include <memory>

class Dx12Wrapper;
class PMDRenderer;
class PMDActor;

/// <summary>
/// シングルトン
/// </summary>
class Application
{
private:
	/// <summary>ウィンドウ周り</summary>
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	std::shared_ptr<Dx12Wrapper> _dx12;
	std::shared_ptr<PMDRenderer> _pmdRenderer;
	std::shared_ptr<PMDActor> _pmdActor;
	
	/// <summary>ゲーム用ウィンドウの生成</summary>
	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);

	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

public:
	/// <summary>Applicationのシングルトンインスタンスを得る</summary>
	static Application& Instance();

	/// <summary>初期化</summary>
	bool Init();

	/// <summary>ループ起動</summary>
	void Run();

	/// <summary>後処理</summary>
	void Terminate();
	SIZE GetWindowSize() const;
	~Application();

};
