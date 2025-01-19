//  CITS2002 Project 2 2024
//  Student1: 23909531 Leran Peng
//  Student2: 24022534 Runzhi Zhao
//  Platform: Apple

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constant definitions.
#define NUM_FRAMES 8                       // Number of frames in RAM.
#define PAGE_SIZE 2                        // Page size.
#define VM_SIZE 32                         // Virtual memory size.
#define RAM_SIZE (NUM_FRAMES * PAGE_SIZE)  // RAM size (8 * 2 = 16).
#define NUM_PROCESSES 4                    // Number of processes.
#define NUM_PAGES 4                        // Number of pages per process.
#define PAGE_ON_DISK 99                    // Indicates page on disk.

// Structure definitions.
typedef struct Page {
    int process_id;
    int page_number;
    int last_accessed;
} Page;

typedef struct Process {
    int page_table[NUM_PAGES];
    int next_page;
} Process;

// Global variable definitions.
Page* virtual_memory[VM_SIZE];
Page* RAM[RAM_SIZE];
Process processes[NUM_PROCESSES];
int current_time;

// Creates a new memory page for the given process and page number.
Page* create_memory_page(int process_id, int page_number) {
    Page* page = (Page*)malloc(sizeof(Page));
    if (page == NULL) {
        fprintf(stderr, "!Memory Allocation failed.");
        exit(EXIT_FAILURE);
    }
    page->process_id = process_id;
    page->page_number = page_number;
    page->last_accessed = -1;
    return page;
}

// Initializes the system.
void initialize() {
    // Initialize virtual memory.
    for (int process_id = 0; process_id < NUM_PROCESSES; process_id++) {
        for (int page_number = 0; page_number < NUM_PAGES; page_number++) {
            int vm_index = (process_id * NUM_PAGES + page_number) * PAGE_SIZE;
            // Create and initialize a memory page.
            Page* page = create_memory_page(process_id, page_number);
            // Each page occupies two consecutive positions in virtual memory.
            virtual_memory[vm_index] = page;
            virtual_memory[vm_index + 1] = page;
        }
    }
    // Initialize RAM to NULL pointers.
    memset(RAM, 0, sizeof(RAM));

    // Initialize processes.
    for (int process_id = 0; process_id < NUM_PROCESSES; process_id++) {
        processes[process_id].next_page = 0;  // Start from the first page.
        for (int page_number = 0; page_number < NUM_PAGES; page_number++) {
            // Initialize page table entries to PAGE_ON_DISK (99), indicating the page is
            // not in RAM.
            processes[process_id].page_table[page_number] = PAGE_ON_DISK;
        }
    }
    // Initialize current time.
    current_time = 0;
}

// Finds a free frame in RAM.
int find_free_frame() {
    for (int frame_index = 0; frame_index < NUM_FRAMES; frame_index++) {
        int RAM_position = frame_index * PAGE_SIZE;
        if (RAM[RAM_position] == NULL) {
            // If the first slot of the frame is NULL, the frame is free.
            return frame_index;  // Found empty spot.
        }
    }
    return -1;  // No empty spot found.
}

// Finds the least recently used (LRU) page of a process in RAM.
int find_local_lru(int process_id) {
    int insert_frame = -1;
    int oldest_time = INT_MAX;
    for (int frame_index = 0; frame_index < NUM_FRAMES; frame_index++) {
        int RAM_position = frame_index * PAGE_SIZE;
        Page* page = RAM[RAM_position];
        if (page != NULL && page->process_id == process_id) {
            // Check if this page belongs to the process and is the least recently used.
            if (page->last_accessed < oldest_time) {
                oldest_time = page->last_accessed;
                insert_frame = frame_index;
            }
        }
    }
    return insert_frame;
}

// Finds the least recently used (LRU) page globally in RAM.
int find_global_lru() {
    int insert_frame = -1;
    int oldest_time = INT_MAX;
    for (int frame_index = 0; frame_index < NUM_FRAMES; frame_index++) {
        int RAM_position = frame_index * PAGE_SIZE;
        Page* page = RAM[RAM_position];
        if (page != NULL && page->last_accessed < oldest_time) {
            // Find the globally least recently used page.
            oldest_time = page->last_accessed;
            insert_frame = frame_index;
        }
    }
    return insert_frame;
}

// Accesses a page for a given process.
void access_page(int process_id) {
    // Check if the process ID is valid.
    if (process_id < 0 || process_id >= NUM_PROCESSES) {
        fprintf(stderr, "!Invalid process ID: %d\n", process_id);
        return;
    }
    // Get the page number to be accessed.
    int access_page_index = processes[process_id].next_page;
    if (access_page_index < 0 || access_page_index >= NUM_PAGES) {
        fprintf(stderr, "!Invalid page index: %d (in Process %d)\n", access_page_index,
                process_id);
        return;
    }
    // Check if the page is already in RAM.
    if (processes[process_id].page_table[access_page_index] != PAGE_ON_DISK) {
        // Page is already in RAM, update last_accessed time.
        int frame_index = processes[process_id].page_table[access_page_index];
        int RAM_index = frame_index * PAGE_SIZE;
        RAM[RAM_index]->last_accessed = current_time;
        RAM[RAM_index + 1]->last_accessed = current_time;
    } else {
        // Page is not in RAM, need to bring it in.
        // Check if there is a free frame in RAM.
        int insert_frame = find_free_frame();
        if (insert_frame == -1) {
            // No free frame found, try to find LRU page of the same process to replace.
            insert_frame = find_local_lru(process_id);
            if (insert_frame == -1) {
                // No page of the process in RAM, use global LRU to prevent starvation.
                insert_frame = find_global_lru();
            }
            // Evict the page currently in the chosen frame.
            int insert_position = insert_frame * PAGE_SIZE;
            int evicting_process_id = RAM[insert_position]->process_id;
            int evicting_page_index = RAM[insert_position]->page_number;
            // Update the page table of the evicted process.
            processes[evicting_process_id].page_table[evicting_page_index] = PAGE_ON_DISK;
            // Remove the page from RAM.
            RAM[insert_position] = NULL;
            RAM[insert_position + 1] = NULL;
        }
        // Bring the page into RAM.
        int vm_index = (process_id * NUM_PAGES + access_page_index) * PAGE_SIZE;
        int insert_position = insert_frame * PAGE_SIZE;
        RAM[insert_position] = virtual_memory[vm_index];
        RAM[insert_position + 1] = virtual_memory[vm_index + 1];
        // Update last_accessed time.
        RAM[insert_position]->last_accessed = current_time;
        // Update process's page table.
        processes[process_id].page_table[access_page_index] = insert_frame;
    }
    // Update process's next page to access.
    processes[process_id].next_page = (processes[process_id].next_page + 1) % NUM_PAGES;
    // Increment the current time.
    current_time++;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "!Usage: %s in.txt out.txt\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // Initialize the system.
    initialize();

    // Open the input file.
    FILE* fin = fopen(argv[1], "r");
    if (fin == NULL) {
        fprintf(stderr, "!Error opening input file\n");
        exit(EXIT_FAILURE);
    }
    // Read process IDs from the input file and access pages.
    int process_id;
    while (fscanf(fin, "%d", &process_id) == 1) {
        access_page(process_id);
    }
    // Close the input file.
    fclose(fin);

    // Open the output file.
    FILE* fout = fopen(argv[2], "w");
    if (fout == NULL) {
        fprintf(stderr, "!Error opening output file\n");
        exit(EXIT_FAILURE);
    }
    // Write page tables of all processes to output file.
    for (int process_id = 0; process_id < NUM_PROCESSES; process_id++) {
        for (int page_number = 0; page_number < NUM_PAGES; page_number++) {
            if (page_number > 0) {
                fprintf(fout, ", ");
            }
            fprintf(fout, "%d", processes[process_id].page_table[page_number]);
        }
        fprintf(fout, "\n");
    }
    // Write the contents of RAM to the output file.
    for (int i = 0; i < RAM_SIZE; i++) {
        if (i > 0) {
            fprintf(fout, "; ");
        }
        if (RAM[i] != NULL) {
            fprintf(fout, "%d,%d,%d", RAM[i]->process_id, RAM[i]->page_number,
                    RAM[i]->last_accessed);
        } else {
            fprintf(fout, "Empty");
        }
    }
    // Close the output file.
    fclose(fout);

    // Free allocated memory for virtual memory pages.
    for (int i = 0; i < VM_SIZE; i += PAGE_SIZE) {
        if (virtual_memory[i] != NULL) {
            free(virtual_memory[i]);
            virtual_memory[i] = NULL;
        }
    }
    return 0;
}