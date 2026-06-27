# RedisDB Architecture and Design

This document details the architecture, high-level design, and the technical decisions made while building RedisDB—a cross-platform, high-performance, in-memory data store written in modern C++. The project is designed to be highly compatible with the official Redis server, utilizing the same RESP protocol and supporting similar internal data structures and persistence mechanisms.

---

## 1. High-Level Architecture Overview

The system architecture is divided into five primary layers:

1.  **Networking Layer:** An asynchronous, event-driven network engine responsible for multiplexing I/O events.
2.  **Protocol Layer:** A streaming parser and serializer for the Redis Serialization Protocol (RESP).
3.  **Server & Command Core:** The central orchestrator that manages client connections, background cron jobs, and dispatches commands to their respective handlers.
4.  **Storage Engine:** The logical databases (`Database`) that store the actual keys and handle TTLs (Time-To-Live).
5.  **Persistence & Replication:** Subsystems responsible for data durability (RDB, AOF) and master-replica synchronization.

---

## 2. Core Components

### 2.1 Networking (`EventLoop` & `ConnectionManager`)
RedisDB uses a Reactor pattern. The `EventLoop` acts as a platform-agnostic abstraction over OS-specific multiplexing APIs (like `epoll` on Linux, `kqueue` on macOS, or `select`/`WSAAsyncSelect` on Windows). 
- **`ConnectionManager`** tracks all active client connections, buffering incoming streams into input buffers and outgoing replies into output buffers.

### 2.2 Protocol Processing (`RespParser` & `RespSerializer`)
- **`RespParser`**: Parses the raw byte stream from clients into abstract syntax trees of `RespValue` objects. It is stateful, meaning it can handle partial packets seamlessly.
- **`RespSerializer`**: Takes `RespValue` objects (simple strings, bulk strings, integers, arrays, and errors) and converts them back into RESP byte streams to be written to the socket.

### 2.3 Server Core (`Server` & `CommandRegistry`)
- **`Server`**: The heart of the application. It runs the event loop, executes the `serverCron` (for background tasks like active key expiration), and handles client blocking (e.g., `BLPOP`).
- **`CommandRegistry`**: A hash map mapping command names (e.g., `SET`, `GET`) to a `CommandHandler`. This decouples the command parsing logic from the execution logic.

### 2.4 Storage Engine (`Database` & `RedisObject`)
- **`Database`**: Contains the main keyspace (a hash table mapping strings to `RedisObject`s) and an expiry table mapping strings to expiration timestamps.
- **`RedisObject`**: A unified wrapper for all underlying data types. Instead of using raw pointers or base classes, it encapsulates the specific type.

### 2.5 Data Structures
To match Redis's algorithmic complexity guarantees, we implemented specific data structures:
- **String**: Wraps `std::string`.
- **List**: A custom doubly `LinkedList` allowing O(1) push and pop on both ends.
- **Hash**: A custom `HashTable` using open addressing or chaining (with tombstones for deletions) to provide O(1) lookup.
- **Set**: A `HashSet` built on top of the `HashTable` using dummy values.
- **Sorted Set**: A combination of a `HashTable` and a `SkipList`. The hash table provides O(1) score lookups by member name, while the skip list provides O(log N) inserts and fast range queries.

### 2.6 Persistence & Replication
- **RDB**: Point-in-time snapshotting that serializes the entire database state into a compact binary format.
- **AOF**: Append-Only File logging that sequentially records every write command.
- **Replication**: An asynchronous master-replica state machine. The master maintains a circular replication backlog buffer, and replicas synchronize via full RDB syncs followed by partial command streaming.

---

## 3. Key Architectural Choices and Rationale

During the development of RedisDB, several significant design choices were made to optimize for performance, maintainability, and compatibility.

### Choice 1: Single-Threaded Event Loop vs. Multi-Threading
**Decision:** We chose a single-threaded reactor pattern for the core command execution engine.
**Rationale:** 
- **Data Consistency:** Avoiding multi-threading for the core keyspace means we completely bypass the need for locks, mutexes, or atomic operations on the data structures. This prevents lock contention and context-switching overhead, mirroring the design of the official Redis.
- **Deterministic Execution:** Commands execute sequentially and atomically by default.
- *Note:* Heavy background tasks (like RDB saving) can be offloaded to child processes (like `fork()` in Unix) or background threads.

### Choice 2: Using `std::variant` vs. Polymorphism (Inheritance) for `RedisObject`
**Decision:** `RedisObject` wraps data structures using a tagged union (`std::variant` or similar internal construct) rather than a base `Object` class with virtual methods.
**Rationale:**
- **Cache Locality:** Avoiding heap allocations for small objects and avoiding virtual table pointer overhead improves CPU cache hit rates.
- **Value Semantics:** It simplifies memory management and deep copying (e.g., during BGSAVE cloning).

### Choice 3: Custom Data Structures over Standard Library (STL)
**Decision:** We built custom `LinkedList`, `HashTable`, and `SkipList` structures instead of relying entirely on `std::list`, `std::unordered_map`, and `std::set`.
**Rationale:**
- **Algorithmic Requirements:** `std::set` is typically implemented as a Red-Black tree. A `SkipList` combined with a hash table is required for Sorted Sets to allow extremely fast range queries and rank queries, which Red-Black trees do not natively support without significant modification.
- **Memory Overhead:** Custom structures allow us to strip away the memory overhead of standard library containers and implement Redis-specific optimizations.

### Choice 4: Abstracting the Event Loop
**Decision:** Created a platform-agnostic `EventLoop` interface.
**Rationale:**
- **Cross-Platform Support:** Redis historically relied heavily on POSIX APIs. By abstracting the event loop, RedisDB can seamlessly compile and run on Windows using optimized networking APIs while preserving `epoll`/`kqueue` usage on Unix systems.

### Choice 5: Explicit Style Guidelines (2-Space Indent & East-Const/Pointer)
**Decision:** We strictly enforced a uniform code style consisting of 2-space indentation, no verbose decorative comments, and East-Reference/East-Pointer placement (e.g., `const std::string &name` rather than `const std::string& name`).
**Rationale:**
- **Readability & Consistency:** A highly consistent codebase is easier to maintain. 
- **East-Pointer Logic:** Placing the `*` or `&` next to the variable type strictly follows the "read right-to-left" rule in C++ type declarations, creating less cognitive dissonance when reading complex pointer and reference declarations.

---

## 4. Conclusion
RedisDB achieves its goal of being a robust, high-performance replica of Redis by strictly adhering to an event-driven, single-threaded core. By making deliberate choices around memory layout, custom data structures, and cross-platform abstractions, the codebase remains efficient while offering the classic algorithmic guarantees of an in-memory data store.
