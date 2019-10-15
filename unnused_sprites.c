#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

#define DEBUG

static char const *sprites_dir_name = "sprites";
static int sprites_dir_name_length = 7;

static int path_buffer_size = 2048;
static int file_read_buffer_size = 2048;

struct sprite_data
{
	char *id;
	char *name;
	int reference_count;
};

struct sprite_data_list
{
	struct sprite_data sprite;
	struct sprite_data_list *next;
};

#ifdef TRACE
#define _trace fprintf
#else
void _trace() {}
#endif

#ifdef DEBUG
#define _debug fprintf
#else
void _debug() {}
#endif

struct sprite_data_list *build_sprite_list(void)
{
	char current_sprite_file_name[path_buffer_size];

	// Initialize sprite list to be searched.
	struct sprite_data_list *sprite_data_list_head = malloc(sizeof(struct sprite_data_list));
	struct sprite_data_list *sprite_data_list_index = sprite_data_list_head;
	sprite_data_list_index->sprite.id = 0;
	sprite_data_list_index->sprite.name = 0;
	sprite_data_list_index->sprite.reference_count = 0;
	sprite_data_list_index->next = 0;

	// Open sprites dir.
	DIR *sprites_dir = opendir(sprites_dir_name);
	if (!sprites_dir)
	{
		fprintf(stderr, "Could not open sprites folder.");
		return sprite_data_list_head;
	}

	// Read entries in sprites dir.
	struct dirent *sprites_ent;
	while (sprites_ent = readdir(sprites_dir))
	{
		// Ignore '.', '..' and potentially hidden files.
		if (sprites_ent->d_name[0] == '.')
		{
			continue;
		}

		// Alias `sprites_ent->d_name` as `sprite_name`.
		char const *sprite_name = sprites_ent->d_name;
		_trace(stdout, "TRACE Processing sprite %s\n", sprite_name);

		// Compute `sprite_name` length, will be used to build the path
		// for the '.yy' file.
		int sprite_name_length = strlen(sprite_name);

		// Each GMS sprite is a folder with PNG files and a '.yy' file
		// with the same name as the sprite.  The '.yy' file has the
		// sprite ID, which we want to check whether it is being
		// referenced by objects.
		// `current_sprite_file_name` will hold a string in format:
		// "sprites\\<sprite_name>\\<sprite_name>.yy"
		memset(current_sprite_file_name, 0, path_buffer_size);
		strcpy(current_sprite_file_name, sprites_dir_name);
		current_sprite_file_name[sprites_dir_name_length] = '\\';
		strcpy(current_sprite_file_name + sprites_dir_name_length + 1, sprite_name);
		current_sprite_file_name[sprites_dir_name_length + sprite_name_length + 1] = '\\';
		strcpy(current_sprite_file_name + sprites_dir_name_length + 2 + sprite_name_length, sprite_name);
		strcpy(current_sprite_file_name + sprites_dir_name_length + 2 + (sprite_name_length * 2), ".yy");

		// Open '.yy' file.
		_trace(stdout, "TRACE Opening file %s\n", current_sprite_file_name);
		FILE *sprite_file = fopen(current_sprite_file_name, "r");

		// The sprite ID is always at the top of the file, so we
		// only read one part of it, no need to read the entire file.
		char file_read_buffer[file_read_buffer_size];
		memset(file_read_buffer, 0, file_read_buffer_size);
		fread(file_read_buffer, 1, file_read_buffer_size, sprite_file);

		// Close '.yy' file.
		fclose(sprite_file);

		// Find the "id" property.
		char *id_start = strstr(file_read_buffer, "\"id\":");

		// Extract the sprite ID.
		char sprite_id[37];
		memset(sprite_id, 0, 37);
		strncpy(sprite_id, id_start + 7, 36);

		// Add sprite data to the list to be searched.
		sprite_data_list_index->sprite.id = malloc(sizeof(char) * 37);
		strcpy(sprite_data_list_index->sprite.id, sprite_id);
		sprite_data_list_index->sprite.name = malloc(sizeof(char) * sprite_name_length + 1);
		strcpy(sprite_data_list_index->sprite.name, sprite_name);
		sprite_data_list_index->next = malloc(sizeof(struct sprite_data_list));
		sprite_data_list_index = sprite_data_list_index->next;
		sprite_data_list_index->sprite.id = 0;
		sprite_data_list_index->sprite.name = 0;
		sprite_data_list_index->sprite.reference_count = 0;
		sprite_data_list_index->next = 0;

		// Log sprite ID and name.
		_debug(stdout, "DEBUG Sprite  %s  %s\n", sprite_id, sprite_name);
	}

	// Close sprites dir.
	closedir(sprites_dir);

	return sprite_data_list_head;
}

void free_sprite_list(struct sprite_data_list *list)
{
	// Free allocated memory for sprite list.
	while (list)
	{
		struct sprite_data_list *current = list;
		list = list->next;
		if (current->sprite.id)
		{
			free(current->sprite.id);
		}
		if (current->sprite.name)
		{
			free(current->sprite.name);
		}
		free(current);
	}
}

char *load_file_content(char *file_name)
{
	FILE *file = fopen(file_name, "r");

	fseek(file, 0L, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0L, SEEK_SET);

	char *file_content = calloc(file_size + 1, sizeof(char));

	size_t elements_read = fread(file_content, 1, file_size, file);
	file_content[elements_read] = 0;

	fclose(file);

	return file_content;
}

void search_sprite_names(char *file_name, struct sprite_data_list *sprite_data_list_head)
{
	char *file_content = load_file_content(file_name);

	struct sprite_data_list *list_index = sprite_data_list_head;
	while (list_index)
	{
		if (list_index->sprite.name)
		{
			_trace(stdout, "TRACE Searching for  %s  in file  %s\n",
				list_index->sprite.name,
				file_name);
			if (strstr(file_content, list_index->sprite.name))
			{
				_debug(stdout, "DEBUG Sprite  %s  referenced by  %s\n",
					list_index->sprite.name,
					file_name);
				list_index->sprite.reference_count += 1;
			}
		}
		list_index = list_index->next;
	}

	free(file_content);
}

void search_sprite_ids(char *file_name, struct sprite_data_list *sprite_data_list_head)
{
	char *file_content = load_file_content(file_name);

	struct sprite_data_list *list_index = sprite_data_list_head;
	while (list_index)
	{
		if (list_index->sprite.id)
		{
			_trace(stdout, "TRACE Searching for  %s  in file  %s\n",
				list_index->sprite.id,
				file_name);
			if (strstr(file_content, list_index->sprite.id))
			{
				_debug(stdout, "DEBUG Sprite  %s  referenced by  %s  by id\n",
					list_index->sprite.name,
					file_name);
				list_index->sprite.reference_count += 1;
			}
		}
		list_index = list_index->next;
	}

	free(file_content);
}

void search_sprite_usage(char const *folder_name, struct sprite_data_list *sprite_data_list_head)
{
	// Open folder.
	DIR *dir = opendir(folder_name);
	if (!dir)
	{
		fprintf(stderr, "Could not open %s folder.", folder_name);
		return;
	}

	// Compute `folder_name` length, used to build subfolders' name.
	int folder_name_length = strlen(folder_name);

	// Read entries in folder.
	struct dirent *ent;
	while (ent = readdir(dir))
	{
		// Ignore '.', '..' and potentially hidden files.
		if (ent->d_name[0] == '.')
		{
			continue;
		}

		// Alias `name` to `ent->d_name`.
		char const *name = ent->d_name;

		_trace(stdout, "TRACE Processing dir %s\n", name);

		// Compute `name` length, used to build subfolder name.
		int name_length = strlen(name);

		// Open subfolder.
		char subfolder_name[path_buffer_size];
		memset(subfolder_name, 0, path_buffer_size);
		strcpy(subfolder_name, folder_name);
		subfolder_name[folder_name_length] = '\\';
		strcpy(subfolder_name + folder_name_length + 1, name);

		DIR *subdir = opendir(subfolder_name);

		// Read entries on current object subfolder.
		struct dirent *subent;
		while (subent = readdir(subdir))
		{
			// Ignore '.', '..' and potentially hidden files.
			if (subent->d_name[0] == '.')
			{
				continue;
			}

			// Alias `file_name` to `object_ent->d_name`.
			char const *file_name = subent->d_name;

			_trace(stdout, "TRACE Processing file %s\n", file_name);

			// Figure out file extension and handle accordingly.
			int file_name_length = strlen(file_name);
			if (file_name[file_name_length - 1] == 'l')
			{
				// .gml extension, search for sprite name.
				int subfolder_name_length = strlen(subfolder_name);

				char *full_file_name = malloc(sizeof(char) * (subfolder_name_length + file_name_length + 3));
				strcpy(full_file_name, subfolder_name);
				full_file_name[subfolder_name_length] = '\\';
				strcpy(full_file_name + subfolder_name_length + 1, file_name);

				search_sprite_names(full_file_name, sprite_data_list_head);

				free(full_file_name);
			}
			else if (file_name[file_name_length - 1] ==  'y')
			{
				// .yy extension, search for sprite id.
				int subfolder_name_length = strlen(subfolder_name);

				char *full_file_name = malloc(sizeof(char) * (subfolder_name_length + file_name_length + 3));
				strcpy(full_file_name, subfolder_name);
				full_file_name[subfolder_name_length] = '\\';
				strcpy(full_file_name + subfolder_name_length + 1, file_name);

				search_sprite_ids(full_file_name, sprite_data_list_head);

				free(full_file_name);
			}
		}

		// Close current subfolder.
		closedir(subdir);
	}

	// Close objects folder.
	closedir(dir);
}

int main(int argc, char **argv)
{
	struct sprite_data_list *sprite_data_list_head = build_sprite_list();

	search_sprite_usage("objects", sprite_data_list_head);
	search_sprite_usage("rooms", sprite_data_list_head);
	search_sprite_usage("scripts", sprite_data_list_head);
	search_sprite_usage("tilesets", sprite_data_list_head);

	struct sprite_data_list *list_index = sprite_data_list_head;
	while (list_index)
	{
		if (list_index->sprite.name && !list_index->sprite.reference_count)
		{
			fprintf(stdout, "INFO You can delete sprite %s\n", list_index->sprite.name);
		}
		list_index = list_index->next;
	}

	// Free allocated memory.
	free_sprite_list(sprite_data_list_head);

	fflush(stdout);
	fflush(stderr);

	// We are done.
	return 0;
}
