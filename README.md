# Simple-Relational-Database-Engine

A lightweight, file-based relational database management system implemented in **C++**. This project offers a simple SQL-like interface for creating and managing databases with basic CRUD operations.

---

## Features

### Core Database Operations
- **Database Management**: Create and open multiple databases
- **Table Operations**: Create, list, and drop tables
- **Data Manipulation**: Insert, select, update, and delete records
- **Persistence**: Automatic file-based storage and retrieval
- **Indexing**: B-Tree indexes for primary keys to optimize queries

### Supported Data Types
- `INTEGER` / `INT`  
- `TEXT` / `VARCHAR` 
- `REAL` / `DOUBLE`
- `BOOLEAN` / `BOOL` 

### SQL Commands Supported
- `CREATE TABLE` with column constraints  
- `INSERT INTO` with values  
- `SELECT` with optional `WHERE` clauses  
- `DELETE FROM` with `WHERE` conditions  
- `SHOW TABLES` to list all tables  

### Column Constraints
- `PRIMARY KEY`: Unique identifier with automatic indexing  
- `NOT NULL`: Prevents empty values  
- `AUTO_INCREMENT`: Automatically generates sequential numbers  

---

## Installation

### Prerequisites
- C++17 compatible compiler (e.g., GCC 7+, Clang 5+, MSVC 2017+)
- Standard C++ library with filesystem support

### Building

```bash
# Clone or download the source code
# Compile with C++17 support
g++ -std=c++17 -o database_engine main.cpp
```

### Running

```bash
./database_engine
```

---

## Architecture

### Core Components

#### Value System
- **Value Class**: Handles multiple data types using `std::variant`
- **Type Safety**: Automatic type checking and conversion
- **Comparison**: Built-in operators for sorting and filtering

#### Storage Engine
- **Table Class**: Manages rows, columns, and constraints
- **Row Class**: Represents individual records
- **Column Class**: Defines schema and constraints

#### Indexing
- **BTreeIndex**: Efficient B-tree implementation for fast lookups
- **Automatic Indexing**: Primary keys are automatically indexed
- **Query Optimization**: Uses indexes when available, falls back to linear search

#### Query Processing
- **QueryParser**: Tokenizes and parses SQL-like commands
- **Execution Engine**: Processes parsed queries and returns results
- **Error Handling**: Comprehensive error reporting for invalid queries

#### Persistence
- **File Format**: Custom binary format for efficient storage
- **Automatic Saving**: Database state is preserved between sessions
- **Directory Structure**: Organized file system layout (`data/<database_name>/`)

---

## File Structure

project_root/
├── main.cpp              # Complete source code
├── README.md            # This file
└── data/                # Created automatically
    └── <database_name>/
        └── <table_name>.tbl

---

## Limitations

This database engine is currently single-threaded with no support for concurrent access. It loads all data into memory during operation and supports only a basic subset of SQL, without joins or the ability to alter table structures. Transactions and ACID compliance are not implemented. Performance-wise, the entire database is saved to disk on each SAVE command, and due to in-memory processing, it's best suited for small to medium datasets under 1GB.

---

## License

This project is open-source and available under the MIT License.
