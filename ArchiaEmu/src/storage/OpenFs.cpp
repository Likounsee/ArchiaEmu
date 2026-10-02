#include "storage/OpenFs.hpp"

#include <algorithm>
#include <map>

namespace myps5emu::storage {

OpenFs::OpenFs()
{
    nodes_.emplace("/", Node{true, {}});
}

bool OpenFs::NormalizePath(const std::string& path,
                           std::string& normalized)
{
    if (path.empty() || path.front() != '/') {
        return false;
    }

    normalized.clear();
    normalized.reserve(path.size());
    normalized.push_back('/');

    std::string component;
    auto flush = [&]() {
        if (component.empty() || component == ".") {
            component.clear();
            return true;
        }
        if (component == "..") {
            return false;
        }
        if (normalized.size() > 1) {
            normalized.push_back('/');
        }
        normalized += component;
        component.clear();
        return true;
    };

    for (std::size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!flush()) {
                return false;
            }
        } else {
            component.push_back(path[i]);
        }
    }

    if (normalized.size() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    return true;
}

std::string OpenFs::ParentPath(const std::string& path)
{
    const auto slash = path.find_last_of('/');
    if (slash == 0) {
        return "/";
    }
    return path.substr(0, slash);
}

std::string OpenFs::BaseName(const std::string& path)
{
    const auto slash = path.find_last_of('/');
    return path.substr(slash + 1);
}

bool OpenFs::CreateDirectory(const std::string& path)
{
    std::string normalized;
    if (!NormalizePath(path, normalized) ||
        normalized == "/" ||
        nodes_.contains(normalized) ||
        !nodes_.contains(ParentPath(normalized)) ||
        !nodes_.at(ParentPath(normalized)).directory) {
        return false;
    }

    nodes_.emplace(normalized, Node{true, {}});
    return true;
}

bool OpenFs::WriteFile(const std::string& path,
                       const std::vector<std::uint8_t>& data)
{
    std::string normalized;
    if (!NormalizePath(path, normalized) ||
        normalized == "/" ||
        !nodes_.contains(ParentPath(normalized)) ||
        !nodes_.at(ParentPath(normalized)).directory) {
        return false;
    }

    auto [it, inserted] = nodes_.try_emplace(normalized, Node{false, {}});
    if (!inserted && it->second.directory) {
        return false;
    }
    it->second.data = data;
    return true;
}

bool OpenFs::ReadFile(const std::string& path,
                      std::vector<std::uint8_t>& data) const
{
    std::string normalized;
    if (!NormalizePath(path, normalized)) {
        return false;
    }
    const auto it = nodes_.find(normalized);
    if (it == nodes_.end() || it->second.directory) {
        return false;
    }
    data = it->second.data;
    return true;
}

bool OpenFs::Exists(const std::string& path) const
{
    std::string normalized;
    return NormalizePath(path, normalized) &&
           nodes_.contains(normalized);
}

bool OpenFs::IsDirectory(const std::string& path) const
{
    std::string normalized;
    if (!NormalizePath(path, normalized)) {
        return false;
    }
    const auto it = nodes_.find(normalized);
    return it != nodes_.end() && it->second.directory;
}

bool OpenFs::Remove(const std::string& path)
{
    std::string normalized;
    if (!NormalizePath(path, normalized) || normalized == "/") {
        return false;
    }

    const auto it = nodes_.find(normalized);
    if (it == nodes_.end()) {
        return false;
    }

    const std::string prefix = normalized + "/";
    const auto child = nodes_.lower_bound(prefix);
    if (child != nodes_.end() &&
        child->first.compare(0, prefix.size(), prefix) == 0) {
        return false;
    }

    nodes_.erase(it);
    return true;
}

std::vector<std::string> OpenFs::ListDirectory(
    const std::string& path) const
{
    std::string normalized;
    if (!NormalizePath(path, normalized) ||
        !IsDirectory(normalized)) {
        return {};
    }

    const std::string prefix =
        normalized == "/" ? "/" : normalized + "/";

    std::vector<std::string> result;
    for (const auto& [name, node] : nodes_) {
        if (name == normalized ||
            name.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }

        const auto remainder = name.substr(prefix.size());
        if (remainder.find('/') == std::string::npos) {
            result.push_back(BaseName(name));
        }
    }

    return result;
}

} // namespace myps5emu::storage
