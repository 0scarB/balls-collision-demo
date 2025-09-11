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

#define VELOCITY_FIXED_POINT_UNIT 32

struct __attribute__((packed, aligned(4))) ball {
    float x;                  // bytes  1,2,3,4
    float y;                  // bytes  5,6,7,8
    unsigned char radius;     // byte   9
    unsigned char color_r;    // byte  10
    unsigned char color_g;    // byte  11
    unsigned char color_b;    // byte  12
    signed short  velocity_x; // bytes 13,14
    signed short  velocity_y; // bytes 15,16
};

#define CHUNK_SIDE_LEN 32

struct __attribute__((packed, aligned(4))) chunk {
    signed short start_index;
    unsigned char balls_count;
    unsigned char unsorted_balls_count;
};

void __attribute__((import_name("drawBalls")))
draw_balls(int struct_size, struct ball*, int balls_count);

const float PI = 3.14159;
const int BALLS_COUNT       = 2000;
const int MIN_BALL_RADIUS   =    3;
const int MAX_BALL_RADIUS   =    6;
const int MIN_BALL_VELOCITY =    0*VELOCITY_FIXED_POINT_UNIT;
const int MAX_BALL_VELOCITY =  100*VELOCITY_FIXED_POINT_UNIT;
int canvas_width  = -1;
int canvas_height = -1;
int chunks_width  = -1;
int chunks_height = -1;
int chunks_count  = -1;
unsigned char  __attribute__((aligned(8))) memory[60*1024];
struct  ball*  balls;
struct chunk* chunks;

void _start(int _canvas_width, int _canvas_height) {
    canvas_width  = _canvas_width;
    canvas_height = _canvas_height;

    chunks_width  = canvas_width  / CHUNK_SIDE_LEN;
    chunks_height = canvas_height / CHUNK_SIDE_LEN;
    chunks_width  += canvas_width  - chunks_width *CHUNK_SIDE_LEN > 0;
    chunks_height += canvas_height - chunks_height*CHUNK_SIDE_LEN > 0;
    chunks_count = chunks_width * chunks_height;
    int max_balls_count =
        (sizeof(memory) - chunks_count*sizeof(chunks_count))
        / sizeof(struct ball);

    assert(canvas_width != -1 && canvas_height != -1,
           "Canvas width and/or height not initialized!");
    assert(sizeof(struct ball) == 16, "Unexpected byte-size of ball struct!");
    assert(BALLS_COUNT < max_balls_count, "Too many balls!");
    int chunk_area    = CHUNK_SIDE_LEN*CHUNK_SIDE_LEN;
    int min_ball_area = PI * MIN_BALL_RADIUS*MIN_BALL_RADIUS;
    assert(chunk_area / min_ball_area < 256,
           "Chunk too large -> ball-count fields may overflow!");

    balls  = (struct  ball*)  memory;
    chunks = (struct chunk*) (memory + BALLS_COUNT*sizeof(struct ball));
    assert((unsigned char*) chunks + chunks_count*sizeof(struct chunk) < memory + sizeof(memory), "Not enough memory!");

    // Randomly generate balls
    for (int i = 0; i < BALLS_COUNT; ++i) {
        int radius =
            MIN_BALL_RADIUS + random()*(MAX_BALL_RADIUS - MIN_BALL_RADIUS);
        balls[i].radius = (unsigned char) radius;
        balls[i].x = radius + random()*(canvas_width  - 2*radius);
        balls[i].y = radius + random()*(canvas_height - 2*radius);
        int max_iter = 16;
        do {
            balls[i].color_r = (unsigned char) (random()*256);
            balls[i].color_g = (unsigned char) (random()*256);
            balls[i].color_b = (unsigned char) (random()*256);
        // Loop to prevent colors that are too dark
        } while (--max_iter // Give up after a maximum number of iterations
            && balls[i].color_r + balls[i].color_g + balls[i].color_b < 256);
        balls[i].velocity_x = (signed short) (MIN_BALL_VELOCITY + random()
                              * (MAX_BALL_VELOCITY - MIN_BALL_VELOCITY));
        balls[i].velocity_y = (signed short) (MIN_BALL_VELOCITY + random()
                              * (MAX_BALL_VELOCITY - MIN_BALL_VELOCITY));
        if (random() < 0.5) { balls[i].velocity_x = -balls[i].velocity_x; }
        if (random() < 0.5) { balls[i].velocity_y = -balls[i].velocity_y; }
    }
    draw_balls(sizeof(struct ball), balls, BALLS_COUNT);
}

float previous_frame_time_in_ms = -1.0;

void __attribute__((export_name("update")))
update(float current_frame_time_in_ms) {
    if (previous_frame_time_in_ms < 0) {
        previous_frame_time_in_ms = current_frame_time_in_ms;
    }
    float time_delta_in_ms =
        current_frame_time_in_ms - previous_frame_time_in_ms;

    // Update ball positions
    for (int i = 0; i < BALLS_COUNT; ++i) {
        balls[i].x +=
            (float) balls[i].velocity_x * time_delta_in_ms
            / VELOCITY_FIXED_POINT_UNIT;
        balls[i].y +=
            (float) balls[i].velocity_y * time_delta_in_ms
            / VELOCITY_FIXED_POINT_UNIT;
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
        int ball_index = chunks[i].start_index;
        while (chunks[i].unsorted_balls_count) {
            int chunk_x = (int) balls[ball_index].x / CHUNK_SIDE_LEN;
            int chunk_y = (int) balls[ball_index].y / CHUNK_SIDE_LEN;
            if (chunk_x <  0            ) { chunk_x = 0; }
            if (chunk_x >= chunks_width ) { chunk_x = chunks_width-1; }
            if (chunk_y <  0            ) { chunk_y = 0; }
            if (chunk_y >= chunks_height) { chunk_y = chunks_height-1; }
            int chunk_index = chunks_width*chunk_y + chunk_x;

            int swap_ball_to_index =
                chunks[chunk_index].start_index
                + --chunks[chunk_index].unsorted_balls_count;

            // Swap balls
            int j = ball_index, k = swap_ball_to_index;
            float temp_float = balls[j].x;
            balls[j].x = balls[k].x;
            balls[k].x = temp_float;
            temp_float = balls[j].y;
            balls[j].y = balls[k].y;
            balls[k].y = temp_float;
            unsigned char temp_byte = balls[j].radius;
            balls[j].radius = balls[k].radius;
            balls[k].radius = temp_byte;
            temp_byte = balls[j].color_r;
            balls[j].color_r = balls[k].color_r;
            balls[k].color_r = temp_byte;
            temp_byte = balls[j].color_g;
            balls[j].color_g = balls[k].color_g;
            balls[k].color_g = temp_byte;
            temp_byte = balls[j].color_b;
            balls[j].color_b = balls[k].color_b;
            balls[k].color_b = temp_byte;
            signed short temp_short = balls[j].velocity_x;
            balls[j].velocity_x = balls[k].velocity_x;
            balls[k].velocity_x = temp_short;
            temp_short = balls[j].velocity_y;
            balls[j].velocity_y = balls[k].velocity_y;
            balls[k].velocity_y = temp_short;
        }
    }
    //// Verify that the balls were sorted into chunks correctly
    //for (int i = 0; i < chunks_count; ++i) {
    //    for (int j = chunks[i].start_index;
    //             j < chunks[i].start_index + chunks[i].balls_count;
    //           ++j
    //    ) {
    //        int chunk_x = balls[j].fixed_point_x
    //                      / (CHUNK_SIDE_LEN * POS_FIXED_POINT_UNIT);
    //        int chunk_y = balls[j].fixed_point_y
    //                      / (CHUNK_SIDE_LEN * POS_FIXED_POINT_UNIT);
    //        int chunk_index = chunks_width*chunk_y + chunk_x;
    //        assert(chunk_index == i, "Balls not sorted into chunks correctly!");
    //    }
    //}

    // Bounce balls off eachother
    int chunk_x = 0;
    int chunk_y = 0;
    for (int chunk = 0; chunk < chunks_count; ++chunk) {
        int start1 = chunks[chunk].start_index;
        int  stop1 = start1 + chunks[chunk].balls_count;
        for (int i = start1; i < stop1; ++i) {
            float x1 = balls[i].x;
            float y1 = balls[i].y;
            float r1 = (float) balls[i].radius;
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
                    float r2 = (float) balls[j].radius;
                    float dx = x2 - x1;
                    float dy = y2 - y1;
                    float dist = r1 + r2;
                    if (dx*dx + dy*dy <= dist*dist) {
                        // Ensure balls don't intersect by adjusting the distance
                        // between them
                        float dist_adjust = dist / __builtin_sqrt(dx*dx + dy*dy);
                        dx *= dist_adjust;
                        dy *= dist_adjust;
                        // Collision normal
                        float nx = dx / dist;
                        float ny = dy / dist;
                        // Masses
                        float m2 = PI * r2*r2;
                        float m1 = PI * r1*r1;
                        // Magitude of relative impulse along normal
                        // --> See https://en.wikipedia.org/wiki/Collision_response
                        float jr =
                            -2*m1*m2*(
                                  (float)
                                    (balls[j].velocity_x - balls[i].velocity_x)*nx
                                + (float)
                                    (balls[j].velocity_y - balls[i].velocity_y)*ny
                            )/(m1 + m2);
                        // Account for integer trunction due to integer conversion
                        //                                              vvvvvv
                        balls[i].velocity_x -= (signed short) (jr*nx/m1 + 0.5);
                        balls[i].velocity_y -= (signed short) (jr*ny/m1 + 0.5);
                        balls[j].velocity_x += (signed short) (jr*nx/m2 + 0.5);
                        balls[j].velocity_y += (signed short) (jr*ny/m2 + 0.5);
                        balls[j].x = x1 + dx;
                        balls[j].y = y1 + dy;
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
    for (int i = 0; i < BALLS_COUNT; ++i) {
        float x = balls[i].x;
        float y = balls[i].y;
        float r = (float) balls[i].radius;
        if (x - r < 0.0) {
            balls[i].x += 2.0*(r - x);
            balls[i].velocity_x = -balls[i].velocity_x;
        }
        if (x + r > (float) canvas_width) {
            balls[i].x += 2.0*((float) canvas_width - r - x);
            balls[i].velocity_x = -balls[i].velocity_x;
        }
        if (y - r < 0.0) {
            balls[i].y += 2.0*(r - y);
            balls[i].velocity_y = -balls[i].velocity_y;
        }
        if (y + r > (float) canvas_height) {
            balls[i].y += 2.0*((float) canvas_height - r - y);
            balls[i].velocity_y = -balls[i].velocity_y;
        }
    }

    draw_balls(sizeof(struct ball), balls, BALLS_COUNT);

    previous_frame_time_in_ms = current_frame_time_in_ms;
}

