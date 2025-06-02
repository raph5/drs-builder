#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>

#ifndef strnlen
size_t strnlen(const char *s, size_t maxlen) {
  size_t i;
  for (i = 0; s[i] != '\0' && i < maxlen; ++i);
  return i;
}
#endif

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

typedef struct {
	char copyright[40];
	char version[4];
	char ftype[12];
	uint32_t table_count;
	uint32_t file_offset;
} DrsHeader;

typedef struct {
	char file_extension[4];
	uint32_t file_info_offset;
	uint32_t file_count;
} DrsTable;

typedef struct {
	uint32_t file_id;
	uint32_t file_offset;
	uint32_t file_size;
} DrsFileInfo;

DrsHeader *drs_read_header(FILE *drs) {
  DrsHeader *header = malloc(sizeof(*header));
  if (header == NULL) {
    perror("drs_read_header malloc");
    return NULL;
  }
  
  size_t items_read = fread(header, sizeof(*header), 1, drs);
  if (items_read < 1) {
    int err;
    if (feof(drs)) printf("drs_read_header fread eof\n");
    else if ((err = ferror(drs))) printf("drs_read_header fread error %d\n", err);
    else printf("drs_read_header fread error\n");
    free(header);
    return NULL;
  }

  if (memcmp(header->version, "1.00", 4) != 0) {
    char version[5];
    memcpy(version, header->version, 4);
    version[4] = '\0';
    printf("drs_read_header want version 1.00, got %s\n", version);
    free(header);
    return NULL;
  }
  if (memcmp(header->ftype, "tribe\0\0\0\0\0\0\0", 12) != 0) {
    char ftype[13];
    memcpy(ftype, header->ftype, 12);
    ftype[12] = '\0';
    printf("drs_read_header want ftype tribe, got %s\n", ftype);
    free(header);
    return NULL;
  }

  return header;
}

DrsTable *drs_read_table_array(FILE *drs, uint32_t table_count) {
  assert(table_count < 1024);
  DrsTable *table_array = malloc(table_count * sizeof(*table_array));
  if (table_array == NULL) {
    perror("drs_read_table_array malloc");
    return NULL;
  }

  size_t items_read = fread(table_array, sizeof(*table_array), table_count, drs);
  if (items_read < table_count) {
    int err;
    if (feof(drs)) printf("drs_read_table_array fread eof\n");
    else if ((err = ferror(drs))) printf("drs_read_table_array fread error %d\n", err);
    else printf("drs_read_table_array fread error\n");
    free(table_array);
    return NULL;
  }

  return table_array;
}

DrsFileInfo *drs_read_file_info_array(FILE *drs, uint32_t file_count, uint32_t file_info_offset) {
  assert(file_count < 4096);
  DrsFileInfo *file_info_array = malloc(file_count * sizeof(*file_info_array));
  if (file_info_array == NULL) {
    perror("drs_read_file_info_array malloc");
    return NULL;
  }

  int err = fseek(drs, file_info_offset, SEEK_SET);
  if (err) {
    perror("drs_read_file_info_array fseek");
    free(file_info_array);
    return NULL;
  }
  size_t items_read = fread(file_info_array, sizeof(*file_info_array), file_count, drs);
  if (items_read < file_count) {
    int err;
    if (feof(drs)) printf("drs_read_file_info_array fread eof\n");
    else if ((err = ferror(drs))) printf("drs_read_file_info_array fread error %d\n", err);
    else printf("drs_read_file_info_array fread error\n");
    free(file_info_array);
    return NULL;
  }

  return file_info_array;
}

#define OUT_FILE_NAME_MAX_LEN 64
int generate_out_file_name(DrsTable *table, DrsFileInfo *file_info, int table_idx, char *name) {
  char extension[5] = {0};
  for (size_t i = 0; i < 4; ++i) {
    if (table->file_extension[3-i] != ' ') {
      extension[i] = table->file_extension[3-i];
    }
  }

  int len = snprintf(name, OUT_FILE_NAME_MAX_LEN+1, "T%dF%d.%s", table_idx, file_info->file_id, extension);
  if (len >= OUT_FILE_NAME_MAX_LEN+1) {
    printf("generate_out_file_name snprintf error\n");
    return -1;
  }
  name[len] = '\0';
  return 0;
}

int copy_stream(FILE *dest, FILE *source, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    int c = fgetc(source);
    if (c == EOF) {
      perror("copy_stream fgetc");
      return -1;
    }
    c = fputc(c, dest);
    if (c == EOF) {
      perror("copy_stream fputc");
      return -1;
    }
  }
  return 0;
}

typedef struct {
  char *drs_file;
  char *out_dir;
} Args;

#define DRS_FILE_MAX_LEN 512
#define OUT_DIR_MAX_LEN 512
void args_parse(int argc, char *argv[], Args *args) {
  if (argc != 4) {
    printf("got %d arguments, want 3\n\n"
           "SYNOPSIS\n"
           "    drsb DRS_FILE -o OUT_DIR\n", argc);
    *args = (Args) {0};
    return;
  }
  if (strcmp(argv[2], "-o") != 0) {
    printf("got %s as second argument, want `-o`\n\n"
           "SYNOPSIS\n"
           "    drsb DRS_FILE -o OUT_DIR\n", argv[2]);
    *args = (Args) {0};
    return;
  }
  if (strnlen(argv[1], DRS_FILE_MAX_LEN+1) >= DRS_FILE_MAX_LEN) {
    printf("DRS_FILE length exeded the %d caracters allowed\n\n"
           "SYNOPSIS\n"
           "    drsb DRS_FILE -o OUT_DIR\n", DRS_FILE_MAX_LEN);
    *args = (Args) {0};
    return;
  }
  if (strnlen(argv[3], OUT_DIR_MAX_LEN+1) >= OUT_DIR_MAX_LEN) {
    printf("OUT_DIR length exeded the %d caracters allowed\n\n"
           "SYNOPSIS\n"
           "    drsb DRS_FILE -o OUT_DIR\n", OUT_DIR_MAX_LEN);
    *args = (Args) {0};
    return;
  }
  *args = (Args) {
    .drs_file = argv[1],
    .out_dir = argv[3],
  };
}

int main(int argc, char *argv[]) {
  FILE *drs = NULL, *out_file = NULL;
  DrsHeader *header = NULL;
  DrsTable *table_array = NULL;
  DrsFileInfo *file_info_array = NULL;

  Args args;
  args_parse(argc, argv, &args);
  if (args.drs_file == NULL) {
    goto error;
  }

  drs = fopen(args.drs_file, "r");
  if (drs == NULL) {
    perror("fopen");
    goto error;
  }
  int err = mkdir(args.out_dir, 0755);
  if (err && errno != EEXIST) {
    perror("mkdir");
    goto error;
  }

  header = drs_read_header(drs);
  if (header == NULL) {
    goto error;
  }
  table_array = drs_read_table_array(drs, header->table_count);
  if (table_array == NULL) {
    goto error;
  }

  for (size_t i = 0; i < header->table_count; ++i) {
    DrsTable *table = &table_array[i];
    file_info_array = drs_read_file_info_array(drs, table->file_count, table->file_info_offset);
    if (file_info_array == NULL) {
      goto error;
    }

    for (size_t j = 0; j < table->file_count; ++j) {
      DrsFileInfo *file_info = &file_info_array[j];

      char out_file_name[OUT_DIR_MAX_LEN + OUT_FILE_NAME_MAX_LEN + 2] = {0};
      size_t out_dir_len = strnlen(args.out_dir, OUT_DIR_MAX_LEN+1);
      memcpy(out_file_name, args.out_dir, out_dir_len);
      out_file_name[out_dir_len] = PATH_SEPARATOR;
      int err = generate_out_file_name(table, file_info, i, out_file_name + out_dir_len + 1);
      if (err) {
        goto error;
      }

      FILE *out_file = fopen(out_file_name, "w");
      if (out_file == NULL) {
        goto error;
      }
      err = fseek(drs, file_info->file_offset, SEEK_SET);
      if (err) {
        perror("out_file fseek");
        goto error;
      }
      err = copy_stream(out_file, drs, file_info->file_size);
      if (err) {
        goto error;
      }
      fclose(out_file);
      out_file = NULL;
    }

    free(file_info_array);
    file_info_array = NULL;
  }
  
  free(header);
  free(table_array);
  fclose(drs);
  return 0;

error:
  free(header);
  free(table_array);
  free(file_info_array);
  if (drs != NULL) fclose(drs);
  if (out_file != NULL) fclose(out_file);
}
