#include "FileOps.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <system_error>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace colibry::files::fileops {
namespace {

std::string lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

bool present(const fs::path& p) {
	std::error_code ec;
	return fs::symlink_status(p, ec).type() != fs::file_type::not_found;
}

fs::path canonicalish(const fs::path& p) {
	std::error_code ec;
	fs::path c = fs::weakly_canonical(p, ec);
	return ec ? p.lexically_normal() : c;
}

}  // namespace

bool isInside(const fs::path& inner, const fs::path& outer) {
	const fs::path a = canonicalish(inner), b = canonicalish(outer);
	auto ai = a.begin(), bi = b.begin();
	for (; bi != b.end(); ++ai, ++bi) {
		if (bi->empty()) continue;  // trailing separator
		if (ai == a.end() || *ai != *bi) return false;
	}
	return true;
}

std::string uniqueName(const fs::path& dir, const std::string& name) {
	if (!present(dir / name)) return name;
	const fs::path p(name);
	std::string ext = p.extension().string();
	std::string stem = p.stem().string();
	if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, ".tar") == 0) {  // "x.tar.gz" is one extension
		ext = ".tar" + ext;
		stem.erase(stem.size() - 4);
	}
	for (int i = 1; i < 10000; ++i) {
		std::string candidate = stem + " (" + std::to_string(i) + ")" + ext;
		if (!present(dir / candidate)) return candidate;
	}
	return name;
}

bool isValidName(const std::string& name, std::string* reason) {
	auto fail = [&](const char* why) { if (reason) *reason = why; return false; };
	if (name.empty()) return fail("The name is empty.");
	if (name == "." || name == "..") return fail("This name is reserved.");
	if (name.find('/') != std::string::npos) return fail("A name cannot contain '/'.");
	if (name.find('\0') != std::string::npos) return fail("A name cannot contain a null character.");
	if (name.size() > 255) return fail("The name is too long.");
	return true;
}

OpResult copy(const fs::path& source, const fs::path& targetDir) {
	std::error_code ec;
	if (!present(source)) return OpResult::failure("The source does not exist.");
	if (!fs::is_directory(targetDir, ec)) return OpResult::failure("The target is not a folder.");
	if (fs::is_directory(fs::symlink_status(source, ec)) && isInside(targetDir, source))
		return OpResult::failure("A folder cannot be copied into itself.");
	const fs::path target = targetDir / uniqueName(targetDir, source.filename().string());
	fs::copy(source, target, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
	if (ec) return OpResult::failure("Cannot copy: " + ec.message());
	return OpResult::success(target);
}

OpResult move(const fs::path& source, const fs::path& targetDir) {
	std::error_code ec;
	if (!present(source)) return OpResult::failure("The source does not exist.");
	if (!fs::is_directory(targetDir, ec)) return OpResult::failure("The target is not a folder.");
	if (fs::is_directory(fs::symlink_status(source, ec)) && isInside(targetDir, source))
		return OpResult::failure("A folder cannot be moved into itself.");
	if (canonicalish(source.parent_path()) == canonicalish(targetDir))
		return OpResult::failure("The item is already in this folder.");
	const fs::path target = targetDir / uniqueName(targetDir, source.filename().string());
	fs::rename(source, target, ec);
	if (ec) {  // another disk: copy, then delete the original
		OpResult copied = fileops::copy(source, targetDir);
		if (!copied.ok) return copied;
		OpResult removed = fileops::remove(source);
		if (!removed.ok) return OpResult::failure("Copied, but the original could not be removed: " + removed.error);
		return copied;
	}
	return OpResult::success(target);
}

OpResult rename(const fs::path& path, const std::string& newName) {
	std::string reason;
	if (!isValidName(newName, &reason)) return OpResult::failure(reason);
	if (!present(path)) return OpResult::failure("The item does not exist.");
	const fs::path target = path.parent_path() / newName;
	if (present(target)) return OpResult::failure("An item with this name already exists.");
	std::error_code ec;
	fs::rename(path, target, ec);
	if (ec) return OpResult::failure("Cannot rename: " + ec.message());
	return OpResult::success(target);
}

OpResult remove(const fs::path& path) {
	if (path.empty() || path == path.root_path()) return OpResult::failure("Refusing to delete this path.");
	if (!present(path)) return OpResult::failure("The item does not exist.");
	std::error_code ec;
	fs::remove_all(path, ec);  // a link is removed, never followed
	if (ec) return OpResult::failure("Cannot delete: " + ec.message());
	return OpResult::success();
}

OpResult createFolder(const fs::path& dir, const std::string& name) {
	std::string reason;
	if (!isValidName(name, &reason)) return OpResult::failure(reason);
	const fs::path target = dir / uniqueName(dir, name);
	std::error_code ec;
	if (!fs::create_directory(target, ec) || ec) return OpResult::failure("Cannot create the folder: " + ec.message());
	return OpResult::success(target);
}

OpResult createFile(const fs::path& dir, const std::string& name) {
	std::string reason;
	if (!isValidName(name, &reason)) return OpResult::failure(reason);
	const fs::path target = dir / uniqueName(dir, name);
	std::FILE* f = std::fopen(target.c_str(), "wbx");
	if (!f) return OpResult::failure("Cannot create the file.");
	std::fclose(f);
	return OpResult::success(target);
}

std::uintmax_t totalSize(const fs::path& path) {
	std::error_code ec;
	const fs::file_status st = fs::symlink_status(path, ec);
	if (fs::is_regular_file(st)) return fs::file_size(path, ec);
	if (!fs::is_directory(st)) return 0;
	std::uintmax_t total = 0;
	for (fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
	     it.increment(ec)) {
		std::error_code e2;
		if (it->is_symlink(e2)) continue;
		if (it->is_regular_file(e2)) total += it->file_size(e2);
	}
	return total;
}

std::string humanSize(std::uintmax_t bytes) {
	if (bytes < 1024) return std::to_string(bytes) + (bytes == 1 ? " byte" : " bytes");
	static const char* units[] = {"KB", "MB", "GB", "TB", "PB"};
	double v = static_cast<double>(bytes);
	int u = -1;
	while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.1f %s", v, units[u]);
	return buf;
}

std::vector<Entry> list(const fs::path& dir, bool showHidden, std::string* error) {
	std::vector<Entry> entries;
	std::error_code ec;
	fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
	if (ec) {
		if (error) *error = "Cannot open the folder: " + ec.message();
		return entries;
	}
	for (fs::directory_iterator end; it != end; it.increment(ec)) {
		if (ec) break;
		Entry e;
		e.name = it->path().filename().string();
		if (!showHidden && !e.name.empty() && e.name[0] == '.') continue;
		e.path = it->path();
		std::error_code e2;
		e.isLink = it->is_symlink(e2);
		e.isDir = it->is_directory(e2);
		if (!e.isDir && it->is_regular_file(e2)) e.size = it->file_size(e2);
		e.modified = it->last_write_time(e2);
		entries.push_back(std::move(e));
	}
	std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
		if (a.isDir != b.isDir) return a.isDir;
		const std::string la = lower(a.name), lb = lower(b.name);
		return la != lb ? la < lb : a.name < b.name;
	});
	if (error) error->clear();
	return entries;
}

}  // namespace colibry::files::fileops
