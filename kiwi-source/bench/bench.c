//Theofanis Tompolis 4855
//Athanasios Fytilis 5381
//Marinos Aristidou 5397
#include "bench.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>      // For clock_gettime
#include <inttypes.h>  // For PRIu64, PRId64 format specifiers
#include <sys/time.h>  

#include "db.h"

// --- Global Variables
DB* db;
// For this pattern (aggregate after join), regular types are okay for globals
uint64_t total_reads_global = 0;
uint64_t total_writes_global = 0;
uint64_t total_read_time_us = 0;  // Store time in microseconds
uint64_t total_write_time_us = 0; // Store time in microseconds

// --- Timing Function ---
// Get time in microseconds using CLOCK_MONOTONIC
uint64_t get_monotonic_us() {
    struct timespec ts;
    // Use CLOCK_MONOTONIC for measuring elapsed time
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE); // Or handle error appropriately
    }
    // Convert seconds to microseconds and add nanoseconds converted to microseconds
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// --- Key Generation ---
void _random_key(char *key, int length) {
    // Simple random key generation (same as before)
    int i;
    // Reduced salt length slightly, ensures we don't go past 'z' + '9' easily if alphabet changes
    const char salt[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    const int salt_len = sizeof(salt) - 1; // Exclude null terminator
    for (i = 0; i < length; i++)
        key[i] = salt[rand() % salt_len];
    key[length] = '\0'; // Ensure null termination
}

// --- Header/Environment Printing ---
void _print_header(int count)
{
    double index_size_mib = (double)((uint64_t)KSIZE + 8 + 1) * count / (1024.0 * 1024.0); 
    double data_size_mib = (double)((uint64_t)VSIZE + 4) * count / (1024.0 * 1024.0); 
    // Output using MiB for clarity
    printf("Keys:\t\t%d bytes each\n", KSIZE);
    printf("Values: \t%d bytes each\n", VSIZE);
    printf("Entries:\t%d\n", count); // Use %d for standard int
    printf("IndexSize:\t%.2f MiB (estimated)\n", index_size_mib);
    printf("DataSize:\t%.2f MiB (estimated)\n", data_size_mib);
    printf("---------------------------------------------------------------------------------------------------\n");
}

void _print_environment()
{
    time_t now = time(NULL);
    printf("Date:\t\t%s", ctime(&now)); // ctime includes newline

    int num_cpus = 0;
    char cpu_type[256] = {0};
    char cache_size[256] = {0};

    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[1024];
        int core_count = 0; // Count physical processors listed
        while (fgets(line, sizeof(line), cpuinfo) != NULL) {
             // Trim leading/trailing whitespace for robust matching
             char* start = line; while (isspace(*start)) start++;
             char* end = start + strlen(start) - 1; while (end > start && isspace(*end)) end--; *(end + 1) = '\0';

            if (strncmp(start, "processor", 9) == 0) { // Count lines starting with "processor"
                core_count++;
             } else if (strncmp(start, "model name", 10) == 0 && cpu_type[0] == '\0') { // Take first model name
                 char* colon = strchr(start, ':');
                 if (colon) {
                     char* model = colon + 1; while (isspace(*model)) model++; // Skip space after colon
                     strncpy(cpu_type, model, sizeof(cpu_type) - 1);
                 }
             } else if (strncmp(start, "cache size", 10) == 0 && cache_size[0] == '\0') { // Take first cache size
                 char* colon = strchr(start, ':');
                 if (colon) {
                     char* cache = colon + 1; while (isspace(*cache)) cache++; // Skip space
                     strncpy(cache_size, cache, sizeof(cache_size) - 1);
                 }
             }
        }
        fclose(cpuinfo);
        // Use the count of 'processor' lines as num_cpus, often more accurate than multiplying
        num_cpus = core_count;
        printf("CPU:\t\t%d * %s\n", num_cpus, cpu_type[0] ? cpu_type : "Unknown");
        printf("CPUCache:\t%s\n", cache_size[0] ? cache_size : "Unknown");
    } else {
        printf("CPU:\t\tUnavailable (/proc/cpuinfo not found)\n");
        printf("CPUCache:\tUnavailable\n");
    }
    printf("---------------------------------------------------------------------------------------------------\n");
}


// --- MIX mode structs & functions ---

typedef struct {
    int thread_id;
    int ops_count; // Number of operations this thread should perform
    int write_ratio_percent; // Write percentage (0-100)
    int use_random_keys;

    // Per-thread results
    uint64_t reads_done;
    uint64_t writes_done;
    uint64_t read_time_us;  // Total microseconds spent in db_get
    uint64_t write_time_us; // Total microseconds spent in db_add
    uint64_t total_time_ms; // Wall clock time for this thread
} thread_args;

void* mix_thread(void* arg) {
    thread_args* args = (thread_args*) arg;
    char key[KSIZE + 1];
    char val[VSIZE + 1]; // Only needed for writes
    Variant k, v;
    k.mem = key; // Point k.mem once
    v.mem = val; // Point v.mem once
    k.length = KSIZE;
    v.length = VSIZE; 

    // Initialize results
    args->reads_done = 0;
    args->writes_done = 0;
    args->read_time_us = 0;
    args->write_time_us = 0;

    uint64_t thread_start_us = get_monotonic_us();

    for (int i = 0; i < args->ops_count; i++) {
        // Determine operation type
        int do_write = (rand() % 100) < args->write_ratio_percent;

        // Generate Key
        if (args->use_random_keys) {
            _random_key(key, KSIZE);
        } else {
            snprintf(key, KSIZE + 1, "key-%0*d", KSIZE - 4, i + args->thread_id * args->ops_count); // Padded key
        }
        // k.length = KSIZE; 

        if (do_write) {
            // Generate Value (only when writing)
            // Simple value generation 
            snprintf(val, VSIZE + 1, "val-%d", i); // Keep null termination for safety if needed
            // v.length = VSIZE; // Already set

            uint64_t op_start_us = get_monotonic_us();
            db_add(db, &k, &v); // <<< --- TIME THIS --- >>>
            uint64_t op_end_us = get_monotonic_us();

            args->writes_done++;
            args->write_time_us += (op_end_us - op_start_us);

        } else { // Read operation
            uint64_t op_start_us = get_monotonic_us();
            // Result of db_get not used, common in benchmarks (check existence/latency)
            db_get(db, &k, &v); // <<< --- TIME THIS --- >>>
            uint64_t op_end_us = get_monotonic_us();

            args->reads_done++;
            args->read_time_us += (op_end_us - op_start_us);
        }
    }

    uint64_t thread_end_us = get_monotonic_us();
    args->total_time_ms = (thread_end_us - thread_start_us) / 1000; // Store thread wall time in ms

    return NULL;
}

// --- Main Benchmark Runner for MIX mode ---
void run_mix(int total_ops_requested, int write_ratio_percent, int num_threads, int use_random_keys) {
    pthread_t tids[num_threads];
    thread_args args[num_threads];

    // Calculate ops per thread, handle uneven division
    int ops_per_thread = total_ops_requested / num_threads;
    int remainder_ops = total_ops_requested % num_threads;
    int total_ops_actual = 0; // Sum of ops actually assigned

    printf("Starting benchmark...\n");
    printf("Requested Total Ops: %d\n", total_ops_requested);
    printf("Threads: %d, Write Ratio: %d%%\n", num_threads, write_ratio_percent);
    printf("Random Keys: %s\n", use_random_keys ? "Yes" : "No");
    printf("---------------------------------------------------------------------------------------------------\n");


    uint64_t start_us = get_monotonic_us();

    db = db_open("testdb");
    if (!db) {
         fprintf(stderr, "ERROR: Failed to open database 'testdb'\n");
         exit(EXIT_FAILURE);
    }

    // Launch threads
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].ops_count = ops_per_thread + (i < remainder_ops ? 1 : 0); 
        args[i].write_ratio_percent = write_ratio_percent;
        args[i].use_random_keys = use_random_keys;
        total_ops_actual += args[i].ops_count; // Track actual ops assigned

        if (pthread_create(&tids[i], NULL, mix_thread, &args[i]) != 0) {
             perror("pthread_create");
             // Consider cleanup / joining already created threads before exiting
             exit(EXIT_FAILURE);
        }
    }

    // Wait for threads to complete and Aggregate results
    total_reads_global = 0;
    total_writes_global = 0;
    total_read_time_us = 0;
    total_write_time_us = 0;
    uint64_t max_thread_time_ms = 0; // Time taken by the slowest thread

    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(tids[i], NULL) != 0) {
            perror("pthread_join");
             // Consider how to handle join errors
        }
        // Aggregate results *after* thread has finished
        total_reads_global += args[i].reads_done;
        total_writes_global += args[i].writes_done;
        total_read_time_us += args[i].read_time_us;
        total_write_time_us += args[i].write_time_us;
        if (args[i].total_time_ms > max_thread_time_ms) {
            max_thread_time_ms = args[i].total_time_ms;
        }
    }

    uint64_t end_us = get_monotonic_us();
    db_close(db); // Close DB after all threads are done

    // --- Calculate Summary Statistics ---
    double total_duration_sec = (end_us - start_us) / 1000000.0;
    uint64_t total_ops_done = total_reads_global + total_writes_global;
    // Ensure total_ops_actual matches total_ops_done if threads complete successfully
    if (total_ops_done != (uint64_t)total_ops_actual) {
        fprintf(stderr, "Warning: Mismatch between assigned ops (%d) and completed ops (%" PRIu64 ")\n",
                total_ops_actual, total_ops_done);
    }

    double ops_per_sec = (total_duration_sec > 0) ? (total_ops_done / total_duration_sec) : 0;

    // Calculate average latencies in microseconds
    double avg_read_latency_us = (total_reads_global > 0) ? ((double)total_read_time_us / total_reads_global) : 0;
    double avg_write_latency_us = (total_writes_global > 0) ? ((double)total_write_time_us / total_writes_global) : 0;
    uint64_t total_op_time_us = total_read_time_us + total_write_time_us;
    double avg_op_latency_us = (total_ops_done > 0) ? ((double)total_op_time_us / total_ops_done) : 0;


    // --- Print Summary Output ---
    printf("+--------------------------------+--------------------------------+\n");
    printf("| Overall Statistics             |                                |\n");
    printf("+--------------------------------+--------------------------------+\n");
    printf("| Wall Clock Time                | %10.3f sec                  |\n", total_duration_sec);
    printf("| Total Operations Completed     | %10" PRIu64 " ops                 |\n", total_ops_done);
    printf("|   Reads                        | %10" PRIu64 " ops                 |\n", total_reads_global);
    printf("|   Writes                       | %10" PRIu64 " ops                 |\n", total_writes_global);
    printf("| Throughput                     | %10.1f ops/sec             |\n", ops_per_sec);
    printf("+--------------------------------+--------------------------------+\n");
    printf("| Average Latencies              |                                |\n");
    printf("+--------------------------------+--------------------------------+\n");
    printf("| Average Operation Latency    | %10.1f us                   |\n", avg_op_latency_us);
    printf("|   Average Read Latency         | %10.1f us                   |\n", avg_read_latency_us);
    printf("|   Average Write Latency        | %10.1f us                   |\n", avg_write_latency_us);
    printf("+--------------------------------+--------------------------------+\n");
    // Optionally print total CPU time spent inside DB ops (convert us to sec)
    printf("| Total DB Ops CPU Time (Sum)    | %10.3f sec                  |\n", total_op_time_us / 1000000.0);
    printf("| Max Thread Wall Time           | %10" PRIu64 " ms                  |\n", max_thread_time_ms);
    printf("+--------------------------------+--------------------------------+\n");

    // Print per-thread summary (can be verbose)
    
    printf("\nPer-Thread Details:\n");
    printf("---------------------------------------------------------------------------------------\n");
    printf("| Thread | Ops Done | Reads  | Writes | Read Time (us) | Write Time (us) | Wall Time (ms) |\n");
    printf("|--------|----------|--------|--------|----------------|-----------------|----------------|\n");
    for(int i = 0; i < num_threads; i++) {
        printf("| %-6d | %-8" PRIu64 " | %-6" PRIu64 " | %-6" PRIu64 " | %-14" PRIu64 " | %-15" PRIu64 " | %-14" PRIu64 " |\n",
            args[i].thread_id,
            args[i].reads_done + args[i].writes_done,
            args[i].reads_done,
            args[i].writes_done,
            args[i].read_time_us,
            args[i].write_time_us,
            args[i].total_time_ms);
    }
    printf("---------------------------------------------------------------------------------------\n");
    
}


// --- Main Function ---
int main(int argc,char** argv)
{
    // Seed random number generator once at the start
    srand(time(NULL));

    if (argc >= 2 && strcmp(argv[1], "mix") == 0) {
        if (argc != 6) {
            fprintf(stderr,"Usage: ./kiwi-bench mix <total_ops> <write_%%> <threads> <random(0|1)>\n");
            fprintf(stderr,"  Example: ./kiwi-bench mix 1000000 10 4 1\n"); // Example usage
            exit(EXIT_FAILURE);
        }
        // Use strtoll for better error checking? For now, atoi is simple.
        long long total_ops_ll = atoll(argv[2]); // Use long long for potentially large op counts
        int write_ratio_percent = atoi(argv[3]);
        int threads = atoi(argv[4]);
        int random_keys = atoi(argv[5]);

        if (total_ops_ll <= 0 || write_ratio_percent < 0 || write_ratio_percent > 100 || threads <= 0 || (random_keys != 0 && random_keys != 1)) {
             fprintf(stderr, "Error: Invalid arguments.\n");
             fprintf(stderr,"Usage: ./kiwi-bench mix <total_ops> <write_%%> <threads> <random(0|1)>\n");
             exit(EXIT_FAILURE);
        }
        if (total_ops_ll > 0 && total_ops_ll < threads) {
            fprintf(stderr, "Warning: Total operations (%lld) is less than thread count (%d). Adjusting threads to %lld.\n",
                    total_ops_ll, threads, total_ops_ll);
            threads = (int)total_ops_ll;
        }

        int total_ops_int = (int)total_ops_ll; // Cast for functions expecting int, check for overflow if needed

        _print_header(total_ops_int);
        _print_environment();
        run_mix(total_ops_int, write_ratio_percent, threads, random_keys);

        return 0;
    }

    // Fallback: legacy write/read (KEEPING THESE FOR BACKWARD COMPATIBILITY
    if (argc < 3) {
        fprintf(stderr,"Usage: %s <write | read> <count> [random]\n", argv[0]);
        fprintf(stderr,"       %s mix <total_ops> <write_%%> <threads> <random(0|1)>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    long count = atol(argv[2]); // Use atol for potentially larger counts
     if (count <= 0) {
         fprintf(stderr, "Error: Invalid count '%s'\n", argv[2]);
         exit(EXIT_FAILURE);
     }
     _print_header((int)count); 
    _print_environment();

    uint64_t start_us = get_monotonic_us(); // Use monotonic clock here too

     if (strcmp(argv[1], "write") == 0) {
        int r = (argc == 4 && atoi(argv[3]) == 1); // Check random flag
        _write_test((int)count, r); // Assuming legacy functions take int count
    } else if (strcmp(argv[1], "read") == 0) {
         int r = (argc == 4 && atoi(argv[3]) == 1); // Check random flag
        _read_test((int)count, r); // Assuming legacy functions take int count
    } else {
         fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        fprintf(stderr,"Usage: %s <write | read | mix> ...\n", argv[0]);
         exit(EXIT_FAILURE);
    }

    uint64_t end_us = get_monotonic_us();
    double duration_sec = (end_us - start_us) / 1000000.0;
    // Keep legacy output format for these modes
    printf("[%-5s] Total time: %.3f sec\n", argv[1], duration_sec);

    return 0;
}
