// SpaceInvaders.cpp 
// Recreating the classic game in C++ and openGL

#include <iostream>
#include <cstdio>
#include <GL/glew.h>
//#include <gl/GL.h>
#include <GLFW/glfw3.h>


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

const size_t buffer_width = 224;
const size_t buffer_height = 256;

//const size_t buffer_width = 448;
//const size_t buffer_height = 512;


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

struct Sprite
{
    
    size_t width, height;
    uint8_t* pixels;
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

struct Game
{
    size_t width, height;
    size_t num_aliens;
    Alien* aliens;
    Player player;
};

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

int main()
{
    glfwSetErrorCallback(errorCallback);

    
    if (!glfwInit()){ return -1; }


    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);


    //let's creata a window
    GLFWwindow*  window = glfwCreateWindow(2 * buffer_width, 2 * buffer_height, "Space Invaders", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

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

    //Initialise SPRITE struct
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


    Sprite alien_sprite;
    alien_sprite.width = 11;
    alien_sprite.height = 8;
    alien_sprite.pixels = new uint8_t[11 * 8]
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
    
    

    //Initilise GAME struct
    Game game;
    game.width = buffer_width;
    game.height = buffer_height;
    game.num_aliens = 55;
    game.aliens = new Alien[game.num_aliens];

    game.player.x = 112;
    game.player.y = 32;

    game.player.life = 3;

   

    //***GAME LOOP***

    


    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        //glClear(GL_COLOR_BUFFER_BIT);
        bufferClear(&buffer, clear_colour);

        drawSprite(&buffer, player_sprite, game.player.x, game.player.y, rgbTOuint32(128, 0, 0));

        //Initilise alien positions
        for (size_t yi = 0; yi < 5; ++yi)
        {
            for (size_t xi = 0; xi < 11; ++xi)
            {
                //yi * 11 + xi
                game.aliens[yi * 11 + xi].x = 16 * xi + 20;
                game.aliens[yi * 11 + xi].y = 17 * yi + 128;

            }
        }

        //Draw Aliens
        for (size_t ai = 0; ai < game.num_aliens; ++ai)
        {
            const Alien& alien = game.aliens[ai];
            drawSprite(&buffer, alien_sprite, alien.x, alien.y, rgbTOuint32(128, 0, 0));
        }

        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0,
            buffer.width, buffer.height,
            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
            buffer.pixels
        );

        glDrawArrays(GL_TRIANGLES, 0, 4);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();


   
}
