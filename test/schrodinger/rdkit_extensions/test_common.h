#pragma once

#include <exception>
#include <string>

#include <filesystem>

/**
 * Gets full path to a file in the testfiles directory in the source
 * @param filename file found in the testfiles folder
 * @return full filesystem path to that file
 */
std::string testfile_path(const std::string& filename)
{
    auto path = std::filesystem::path(getenv("SCHRODINGER_SRC")) / "mmshare" /
                "test" / "schrodinger" / "rdkit_extensions" / "testfiles" /
                filename;
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("File not found: " +
                                 std::filesystem::absolute(path).string());
    }
    return path.string();
}

/**
 * Predicate to check if an exception message contains a specific substring
 */
struct MsgSubstr {
    MsgSubstr(const std::string& expected) : m_expected(expected)
    {
    }
    bool operator()(const std::exception& ex) const
    {
        return std::string(ex.what()).find(m_expected) != std::string::npos;
    }

  private:
    std::string m_expected;
};

/**
 * Marco to check a specific exception type with a specific message substring
 */
#define TEST_CHECK_EXCEPTION_MSG_SUBSTR(statement, exception_type, substr) \
    BOOST_CHECK_EXCEPTION(statement, exception_type, MsgSubstr(substr))
