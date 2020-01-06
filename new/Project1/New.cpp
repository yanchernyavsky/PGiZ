//PGIZ_FIRST.cpp----------------------------------------------------------------------------------

#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include "Resource.h"

float m = 0.0f, n = 0.0f, u = 0.0f, k = 0.0f;
double Speed = 20;
XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};


struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE               g_hInst = NULL;
HWND                    g_hWnd = NULL;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pImmediateContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11Texture2D* g_pDepthStencil = NULL;
ID3D11DepthStencilView* g_pDepthStencilView = NULL;
ID3D11VertexShader* g_pVertexShader = NULL;
ID3D11PixelShader* g_pPixelShader = NULL;
ID3D11InputLayout* g_pVertexLayout = NULL;
ID3D11Buffer* g_pVertexBuffer = NULL;
ID3D11Buffer* g_pIndexBuffer = NULL;
ID3D11Buffer* g_pConstantBuffer = NULL;
XMMATRIX                g_World1;
XMMATRIX                g_World2;
XMMATRIX                g_View;
XMMATRIX                g_Projection;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}

	CleanupDevice();

	return (int)msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"TutorialWindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 640, 480 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(hWnd, "Respect", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
	if (FAILED(hr))
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		if (pErrorBlob) pErrorBlob->Release();
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;


	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)& pBackBuffer);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	// Compile the vertex shader
	ID3DBlob* pVSBlob = NULL;
	hr = CompileShaderFromFile(L"Tutorial05.fx", "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Compile the pixel shader
	ID3DBlob* pPSBlob = NULL;
	hr = CompileShaderFromFile(L"Tutorial05.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Create vertex buffer
	SimpleVertex vertices[] =
	{

		{ XMFLOAT3(4.0f, 1.0f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 1.0f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 2.5f, 0.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		{ XMFLOAT3(4.0f, 1.0f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.0f, 2.5f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 2.5f, 0.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		{ XMFLOAT3(2.0f, 2.5f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(7.0f, 2.5f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 5.0f, 0.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(2.5f, 3.8f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(6.5f, 3.8f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 6.0f, 0.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(3.0f, 5.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(6.0f, 5.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 7.0f, 0.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		///////////////////////////////////////////////////

		{ XMFLOAT3(4.0f, 1.0f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 1.0f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 2.5f, 1.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		{ XMFLOAT3(4.0f, 1.0f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.0f, 2.5f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 2.5f, 1.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		{ XMFLOAT3(2.0f, 2.5f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(7.0f, 2.5f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 5.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(2.5f, 3.8f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(6.5f, 3.8f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 6.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(3.0f, 5.0f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(6.0f, 5.0f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 7.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		//////////////////////////////////////////////////////////////// ствол

		{ XMFLOAT3(4.0f, 1.0f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 1.0f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 1.0f, 1.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		{ XMFLOAT3(4.0f, 1.0f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 1.0f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.0f, 1.0f, 1.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		{ XMFLOAT3(4.0f, 1.0f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.0f, 1.0f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.0f, 2.5f, 0.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		{ XMFLOAT3(4.0f, 2.5f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.0f, 1.0f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.0f, 2.5f, 0.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },


		{ XMFLOAT3(5.0f, 1.0f, 0.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 1.0f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 2.5f, 0.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		{ XMFLOAT3(5.0f, 2.5f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 1.0f, 1.0f),   XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },
		{ XMFLOAT3(5.0f, 2.5f, 0.0f),  XMFLOAT4(1.0f, 0.25f, 0.0f, 1.0f) },

		////////////////////////////////////////////////// конец ствола

		{ XMFLOAT3(2.0f, 2.5f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(7.0f, 2.5f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(7.0f, 2.5f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(2.0f, 2.5f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(2.0f, 2.5f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(7.0f, 2.5f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(2.0f, 2.5f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 5.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 5.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

		{ XMFLOAT3(2.0f, 2.5f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(2.0f, 2.5f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 5.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

		{ XMFLOAT3(7.0f, 2.5f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 5.0f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 5.0f, 1.0f),  XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(7.0f, 2.5f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(7.0f, 2.5f, 1.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(4.5f, 5.0f, 1.0f),  XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(2.5f, 3.8f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(6.5f, 3.8f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(6.5f, 3.8f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		{ XMFLOAT3(2.5f, 3.8f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(2.5f, 3.8f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(6.5f, 3.8f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		//////////////// стенки 2

	{ XMFLOAT3(2.5f, 3.8f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
	{ XMFLOAT3(4.5f, 6.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
	{ XMFLOAT3(4.5f, 6.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

	{ XMFLOAT3(2.5f, 3.8f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
	{ XMFLOAT3(2.5f, 3.8f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
	{ XMFLOAT3(4.5f, 6.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

	{ XMFLOAT3(6.5f, 3.8f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
	{ XMFLOAT3(4.5f, 6.0f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
	{ XMFLOAT3(4.5f, 6.0f, 1.0f),  XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

	{ XMFLOAT3(6.5f, 3.8f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
	{ XMFLOAT3(6.5f, 3.8f, 1.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
	{ XMFLOAT3(4.5f, 6.0f, 1.0f),  XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

	///////////////////////стенки 3

   { XMFLOAT3(3.0f, 5.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
   { XMFLOAT3(4.5f, 7.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
   { XMFLOAT3(4.5f, 7.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

   { XMFLOAT3(3.0f, 5.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
   { XMFLOAT3(3.0f, 5.0f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
   { XMFLOAT3(4.5f, 7.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

   { XMFLOAT3(6.0f, 5.0f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
   { XMFLOAT3(4.5f, 7.0f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
   { XMFLOAT3(4.5f, 7.0f, 1.0f),  XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

   { XMFLOAT3(6.0f, 5.0f, 0.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
   { XMFLOAT3(6.0f, 5.0f, 1.0f),   XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
   { XMFLOAT3(4.5f, 7.0f, 1.0f),  XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

   { XMFLOAT3(3.0f, 5.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
   { XMFLOAT3(6.0f, 5.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
   { XMFLOAT3(6.0f, 5.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

   { XMFLOAT3(3.0f, 5.0f, 0.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
   { XMFLOAT3(3.0f, 5.0f, 1.0f),   XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
   { XMFLOAT3(6.0f, 5.0f, 1.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

	};
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 102;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Create index buffer
	WORD indices[] =
	{

		0,1,2,
		2,1,0,

		3,4,5,
		5,4,3,

		6,7,8,
		8,7,6,

		9,10,11,
		11,10,9,

		12,13,14,
		14,13,12,

		15,16,17,
		17,16,15,

		18,19,20,
		20,19,18,

		21,22,23,
		23,22,21,

		24,25,26,
		26,25,24,

		27,28,29,
		29,28,27,

		30,31,32,
		32,31,30,

		33,34,35,
		35,34,33,

		36,37,38,
		38,37,36,

		39,40,41,
		41,40,39,

		42,43,44,
		44,43,42,

		45,46,47,
		47,46,45,

		48,49,50,
		50,49,48,

		51,52,53,
		53,52,51,

		54,55,56,
		56,55,54,

		57,58,59,
		59,58,57,

		60,61,62,
		62,61,60,

		63,64,65,
		65,64,63,

		66,67,68,
		68,67,66,

		69,70,71,
		71,70,69,

		72,73,74,
		74,73,72,

		75,76,77,
		77,76,75,

		78,79,80,
		80,79,78,

		81,82,83,
		83,82,81,

		84,85,86,
		86,85,84,

		87,88,89,
		89,88,87,

		90,91,92,
		92,91,90,

		93,94,95,
		95,94,93,

		96,97,98,
		98,97,96,

		99,100,101,
		101,100,99,

	};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * 206;        // 36 vertices needed for 12 triangles in a triangle list
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
	if (FAILED(hr))
		return hr;

	// Set index buffer
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pConstantBuffer);
	if (FAILED(hr))
		return hr;

	// Initialize the world matrix
	g_World1 = XMMatrixIdentity();
	g_World2 = XMMatrixIdentity();
	// Initialize the view matrix1

	g_View = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (FLOAT)height, 0.01f, 100.0f);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	if (g_pConstantBuffer) g_pConstantBuffer->Release();
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (g_pVertexLayout) g_pVertexLayout->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}

double xx, yy, zz, vx, vy, vz, s = 0;

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	double dt = 0.4 * Speed;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_RBUTTONDOWN:
		zz -= 1.1;
		break;

	case WM_LBUTTONDOWN:
		zz += 1.1;
		break;

	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'A': xx -= .1; break;
		case 'D': xx += .1; break;
		case 'W': yy += .1; break;
		case 'S': yy -= .1; break;
		case 'Q': zz += .1; break;
		case 'E': zz -= .1; break;
		case 'F': vx -= .1; break;
		case 'H': vx += .1; break;
		case 'T': vy += .1; break;
		case 'G': vy -= .1; break;
		case 'Y': vz -= .1; break;
		case 'Z': m += .01; break;
		case 'X': n -= .01; break;
		case 'O': s += .01; break;
		}

		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	XMVECTOR Eye = XMVectorSet(0.0f + xx, 1.0f + yy, -5.0f + zz, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f + vx, 1.0f + vy, 0.0f + vz, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	g_View = XMMatrixLookAtLH(Eye, At, Up);
	return 0;
}

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	// Update our time
	static float t = 0.0f;
	if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		t += (float)XM_PI * 0.0125f;
	}
	else
	{
		static DWORD dwTimeStart = 0;
		DWORD dwTimeCur = GetTickCount();
		if (dwTimeStart == 0)
			dwTimeStart = dwTimeCur;
		t = (dwTimeCur - dwTimeStart) / 1000.0f;
	}

	// 1st Cube: Rotate around the origin
	g_World1 = XMMatrixRotationY(t);
	//g_World1 = XMMatrixRotationX(s);

	// 2nd Cube:  Rotate around origin
	XMMATRIX mSpin = XMMatrixRotationZ(-t * 0.5f);
	XMMATRIX mOrbit = XMMatrixRotationY(-t * 0.4f * (t));
	XMMATRIX mTranslate = XMMatrixTranslation(-2.0f, 0.0f + u + k, 0.0f);
	XMMATRIX mScale = XMMatrixScaling(0.4f + m + n, 0.4f + m + n, 0.4f + m + n);

	g_World2 = mScale * mSpin * mTranslate * mOrbit;

	//
	// Clear the back buffer
	//
	float ClearColor[4] = { 1.0f, 0.5f, 0.3f, 1.0f }; //red, green, blue, alpha
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

	//
	// Clear the depth buffer to 1.0 (max depth)
	//
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	//
	// Update variables for the first cube
	//
	ConstantBuffer cb1;
	for (int i = 0; i < 10; i++)
	{
		cb1.mWorld = XMMatrixTranspose(XMMatrixTranslation(((float)i - 5.0f) * 10.0f, ((float)2 * sin(i / 2)), 0)) * XMMatrixRotationY(t);
		cb1.mView = XMMatrixTranspose(g_View);
		cb1.mProjection = XMMatrixTranspose(g_Projection);
		g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &cb1, 0, 0);

		//
		// Render the first 
		//
		g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
		g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
		g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);
		g_pImmediateContext->DrawIndexed(206, 0, 0); //
	}
	//
	// Update variables for the second cube
	//
	/*ConstantBuffer cb2;

	cb2.mWorld = XMMatrixTranspose(g_World2);
	cb2.mView = XMMatrixTranspose(g_View);
	cb2.mProjection = XMMatrixTranspose(g_Projection);
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &cb2, 0, 0);
	//
	// Render the second cube
	//
	g_pImmediateContext->DrawIndexed(42, 0, 0);*/

	//
	// Present our back buffer to our front buffer
	//
	g_pSwapChain->Present(0, 0);
}
