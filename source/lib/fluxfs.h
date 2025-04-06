#ifndef FLUXFS_H
#define FLUXFS_H

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

struct fluxfs_vf {
	FILE *files[256];
	char *vpath;
	struct vf_strings *strings;
	struct vf_entry *head;
	struct vf_entry *tail;
	uint64_t size;
};

void fluxfs_free_vf(struct fluxfs_vf *vf);
char *fluxfs_get_vpath(const char *filePath);
uint64_t fluxfs_get_vf_size(const char *filePath);
struct fluxfs_vf *fluxfs_load_vf(const char *filePath);
struct fluxfs_vf *fluxfs_create_vf(char *path);
uint8_t fluxfs_vf_add_path(struct fluxfs_vf *vf, const char *filePath);
struct vf_entry *fluxfs_vf_add_data(struct fluxfs_vf *vf, uint64_t length, const char *data);
struct vf_entry *fluxfs_vf_add_file_offset(struct fluxfs_vf *vf, uint8_t fileIndex, uint64_t length, uint64_t offset);
int fluxfs_save_vf(struct fluxfs_vf *vf, const char *filePath) ;
int fluxfs_read_from_vf(struct fluxfs_vf *vf, char *buf, size_t size, uint64_t offset);
void fluxfs_print_vf(struct fluxfs_vf *vf);

#endif // !FLUXFS_H
