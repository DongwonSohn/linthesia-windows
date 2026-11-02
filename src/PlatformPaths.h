#ifndef LINTHESIA_PLATFORM_PATHS_H
#define LINTHESIA_PLATFORM_PATHS_H

#include <string>
#include <vector>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// The application uses UTF-8 internally and Unicode Windows filesystem APIs.
inline std::wstring LinthesiaNativePath(const std::string& path) {
  if (path.empty())
    return std::wstring();
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                      path.data(), static_cast<int>(path.size()), nullptr, 0);
  if (!size)
    throw std::runtime_error("Invalid UTF-8 file path.");
  std::wstring result(size, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
                      static_cast<int>(path.size()), &result[0], size);
  return result;
}

inline std::string LinthesiaUtf8(const std::wstring& path) {
  if (path.empty())
    return std::string();
  const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                      path.data(), static_cast<int>(path.size()),
                                      nullptr, 0, nullptr, nullptr);
  if (!size)
    throw std::runtime_error("Invalid Unicode file path.");
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(),
                      static_cast<int>(path.size()), &result[0], size, nullptr, nullptr);
  return result;
}

inline std::string LinthesiaExecutableDirectory() {
  std::vector<wchar_t> path(512);
  for (;;) {
    const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size)
      throw std::runtime_error("Cannot locate the Linthesia executable.");
    if (size < path.size()) {
      const std::string filename = LinthesiaUtf8(std::wstring(path.data(), size));
      return filename.substr(0, filename.find_last_of("\\/"));
    }
    path.resize(path.size() * 2);
  }
}

inline std::string LinthesiaResourceDirectory() {
  return LinthesiaExecutableDirectory();
}

inline bool LinthesiaIsDirectory(const std::string& path) {
  const DWORD attributes = GetFileAttributesW(LinthesiaNativePath(path).c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

inline bool LinthesiaIsFile(const std::string& path) {
  const DWORD attributes = GetFileAttributesW(LinthesiaNativePath(path).c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}
#else
#include <sys/stat.h>

inline const std::string& LinthesiaNativePath(const std::string& path) {
  return path;
}

inline bool LinthesiaIsDirectory(const std::string& path) {
  struct stat status;
  return stat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode);
}

inline bool LinthesiaIsFile(const std::string& path) {
  struct stat status;
  return stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode);
}
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>

// Keep the app movable: resources live beside Contents/MacOS in the bundle.
inline std::string LinthesiaExecutableDirectory() {
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> executable(size);
  if (_NSGetExecutablePath(executable.data(), &size) != 0)
    return ".";
  char resolved[PATH_MAX];
  std::string path = realpath(executable.data(), resolved)
      ? resolved : executable.data();
  return path.substr(0, path.find_last_of('/'));
}

inline std::string LinthesiaResourceDirectory() {
  std::string path = LinthesiaExecutableDirectory();
  const std::string suffix = "/Contents/MacOS";
  if (path.size() >= suffix.size() &&
      path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0)
    return path.substr(0, path.size() - 5) + "Resources";
  return path;
}

#endif

#if defined(__APPLE__) || defined(_WIN32)
#undef GRAPHDIR
#undef MUSICDIR
#define GRAPHDIR (LinthesiaResourceDirectory() + "/graphics")
#define MUSICDIR (LinthesiaResourceDirectory() + "/music")
#endif

#endif
