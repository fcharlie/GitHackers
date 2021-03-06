// git-waze.cpp: 定义控制台应用程序的入口点。
//
#include "stdafx.h"
#include "base.hpp"
#include <vector>
#include <algorithm>
#include <filesystem>
#include "idxfile.hpp"
#include "packfile.hpp"

/// We known, only VC20 support std::filesystem
#if _MSC_VER<2000
namespace std {
	namespace filesystem = std::experimental::filesystem;
}
#endif

bool packresolve(std::wstring_view file) {
	base::Wfs wfs;
	pack::PackAnalyzer pa(wfs);
	if (!pa.resolve(file)) {
		return false;
	}
	return true;
}



int RepositoryLoop(std::wstring_view dir) {
	std::wstring objdir = std::wstring(dir).append(L"\\objects");
	std::filesystem::path objpath(objdir);
	if (!std::filesystem::exists(objpath)) {
		console::Printeln(L"Repository: %s not found dir", dir);
		return 1;
	}
	for (auto &p : std::filesystem::recursive_directory_iterator(objpath)) {
		if (p.path().extension().compare(L"pack") == 0) {
			auto r = packresolve(p.path().wstring());
#if CHECKLIMIT_RETURN
			if (!r) {
				return -1;
			}
#else
			(void)r;
#endif
			continue;
		}
		/// Skip all idx file
		if (p.path().extension().compare(L"idx") == 0) {
			continue;
		}
	}
	return 0;
}


int wmain(int argc, wchar_t **argv)
{
	if (argc < 2) {
		console::Printeln(L"usage: %s gitdir ...", argv[0]);
		return 1;
	}
	for (auto i = 1; i < argc; i++) {
		RepositoryLoop(argv[i]);
	}
	return 0;
}

