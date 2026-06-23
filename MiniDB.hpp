#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace minidb {

struct Column {
    std::string name;
    std::string type;   // INT, DOUBLE, TEXT
    bool primaryKey{false};
};

struct Row {
    std::vector<std::string> values;
    bool deleted{false};
};

struct Condition {
    std::string column;
    std::string value;
    bool hasCondition{false};
};

struct OrderBy {
    std::string column;
    bool ascending{true};
    bool hasOrder{false};
};

struct SelectQuery {
    std::vector<std::string> columns; // empty => *
    std::string tableName;
    Condition where;
    OrderBy orderBy;
};

struct DeleteQuery {
    std::string tableName;
    Condition where;
};

struct InsertQuery {
    std::string tableName;
    std::vector<std::string> values;
};

struct CreateTableQuery {
    std::string tableName;
    std::vector<Column> columns;
};

class BTreeIndex {
public:
    explicit BTreeIndex(int minimumDegree = 3);
    void clear();
    void insert(const std::string& key, std::size_t rowId);
    std::vector<std::size_t> orderedRowIds(bool ascending = true) const;
    std::vector<std::size_t> search(const std::string& key) const;

private:
    struct Node {
        bool leaf{true};
        std::vector<std::string> keys;
        std::vector<std::vector<std::size_t>> payloads;
        std::vector<std::unique_ptr<Node>> children;

        explicit Node(bool isLeaf = true) : leaf(isLeaf) {}
    };

    int t_;
    std::unique_ptr<Node> root_;

    void insertNonFull(Node* node, const std::string& key, std::size_t rowId);
    void splitChild(Node* parent, int index);
    void traverse(const Node* node, std::vector<std::size_t>& out) const;
    void traverseReverse(const Node* node, std::vector<std::size_t>& out) const;
    std::vector<std::size_t> search(const Node* node, const std::string& key) const;
};

class Table {
public:
    Table() = default;
    Table(std::string name, std::vector<Column> columns);

    const std::string& name() const { return name_; }
    const std::vector<Column>& columns() const { return columns_; }
    std::size_t columnCount() const { return columns_.size(); }
    int primaryKeyIndex() const { return primaryKeyIndex_; }

    bool insertRow(const std::vector<std::string>& values, std::string& message);
    bool deleteRows(const Condition& condition, std::string& message);
    std::vector<std::vector<std::string>> selectRows(const SelectQuery& query, std::string& message) const;

    std::string saveToFile(const std::filesystem::path& filePath) const;
    static std::optional<Table> loadFromFile(const std::filesystem::path& filePath, std::string& error);

    std::string schemaString() const;

private:
    std::string name_;
    std::vector<Column> columns_;
    int primaryKeyIndex_{-1};
    std::vector<Row> rows_;

    // HashMap index for equality lookup per column.
    std::vector<std::unordered_map<std::string, std::vector<std::size_t>>> hashIndex_;

    // B-tree style ordered index per column for ORDER BY.
    std::vector<std::unique_ptr<BTreeIndex>> orderIndex_;

    void rebuildIndexes();
    std::size_t findColumnIndex(const std::string& colName) const;
    bool rowMatches(const Row& row, const Condition& condition) const;
    static std::string normalizeType(std::string type);
    static bool isNumericType(const std::string& type);
    static bool compareValuesEqual(const std::string& type, const std::string& a, const std::string& b);
    static std::string stripQuotes(std::string s);
    static std::string escapeField(const std::string& s);
    static std::string unescapeField(const std::string& s);
    static std::vector<std::string> splitEscaped(const std::string& s, char delimiter);
    static std::string toUpper(std::string s);
    static std::string trim(std::string s);
    static bool isInteger(const std::string& s);
    static bool isDouble(const std::string& s);
    std::vector<std::size_t> candidateRowIds(const Condition& condition) const;
    std::vector<std::size_t> orderedCandidateRowIds(const OrderBy& orderBy, const std::unordered_set<std::size_t>& allowed) const;
};

class Database {
public:
    explicit Database(std::filesystem::path storageDir = "db_data");

    bool createTable(const CreateTableQuery& query, std::string& message);
    bool insertInto(const InsertQuery& query, std::string& message);
    bool selectFrom(const SelectQuery& query, std::string& message) const;
    bool deleteFrom(const DeleteQuery& query, std::string& message);
    std::string execute(const std::string& sql);

private:
    std::filesystem::path storageDir_;
    std::unordered_map<std::string, Table> tables_;

    static std::string toUpper(std::string s);
    static std::string trim(std::string s);
    static std::string removeTrailingSemicolon(std::string s);
    static std::vector<std::string> splitCSV(const std::string& s);
    static std::optional<CreateTableQuery> parseCreateTable(const std::string& sql, std::string& error);
    static std::optional<InsertQuery> parseInsert(const std::string& sql, std::string& error);
    static std::optional<SelectQuery> parseSelect(const std::string& sql, std::string& error);
    static std::optional<DeleteQuery> parseDelete(const std::string& sql, std::string& error);
    static std::vector<std::string> splitColumns(const std::string& s);
    static std::string unquote(std::string s);

    void loadAllTables();
    void persistTable(const Table& table) const;
    std::filesystem::path tablePath(const std::string& tableName) const;
};

} // namespace minidb
