#include <stdlib.h>
#include <string.h>
#include "texture_atlas.h"

texture_atlas *texture_atlas_create(int width, int height, SceGxmTextureFormat format)
{
	texture_atlas *atlas = malloc(sizeof(*atlas));
	if (!atlas)
		return NULL;

	bp2d_rectangle rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = width;
	rect.h = height;

	atlas->tex = vita2d_create_empty_texture_format(width, height, format);
	atlas->bp_root = bp2d_create(&rect);
	atlas->htab = int_htab_create(256);

	vita2d_texture_set_filters(atlas->tex, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);

	return atlas;
}

void texture_atlas_free(texture_atlas *atlas)
{
	vita2d_free_texture(atlas->tex);
	bp2d_free(atlas->bp_root);
	int_htab_free(atlas->htab);
	free(atlas);
}

extern int netdbg(const char *text, ...);

int texture_atlas_insert(texture_atlas *atlas, unsigned int character, int width, int height, int bitmap_left, int bitmap_top, int advance_x, int advance_y, int glyph_size, int *inserted_x, int *inserted_y)
{
	bp2d_size size;
	size.w = width;
	size.h = height;

	bp2d_node *new_node;
	if (!bp2d_insert(atlas->bp_root, &size, &new_node)) {
		netdbg("insert failed char: %d\n", character);
		const bp2d_size new_size = {
			atlas->bp_root->rect.w * 2,
			atlas->bp_root->rect.h * 2
		};
		bp2d_node *new_root;
		if (!bp2d_resize(atlas->bp_root, &new_size, &new_root)) {
			netdbg("\tresize failed\n");
			return 0;
		}

		netdbg("\tresized to %d x %d\n", new_root->rect.w, new_root->rect.h);

		atlas->bp_root = new_root;

		if (!bp2d_insert(atlas->bp_root, &size, &new_node)) {
			netdbg("\tinsert failed AGAIN\n");
			return 0;
		}

		vita2d_texture *new_tex;
		new_tex = vita2d_create_empty_texture_format(new_size.w, new_size.h,
			vita2d_texture_get_format(atlas->tex));
		if (!new_tex)
			return 0;

		netdbg("\tgot new tex\n");

		vita2d_texture_set_filters(new_tex, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);

		void *old_data = vita2d_texture_get_datap(atlas->tex);
		void *new_data = vita2d_texture_get_datap(new_tex);
		unsigned int old_width = vita2d_texture_get_width(atlas->tex);
		unsigned int new_width = vita2d_texture_get_width(new_tex);

		int i;
		for (i = 0; i < new_root->rect.h; i++) {
			memcpy(new_data + (new_root->rect.x + (new_root->rect.y + i)*new_width),
				old_data + i*old_width,
				old_width);
		}

		// FIXME: MEMLEAK!
		//vita2d_free_texture(atlas->tex);

		atlas->tex = new_tex;
	}

	atlas_htab_entry *entry = malloc(sizeof(*entry));

	entry->rect = new_node->rect;
	entry->bitmap_left = bitmap_left;
	entry->bitmap_top = bitmap_top;
	entry->advance_x = advance_x;
	entry->advance_y = advance_y;
	entry->glyph_size = glyph_size;

	if (!int_htab_insert(atlas->htab, character, entry)) {
		bp2d_delete(atlas->bp_root, new_node);
		return 0;
	}

	if (inserted_x)
		*inserted_x = entry->rect.x;
	if (inserted_y)
		*inserted_y = entry->rect.y;

	return 1;
}

int texture_atlas_insert_draw(texture_atlas *atlas, unsigned int character, const void *image, int width, int height, int bitmap_left, int bitmap_top, int advance_x, int advance_y, int glyph_size)
{
	int pos_x;
	int pos_y;
	if (!texture_atlas_insert(atlas, character, width, height, bitmap_left, bitmap_top, advance_x, advance_y, glyph_size, &pos_x, &pos_y))
		return 0;

	void *tex_data = vita2d_texture_get_datap(atlas->tex);
	unsigned int tex_width = vita2d_texture_get_width(atlas->tex);

	int i;
	for (i = 0; i < height; i++) {
		memcpy(tex_data + (pos_x + (pos_y + i)*tex_width), image + i*width, width);
	}

	return 1;
}

int texture_atlas_exists(const texture_atlas *atlas, unsigned int character)
{
	return int_htab_find(atlas->htab, character) != NULL;
}

int texture_atlas_get(texture_atlas *atlas, unsigned int character, bp2d_rectangle *rect, int *bitmap_left, int *bitmap_top, int *advance_x, int *advance_y, int *glyph_size)
{
	atlas_htab_entry *entry = int_htab_find(atlas->htab, character);
	if (!entry)
		return 0;

	rect->x = entry->rect.x;
	rect->y = entry->rect.y;
	rect->w = entry->rect.w;
	rect->h = entry->rect.h;
	*bitmap_left = entry->bitmap_left;
	*bitmap_top = entry->bitmap_top;
	*advance_x = entry->advance_x;
	*advance_y = entry->advance_y;
	if (glyph_size)
		*glyph_size = entry->glyph_size;

	return 1;
}
