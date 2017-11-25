#ifndef IDXFILE_HPP
#define IDXFILE_HPP
#include "base.hpp"
#pragma once

namespace idx{
	template <typename IntegerT> struct object_base {
		static_assert(std::is_integral<IntegerT>::value, "only support integer");
		bool operator<(const object_base<IntegerT> &o) { return offset > o.offset; }
		IntegerT offset{ 0 };
		std::uint32_t index{ 0 };
	};
	typedef object_base<std::uint32_t> ObjectIndex;
	typedef object_base<std::uint64_t> ObjectIndexLarge;

	class IdxAnalyzer {
	public:
	private:
		HANDLE hIdx{ nullptr };
	};
}

#endif
