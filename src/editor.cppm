// TODO: add more editor support
export module d2x.editor;

import std;

import d2x.log;

namespace d2x {
namespace editor {

export void open(const std::string& file_path) {
    auto absolute_path = std::filesystem::absolute(file_path).string();
    auto command = std::format("code \"{}\"", absolute_path);
    auto result = std::system(command.c_str());
    if (result != 0) {
        log::warning("Failed to open file '{}' in VS Code.", file_path);
    }
}

} // namespace editor
} // namespace d2x