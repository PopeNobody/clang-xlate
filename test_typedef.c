/* Test file with typedef struct patterns that confuse g++ */

// This pattern is common in C but problematic for C++ parsers
typedef struct word_list {
    char *word;
    struct word_list *next;
} WORD_LIST;

// Another common pattern
typedef struct {
    int x;
    int y;
} POINT;

// Forward declaration with typedef
typedef struct connection CONNECTION;

struct connection {
    int fd;
    CONNECTION *next;
};

// Using the typedef'd type
WORD_LIST *create_word_list(const char *word);
void free_word_list(WORD_LIST *list);

// Mixed usage that confuses things
struct word_list *get_next(WORD_LIST *current);
