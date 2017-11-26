// GitHackers.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "base.hpp"
#include <vector>
#include <algorithm>
#include "idxfile.hpp"
#include "packfile.hpp"

bool packresolve(const wchar_t *file) {
	base::Wfs wfs;
	pack::PackAnalyzer pa(wfs);
	if (!pa.resolve(file)) {
		return false;
	}
	return true;
}

int wmain(int argc, wchar_t **argv)
{
	std::vector<idx::ObjectIndex> v;
	std::sort(v.begin(), v.end());
	if (std::is_standard_layout<idx::ObjectIndex>::value) {
		fprintf(stderr, "is standard layout");
	}
	return 0;
}

