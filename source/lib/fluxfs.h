
#include <stdint.h>
#include <inttypes.h>

struct vf_strings {
	uint8_t cnt;
	char *paths[256];
};

struct vf_entry {
	// Type of entry
	uint8_t type;
	// Number of bytes for this entry
	uint64_t length;
	union {
		// Data for this entry
		uint8_t *bytes;
		// Offset into the external file for this entry
		uint64_t offset;
	} data;
	// Index into the paths strings
	uint8_t pathIndex;
	// Next entry in the linked list
	struct vf_entry *next;
};

struct vir_file {
	FILE *files[256];
	struct vf_strings *strings;
	struct vf_entry *head;
	struct vf_entry *tail;
	uint64_t size;
};

void free_vf(struct vir_file *vf);
struct vir_file *load_vf(FILE *file);
struct vir_file *create_vf();
uint8_t vf_add_path(struct vir_file *vf, const char *filePath);
struct vf_entry *vf_add_data(struct vir_file *vf, uint64_t length, const char *data);
struct vf_entry *vf_add_file_offset(struct vir_file *vf, uint8_t fileIndex, uint64_t length, uint64_t offset);
int save_vf(struct vir_file *vf, const char *filePath) ;
int read_from_vf(struct vir_file *vf, char *buf, size_t size, uint64_t offset);
void print_vf(struct vir_file *vf);
