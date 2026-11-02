// Native launcher for the portable Windows layout. The application and all of
// its DLLs/resources live in app/; this executable needs only Windows system DLLs.
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

class Handle {
 public:
  explicit Handle(HANDLE value) : value_(value) {}
  ~Handle() {
    if (value_ && value_ != INVALID_HANDLE_VALUE)
      CloseHandle(value_);
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  HANDLE get() const { return value_; }

 private:
  HANDLE value_;
};

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

std::wstring ApplicationPath() {
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
      return launcher.substr(0, separator) + L"\\app\\Linthesia.exe";
    }
    if (buffer.size() >= 32768)
      throw std::wstring(L"The Linthesia launcher path is too long.");
    buffer.resize(buffer.size() * 2);
  }
}

// Quote one argument using the Windows backslash/quotation-mark rules. This
// preserves empty arguments, embedded quotes and trailing path separators.
std::wstring QuoteArgument(const std::wstring& argument) {
  std::wstring quoted(1, L'"');
  size_t backslashes = 0;
  for (wchar_t character : argument) {
    if (character == L'\\') {
      ++backslashes;
    } else {
      quoted.append(character == L'"' ? backslashes * 2 + 1 : backslashes,
                    L'\\');
      quoted += character;
      backslashes = 0;
    }
  }
  quoted.append(backslashes * 2, L'\\');
  quoted += L'"';
  return quoted;
}

std::wstring ChildCommandLine(const std::wstring& application) {
  int count = 0;
  wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
  if (!arguments)
    Fail(L"Cannot read the Linthesia command line.", GetLastError());
  struct Arguments {
    wchar_t** values;
    ~Arguments() { LocalFree(values); }
  } allocated = {arguments};

  std::wstring command = QuoteArgument(application);
  for (int index = 1; index < count; ++index)
    command += L" " + QuoteArgument(arguments[index]);
  return command;
}

// Preserve redirected CLI output for --help/--version and automated checks.
// A normal Explorer launch has no standard handles, so use NUL in that case.
HANDLE InheritableStandardHandle(DWORD kind, DWORD access) {
  const HANDLE original = GetStdHandle(kind);
  if (original && original != INVALID_HANDLE_VALUE) {
    HANDLE duplicate = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), original, GetCurrentProcess(),
                        &duplicate, 0, TRUE, DUPLICATE_SAME_ACCESS))
      return duplicate;
    // Shell launches can supply non-stream values, such as a monitor handle.
    // Treat an unusable standard handle like an absent one.
  }
  SECURITY_ATTRIBUTES security = {sizeof(security), nullptr, TRUE};
  const HANDLE fallback = CreateFileW(
      L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (fallback == INVALID_HANDLE_VALUE)
    Fail(L"Cannot prepare standard input/output for Linthesia.", GetLastError());
  return fallback;
}

int Launch() {
  const std::wstring application = ApplicationPath();
  const DWORD attributes = GetFileAttributesW(application.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES)
    Fail(L"Cannot find app\\Linthesia.exe. Extract the entire ZIP and keep the "
         L"app folder beside Linthesia.exe.", GetLastError());
  if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    throw std::wstring(L"app\\Linthesia.exe must be a file. Extract the entire "
                       L"Linthesia ZIP again.");

  std::wstring command = ChildCommandLine(application);
  Handle input(InheritableStandardHandle(STD_INPUT_HANDLE, GENERIC_READ));
  Handle output(InheritableStandardHandle(STD_OUTPUT_HANDLE, GENERIC_WRITE));
  Handle error(InheritableStandardHandle(STD_ERROR_HANDLE, GENERIC_WRITE));
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = input.get();
  startup.hStdOutput = output.get();
  startup.hStdError = error.get();
  PROCESS_INFORMATION child = {};

  // Keep the caller's working directory and environment. Relative MIDI paths
  // continue to refer to the caller's folder; Windows finds DLLs beside the child.
  if (!CreateProcessW(application.c_str(), &command[0], nullptr, nullptr, TRUE,
                      0, nullptr, nullptr, &startup, &child))
    Fail(L"Could not start Linthesia. Keep the complete app folder beside "
         L"Linthesia.exe.", GetLastError());
  Handle process(child.hProcess);
  Handle thread(child.hThread);
  if (WaitForSingleObject(process.get(), INFINITE) != WAIT_OBJECT_0)
    Fail(L"Could not wait for Linthesia to exit.", GetLastError());
  DWORD exit_code = 1;
  if (!GetExitCodeProcess(process.get(), &exit_code))
    Fail(L"Could not read the Linthesia exit status.", GetLastError());
  return static_cast<int>(exit_code);
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
