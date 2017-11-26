#ifndef GIT_WAZE_BASE_HPP
#define GIT_WAZE_BASE_HPP
#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <string_view>
#include <Windows.h>

#ifndef CHECKLIMIT_RETURN
#define CHECKLIMIT_RETURN 0
#endif

#define bswap32(x) _byteswap_ulong(x)
#define bswap64(x) _byteswap_uint64(x)


namespace base {
	enum StoreScale : uint64_t {
		Kilobyte = 1024UL, //
		Megabyte = (1UL << 20),
		Gigabyte = (1ULL << 30),
		Trillionbyte = (1ULL << 40),
		Petabyte = (1ULL << 50),
		Exabyte = (1ULL << 60)
	};

	struct FileInfo {
		std::wstring file;
		std::uint64_t size;
	};
	struct Wfs {
		enum {
			MaxNumberOfDetails = 7
		};
		std::vector<FileInfo> files;
		std::size_t counts{ 0 };
		std::size_t limits{ MaxNumberOfDetails };
		std::size_t memlimit{ Megabyte * 256 };
	};

	template<typename IntegerT>
	bool FileSeek(HANDLE hFile, IntegerT offset, DWORD dwMoveMethod) {
		static_assert(std::is_integral<IntegerT>::value, "only support integer");
		LARGE_INTEGER li;
		li.QuadPart = offset;
		return SetFilePointerEx(hFile, li, nullptr, dwMoveMethod) == TRUE;
	}

	// support C style struct and integer(or array, pointer)
	template<typename BaseType>
	bool Readimpl(HANDLE hFile, BaseType *in, std::size_t num = 1) {
		static_assert(
			std::is_standard_layout<BaseType>::value || std::is_integral<BaseType>::value,
			"only support Interger or buffer");
		DWORD dwread = 0;
		if (::ReadFile(hFile, in, sizeof(BaseType)*num, &dwread, nullptr)
			&& dwread == sizeof(BaseType)*num) {
			return true;
		}
		return false;
	}
	inline HANDLE Openreadonly(std::wstring_view path) {
		auto hFile = CreateFileW(path.data(),
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);
		return hFile;
	}

	inline std::int64_t Filesize(std::wstring_view path) {
		auto hFile = Openreadonly(path);
		if (hFile == INVALID_HANDLE_VALUE) {
			return -1;
		}
		LARGE_INTEGER li;
		if (GetFileSizeEx(hFile, &li) != TRUE) {
			li.QuadPart = -1;
		}
		CloseHandle(hFile);
		return li.QuadPart;
	}

	inline std::shared_ptr<wchar_t > SystemErrorZerocopy() {
		LPWSTR pszbuf = nullptr;
		auto dwret = FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
			(LPWSTR)&pszbuf, 0, nullptr);
		if (dwret == 0) {
			return nullptr;
		}
		return std::shared_ptr<wchar_t>(pszbuf, [](wchar_t *ptr) {
			::LocalFree(ptr);
		});
	}

	inline std::wstring SystemError() {
		LPWSTR pszbuf = nullptr;
		auto dwret = FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
			(LPWSTR)&pszbuf, 0, nullptr);
		if (dwret == 0) {
			return L"unkown";
		}
		std::wstring msg;
		msg.assign(pszbuf, dwret);
		LocalFree(pszbuf);
		return msg;
	}
	inline std::wstring Sha1FromIndex(HANDLE hFile, std::uint32_t i) {
		if (SetFilePointer(hFile, i, nullptr, FILE_BEGIN) != 0) {
			return L"invaild";
		}
		char sha1[20];
		DWORD dwRead = 0;
		if (!::ReadFile(hFile, sha1, 20, &dwRead, nullptr)) {
			return L"invaild";
		}
		static const wchar_t hex[] = L"0123456789abcdef";
		std::wstring ws;
		ws.reserve(48);
		for (int i = 0; i < 20; i++) {
			unsigned int val = sha1[i];
			ws.push_back(hex[val >> 4]);
			ws.push_back(hex[val & 0xf]);
		}
		return ws;
	}
	inline const wchar_t *Sha1FromIndex(HANDLE hFile, wchar_t *buffer, std::uint32_t i) {
		if (SetFilePointer(hFile, i, nullptr, FILE_BEGIN) != 0) {
			return L"invaild";
		}
		char sha1[20];
		DWORD dwRead = 0;
		if (!ReadFile(hFile, sha1, 20, &dwRead, nullptr)) {
			return L"invaild";
		}
		static const wchar_t hex[] = L"0123456789abcdef";
		wchar_t *buf = buffer;
		for (int i = 0; i < 20; i++) {
			unsigned int val = sha1[i];
			*buf++ = hex[val >> 4];
			*buf++ = hex[val & 0xf];
		}
		*buf = '\0';
		return buffer;
	}


}

#endif
