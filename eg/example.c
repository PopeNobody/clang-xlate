/* Example C file with various declarations */
#include <stdio.h>

#define MAX_SIZE 100
#define HANDLE void*

// Typedef declarations
typedef void* handle_t;
typedef struct buffer* buffer_handle;
typedef int (*callback_fn)(void* data);

// Struct declarations and definitions
struct buffer {
    char* data;
    size_t size;
};

struct connection;  // Forward declaration

// Union
union value {
    int i;
    float f;
    void* ptr;
};

// Enum
enum status {
    STATUS_OK = 0,
    STATUS_ERROR = -1,
    STATUS_PENDING
};

// Global variables (the handle/pointer issue you mentioned)
HANDLE global_handle;  // Should probably be HANDLE*?
struct buffer* buffer_ptr;
handle_t process_handle;

// Function declarations
int process_data(struct buffer* buf);
void* allocate_handle(size_t size);
HANDLE create_connection(const char* host);

// Function definition
int process_data(struct buffer* buf) {
    if (!buf || !buf->data) {
        return -1;
    }
    return 0;
}

// Another definition
void cleanup_resources(HANDLE h, int flags) {
    // Implementation here
    free(h);
}
