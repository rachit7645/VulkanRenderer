#include "Files.h"

#include <filesystem>

// Namespace aliases
namespace filesystem = std::filesystem;

namespace Engine
{
    void Files::SetResources(const std::string_view relPath)
    {
        // Set resource path
        m_resDir = filesystem::absolute(relPath).string();
    }

    const std::string& Files::GetResources()
    {
        // Return resource path
        return m_resDir;
    }

    std::string Files::GetName(const std::string_view path)
    {
        // Return name
        return filesystem::path(path).filename().string();
    }

    std::string Files::GetDirectory(const std::string_view path)
    {
        // Get directory
        auto directory = filesystem::path(path).parent_path().string();
        // Get separator
        auto separator = static_cast<char>(filesystem::path::preferred_separator);
        // Add and return
        return directory + separator;
    }

    Files& Files::GetInstance()
    {
        // Static storage
        static Files files;
        // Return
        return files;
    }
}