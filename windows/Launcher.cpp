// Native launcher for the portable Windows layout. All application and runtime
// DLLs live in dll/; this executable needs only Windows system DLLs.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace {

[[noreturn]] void Fail(const std::wstring& description, DWORD error) {
  wchar_t* text = nullptr;
  const DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
  std::wstring message = description + L"\n\nWindows error " +
                         std::to_wstring(error);
  if (length)
    message += L": " + std::wstring(text, length);
  if (text)
    LocalFree(text);
  throw message;
}

std::wstring ExecutableDirectory() {
  std::vector<wchar_t> buffer(512);
  for (;;) {
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length)
      Fail(L"Cannot locate the Linthesia launcher.", GetLastError());
    if (length < buffer.size()) {
      const std::wstring launcher(buffer.data(), length);
      const size_t separator = launcher.find_last_of(L"\\/");
      if (separator == std::wstring::npos)
        throw std::wstring(L"Cannot locate the Linthesia application folder.");
      return launcher.substr(0, separator);
    }
    if (buffer.size() >= 32768)
      throw std::wstring(L"The Linthesia launcher path is too long.");
    buffer.resize(buffer.size() * 2);
  }
}

// Convert once from Windows' Unicode command line. Mutable, null-terminated
// buffers stay owned by the launcher while the application borrows argv.
class Arguments {
 public:
  Arguments() {
    int count = 0;
    wchar_t** wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!wide)
      Fail(L"Cannot read the Linthesia command line.", GetLastError());
    struct WideArguments {
      wchar_t** values;
      ~WideArguments() { LocalFree(values); }
    } allocated = {wide};

    buffers_.reserve(static_cast<size_t>(count));
    pointers_.reserve(static_cast<size_t>(count) + 1);
    for (int index = 0; index < count; ++index) {
      const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
          wide[index], -1, nullptr, 0, nullptr, nullptr);
      if (!size)
        Fail(L"Cannot convert a Linthesia command-line argument to UTF-8.",
             GetLastError());
      buffers_.emplace_back(static_cast<size_t>(size));
      if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[index], -1,
              buffers_.back().data(), size, nullptr, nullptr))
        Fail(L"Cannot convert a Linthesia command-line argument to UTF-8.",
             GetLastError());
      pointers_.push_back(buffers_.back().data());
    }
    pointers_.push_back(nullptr);
  }

  int count() const { return static_cast<int>(buffers_.size()); }
  char** values() { return pointers_.data(); }

 private:
  std::vector<std::vector<char>> buffers_;
  std::vector<char*> pointers_;
};

int Launch() {
  const std::wstring dll_directory = ExecutableDirectory() + L"\\dll";
  const std::wstring application = dll_directory + L"\\LinthesiaApp.dll";
  const DWORD attributes = GetFileAttributesW(application.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES)
    Fail(L"Cannot find dll\\LinthesiaApp.dll. Extract the entire ZIP and keep "
         L"the dll folder beside Linthesia.exe.", GetLastError());
  if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    throw std::wstring(L"dll\\LinthesiaApp.dll must be a file. Extract the "
                       L"entire Linthesia ZIP again.");

  Arguments arguments;
  // Also cover libraries SDL may load later. Avoid looking in the caller's
  // working directory or relying on a developer installation on PATH.
  if (!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 |
                               LOAD_LIBRARY_SEARCH_USER_DIRS))
    Fail(L"Cannot prepare the Linthesia DLL search path.", GetLastError());
  const DLL_DIRECTORY_COOKIE directory_cookie =
      AddDllDirectory(dll_directory.c_str());
  if (!directory_cookie)
    Fail(L"Cannot use the Linthesia dll folder.", GetLastError());

  const HMODULE module = LoadLibraryExW(application.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!module)
    Fail(L"Could not load Linthesia. Keep all supplied DLL files in the dll "
         L"folder beside Linthesia.exe.", GetLastError());
  const FARPROC entry = GetProcAddress(module, "LinthesiaRun");
  if (!entry)
    Fail(L"The Linthesia application DLL is incompatible. Extract the entire "
         L"ZIP again.", GetLastError());

  // In C++14 noexcept is not part of the function pointer type. The exported
  // implementation still guarantees that no exceptions cross this boundary.
  typedef int (WINAPI *RunApplication)(int, char**);
  const RunApplication run = reinterpret_cast<RunApplication>(entry);
  // Keep the DLL and its search directory registered until process exit. An
  // application error can leave background threads alive until Windows exits.
  // CWD, environment, and redirected standard handles are unchanged.
  return run(arguments.count(), arguments.values());
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  try {
    return Launch();
  } catch (const std::wstring& error) {
    MessageBoxW(nullptr, error.c_str(), L"Linthesia", MB_OK | MB_ICONERROR);
  } catch (...) {
    MessageBoxW(nullptr, L"Could not start Linthesia. Extract the entire ZIP "
                         L"and try again.",
                L"Linthesia", MB_OK | MB_ICONERROR);
  }
  return 1;
}
