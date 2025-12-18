import std;

import d2x.log;
import d2x.checker;

void print_help() {
    std::println("d2x version: 0.1.0\n");
    std::println("Usage: $ d2x [command] [target]\n");
    std::println("Commands:");
    std::println("\t new,      \t create new d2x project");
    std::println("\t book,     \t open project's book");
    std::println("\t run,      \t run sourcecode file");
    std::println("\t checker,  \t run checker for d2x project's exercises");
    std::println("\t help,     \t help info");
}

int main(int argc, char* argv[]) {

    if (argc == 1 || (argc == 2 && std::string(argv[1]) == "help")) {
        print_help();
        return 0;
    }

    std::string command = argv[1];

    if (command == "new") {
        std::println("TODO: Creating new d2x project...");
    } else if (command == "book") {
        // if book exists, open it by mdbook
        if (std::filesystem::exists("book")) {
            std::system("mdbook serve --open book");
        } else {
            std::println("Error: No book found in the current directory.");
        }
    } else if (command == "run") {
        std::println("TODO: Running sourcecode file...");
    } else if (command == "checker") {
            d2x::checker::run();
    } else {
        std::println("Unknown command: {}", command);
        std::println("Use 'd2x help' for usage information");
        return 1;
    }

    return 0;
}