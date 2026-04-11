#include "pch.h"
#include "FileSys.h"

byte_buffer_t BinaryFile::ReadFile(const std::wstring& _fileName)
{
  byte_buffer_t data;

	std::wstring fullName = FileSys::GetHomeDirectory() + _fileName;
  ScopedHandle hFile(safe_handle(CreateFile2(fullName.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
  if (!hFile)
    return {};

  // Get the file size.
  FILE_STANDARD_INFO fileInfo;
  if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    return {};

  // File is too big for 32-bit allocation, so reject read.
  if (fileInfo.EndOfFile.HighPart > 0)
    return {};

  // Create enough space for the file data.
  data.resize(fileInfo.EndOfFile.LowPart);

  // Read the data in.
  DWORD bytesRead = 0;

  if (!::ReadFile(hFile.get(), data.data(), fileInfo.EndOfFile.LowPart, &bytesRead, nullptr))
    return {};

  if (bytesRead < fileInfo.EndOfFile.LowPart)
    return {};

  return data;
}

bool BinaryFile::WriteFile(const std::wstring& _fileName, const byte_buffer_t& _data)
{
  std::wstring fullName = FileSys::GetHomeDirectory() + _fileName;
  ScopedHandle hFile(safe_handle(CreateFile2(fullName.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr)));
  if (!hFile)
    return false;

  DWORD bytesWritten = 0;
  if (!::WriteFile(hFile.get(), _data.data(), static_cast<DWORD>(_data.size()), &bytesWritten, nullptr))
    return false;

  return bytesWritten == static_cast<DWORD>(_data.size());
}

std::wstring TextFile::ReadFile(const std::wstring& _fileName)
{
  std::wstring data;

  ScopedHandle hFile(safe_handle(CreateFile2(_fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
  if (!hFile)
    return {};

  // Get the file size.
  FILE_STANDARD_INFO fileInfo;
  if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    return {};

  // File is too big for 32-bit allocation, so reject read.
  if (fileInfo.EndOfFile.HighPart > 0)
    return {};

  // Create enough space for the file data.
  data.resize(fileInfo.EndOfFile.LowPart);

  // Read the data in.
  DWORD bytesRead = 0;

  if (!::ReadFile(hFile.get(), data.data(), fileInfo.EndOfFile.LowPart, &bytesRead, nullptr))
    return {};

  if (bytesRead < fileInfo.EndOfFile.LowPart)
    return {};

  return data;
}
