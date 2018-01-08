// MyDelete.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <boost/system/error_code.hpp>
using namespace boost::filesystem;

//Note: Google Drive will replace %XX to _XX

std::vector<boost::regex> build_regexs(const std::vector<path>& folder_names)
{
    std::vector<boost::regex> regexs;

    for (const path& folder : folder_names)
    {
        std::string name = folder.string();
        static const std::vector<std::pair<const char*, const char*>> items =
        {
            { "/", "\\" },
            { "\\", "\\\\" },
            { " ", "[ ]" },
            { "(", "\\(" }, { ")", "\\)" },
            { "[", "\\[" }, { "]", "\\]" },
            { "{", "\\{" }, { "}", "\\}" },
            { ".", "\\." },
            { "*", ".*?" },
            { "?", "." },
            { "^", "\\^" },
            { "$", "\\$" },
        };

        for (auto pair : items)
        {
            boost::replace_all(name, pair.first, pair.second);
        }

        regexs.push_back(boost::regex("(?ix) \\\\" + name + "$"));
    }

    return regexs;
}

std::vector<path> get_files(const path& dir_path)
{
    std::vector<path> files;
    path path(dir_path);

    if (!exists(dir_path))
    {
        std::cout << "error: " << dir_path.string() << " does not exist." << std::endl;
        return files;
    }

    boost::filesystem::recursive_directory_iterator end_itr;

    for (boost::filesystem::recursive_directory_iterator it(path); it != end_itr; ++it)
    {
        if (boost::filesystem::is_regular_file(it->status()))
        {
            files.push_back(it->path());
        }
    }

    return files;
}

bool remove_attribute(const std::string& file_name, const DWORD attribute = FILE_ATTRIBUTE_READONLY)
{
    DWORD dwAttrs = ::GetFileAttributes(file_name.c_str());

    if (INVALID_FILE_ATTRIBUTES == dwAttrs)
    {
        std::cout << "failed to get attribute for " << file_name << std::endl;
        return false;
    }

    if (dwAttrs & attribute)
    {
        return ::SetFileAttributes(file_name.c_str(), dwAttrs & ~attribute);
    }

    return true;
}

std::vector<path> get_percent_encoding_url_files(const std::vector<path>& files)
{
    std::vector<path> result;
    static const boost::regex e("[%_][0-9A-F]{2}");

    for (const path& file : files)
    {
        std::string filename = file.filename().string();

        if (boost::regex_search(filename, e))
        {
            result.push_back(file);
            //std::cout << filename << std::endl;
        }
    }

    return result;
}

std::vector<std::pair<path, path>> rename(const std::vector<path>& files)
{
    std::vector<std::pair<path, path>> result;
    static const boost::regex e("[%_][0-9A-F]{2}");

    for (const path& file : files)
    {
        std::vector<std::pair<std::string, std::string>> replaces;
        std::string filename = file.filename().string();
        boost::sregex_iterator beg(filename.begin(), filename.end(), e);
        boost::sregex_iterator end;

        for (; beg != end; ++beg)
        {
            try
            {
                std::string s = beg->str();
                std::string s2 = "0x" + s.substr(1);
                size_t x = std::stoul(s2.c_str(), nullptr, 16);

                if (boost::is_print()(x))
                {
                    //std::cout << std::string(1, (char)x) << std::endl;
                    replaces.push_back(std::make_pair(beg->str(), std::string(1, (char)x)));
                }
            }
            catch (...)
            {
            }
        }

        if (replaces.empty())
        {
            continue;
        }

        for (std::pair<std::string, std::string>& pair : replaces)
        {
            boost::replace_all(filename, pair.first, pair.second);
        }

        path new_file = file.parent_path() / filename;
        std::cout << file.string() << " ==> " << new_file.filename().string() << std::endl;
        result.push_back(std::make_pair(file, new_file));
    }

    return result;
}

void real_rename(std::vector<std::pair<path, path>>& files)
{
    for (std::pair<path, path>& pair : files)
    {
        remove_attribute(pair.first.string());

        boost::system::error_code err;
        rename(pair.first, pair.second, err);

        if (err)
        {
            std::cout << "error: " << err.message() << ", " << pair.first.string() << std::endl;
        }
    }
}

std::vector<std::pair<path, path>> remove_specific(path dir, const std::string& remove_tring)
{
    std::vector<std::pair<path, path>> result;
    std::vector<path> files = get_files(dir);

    for (path& file : files)
    {
        std::string filename = file.filename().string();

        if (boost::contains(filename, remove_tring))
        {
            boost::replace_all(filename, remove_tring, "");
            result.push_back(std::make_pair(file, file.parent_path() / filename));
            std::cout << file.string() << " ==> " << filename << std::endl;
        }
    }

    return result;
}


int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2)
    {
        std::cout
                << "Usage:\n\t"
                << path(argv[0]).stem().string() << " <path>\n";

        return 0;
    }

    if (argc == 3)
    {
        std::vector<std::pair<path, path>>& files = remove_specific(argv[1], argv[2]);
        real_rename(files);
        return 0;
    }

    std::vector<path> folders_to_remove(argv + 2, argv + argc);
    const std::vector<path>& files = get_files(argv[1]);

    {
        std::vector<path> urls = get_percent_encoding_url_files(files);
        std::vector<std::pair<path, path>>& files = rename(urls);
        real_rename(files);
        return 0;
    }

    if (files.empty())
    {
        system("cls");
        std::cout << "folder not found" << std::endl;
        return 0;
    }

    std::cout << "type 'yes' for delete: ";

    std::string command;
    std::cin >> command;

    if (boost::iequals("yes", command))
    {
        for (size_t i = 0; i < files.size(); ++i)
        {
            if (exists(files[i]))
            {
                boost::filesystem::recursive_directory_iterator end_itr; // default construction yields past-the-end

                for (boost::filesystem::recursive_directory_iterator it(files[i]); it != end_itr; ++it)
                {
                    remove_attribute(it->path().string(), FILE_ATTRIBUTE_READONLY);
                }

                boost::system::error_code ec;
                remove_all(files[i], ec);

                if (ec)
                {
                    std::cout << files[i].string() << ": " << ec.message() << std::endl;
                }
            }
        }
    }

    return 0;
}
