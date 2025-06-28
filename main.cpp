#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <regex>
#include <variant>
#include <functional>
#include <filesystem>

// Forward declarations
class Table;
class Database;
class QueryParser;

// Data types supported by the database
enum class DataType {
    INTEGER,
    TEXT,
    REAL,
    BOOLEAN
};

// Value wrapper for different data types
class Value {
public:
    std::variant<int, std::string, double, bool> data;
    DataType type;

    Value(int val) : data(val), type(DataType::INTEGER) {}
    Value(const std::string& val) : data(val), type(DataType::TEXT) {}
    Value(double val) : data(val), type(DataType::REAL) {}
    Value(bool val) : data(val), type(DataType::BOOLEAN) {}

    std::string toString() const {
        switch (type) {
            case DataType::INTEGER:
                return std::to_string(std::get<int>(data));
            case DataType::TEXT:
                return std::get<std::string>(data);
            case DataType::REAL:
                return std::to_string(std::get<double>(data));
            case DataType::BOOLEAN:
                return std::get<bool>(data) ? "true" : "false";
        }
        return "";
    }

    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        return data == other.data;
    }

    bool operator<(const Value& other) const {
        if (type != other.type) return false;
        return data < other.data;
    }
};

// Column definition
struct Column {
    std::string name;
    DataType type;
    bool primaryKey = false;
    bool notNull = false;
    bool autoIncrement = false;

    Column(const std::string& n, DataType t) : name(n), type(t) {}
};

// Row represents a single record
class Row {
public:
    std::vector<Value> values;
    
    Row(const std::vector<Value>& vals) : values(vals) {}
    
    Value& operator[](size_t index) {
        return values[index];
    }
    
    const Value& operator[](size_t index) const {
        return values[index];
    }
    
    size_t size() const {
        return values.size();
    }
};

// Simple B-Tree index for fast lookups
class BTreeIndex {
private:
    std::map<Value, std::vector<size_t>> index;
    
public:
    void insert(const Value& key, size_t rowIndex) {
        index[key].push_back(rowIndex);
    }
    
    void remove(const Value& key, size_t rowIndex) {
        auto it = index.find(key);
        if (it != index.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), rowIndex), vec.end());
            if (vec.empty()) {
                index.erase(it);
            }
        }
    }
    
    std::vector<size_t> find(const Value& key) const {
        auto it = index.find(key);
        return it != index.end() ? it->second : std::vector<size_t>();
    }
    
    void clear() {
        index.clear();
    }
};

// Table class
class Table {
private:
    std::string name;
    std::vector<Column> columns;
    std::vector<Row> rows;
    std::unordered_map<std::string, size_t> columnMap;
    std::unordered_map<std::string, std::unique_ptr<BTreeIndex>> indexes;
    size_t nextAutoIncrement = 1;

public:
    Table(const std::string& tableName) : name(tableName) {}

    void addColumn(const Column& column) {
        columnMap[column.name] = columns.size();
        columns.push_back(column);
        
        // Create index for primary key
        if (column.primaryKey) {
            indexes[column.name] = std::make_unique<BTreeIndex>();
        }
    }

    bool insertRow(const std::vector<Value>& values) {
        if (values.size() != columns.size()) {
            return false;
        }

        // Check constraints
        for (size_t i = 0; i < columns.size(); i++) {
            if (columns[i].notNull && values[i].toString().empty()) {
                return false;
            }
        }

        // Handle auto increment
        std::vector<Value> rowValues = values;
        for (size_t i = 0; i < columns.size(); i++) {
            if (columns[i].autoIncrement) {
                rowValues[i] = Value(static_cast<int>(nextAutoIncrement++));
            }
        }

        // Update indexes
        for (size_t i = 0; i < columns.size(); i++) {
            if (indexes.find(columns[i].name) != indexes.end()) {
                indexes[columns[i].name]->insert(rowValues[i], rows.size());
            }
        }

        rows.emplace_back(rowValues);
        return true;
    }

    std::vector<Row> selectAll() const {
        return rows;
    }

    std::vector<Row> selectWhere(const std::string& columnName, const Value& value) const {
        std::vector<Row> result;
        
        // Use index if available
        if (indexes.find(columnName) != indexes.end()) {
            auto rowIndexes = indexes.at(columnName)->find(value);
            for (size_t idx : rowIndexes) {
                if (idx < rows.size()) {
                    result.push_back(rows[idx]);
                }
            }
        } else {
            // Linear search
            auto colIt = columnMap.find(columnName);
            if (colIt != columnMap.end()) {
                size_t colIndex = colIt->second;
                for (const auto& row : rows) {
                    if (row[colIndex] == value) {
                        result.push_back(row);
                    }
                }
            }
        }
        
        return result;
    }

    bool deleteWhere(const std::string& columnName, const Value& value) {
        auto colIt = columnMap.find(columnName);
        if (colIt == columnMap.end()) return false;
        
        size_t colIndex = colIt->second;
        bool deleted = false;
        
        for (auto it = rows.begin(); it != rows.end();) {
            if ((*it)[colIndex] == value) {
                // Update indexes
                for (size_t i = 0; i < columns.size(); i++) {
                    if (indexes.find(columns[i].name) != indexes.end()) {
                        size_t rowIdx = std::distance(rows.begin(), it);
                        indexes[columns[i].name]->remove((*it)[i], rowIdx);
                    }
                }
                it = rows.erase(it);
                deleted = true;
            } else {
                ++it;
            }
        }
        
        return deleted;
    }

    const std::vector<Column>& getColumns() const {
        return columns;
    }

    const std::string& getName() const {
        return name;
    }

    size_t getRowCount() const {
        return rows.size();
    }

    // Persistence methods
    bool saveToFile(const std::string& filename) const {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;

        // Write table name
        size_t nameLen = name.length();
        file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        file.write(name.c_str(), nameLen);

        // Write columns
        size_t colCount = columns.size();
        file.write(reinterpret_cast<const char*>(&colCount), sizeof(colCount));
        
        for (const auto& col : columns) {
            size_t colNameLen = col.name.length();
            file.write(reinterpret_cast<const char*>(&colNameLen), sizeof(colNameLen));
            file.write(col.name.c_str(), colNameLen);
            file.write(reinterpret_cast<const char*>(&col.type), sizeof(col.type));
            file.write(reinterpret_cast<const char*>(&col.primaryKey), sizeof(col.primaryKey));
            file.write(reinterpret_cast<const char*>(&col.notNull), sizeof(col.notNull));
            file.write(reinterpret_cast<const char*>(&col.autoIncrement), sizeof(col.autoIncrement));
        }

        // Write rows
        size_t rowCount = rows.size();
        file.write(reinterpret_cast<const char*>(&rowCount), sizeof(rowCount));
        
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); i++) {
                const Value& val = row[i];
                file.write(reinterpret_cast<const char*>(&val.type), sizeof(val.type));
                
                switch (val.type) {
                    case DataType::INTEGER: {
                        int intVal = std::get<int>(val.data);
                        file.write(reinterpret_cast<const char*>(&intVal), sizeof(intVal));
                        break;
                    }
                    case DataType::TEXT: {
                        std::string strVal = std::get<std::string>(val.data);
                        size_t strLen = strVal.length();
                        file.write(reinterpret_cast<const char*>(&strLen), sizeof(strLen));
                        file.write(strVal.c_str(), strLen);
                        break;
                    }
                    case DataType::REAL: {
                        double realVal = std::get<double>(val.data);
                        file.write(reinterpret_cast<const char*>(&realVal), sizeof(realVal));
                        break;
                    }
                    case DataType::BOOLEAN: {
                        bool boolVal = std::get<bool>(val.data);
                        file.write(reinterpret_cast<const char*>(&boolVal), sizeof(boolVal));
                        break;
                    }
                }
            }
        }

        return true;
    }

    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;

        // Clear existing data
        columns.clear();
        rows.clear();
        columnMap.clear();
        indexes.clear();

        // Read table name
        size_t nameLen;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        name.resize(nameLen);
        file.read(&name[0], nameLen);

        // Read columns
        size_t colCount;
        file.read(reinterpret_cast<char*>(&colCount), sizeof(colCount));
        
        for (size_t i = 0; i < colCount; i++) {
            size_t colNameLen;
            file.read(reinterpret_cast<char*>(&colNameLen), sizeof(colNameLen));
            std::string colName(colNameLen, '\0');
            file.read(&colName[0], colNameLen);
            
            DataType type;
            bool primaryKey, notNull, autoIncrement;
            file.read(reinterpret_cast<char*>(&type), sizeof(type));
            file.read(reinterpret_cast<char*>(&primaryKey), sizeof(primaryKey));
            file.read(reinterpret_cast<char*>(&notNull), sizeof(notNull));
            file.read(reinterpret_cast<char*>(&autoIncrement), sizeof(autoIncrement));
            
            Column col(colName, type);
            col.primaryKey = primaryKey;
            col.notNull = notNull;
            col.autoIncrement = autoIncrement;
            addColumn(col);
        }

        // Read rows
        size_t rowCount;
        file.read(reinterpret_cast<char*>(&rowCount), sizeof(rowCount));
        
        for (size_t i = 0; i < rowCount; i++) {
            std::vector<Value> values;
            
            for (size_t j = 0; j < columns.size(); j++) {
                DataType type;
                file.read(reinterpret_cast<char*>(&type), sizeof(type));
                
                switch (type) {
                    case DataType::INTEGER: {
                        int intVal;
                        file.read(reinterpret_cast<char*>(&intVal), sizeof(intVal));
                        values.emplace_back(intVal);
                        break;
                    }
                    case DataType::TEXT: {
                        size_t strLen;
                        file.read(reinterpret_cast<char*>(&strLen), sizeof(strLen));
                        std::string strVal(strLen, '\0');
                        file.read(&strVal[0], strLen);
                        values.emplace_back(strVal);
                        break;
                    }
                    case DataType::REAL: {
                        double realVal;
                        file.read(reinterpret_cast<char*>(&realVal), sizeof(realVal));
                        values.emplace_back(realVal);
                        break;
                    }
                    case DataType::BOOLEAN: {
                        bool boolVal;
                        file.read(reinterpret_cast<char*>(&boolVal), sizeof(boolVal));
                        values.emplace_back(boolVal);
                        break;
                    }
                }
            }
            
            rows.emplace_back(values);
        }

        return true;
    }
};

// Database class
class Database {
private:
    std::string dbName;
    std::unordered_map<std::string, std::unique_ptr<Table>> tables;
    std::string dataDir;

public:
    Database(const std::string& name) : dbName(name) {
        dataDir = "data/" + dbName;
        std::filesystem::create_directories(dataDir);
    }

    bool createTable(const std::string& tableName, const std::vector<Column>& columns) {
        if (tables.find(tableName) != tables.end()) {
            return false; // Table already exists
        }

        auto table = std::make_unique<Table>(tableName);
        for (const auto& col : columns) {
            table->addColumn(col);
        }

        tables[tableName] = std::move(table);
        return true;
    }

    Table* getTable(const std::string& tableName) {
        auto it = tables.find(tableName);
        return it != tables.end() ? it->second.get() : nullptr;
    }

    bool dropTable(const std::string& tableName) {
        auto it = tables.find(tableName);
        if (it != tables.end()) {
            // Remove file
            std::string filename = dataDir + "/" + tableName + ".tbl";
            std::filesystem::remove(filename);
            
            tables.erase(it);
            return true;
        }
        return false;
    }

    std::vector<std::string> listTables() const {
        std::vector<std::string> tableNames;
        for (const auto& pair : tables) {
            tableNames.push_back(pair.first);
        }
        return tableNames;
    }

    bool saveToFile() {
        for (const auto& pair : tables) {
            std::string filename = dataDir + "/" + pair.first + ".tbl";
            if (!pair.second->saveToFile(filename)) {
                return false;
            }
        }
        return true;
    }

    bool loadFromFile() {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(dataDir)) {
                if (entry.path().extension() == ".tbl") {
                    std::string tableName = entry.path().stem().string();
                    auto table = std::make_unique<Table>(tableName);
                    
                    if (table->loadFromFile(entry.path().string())) {
                        tables[tableName] = std::move(table);
                    }
                }
            }
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
};

// SQL Query Parser
class QueryParser {
private:
    std::string query;
    std::vector<std::string> tokens;
    size_t currentToken = 0;

    void tokenize() {
        tokens.clear();
        std::regex tokenRegex(R"([a-zA-Z_][a-zA-Z0-9_]*|'[^']*'|"[^"]*"|\d+\.?\d*|[(),;=<>!]+|[+\-*/])");
        std::sregex_iterator iter(query.begin(), query.end(), tokenRegex);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            tokens.push_back(iter->str());
        }
    }

    std::string getCurrentToken() const {
        return currentToken < tokens.size() ? tokens[currentToken] : "";
    }

    void consumeToken() {
        if (currentToken < tokens.size()) {
            currentToken++;
        }
    }

    bool expectToken(const std::string& expected) {
        if (getCurrentToken() == expected) {
            consumeToken();
            return true;
        }
        return false;
    }

    DataType parseDataType(const std::string& typeStr) {
        std::string upper = typeStr;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if (upper == "INTEGER" || upper == "INT") return DataType::INTEGER;
        if (upper == "TEXT" || upper == "VARCHAR") return DataType::TEXT;
        if (upper == "REAL" || upper == "DOUBLE") return DataType::REAL;
        if (upper == "BOOLEAN" || upper == "BOOL") return DataType::BOOLEAN;
        
        return DataType::TEXT; // Default
    }

    Value parseValue(const std::string& valueStr) {
        // Remove quotes if present
        if ((valueStr.front() == '\'' && valueStr.back() == '\'') ||
            (valueStr.front() == '"' && valueStr.back() == '"')) {
            return Value(valueStr.substr(1, valueStr.length() - 2));
        }
        
        // Try to parse as number
        if (valueStr.find('.') != std::string::npos) {
            try {
                return Value(std::stod(valueStr));
            } catch (...) {}
        } else {
            try {
                return Value(std::stoi(valueStr));
            } catch (...) {}
        }
        
        // Parse as boolean
        if (valueStr == "true" || valueStr == "TRUE") return Value(true);
        if (valueStr == "false" || valueStr == "FALSE") return Value(false);
        
        return Value(valueStr);
    }

public:
    QueryParser(const std::string& sql) : query(sql), currentToken(0) {
        tokenize();
    }

    bool parseCreateTable(Database& db, std::string& tableName, std::vector<Column>& columns) {
        if (!expectToken("CREATE")) return false;
        if (!expectToken("TABLE")) return false;
        
        tableName = getCurrentToken();
        consumeToken();
        
        if (!expectToken("(")) return false;
        
        while (getCurrentToken() != ")" && currentToken < tokens.size()) {
            std::string colName = getCurrentToken();
            consumeToken();
            
            std::string typeStr = getCurrentToken();
            consumeToken();
            
            Column col(colName, parseDataType(typeStr));
            
            // Check for constraints
            while (getCurrentToken() != "," && getCurrentToken() != ")" && currentToken < tokens.size()) {
                std::string constraint = getCurrentToken();
                std::transform(constraint.begin(), constraint.end(), constraint.begin(), ::toupper);
                
                if (constraint == "PRIMARY") {
                    consumeToken();
                    if (expectToken("KEY")) {
                        col.primaryKey = true;
                    }
                } else if (constraint == "NOT") {
                    consumeToken();
                    if (expectToken("NULL")) {
                        col.notNull = true;
                    }
                } else if (constraint == "AUTO_INCREMENT" || constraint == "AUTOINCREMENT") {
                    col.autoIncrement = true;
                    consumeToken();
                } else {
                    consumeToken();
                }
            }
            
            columns.push_back(col);
            
            if (getCurrentToken() == ",") {
                consumeToken();
            }
        }
        
        return expectToken(")");
    }

    bool parseInsert(Database& db, std::string& tableName, std::vector<Value>& values) {
        if (!expectToken("INSERT")) return false;
        if (!expectToken("INTO")) return false;
        
        tableName = getCurrentToken();
        consumeToken();
        
        if (!expectToken("VALUES")) return false;
        if (!expectToken("(")) return false;
        
        while (getCurrentToken() != ")" && currentToken < tokens.size()) {
            values.push_back(parseValue(getCurrentToken()));
            consumeToken();
            
            if (getCurrentToken() == ",") {
                consumeToken();
            }
        }
        
        return expectToken(")");
    }

    bool parseSelect(Database& db, std::string& tableName, std::string& whereColumn, Value& whereValue, bool& hasWhere) {
        if (!expectToken("SELECT")) return false;
        
        // Skip column list for now (assume SELECT *)
        while (getCurrentToken() != "FROM" && currentToken < tokens.size()) {
            consumeToken();
        }
        
        if (!expectToken("FROM")) return false;
        
        tableName = getCurrentToken();
        consumeToken();
        
        hasWhere = false;
        if (getCurrentToken() == "WHERE") {
            consumeToken();
            whereColumn = getCurrentToken();
            consumeToken();
            
            if (expectToken("=")) {
                whereValue = parseValue(getCurrentToken());
                consumeToken();
                hasWhere = true;
            }
        }
        
        return true;
    }

    bool parseDelete(Database& db, std::string& tableName, std::string& whereColumn, Value& whereValue) {
        if (!expectToken("DELETE")) return false;
        if (!expectToken("FROM")) return false;
        
        tableName = getCurrentToken();
        consumeToken();
        
        if (!expectToken("WHERE")) return false;
        
        whereColumn = getCurrentToken();
        consumeToken();
        
        if (!expectToken("=")) return false;
        
        whereValue = parseValue(getCurrentToken());
        consumeToken();
        
        return true;
    }
};

// Database Engine
class DatabaseEngine {
private:
    std::unique_ptr<Database> currentDb;

public:
    bool createDatabase(const std::string& dbName) {
        currentDb = std::make_unique<Database>(dbName);
        return true;
    }

    bool openDatabase(const std::string& dbName) {
        currentDb = std::make_unique<Database>(dbName);
        return currentDb->loadFromFile();
    }

    bool saveDatabase() {
        return currentDb ? currentDb->saveToFile() : false;
    }

    std::string executeQuery(const std::string& query) {
        if (!currentDb) {
            return "Error: No database selected";
        }

        QueryParser parser(query);
        std::stringstream result;

        // Determine query type
        std::string queryUpper = query;
        std::transform(queryUpper.begin(), queryUpper.end(), queryUpper.begin(), ::toupper);

        if (queryUpper.find("CREATE TABLE") == 0) {
            std::string tableName;
            std::vector<Column> columns;
            
            if (parser.parseCreateTable(*currentDb, tableName, columns)) {
                if (currentDb->createTable(tableName, columns)) {
                    result << "Table '" << tableName << "' created successfully";
                } else {
                    result << "Error: Table '" << tableName << "' already exists";
                }
            } else {
                result << "Error: Invalid CREATE TABLE syntax";
            }
        }
        else if (queryUpper.find("INSERT INTO") == 0) {
            std::string tableName;
            std::vector<Value> values;
            
            if (parser.parseInsert(*currentDb, tableName, values)) {
                Table* table = currentDb->getTable(tableName);
                if (table) {
                    if (table->insertRow(values)) {
                        result << "Row inserted successfully";
                    } else {
                        result << "Error: Failed to insert row";
                    }
                } else {
                    result << "Error: Table '" << tableName << "' not found";
                }
            } else {
                result << "Error: Invalid INSERT syntax";
            }
        }
        else if (queryUpper.find("SELECT") == 0) {
            std::string tableName, whereColumn;
            Value whereValue("");
            bool hasWhere;
            
            if (parser.parseSelect(*currentDb, tableName, whereColumn, whereValue, hasWhere)) {
                Table* table = currentDb->getTable(tableName);
                if (table) {
                    std::vector<Row> rows;
                    
                    if (hasWhere) {
                        rows = table->selectWhere(whereColumn, whereValue);
                    } else {
                        rows = table->selectAll();
                    }
                    
                    // Format output
                    const auto& columns = table->getColumns();
                    
                    // Headers
                    for (size_t i = 0; i < columns.size(); i++) {
                        if (i > 0) result << "\t";
                        result << columns[i].name;
                    }
                    result << "\n";
                    
                    // Data
                    for (const auto& row : rows) {
                        for (size_t i = 0; i < row.size(); i++) {
                            if (i > 0) result << "\t";
                            result << row[i].toString();
                        }
                        result << "\n";
                    }
                    
                    result << "\n" << rows.size() << " rows returned";
                } else {
                    result << "Error: Table '" << tableName << "' not found";
                }
            } else {
                result << "Error: Invalid SELECT syntax";
            }
        }
        else if (queryUpper.find("DELETE FROM") == 0) {
            std::string tableName, whereColumn;
            Value whereValue("");
            
            if (parser.parseDelete(*currentDb, tableName, whereColumn, whereValue)) {
                Table* table = currentDb->getTable(tableName);
                if (table) {
                    if (table->deleteWhere(whereColumn, whereValue)) {
                        result << "Rows deleted successfully";
                    } else {
                        result << "No rows matched the condition";
                    }
                } else {
                    result << "Error: Table '" << tableName << "' not found";
                }
            } else {
                result << "Error: Invalid DELETE syntax";
            }
        }
        else if (queryUpper.find("SHOW TABLES") == 0) {
            auto tables = currentDb->listTables();
            result << "Tables:\n";
            for (const auto& table : tables) {
                result << table << "\n";
            }
        }
        else {
            result << "Error: Unsupported query type";
        }

        return result.str();
    }

    void showHelp() {
        std::cout << "\n=== Simple Database Engine Help ===\n";
        std::cout << "Commands:\n";
        std::cout << "  CREATE DATABASE <name>     - Create new database\n";
        std::cout << "  OPEN DATABASE <name>       - Open existing database\n";
        std::cout << "  SAVE                       - Save current database\n";
        std::cout << "  HELP                       - Show this help\n";
        std::cout << "  EXIT                       - Exit the program\n\n";
        std::cout << "SQL Commands:\n";
        std::cout << "  CREATE TABLE <name> (<columns>)\n";
        std::cout << "  INSERT INTO <table> VALUES (<values>)\n";
        std::cout << "  SELECT * FROM <table> [WHERE <column> = <value>]\n";
        std::cout << "  DELETE FROM <table> WHERE <column> = <value>\n";
        std::cout << "  SHOW TABLES\n\n";
        std::cout << "Example:\n";
        std::cout << "  CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL)\n";
        std::cout << "  INSERT INTO users VALUES (1, 'John Doe')\n";
        std::cout << "  SELECT * FROM users WHERE id = 1\n";
        std::cout << "===================================\n\n";
    }
};

// Main application
int main() {
    DatabaseEngine engine;
    std::string input;

    std::cout << "=== Simple Relational Database Engine ===\n";
    std::cout << "Type 'HELP' for commands or 'EXIT' to quit\n\n";

    while (true) {
        std::cout << "db> ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        // Convert to uppercase for command checking
        std::string upperInput = input;
        std::transform(upperInput.begin(), upperInput.end(), upperInput.begin(), ::toupper);

        if (upperInput == "EXIT" || upperInput == "QUIT") {
            break;
        }
        else if (upperInput == "HELP") {
            engine.showHelp();
        }
        else if (upperInput == "SAVE") {
            if (engine.saveDatabase()) {
                std::cout << "Database saved successfully\n";
            } else {
                std::cout << "Error: Failed to save database\n";
            }
        }
        else if (upperInput.find("CREATE DATABASE") == 0) {
            std::istringstream iss(input);
            std::string create, database, dbName;
            iss >> create >> database >> dbName;
            
            if (!dbName.empty()) {
                if (engine.createDatabase(dbName)) {
                    std::cout << "Database '" << dbName << "' created successfully\n";
                } else {
                    std::cout << "Error: Failed to create database\n";
                }
            } else {
                std::cout << "Error: Database name required\n";
            }
        }
        else if (upperInput.find("OPEN DATABASE") == 0) {
            std::istringstream iss(input);
            std::string open, database, dbName;
            iss >> open >> database >> dbName;
            
            if (!dbName.empty()) {
                if (engine.openDatabase(dbName)) {
                    std::cout << "Database '" << dbName << "' opened successfully\n";
                } else {
                    std::cout << "Error: Failed to open database '" << dbName << "'\n";
                }
            } else {
                std::cout << "Error: Database name required\n";
            }
        }
        else {
            // Execute SQL query
            std::string result = engine.executeQuery(input);
            std::cout << result << "\n";
        }
        
        std::cout << "\n";
    }

    // Auto-save before exit
    engine.saveDatabase();
    std::cout << "Goodbye!\n";
    return 0;
}