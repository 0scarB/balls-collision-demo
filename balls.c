float __attribute__((import_name("random"))) random(void);
void  __attribute__((import_name("handleString")))
handle_string(int action, char*);

//
// Errors and Assertions
//

int _throw_error(char* file_name, int line_no, char* error_message);
#define throw_error(error_message) \
    _throw_error(__FILE__, __LINE__, error_message)
#define assert(expression, error_message) (        \
    expression || _throw_error(__FILE__, __LINE__, \
        "ASSERTION FAILED: " #expression "! " error_message))


//
// Global String Builder
//

const int PRINT_STRING            = 0;
const int THROW_ERROR_FROM_STRING = 1;
const float DEFAULT_FLOAT_MIN_PRECISION = 0.0001;

#define STRING_BUILDER_BUFFER_MAX_LEN 1024
char    string_builder_buffer[STRING_BUILDER_BUFFER_MAX_LEN];
int     string_builder_buffer_len = 0;

void string_builder_char(char c) {
    if (string_builder_buffer_len >= STRING_BUILDER_BUFFER_MAX_LEN) {
        throw_error("String-builder's maximum length was exceeded!");
    }
    string_builder_buffer[string_builder_buffer_len] = c;
    ++string_builder_buffer_len;
}
void string_builder_null_terminator(void) {
    string_builder_char('\0');
    --string_builder_buffer_len;
}
void string_builder_reset(void) {
    string_builder_buffer_len = 0;
    string_builder_null_terminator();
}
void string_builder_text(char* text) {
    for (char c = *text; c; c = *(++text)) {
        string_builder_char(c);
    }
    string_builder_null_terminator();
}
void string_builder_int(int x) {
    if (x == 0) {
        string_builder_char('0');
    }
    if (x < 0) {
        string_builder_char('-');
        x = -x;
    }
    int reversed_start = string_builder_buffer_len;
    for (; x; x /= 10) {
        string_builder_char('0' + (char) (x % 10));
    }
    int reversed_stop = string_builder_buffer_len;
    while (reversed_start < reversed_stop) {
        --reversed_stop;
        char temp = string_builder_buffer[reversed_start];
        string_builder_buffer[reversed_start] =
            string_builder_buffer[reversed_stop];
        string_builder_buffer[reversed_stop] = temp;
        ++reversed_start;
    }
    string_builder_null_terminator();
}
void string_builder_float(float x, float min_precision) {
    if (min_precision < 0.0) {
        min_precision = DEFAULT_FLOAT_MIN_PRECISION;
    }
    if (x < 0.0) {
        string_builder_char('-');
        x = -x;
    }
    int        whole_part = (int) x;
    float fractional_part = x - (float) whole_part;
    string_builder_int(whole_part);
    string_builder_char('.');
    if (fractional_part <= min_precision) {
        string_builder_char('0');
    }
    while (fractional_part >= min_precision) {
        fractional_part *= 10.0;
        int digit = (int) fractional_part;
        string_builder_char('0' + (char) digit);
        fractional_part -= (float) digit;
    }
    string_builder_null_terminator();
}
void string_builder_print(void) {
    string_builder_null_terminator();
    handle_string(PRINT_STRING, string_builder_buffer);
}

//
// ---
//

int throw_error_max_recursion = 1;

int _throw_error(char* file_name, int line_no, char* error_message) {
    if (--throw_error_max_recursion < 0) {
        string_builder_reset();
        handle_string(THROW_ERROR_FROM_STRING, "BUG: Recursion in throw_error!");
        return -1;
    }
    string_builder_reset();
    string_builder_text(file_name);
    string_builder_char(':');
    string_builder_int(line_no);
    string_builder_char(':');
    string_builder_char(' ');
    string_builder_text(error_message);
    string_builder_null_terminator();
    handle_string(THROW_ERROR_FROM_STRING, string_builder_buffer);
    return 0;
}


//
// Balls
//

#define CHUNK_SIDE_LEN 32

struct __attribute__((packed, aligned(4))) ball {
    float         x;
    float         y;
    unsigned char radius;
    // Most significant 24 bits of the float32 representation
    unsigned int  velocity_x_bits:24;
    unsigned char color_rgb_3x3x2;
    // Most significant 24 bits of the float32 representation
    unsigned int  velocity_y_bits:24;
};

// The following union is used to get and set the float32 values
// of the x and y velocity components, as follows:
//
//    float velocity_x =
//       (union float_bits) {.bits = ball[i].velocity_x_bits << 8}.float_;
//    ball[i].velocity_x_bits =
//       (union float_bits) {velocity_x}.bits >> 8;
//
union float_bits { float float_; unsigned int bits; };

struct __attribute__((packed, aligned(4))) chunk {
    signed short  start_index;
    unsigned char balls_count;
    unsigned char unsorted_balls_count;
};

void __attribute__((import_name("drawBalls")))
draw_balls(int struct_size, struct ball*, int balls_count);

const int   BALLS_COUNT       =  3500;
const float MIN_BALL_RADIUS   =   3.0;
const float MAX_BALL_RADIUS   =   6.0;
const float MIN_BALL_VELOCITY =   0.0/1000.0; // /1000.0 because milliseconds
const float MAX_BALL_VELOCITY = 100.0/1000.0;
float canvas_width  = -1.0;
float canvas_height = -1.0;
int   chunks_width  = -1;
int   chunks_height = -1;
int   chunks_count  = -1;
unsigned char  __attribute__((aligned(8))) memory[60*1024];
struct  ball*  balls;
struct chunk* chunks;

void _start(int _canvas_width, int _canvas_height) {
    canvas_width  = (float) _canvas_width ;
    canvas_height = (float) _canvas_height;

    chunks_width  = _canvas_width  / CHUNK_SIDE_LEN;
    chunks_height = _canvas_height / CHUNK_SIDE_LEN;
    chunks_count  = chunks_width * chunks_height;
    int max_balls_count =
        (sizeof(memory) - chunks_count*sizeof(chunks_count))
        / sizeof(struct ball);

    assert(canvas_width != -1 && canvas_height != -1,
           "Canvas width and/or height not initialized!");
    assert(sizeof(struct ball) == 16, "Unexpected byte-size of ball struct!");
    assert(BALLS_COUNT < max_balls_count, "Too many balls!");
    int chunk_area    = CHUNK_SIDE_LEN*CHUNK_SIDE_LEN;
    int min_ball_area = 3.14159 * MIN_BALL_RADIUS*MIN_BALL_RADIUS;
    assert(chunk_area / min_ball_area < 256,
           "Chunk too large -> ball-count fields may overflow!");

    balls  = (struct  ball*)  memory;
    chunks = (struct chunk*) (memory + BALLS_COUNT*sizeof(struct ball) + 1024);
    assert((unsigned char*) chunks + chunks_count*sizeof(struct chunk) < memory + sizeof(memory), "Not enough memory!");

    // Randomly generate balls
    for (int i = 0; i < BALLS_COUNT; ++i) {
        float radius =
            MIN_BALL_RADIUS + random()*(MAX_BALL_RADIUS - MIN_BALL_RADIUS);
        balls[i].x = radius + random()*(canvas_width  - 2.0*radius);
        balls[i].y = radius + random()*(canvas_height - 2.0*radius);
        balls[i].radius = (unsigned char) radius;
        int max_iter = 16;
        unsigned char r, g, b;
        do {
            r = (unsigned char) (random()*8.0);
            g = (unsigned char) (random()*8.0);
            b = (unsigned char) (random()*4.0);
            balls[i].color_rgb_3x3x2 = (r<<5)|(g<<2)|b;
        // Loop to prevent colors that are too dark
        } while (--max_iter // Give up after a maximum number of iterations
            && r+g+b < 256);
        balls[i].velocity_x_bits = (union float_bits) {
            (random() < 0.5 ? -1.0 : 1.0)
            * (MIN_BALL_VELOCITY
               + random()*(MAX_BALL_VELOCITY - MIN_BALL_VELOCITY))
        }.bits >> 8;
        balls[i].velocity_y_bits = (union float_bits) {
            (random() < 0.5 ? -1.0 : 1.0)
            * (MIN_BALL_VELOCITY
               + random()*(MAX_BALL_VELOCITY - MIN_BALL_VELOCITY))
        }.bits >> 8;
    }
    draw_balls(sizeof(struct ball), balls, BALLS_COUNT);
}

float previous_frame_time_in_ms = -1.0;

void __attribute__((export_name("update")))
update(float current_frame_time_in_ms) {
    if (previous_frame_time_in_ms < 0.0) {
        previous_frame_time_in_ms = current_frame_time_in_ms;
    }
    float time_delta_in_ms =
        current_frame_time_in_ms - previous_frame_time_in_ms;

    // Update ball positions
    for (int i = 0; i < BALLS_COUNT; ++i) {
        balls[i].x +=
            (union float_bits) {.bits = balls[i].velocity_x_bits << 8}.float_
            * time_delta_in_ms;
        balls[i].y +=
            (union float_bits) {.bits = balls[i].velocity_y_bits << 8}.float_
            * time_delta_in_ms;
    }

    // Zero array of chunk structs
    for (int i = 0; i < chunks_count; ++i) {
        *((unsigned int*) (chunks + i)) = 0;
    }
    // Count the number of balls that will be sorted into each chunk
    for (int i = 0; i < BALLS_COUNT; ++i) {
        int chunk_x = (int) balls[i].x / CHUNK_SIDE_LEN;
        int chunk_y = (int) balls[i].y / CHUNK_SIDE_LEN;
        if (chunk_x <  0            ) { chunk_x = 0; }
        if (chunk_x >= chunks_width ) { chunk_x = chunks_width-1; }
        if (chunk_y <  0            ) { chunk_y = 0; }
        if (chunk_y >= chunks_height) { chunk_y = chunks_height-1; }
        int chunk_index = chunks_width*chunk_y + chunk_x;
        ++chunks[chunk_index].balls_count;
    }
    // Calculate the start indices of each chunk in the sorted array
    int chunk_start_index = 0;
    for (int i = 0; i < chunks_count; ++i) {
        unsigned char balls_count = chunks[i].balls_count;
        chunks[i].start_index          = chunk_start_index;
        chunks[i].unsorted_balls_count = balls_count;
        chunk_start_index += (int) balls_count;
    }
    assert(chunk_start_index == BALLS_COUNT, "Invalid chunk data!");
    // Sort balls into chunks
    for (int i = 0; i < chunks_count; ++i) {
        struct ball* ball_ptr = balls + chunks[i].start_index;
        int max_iter = 1000;
        while (chunks[i].unsorted_balls_count && --max_iter) {
            int chunk_x = (int) (ball_ptr->x / CHUNK_SIDE_LEN);
            int chunk_y = (int) (ball_ptr->y / CHUNK_SIDE_LEN);
            if (chunk_x <  0            ) { chunk_x = 0; }
            if (chunk_x >= chunks_width ) { chunk_x = chunks_width-1; }
            if (chunk_y <  0            ) { chunk_y = 0; }
            if (chunk_y >= chunks_height) { chunk_y = chunks_height-1; }
            int chunk_index = chunks_width*chunk_y + chunk_x;

            // Swap balls
            unsigned int* ptr1 = (unsigned int*)  ball_ptr;
            unsigned int* ptr2 = (unsigned int*) (balls + (
                  chunks[chunk_index].start_index
                + chunks[chunk_index].unsorted_balls_count-1));
            for (int j = 0; j < sizeof(struct ball)/sizeof(unsigned int); ++j) {
                unsigned int temp = ptr1[j];
                ptr1[j] = ptr2[j];
                ptr2[j] = temp;
            }

            --chunks[chunk_index].unsorted_balls_count;
        }
    }

    // Bounce balls off eachother
    int chunk_x = 0;
    int chunk_y = 0;
    for (int chunk = 0; chunk < chunks_count; ++chunk) {
        int start1 = chunks[chunk].start_index;
        int  stop1 = start1 + chunks[chunk].balls_count;
        for (int i = start1; i < stop1; ++i) {
            int x1 = balls[i].x;
            int y1 = balls[i].y;
            int r1 = balls[i].radius;
            // Save color data because it will be overwritten and need to be
            // restored
            unsigned char balls_i_color = balls[i].color_rgb_3x3x2;
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                int start2, stop2;
                switch (quadrant) {
                    case 0:
                        start2 = start1;
                        stop2  = i;
                        break;
                    case 1:
                        if (chunk_x >= chunks_width) { continue; }
                        start2 = chunks[chunk+1].start_index;
                        stop2  = start2
                               + chunks[chunk+1].balls_count;
                        break;
                    case 2:
                        if (chunk_y >= chunks_height) { continue; }
                        start2 = chunks[chunk+chunks_width].start_index;
                        stop2  = start2
                               + chunks[chunk+chunks_width].balls_count;
                        break;
                    case 3:
                        if (chunk_x >= chunks_width ||
                            chunk_y >= chunks_height) { continue; }
                        start2 = chunks[chunk+chunks_width+1].start_index;
                        stop2  = start2
                               + chunks[chunk+chunks_width+1].balls_count;
                        break;
                }
                for (int j = start2; j < stop2; ++j) {
                    float x2 = balls[j].x;
                    float y2 = balls[j].y;
                    float r2 = balls[j].radius;
                    float dist_x = x2 - x1;
                    float dist_y = y2 - y1;
                    float dist_mag_squared = dist_x*dist_x + dist_y*dist_y;
                    if (dist_mag_squared <= (r1+r2)*(r1+r2)) {
                        float dist_mag = __builtin_sqrt(dist_mag_squared);
                        float nx = dist_x/dist_mag;
                        float ny = dist_y/dist_mag;

                        float m1 = r1*r1;
                        float m2 = r2*r2;

                        // Update velocities to account for the collision
                        // --> See https://en.wikipedia.org/wiki/Collision_response
                        float vix = (union float_bits) {.bits = balls[i].velocity_x_bits << 8}.float_;
                        float viy = (union float_bits) {.bits = balls[i].velocity_y_bits << 8}.float_;
                        float vjx = (union float_bits) {.bits = balls[j].velocity_x_bits << 8}.float_;
                        float vjy = (union float_bits) {.bits = balls[j].velocity_y_bits << 8}.float_;
                        float factor = -2.0*((vjx - vix)*nx + (vjy - viy)*ny)/(m1 + m2);
                        float factor_m1 = factor*m1;
                        float factor_m2 = factor*m2;
                        vix -= nx*factor_m2;
                        viy -= ny*factor_m2;
                        vjx += nx*factor_m1;
                        vjy += ny*factor_m1;
                        balls[i].velocity_x_bits = (union float_bits) {vix}.bits >> 8;
                        balls[i].velocity_y_bits = (union float_bits) {viy}.bits >> 8;
                        balls[j].velocity_x_bits = (union float_bits) {vjx}.bits >> 8;
                        balls[j].velocity_y_bits = (union float_bits) {vjy}.bits >> 8;

                        // Ensure balls don't intersect by adjusting the distance
                        // between them
                        balls[j].x = x1 + nx*(r1+r2);
                        balls[j].y = y1 + ny*(r1+r2);
                    }
                }
            }
        }

        if (++chunk_x > chunks_width) {
            chunk_x = 0;
            ++chunk_y;
        }
    }

    // Bounce balls off canvas edges
    int chunks_start = (chunks_height-1)*chunks_width;
    for (int i = chunks_start; i < (chunks_start + chunks_width); ++i) {
        int chunk_start = chunks[i].start_index;
        int chunk_stop  = chunk_start + chunks[i].balls_count;
        for (int j = chunk_start; j < chunk_stop; ++j) {
            float y = balls[j].y;
            float r = (float) balls[j].radius;
            if (y + r > canvas_height) {
                balls[j].y += 2.0*(canvas_height - r - y);
                // Reverse the y-velocity by flipping the float32 sign bit
                balls[j].velocity_y_bits ^= (1<<23);
            }
        }
    }
    for (int i = 0; i < chunks_width; ++i) {
        int chunk_start = chunks[i].start_index;
        int chunk_stop  = chunk_start + chunks[i].balls_count;
        for (int j = chunk_start; j < chunk_stop; ++j) {
            float y = balls[j].y;
            float r = (float) balls[j].radius;
            if (y - r < 0.0) {
                balls[j].y += 2.0*(r - y);
                // Reverse the y-velocity by flipping the float32 sign bit
                balls[j].velocity_y_bits ^= (1<<23);
            }
        }
    }
    for (int i = 0; i < chunks_height; ++i) {
        int chunk_index = i*chunks_width;
        int chunk_start = chunks[chunk_index].start_index;
        int chunk_stop  = chunk_start + chunks[chunk_index].balls_count;
        for (int j = chunk_start; j < chunk_stop; ++j) {
            float x = balls[j].x;
            float r = (float) balls[j].radius;
            if (x - r < 0.0) {
                balls[j].x += 2.0*(r - x);
                // Reverse the x-velocity by flipping the float32 sign bit
                balls[j].velocity_x_bits ^= (1<<23);
            }
        }
        chunk_index += chunks_width-1;
        chunk_start = chunks[chunk_index].start_index;
        chunk_stop  = chunk_start + chunks[chunk_index].balls_count;
        for (int j = chunk_start; j < chunk_stop; ++j) {
            float x = balls[j].x;
            float r = (float) balls[j].radius;
            if (x + r > canvas_width) {
                balls[j].x += 2.0*(canvas_width - r - x);
                // Reverse the x-velocity by flipping the float32 sign bit
                balls[j].velocity_x_bits ^= (1<<23);
            }
        }
    }

    previous_frame_time_in_ms = current_frame_time_in_ms;

    draw_balls(sizeof(struct ball), balls, BALLS_COUNT);
}

