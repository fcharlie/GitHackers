#ifndef GIT_HACKERS_BASE_HPP
#define GIT_HACKERS_BASE_HPP
#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <Windows.h>
//#pragma comment(lib,"Ws2_32")

namespace base {
	enum StoreScale : uint64_t {
		Kilobyte = 1024UL, //
		Megabyte = (1UL << 20),
		Gigabyte = (1ULL << 30),
		Trillionbyte = (1ULL << 40),
		Petabyte = (1ULL << 50),
		Exabyte = (1ULL << 60)
	};
#define bswap32(x) _byteswap_ulong(x)
#define bswap64(x) _byteswap_uint64(x)
	template<typename IntegerT>
	bool FileSeek(HANDLE hFile, IntegerT offset, DWORD dwMoveMethod) {
		static_assert(std::is_integral<IntegerT>::value, "only support integer");
		LARGE_INTEGER li;
		li.QuadPart = offset;
		return SetFilePointerEx(hFile, li, nullptr, dwMoveMethod) == TRUE;
	}

	template<typename IntegerT>
	bool Readimpl(HANDLE hFile, IntegerT *in, std::size_t num=1) {
		static_assert(
			std::is_standard_layout<IntegerT>::value||std::is_integral<IntegerT>::value,
			"only support Interger buffer");
		DWORD dwread = 0;
		if (::ReadFile(hFile, in, sizeof(IntegerT)*num, &dwread, nullptr)
			&& dwread == sizeof(IntegerT)*num) {
			return true;
		}
		return false;
	}

	std::shared_ptr<wchar_t > SystemErrorZerocopy() {
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

}

#endif
