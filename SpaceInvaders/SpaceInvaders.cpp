// SpaceInvaders.cpp 
// Recreating the classic game in C++ and openGL

#include <iostream>
#include <cstdio>
#include <GL/glew.h>
//#include <gl/GL.h>
#include <GLFW/glfw3.h>

//***Globals***

#define MAX_PROJECTILES 128

const size_t buffer_width = 224;
const size_t buffer_height = 256;

bool game_running = false;
int move_dir = 0;
bool is_shooting = 0;
const int no_alien_types = 3;



uint32_t rng = 30;

//debug
bool alien_is_shooting = 0;

///******

enum AlienType : uint8_t
{
    ALIEN_DEAD = 0,
    ALIEN_TYPE_A = 1,
    ALIEN_TYPE_B = 2,
    ALIEN_TYPE_C = 3
};


#define GL_ERROR_CASE(glerror)\
    case glerror: snprintf(error, sizeof(error), "%s", #glerror)


namespace shaders {
    const char* vertex_shader = 
        "\n"
        "#version 330\n"
        "\n"
        "noperspective out vec2 TexCoord;\n"
        "\n"
        "void main(void){\n"
        "\n"
        "    TexCoord.x = (gl_VertexID == 2)? 2.0: 0.0;\n"
        "    TexCoord.y = (gl_VertexID == 1)? 2.0: 0.0;\n"
        "    \n"
        "    gl_Position = vec4(2.0 * TexCoord - 1.0, 0.0, 1.0);\n"
        "}\n";

    const char* fragment_shader =
        "\n"
        "#version 330\n"
        "\n"
        "uniform sampler2D buffer;\n"
        "noperspective in vec2 TexCoord;\n"
        "\n"
        "out vec3 outColor;\n"
        "\n"
        "void main(void){\n"
        "    outColor = texture(buffer, TexCoord).rgb;\n"
        "}\n";
}



inline void gl_debug(const char* file, int line) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        char error[128];

        switch (err) {
            GL_ERROR_CASE(GL_INVALID_ENUM); break;
            GL_ERROR_CASE(GL_INVALID_VALUE); break;
            GL_ERROR_CASE(GL_INVALID_OPERATION); break;
            GL_ERROR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION); break;
            GL_ERROR_CASE(GL_OUT_OF_MEMORY); break;
        default: snprintf(error, sizeof(error), "%s", "UNKNOWN_ERROR"); break;
        }

        fprintf(stderr, "%s - %s: %d\n", error, file, line);
    }
}

#undef GL_ERROR_CASE


// Used to intercept OpenGL shader information during compilation
void validateShader(GLuint shader, const char* file = 0)
{
    static const GLsizei BUFFER_SIZE = 512;
    GLchar buffer[BUFFER_SIZE];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, BUFFER_SIZE, &length, buffer);

    if (length > 0)
    {
        printf("Shader %d(%s) compile error: %s\n", shader, (file ? file : ""), buffer);
    }
}

// Used to intercept OpenGL program information during compilation
bool validateProgram(GLuint program)
{
    static const GLsizei BUFFER_SIZE = 512;
    GLchar buffer[BUFFER_SIZE];
    GLsizei length = 0;
    glGetShaderInfoLog(program, BUFFER_SIZE, &length, buffer);

    if (length > 0)
    {
        printf("Program %d link error: %s\n", program, buffer);
        return false;
    }
    return true;
}


void errorCallback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}


//Callback for catching input events
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    switch (key)
    {
    case GLFW_KEY_ESCAPE:
        if (action == GLFW_PRESS) {
            game_running = false;
            printf("Game exiting...");
        }
        break;
    case GLFW_KEY_RIGHT:
        if (action == GLFW_PRESS) move_dir += 1;
        else if (action == GLFW_RELEASE) move_dir -= 1;
        break;
    case GLFW_KEY_LEFT:
        if (action == GLFW_PRESS) move_dir -= 1;
        else if (action == GLFW_RELEASE) move_dir += 1;
        break;
    case GLFW_KEY_SPACE:
        if (action == GLFW_PRESS) is_shooting = 1;
        break;
    case GLFW_KEY_P:
        if (action == GLFW_PRESS) alien_is_shooting = 1;
        break;
    default:
        break;
    }
}


struct Buffer
{
    size_t width, height;
    uint32_t* pixels; //32bit makes indexing easier
};


//Sets left-most 24-bits to the r,g,b values. 8 right-most bits are set to 255, alpha channel won't be used
uint32_t rgbTOuint32(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 24) | (g << 16) | (b << 8) | 255;
}


//Iterates over all pixels and sets each of them to a color
void bufferClear(Buffer* buffer, uint32_t colour)
{
    for (size_t i = 0; i < buffer->width * buffer->height; i++)
    {
        buffer->pixels[i] = colour;
    }
}


//xorshift random number generator
//https://en.wikipedia.org/wiki/Xorshift
struct xorshift32_state {
    uint32_t a;
};

/* The state word must be initialized to non-zero */
uint32_t xorshift32(struct xorshift32_state* state)
{
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    uint32_t x = state->a;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return state->a = x;
}

double random(xorshift32_state* state)
{
    return (double)xorshift32(state) / std::numeric_limits<uint32_t>::max();
}


struct Sprite
{
    
    size_t width, height;
    uint8_t* pixels;
};

struct SpriteAnimation
{
    bool loop;
    size_t num_frames;
    size_t frame_duration;
    size_t time;
    Sprite** frames;
};

struct Alien
{
    size_t x, y; 
    uint8_t type; //3 differnet types of aliens
};

struct Player
{
    size_t x, y;
    uint8_t life;
};

struct Projectile
{
    size_t x, y;
    int dir; //up(+) down(-)
};

struct Game
{
    size_t width, height;
    size_t num_aliens;
    size_t num_projectiles;
    size_t score;
    size_t aliens_killed;
    size_t alien_update_timer;
    size_t alien_update_frequency;
    Alien* aliens;
    Player player;
    Projectile projectiles[MAX_PROJECTILES];
};

//check if two sprites are overlapping
//we only check sprite rectangles, 
//we should been checking if any pixel of sprite A overlap with any pixel of sprite B
bool isSpriteOverlap(
    const Sprite& sprite_a, size_t x_a, size_t y_a,
    const Sprite& sprite_b, size_t x_b, size_t y_b
)
{
    if (x_a < x_b + sprite_b.width && x_a + sprite_a.width > x_b &&
        y_a < y_b + sprite_b.height && y_a + sprite_a.height > y_b)
    {
        return true;
    }

    return false;

}

//Function draws the "1" pixels at given coordinates if they are within buffer bounds
void drawSprite(Buffer* buffer, const Sprite& sprite, size_t x, size_t y, uint32_t colour)
{
    for (size_t xi = 0; xi < sprite.width; ++xi)
    {
        for (size_t yi = 0; yi < sprite.height; ++yi)
        {
            if (sprite.pixels[yi * sprite.width + xi] &&
                (sprite.height - 1 + y - yi) < buffer->height &&
                (x + xi) < buffer->width)
            {
                buffer->pixels[(sprite.height - 1 + y - yi) * buffer->width + (x + xi)] = colour;
            }
        }
    }
}

//Function draws a text
//Found in tutorial by Nick Tasios
//We define a new spritesheet containing 65 5x7 ASCII character sprites starting from 'space', 
//which has the value of 32 in ASCII, up to character '`', which has ASCII value 96. 
//Note that we only include uppercase letters and a few special characters.

//Function takes a piece of text and draws it in the buffer at the specified coordinates 
//and with the specified color, like we do with sprites. 
//The function simply iterates through all of the characters in the text until the null character, i.e. the string termination character, is encountered, 
//and draws each character using drawSprite(). 
//The tricky part is getting the correct character from the spritesheet and correctly drawing characters, correctly spaced, from left to right. 
//We achieve the first by indexing the character into the spritesheet by, (c - 32) * stride, where stride is the size of one character sprite, i.e. 7×5 = 35. 
//The character drawing position, xp, is moved by the sprite width incremented by 1.
void drawText(Buffer* buffer, const Sprite& text_spritesheet, const char* text, size_t x, size_t y, uint32_t colour)
{
    size_t xp = x;
    size_t stride = text_spritesheet.width * text_spritesheet.height;
    Sprite sprite = text_spritesheet;
    for (const char* charptr = text; *charptr != '\0'; ++charptr)
    {
        char character = *charptr - 32;
        if (character < 0 || character >= 65) continue;

        sprite.pixels = text_spritesheet.pixels + character * stride;
        drawSprite(buffer, sprite, xp, y, colour);
        xp += sprite.width + 1;
    }
}

void drawNumber(Buffer* buffer, const Sprite& number_spritesheet, size_t number, size_t x, size_t y, uint32_t colour)
{
    uint8_t digits[64];
    size_t num_digits = 0;

    size_t current_number = number;
    do
    {
        digits[num_digits++] = current_number % 10;
        current_number = current_number / 10;
    } while (current_number > 0);

    size_t xp = x;
    size_t stride = number_spritesheet.width * number_spritesheet.height;
    Sprite sprite = number_spritesheet;
    for (size_t i = 0; i < num_digits; ++i)
    {
        uint8_t digit = digits[num_digits - i - 1];
        sprite.pixels = number_spritesheet.pixels + digit * stride;
        drawSprite(buffer, sprite, xp, y, colour);
        xp += sprite.width + 1;
    }


}

int main()
{
    glfwSetErrorCallback(errorCallback);

    
    if (!glfwInit()){ return -1; }


    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);


    //create a window and OpenGl context
    GLFWwindow*  window = glfwCreateWindow(2 * buffer_width, 2 * buffer_height, "Space Invaders", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, keyCallback);

    //initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        fprintf(stderr, "Error initializing GLEW.\n");
        glfwTerminate();
        return -1;
    }

    int glVersion[2] = { -1, 1 };
    glGetIntegerv(GL_MAJOR_VERSION, &glVersion[0]);
    glGetIntegerv(GL_MINOR_VERSION, &glVersion[1]);

    gl_debug(__FILE__, __LINE__);

    printf("Using OpenGL: %d.%d\n", glVersion[0], glVersion[1]);
    printf("Renderer used: %s\n", glGetString(GL_RENDERER));
    printf("Shading Language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    //Vsync on
    glfwSwapInterval(1);

    glClearColor(1.0, 0.0, 0.0, 1.0);

    //create graphics buffer
    uint32_t clear_colour = rgbTOuint32(0, 128, 0); //green
    Buffer buffer;
    buffer.width = buffer_width;
    buffer.height = buffer_height;
    buffer.pixels = new uint32_t[buffer.width * buffer.height];
    bufferClear(&buffer, 0);

  


    //Generate texture
    GLuint buffer_texture;
    glGenTextures(1, &buffer_texture);
    glBindTexture(GL_TEXTURE_2D, buffer_texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB8,
        buffer.width,
        buffer.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_INT_8_8_8_8,
        buffer.pixels
    );
    //Each pixel is in the rgba format and is represented as 4 unsigned 8-bit integers
    //tells GPU not to apply any filtering(smoothing) when reading pixels
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //if GPU tries read beyond texture bounds use value of edges instead
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    // Generate fullscreen triangle
    GLuint fullscreen_triangle_vao;
    glGenVertexArrays(1, &fullscreen_triangle_vao);
    glBindVertexArray(fullscreen_triangle_vao);

    GLuint shader_id = glCreateProgram();
    //Create vertex shader
    {
        GLuint shader_vp = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(shader_vp, 1, &shaders::vertex_shader, 0);
        glCompileShader(shader_vp);
        validateShader(shader_vp, shaders::vertex_shader);
        glAttachShader(shader_id, shader_vp);

        glDeleteShader(shader_vp);
    }

    //Create fragment shader
    {
        GLuint shader_fp = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(shader_fp, 1, &shaders::fragment_shader, 0);
        glCompileShader(shader_fp);
        validateShader(shader_fp, shaders::fragment_shader);
        glAttachShader(shader_id, shader_fp);

        glDeleteShader(shader_fp);
    }

    glLinkProgram(shader_id);

    //Intercept errors if any occur
    if (!validateProgram(shader_id))
    {
        fprintf(stderr, "Error while validating shader.\n");
        glfwTerminate();
        glDeleteVertexArrays(1, &fullscreen_triangle_vao);
        delete[] buffer.pixels;
        return -1;
    }

    glUseProgram(shader_id);


   
    
    GLint location = glGetUniformLocation(shader_id, "buffer");
    glUniform1i(location, 0);

    //before game loop disable depth testing and bind the vertex array 
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(fullscreen_triangle_vao);

    Sprite text_spritesheet;
    text_spritesheet.width = 5;
    text_spritesheet.height = 7;
    text_spritesheet.pixels = new uint8_t[65 * 35]
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
        0,1,0,1,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,1,0,1,0,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,0,1,0,1,0,
        0,0,1,0,0,0,1,1,1,0,1,0,1,0,0,0,1,1,1,0,0,0,1,0,1,0,1,1,1,0,0,0,1,0,0,
        1,1,0,1,0,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,0,1,0,1,1,
        0,1,1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,1,0,0,1,0,0,1,0,1,0,0,0,1,0,1,1,1,1,
        0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
        1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,0,1,0,0,1,0,1,0,1,0,1,1,1,0,0,0,1,0,0,0,1,1,1,0,1,0,1,0,1,0,0,1,0,0,
        0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
        0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,

        0,1,1,1,0,1,0,0,0,1,1,0,0,1,1,1,0,1,0,1,1,1,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,0,0,1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,0,1,1,1,1,1,0,0,0,1,0,0,0,0,1,0,
        1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,

        0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
        0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
        0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
        1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,1,0,1,1,1,0,1,0,0,1,0,0,0,1,0,1,1,1,0,

        0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,1,1,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
        0,1,1,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
        0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,1,0,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
        1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,0,0,0,1,1,1,0,1,1,1,0,1,0,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,1,0,0,0,1,1,0,0,0,1,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,0,1,1,0,1,1,1,1,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,1,1,1,0,
        1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,1,0,1,1,1,0,1,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,

        0,0,0,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,1,
        0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,
        1,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,0,
        0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
        0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    Sprite number_spritesheet = text_spritesheet;
    number_spritesheet.pixels += 16 * 35;




    //Initialise Player Sprite struct
    Sprite player_sprite;
    player_sprite.width = 11;
    player_sprite.height = 7;
    player_sprite.pixels = new uint8_t[77]
    {
        0,0,0,0,0,1,0,0,0,0,0, // .....@.....
        0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
        0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
        0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
    };

    //Player Bullet Sprite
    Sprite player_projectile_sprite;
    player_projectile_sprite.width = 1;
    player_projectile_sprite.height = 3;
    player_projectile_sprite.pixels = new uint8_t[3]
    {
        1, //@
        1, //@
        1, //@
    };

    //Alien Bullet Sprite
    Sprite alien_projectile_sprite[2];
    alien_projectile_sprite[0].width = 3;
    alien_projectile_sprite[0].height = 7;
    alien_projectile_sprite[0].pixels = new uint8_t[21]
    {
        0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0,
    };
    alien_projectile_sprite[1].width = 3;
    alien_projectile_sprite[1].height = 7;
    alien_projectile_sprite[1].pixels = new uint8_t[21]
    {
        0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0,
    };

    //Alien bullet animation (it was a bolt thingy in the original game)
    SpriteAnimation alien_projectile_animation;
    alien_projectile_animation.num_frames = 2;
    alien_projectile_animation.frame_duration = 5;
    alien_projectile_animation.loop = true;
    alien_projectile_animation.time = 0;
    alien_projectile_animation.frames = new Sprite*[2];
    alien_projectile_animation.frames[0] = &alien_projectile_sprite[0];
    alien_projectile_animation.frames[1] = &alien_projectile_sprite[1];


    Sprite explosion_sprite;
    explosion_sprite.width = 13;
    explosion_sprite.height = 7;
    explosion_sprite.pixels = new uint8_t[91]
    {
        0,1,0,0,1,0,0,0,1,0,0,1,0, // .@..@...@..@.
        0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
        0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
        1,1,0,0,0,0,0,0,0,0,0,1,1, // @@.........@@
        0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
        0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
        0,1,0,0,1,0,0,0,1,0,0,1,0  // .@..@...@..@.
    };

    Sprite alien_sprites[6];

    //-------------------------------------------------
    // ***ALIEN 1***

    alien_sprites[0].width = 12;
    alien_sprites[0].height = 8;
    alien_sprites[0].pixels = new uint8_t[96]
    {
        0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
        0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        0,0,0,1,1,0,0,1,1,0,0,0, // ...@@..@@...
        0,0,1,1,0,1,1,0,1,1,0,0, // ..@@.@@.@@..
        1,1,0,0,0,0,0,0,0,0,1,1  // @@........@@
    };


    alien_sprites[1].width = 12;
    alien_sprites[1].height = 8;
    alien_sprites[1].pixels = new uint8_t[96]
    {
        0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
        0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        0,0,1,1,1,0,0,1,1,1,0,0, // ..@@@..@@@..
        0,1,1,0,0,1,1,0,0,1,1,0, // .@@..@@..@@.
        0,0,1,1,0,0,0,0,1,1,0,0  // ..@@....@@..
    };

    // ***ALIEN 1***
    //-------------------------------------------------

    //-------------------------------------------------
    // ***ALIEN 2***

    alien_sprites[2].width = 8;
    alien_sprites[2].height = 8;
    alien_sprites[2].pixels = new uint8_t[64]
    {
        0,0,0,1,1,0,0,0, // ...@@...
        0,0,1,1,1,1,0,0, // ..@@@@..
        0,1,1,1,1,1,1,0, // .@@@@@@.
        1,1,0,1,1,0,1,1, // @@.@@.@@
        1,1,1,1,1,1,1,1, // @@@@@@@@
        0,1,0,1,1,0,1,0, // .@.@@.@.
        1,0,0,0,0,0,0,1, // @......@
        0,1,0,0,0,0,1,0  // .@....@.
    };

    alien_sprites[3].width = 8;
    alien_sprites[3].height = 8;
    alien_sprites[3].pixels = new uint8_t[64]
    {
        0,0,0,1,1,0,0,0, // ...@@...
        0,0,1,1,1,1,0,0, // ..@@@@..
        0,1,1,1,1,1,1,0, // .@@@@@@.
        1,1,0,1,1,0,1,1, // @@.@@.@@
        1,1,1,1,1,1,1,1, // @@@@@@@@
        0,0,1,0,0,1,0,0, // ..@..@..
        0,1,0,1,1,0,1,0, // .@.@@.@.
        1,0,1,0,0,1,0,1  // @.@..@.@
    };

    // ***ALIEN 2***
    //-------------------------------------------------

    //-------------------------------------------------
    // ***ALIEN 3***

    alien_sprites[4].width = 11;
    alien_sprites[4].height = 8;
    alien_sprites[4].pixels = new uint8_t[11 * 8]
    {
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        0,0,0,1,0,0,0,1,0,0,0, // ...@...@...
        0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
        0,1,1,0,1,1,1,0,1,1,0, // .@@.@@@.@@.
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
        1,0,1,0,0,0,0,0,1,0,1, // @.@.....@.@
        0,0,0,1,1,0,1,1,0,0,0  // ...@@.@@...
    };

    alien_sprites[5].width = 11;
    alien_sprites[5].height = 8;
    alien_sprites[5].pixels = new uint8_t[88]
    {
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        1,0,0,1,0,0,0,1,0,0,1, // @..@...@..@
        1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
        1,1,1,0,1,1,1,0,1,1,1, // @@@.@@@.@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        0,1,0,0,0,0,0,0,0,1,0  // .@.......@.
    };

    // ***ALIEN 3***
    //-------------------------------------------------






    
    //xorshift random number generator
    xorshift32_state state{rng};

    
    //Initilise GAME struct
    Game game;
    game.width = buffer_width;
    game.height = buffer_height;
    game.num_aliens = 55;
    game.num_projectiles = 0;
    game.aliens = new Alien[game.num_aliens];

    game.player.x = 112;
    game.player.y = 32;

    game.score = 0;
    game.player.life = 3;

    game.aliens_killed = 0;
    game.alien_update_timer = 0;
    game.alien_update_frequency = 30;
    bool should_change_speed = false;

    int alien_move_dir = 4;
    size_t alien_swarm_position = 24;
    size_t alien_swarm_max_position = game.width - 16 * 11 - 3;




    //2 frame animations of Aliens
    SpriteAnimation alien_animations[no_alien_types];

    for (size_t i = 0; i < no_alien_types; ++i)
    {
        alien_animations[i].loop = true;
        alien_animations[i].num_frames = 2;
        alien_animations[i].frame_duration = 10;
        alien_animations[i].time = 0;

        alien_animations[i].frames = new Sprite * [2];
        alien_animations[i].frames[0] = &alien_sprites[2 * i];
        alien_animations[i].frames[1] = &alien_sprites[2 * i + 1];
    }


    game_running = true;




    

    //Initilise death counter
    //If alien is dead, each frame couter goes down if counter hits 0 sprite won't be drawn
    //death_counters are only used so explosion sprite is visible for 10 frames
    uint8_t* death_counters = new uint8_t[game.num_aliens];
    for (size_t i = 0; i < game.num_aliens; i++)
    {
        death_counters[i] = 10;
    }



     //Initilise alien positions
    for (size_t yi = 0; yi < 5; ++yi)
    {
        for (size_t xi = 0; xi < 11; ++xi)
        {
            Alien& alien = game.aliens[yi * 11 + xi];
            //alien type is changing depending what row we are currently in
            //alien.type = 4 - (yi / 2.5) - 1;
            alien.type = 5 + (yi / 2) - 4;
            //alien.type = (5 - yi) / 2 + 1;
           

            //Do I need this?
           /* game.aliens[yi * 11 + xi].x = 16 * xi + 20;
            game.aliens[yi * 11 + xi].y = 17 * yi + 128;*/

            //const Sprite& sprite = alien_sprites[0];
            const Sprite& sprite = alien_sprites[2 * (alien.type - 1)];

            alien.x = 16 * xi + alien_swarm_position + (explosion_sprite.width - sprite.width) / 2;
            alien.y = 17 * yi + 128;

           // std::cout << (int)alien.type << std::endl;

        }
    }

    /////////////////////////////////////////////
    /////////////////////////////////////////////
    //***GAME LOOP***
    /////////////////////////////////////////////
    /////////////////////////////////////////////


    while (!glfwWindowShouldClose(window) && game_running)
    {
        /* Render here */
        //glClear(GL_COLOR_BUFFER_BIT);

        bufferClear(&buffer, clear_colour);



        if (game.player.life == 0)
        {

            drawText(&buffer, text_spritesheet, "GAME OVER", game.width / 2 - 30, game.height / 2, rgbTOuint32(128, 0, 0));
            drawText(&buffer, text_spritesheet, "SCORE", 4, game.height - text_spritesheet.height - 7, rgbTOuint32(128, 0, 0));
            drawNumber(&buffer, number_spritesheet, game.score, 4 + 2 * number_spritesheet.width, game.height - 2 * number_spritesheet.height - 12, rgbTOuint32(128, 0, 0));

            glTexSubImage2D(
                GL_TEXTURE_2D, 0, 0, 0,
                buffer.width, buffer.height,
                GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                buffer.pixels
            );
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }





        //Draw Text and Score
        drawText(
            &buffer,
            text_spritesheet, "SCORE",
            4, game.height - text_spritesheet.height - 7,
            rgbTOuint32(128, 0, 0)
        );

        drawText(
            &buffer,
            text_spritesheet, "LIVES",
            164, 7,
            rgbTOuint32(128, 0, 0)
        );


        drawNumber(
            &buffer, 
            number_spritesheet,
            game.score, 
            4 + 2 * number_spritesheet.width, 
            game.height - 2 * number_spritesheet.height - 12, 
            rgbTOuint32(128, 0, 0)
        );

        drawNumber(
            &buffer,
            number_spritesheet,
            game.player.life,
            200, 7,
            rgbTOuint32(128, 0, 0)
        );


        //Draw Player
        drawSprite(&buffer, player_sprite, game.player.x, game.player.y, rgbTOuint32(128, 0, 0));
       
        //Draw Aliens
        for (size_t ai = 0; ai < game.num_aliens; ++ai)
        {
            if (!death_counters[ai]) continue;

            const Alien& alien = game.aliens[ai];
            if (alien.type == ALIEN_DEAD)
            {
                drawSprite(&buffer, explosion_sprite, alien.x, alien.y, rgbTOuint32(128, 0, 0));
            }
            else 
            {
            const SpriteAnimation& animation = alien_animations[alien.type - 1];
            size_t current_frame = animation.time / animation.frame_duration;
            const Sprite& sprite = *animation.frames[current_frame];
            drawSprite(&buffer, sprite, alien.x, alien.y, rgbTOuint32(128, 0, 0));
            }
        }
        //Draw Projectiles
        for (size_t bi = 0; bi < game.num_projectiles; ++bi)
        {
            //const Projectile& projectile = game.projectiles[bi];
            //const Sprite& sprite = projectile_sprite;
            //drawSprite(&buffer, sprite, projectile.x, projectile.y, rgbTOuint32(128, 0, 0));
            
            const Projectile& projectile = game.projectiles[bi];
            const Sprite* sprite;

            if (projectile.dir > 0) sprite = &player_projectile_sprite;
            else
            {
                size_t f = alien_projectile_animation.time / alien_projectile_animation.frame_duration;
                sprite = &alien_projectile_sprite[f];
            }
            drawSprite(&buffer, *sprite, projectile.x, projectile.y, rgbTOuint32(128, 0, 0));
        
        }


    /*    glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0,
            buffer.width, buffer.height,
            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
            buffer.pixels
        );
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);*/



        //Projectile movement update
        for (size_t bi = 0; bi < game.num_projectiles; ++bi)
        {
            game.projectiles[bi].y += game.projectiles[bi].dir;
            // Delete bullet if out of range
            if (game.projectiles[bi].y >= game.height || game.projectiles[bi].y < player_projectile_sprite.height)
            {
                game.projectiles[bi] = game.projectiles[game.num_projectiles - 1];
                --game.num_projectiles;
                --bi;
                continue;
            }

            // Alien bullet
            if (game.projectiles[bi].dir < 0)
            {
                bool isPlayerHit = isSpriteOverlap(
                    alien_projectile_sprite[0],
                    game.projectiles[bi].x,
                    game.projectiles[bi].y,
                    player_sprite,
                    game.player.x,
                    game.player.y);

                if (isPlayerHit)
                {
                    --game.player.life;
                    game.projectiles[bi] = game.projectiles[game.num_projectiles - 1];
                    --game.num_projectiles;
                    
                    //TODO: rest of the frame is still going to be simulated so we need checks if game over or not

                    
                    break;

                }
            }
            // Player bullet
            else 
            {
                // First let's check if player bullet hits alien bullet
                for (size_t b = 0; b < game.num_projectiles; b++)
                {
                    if (b == bi) continue;

                    bool isBulletsHit = isSpriteOverlap(
                        player_projectile_sprite,
                        game.projectiles[bi].x,
                        game.projectiles[bi].y,
                        alien_projectile_sprite[0],
                        game.projectiles[b].x,
                        game.projectiles[b].y
                    );

                    if (isBulletsHit)
                    {
                        if (b == game.num_projectiles - 1)
                        {
                            game.projectiles[bi] = game.projectiles[game.num_projectiles - 2];
                        }
                        else if (bi == game.num_projectiles - 1)
                        {
                            game.projectiles[b] = game.projectiles[game.num_projectiles - 2];
                        }
                        else
                        {
                            game.projectiles[(bi < b) ? bi : b] = game.projectiles[game.num_projectiles - 1];
                            game.projectiles[(bi < b) ? b : bi] = game.projectiles[game.num_projectiles - 2];
                        }
                        game.num_projectiles -= 2;
                        break;
                    }


                }

                // Check hit
                for (size_t ai = 0; ai < game.num_aliens; ++ai)
                {
                    const Alien& alien = game.aliens[ai];
                    if (alien.type == ALIEN_DEAD) continue;

                    const SpriteAnimation& animation = alien_animations[alien.type - 1];
                    size_t current_frame = animation.time / animation.frame_duration;
                    const Sprite& alien_sprite = *animation.frames[current_frame];
                    bool isAlienHit = isSpriteOverlap(
                        player_projectile_sprite, 
                        game.projectiles[bi].x, 
                        game.projectiles[bi].y,
                        alien_sprite, 
                        alien.x, 
                        alien.y
                    );
                    if (isAlienHit)
                    {
                        //SCORING = type1 = 20 points, type2 = 30 points, type3 = 40 points
                        //int score_gained = alien.type;
                        game.score += 10 * (1 + alien.type);
                        game.aliens[ai].type = ALIEN_DEAD;


                        // NOTE: Hack to recenter death sprite
                        game.aliens[ai].x -= (explosion_sprite.width - alien_sprite.width) / 2;
                        game.projectiles[bi] = game.projectiles[game.num_projectiles - 1];
                        --game.num_projectiles;

                        break;
                    }
                }
            }
            
           
            //++bi;
        }


        //Alien AI 

        if (should_change_speed)
        {
            should_change_speed = false;
            game.alien_update_frequency /= 2;
            for (size_t i = 0; i < no_alien_types; i++)
            {
                alien_animations[i].frame_duration = game.alien_update_frequency;
            }
        }
        
        if (game.alien_update_timer >= game.alien_update_frequency)
        {
            game.alien_update_timer = 0;

            if (game.aliens_killed < game.num_aliens)
            {
                size_t randA = game.num_aliens * random(&state);
                while (game.aliens[randA].type == ALIEN_DEAD)
                {
                    randA = game.num_aliens * random(&state);
                }
                const Sprite& alien_sprite = *alien_animations[game.aliens[randA].type - 1].frames[0];
                game.projectiles[game.num_projectiles].x = game.aliens[randA].x + alien_sprite.width / 2;
                game.projectiles[game.num_projectiles].y = game.aliens[randA].y - alien_projectile_sprite[0].height;
                game.projectiles[game.num_projectiles].dir = -2;
                ++game.num_projectiles;
            }
        }

        


        //Update aliens and aliens death animation (death_counter)
        for (size_t ai = 0; ai < game.num_aliens; ++ai)
        {
            const Alien& alien = game.aliens[ai];
            if (alien.type == ALIEN_DEAD && death_counters[ai])
            {
                --death_counters[ai];
            }
        }



        // Update animations
        for (size_t i = 0; i < no_alien_types; ++i)
        {
            ++alien_animations[i].time;
            if (alien_animations[i].time == alien_animations[i].num_frames * alien_animations[i].frame_duration)
            {
                alien_animations[i].time = 0;
            }
        }

        ++alien_projectile_animation.time;
        if (alien_projectile_animation.time >= alien_projectile_animation.num_frames * alien_projectile_animation.frame_duration)
        {
            alien_projectile_animation.time = 0;
        }

        game.alien_update_timer++;

        //Player movement update
        int player_move_dir = 2 * move_dir;
        if (player_move_dir != 0)
        {
            if (game.player.x + player_sprite.width + player_move_dir >= game.width)
            {
                game.player.x = game.width - player_sprite.width;
            }
            else if ((int)game.player.x + player_move_dir <= 0)
            {
                game.player.x = 0;
            }
            else game.player.x += player_move_dir;
        }


           //Projectile creation (when space is pressed)
        if (is_shooting && game.num_projectiles < MAX_PROJECTILES)
        {
            game.projectiles[game.num_projectiles].x = game.player.x + player_sprite.width / 2;
            game.projectiles[game.num_projectiles].y = game.player.y + player_sprite.height;
            game.projectiles[game.num_projectiles].dir = 2;
            ++game.num_projectiles;
        }
        is_shooting = false;

        ////Projectile creation (when space is pressed)
        //if (alien_is_shooting && game.num_projectiles < MAX_PROJECTILES)
        //{
        //    game.projectiles[game.num_projectiles].x = game.player.x + player_sprite.width / 2;
        //    game.projectiles[game.num_projectiles].y = game.player.y + player_sprite.height;
        //    game.projectiles[game.num_projectiles].dir = -2;
        //    ++game.num_projectiles;
        //}
        //alien_is_shooting = false;
        


      

        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0,
            buffer.width, buffer.height,
            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
            buffer.pixels
        );
        
        glDrawArrays(GL_TRIANGLES, 0, 4);
       
        
        glfwSwapBuffers(window);

        glfwPollEvents();
       // glClear(GL_COLOR_BUFFER_BIT);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    

    glDeleteVertexArrays(1, &fullscreen_triangle_vao);

    for (size_t i = 0; i < no_alien_types * 2; ++i)
    {
        delete[] alien_sprites[i].pixels;
    }

    delete[] explosion_sprite.pixels;

    for (size_t i = 0; i < no_alien_types; ++i)
    {
        delete[] alien_animations[i].frames;
    }
    delete[] buffer.pixels;
    delete[] game.aliens;
    delete[] death_counters;

    return 0;
   
}
