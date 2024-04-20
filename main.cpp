#include "node_filesystem.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <operation> [<args>]\n";
        std::cerr << "Operations:\n"
                  << "\tcreate <file_name> <content>\n"
                  << "\tread <file_name>\n"
                  << "\tdelete <file_name>\n"
                  << "\tlist\n";
        return 1;
    }

    std::string operation = argv[1];
    NodeFileSystem fs("./files"); // All operations will be performed in the ./files directory

    if (operation == "create") {
        if (argc != 4) {
            std::cerr << "Usage: " << argv[0] << " create <file_name> <content>\n";
            return 1;
        }
        fs.createFile(argv[2], argv[3]);
    } else if (operation == "read") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " read <file_name>\n";
            return 1;
        }
        fs.readFile(argv[2]);
    } else if (operation == "delete") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " delete <file_name>\n";
            return 1;
        }
        fs.deleteFile(argv[2]);
    } else if (operation == "list") {
        fs.listFiles();
    } else {
        std::cerr << "Invalid operation: " << operation << "\n";
        return 1;
    }

    return 0;
}