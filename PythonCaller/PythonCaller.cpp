// PythonCaller.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "PythonCaller.h"
#include <shellapi.h>
#include <stdio.h>

#define DEBUG_STR 1

#if DEBUG_STR
	#define OutDbgStrW(msg) OutputDebugStringW(msg)
#else
	#define OutDbgStrW(msg)
#endif

typedef int (*Py_Main_Func)(int argc, wchar_t** argv);

static void TrimTrailingNewline(wchar_t* line)
{
	size_t len = wcslen(line);
	while (len > 0 && (line[len - 1] == L'\n' || line[len - 1] == L'\r'))
	{
		line[--len] = L'\0';
	}
}

static bool ReadConfLine(FILE* pFile, wchar_t* line, size_t lineSize)
{
	if (fgetws(line, (int)lineSize, pFile) == nullptr)
	{
		line[0] = L'\0';
		return false;
	}

	TrimTrailingNewline(line);
	return line[0] != L'\0';
}

static bool CombinePath(wchar_t* dest, size_t destCount, const wchar_t* baseDir, const wchar_t* relativePath)
{
	if (relativePath[0] == L'\0')
	{
		return false;
	}

	if (relativePath[1] == L':' || relativePath[0] == L'\\' || wcsncmp(relativePath, L"\\\\", 2) == 0)
	{
		wcscpy_s(dest, destCount, relativePath);
		return true;
	}

	const size_t baseLen = wcslen(baseDir);
	wcscpy_s(dest, destCount, baseDir);
	if (baseLen > 0 && baseDir[baseLen - 1] != L'\\' && baseDir[baseLen - 1] != L'/')
	{
		wcscat_s(dest, destCount, L"\\");
	}
	wcscat_s(dest, destCount, relativePath);
	return true;
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	wchar_t exePath[MAX_PATH]{};
	wchar_t exeDir[MAX_PATH]{};
	wchar_t confPath[MAX_PATH]{};
	wchar_t workDir[MAX_PATH]{};
	wchar_t scriptPath[MAX_PATH]{};
	wchar_t pythonDllPath[MAX_PATH]{};

	if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0)
	{
		OutDbgStrW(L"GetModuleFileNameW failed");
		return -1;
	}
	OutDbgStrW(L"\n--------\nGetModuleFileNameW=");
	OutDbgStrW(exePath);

	wcscpy_s(exeDir, exePath);
	wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
	if (lastSlash == nullptr)
	{
		OutDbgStrW(L"exe path has no directory");
		return -1;
	}
	*(lastSlash + 1) = L'\0';

	wcscpy_s(confPath, exePath);
	wcscat_s(confPath, L".conf");

	FILE* pFile = nullptr;
	if (_wfopen_s(&pFile, confPath, L"rt") != 0 || pFile == nullptr)
	{
		OutDbgStrW(L".conf not found");
		return -1;
	}

	wchar_t workDirRelative[MAX_PATH]{};
	wchar_t scriptRelative[MAX_PATH]{};
	if (!ReadConfLine(pFile, workDirRelative, MAX_PATH) ||
		!ReadConfLine(pFile, scriptRelative, MAX_PATH))
	{
		fclose(pFile);
		OutDbgStrW(L".conf invalid");
		return -1;
	}
	fclose(pFile);

	if (!CombinePath(workDir, MAX_PATH, exeDir, workDirRelative) ||
		!CombinePath(scriptPath, MAX_PATH, workDir, scriptRelative) ||
		!CombinePath(pythonDllPath, MAX_PATH, workDir, L"python3.dll"))
	{
		OutDbgStrW(L"path combine failed");
		return -1;
	}

	OutDbgStrW(L"SetCurrentDirectoryW:");
	OutDbgStrW(workDir);
	if (!SetCurrentDirectoryW(workDir))
	{
		OutDbgStrW(L"SetCurrentDirectoryW failed");
		return -1;
	}

	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == nullptr)
	{
		OutDbgStrW(L"CommandLineToArgvW failed");
		return -1;
	}

	wchar_t** wargv = (wchar_t**)calloc((size_t)argc + 2, sizeof(wchar_t*));
	if (wargv == nullptr)
	{
		LocalFree(argv);
		return -1;
	}

	wargv[0] = exePath;   // not argv[0]: may be relative when launched from another directory
	wargv[1] = scriptPath;
	for (int n = 1; n < argc; ++n)
	{
		wargv[n + 1] = argv[n];
	}
	argc += 1;

	HMODULE hLib = LoadLibraryW(pythonDllPath);
	if (hLib == nullptr)
	{
		OutDbgStrW(L"load python3.dll failed");
		LocalFree(argv);
		free(wargv);
		return -1;
	}

	int exitCode = -1;
	Py_Main_Func PyMain = (Py_Main_Func)GetProcAddress(hLib, "Py_Main");
	if (PyMain != nullptr)
	{
		OutDbgStrW(L"call Py_Main");
		exitCode = PyMain(argc, wargv);
	}
	else
	{
		OutDbgStrW(L"GetProcAddress failed");
	}

	FreeLibrary(hLib);
	LocalFree(argv);
	free(wargv);
	return exitCode;
}
