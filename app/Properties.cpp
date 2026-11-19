#include "Properties.hpp"

#include "FileOps.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <cctype>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace colibry::files {

std::string permissionString(fs::perms p) {
	std::string s(9, '-');
	const fs::perms bits[9] = {fs::perms::owner_read, fs::perms::owner_write, fs::perms::owner_exec,
	                           fs::perms::group_read, fs::perms::group_write, fs::perms::group_exec,
	                           fs::perms::others_read, fs::perms::others_write, fs::perms::others_exec};
	const char letters[3] = {'r', 'w', 'x'};
	for (int i = 0; i < 9; ++i)
		if ((p & bits[i]) != fs::perms::none) s[static_cast<std::size_t>(i)] = letters[i % 3];
	return s;
}

static std::string typeOf(const fs::path& path, const fs::file_status& st, bool isLink) {
	if (isLink) return "Symbolic link";
	if (fs::is_directory(st)) return "Folder";
	if (!fs::is_regular_file(st)) return "Special file";
	std::string ext = path.extension().string();
	for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (ext == ".txt" || ext == ".md" || ext == ".log") return "Text file";
	if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp" || ext == ".webp") return "Image";
	if (ext == ".mp3" || ext == ".ogg" || ext == ".wav" || ext == ".flac") return "Audio";
	if (ext == ".mp4" || ext == ".mkv" || ext == ".webm") return "Video";
	if (ext == ".gz" || ext == ".tgz" || ext == ".tar" || ext == ".zip") return "Archive";
	return ext.empty() ? "File" : ext.substr(1) + " file";
}

Properties describe(const fs::path& path) {
	Properties p;
	std::error_code ec;
	const fs::file_status link = fs::symlink_status(path, ec);
	if (ec || !fs::exists(link)) {
		p.error = "The item does not exist.";
		return p;
	}
	const bool isLink = fs::is_symlink(link);
	const fs::file_status st = fs::status(path, ec);
	p.name = path.filename().string();
	p.location = path.parent_path().string();
	p.type = typeOf(path, st, isLink);
	p.permissions = permissionString(link.permissions());
	p.size = fileops::totalSize(path);
	if (!isLink && fs::is_directory(link)) {
		for (fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
			++p.items;
	}
	const auto ft = fs::last_write_time(path, ec);
	if (!ec) {
		const auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
		    ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
		const std::time_t t = std::chrono::system_clock::to_time_t(sys);
		std::tm tm{};
		localtime_r(&t, &tm);
		std::ostringstream out;
		out << std::put_time(&tm, "%Y-%m-%d %H:%M");
		p.modified = out.str();
	}
	p.ok = true;
	return p;
}

std::string Properties::text() const {
	if (!ok) return error;
	std::ostringstream out;
	out << "Name: " << name << "\nType: " << type << "\nLocation: " << location
	    << "\nSize: " << fileops::humanSize(size);
	if (type == "Folder") out << " (" << items << " items)";
	out << "\nModified: " << modified << "\nPermissions: " << permissions;
	return out.str();
}

}  // namespace colibry::files
