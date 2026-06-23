#include "MiniDB.hpp"

using namespace minidb;

int main() {
    Database db("db_data");
    std::cout << "MiniDB ready. Enter SQL-like commands ending with semicolon.\n";
    std::cout << "Supported: CREATE TABLE, INSERT INTO, SELECT, DELETE FROM, SHOW TABLES, DESCRIBE, EXIT\n";

    std::string line, buffer;
    while (true) {
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        buffer += line;
        buffer += ' ';

        if (line.find(';') == std::string::npos) continue;

        std::string cmd = buffer;
        buffer.clear();

        std::string upper;
        upper.reserve(cmd.size());
        for (char c : cmd) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (upper.find("EXIT") != std::string::npos || upper.find("QUIT") != std::string::npos) {
            std::cout << "Bye.\n";
            break;
        }

        std::string out = db.execute(cmd);
        if (!out.empty()) std::cout << out << '\n';
    }
    return 0;
}
