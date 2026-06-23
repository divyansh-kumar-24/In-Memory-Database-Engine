# In-Memory Database Engine (C++)

## Overview

This project is a lightweight relational database engine implemented entirely in C++. The goal of the project is to understand and demonstrate the core concepts behind modern Database Management Systems (DBMS) such as:

* Table creation and schema management
* Record insertion and deletion
* Query execution
* Indexing
* Sorting using ordered indexes
* Data persistence
* Object-Oriented Software Design
* Efficient Data Structures and Algorithms

The database stores all records in memory for fast access while also maintaining persistent storage on disk so that data survives program restarts.

---

# Features

## Supported Commands

### CREATE TABLE

Creates a new table with a user-defined schema.

Example:

```sql
CREATE TABLE Students (ID, Name, Age);
```

---

### INSERT

Adds a new row into an existing table.

Example:

```sql
INSERT INTO Students VALUES (1, Alice, 20);
```

---

### SELECT

Retrieves records from a table.

Example:

```sql
SELECT * FROM Students;
```

---

### DELETE

Removes records matching a condition.

Example:

```sql
DELETE FROM Students WHERE ID = 1;
```

---

### ORDER BY

Returns records sorted according to a column.

Example:

```sql
SELECT * FROM Students ORDER BY Age;
```

---

### Persistence

All table data is automatically stored on disk and loaded back when the database restarts.

---

# System Architecture

The system follows a layered architecture.

```text
+----------------------+
|      SQL Parser      |
+----------+-----------+
           |
           v
+----------------------+
|   Database Engine    |
+----------+-----------+
           |
           |
  +--------+--------+
  |                 |
  v                 v
Table Manager   Persistence Layer
  |
  |
  +-----------------------+
  |                       |
  v                       v
Hash Index          B-Tree Index
```

---

# Major Components

## 1. Database Class

The Database class acts as the top-level controller.

Responsibilities:

* Maintains all tables
* Routes commands
* Loads data from disk
* Saves data to disk
* Provides interface for query execution

### Why?

Following the Single Responsibility Principle, all database-wide operations are centralized in one place.

---

## 2. Table Class

Represents an individual table.

Stores:

```cpp
vector<string> columns;
vector<Row> rows;
```

Responsibilities:

* Insert records
* Delete records
* Search records
* Maintain indexes
* Provide sorted output

Each table behaves similarly to a relation in a relational DBMS.

---

## 3. Row Class / Structure

Represents a single record.

Example:

```cpp
{
    "1",
    "Alice",
    "20"
}
```

A row is internally stored as:

```cpp
vector<string>
```

---

## 4. Index Manager

Responsible for accelerating queries.

Maintains:

### Hash Index

For exact-match lookups.

Example:

```sql
SELECT * FROM Students
WHERE ID = 101;
```

Instead of scanning the entire table:

```text
O(n)
```

the hash index performs:

```text
O(1)
```

average lookup.

Implementation:

```cpp
unordered_map<
    string,
    vector<int>
>
```

---

### B-Tree Index

Used for:

```sql
ORDER BY column
```

and ordered retrieval.

Implementation:

```cpp
map<
    string,
    vector<int>
>
```

C++ STL map internally uses a Red-Black Tree.

A Red-Black Tree is a self-balancing Binary Search Tree and offers the same asymptotic complexity guarantees as a B-Tree style ordered index for this project.

Operations:

| Operation        | Complexity |
| ---------------- | ---------- |
| Insert           | O(log n)   |
| Delete           | O(log n)   |
| Search           | O(log n)   |
| Sorted Traversal | O(n)       |

---

# Data Structures Used

## Vector

Used for:

```cpp
vector<Row>
```

Advantages:

* Contiguous memory
* Fast iteration
* Cache friendly

Complexities:

| Operation | Complexity     |
| --------- | -------------- |
| Access    | O(1)           |
| Push Back | O(1) amortized |

---

## Hash Map

Used for indexing.

```cpp
unordered_map
```

Provides:

```text
Average O(1)
```

lookup.

Used in:

* WHERE clauses
* Exact-match queries
* Record deletion

---

## Ordered Tree

Used for ORDER BY queries.

```cpp
map
```

Provides:

```text
O(log n)
```

insertion and search.

---

## File Streams

Used for persistence.

```cpp
ifstream
ofstream
```

Stores tables as:

```text
table_name.tbl
```

files.

---

# Query Execution Flow

Consider:

```sql
SELECT * FROM Students
WHERE ID = 100
ORDER BY Age;
```

Execution Pipeline:

```text
Parse Query
      |
      v
Identify Table
      |
      v
Use Hash Index
      |
      v
Retrieve Matching Rows
      |
      v
Use Ordered Index
      |
      v
Return Sorted Result
```

---

# Persistence Design

## Why Persistence?

Without persistence:

```text
Program Exit
    =>
Data Lost
```

Persistence guarantees durability.

---

## Save Process

Whenever modifications occur:

```text
INSERT
DELETE
CREATE TABLE
```

the database writes updated contents to disk.

Example:

```text
Students.tbl
```

Contents:

```text
ID,Name,Age
1,Alice,20
2,Bob,21
```

---

## Load Process

During startup:

```cpp
loadDatabase();
```

The engine:

1. Reads table files.
2. Reconstructs rows.
3. Rebuilds indexes.
4. Restores database state.

---

# Object-Oriented Programming Principles

The project was intentionally designed using core OOP principles.

---

## Encapsulation

Data and behavior are grouped together.

Example:

```cpp
class Table
{
private:
    vector<Row> rows;

public:
    void insertRow(...);
    void deleteRow(...);
};
```

Benefits:

* Data protection
* Controlled access
* Easier maintenance

---

## Abstraction

Users interact through commands:

```sql
INSERT INTO ...
```

without knowing:

* Index implementation
* File storage format
* Internal memory layout

Complex implementation details are hidden.

---

## Modularity

Every component has a dedicated responsibility.

Examples:

```text
Database
Table
Parser
Index Manager
Persistence Manager
```

Benefits:

* Easier debugging
* Easier testing
* Easier future extension

---

## Composition

Database owns multiple tables.

```cpp
Database
  └── Table
         └── Rows
```

This follows a "has-a" relationship.

---

## Open/Closed Principle

New query types can be added without modifying existing functionality extensively.

Future commands:

```sql
UPDATE
GROUP BY
JOIN
LIMIT
```

can be added by extending parser and execution logic.

---

# Time Complexity Analysis

## CREATE TABLE

| Operation     | Complexity |
| ------------- | ---------- |
| Create Schema | O(c)       |

where:

```text
c = number of columns
```

---

## INSERT

| Operation         | Complexity |
| ----------------- | ---------- |
| Row Insert        | O(1)       |
| Hash Index Update | O(1)       |
| Tree Index Update | O(log n)   |

Overall:

```text
O(log n)
```

---

## SELECT

Without Index:

```text
O(n)
```

With Hash Index:

```text
O(1)
```

average.

---

## DELETE

Find Row:

```text
O(1)
```

using hash index.

Index update:

```text
O(log n)
```

Overall:

```text
O(log n)
```

---

## ORDER BY

Using ordered index:

```text
O(n)
```

for traversal after index retrieval.

Without index:

```text
O(n log n)
```

due to sorting.

This demonstrates why maintaining a tree-based ordered index is beneficial.

---

# DBMS Concepts Demonstrated

This project demonstrates several concepts taught in Database Management Systems courses:

### Schema Management

Definition of table structures.

---

### Record Management

Insertion and deletion of tuples.

---

### Indexing

Hash indexing for exact matches.

Tree indexing for ordered retrieval.

---

### Query Processing

Parsing and execution of SQL-like commands.

---

### Durability

Persistent storage using files.

---

### Storage Engine Concepts

* Table files
* Record storage
* Index maintenance

---

# Limitations

Current implementation does not support:

* JOIN
* UPDATE
* GROUP BY
* Aggregations
* Transactions
* Concurrency control
* Multi-column indexes
* Query optimization

These can be added as future enhancements.

---

# Future Enhancements

1. SQL Parser using Lexer + Parser
2. B+ Tree implementation
3. Query Optimizer
4. Transactions (ACID)
5. WAL (Write Ahead Logging)
6. Buffer Manager
7. LRU Page Cache
8. Multi-column Indexes
9. UPDATE Queries
10. JOIN Support
11. GROUP BY and Aggregations
12. Concurrency using Reader-Writer Locks

---

# Key Learnings

Through this project, the following concepts were implemented and understood:

* Object-Oriented Design in C++
* Encapsulation and Abstraction
* Data Structures (Hash Tables, Trees, Vectors)
* Algorithm Complexity Analysis
* Query Processing
* Indexing Strategies
* Persistent Storage
* Basic DBMS Internal Architecture

This project serves as a simplified implementation of the storage and query execution layer of a relational database and demonstrates practical application of OOP, DSA, and DBMS concepts in C++.
