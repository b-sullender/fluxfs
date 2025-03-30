
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "../lib/fluxfs.h"

// Simple file to use as a data reference
int createSourceFile() {
	unsigned char sourceFile[] = {
		0xDE, 0xAD, 0xBE, 0xEF, 0x00,
		0xFF, 0x12, 0x34, 0x40, 0x30,
		0x64, 0x10, 0x92, 0x29, 0x43,
		0x78, 0x83, 0x37, 0x08, 0xCD,
		0x44, 0xED, 0x02, 0xD3, 0xC0
	};

	size_t arraySize = sizeof(sourceFile);

	FILE *file = fopen("source.bin", "wb");
	if (!file) {
		perror("Error opening file");
		return EXIT_FAILURE;
	}

	size_t written = fwrite(sourceFile, sizeof(unsigned char), arraySize, file);
	if (written != arraySize) {
		perror("Error writing to file");
		fclose(file);
		return EXIT_FAILURE;
	}

	fclose(file);
	printf("Test file written to source.bin\n");

	return EXIT_SUCCESS;
}

// This is an example of creating a virtual file
int createVirtualFile(const char *filePath) {
	// The path arg is the virtual path on the file system
	struct vir_file *vf = create_vf("files/bytes.bin");
	if (!vf) {
		fprintf(stderr, "Failed to create virtual file\n");
		return EXIT_FAILURE;
	}

	// Add the path of the source file
	uint8_t fileIndex = vf_add_path(vf, "source.bin");

	// Some metadata
	char data1[] = {
		0x45, 0x80, 0xF3, 0x12, 0x00,
		0x5F, 0x1A, 0x31, 0x10, 0xF3
	};
	vf_add_data(vf, 10, data1);

	// Create a reference into the source file
	vf_add_file_offset(vf, fileIndex, 10, 5);

	// More meatadata
	char data2[] = {
		0x78, 0x40, 0x21, 0x37, 0x98,
		0xA2, 0xB9, 0x11, 0x23, 0x77
	};
	vf_add_data(vf, 10, data2);

	// Save the file
	if (save_vf(vf, filePath) != EXIT_SUCCESS) {
		fprintf(stderr, "Failed to save virtual file\n");
		return EXIT_FAILURE;
	}

	printf("Test file written to %s\n", filePath);

	return EXIT_SUCCESS;
}

// The bytes that would be read from virfile.vf
static char expected_bytes[] = {
	0x45, 0x80, 0xF3, 0x12, 0x00,
	0x5F, 0x1A, 0x31, 0x10, 0xF3,
	0xFF, 0x12, 0x34, 0x40, 0x30, // 2nd row of source.bin
	0x64, 0x10, 0x92, 0x29, 0x43, // 3rd row of source.bin
	0x78, 0x40, 0x21, 0x37, 0x98,
	0xA2, 0xB9, 0x11, 0x23, 0x77
};

// This function uses read_from_vf to make sure the expected bytes are read
int test_vf(struct vir_file *vf) {
	printf("Virtual Read Test:\n");

	// Pass 1 tests reading each byte one-by-one

	char buffer[4];
	printf("Running Pass 1...\n");
	for (uint64_t i = 0; i < vf->size; i++) {
		read_from_vf(vf, buffer, 1, i);
		if (buffer[0] != expected_bytes[i]) {
			printf("Pass 1 Failed\n");
			return EXIT_FAILURE;
		}
	}
	printf("Pass 1 Successful\n");

	// Pass 2 tests reading 2 bytes at a time

	printf("Running Pass 2...\n");
	for (uint64_t i = 0; i < (vf->size - 1); i++) {
		read_from_vf(vf, buffer, 2, i);
		if ((buffer[0] != expected_bytes[i]) || (buffer[1] != expected_bytes[i + 1])) {
			printf("Pass 2 Failed\n");
			printf("Bytes read: 0x%02X 0x%02X\n", buffer[0], buffer[1]);
			printf("Bytes expected: 0x%02X 0x%02X\n", expected_bytes[i], expected_bytes[i + 1]);
			printf("At Offset: %lu\n", i);
			return EXIT_FAILURE;
		}
	}
	printf("Pass 2 Successful\n");

	return EXIT_SUCCESS;
}

int main() {
	createSourceFile();
	createVirtualFile("fluxfs.vf");
	printf("-------------------------------------------------\n");

	struct vir_file *vf = load_vf("fluxfs.vf");
	if (!vf) {
		fprintf(stderr, "Failed to load virtual file\n");
		return EXIT_FAILURE;
	}

	print_vf(vf);
	test_vf(vf);

	free_vf(vf);

	return EXIT_SUCCESS;
}
