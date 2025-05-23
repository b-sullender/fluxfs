
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <inttypes.h>
#include <setjmp.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

#include "fluxfs.h"

uint8_t read_uint8(FILE *file, jmp_buf *env) {
	uint8_t value;
	if (fread(&value, sizeof(uint8_t), 1, file) == 0) {
		longjmp(*env, 1);
	}
	return value;
}

uint16_t read_uint16(FILE *file, jmp_buf *env) {
	uint16_t value;
	if (fread(&value, sizeof(uint16_t), 1, file) == 0) {
		longjmp(*env, 1);
	}
	return value;
}

uint32_t read_uint32(FILE *file, jmp_buf *env) {
	uint32_t value;
	if (fread(&value, sizeof(uint32_t), 1, file) == 0) {
		longjmp(*env, 1);
	}
	return value;
}

uint64_t read_uint64(FILE *file, jmp_buf *env) {
	uint64_t value;
	if (fread(&value, sizeof(uint64_t), 1, file) == 0) {
		longjmp(*env, 1);
	}
	return value;
}

void read_string(FILE *file, jmp_buf *env, char *buffer, size_t bufLength) {
	if (bufLength == 0) {
		return;
	}

	size_t i = 0;
	buffer[i] = read_uint8(file, env);
	while (buffer[i] != 0) {
		i++;
		if (i >= bufLength) {
			// Force null-termination to avoid overflow
			buffer[i - 1] = 0;
			return;
		}
		buffer[i] = read_uint8(file, env);
	}
}

uint64_t read_length(FILE *file, jmp_buf *env, int lenSize) {
	if (lenSize == 0) {
		return read_uint8(file, env);
	} else if (lenSize == 1) {
		return read_uint16(file, env);
	} else if (lenSize == 2) {
		return read_uint32(file, env);
	} else {
		return read_uint64(file, env);
	}
}

uint64_t read_offset(FILE *file, jmp_buf *env, int offsetSize) {
	return read_length(file, env, offsetSize);
}

void read_data(FILE *file, jmp_buf *env, struct vf_entry *entry) {
	entry->data.bytes = malloc(entry->length);
	for (uint64_t i = 0; i < entry->length; i++) {
		entry->data.bytes[i] = read_uint8(file, env);
	}
}

void fluxfs_free_vf(struct fluxfs_vf *vf) {
	if (vf) {
		if (vf->vpath) {
			free(vf->vpath);
		}
		if (vf->strings) {
			for (uint8_t i = 0; i < vf->strings->cnt; i++) {
				if (vf->files[i]) {
					fclose(vf->files[i]);
				}
				if (vf->strings->paths[i]) {
					free(vf->strings->paths[i]);
				}
			}
			free(vf->strings);
		}
		struct vf_entry *current = vf->head;
		while (current) {
			if (current->type == 0) {
				free(current->data.bytes);
			}
			struct vf_entry *next = current->next;
			free(current);
			current = next;
		}
		free(vf);
	}
}

char *fluxfs_get_vpath(const char *filePath) {
	FILE *file = fopen(filePath, "rb");
	if (!file) {
		perror("Error opening file");
		return NULL;
	}

	char *vpath = NULL;
	jmp_buf env;

	if (setjmp(env) == 1) {
		fclose(file);
		return vpath;
	}

	char signature[10];
	read_string(file, &env, signature, 10);
	if (strcmp(signature, "FluxFS VF") != 0) {
		fprintf(stderr, "%s is not a FluxFS virtual file (invalid signature)", filePath);
		fclose(file);
		return NULL;
	}

	uint16_t pathLen = read_uint16(file, &env);
	vpath = malloc(pathLen);
	if (!vpath) {
		perror("malloc failed");
		fclose(file);
		return NULL;
	}
	read_string(file, &env, vpath, pathLen);

	return vpath;
}

uint64_t fluxfs_get_vf_size(const char *filePath) {
	struct fluxfs_vf *vf = fluxfs_load_vf(filePath);
	if (!vf) {
		return 0;
	}
	uint64_t size = vf->size;

	fluxfs_free_vf(vf);

	return size;
}

struct fluxfs_vf *fluxfs_load_vf(const char *filePath) {
	char oldCwd[PATH_MAX];
	if (!getcwd(oldCwd, sizeof(oldCwd))) {
		perror("getcwd failed");
		return NULL;
	}

	char pathCopy[PATH_MAX];
	strncpy(pathCopy, filePath, PATH_MAX);
	char *dir = dirname(pathCopy);
	if (chdir(dir) != 0) {
		perror("chdir failed");
		return NULL;
	}

	FILE *file = fopen(basename((char *)filePath), "rb");
	if (!file) {
		perror("Error opening file");
		chdir(oldCwd);
		return NULL;
	}

	struct fluxfs_vf *vf = NULL;
	jmp_buf env;

	if (setjmp(env) == 1) {
		fclose(file);
		chdir(oldCwd);
		return vf;
	}

	vf = malloc(sizeof(struct fluxfs_vf));
	if (!vf) {
		perror("malloc failed");
		goto error;
	}
	memset(vf, 0, sizeof(struct fluxfs_vf));

	struct vf_strings *strings = malloc(sizeof(struct vf_strings));
	if (!strings) {
		perror("malloc failed");
		goto error;
	}
	vf->strings = strings;

	char signature[10];
	read_string(file, &env, signature, 10);
	if (strcmp(signature, "FluxFS VF") != 0) {
		fprintf(stderr, "%s is not a FluxFS virtual file (invalid signature)", filePath);
		goto error;
	}

	uint16_t pathLen = read_uint16(file, &env);
	vf->vpath = malloc(pathLen);
	if (!vf->vpath) {
		perror("malloc failed");
		goto error;
	}
	read_string(file, &env, vf->vpath, pathLen);

	strings->cnt = read_uint8(file, &env);
	for (int i = 0; i < strings->cnt; i++) {
		pathLen = read_uint16(file, &env);
		strings->paths[i] = malloc(pathLen);
		if (!strings->paths[i]) {
			perror("malloc failed");
			goto error;
		}
		read_string(file, &env, strings->paths[i], pathLen);
		vf->files[i] = fopen(strings->paths[i], "rb");
		if (!vf->files[i]) {
			fprintf(stderr, "Error opening file: %s\n", vf->strings->paths[i]);
			goto error;
		}
	}

	while (!feof(file)) {
		struct vf_entry *entry = malloc(sizeof(struct vf_entry));
		if (!entry) {
			perror("malloc failed");
			goto error;
		}
		memset(entry, 0, sizeof(struct vf_entry));
		entry->type = read_uint8(file, &env);
		uint8_t type = entry->type & 1;
		entry->length = read_length(file, &env, (entry->type >> 1) & 3);
		vf->size += entry->length;
		if (type == 0) {
			read_data(file, &env, entry);
		} else {
			entry->data.offset = read_offset(file, &env, (entry->type >> 3) & 3);
			entry->pathIndex = entry->type >> 5;
			if (entry->pathIndex == 7) {
				entry->pathIndex = read_uint8(file, &env);
			}
		}
		entry->type = type;
		entry->next = NULL;
		if (vf->tail) {
			vf->tail->next = entry;
		} else {
			vf->head = entry;
		}
		vf->tail = entry;
	}

	fclose(file);
	chdir(oldCwd);
	return vf;

	error:
	fluxfs_free_vf(vf);
	fclose(file);
	chdir(oldCwd);
	return NULL;
}

struct fluxfs_vf *fluxfs_create_vf(char *vpath) {
	struct fluxfs_vf *vf = NULL;

	vf = malloc(sizeof(struct fluxfs_vf));
	if (!vf) {
		return NULL;
	}
	memset(vf, 0, sizeof(struct fluxfs_vf));
	vf->vpath = strdup(vpath);
	if (!vf->vpath) {
		free(vf);
		return NULL;
	}

	struct vf_strings *strings = malloc(sizeof(struct vf_strings));
	if (!strings) {
		free(vf->vpath);
		free(vf);
		return NULL;
	}
	memset(strings, 0, sizeof(struct vf_strings));
	vf->strings = strings;

	return vf;
}

uint8_t fluxfs_vf_add_path(struct fluxfs_vf *vf, const char *filePath) {
	uint8_t i = vf->strings->cnt;
	vf->strings->paths[i] = strdup(filePath);
	vf->strings->cnt++;
	return i;
}

struct vf_entry *fluxfs_vf_add_data(struct fluxfs_vf *vf, uint64_t length, const char *data) {
	struct vf_entry *entry = malloc(sizeof(struct vf_entry));
	if (!entry) {
		return NULL;
	}
	memset(entry, 0, sizeof(struct vf_entry));

	entry->type = 0;
	entry->length = length;
	entry->data.bytes = malloc(length);
	if (!entry->data.bytes) {
		free(entry);
		return NULL;
	}
	memcpy(entry->data.bytes, data, length);

	if (vf->tail) {
		vf->tail->next = entry;
	} else {
		vf->head = entry;
	}
	vf->tail = entry;

	return entry;
}

struct vf_entry *fluxfs_vf_add_file_offset(struct fluxfs_vf *vf, uint8_t fileIndex, uint64_t length, uint64_t offset) {
	struct vf_entry *entry = malloc(sizeof(struct vf_entry));
	if (!entry) {
		return NULL;
	}
	memset(entry, 0, sizeof(struct vf_entry));

	entry->type = 1;
	entry->pathIndex = fileIndex;
	entry->length = length;
	entry->data.offset = offset;

	if (vf->tail) {
		vf->tail->next = entry;
	} else {
		vf->head = entry;
	}
	vf->tail = entry;

	return entry;
}

int fluxfs_save_vf(struct fluxfs_vf *vf, const char *filePath) {
	FILE *file = fopen(filePath, "wb");
	if (!file) {
		perror("Error opening file");
		return 1;
	}

	char signature[] = "FluxFS VF";
	uint16_t stringLen = strlen(signature) + 1;
	fwrite(signature, stringLen, 1, file);

	stringLen = strlen(vf->vpath) + 1;
	fwrite(&stringLen, 2, 1, file);
	fwrite(vf->vpath, stringLen, 1, file);

	fwrite(&vf->strings->cnt, 1, 1, file);
	for (uint8_t i = 0; i < vf->strings->cnt; i++) {
		stringLen = strlen(vf->strings->paths[i]) + 1;
		fwrite(&stringLen, 2, 1, file);
		fwrite(vf->strings->paths[i], stringLen, 1, file);
	}

	struct vf_entry *entry = vf->head;
	while (entry) {
		uint8_t lengthSize;
		uint8_t offsetSize = 0;
		uint8_t pathIndex = 0;

		if (entry->length > UINT32_MAX) {
			lengthSize = 3; // uint64_t
		} else if (entry->length > UINT16_MAX) {
			lengthSize = 2; // uint32_t
		} else if (entry->length > UINT8_MAX) {
			lengthSize = 1; // uint16_t
		} else {
			lengthSize = 0; // uint8_t
		}

		if (entry->type == 1) {
			if (entry->data.offset > UINT32_MAX) {
				offsetSize = 3; // uint64_t
			} else if (entry->data.offset > UINT16_MAX) {
				offsetSize = 2; // uint32_t
			} else if (entry->data.offset > UINT8_MAX) {
				offsetSize = 1; // uint16_t
			} else {
				offsetSize = 0; // uint8_t
			}
			if (entry->pathIndex > 6) {
				pathIndex = 7;
			} else {
				pathIndex = entry->pathIndex;
			}
		}

		uint8_t type = entry->type | (lengthSize << 1) | (offsetSize << 3) | (pathIndex << 5);
		fwrite(&type, 1, 1, file);

		if (lengthSize == 0) {
			uint8_t length = entry->length;
			fwrite(&length, 1, 1, file);
		} else if (lengthSize == 1) {
			uint16_t length = entry->length;
			fwrite(&length, 2, 1, file);
		} else if (lengthSize == 2) {
			uint32_t length = entry->length;
			fwrite(&length, 4, 1, file);
		} else {
			uint64_t length = entry->length;
			fwrite(&length, 8, 1, file);
		}

		if (entry->type == 0) {
			for (uint64_t i = 0; i < entry->length; i++) {
				fwrite(&entry->data.bytes[i], 1, 1, file);
			}
		} else {
			if (offsetSize == 0) {
				uint8_t offset = entry->data.offset;
				fwrite(&offset, 1, 1, file);
			} else if (offsetSize == 1) {
				uint16_t offset = entry->data.offset;
				fwrite(&offset, 2, 1, file);
			} else if (offsetSize == 2) {
				uint32_t offset = entry->data.offset;
				fwrite(&offset, 4, 1, file);
			} else {
				uint64_t offset = entry->data.offset;
				fwrite(&offset, 8, 1, file);
			}
			if (pathIndex == 7) {
				fwrite(&entry->pathIndex, 1, 1, file);
			}
		}

		entry = entry->next;
	}

	fclose(file);

	return 0;
}

/*int read_from_vf(struct fluxfs_vf *vf, char *buf, size_t size, off_t offset) {
	uint64_t vf_offset = 0;
	int bytesRead = 0;
	struct vf_entry *entry = vf->head;

	while (entry && size) {
		if (offset >= vf_offset && offset < (vf_offset + entry->length)) {
			size_t bytesToRead = size;
			if (size > entry->length) {
				bytesToRead = entry->length;
			}
			if (entry->type == 0) {
				uint64_t entryOffset = offset - vf_offset;
				memcpy(buf + bytesRead, &entry->data.bytes[entryOffset], bytesToRead);
			} else {
				FILE *file = vf->files[entry->pathIndex];
				//uint64_t fileOffset = offset - entry->data.offset;
				uint64_t fileOffset = (offset - vf_offset) + entry->data.offset;
				fseek(file, fileOffset, SEEK_SET);
				fread(buf + bytesRead, 1, bytesToRead, file);
			}
			bytesRead += bytesToRead;
			size -= bytesToRead;
			offset += bytesToRead;
		}

		vf_offset += entry->length;
		entry = entry->next;
	}

	return bytesRead;
}*/

int fluxfs_read_from_vf(struct fluxfs_vf *vf, char *buf, size_t size, uint64_t offset) {
	uint64_t vf_offset = 0;
	int bytesRead = 0;
	struct vf_entry *entry = vf->head;

	while (entry && size) {
		if (offset >= vf_offset && offset < (vf_offset + entry->length)) {
			size_t entryOffset = offset - vf_offset;
			size_t availableBytes = entry->length - entryOffset;
			size_t bytesToRead = (size < availableBytes) ? size : availableBytes;

			if (entry->type == 0) {
				memcpy(buf + bytesRead, &entry->data.bytes[entryOffset], bytesToRead);
			} else {
				FILE *file = vf->files[entry->pathIndex];
				uint64_t fileOffset = (offset - vf_offset) + entry->data.offset;
				if (fseek(file, fileOffset, SEEK_SET) != 0) {
					return -1;
				}
				size_t readBytes = fread(buf + bytesRead, 1, bytesToRead, file);
				if (readBytes < bytesToRead) {
					return -1;
				}
			}

			bytesRead += bytesToRead;
			size -= bytesToRead;
			offset += bytesToRead;
		}

		vf_offset += entry->length;
		entry = entry->next;
	}

	return bytesRead;
}

void fluxfs_print_vf(struct fluxfs_vf *vf) {
	printf("Virtual Path: %s\n", vf->vpath);
	printf("Virtual Size: %" PRIu64 "\n", vf->size);
	printf("Path Strings:\n");
	// Print path strings
	for (uint8_t i = 0; i < vf->strings->cnt; i++) {
		printf("%s\n", vf->strings->paths[i]);
	}
	printf("-------------------------------------------------\n");
	// Print entries
	struct vf_entry *entry = vf->head;
	while (entry) {
		printf("Length: %" PRIu64 "\n", entry->length);
		if (entry->type == 0) {
			printf("Data Entry\n");
			for (uint64_t i = 0; i < entry->length; i++) {
				printf("%02X ", entry->data.bytes[i]);
			}
			printf("\n");
		} else {
			printf("File Offset Entry\n");
			printf("Offset: %" PRIu64 "\n", entry->data.offset);
		}
		printf("-------------------------------------------------\n");
		entry = entry->next;
	}
}
