#include "Files.h"
#include "Log.h"

#include <filesystem>
#include <fstream>

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

    usize Files::GetFileSize(const std::string_view path)
    {
        // Assert
        static_assert(sizeof(usize) >= sizeof(std::uintmax_t), "How???");
        // Get file size
        return filesystem::file_size(path);
    }

    std::vector<u8> Files::ReadBytes(const std::string_view path)
    {
        // Create file
        auto bin = std::ifstream(path.data(), std::ios::binary);

        // Make sure files was opened
        if (!bin.is_open())
        {
            // Log
            LOG_ERROR("Failed to load shader binary {}!\n", path);
        }

        // Get file size
        auto fileSize = GetFileSize(path);
        // Binary data
        auto binary = std::vector<u8>();
        binary.resize(fileSize);

        // Read data into buffer
        bin.read(reinterpret_cast<char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
        // Close shader file
        bin.close();

        // Return
        return binary;
    }

    Files& Files::GetInstance()
    {
        // Static storage
        static Files files;
        // Return
        return files;
    }
}