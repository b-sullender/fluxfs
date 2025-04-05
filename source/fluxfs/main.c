
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define FUSE_USE_VERSION 30
#include <fuse.h>

#include "../lib/fluxfs.h"

struct fluxfs_file {
	char *name;
	uint64_t size;
	struct fluxfs_file *next;
};

struct fluxfs_dir {
	char *name;
	struct fluxfs_dir *parent;
	struct fluxfs_dir *subdirs;
	struct fluxfs_dir *next;
	struct fluxfs_file *files;
};

static struct fluxfs_dir *root = NULL;

// Get a directory
struct fluxfs_dir *get_directory(struct fluxfs_dir *parent, const char *dirname) {
	// Search for existing directory
	struct fluxfs_dir *current = parent->subdirs;
	while (current) {
		if (strcmp(current->name, dirname) == 0) {
			return current;
		}
		current = current->next;
	}

	return NULL;
}

// Get or create a directory
struct fluxfs_dir *goc_directory(struct fluxfs_dir *parent, const char *dirname) {
	// Search for existing directory
	struct fluxfs_dir *current = parent->subdirs;
	while (current) {
		if (strcmp(current->name, dirname) == 0) {
			return current;
		}
		current = current->next;
	}

	// If directory doesn't exist, create it
	struct fluxfs_dir *newdir = malloc(sizeof(struct fluxfs_dir));
	memset(newdir, 0, sizeof(struct fluxfs_dir));
	newdir->name = strdup(dirname);
	newdir->parent = parent;
	newdir->next = parent->subdirs;
	parent->subdirs = newdir;

	return newdir;
}

struct fluxfs_file *add_file_to_directory(struct fluxfs_dir *dir, const char *filename, uint64_t size) {
	struct fluxfs_file *newfile = malloc(sizeof(struct fluxfs_file));

	newfile->name = strdup(filename);
	newfile->size = size;
	newfile->next = dir->files;
	dir->files = newfile;

	return newfile;
}

int add_virtual_file(const char *vpath, uint64_t size) {
	struct fluxfs_dir *current = root;
	char *path = strdup(vpath);
	char *token = strtok(path, "/");

	while (token) {
		char *next_token = strtok(NULL, "/");
		if (next_token) {
			current = goc_directory(current, token);
		} else {
			add_file_to_directory(current, token, size);
		}
		token = next_token;
	}

	return EXIT_SUCCESS;
}

char **get_scan_directories(const char *filename, size_t *line_count) {
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		perror("Error opening file");
		*line_count = 0;
		return NULL;
	}

	char **lines = NULL;
	size_t count = 0;
	char *line = NULL;
	size_t len = 0;

	while (getline(&line, &len, file) != -1) {
		// Remove trailing newline safely
		size_t line_length = strlen(line);
		if (line_length > 0 && line[line_length - 1] == '\n') {
			line[line_length - 1] = '\0';
		}

		// Resize the array to hold one more line
		char **temp = realloc(lines, (count + 1) * sizeof(char *));
		if (!temp) {
			perror("Memory allocation failed");

			// Free previously allocated lines before returning
			for (size_t i = 0; i < count; i++) {
				free(lines[i]);
			}
			free(lines);
			free(line);
			fclose(file);
			*line_count = 0;

			return NULL;
		}
		lines = temp;

		lines[count] = line;
		line = NULL;
		count++;
	}

	fclose(file);
	free(line);

	*line_count = count;

	return lines;
}

void scan_directory(const char *dir_path, char ***virtual_files, size_t *count) {
	DIR *dir = opendir(dir_path);
	if (!dir) {
		perror("Could not open directory");
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		// Skip "." and ".."
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		// Construct full path: directory + "/" + filename
		size_t path_len = strlen(dir_path) + strlen(entry->d_name) + 2;
		char *full_path = malloc(path_len);
		if (!full_path) {
			perror("Memory allocation failed");
			continue;
		}
		snprintf(full_path, path_len, "%s/%s", dir_path, entry->d_name);

		struct stat path_stat;
		if (stat(full_path, &path_stat) == 0) {
			if (S_ISDIR(path_stat.st_mode)) {
				// If it's a directory, recurse into it
				scan_directory(full_path, virtual_files, count);
			} else if (S_ISREG(path_stat.st_mode)) {
				// If it's a regular file, check for ".vf" extension
				size_t len = strlen(entry->d_name);
				if (len > 3 && strcmp(entry->d_name + len - 3, ".vf") == 0) {
					// Resize virtual_files array
					char **temp = realloc(*virtual_files, (*count + 1) * sizeof(char *));
					if (!temp) {
						perror("Memory allocation failed");
						free(full_path);
						continue;
					}
					*virtual_files = temp;

					(*virtual_files)[*count] = full_path;
					(*count)++;
					continue;
				}
			}
		}
		free(full_path);
	}
	closedir(dir);
}

char **find_virtual_files(char **directories, size_t dir_count, size_t *file_count) {
	char **virtual_files = NULL;
	*file_count = 0;

	for (size_t i = 0; i < dir_count; i++) {
		scan_directory(directories[i], &virtual_files, file_count);
	}

	return virtual_files;
}

void print_fs(struct fluxfs_dir *dir, int depth) {
	if (dir == NULL) {
		return;
	}

	// Indentation for visual hierarchy
	for (int i = 0; i < depth; ++i) {
		printf("  ");
	}
	printf("[DIR] %s\n", dir->name);

	// Print all files in this directory
	struct fluxfs_file *file = dir->files;
	while (file != NULL) {
		for (int i = 0; i < depth + 1; ++i) {
			printf("  ");
		}
		printf("- %s\n", file->name);
		file = file->next;
	}

	// Recursively print all subdirectories
	struct fluxfs_dir *subdir = dir->subdirs;
	while (subdir != NULL) {
		print_fs(subdir, depth + 1);
		subdir = subdir->next;
	}
}

static int do_getattr(const char *path, struct stat *st) {
	printf("[getattr] Called\n");
	printf("\tAttributes of %s requested\n", path);

	if (strcmp(path, "/") == 0) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_atime = time(NULL);
		st->st_mtime = time(NULL);
		return 0;
	}

	struct fluxfs_dir *current = root;
	char *lpath = strdup(path);
	char *token, *saveptr;

	token = strtok_r(lpath, "/", &saveptr);
	char *filename = NULL;

	while (token) {
		char *next_token = strtok_r(NULL, "/", &saveptr);
		if (next_token) {
			current = get_directory(current, token);
			if (!current) {
				free(lpath);
				return -ENOENT;
			}
		}
		if (!next_token) {
			filename = token;
		}
		token = next_token;
	}

	int found = 0;

	if (current->subdirs) {
		struct fluxfs_dir *subdir = current->subdirs;
		while (subdir) {
			if (strcmp(subdir->name, filename) == 0) {
				st->st_mode = S_IFDIR | 0755;
				st->st_nlink = 2;
				found = 1;
				break;
			}
			subdir = subdir->next;
		}
	}

	if (!found && current->files) {
		struct fluxfs_file *file = current->files;
		while (file) {
			if (strcmp(file->name, filename) == 0) {
				st->st_mode = S_IFREG | 0644;
				st->st_nlink = 1;
				st->st_size = file->size;
				found = 1;
				break;
			}
			file = file->next;
		}
	}

	if (!found) {
		free(lpath);
		return -ENOENT;
	}

	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);

	free(lpath);

	return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	printf("--> Getting The List of Files of %s\n", path);

	struct fluxfs_dir *current = root;
	char *lpath = strdup(path);
	char *token, *saveptr;

	token = strtok_r(lpath, "/", &saveptr);
	while (token) {
		current = get_directory(current, token);
		if (!current) {
			free(lpath);
			return -ENOENT;
		}
		token = strtok_r(NULL, "/", &saveptr);
	}

	printf("Found: %s\n", current ? current->name : "NULL");

	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);

	if (current->subdirs) {
		struct fluxfs_dir *subdir = current->subdirs;
		while (subdir) {
			filler(buffer, subdir->name, NULL, 0);
			subdir = subdir->next;
		}
	}

	if (current->files) {
		struct fluxfs_file *file = current->files;
		while (file) {
			filler(buffer, file->name, NULL, 0);
			file = file->next;
		}
	}

	free(lpath);

	return 0;
}

static struct fuse_operations operations = {
	.getattr	= do_getattr,
	.readdir	= do_readdir,
	//.read	= do_read,
};

int main(int argc, char *argv[]) {
	size_t dir_count, file_count;
	char **directories = get_scan_directories("scan.conf", &dir_count);

	if (!directories) {
		printf("No scan directories found.\n");
		return EXIT_FAILURE;
	}

	char **virtual_files = find_virtual_files(directories, dir_count, &file_count);

	if (virtual_files) {
		printf("Found Files:\n");
		for (size_t i = 0; i < file_count; i++) {
			printf("%s\n", virtual_files[i]);
		}
	} else {
		printf("No virtual files found.\n");
	}

	root = malloc(sizeof(struct fluxfs_dir));
	memset(root, 0, sizeof(struct fluxfs_dir));

	printf("Virtual Paths:\n");
	for (size_t i = 0; i < file_count; i++) {
		char *vpath = fluxfs_get_vpath(virtual_files[i]);
		if (vpath) {
			printf("%s\n", vpath);
			size_t size = fluxfs_get_vf_size(virtual_files[i]);
			add_virtual_file(vpath, size);
		}
	}

	printf("FluxFS File System:\n");
	print_fs(root, 0);

	fuse_main(argc, argv, &operations, NULL);

	for (size_t i = 0; i < dir_count; i++) {
		free(directories[i]);
	}
	free(directories);

	return EXIT_SUCCESS;
}
