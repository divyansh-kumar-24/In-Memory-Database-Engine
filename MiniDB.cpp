#include "MiniDB.hpp"

namespace minidb {

// ------------------------- Utility helpers -------------------------

static inline bool startsWithInsensitive(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(s[i])) != std::toupper(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

std::string Table::toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

std::string Table::trim(std::string s) {
    auto notSpace = [](unsigned char ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string Database::toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

std::string Database::trim(std::string s) {
    auto notSpace = [](unsigned char ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string Database::removeTrailingSemicolon(std::string s) {
    s = trim(std::move(s));
    if (!s.empty() && s.back() == ';') s.pop_back();
    return trim(std::move(s));
}

std::string Table::normalizeType(std::string type) {
    return toUpper(trim(std::move(type)));
}

bool Table::isNumericType(const std::string& type) {
    std::string t = normalizeType(type);
    return t == "INT" || t == "INTEGER" || t == "DOUBLE" || t == "FLOAT" || t == "REAL";
}

bool Table::isInteger(const std::string& s) {
    if (s.empty()) return false;
    std::size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i == s.size()) return false;
    for (; i < s.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

bool Table::isDouble(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end && *end == '\0';
}

std::string Table::stripQuotes(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2) {
        if ((s.front() == '\'' && s.back() == '\'') || (s.front() == '"' && s.back() == '"')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

std::string Table::escapeField(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '|': out += "\\p"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string Table::unescapeField(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            switch (n) {
                case '\\': out += '\\'; break;
                case 'p': out += '|'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                default: out += n; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::string> Table::splitEscaped(const std::string& s, char delimiter) {
    std::vector<std::string> parts;
    std::string cur;
    bool escape = false;
    for (char c : s) {
        if (escape) {
            cur += '\\';
            cur += c;
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == delimiter) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (escape) cur += '\\';
    parts.push_back(cur);
    return parts;
}

bool Table::compareValuesEqual(const std::string& type, const std::string& a, const std::string& b) {
    if (isNumericType(type)) {
        // compare numerically when possible; fall back to string.
        if (isInteger(a) && isInteger(b)) return std::stoll(a) == std::stoll(b);
        if (isDouble(a) && isDouble(b)) return std::stod(a) == std::stod(b);
    }
    return a == b;
}


// ------------------------- BTreeIndex -------------------------

BTreeIndex::BTreeIndex(int minimumDegree) : t_(std::max(2, minimumDegree)), root_(std::make_unique<Node>(true)) {}

void BTreeIndex::clear() {
    root_ = std::make_unique<Node>(true);
}

std::vector<std::size_t> BTreeIndex::search(const Node* node, const std::string& key) const {
    if (!node) return {};
    std::size_t i = 0;
    while (i < node->keys.size() && key > node->keys[i]) ++i;
    if (i < node->keys.size() && key == node->keys[i]) return node->payloads[i];
    if (node->leaf) return {};
    return search(node->children[i].get(), key);
}

std::vector<std::size_t> BTreeIndex::search(const std::string& key) const {
    return search(root_.get(), key);
}

void BTreeIndex::splitChild(Node* parent, int index) {
    Node* y = parent->children[index].get();
    auto z = std::make_unique<Node>(y->leaf);

    const int t = t_;

    // Move last t-1 keys and payloads to z.
    z->keys.insert(z->keys.end(),
                   std::make_move_iterator(y->keys.begin() + t),
                   std::make_move_iterator(y->keys.end()));
    z->payloads.insert(z->payloads.end(),
                       std::make_move_iterator(y->payloads.begin() + t),
                       std::make_move_iterator(y->payloads.end()));

    const std::string middleKey = y->keys[t - 1];
    const auto middlePayload = y->payloads[t - 1];

    y->keys.resize(t - 1);
    y->payloads.resize(t - 1);

    if (!y->leaf) {
        z->children.insert(z->children.end(),
                           std::make_move_iterator(y->children.begin() + t),
                           std::make_move_iterator(y->children.end()));
        y->children.resize(t);
    }

    parent->children.insert(parent->children.begin() + index + 1, std::move(z));
    parent->keys.insert(parent->keys.begin() + index, middleKey);
    parent->payloads.insert(parent->payloads.begin() + index, middlePayload);
}

void BTreeIndex::insertNonFull(Node* node, const std::string& key, std::size_t rowId) {
    std::size_t i = node->keys.size();

    if (node->leaf) {
        // If duplicate exists, append rowId.
        auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
        std::size_t pos = static_cast<std::size_t>(std::distance(node->keys.begin(), it));
        if (it != node->keys.end() && *it == key) {
            node->payloads[pos].push_back(rowId);
            return;
        }
        node->keys.insert(it, key);
        node->payloads.insert(node->payloads.begin() + static_cast<std::ptrdiff_t>(pos), std::vector<std::size_t>{rowId});
        return;
    }

    while (i > 0 && key < node->keys[i - 1]) --i;

    if (i < node->keys.size() && node->keys[i] == key) {
        node->payloads[i].push_back(rowId);
        return;
    }

    if (node->children[i]->keys.size() == static_cast<std::size_t>(2 * t_ - 1)) {
        splitChild(node, static_cast<int>(i));
        if (key == node->keys[i]) {
            node->payloads[i].push_back(rowId);
            return;
        } else if (key > node->keys[i]) {
            ++i;
        }
    }

    insertNonFull(node->children[i].get(), key, rowId);
}

void BTreeIndex::insert(const std::string& key, std::size_t rowId) {
    if (root_->keys.size() == static_cast<std::size_t>(2 * t_ - 1)) {
        auto s = std::make_unique<Node>(false);
        s->children.push_back(std::move(root_));
        splitChild(s.get(), 0);

        std::size_t i = 0;
        if (key > s->keys[0]) i = 1;
        else if (key == s->keys[0]) {
            s->payloads[0].push_back(rowId);
            root_ = std::move(s);
            return;
        }

        insertNonFull(s->children[i].get(), key, rowId);
        root_ = std::move(s);
    } else {
        insertNonFull(root_.get(), key, rowId);
    }
}

void BTreeIndex::traverse(const Node* node, std::vector<std::size_t>& out) const {
    if (!node) return;
    for (std::size_t i = 0; i < node->keys.size(); ++i) {
        if (!node->leaf) traverse(node->children[i].get(), out);
        out.insert(out.end(), node->payloads[i].begin(), node->payloads[i].end());
    }
    if (!node->leaf) traverse(node->children[node->keys.size()].get(), out);
}

void BTreeIndex::traverseReverse(const Node* node, std::vector<std::size_t>& out) const {
    if (!node) return;
    if (!node->leaf) traverseReverse(node->children[node->keys.size()].get(), out);
    for (std::size_t idx = node->keys.size(); idx-- > 0;) {
        out.insert(out.end(), node->payloads[idx].rbegin(), node->payloads[idx].rend());
        if (!node->leaf) traverseReverse(node->children[idx].get(), out);
    }
}

std::vector<std::size_t> BTreeIndex::orderedRowIds(bool ascending) const {
    std::vector<std::size_t> out;
    if (ascending) traverse(root_.get(), out);
    else traverseReverse(root_.get(), out);
    return out;
}

// ------------------------- Table -------------------------

Table::Table(std::string name, std::vector<Column> columns)
    : name_(trim(std::move(name))), columns_(std::move(columns)) {
    hashIndex_.resize(columns_.size());
    orderIndex_.resize(columns_.size());

    for (std::size_t i = 0; i < columns_.size(); ++i) {
        columns_[i].name = trim(columns_[i].name);
        columns_[i].type = normalizeType(columns_[i].type);
        if (columns_[i].primaryKey) primaryKeyIndex_ = static_cast<int>(i);
        orderIndex_[i] = std::make_unique<BTreeIndex>(3);
    }
    rebuildIndexes();
}

std::size_t Table::findColumnIndex(const std::string& colName) const {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (toUpper(columns_[i].name) == toUpper(colName)) return i;
    }
    throw std::runtime_error("Unknown column: " + colName);
}

void Table::rebuildIndexes() {
    hashIndex_.assign(columns_.size(), {});
    orderIndex_.clear();
    orderIndex_.resize(columns_.size());
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        orderIndex_[i] = std::make_unique<BTreeIndex>(3);
    }

    for (std::size_t rowId = 0; rowId < rows_.size(); ++rowId) {
        if (rows_[rowId].deleted) continue;
        for (std::size_t col = 0; col < columns_.size(); ++col) {
            const auto& v = rows_[rowId].values[col];
            hashIndex_[col][v].push_back(rowId);
            orderIndex_[col]->insert(v, rowId);
        }
    }
}


bool Table::insertRow(const std::vector<std::string>& values, std::string& message) {
    if (values.size() != columns_.size()) {
        message = "Insert failed: value count does not match column count.";
        return false;
    }

    // Basic type validation and primary-key uniqueness.
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::string v = stripQuotes(values[i]);
        const auto& type = columns_[i].type;
        if (isNumericType(type) && !v.empty()) {
            if (!isInteger(v) && !isDouble(v)) {
                message = "Insert failed: value '" + v + "' does not match numeric column " + columns_[i].name + ".";
                return false;
            }
        }
        if (columns_[i].primaryKey) {
            auto it = hashIndex_[i].find(v);
            if (it != hashIndex_[i].end()) {
                message = "Insert failed: duplicate primary key value '" + v + "'.";
                return false;
            }
        }
    }

    Row row;
    row.values.reserve(values.size());
    for (const auto& v : values) row.values.push_back(stripQuotes(v));

    rows_.push_back(std::move(row));
    rebuildIndexes();
    message = "Row inserted successfully.";
    return true;
}

bool Table::rowMatches(const Row& row, const Condition& condition) const {
    if (!condition.hasCondition) return true;
    std::size_t idx = findColumnIndex(condition.column);
    return compareValuesEqual(columns_[idx].type, row.values[idx], stripQuotes(condition.value));
}

std::vector<std::size_t> Table::candidateRowIds(const Condition& condition) const {
    std::vector<std::size_t> ids;
    if (!condition.hasCondition) {
        for (std::size_t i = 0; i < rows_.size(); ++i) if (!rows_[i].deleted) ids.push_back(i);
        return ids;
    }

    std::size_t idx = findColumnIndex(condition.column);
    std::string key = stripQuotes(condition.value);
    auto it = hashIndex_[idx].find(key);
    if (it != hashIndex_[idx].end()) {
        for (auto rid : it->second) {
            if (rid < rows_.size() && !rows_[rid].deleted) ids.push_back(rid);
        }
        return ids;
    }

    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (!rows_[i].deleted && rowMatches(rows_[i], condition)) ids.push_back(i);
    }
    return ids;
}

std::vector<std::size_t> Table::orderedCandidateRowIds(const OrderBy& orderBy, const std::unordered_set<std::size_t>& allowed) const {
    if (!orderBy.hasOrder) return {};
    std::size_t idx = findColumnIndex(orderBy.column);
    auto ordered = orderIndex_[idx]->orderedRowIds(orderBy.ascending);
    std::vector<std::size_t> out;
    out.reserve(ordered.size());
    for (auto rid : ordered) {
        if (rid < rows_.size() && !rows_[rid].deleted && allowed.find(rid) != allowed.end()) {
            out.push_back(rid);
        }
    }
    return out;
}

bool Table::deleteRows(const Condition& condition, std::string& message) {
    auto ids = candidateRowIds(condition);
    if (ids.empty()) {
        message = "No matching rows found.";
        return true;
    }

    for (auto rid : ids) {
        if (rid < rows_.size()) rows_[rid].deleted = true;
    }
    rebuildIndexes();
    message = std::to_string(ids.size()) + " row(s) deleted.";
    return true;
}

std::vector<std::vector<std::string>> Table::selectRows(const SelectQuery& query, std::string& message) const {
    std::vector<std::vector<std::string>> result;
    try {
        auto candidateIds = candidateRowIds(query.where);
        std::unordered_set<std::size_t> allowed(candidateIds.begin(), candidateIds.end());

        std::vector<std::size_t> orderedIds;
        if (query.orderBy.hasOrder) {
            orderedIds = orderedCandidateRowIds(query.orderBy, allowed);
        } else {
            orderedIds = candidateIds;
        }

        for (auto rid : orderedIds) {
            if (rid >= rows_.size() || rows_[rid].deleted) continue;
            if (!rowMatches(rows_[rid], query.where)) continue;
            result.push_back(rows_[rid].values);
        }

        if (result.empty()) {
            message = "No rows matched.";
        } else {
            message = "Returned " + std::to_string(result.size()) + " row(s).";
        }
    } catch (const std::exception& e) {
        message = e.what();
    }
    return result;
}

std::string Table::schemaString() const {
    std::ostringstream oss;
    oss << "TABLE " << name_ << "\n";
    oss << "COLUMNS " << columns_.size() << "\n";
    for (const auto& c : columns_) {
        oss << c.name << " " << c.type << " " << (c.primaryKey ? "PRIMARY_KEY" : "NORMAL") << "\n";
    }
    return oss.str();
}

std::string Table::saveToFile(const std::filesystem::path& filePath) const {
    std::ofstream out(filePath);
    if (!out) return "Failed to open file for writing: " + filePath.string();

    out << "TABLE " << escapeField(name_) << "\n";
    out << "COLUMNS " << columns_.size() << "\n";
    for (const auto& c : columns_) {
        out << escapeField(c.name) << "|" << escapeField(c.type) << "|" << (c.primaryKey ? "1" : "0") << "\n";
    }
    std::size_t liveCount = 0;
    for (const auto& r : rows_) if (!r.deleted) ++liveCount;
    out << "ROWS " << liveCount << "\n";
    for (const auto& r : rows_) {
        if (r.deleted) continue;
        for (std::size_t i = 0; i < r.values.size(); ++i) {
            if (i) out << "|";
            out << escapeField(r.values[i]);
        }
        out << "\n";
    }
    out << "END\n";
    return {};
}

std::optional<Table> Table::loadFromFile(const std::filesystem::path& filePath, std::string& error) {
    std::ifstream in(filePath);
    if (!in) {
        error = "Failed to open table file: " + filePath.string();
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(in, line)) {
        error = "Empty table file: " + filePath.string();
        return std::nullopt;
    }
    line = trim(line);
    if (!startsWithInsensitive(line, "TABLE ")) {
        error = "Invalid table file header in " + filePath.string();
        return std::nullopt;
    }
    std::string tableName = unescapeField(trim(line.substr(6)));

    if (!std::getline(in, line)) {
        error = "Missing column section in " + filePath.string();
        return std::nullopt;
    }
    line = trim(line);
    if (!startsWithInsensitive(line, "COLUMNS ")) {
        error = "Invalid columns header in " + filePath.string();
        return std::nullopt;
    }

    int colCount = std::stoi(trim(line.substr(8)));
    std::vector<Column> cols;
    cols.reserve(colCount);

    for (int i = 0; i < colCount; ++i) {
        if (!std::getline(in, line)) {
            error = "Unexpected EOF while reading columns.";
            return std::nullopt;
        }
        auto parts = splitEscaped(line, '|');
        if (parts.size() != 3) {
            error = "Invalid column definition in " + filePath.string();
            return std::nullopt;
        }
        Column c;
        c.name = unescapeField(parts[0]);
        c.type = normalizeType(unescapeField(parts[1]));
        c.primaryKey = (trim(parts[2]) == "1");
        cols.push_back(std::move(c));
    }

    if (!std::getline(in, line)) {
        error = "Missing rows header.";
        return std::nullopt;
    }
    line = trim(line);
    if (!startsWithInsensitive(line, "ROWS ")) {
        error = "Invalid rows header in " + filePath.string();
        return std::nullopt;
    }
    int rowCount = std::stoi(trim(line.substr(5)));

    Table table(tableName, cols);
    table.rows_.clear();

    for (int i = 0; i < rowCount; ++i) {
        if (!std::getline(in, line)) {
            error = "Unexpected EOF while reading rows.";
            return std::nullopt;
        }
        auto parts = splitEscaped(line, '|');
        if (static_cast<int>(parts.size()) != colCount) {
            error = "Row value count mismatch in " + filePath.string();
            return std::nullopt;
        }
        Row r;
        for (auto& p : parts) r.values.push_back(unescapeField(p));
        table.rows_.push_back(std::move(r));
    }

    // consume END if present
    table.rebuildIndexes();
    return table;
}

// ------------------------- Database -------------------------

Database::Database(std::filesystem::path storageDir) : storageDir_(std::move(storageDir)) {
    std::filesystem::create_directories(storageDir_);
    loadAllTables();
}

std::filesystem::path Database::tablePath(const std::string& tableName) const {
    return storageDir_ / (tableName + ".tbl");
}

void Database::persistTable(const Table& table) const {
    auto err = table.saveToFile(tablePath(table.name()));
    if (!err.empty()) {
        std::cerr << err << '\n';
    }
}

void Database::loadAllTables() {
    tables_.clear();
    if (!std::filesystem::exists(storageDir_)) return;

    for (const auto& entry : std::filesystem::directory_iterator(storageDir_)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".tbl") continue;
        std::string error;
        auto tableOpt = Table::loadFromFile(entry.path(), error);
        if (tableOpt) {
            tables_.emplace(tableOpt->name(), std::move(*tableOpt));
        } else {
            std::cerr << "Load warning: " << error << '\n';
        }
    }
}

std::vector<std::string> Database::splitCSV(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    bool inQuote = false;
    char quoteChar = '\0';
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (inQuote) {
            if (c == '\\' && i + 1 < s.size()) {
                cur += c;
                cur += s[++i];
            } else if (c == quoteChar) {
                inQuote = false;
                cur += c;
            } else {
                cur += c;
            }
        } else {
            if (c == '\'' || c == '"') {
                inQuote = true;
                quoteChar = c;
                cur += c;
            } else if (c == ',') {
                parts.push_back(trim(cur));
                cur.clear();
            } else {
                cur += c;
            }
        }
    }
    if (!cur.empty() || (!s.empty() && s.back() == ',')) parts.push_back(trim(cur));
    return parts;
}

std::vector<std::string> Database::splitColumns(const std::string& s) {
    return splitCSV(s);
}

std::string Database::unquote(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2) {
        if ((s.front() == '\'' && s.back() == '\'') || (s.front() == '"' && s.back() == '"')) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return s;
}

std::optional<CreateTableQuery> Database::parseCreateTable(const std::string& sql, std::string& error) {
    std::string s = removeTrailingSemicolon(sql);
    std::string upper = toUpper(s);
    const std::string prefix = "CREATE TABLE ";
    if (!startsWithInsensitive(upper, prefix)) {
        error = "Invalid CREATE TABLE syntax.";
        return std::nullopt;
    }

    std::size_t lparen = s.find('(', prefix.size());
    std::size_t rparen = s.rfind(')');
    if (lparen == std::string::npos || rparen == std::string::npos || rparen < lparen) {
        error = "CREATE TABLE must contain column definitions in parentheses.";
        return std::nullopt;
    }

    CreateTableQuery q;
    q.tableName = trim(s.substr(prefix.size(), lparen - prefix.size()));
    if (q.tableName.empty()) {
        error = "Table name cannot be empty.";
        return std::nullopt;
    }

    std::string colsPart = s.substr(lparen + 1, rparen - lparen - 1);
    auto defs = splitCSV(colsPart);
    if (defs.empty()) {
        error = "At least one column is required.";
        return std::nullopt;
    }

    for (auto def : defs) {
        def = trim(def);
        std::istringstream iss(def);
        Column c;
        if (!(iss >> c.name >> c.type)) {
            error = "Invalid column definition: " + def;
            return std::nullopt;
        }
        std::string rest;
        std::getline(iss, rest);
        std::string restUpper = toUpper(rest);
        c.primaryKey = restUpper.find("PRIMARY KEY") != std::string::npos || restUpper.find("PRIMARY_KEY") != std::string::npos;
        c.type = toUpper(trim(c.type));
        q.columns.push_back(std::move(c));
    }

    return q;
}

std::optional<InsertQuery> Database::parseInsert(const std::string& sql, std::string& error) {
    std::string s = removeTrailingSemicolon(sql);
    std::string upper = toUpper(s);
    const std::string prefix = "INSERT INTO ";
    if (!startsWithInsensitive(upper, prefix)) {
        error = "Invalid INSERT syntax.";
        return std::nullopt;
    }

    std::size_t valuesPos = upper.find(" VALUES ", prefix.size());
    if (valuesPos == std::string::npos) {
        error = "INSERT must contain VALUES.";
        return std::nullopt;
    }

    InsertQuery q;
    q.tableName = trim(s.substr(prefix.size(), valuesPos - prefix.size()));
    std::size_t lparen = s.find('(', valuesPos);
    std::size_t rparen = s.rfind(')');
    if (lparen == std::string::npos || rparen == std::string::npos || rparen < lparen) {
        error = "INSERT VALUES must be in parentheses.";
        return std::nullopt;
    }

    std::string valsPart = s.substr(lparen + 1, rparen - lparen - 1);
    q.values = splitCSV(valsPart);
    return q;
}

std::optional<SelectQuery> Database::parseSelect(const std::string& sql, std::string& error) {
    std::string s = removeTrailingSemicolon(sql);
    std::string upper = toUpper(s);

    if (!startsWithInsensitive(upper, "SELECT ")) {
        error = "Invalid SELECT syntax.";
        return std::nullopt;
    }

    std::size_t fromPos = upper.find(" FROM ");
    if (fromPos == std::string::npos) {
        error = "SELECT must contain FROM.";
        return std::nullopt;
    }

    SelectQuery q;
    std::string projection = trim(s.substr(7, fromPos - 7));
    if (projection != "*") {
        q.columns = splitColumns(projection);
        for (auto& c : q.columns) c = trim(c);
    }

    std::size_t wherePos = upper.find(" WHERE ", fromPos + 6);
    std::size_t orderPos = upper.find(" ORDER BY ", fromPos + 6);

    std::size_t tableEnd = std::min(wherePos == std::string::npos ? s.size() : wherePos,
                                    orderPos == std::string::npos ? s.size() : orderPos);
    q.tableName = trim(s.substr(fromPos + 6, tableEnd - (fromPos + 6)));

    if (wherePos != std::string::npos) {
        std::size_t whereEnd = (orderPos != std::string::npos && orderPos > wherePos) ? orderPos : s.size();
        std::string whereClause = trim(s.substr(wherePos + 7, whereEnd - (wherePos + 7)));
        std::size_t eq = whereClause.find('=');
        if (eq == std::string::npos) {
            error = "WHERE clause must be of form column = value.";
            return std::nullopt;
        }
        q.where.hasCondition = true;
        q.where.column = trim(whereClause.substr(0, eq));
        q.where.value = trim(whereClause.substr(eq + 1));
    }

    if (orderPos != std::string::npos) {
        std::string orderClause = trim(s.substr(orderPos + 10));
        std::istringstream iss(orderClause);
        iss >> q.orderBy.column;
        std::string dir;
        if (iss >> dir) {
            dir = toUpper(dir);
            q.orderBy.ascending = !(dir == "DESC");
        } else {
            q.orderBy.ascending = true;
        }
        q.orderBy.hasOrder = true;
    }

    return q;
}

std::optional<DeleteQuery> Database::parseDelete(const std::string& sql, std::string& error) {
    std::string s = removeTrailingSemicolon(sql);
    std::string upper = toUpper(s);

    if (!startsWithInsensitive(upper, "DELETE FROM ")) {
        error = "Invalid DELETE syntax.";
        return std::nullopt;
    }

    DeleteQuery q;
    std::size_t wherePos = upper.find(" WHERE ");
    if (wherePos == std::string::npos) {
        q.tableName = trim(s.substr(12));
        q.where.hasCondition = false;
        return q;
    }

    q.tableName = trim(s.substr(12, wherePos - 12));
    std::string whereClause = trim(s.substr(wherePos + 7));
    std::size_t eq = whereClause.find('=');
    if (eq == std::string::npos) {
        error = "WHERE clause must be of form column = value.";
        return std::nullopt;
    }
    q.where.hasCondition = true;
    q.where.column = trim(whereClause.substr(0, eq));
    q.where.value = trim(whereClause.substr(eq + 1));
    return q;
}

bool Database::createTable(const CreateTableQuery& query, std::string& message) {
    if (tables_.find(query.tableName) != tables_.end()) {
        message = "Table already exists: " + query.tableName;
        return false;
    }
    if (query.columns.empty()) {
        message = "Table must have at least one column.";
        return false;
    }

    int pkCount = 0;
    for (const auto& c : query.columns) if (c.primaryKey) ++pkCount;
    if (pkCount > 1) {
        message = "Only one primary key column is allowed.";
        return false;
    }

    tables_.emplace(query.tableName, Table(query.tableName, query.columns));
    persistTable(tables_.at(query.tableName));
    message = "Table created: " + query.tableName;
    return true;
}

bool Database::insertInto(const InsertQuery& query, std::string& message) {
    auto it = tables_.find(query.tableName);
    if (it == tables_.end()) {
        message = "Unknown table: " + query.tableName;
        return false;
    }
    bool ok = it->second.insertRow(query.values, message);
    if (ok) persistTable(it->second);
    return ok;
}

bool Database::selectFrom(const SelectQuery& query, std::string& message) const {
    auto it = tables_.find(query.tableName);
    if (it == tables_.end()) {
        message = "Unknown table: " + query.tableName;
        return false;
    }

    auto rows = it->second.selectRows(query, message);
    if (rows.empty()) {
        std::cout << message << '\n';
        return true;
    }

    const auto& cols = it->second.columns();
    std::vector<std::string> selectedColumns = query.columns;
    if (selectedColumns.empty()) {
        for (const auto& c : cols) selectedColumns.push_back(c.name);
    }

    std::cout << "=== " << query.tableName << " ===\n";
    // print header
    for (std::size_t i = 0; i < selectedColumns.size(); ++i) {
        if (i) std::cout << " | ";
        std::cout << selectedColumns[i];
    }
    std::cout << "\n";

    for (const auto& row : rows) {
        std::vector<std::size_t> indices;
        for (const auto& colName : selectedColumns) {
            auto itc = std::find_if(cols.begin(), cols.end(), [&](const Column& c){ return toUpper(c.name) == toUpper(colName); });
            if (itc == cols.end()) continue;
            indices.push_back(static_cast<std::size_t>(std::distance(cols.begin(), itc)));
        }
        for (std::size_t i = 0; i < indices.size(); ++i) {
            if (i) std::cout << " | ";
            std::cout << row[indices[i]];
        }
        std::cout << "\n";
    }
    std::cout << message << '\n';
    return true;
}

bool Database::deleteFrom(const DeleteQuery& query, std::string& message) {
    auto it = tables_.find(query.tableName);
    if (it == tables_.end()) {
        message = "Unknown table: " + query.tableName;
        return false;
    }
    bool ok = it->second.deleteRows(query.where, message);
    if (ok) persistTable(it->second);
    return ok;
}

std::string Database::execute(const std::string& sql) {
    std::string s = removeTrailingSemicolon(sql);
    if (s.empty()) return {};

    std::string upper = toUpper(trim(s));
    std::string message;

    if (startsWithInsensitive(upper, "CREATE TABLE ")) {
        auto q = parseCreateTable(s, message);
        if (!q) return "Error: " + message;
        if (!createTable(*q, message)) return "Error: " + message;
        return message;
    }
    if (startsWithInsensitive(upper, "INSERT INTO ")) {
        auto q = parseInsert(s, message);
        if (!q) return "Error: " + message;
        if (!insertInto(*q, message)) return "Error: " + message;
        return message;
    }
    if (startsWithInsensitive(upper, "SELECT ")) {
        auto q = parseSelect(s, message);
        if (!q) return "Error: " + message;
        if (!selectFrom(*q, message)) return "Error: " + message;
        return {};
    }
    if (startsWithInsensitive(upper, "DELETE FROM ")) {
        auto q = parseDelete(s, message);
        if (!q) return "Error: " + message;
        if (!deleteFrom(*q, message)) return "Error: " + message;
        return message;
    }
    if (startsWithInsensitive(upper, "SHOW TABLES")) {
        std::ostringstream oss;
        for (const auto& [name, table] : tables_) {
            (void)table;
            oss << name << '\n';
        }
        return oss.str();
    }

    if (startsWithInsensitive(upper, "DESCRIBE ")) {
        std::string tableName = trim(s.substr(9));
        auto it = tables_.find(tableName);
        if (it == tables_.end()) return "Error: Unknown table: " + tableName;
        return it->second.schemaString();
    }

    return "Error: Unknown command.";
}

} // namespace minidb
