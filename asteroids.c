#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#include <raylib.h>

#define TARGET_FPS 60
#define W_WIDTH 1920
#define W_HEIGHT 1080

#define PLAYER_LEN 50
#define BASE_WIDTH 35

#define PLAYER_SPEED 1
#define PLAYER_TURN_SPEED 5.0f

#define MAX_SHAPES 100
#define MAX_BULLETS 500

#define FONT_SIZE 20

typedef struct {
    Vector2 accel;
    Vector2 pos;
    float angle;
    int bullets;
} player_t;

typedef struct {
    Vector2 *vertices;
    int n;
    Vector2 centroid;
    Vector2 velocity;

    enum {
        ASTEROID,
        BULLET,
    } tag;
} shape_t;

static Vector2 center_screen = (Vector2) {W_WIDTH / 2.0f, W_HEIGHT / 2.0f};
static player_t player;

static int best_slot = 0;
static int score = 0;
static bool lost_game = false;

static int shape_count;
static shape_t shapes[MAX_SHAPES];

static int bullet_count;
static shape_t bullets[MAX_BULLETS];

float deg_to_rad(float deg) {
    return deg * PI / 180.0f;
}

float rand_range(float min, float max) {
    return min + (rand() / (float)RAND_MAX) * (max - min);
}

bool point_in_polygon(Vector2 point, Vector2 *vertices, int n) {
    bool result = false;
    int j = n - 1;
    for (int i = 0; i < n; i++) {
        if ((vertices[i].y > point.y) != (vertices[j].y > point.y) &&
            (point.x < (vertices[j].x - vertices[i].x) * (point.y - vertices[i].y) /
            (vertices[j].y - vertices[i].y) + vertices[i].x)) {

            result = !result;
        }
        j = i;
    }
    return result;
}

Vector2 calc_centroid(Vector2 *vertices, int n) {
    Vector2 centroid = {0, 0};
    for (int i = 0; i < n; i++) {
        centroid.x += vertices[i].x;
        centroid.y += vertices[i].y;
    }
    centroid.x /= n;
    centroid.y /= n;
    return centroid;
}

shape_t create_shape(Vector2 pos, int n, int min_len, int max_len, int type) {
    shape_t shape;
    shape.n = n;
    shape.vertices = malloc(sizeof(Vector2) * n);

    float angle_increment = 2 * PI / n;

    for (int i = 0; i < n; i++) {
        float angle = i * angle_increment;
        float radius = rand_range(min_len, max_len);
        shape.vertices[i].x = pos.x + radius * cosf(angle);
        shape.vertices[i].y = pos.y + radius * sinf(angle);
    }
    shape.velocity = (Vector2) {0, 0};
    shape.centroid = calc_centroid(shape.vertices, shape.n);
    shape.tag = type;
    return shape;
}

void move_shape(shape_t *shape, Vector2 new_pos) {
    Vector2 delta = (Vector2) {new_pos.x - shape->centroid.x, new_pos.y - shape->centroid.y};
    for (int i = 0; i < shape->n; i++) {
        shape->vertices[i].x += delta.x;
        shape->vertices[i].y += delta.y;
    }
    shape->centroid = new_pos;
}

void draw_shape(shape_t shape) {
    for (int i = 0; i < shape.n; i++) {
        int next_index = (i + 1) % shape.n;
        DrawLine(shape.vertices[i].x, shape.vertices[i].y,
                 shape.vertices[next_index].x,
                 shape.vertices[next_index].y, RAYWHITE);
    }
}

void destroy_shape(shape_t *shape) {
    free(shape->vertices);
    shape->vertices = NULL;
    shape->n = 0;
    shape->centroid = (Vector2) {0, 0};
    shape->velocity = (Vector2) {0, 0};
}

void update_shape(shape_t *shape) {
    Vector2 new_pos = (Vector2) {
        shape->centroid.x + shape->velocity.x * (1.0f / TARGET_FPS),
        shape->centroid.y + shape->velocity.y * (1.0f / TARGET_FPS)
    };
    move_shape(shape, new_pos);
}

void spawn_asteroid() {
    if (shape_count < MAX_SHAPES) {
        Vector2 pos;

        int x, y;
        x = rand_range(50, W_WIDTH - 50);
        while (x > 150 && x < W_WIDTH - 150) {
            x = rand_range(50, W_WIDTH - 50);
        }

        y = rand_range(50, W_HEIGHT - 50);
        while (y > 150 && y < W_HEIGHT - 150) {
            y = rand_range(50, W_HEIGHT - 50);
        }

        pos.x = x;
        pos.y = y;

        shape_t new_shape = create_shape(
            pos, rand_range(3, 15), 50, 100, ASTEROID);

        int edge = rand() % 4;
        if (edge == 0) {
            move_shape(&new_shape, (Vector2){rand_range(0, 150),
                       rand_range(0, W_HEIGHT)});
        } else if (edge == 1) {
            move_shape(&new_shape, (Vector2){
                       rand_range(W_WIDTH - 150, W_WIDTH),
                       rand_range(0, W_HEIGHT)});
        } else if (edge == 2) {
            move_shape(&new_shape, (Vector2){rand_range(0, W_WIDTH),
                       rand_range(0, 150)});
        } else if (edge == 3) {
            move_shape(&new_shape, (Vector2){
                       rand_range(0, W_WIDTH),
                       rand_range(W_HEIGHT - 150, W_HEIGHT)});
        }

        Vector2 direction = {center_screen.x - new_shape.centroid.x,
            center_screen.y - new_shape.centroid.y};

        double dist = sqrt(direction.x * direction.x +
                           direction.y * direction.y) / 10 + score;

        new_shape.velocity = (Vector2){direction.x / dist,
            direction.y / dist};
        shapes[shape_count++] = new_shape;
    }
}

void spawn_bullet() {
    float rad = deg_to_rad(player.angle);
    Vector2 top_vertex = {
        player.pos.x + PLAYER_LEN * sinf(rad),
        player.pos.y - PLAYER_LEN * cosf(rad)
    };

    shape_t new_bullet = create_shape(
        top_vertex, 4, 3, 5, BULLET);

    new_bullet.velocity = (Vector2) {
        sinf(rad) * 500,
       -cosf(rad) * 500
    };

    bullets[bullet_count++] = new_bullet;
}

double calc_distance(Vector2 a, Vector2 b) {
    return sqrt(
        (b.x - a.x) * (b.x - a.x) +
        (b.y - a.y) * (b.y - a.y)
    );
}

Vector2 sub_vec(Vector2 a, Vector2 b) {
    return (Vector2) {a.x - b.x,
                      a.y - b.y};
}

Vector2 add_vec(Vector2 a, Vector2 b) {
    return (Vector2) {a.x + b.x,
                      a.y + b.y};
}

Vector2 rotate_point(Vector2 point, float angle) {
    float rad = deg_to_rad(angle);
    return (Vector2) {
        .x = point.x * cosf(rad) - point.y * sinf(rad),
        .y = point.x * sinf(rad) + point.y * cosf(rad),
    };
}

Vector2 get_centroid(Vector2 a, Vector2 b, Vector2 c) {
    return calc_centroid((Vector2 []){a, b, c}, 3);
}

void calculate_base_vert(Vector2 top, float height, float base_width,
                         Vector2 *bottom_left_vert, Vector2 *bottom_right_vert) {
    float half_base = base_width / 2;

    *bottom_left_vert = (Vector2) {top.x - half_base, top.y + height};
    *bottom_right_vert = (Vector2) {top.x + half_base, top.y + height};
}

void draw_triangle(Vector2 centroid, float angle, float height,
                   float base_width, Color color) {

    float half_base = base_width / 2;

    Vector2 top = (Vector2) {
        centroid.x + sinf(deg_to_rad(angle)) * height,
        centroid.y - cosf(deg_to_rad(angle)) * height
    };

    Vector2 bottom_left = (Vector2) {
        centroid.x + sinf(deg_to_rad(angle + 120)) * half_base,
        centroid.y - cosf(deg_to_rad(angle + 120)) * half_base
    };

    Vector2 bottom_right = (Vector2) {
        centroid.x + sinf(deg_to_rad(angle - 120)) * half_base,
        centroid.y - cosf(deg_to_rad(angle - 120)) * half_base
    };

    DrawTriangleLines(top, bottom_left, bottom_right, color);
}

void draw_player() {
    draw_triangle(player.pos, player.angle, PLAYER_LEN, BASE_WIDTH, RAYWHITE);
}

void init() {
    srand(time(0));

    player = (player_t) {
        .pos = (Vector2) {W_WIDTH / 2.0f, W_HEIGHT / 2.0f},
        .angle = 0,
        .bullets = MAX_BULLETS
    };

    lost_game = false;
}

void lose_game() {
    const char *str = TextFormat("Game Over, you scored %d", score);
    const int font_size = FONT_SIZE * 3;
    DrawText(str,
             (W_WIDTH - MeasureText(str, font_size)) / 2,
             (W_HEIGHT - font_size) / 2,
             font_size, WHITE);

    const char *retry_str = "Press R to retry";
    const int retry_font_size = FONT_SIZE * 2;
    DrawText(retry_str,
             (W_WIDTH - MeasureText(retry_str, retry_font_size)) / 2,
             (W_HEIGHT - font_size) / 2 + font_size + retry_font_size / 2,
             retry_font_size, WHITE);
}

void render() {
    BeginDrawing();
    ClearBackground(BLACK);

        if (lost_game) {
            lose_game();
        }

        draw_player();

        for (int i = 0; i < shape_count; i++)
            draw_shape(shapes[i]);

        for (int i = 0; i < bullet_count; i++)
            draw_shape(bullets[i]);

        const char *bullet_str = TextFormat("Bullets: %d", player.bullets);
        DrawText(bullet_str, 0, W_HEIGHT - FONT_SIZE, FONT_SIZE, WHITE);
        DrawText(TextFormat("Score: %d", score),
                 MeasureText(bullet_str, FONT_SIZE) + FONT_SIZE,
                 W_HEIGHT - FONT_SIZE, FONT_SIZE, WHITE);
    EndDrawing();
}

void input() {
    if (IsKeyDown(KEY_R)) {
        bullet_count = 0;
        shape_count = 0;
        score = 0;
        init();
    }

    if (lost_game)
        return;

    if (IsKeyDown(KEY_A))
        player.angle -= PLAYER_TURN_SPEED;
    if (IsKeyDown(KEY_D))
        player.angle += PLAYER_TURN_SPEED;

    if (IsKeyDown(KEY_W)) {
        float rad = deg_to_rad(player.angle);
        player.accel.x += sinf(rad) * PLAYER_SPEED;
        player.accel.y -= cosf(rad) * PLAYER_SPEED;
    }

    if (IsKeyDown(KEY_SPACE)) {
        if (player.bullets > 0 && bullet_count < MAX_BULLETS) {
            spawn_bullet();

            float rad = deg_to_rad(player.angle);
            player.accel.x -= 0.05f * sinf(rad);
            player.accel.y += 0.05f * cosf(rad);
            player.bullets--;
        }
    }
}

bool check_bound_single(Vector2 a) {
    return (a.x >= W_WIDTH || a.x <= 0 || a.y >= W_HEIGHT || a.y <= 0);
}

bool check_bullet_asteroid_collisions(int i) {
    for (int j = 0; j < shape_count; j++) {
        if (shapes[j].tag == ASTEROID) {
            for (int k = 0; k < bullets[i].n; k++) {
                if (point_in_polygon(bullets[i].vertices[k],
                                     shapes[j].vertices, shapes[j].n)) {
                    destroy_shape(&bullets[i]);
                    bullets[i] = bullets[--bullet_count];

                    destroy_shape(&shapes[j]);
                    shapes[j] = shapes[--shape_count];

                    return true;
                }
            }
        }
    }
    return false;
}

void check_player_asteroid_collisions(int i) {
    if (shapes[i].tag == ASTEROID) {
        Vector2 player_vertices[3];
        Vector2 blv, brv;
        calculate_base_vert(player.pos, PLAYER_LEN, BASE_WIDTH, &blv, &brv);

        player_vertices[0] = player.pos;
        player_vertices[1] = blv;
        player_vertices[2] = brv;

        for (int j = 0; j < 3; j++) {
            if (point_in_polygon(player_vertices[j], shapes[i].vertices, shapes[i].n)) {
                lost_game = true;
                return;
            }
        }
    }
}

static int frame_count;
void update() {
    input();

    if (lost_game)
        return;

    frame_count++;
    if (frame_count % TARGET_FPS / 2 == 0) {
        player.bullets++;
        if (player.bullets > MAX_BULLETS) {
            player.bullets = MAX_BULLETS;
        }
    }

    if (frame_count % TARGET_FPS == 0) {
        spawn_asteroid();
    }

    double dt_squared = (1.0 / TARGET_FPS * TARGET_FPS);

    Vector2 np = (Vector2) {
        player.pos.x + 0.5f * player.accel.x * dt_squared,
        player.pos.y + 0.5f * player.accel.y * dt_squared
    };

    Vector2 blv, brv;
    calculate_base_vert(np, PLAYER_LEN, BASE_WIDTH, &blv, &brv);

    if (check_bound_single(np)  ||
        check_bound_single(blv) ||
        check_bound_single(brv)) {

        player.accel = (Vector2) {
            player.accel.x * -0.5f,
            player.accel.y * -0.5f
        };
    } else {
        player.pos = np;
    }

    if (player.angle >= 360) player.angle -= 360;
    else if (player.angle < 0) player.angle += 360;

    if (fabs(player.accel.x) < 0.01f)
        player.accel.x = 0;

    if (fabs(player.accel.y) < 0.01f)
        player.accel.y = 0;

    for (int i = 0; i < shape_count; i++) {
        update_shape(&shapes[i]);
        if (check_bound_single(shapes[i].centroid)) {
            destroy_shape(&shapes[i]);
            shapes[i] = shapes[--shape_count];
        } else {
            check_player_asteroid_collisions(i);
        }
    }

    for (int i = 0; i < bullet_count; i++) {
        update_shape(&bullets[i]);
        if (check_bound_single(bullets[i].centroid)) {
            destroy_shape(&bullets[i]);
            bullets[i] = bullets[--bullet_count];
        } else {
            if (check_bullet_asteroid_collisions(i)) {
                score++;
                break;
            }
        }
    }
}

void cleanup() {
    CloseWindow();
}

void mainloop() {
    while (!WindowShouldClose()) {
        update();
        render();
    }
}

int main(int argc, char **argv) {
    InitWindow(W_WIDTH, W_HEIGHT, "Asteroids");
    SetTargetFPS(TARGET_FPS);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    init();
    mainloop();
    cleanup();
    return 0;
}

