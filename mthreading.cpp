#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <thread>
#include <future>
#include <algorithm>
#include <memory>

namespace fs = std::filesystem;

// State-machine tokenizer that handles:
//   - Double quotes:   cd "My Documents"  → ["cd", "My Documents"]
//   - Single quotes:   echo 'hello world' → ["echo", "hello world"]
//   - Backslash escape: cp My\ File.txt dest/ → ["cp", "My File.txt", "dest/"]
//   - Multiple/leading/trailing spaces are ignored
enum class TokenState { NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE, ESCAPED };

std::vector<std::string> tokenize(const std::string &input)
{
    std::vector<std::string> tokens;
    std::string current;
    TokenState state = TokenState::NORMAL;
    TokenState prev_state = TokenState::NORMAL; // saved state before ESCAPED

    for (size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];

        // Strip Windows-style carriage returns
        if (c == '\r') continue;

        switch (state)
        {
        case TokenState::NORMAL:
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                // Space in NORMAL mode = end of current token
                if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }
                // Consecutive spaces are simply skipped
            }
            else if (c == '\'')
            {
                // Enter single-quote mode: everything until closing ' is literal
                state = TokenState::IN_SINGLE_QUOTE;
            }
            else if (c == '"')
            {
                // Enter double-quote mode: supports backslash escapes inside
                state = TokenState::IN_DOUBLE_QUOTE;
            }
            else if (c == '\\')
            {
                // Backslash in NORMAL: take the next character literally
                prev_state = TokenState::NORMAL;
                state = TokenState::ESCAPED;
            }
            else
            {
                current += c;
            }
            break;

        case TokenState::IN_SINGLE_QUOTE:
            if (c == '\'')
            {
                // Closing single quote: return to NORMAL
                state = TokenState::NORMAL;
            }
            else
            {
                // Everything inside single quotes is literal (spaces, *, etc.)
                current += c;
            }
            break;

        case TokenState::IN_DOUBLE_QUOTE:
            if (c == '"')
            {
                // Closing double quote: return to NORMAL
                state = TokenState::NORMAL;
            }
            else if (c == '\\')
            {
                // Backslash inside double quotes: escape the next char
                prev_state = TokenState::IN_DOUBLE_QUOTE;
                state = TokenState::ESCAPED;
            }
            else
            {
                current += c;
            }
            break;

        case TokenState::ESCAPED:
            // Whatever the next char is, take it literally (e.g. space, quote, backslash)
            current += c;
            state = prev_state;
            break;
        }
    }

    // Handle unterminated quotes
    if (state == TokenState::IN_SINGLE_QUOTE || state == TokenState::IN_DOUBLE_QUOTE)
    {
        std::cerr << "Error: unterminated quote in input\n";
        return {};
    }

    // Flush the last token (input doesn't end with a space)
    if (!current.empty())
    {
        tokens.push_back(current);
    }

    return tokens;
}

class Command
{
public:
    virtual void execute(const std::vector<std::string> &args) = 0;
    virtual ~Command() {}
};

class ModifyCommand : public Command
{
public:
    void execute(const std::vector<std::string> &args) override
    {
    }
};

class NoModifyCommand : public Command
{
public:
    void execute(const std::vector<std::string> &args) override
    {
    }
};

class CdCommand : public NoModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override
    {
        bool verbose = false;

        for (size_t i = 1; i < args.size(); ++i)
        {
            if (args[i] == "-h" || args[i] == "--help")
            {
                displayHelp();
                return;
            }
            else if (args[i] == "-v" || args[i] == "--verbose")
            {
                verbose = true;
            }
        }

        if (args.size() == 1 || (args.size() == 2 && args[1] == "/"))
        {
            fs::current_path(fs::current_path().root_path());
            if (verbose)
            {
                std::cout << "Changed to the root directory: " << fs::current_path() << "\n";
            }
        }
        else
        {
            std::string targetDir = args.back();
            if (targetDir == "~")
            {
                targetDir = getenv("HOME");
            }
            else if (targetDir.substr(0, 2) == "~/" || targetDir.substr(0, 3) == "~/.")
            {
                // Handle ~ and ~. notation
                targetDir.replace(0, 1, getenv("HOME"));
            }

            if (fs::exists(targetDir) && fs::is_directory(targetDir))
            {
                fs::current_path(targetDir);
                if (verbose)
                {
                    std::cout << "Changed to: " << fs::current_path() << "\n";
                }
            }
            else
            {
                std::cout << "Invalid directory: " << targetDir << "\n";
            }
        }
    }

private:
    void displayHelp()
    {
        std::cout << "Usage: cd <directory>\n"
                  << "Change the current working directory to the specified directory.\n\n"
                  << "Options:\n"
                  << " -h, --help    display this help and exit\n"
                  << " cd ~           To change to the home directory,\n"
                  << " cd ..            To go to parent directory\n"
                  << " cd -v/-verbose <path>,     print the full path of the new directory\n"
                  << "cd /              To go to the ROOT directory\n";
    }
};

class LsCommand : public NoModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override
    {
        bool recursive = false;
        bool showHelp = false;
        bool humanReadable = false;

        for (size_t i = 1; i < args.size(); ++i)
        {
            if (args[i] == "-r" || args[i] == "--recursive")
            {
                recursive = true;
            }
            else if (args[i] == "-h" || args[i] == "--help")
            {
                showHelp = true;
            }
            else if (args[i] == "-H" || args[i] == "--human-readable")
            {
                humanReadable = true;
            }
        }

        if (showHelp)
        {
            displayHelp();
        }
        else
        {
            std::string directoryPath = fs::current_path();

            if (args.size() > 1 && args[1] == "~")
            {
                directoryPath = fs::path(getenv("HOME")).string();
            }

            listDirectory(directoryPath, recursive, humanReadable);
        }
    }

private:
    void listDirectory(const fs::path &dirPath, bool recursive, bool humanReadable)
    {
        std::vector<std::future<void>> futures;

        fs::directory_iterator end_itr;
        for (fs::directory_iterator itr(dirPath); itr != end_itr; ++itr)
        {
            if (recursive && fs::is_directory(itr->path()))
            {
                futures.push_back(std::async(std::launch::async, &LsCommand::listDirectory, this, itr->path(), true, humanReadable));
            }
            else
            {
                std::cout << itr->path().filename();
                if (humanReadable)
                {
                    std::cout << " (" << formatSize(fs::file_size(itr->path())) << ")";
                }
                std::cout << "\n";
            }
        }

        // Wait for all threads to finish
        for (auto &future : futures)
        {
            future.get();
        }
    }

    void displayHelp()
    {
        std::cout << "Usage: ls [OPTION] [DIRECTORY]\n"
                  << "List information about the FILEs (the current directory by default).\n\n"
                  << "Options:\n"
                  << "  -r, --recursive         list subdirectories recursively\n"
                  << "  -h, --help              display this help and exit\n"
                  << "  -H, --human-readable    list file sizes in human-readable format\n"
                  << "  - ~(tilde)              list users home directory\n";
    }

    std::string formatSize(uintmax_t size)
    {
        static const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};
        int suffixIndex = 0;
        while (size >= 1024 && suffixIndex < 4)
        {
            size /= 1024;
            ++suffixIndex;
        }
        std::stringstream ss;
        ss << size << " " << suffixes[suffixIndex];
        return ss.str();
    }
};

class MvCommand : public ModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override
    {
        bool listFiles = false;
        bool recursive = false;
        bool noOverwrite = false;

        for (size_t i = 1; i < args.size(); ++i)
        {
            const std::string &arg = args[i];

            if (arg == "-h" || arg == "--help")
            {
                displayHelp();
                return;
            }
            else if (arg == "-l")
            {
                listFiles = true;
            }
            else if (arg == "-R")
            {
                recursive = true;
            }
            else if (arg == "-n")
            {
                noOverwrite = true;
            }
        }

        if (args.size() < 3)
        {
            std::cerr << "Usage: mv [OPTION] <source> <destination>\n";
            return;
        }

        const std::string &source = args[args.size() - 2];
        const std::string &destination = args[args.size() - 1];

        if (listFiles)
        {
            listFilesToBeMoved(source, destination, recursive);
        }
        else
        {
            performMove(source, destination, recursive, noOverwrite);
        }
    }

private:
    void performMove(const std::string &source, const std::string &destination, bool recursive, bool noOverwrite)
    {
        try
        {
            if (noOverwrite && fs::exists(destination))
            {
                std::cout << "Destination file already exists. Skipping move.\n";
                return;
            }

            if (recursive)
            {
                std::promise<void> promise;
                std::future<void> future = promise.get_future();

                std::thread thread(&MvCommand::recursiveMove, this, source, destination, std::move(promise));

                try { future.get(); }
                catch (const std::exception &e)
                {
                    thread.join();
                    throw;
                }

                thread.join();
            }
            else
            {
                fs::rename(source, destination);
                std::cout << "Moved: " << source << " to " << destination << "\n";
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error moving " << source << " to " << destination << ": " << e.what() << "\n";
        }
    }

    void recursiveMove(const std::string &source, const std::string &destination, std::promise<void> promise)
    {
        try
        {
            fs::copy(source, destination, fs::copy_options::recursive | fs::copy_options::skip_existing);
            fs::remove_all(source);

            std::cout << "Moved (recursively): " << source << " to " << destination << "\n";

            promise.set_value();
        }
        catch (...)
        {
            try { promise.set_exception(std::current_exception()); }
            catch (...) {}
        }
    }

    void listFilesToBeMoved(const std::string &source, const std::string &destination, bool recursive)
    {
        try
        {
            if (!recursive)
            {
                for (const auto &entry : fs::recursive_directory_iterator(source))
                {
                    const std::string &sourceFile = entry.path().string();
                    const std::string destinationFile = destination + sourceFile.substr(source.size());
                    std::cout << "Will move: " << sourceFile << " to " << destinationFile << "\n";
                }
            }
            else
            {
                std::cout << "Will move: " << source << " to " << destination << "\n";
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error listing files to be moved: " << e.what() << "\n";
        }
    }

    void displayHelp()
    {
        std::cout << "Usage: mv [OPTION] <source> <destination>\n"
                  << "Move (rename) the specified source file or directory to the destination.\n"
                  << "If the destination is an existing directory, the source is moved into the directory.\n\n"
                  << "Options:\n"
                  << "  -l            list the files that will be moved\n"
                  << "  -R            recursively move directories and their contents\n"
                  << "  -n            do not overwrite existing files\n"
                  << "  -h, --help    display this help and exit\n";
    }
};

class CpCommand : public ModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override
    {
        bool recursive = false;
        bool noOverwrite = false;
        bool verbose = false;

        for (size_t i = 1; i < args.size(); ++i)
        {
            const std::string &arg = args[i];

            if (arg == "-h" || arg == "--help")
            {
                displayHelp();
                return;
            }
            else if (arg == "-r" || arg == "-R")
            {
                recursive = true;
            }
            else if (arg == "-n")
            {
                noOverwrite = true;
            }
            else if (arg == "-v" || arg == "--verbose")
            {
                verbose = true;
            }
        }

        if (args.size() < 3)
        {
            std::cerr << "Usage: cp [OPTION] <source> <destination>\n";
            return;
        }

        const std::string &source = args[args.size() - 2];
        const std::string &destination = args[args.size() - 1];

        copyFiles(source, destination, recursive, noOverwrite, verbose);
    }

private:
    void copyFiles(const std::string &source, const std::string &destination, bool recursive, bool noOverwrite, bool verbose)
    {
        try
        {
            fs::copy_options options = fs::copy_options::none;
            if (recursive)
            {
                std::promise<void> promise;
                std::future<void> future = promise.get_future();

                std::thread thread(&CpCommand::recursiveCopy, this, source, destination, std::move(promise));

                try { future.get(); }
                catch (const std::exception &e)
                {
                    thread.join();
                    throw;
                }

                thread.join();
            }
            else
            {
                fs::copy(source, destination, options);
            }

            if (verbose)
            {
                std::cout << "Copied: " << source << " to " << destination << "\n";
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error copying " << source << " to " << destination << ": " << e.what() << "\n";
        }
    }

    void recursiveCopy(const std::string &source, const std::string &destination, std::promise<void> promise)
    {
        try
        {
            fs::copy(source, destination, fs::copy_options::recursive | fs::copy_options::skip_existing);

            promise.set_value();
        }
        catch (...)
        {
            try { promise.set_exception(std::current_exception()); }
            catch (...) {}
        }
    }

    void displayHelp()
    {
        std::cout << "Usage: cp [OPTION] <source> <destination>\n"
                  << "Copy the specified source file or directory to the destination.\n\n"
                  << "Options:\n"
                  << "  -r, -R        copy directories recursively\n"
                  << "  -n            do not overwrite existing files\n"
                  << "  -v, --verbose print the names of the files and directories that are copied\n"
                  << "  -h, --help    display this help and exit\n";
    }
};

class RmCommand : public ModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override
    {
        bool recursive = false;
        bool force = false;
        bool verbose = false;
        std::vector<std::future<void>> futures;

        for (size_t i = 1; i < args.size(); ++i)
        {
            const std::string &arg = args[i];

            if (arg == "-h" || arg == "--help")
            {
                displayHelp();
                return;
            }
            else if (arg == "-r" || arg == "-R" || arg == "--recursive")
            {
                recursive = true;
            }
            else if (arg == "-f" || arg == "--force")
            {
                force = true;
            }
            else if (arg == "-v" || arg == "--verbose")
            {
                verbose = true;
            }
            else
            {
                futures.push_back(
                    std::async(std::launch::async,
                               &RmCommand::removeFileOrDirectory,
                               this, arg, recursive, force, verbose)
                );
            }
        }

        for (auto &f : futures)
        {
            try { f.get(); }
            catch (const std::exception &e)
            {
                std::cerr << "rm task failed: " << e.what() << "\n";
            }
        }
    }

private:
    void removeFileOrDirectory(const std::string &targetPath, bool recursive, bool force, bool verbose)
    {
        if (!fs::exists(targetPath))
        {
            if (!force)
            {
                std::cerr << "Error: " << targetPath << " does not exist.\n";
            }
            return;
        }

        try
        {
            if (fs::is_directory(targetPath))
            {
                if (recursive)
                {
                    std::vector<std::future<void>> futures;

                    // Recursively remove each file or directory in a separate thread
                    for (const auto &entry : fs::recursive_directory_iterator(targetPath))
                    {
                        futures.push_back(std::async(std::launch::async, &RmCommand::removeFileOrDirectory, this, entry.path().string(), true, force, verbose));
                    }

                    // Wait for all threads to finish
                    std::for_each(futures.begin(), futures.end(), [](std::future<void> &f)
                                  { f.wait(); });

                    // Remove the parent directory
                    fs::remove(targetPath);
                    if (verbose)
                    {
                        std::cout << "Removed: " << targetPath << "\n";
                    }
                }
                else
                {
                    std::cerr << "Error: " << targetPath << " is a directory. Use -r, -R, or --recursive to remove directories recursively.\n";
                }
            }
            else
            {
                fs::remove(targetPath);
                if (verbose)
                {
                    std::cout << "Removed: " << targetPath << "\n";
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error removing " << targetPath << ": " << e.what() << "\n";
        }
    }

    void displayHelp()
    {
        std::cout << "Usage: rm [OPTION] <file/directory>...\n"
                  << "Remove (unlink) the specified files or directories.\n\n"
                  << "Options:\n"
                  << "  -r, -R, --recursive    remove directories and their contents recursively\n"
                  << "  -f, --force           ignore nonexistent files, and never prompt before removing\n"
                  << "  -v, --verbose         print the names of the files and directories that are removed\n"
                  << "  -h, --help            display this help and exit\n";
    }
};

int main()
{
    std::string input;
    std::cout << ".........Welcome to New Shell........\n";

    while (true)
    {
        std::cout << ">>";
        std::cout << "Enter command: ";
        std::getline(std::cin, input);

        std::vector<std::string> tokens = tokenize(input);
        if (tokens.empty()) continue;

        std::unique_ptr<Command> command;
        if (tokens[0] == "cd")
            command = std::make_unique<CdCommand>();
        else if (tokens[0] == "ls")
            command = std::make_unique<LsCommand>();
        else if (tokens[0] == "mv")
            command = std::make_unique<MvCommand>();
        else if (tokens[0] == "cp")
            command = std::make_unique<CpCommand>();
        else if (tokens[0] == "rm")
            command = std::make_unique<RmCommand>();
        else if (tokens[0] == "exit")
            break;
        else
        {
            std::cout << "Invalid command\n";
            continue;
        }

        command->execute(tokens);
    }

    return 0;
}
