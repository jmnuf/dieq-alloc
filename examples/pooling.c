/**
 * I do apologize for this code is not as simple of an example as I would like,
 * but I wanted to do a fire-like effect with particles using pooling and I just copy
 * pasted some stuff from my personal SDL3 exploration code from like a month ago
 * as of the writing of this. So there's a lot of fluff just for making it look kinda
 * decent and satisfying for me. At least I didn't end up writing a circle function,
 * I thought about doing so for a bit longer than I should have...
 *
 * The main view of this is the Particle_System struct and the 2 wrapper functions
 * written for using the Dieq_Pool in a semi-nice fashion.
 * Look at the functions `request_particle` and `release_particle` for seeing the
 * usage of struct Dieq_Pool in this example.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_events.h>

#define DIEQ_IMPLEMENTATION
#include "dieq.h"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT (WINDOW_WIDTH/16*9)

#define da_foreach(Type, it, da) for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)
#define return_defer(value) do { result = (value); goto defer; } while(0)

void push_sdl_quit_event() {
  SDL_Event quit_event;
  SDL_zero(quit_event);
  quit_event.type = SDL_EVENT_QUIT;
  quit_event.quit.timestamp = SDL_GetTicksNS();
  SDL_PushEvent(&quit_event);
}

static inline float normalized(float v, float min, float max) {
  return (v - min)/(max - min);
}

float randf() {
  return rand()/(float)RAND_MAX;
}

float randf_range(float min, float max) {
  float norm = rand()/(float)RAND_MAX;
  return norm * (max - min) + min;
}

static inline float lerp(float a, float b, float t) {
  return a+(b-a)*t;
}

SDL_Color lerp_color(SDL_Color a, SDL_Color b, float t) {
  SDL_Color result = {0};

  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  result.r = a.r == b.r ? a.r : (Uint8)lerp((float)a.r, (float)b.r, t);
  result.g = a.g == b.g ? a.g : (Uint8)lerp((float)a.g, (float)b.g, t);
  result.b = a.b == b.b ? a.b : (Uint8)lerp((float)a.b, (float)b.b, t);
  result.a = a.a == b.a ? a.a : (Uint8)lerp((float)a.a, (float)b.a, t);

  return result;
}

void render_frame();
void update_frame(float dt);

typedef struct {
  SDL_FPoint pos;
  SDL_FPoint vel;
  SDL_Color color;
  float lifetime;
} Particle;

#define MAX_PARTICLES 100
#define BASE_LIFETIME 1.05f
#define INITIAL_COLOR ((SDL_Color){ .r = 255, .g = 205, .b = 3, .a = 255 })
#define MID_COLOR ((SDL_Color){ .r = 245, .g = 15, .b = 15, .a = 180 })
#define END_COLOR ((SDL_Color){ .r = 0xFE, .g = 0xFE, .b = 0xFE, .a = 50 })

void particle_update_color(Particle *p) {
  float first_break = BASE_LIFETIME*0.5;
  float second_break = BASE_LIFETIME*0.015;
  float third_break = BASE_LIFETIME*0.001;

  if (p->lifetime >= first_break) {
    float t = normalized(p->lifetime, first_break, BASE_LIFETIME);
    p->color = lerp_color(INITIAL_COLOR, MID_COLOR, t);
    return;
  }

  if (p->lifetime >= second_break) {
    float t = normalized(p->lifetime, third_break, second_break);
    p->color = lerp_color(MID_COLOR, END_COLOR, t);
    return;
  }

  float t = normalized(p->lifetime, 0, third_break);
  SDL_Color c = END_COLOR;
  p->color.a = (Uint8)lerp(c.a, 0.0f, t);
}

typedef struct {
  Dieq_Pool pool;
  struct {
    Particle *items[MAX_PARTICLES];
    size_t count;
  } alive;

  SDL_FPoint start_pos;
  SDL_FPoint start_vel;
} Particle_System;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

Particle_System ps = {
  .start_pos = { .x = WINDOW_WIDTH*0.5f, .y = WINDOW_HEIGHT*0.95f },
  .start_vel = { .y = -100 },
};

Particle *request_particle() {
  Particle *p = dieq_pool_request(&ps.pool);
  if (p == NULL) return NULL;
  *p = (Particle) {
    .pos = ps.start_pos,
    .vel = ps.start_vel,
    .color = INITIAL_COLOR,
    .lifetime = BASE_LIFETIME,
  };
  float offset = 10;
  p->pos.x += randf_range(-offset, offset);
  p->pos.y += randf_range(-offset, offset);

  p->vel.x += randf_range(-100, 100);
  p->vel.y -= randf_range(-10, 300);

  p->lifetime += randf_range(-0.1, 0.1);

  ps.alive.items[ps.alive.count++] = p;
  return p;
}

void release_particle(Particle *p) {
  if (p == NULL) return;
  dieq_pool_release(&ps.pool, p);
  if (ps.alive.count == 0) return;

  ssize_t index = -1;
  for (size_t i = 0; i < ps.alive.count; ++i) {
    if (ps.alive.items[i] == p) {
      index = (ssize_t)i;
      break;
    }
  }
  if (index == -1) return;

  ps.alive.items[index] = ps.alive.items[--ps.alive.count];
}


int main(void) {
  int result = 0;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "[ERROR] SDL could not initialize: %s\n", SDL_GetError());
    return_defer(1);
  }
  printf("[INFO] SDL video was initialized\n");

  if (!SDL_CreateWindowAndRenderer("Hello from SDL3!", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
    fprintf(stderr, "[ERROR] SDL failed to create window & renderer: %s\n", SDL_GetError());
    return_defer(1);
  }
  printf("[INFO] SDL window and renderer were created\n");

  {
    Dieq_Allocator allocator = { .alloc = malloc, .free = free, };
    if (!dieq_pool_init_with_allocator(&ps.pool, sizeof(Particle), MAX_PARTICLES, allocator)) {
      fprintf(stderr, "[ERROR] Failed to allocate pool\n");
      return_defer(1);
    }
    printf("[INFO] Pool allocate with a capacity for %zu items\n", ps.pool.cap);
  }

  bool quit = false;
  const Uint64 target_fps = 60;
  Uint64 time_start = SDL_GetTicks();
  Uint64 time_between_frames = (1000/target_fps) - 1; // 1ms of buffer space for error
  Uint64 frame_count = 0;
  Uint64 last_tick = 0;
  SDL_Event e;

  while (!quit) {
    SDL_zero(e);
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_EVENT_QUIT:
        quit = true;
        break;

      case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_ESCAPE) push_sdl_quit_event();
        break;

      }
    }

    // 60fps -> 16.66666667 ms
    // 30fps -> 33.33333333 ms
    Uint64 frame_time = SDL_GetTicks();
    Uint64 time_since_last_tick = frame_time-last_tick;
    frame_count ++;

    render_frame();
    SDL_RenderPresent(renderer);

    if (time_since_last_tick >= time_between_frames) {
      float dt = time_since_last_tick / 1000.0f;
      update_frame(dt);
      last_tick = SDL_GetTicks();
    }

    if (frame_count >= 400) {
      float avg_fps = frame_count / ((SDL_GetTicks() - time_start) / 1000.f);
      frame_count = 0;
      time_start = SDL_GetTicks();
      printf("FPS: %.4f\n", avg_fps);
    }
  }

defer:
  dieq_pool_deinit(&ps.pool);
  
  if (renderer) SDL_DestroyRenderer(renderer);

  if (window) SDL_DestroyWindow(window);

  SDL_Quit();

  printf("[INFO] De-initialized SDL\n");

  return result;
}

float map_value(float v, float pmin, float pmax, float nmin, float nmax) {
  float norm = (v - pmin) / (pmax - pmin);
  if (nmin == 0.0f) return norm * nmax;
  return (norm * (nmax - nmin)) + nmin;
}

void render_frame() {
  SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
  SDL_RenderClear(renderer);

  da_foreach(Particle*, it, &ps.alive) {
    Particle *p = *it;
    const float max_size = 50;
    const float break_point = BASE_LIFETIME*0.65;
    float size = p->lifetime >= break_point
               ? map_value(p->lifetime, break_point, BASE_LIFETIME, max_size, 0)
               : map_value(p->lifetime, 0, break_point, 0, max_size);
    SDL_FRect rect = {
      .x = p->pos.x - size*0.5f, .y = p->pos.y - size*0.5f,
      .w = size, .h = size,
    };
    SDL_SetRenderDrawColor(renderer, p->color.r, p->color.g, p->color.b, p->color.a);
    SDL_RenderFillRect(renderer, &rect);
  }
}

void update_frame(float dt) {
  for (ssize_t i = (ssize_t)ps.alive.count - 1; i >= 0; --i) {
    Particle *p = ps.alive.items[i];

    float vx = p->vel.x * dt;
    p->pos.x += vx;

    float vy = p->vel.y * dt;
    p->pos.y += vy;
    p->lifetime -= dt;

    particle_update_color(p);

    if (p->lifetime <= 0) release_particle(p);
  }

  request_particle();
}
