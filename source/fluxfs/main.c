
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "../lib/fluxfs.h"

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

int main() {
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

	printf("Virtual Paths:\n");
	for (size_t i = 0; i < file_count; i++) {
		char *vpath = fluxfs_get_vpath(virtual_files[i]);
		if (vpath) {
			printf("%s\n", vpath);
		}
	}

	for (size_t i = 0; i < dir_count; i++) {
		free(directories[i]);
	}
	free(directories);

	return EXIT_SUCCESS;
}
