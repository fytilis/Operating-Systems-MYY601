# MYY601 Operating Systems Labs

This repository contains the projects developed for the "MYY601 Operating Systems" course at the Department of Computer Science & Engineering, University of Ioannina. The labs focus on low-level systems programming, exploring concepts in concurrency, synchronization, and kernel-level file system instrumentation.

---

## Lab 1: Implementing Multithreading in a Key-Value Storage Engine

This project enhances a basic Log-Structured Merge-tree (LSM-tree) based key-value storage engine with multithreaded capabilities, enabling it to handle concurrent read and write operations safely and efficiently.

### Core Objective

The primary goal was to transform a single-threaded storage engine into a high-performance, thread-safe system capable of managing concurrent data access, a fundamental requirement for modern databases and cloud platforms.

### Key Implementations & Concepts

*   **Concurrency Model**: The implementation follows the **readers-writer lock** pattern. This allows multiple threads to read data simultaneously, while write operations are granted exclusive access to ensure data consistency and integrity.

*   **Synchronization Primitives**:
    *   **`pthread_rwlock_t`**: Used as the primary mechanism to manage concurrent access. It provides shared read locks and exclusive write locks, which is more performant than a simple mutex in read-heavy scenarios.
    *   **Mutex Locks & Condition Variables**: Utilized to prevent race conditions and manage shared resources, avoiding issues like deadlocks and synchronization failures.

*   **Benchmarking Harness (`bench.c`)**:
    *   A custom benchmarking tool was developed to measure the performance of the multithreaded engine.
    *   It creates a configurable number of worker threads, each performing a mix of `read` and `write` operations.
    *   The tool supports both random and sequential key generation to simulate different access patterns.

*   **Performance Metrics**: The system's performance was evaluated based on:
    *   **Throughput**: Measured in operations per second (ops/sec).
    *   **Latency**: Average time taken for read and write operations, measured in microseconds (µs).
    *   **Wall Clock Time**: Total execution time under various loads (number of entries, threads, and write ratios).

---

## Lab 2: VFAT File System Logging with Linux Kernel Library (LKL)

This project implements a logging (journaling) mechanism to monitor and record core operations of the VFAT file system, running within the sandboxed environment of the Linux Kernel Library (LKL).

### Core Objective

The goal was to instrument the VFAT file system code at the kernel level to log critical events to an external user-space file. This facilitates debugging, performance analysis, and a deeper understanding of file system behavior without modifying the host kernel.

### Architecture & Approach

A **callback-based architecture** was designed to bridge the LKL kernel space and the user-space test harness. Direct syscalls from within the LKL kernel context proved unstable, so a safer approach was adopted:
1.  Kernel-level VFAT functions trigger a lightweight, external C function.
2.  This external function, running in user space, uses standard I/O (`fopen`, `fprintf`, `fclose`) to write formatted log messages to a `journal.log` file.
3.  This design decouples the logging logic from the kernel, ensuring stability.

### Key Implementations & Concepts

*   **Logged VFAT Operations**: The logging mechanism was embedded to trace the following key functions:
    *   `fat_fill_super`: Logs file system **mount** events, including device and flags.
    *   `__fat_readdir`: Logs directory **read** operations.
    *   `fat_add_entries`: Logs the creation of new directory entries.
    *   `fat_build_inode`: Logs the in-memory construction of a new inode when a file is created.

*   **Build System Integration**: The project's `Makefile` was modified to compile the new logging module (`fat_log.c`) and link it into the static library `liblkl.a`, making the logging functions available to the kernel.

*   **Testing and Verification**:
    *   Initial testing was done with `printk` to confirm that kernel code could be successfully modified and recompiled.
    *   Two test scenarios were used to validate the final implementation:
        1.  **`disk.sh`**: A simple script for basic file system operations.
        2.  **`cptofs`**: A tool used to perform a full file copy, which successfully triggered a wider range of VFAT functions (`fat_add_entries`, `fat_build_inode`) that were not invoked by `disk.sh`.
    *   The logging mode was changed from write (`"w"`) to append (`"a"`) to ensure that logs from consecutive test runs were preserved.

## ✍️ Authors

*   **Theofanis Tombolis** (4855)
*   **Athanasios Fytilis** (5381)
*   **Marinos Aristeidou** (5397)
