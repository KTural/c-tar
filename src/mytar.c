#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#define INSUFFICIENT_ARGS "need at least one option"
#define NOT_FOUND_FILE "mytar: %s: Not found in archive\n"
#define HEADER_ERROR "mytar: Unsupported header type: %d\n"
#define EXIT_ERROR "mytar: Exiting with failure status due to previous errors\n"
#define ARCHIVE_ERROR "mytar: This does not look like a tar archive\nmytar: Exiting with failure status due to previous errors\n"
#define UNKNOWN_SWITCH "Unknown option: -%c"
#define HELP "Usage: ./mytar [OPTION...] [FILE]...\n"\
			"'mytar' saves many files together into a single tape or disk archive, and can\n"\
			"restore individual files from the archive.\n\n"\
			"Examples:\n  ./mytar -f archive.tar -t # List all files in archive.tar.\n"\
			"  ./mytar -x -f archive.tar # Extract all files from archive.tar.\n"\
			"  ./mytar -v -x -f archive.tar # Extract all files from archive.tar verbosely.\n\n"\
			" Main operation mode:\n\n  -t\t\tlist the contents of an archive\n"\
			"  -x\t\textract files from an archive\n\n Device selection and switching:\n\n"\
			"  -f\t\tuse archive file or device ARCHIVE\n\n Informative output:\n\n"\
			"  -v\t\tverbosely list files processed\n\n Other options:\n\n"\
			"  --help\t\tgive this help list\n"\
			"  --usage\t\tgive a short usage message"
#define USAGE "Usage: ./mytar [-f ARCHIVE] [-f ARCHIVE -t] [-t -v -f ARCHIVE]\n"\
			"\t[-x -f ARCHIVE] [-v -x -f ARCHIVE] [--help] [--usage]..."
#define INVALID_SWITCHES_X_T "mytar: You must choose one of the '-x', '-t' options"
#define NO_ARCHIVE " %s: Cannot open: No such file or directory\nmytar: Error is not recoverable: exiting now"
#define INVALID_SWITCH_V "mytar: You must choose '-v' option with one of the '-x', '-t' options"
#define ERROR_BLOCK "mytar: A lone zero block at %d\n"
#define EOF_ERROR "mytar: Unexpected EOF in archive\nmytar: Error is not recoverable: exiting now\n"
#define NOT_FOUND_ARCHIVE "mytar: %s: Cannot open: No such file or directory\nmytar: Error is not recoverable: exiting now"

struct operations {
	int extract;
	int list_contents;
	int verbose;
};

struct posix_header {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
	char link_name[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char dev_major[8];
	char dev_minor[8];
	char pad[12];
	char prefix[155];
};

int handleExtraBlock(struct posix_header psx_head, FILE *cur_file, int block_size) {
	long cur_file_size;
	int rem_block_size = block_size - ftell(cur_file) % block_size;
	sscanf(psx_head.size, "%lo", &cur_file_size);

	int block_fix = fseek(cur_file, rem_block_size % block_size, SEEK_CUR);
	if (fseek(cur_file, cur_file_size, SEEK_CUR) || block_fix) 
		return 2;
	
	return 0;
}

void archiveVerbose(struct posix_header psx_head, int verbose) {
	if (verbose) 
		printf("%s\n", psx_head.name);
	
	fflush(stdout);
}

int handleExtraction(int rem_bytes, char *buffer, FILE *cur_file, int file_size, int *return_code,\
					FILE *extract_cur_file) {
	for (int i = rem_bytes; i > 0; i -= file_size) {
		int buffer_read_file = fread(buffer, file_size, 1, cur_file);

		if (buffer_read_file == 0) {
			if (feof(cur_file)) {
				*return_code = 2;
				break;
			} else 
				break;
		}
		int buffer_write_extract_file = fwrite(buffer, file_size, 1, extract_cur_file);

		if (buffer_write_extract_file == 0) {
			*return_code = 2;
			break;
		}

		if (rem_bytes - file_size < 0) 
			file_size = rem_bytes;

	}

	return *return_code;
}

int archiveExtract(FILE *cur_file, struct posix_header psx_head, int block_size) { 
	int return_code = 0;
	long file_block_size = block_size;
	char cur_buffer[block_size];
	long rem_bytes;

	FILE *extract_cur_file = fopen(psx_head.name, "wb");

	if (extract_cur_file == 0)
		return 2;

	sscanf(psx_head.size, "%lo", &rem_bytes);

	return_code = handleExtraction(rem_bytes, cur_buffer, cur_file, file_block_size, &return_code, extract_cur_file);
	int close_extract_cur_file = fclose(extract_cur_file);

	if (close_extract_cur_file) 
		return return_code;

	return return_code;
}


void handleInvalidSwitches(char *cur_file, int list_contents, int extract, int verbose) {
	FILE *read_cur_file = fopen(cur_file, "rb");

	if (!read_cur_file) 
		errx(2, NO_ARCHIVE, cur_file);

	if (verbose) {
		if (!extract && !list_contents) 
			errx(2, INVALID_SWITCH_V);
	}

	if (extract) {
		if (list_contents) 
			errx(2, INVALID_SWITCHES_X_T);
	}
}

int handleBlockAndBuffer(FILE *cur_file, int block_vals, int block_size) {
	char buffer_vals[block_size];
	int cur_char = fgetc(cur_file);
	int cur_buffer_read = fread(buffer_vals, block_size, 1, cur_file);
	int cur_block_val = block_vals + 1;
 
	int exit_val = 0;

	if (cur_char == EOF) 
		exit_val = 1;
	else {
		fseek(cur_file, -1, SEEK_CUR);
		fputc(cur_char, cur_file);
		exit_val = 0;
	}

	if (exit_val)
		fprintf(stderr, ERROR_BLOCK, cur_block_val);
	else if (cur_buffer_read == 0)
		exit(2);
	
	return 0;
}

void handleInput(int argc, char **argv, char **cur_file, int *extract, \
				int *list_contents, int *verbose, int *args_count, char ***files) {
	if (argc < 2) 
		errx(2, INSUFFICIENT_ARGS);

	for (int i = 1; i < argc; ++i) {
		char switch_char_zero = *(*(argv + i) + 0);
		char switch_char_one = *(*(argv + i) + 1);
		if (switch_char_zero == '-') {
			switch (switch_char_one) {
				case 'f':
					i = i + 1;
					*cur_file = *(argv + i);
					break;
				case 't':
					*list_contents = 1;
					// marker comment
					// fall through
				case 'v':
					*verbose = 1;
					break;
				case 'x':
					*extract = 1;
					break;
				default:
					if (switch_char_one == '-') {
						char cur_switch_two = *(*(argv + i) + 2);
						if (cur_switch_two == 'h') 
							errx(0, HELP);
						else if (cur_switch_two == 'u') 
							errx(0, USAGE);
					}
					errx(2, UNKNOWN_SWITCH, switch_char_one);
			}
		} else if (list_contents || extract) {
			*files = &*(argv + i);
			*args_count = argc - i;
			break;
		} 
	}
}

int processArchive(FILE *cur_file, struct posix_header psx_head, int verbose, int extract, int block_size, int args_count, char **files) {
	int none_block = 1;
	size_t val = 0;

	while (none_block) {
		int cur_exit_code;
		int cur_char = fgetc(cur_file);

		if (cur_char == EOF) 
			cur_exit_code = 1;
		else {
			fseek(cur_file, -1, SEEK_CUR);
			fputc(cur_char, cur_file);
			cur_exit_code = 0;
		}

		if (cur_exit_code) {
			if (fseek(cur_file, -1, SEEK_CUR))
				return 2;
			
			int new_cur_char = fgetc(cur_file);

			if (new_cur_char == EOF) {
				cur_exit_code = 1;
			} else {
				fseek(cur_file, -1, SEEK_CUR);
				fputc(new_cur_char, cur_file);
				cur_exit_code = 0;
			}
			
			if (cur_exit_code)
				return 2;

			return 0;
		}

		int block_vals = ftell(cur_file) / block_size;
		int cur_read_file_psx = fread(&psx_head, block_size, 1, cur_file);

		if (cur_read_file_psx == 0) 
			return 2;

		void *psx = &psx_head; 

		for (;val < (size_t) block_size; val++) {
			char *cur_psx = (char *) psx;
			if (cur_psx[val]) {
				cur_exit_code = 0;
				break;
			} else {
				cur_exit_code = 1;
				break;
			}
		}

		if (cur_exit_code) 
			return handleBlockAndBuffer(cur_file, block_vals, block_size);

		if (psx_head.typeflag != '0') {
			fprintf(stderr, HEADER_ERROR, psx_head.typeflag);
			return 5;
		}

		int new_check_val = 0;

		for (int i = 0; i < args_count; ++i) {
			int str_cmp_res = strcmp(psx_head.name, files[i]);
			if (str_cmp_res == 0) {
				files[i] = "";
				new_check_val = 1;
				break;
			}
		}
		
		if (new_check_val || args_count == 0) {
			archiveVerbose(psx_head, verbose);

			if (extract) {
				int rem_block_size = block_size - ftell(cur_file) % block_size;
				int block_fix = fseek(cur_file, rem_block_size % block_size, SEEK_CUR);
				int extract_exit_code = archiveExtract(cur_file, psx_head, block_size);

				if (block_fix)
					return 2;

				if (extract_exit_code)
					return extract_exit_code;	
				continue;
			}
		}
		handleExtraBlock(psx_head, cur_file, block_size);
	}

	return 0;
}

int handleExitValue(FILE *cur_file, int cur_exit_val) {
	int close_cur_file = fclose(cur_file);
	if (close_cur_file == 0)		
		return cur_exit_val;

	return 2;
}

int notFoundFileError(int args_count, char **files, int cur_exit_val, int not_found_file) {
	int cur_val = 0;

	while (cur_val < args_count) {
		if (*(*(files + cur_val))) {
			not_found_file = 1;
			fprintf(stderr, NOT_FOUND_FILE, *(files + cur_val));
		}
		cur_val += 1;
	}

	if (not_found_file) {
		fprintf(stderr, EXIT_ERROR);
		cur_exit_val = 2;
	}

	return cur_exit_val;
}


int handleFileErrors(int cur_exit_val, int args_count, char **files) {
	int not_found_file = 0;

	if (cur_exit_val == 0)
		cur_exit_val = notFoundFileError(args_count, files, cur_exit_val, not_found_file);
	else if (cur_exit_val == 1) {
		fprintf(stderr, ARCHIVE_ERROR);
		return 2;
	} else if (cur_exit_val == 2) {
		fprintf(stderr, EOF_ERROR);
		return 2;
	} else
		return 2;

	return cur_exit_val;
}

int main(int argc, char **argv) {
	int block_size = 512;
	int cur_exit_val;
	int args_count;

	struct operations op;
	struct posix_header psx_head;

	op.extract = 0;
	op.verbose = 0;
	op.list_contents = 0;

	char *file;

	char **files;

	handleInput(argc, argv, &file, &op.extract, &op.list_contents, &op.verbose, &args_count, &files);

	FILE *cur_file = fopen(file, "rb");

	handleInvalidSwitches(file, op.list_contents, op.extract, op.verbose);

	int cur_read = fread(&psx_head, block_size, 1, cur_file);
	int cmp_tar_magic = strcmp(psx_head.magic, "ustar  ");

	if (cur_read == 0) {
		if (feof(cur_file))
			cur_exit_val = 1;
		else 
			cur_exit_val = 2;
	} else {
		if (cmp_tar_magic)
			cur_exit_val = 1;
		 else 
			cur_exit_val = 0;
	}

	if (cur_exit_val == 0) {
		if (fseek(cur_file, 0, SEEK_SET))
			exit(2);
		cur_exit_val = processArchive(cur_file, psx_head, op.verbose, op.extract, block_size, args_count, files);
		
		fclose(cur_file);
	}

	return handleFileErrors(cur_exit_val, args_count, files);

}