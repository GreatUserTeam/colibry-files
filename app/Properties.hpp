// Properties of a file or folder (MIT).
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace colibry::files {

struct Properties {
	bool ok = false;
	std::string error;
	std::string name;
	std::string location;
	std::string type;         // "Folder", "Symbolic link", "Text file", ...
	std::uintmax_t size = 0;  // bytes; folders: everything inside
	std::uintmax_t items = 0; // folders: number of files and folders inside (recursive)
	std::string modified;     // "2026-11-09 14:05"
	std::string permissions;  // "rwxr-xr-x"

	// The lines shown in the properties panel.
	std::string text() const;
};

Properties describe(const std::filesystem::path& path);
std::string permissionString(std::filesystem::perms p);

}  // namespace colibry::files
