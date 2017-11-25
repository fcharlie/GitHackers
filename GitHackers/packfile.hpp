#ifndef PACKFILE_HPP
#define PACKFILE_HPP
#include <string_view>
#include "base.hpp"
#pragma once
namespace pack {
	struct IndexHeader {
		std::uint32_t magic;
		std::uint32_t version;
	};
	struct FileIndex {
		std::uint64_t size;
		std::uint32_t index;
	};
	class PackAnalyzer {
	public:
		PackAnalyzer(base::Wfs&wfs_):wfs(wfs_){}
		~PackAnalyzer() {
			if (hIdx != INVALID_HANDLE_VALUE) {
				CloseHandle(hIdx);
			}
			if (hPk != INVALID_HANDLE_VALUE) {
				CloseHandle(hPk);
			}
		}
		const auto &LastError()const {
			return lasterror;
		}
		bool resolve(std::wstring_view file) {
			hPk = CreateFileW(file.data(), 
				GENERIC_READ, 
				FILE_SHARE_READ, 
				nullptr, 
				OPEN_EXISTING, 
				FILE_ATTRIBUTE_NORMAL, 
				nullptr);
			if (hPk == INVALID_HANDLE_VALUE) {
				lasterror.assign(L"open packfile: ").append(base::SystemError());
				return false;
			}
			auto idf = std::wstring(file.substr(0, file.size() - sizeof("pack") + 1)).append(L"idx"); /// replace subffix
			hIdx= CreateFileW(idf.data(),
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr);
			if (hIdx == INVALID_HANDLE_VALUE) {
				lasterror.assign(L"open idxfile: ").append(base::SystemError());
				return false;
			}
			if (!GetFileSizeEx(hIdx, &idxsize)) {
				lasterror.assign(L"get idxfile size: ").append(base::SystemError());
				return false;
			}
			IndexHeader idh;
			if (!base::Readimpl(hIdx, &idh)) {
				return false;
			}
			/// we known Windows ARM is LE
			/// https://blogs.msdn.microsoft.com/larryosterman/2005/06/07/the-endian-of-windows/
			/// https://msdn.microsoft.com/en-us/library/dn736986.aspx
			/// iOS is LE
			if (idh.version != bswap32(2)) {
				return false;
			}
			if (!base::FileSeek(hIdx,(std::uint64_t)(4 * 255), FILE_BEGIN)) {
				return false;
			}
			std::uint32_t nr;
			if (!base::Readimpl(hIdx, &nr)) {
				return false;
			}
			norsize = bswap32(nr);
			lasize = static_cast<std::uint32_t>((idxsize.QuadPart - norsize * (20 + 4 + 4) - 4 * 2 - 4 * 256 - 2 * 20) / 8);
			return true;
		}
		bool review(std::uint64_t limitsize, std::uint64_t warnsize) {
			std::vector<std::uint64_t> lnrv;
			if (lasize > 0) {
				if (lasize * sizeof(std::uint64_t) > wfs.memlimit) {
					lasterror.assign(L"Large number counts more than memory limit: %ld", wfs.memlimit);
					return false;
				}
				lnrv.resize(lasize);
				if (!base::FileSeek(hIdx, 4 + 4 + 256 * 4 + norsize * (20 + 4 + 4), FILE_BEGIN)) {
					return false;
				}
				if (!base::Readimpl(hIdx, lnrv.data(), lasize)) {
					return false;
				}
			}
			if (!base::FileSeek(hIdx, 4 + 4 + 256 * 4 + norsize * (20 + 4), FILE_BEGIN)) {
				return false;
			}
			std::vector<FileIndex> windex;
			windex.reserve(4);
			for (std::uint32_t i = 0; i < norsize; i++) {
				std::uint32_t in;
				std::uint64_t offset{ 0 };
				if (!base::Readimpl(hIdx, &in)) {
					return false;
				}
				auto off = bswap32(in);
				if (!(off & 0x80000000)) {
					offset = off;
				}
				else {
					off = off * 0x7fffffff;
					if (off >= lasize) {
						return false;
					}
					offset = bswap64(lnrv[off]);
				}
				auto sz = ObjectSize(offset);
				if (sz > limitsize) {
					/*
					    console::Printeln("File: %s size %4.2f MB, more than %4.2f MB",
                        Sha1FromIndex(fdx, buf, i), (float)sz / scale::Megabyte,
                        (float)limitsize / scale::Megabyte);
					*/
					return false;
				}
				else if (sz > warnsize) {
					FileIndex fi;
					fi.index = i;
					fi.size = sz;
					windex.push_back(fi);
				}
			}
			wfs.counts += windex.size();
			for (auto &wi : windex) {
				if (wfs.files.size() >= wfs.limits) {
					break;
				}
				base::FileInfo fileinfo;
				fileinfo.file = base::Sha1FromIndex(hIdx, wi.index);
				fileinfo.size = wi.size;
				wfs.files.push_back(std::move(fileinfo));
			}
			return true;
		}
	private:
		std::uint64_t ObjectSize(std::uint64_t offset) {
			const constexpr uint8_t firstLengthBites = 4;
			const constexpr uint8_t lengthBits = 7;
			const constexpr int maskFirstLength = 15;
			const constexpr int maskContinue = 0x80;
			const constexpr uint8_t maskType = 112;
			const constexpr uint8_t maskLength = 127;
			if (!base::FileSeek(hPk, offset, FILE_BEGIN)) {
				return UINT64_MAX;
			}
			unsigned char b;
			DWORD dwRead = 0;
			if (ReadFile(hPk, &b, 1, &dwRead, nullptr) != TRUE) {
				return UINT64_MAX;
			}
			// https://github.com/src-d/go-git/blob/master/plumbing/format/packfile/scanner.go#L243
			auto t = ((b & maskType) >> firstLengthBites); // type
			auto length = (int64_t)(b & maskFirstLength);
			auto shift = firstLengthBites;
			while ((b & maskContinue) > 0) {
				if (ReadFile(hPk, &b, 1, &dwRead, nullptr) != TRUE) {
					return UINT64_MAX;
				}
				length += (int64_t)(b & maskLength) << shift;
				shift += lengthBits;
			}
			return length;
			return UINT64_MAX;
		}
		HANDLE hIdx{ INVALID_HANDLE_VALUE };
		HANDLE hPk{ INVALID_HANDLE_VALUE };
		std::wstring lasterror;
		base::Wfs &wfs;
		LARGE_INTEGER idxsize;
		std::uint32_t norsize;
		std::uint32_t lasize;
	};
}

#endif
