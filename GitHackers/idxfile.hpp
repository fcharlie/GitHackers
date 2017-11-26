#ifndef IDXFILE_HPP
#define IDXFILE_HPP
#include "base.hpp"
#pragma once

namespace idx {
	template <typename IntegerT> struct object_base {
		static_assert(std::is_integral<IntegerT>::value, "only support integer");
		bool operator<(const object_base<IntegerT> &o) { return offset > o.offset; }
		IntegerT offset{ 0 };
		std::uint32_t index{ 0 };
	};
	typedef object_base<std::uint32_t> ObjectIndex;
	typedef object_base<std::uint64_t> ObjectIndexLarge;
	struct IndexHeader {
		std::uint32_t magic;
		std::uint32_t version;
	};
	class IdxAnalyzer {
	public:
		IdxAnalyzer(base::Wfs &wfs_) :wfs(wfs_) {}
		~IdxAnalyzer() {
			if (hIdx != INVALID_HANDLE_VALUE) {
				CloseHandle(hIdx);
			}
		}
		bool verify(std::wstring_view file) {
			if ((pkflen = base::Filesize(file)) == -1) {
				return false;
			}
			auto idf = std::wstring(file.substr(0, file.size() - sizeof("pack") + 1)).append(L"idx"); /// replace subffix
			hIdx = CreateFileW(idf.data(),
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
			if (!base::FileSeek(hIdx, (std::uint64_t)(4 * 255), FILE_BEGIN)) {
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
		bool review(std::uint64_t limit, std::uint64_t warn) {
			if (lasize > 0) {
				return reviewlarge(limit, warn);
			}
			return reviewsmall(limit, warn);
		}
	private:
		bool reviewsmall(std::uint64_t limit, std::uint64_t warn) {
			if (base::FileSeek(hIdx, 4 + 4 + norsize * (20 + 4) + 256 * 4, FILE_BEGIN)) {
				return false;
			}
			if (norsize * sizeof(ObjectIndex) > wfs.memlimit) {
				return false;
			}
			std::vector<ObjectIndex> objs(norsize);
			auto objsraw = objs.data();
			auto bufc = reinterpret_cast<char *>(objsraw);
			auto binteger = reinterpret_cast<std::uint32_t*>(
				bufc + (sizeof(ObjectIndex) - sizeof(std::uint32_t))*norsize);
			if (!base::Readimpl(hIdx, binteger, norsize)) {
				return false;
			}
			for (std::uint32_t i = 0; i < norsize; i++) {
				objsraw[i].offset = ntohl(binteger[i]);
				objsraw[i].index = i;
			}
			std::sort(objs.begin(), objs.end());
			auto pre = pkflen - 20;
			for (const auto &i : objs) {
				auto size = pre - i.offset;
				pre = i.offset;
				if (size > limit) {
					return false;
				}
				else if (size > warn) {
					if (wfs.files.size < wfs.limits) {
						base::FileInfo fileinfo;
						fileinfo.file = base::Sha1FromIndex(hIdx, i.offset);
						fileinfo.size = size;
						wfs.files.push_back(std::move(fileinfo));
					}
					wfs.counts++;
				}
			}
			return true;
		}
		bool reviewlarge(std::uint64_t limit, std::uint64_t warn) {
			if (base::FileSeek(hIdx, 4 + 4 + norsize * (20 + 4) + 256 * 4, FILE_BEGIN)) {
				return false;
			}
			if ((norsize * sizeof(ObjectIndexLarge) + sizeof(std::uint64_t)*lasize) > wfs.memlimit) {
				return false;
			}
			std::vector<ObjectIndexLarge> objs(norsize);
			auto objsraw = objs.data();
			auto bufc = reinterpret_cast<char *>(objsraw);
			/// 4*counts
			auto binteger = reinterpret_cast<std::uint32_t *>(
				bufc + (sizeof(ObjectIndexLarge) - sizeof(std::uint32_t))* norsize);
			if (!base::Readimpl(hIdx, binteger, norsize)) {
				return false;
			}
			std::vector<std::uint64_t> lnrv(lasize);
			if (!base::Readimpl(hIdx, lnrv.data(), lasize)) {
				return false;
			}
			for (uint32_t i = 0; i < norsize; i++) {
				/// DON'T  change the order of operations
				auto off = ntohl(binteger[i]);
				if (!(off & 0x80000000)) {
					objsraw[i].offset = off;
					objsraw[i].index = i;
					continue;
				}
				off = off & 0x7fffffff;
				if (off >= lasize) {
					return false;
				}
				objsraw[i].offset = bswap64(lnrv[off]);
				objsraw[i].index = i;
			}
			std::sort(objs.begin(), objs.end());
			auto pre = pkflen - 20;
			for (const auto &i : objs) {
				auto size = pre - i.offset;
				pre = i.offset;
				if (size > limit) {
					return false;
				}
				else if (size > warn) {
					if (wfs.files.size < wfs.limits) {
						base::FileInfo fileinfo;
						fileinfo.file = base::Sha1FromIndex(hIdx, i.offset);
						fileinfo.size = size;
						wfs.files.push_back(std::move(fileinfo));
					}
					wfs.counts++;
				}
			}
			return true;
		}
		std::wstring lasterror;
		HANDLE hIdx{ INVALID_HANDLE_VALUE };
		base::Wfs &wfs;
		LARGE_INTEGER idxsize;
		std::int64_t pkflen;
		std::uint32_t norsize;
		std::uint32_t lasize;
	};
}

#endif
