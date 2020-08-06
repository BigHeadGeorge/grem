#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define WINDOW_MIN_WIDTH  400
#define WINDOW_MIN_HEIGHT 150

#define FONT_PATH "/usr/share/fonts/TTF/DejaVuSans.ttf"

/* TODO: make the drawn image look a little less shit (color profile is off, aliased pixels) */
int draw_image(SDL_Renderer *r, SDL_Texture *img) {
	int w = 0, h = 0;
	SDL_GetRendererOutputSize(r, &w, &h);
	int min_dimension = w;
	if (h < min_dimension)
		min_dimension = h;

	SDL_Rect blit_area = { 0, 0, min_dimension, min_dimension };
	SDL_RenderCopy(r, img, NULL, &blit_area);
	return min_dimension;
}

void draw_text(SDL_Renderer *r, TTF_Font *font, char *s, int x, int y) {
	SDL_Rect pos = { x, y, 0, 0 };
	SDL_Color white = { 255, 255, 255, 255 };
	SDL_Surface *text_surface = TTF_RenderText_Blended(font, s, white);
	pos.w = text_surface->w;
	pos.h = text_surface->h;
	pos.y -= pos.h / 2;
	SDL_Texture *text = SDL_CreateTextureFromSurface(r, text_surface);
	SDL_RenderCopy(r, text, NULL, &pos);
	SDL_DestroyTexture(text);
	SDL_FreeSurface(text_surface);
}

struct playing {
	char *song;
	char *artist;
	char *cover_path;
};

void draw_everything(SDL_Renderer *r, SDL_Texture *img, TTF_Font *font, struct playing *p) {
	SDL_RenderClear(r);
	int x = draw_image(r, img) + 10;
	int w = 0, h = 0;
	SDL_GetRendererOutputSize(r, &w, &h);
	/* TODO: center-align text + adjust the gap + have different text color, size for each,
	 *       maybe make a struct to pass to draw_text with all of these properties */
	draw_text(r, font, p->song, x, h * 1/4);
	draw_text(r, font, p->artist, x, h * 3/4);
	SDL_RenderPresent(r);
}

int read_playing(struct playing *p, char *path, int buf_len, char **buf) {
	/* TODO: maybe use inotify(7)? not portable tho */
	FILE *f = fopen(path, "r");
	if (!f) {
		perror("error opening file");
		return 1;
	}
	fseek(f, 0L, SEEK_END);
	int f_len = ftell(f);
	if (f_len > buf_len || *buf == NULL)
		*buf = realloc(*buf, f_len);
	rewind(f);
	if (fread(*buf, sizeof(char), f_len, f) < (size_t)f_len) {
		perror("fread");
		return 0;
	}
	fclose(f);

	p->song = strtok(*buf, "\n");
	p->artist = strtok(NULL, "\n");
	p->cover_path = strtok(NULL, "\n");
	if (p->cover_path == NULL) {
		fprintf(stderr, "song file has missing lines\n");
		return 0;
	}

	return f_len;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: grem [IMAGE_PATH]\n");
		return 1;
	}

	struct playing p = {0};
	char *buf = NULL;
	int buf_len = read_playing(&p, argv[1], 0, &buf);
	if (buf_len == 0)
		return 1;

	if (!IMG_Init(IMG_INIT_JPG)) {
		fprintf(stderr, "error initializing SDL_image: %s\n", IMG_GetError());
		return 1;
	}
	SDL_Surface *image_surface = IMG_Load(p.cover_path);
	if (!image_surface) {
		fprintf(stderr, "error loading image: %s\n", IMG_GetError());
		return 1;
	}

	if (TTF_Init() < 0) {
		fprintf(stderr, "error initializing SDL_ttf: %s\n", TTF_GetError());
		return 1;
	}
	TTF_Font *font = TTF_OpenFont(FONT_PATH, 16);
	if (!font) {
		fprintf(stderr, "error opening font: %s\n", TTF_GetError());
		return 1;
	}

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
		SDL_Log("error initializing SDL: %s", SDL_GetError());
		return 1;
	}

	SDL_Window *w;
	SDL_Renderer *r;
	if (SDL_CreateWindowAndRenderer(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT, SDL_WINDOW_RESIZABLE, &w, &r)) {
		SDL_Log("error creating window: %s", SDL_GetError());
		return 1;
	}
	SDL_SetWindowMinimumSize(w, WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);

	SDL_Texture *image = SDL_CreateTextureFromSurface(r, image_surface);
	if (!image) {
		SDL_Log("error loading image as texture: %s", SDL_GetError());
		return 1;
	}

	SDL_SetRenderDrawColor(r, 34, 34, 34, 255);
	draw_everything(r, image, font, &p);

	SDL_Event e;
	while (1) {
		if (SDL_WaitEvent(&e)) {
			if (e.type == SDL_QUIT) {
				break;
			} else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
				draw_everything(r, image, font, &p);
			} else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_EXPOSED) {
				draw_everything(r, image, font, &p);
			}
		}
	}

	free(buf);
	SDL_DestroyRenderer(r);
	SDL_DestroyWindow(w);
	SDL_DestroyTexture(image);
	SDL_FreeSurface(image_surface);
	IMG_Quit();
	TTF_CloseFont(font);
	TTF_Quit();
	SDL_Quit();
	return 0;
}
