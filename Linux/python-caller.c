#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#define DEBUG_STR 1

#if DEBUG_STR
#define DbgPrintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define DbgPrintf(...)
#endif

typedef void (*Py_SetPythonHome_t)(const wchar_t*);
typedef void (*Py_SetProgramName_t)(const wchar_t*);
typedef int (*Py_BytesMain_t)(int argc, char** argv);

static void trim_trailing_newline(char* line)
{
	size_t len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
	{
		line[--len] = '\0';
	}
}

static int read_conf_line(FILE* fp, char* line, size_t line_size)
{
	if (fgets(line, (int)line_size, fp) == NULL)
	{
		line[0] = '\0';
		return 0;
	}

	trim_trailing_newline(line);
	return line[0] != '\0';
}

static int combine_path(char* dest, size_t dest_size, const char* base_dir, const char* relative_path)
{
	if (relative_path[0] == '\0')
	{
		return 0;
	}

	if (relative_path[0] == '/')
	{
		strncpy(dest, relative_path, dest_size);
		dest[dest_size - 1] = '\0';
		return 1;
	}

	size_t base_len = strlen(base_dir);
	int needs_sep = base_len > 0 && base_dir[base_len - 1] != '/';
	int written = snprintf(dest, dest_size, needs_sep ? "%s/%s" : "%s%s", base_dir, relative_path);
	return written > 0 && (size_t)written < dest_size;
}

static int get_exe_path(char* exe_path, size_t exe_path_size)
{
	ssize_t len = readlink("/proc/self/exe", exe_path, exe_path_size - 1);
	if (len <= 0)
	{
		return 0;
	}

	exe_path[len] = '\0';
	return 1;
}

static int get_dirname(const char* path, char* dir, size_t dir_size)
{
	strncpy(dir, path, dir_size);
	dir[dir_size - 1] = '\0';

	char* slash = strrchr(dir, '/');
	if (slash == NULL)
	{
		return 0;
	}

	if (slash == dir)
	{
		dir[1] = '\0';
	}
	else
	{
		*slash = '\0';
	}
	return 1;
}

static wchar_t* to_wide(const char* text)
{
	if (text == NULL)
	{
		return NULL;
	}

	size_t len = mbstowcs(NULL, text, 0);
	if (len == (size_t)-1)
	{
		return NULL;
	}

	wchar_t* wide = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if (wide == NULL)
	{
		return NULL;
	}

	if (mbstowcs(wide, text, len + 1) == (size_t)-1)
	{
		free(wide);
		return NULL;
	}

	return wide;
}

static int get_python_home(const char* work_dir, char* python_home, size_t python_home_size)
{
	const char* lib_suffix = "/lib";
	size_t work_len = strlen(work_dir);

	if (work_len >= PATH_MAX)
	{
		return 0;
	}

	if (work_len >= 4 && strcmp(work_dir + work_len - 4, lib_suffix) == 0)
	{
		return get_dirname(work_dir, python_home, python_home_size);
	}

	strncpy(python_home, work_dir, python_home_size);
	python_home[python_home_size - 1] = '\0';
	return 1;
}

int main(int argc, char** argv)
{
	char exe_path[PATH_MAX];
	char exe_dir[PATH_MAX];
	char conf_path[PATH_MAX];
	char work_dir[PATH_MAX];
	char script_path[PATH_MAX];
	char python_home[PATH_MAX];
	char python_so_path[PATH_MAX];

	if (!get_exe_path(exe_path, sizeof(exe_path)))
	{
		DbgPrintf("readlink /proc/self/exe failed: %s\n", strerror(errno));
		return 1;
	}
	DbgPrintf("exe_path=%s\n", exe_path);

	if (!get_dirname(exe_path, exe_dir, sizeof(exe_dir)))
	{
		DbgPrintf("exe path has no directory\n");
		return 1;
	}

	if (snprintf(conf_path, sizeof(conf_path), "%s.conf", exe_path) >= (int)sizeof(conf_path))
	{
		DbgPrintf("conf path too long\n");
		return 1;
	}

	FILE* fp = fopen(conf_path, "r");
	if (fp == NULL)
	{
		DbgPrintf("conf not found: %s\n", conf_path);
		return 1;
	}

	char work_dir_relative[PATH_MAX];
	char script_relative[PATH_MAX];
	if (!read_conf_line(fp, work_dir_relative, sizeof(work_dir_relative)) ||
		!read_conf_line(fp, script_relative, sizeof(script_relative)))
	{
		fclose(fp);
		DbgPrintf("conf invalid: %s\n", conf_path);
		return 1;
	}
	fclose(fp);

	if (!combine_path(work_dir, sizeof(work_dir), exe_dir, work_dir_relative) ||
		!combine_path(script_path, sizeof(script_path), work_dir, script_relative) ||
		!combine_path(python_so_path, sizeof(python_so_path), work_dir, "libpython3.12.so") ||
		!get_python_home(work_dir, python_home, sizeof(python_home)))
	{
		DbgPrintf("path setup failed\n");
		return 1;
	}

	DbgPrintf("python_home=%s\n", python_home);
	DbgPrintf("work_dir=%s\n", work_dir);
	DbgPrintf("script_path=%s\n", script_path);
	DbgPrintf("python_so=%s\n", python_so_path);

	if (chdir(work_dir) != 0)
	{
		DbgPrintf("chdir failed: %s\n", strerror(errno));
		return 1;
	}

	char** py_argv = calloc((size_t)argc + 2, sizeof(char*));
	if (py_argv == NULL)
	{
		return 1;
	}

	py_argv[0] = exe_path;
	py_argv[1] = script_path;
	for (int i = 1; i < argc; ++i)
	{
		py_argv[i + 1] = argv[i];
	}
	int py_argc = argc + 1;

	void* handle = dlopen(python_so_path, RTLD_NOW | RTLD_GLOBAL);
	if (handle == NULL)
	{
		DbgPrintf("dlopen failed: %s\n", dlerror());
		free(py_argv);
		return 1;
	}

	int exit_code = 1;
	Py_SetPythonHome_t set_python_home = (Py_SetPythonHome_t)dlsym(handle, "Py_SetPythonHome");
	Py_SetProgramName_t set_program_name = (Py_SetProgramName_t)dlsym(handle, "Py_SetProgramName");
	Py_BytesMain_t py_bytes_main = (Py_BytesMain_t)dlsym(handle, "Py_BytesMain");

	wchar_t* wide_python_home = to_wide(python_home);
	wchar_t* wide_program_name = to_wide(exe_path);
	if (wide_python_home == NULL || wide_program_name == NULL)
	{
		DbgPrintf("path conversion failed\n");
	}
	else
	{
		if (set_python_home != NULL)
		{
			set_python_home(wide_python_home);
		}
		if (set_program_name != NULL)
		{
			set_program_name(wide_program_name);
		}

		if (py_bytes_main != NULL)
		{
			DbgPrintf("call Py_BytesMain\n");
			exit_code = py_bytes_main(py_argc, py_argv);
		}
		else
		{
			DbgPrintf("dlsym Py_BytesMain failed: %s\n", dlerror());
		}
	}

	free(wide_python_home);
	free(wide_program_name);
	dlclose(handle);
	free(py_argv);
	return exit_code;
}
