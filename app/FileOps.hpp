// File operations without any UI (MIT). Rules shared by all of them:
//  - nothing is overwritten silently: a name clash gives "name (1).ext";
//  - a folder is never copied or moved into itself;
//  - symbolic links are copied as links and never followed when deleting or measuring.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <utility>

namespace colibry::files {

struct OpResult {
	bool ok = false;
	std::filesystem::path path;  // the resulting location on success
	std::string error;
	static OpResult success(std::filesystem::path p = {}) { return {true, std::move(p), {}}; }
	static OpResult failure(std::string e) { return {false, {}, std::move(e)}; }
};

struct Entry {
	std::string name;
	std::filesystem::path path;
	bool isDir = false;   // folders and links to folders
	bool isLink = false;
	std::uintmax_t size = 0;
	std::filesystem::file_time_type modified;
};

namespace fileops {

// `name` if free inside `dir`, otherwise "name (1).ext", "name (2).ext", ...
std::string uniqueName(const std::filesystem::path& dir, const std::string& name);
bool isValidName(const std::string& name, std::string* reason = nullptr);

OpResult copy(const std::filesystem::path& source, const std::filesystem::path& targetDir);
OpResult move(const std::filesystem::path& source, const std::filesystem::path& targetDir);
OpResult rename(const std::filesystem::path& path, const std::string& newName);
// Permanent. Refuses the filesystem root and empty paths.
OpResult remove(const std::filesystem::path& path);
OpResult createFolder(const std::filesystem::path& dir, const std::string& name);
OpResult createFile(const std::filesystem::path& dir, const std::string& name);

// Size of a file, or of everything inside a folder (links are not followed and count as 0).
std::uintmax_t totalSize(const std::filesystem::path& path);
// "1.5 MB", "832 bytes", ...
std::string humanSize(std::uintmax_t bytes);

// Folders first, then names without regard to case. Hidden (dot) files only when asked.
std::vector<Entry> list(const std::filesystem::path& dir, bool showHidden, std::string* error = nullptr);

// True when `inner` is `outer` itself or lies inside it (after resolving "." and "..").
bool isInside(const std::filesystem::path& inner, const std::filesystem::path& outer);

}  // namespace fileops
}  // namespace colibry::files
