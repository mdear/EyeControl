#include "stdafx.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <fstream>
#include <iostream>
#include <objidl.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")
#include "OVManager.h"

int q = 0;
int r = 0;

OVManager* OVM;

VOID OnPaint(HDC hdc)	//this is the draw function for everything in the overlay. Currently just routes to the OVM, which passes it down to the controls.
{
	OVM->draw(hdc);
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{
	HWND                hWnd;
	MSG                 msg;
	WNDCLASS            wndClass;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	// Initialize GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	//LOAD BITMAP FUNCTION, left here for reference
	/*HBITMAP result = NULL;
	WCHAR* filename = L"../img/smiley.png";
	Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(filename, false);
	if (bitmap)
	{
		bitmap->GetHBITMAP(Color(255, 255, 255), &result);
		delete bitmap;
	}*/

	// Initialize overlay
	OVM = new OVManager(&hWnd);

	ControlDriver::Init();


















	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = WndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hInstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = TEXT("EyeControl");

	RegisterClass(&wndClass);

	hWnd = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_TOPMOST,	//extended window style
		TEXT("EyeControl"),   // window class name
		TEXT("EyeControl"),  // window caption
		WS_MAXIMIZE,      // window style
		CW_USEDEFAULT,            // initial x position
		CW_USEDEFAULT,            // initial y position
		CW_USEDEFAULT,            // initial x size
		CW_USEDEFAULT,            // initial y size
		NULL,                     // parent window handle
		NULL,                     // window menu handle
		hInstance,                // program instance handle
		NULL);                    // creation parameters


	SetWindowLong(hWnd, GWL_STYLE, 0);	//nullifies style to remove borders and title bar
	SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);	//sets so white is transparent
	ShowWindow(hWnd, iCmdShow);
	UpdateWindow(hWnd);

	while (OVM->loop())	//run system loop, if loop returns false exit conditions have been met
	{
		//RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);	//TODO: place within framerate manager in other program (drawing too darn much), or possibly only trigger on changes





		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	delete OVM;
	GdiplusShutdown(gdiplusToken);
	//return msg.wParam;
}  // WinMain

LRESULT CALLBACK WndProc(HWND hWnd, UINT message,
	WPARAM wParam, LPARAM lParam)
{
	HDC          hdc;
	PAINTSTRUCT  ps;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		OnPaint(hdc);
		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
} // WndProc