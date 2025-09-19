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
// Performance Measurements
//

#define PERFORMANCE_SUB_MILLI_PERCISION 16.0
const unsigned char UPDATE_BALL_POSITIONS   =   0;
const unsigned char SORT_BALLS_INTO_CHUNKS  =   1;
const unsigned char DO_BALL_BALL_COLLISIONS =   2;
const unsigned char DO_BALL_WALL_COLLISIONS =   3;
const unsigned char ANON_PERFORMANCE_TAG    = 128;

struct __attribute__((packed)) performance_interval {
    unsigned char tag;
    unsigned char duration;
    signed short  frame_start_offset;
};

struct performance_interval performance_intervals[256];

unsigned int performance_interval_count =    0;
float        performance_interval_start = -1.0;
float        frame_start_time           = -1.0;

float __attribute__((import_name("performanceNow"))) performance_now(void);
float __attribute__((import_name("performanceRecordIntervalsInDevTools")))
performance_record_intervals_in_dev_tools(
    struct performance_interval*, unsigned int);

void performance_start_interval(void) {
    performance_interval_start = performance_now();
}

void performance_end_interval(unsigned char tag) {
    float interval_stop  = performance_now();
    float interval_start = performance_interval_start;
    performance_interval_start = interval_stop;

    performance_intervals[performance_interval_count].tag      = tag;
    performance_intervals[performance_interval_count].duration =
        (unsigned char) ((interval_stop - interval_start) * PERFORMANCE_SUB_MILLI_PERCISION);
    performance_intervals[performance_interval_count].frame_start_offset =
        (unsigned char) ((interval_start - frame_start_time) * PERFORMANCE_SUB_MILLI_PERCISION);

    ++performance_interval_count;
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

struct __attribute__((packed)) ball {
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

struct __attribute__((packed)) chunk {
    signed short  start_index;
    unsigned char balls_count;
    unsigned char unsorted_balls_count;
};

void __attribute__((import_name("drawBalls")))
draw_balls(int struct_size, struct ball*, int balls_count);

#define CHUNK_SIDE_LEN 16
const int   BALLS_COUNT         =  3000;
const float MIN_BALL_RADIUS     =   3.0;
const float MAX_BALL_RADIUS     =   6.0;
const float MIN_BALL_VELOCITY   =   0.0/1000.0; // /1000.0 because milliseconds
const float MAX_BALL_VELOCITY   = 200.0/1000.0;
const float SIM_TIME_STEP_IN_MS =   4.0;
float       sim_time            =  -1.0;
float canvas_width  = -1.0;
float canvas_height = -1.0;
int   chunks_width  = -1;
int   chunks_height = -1;
int   chunks_count  = -1;
unsigned char memory[60*1024];
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

void tick(void) {
    performance_start_interval();
    // Update ball positions
    for (int i = 0; i < BALLS_COUNT; ++i) {
        balls[i].x +=
            (union float_bits) {.bits = balls[i].velocity_x_bits << 8}.float_
            * SIM_TIME_STEP_IN_MS;
        balls[i].y +=
            (union float_bits) {.bits = balls[i].velocity_y_bits << 8}.float_
            * SIM_TIME_STEP_IN_MS;
    }
    performance_end_interval(UPDATE_BALL_POSITIONS);

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
    //assert(chunk_start_index == BALLS_COUNT, "Invalid chunk data!");
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
    performance_end_interval(SORT_BALLS_INTO_CHUNKS);

    // Bounce balls off eachother
    for (int chunk_row_start = 0;
             chunk_row_start < chunks_count;
             chunk_row_start += chunks_width) 
    {
        for (int chunk_row_offset = 0;
                 chunk_row_offset < chunks_width;
                 ++chunk_row_offset
        ) {
            int chunk_i = chunk_row_start + chunk_row_offset;
            int start_i = chunks[chunk_i].start_index;
            int  stop_i = start_i + chunks[chunk_i].balls_count;
            for (int same_row = 1; same_row != -1; --same_row) {
                int start_j, stop_j;
                if (same_row) {
                    start_j = start_i;
                    if (chunk_row_offset == chunks_width-1) {
                        stop_j = stop_i;
                    } else {
                        stop_j = chunks[chunk_i+1].start_index
                               + chunks[chunk_i+1].balls_count;
                    }
                } else if (chunk_row_start + chunks_width != chunks_count) {
                    start_j = chunks[chunk_i+chunks_width].start_index;
                    if (chunk_row_offset == chunks_width-1) {
                        stop_j = start_j + chunks[chunk_i+chunks_width].balls_count;
                    } else {
                        stop_j = chunks[chunk_i+chunks_width+1].start_index
                               + chunks[chunk_i+chunks_width+1].balls_count;
                    }
                } else { continue; }
                for (int i = start_i; i < stop_i; ++i) {
                    int x1 = balls[i].x;
                    int y1 = balls[i].y;
                    int r1 = balls[i].radius;
                    for (int j = start_j > i+1 ? start_j : i+1; j < stop_j; ++j) {
                        float x2 = balls[j].x;
                        float y2 = balls[j].y;
                        float r2 = balls[j].radius;
                        float collision_dist = r1 + r2;
                        float dist_x = x2 - x1;
                        float dist_y = y2 - y1;
                        float dist_mag_squared = dist_x*dist_x + dist_y*dist_y;
                        if (dist_mag_squared <= collision_dist*collision_dist) {
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
                            balls[j].x = x1 + nx*collision_dist;
                            balls[j].y = y1 + ny*collision_dist;
                        }
                    }
                }
            }
        }
    }
    performance_end_interval(DO_BALL_BALL_COLLISIONS);

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

    performance_end_interval(DO_BALL_WALL_COLLISIONS);
}


void __attribute__((export_name("update")))
update(float current_frame_time) {
    frame_start_time = current_frame_time;
    performance_interval_count = 0;

    if (sim_time < -1.0) {
        sim_time = current_frame_time;
    }

    int max_ticks = (int) (3.0 * 1000.0 / 60.0 / SIM_TIME_STEP_IN_MS);
    while (sim_time < current_frame_time && --max_ticks) {
        tick();
        sim_time += SIM_TIME_STEP_IN_MS;
    }
    // Drop ticks if the simulation is running too slow
    if (!max_ticks) {
        sim_time = current_frame_time;
    }

    performance_record_intervals_in_dev_tools(
        performance_intervals, performance_interval_count);
    draw_balls(sizeof(struct ball), balls, BALLS_COUNT);
}

