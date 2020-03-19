#include "qPlayStation.hpp"

const int WINDOW_WIDTH = 640;
const int WINDOW_HEIGHT = 480;

SDL_Window* window;
SDL_Surface* screenSurface;

SDL_GLContext glContext;

GLuint vao;

typedef float t_mat4x4[16]; //Typedef 4x4 matrix to a float array of length 16

static inline void mat4x4_ortho(t_mat4x4 out, float left, float right, float bottom, float top, float znear, float zfar)
{
#define T(a, b) (a * 4 + b)

    out[T(0, 0)] = 2.0f / (right - left);
    out[T(0, 1)] = 0.0f;
    out[T(0, 2)] = 0.0f;
    out[T(0, 3)] = 0.0f;

    out[T(1, 1)] = 2.0f / (top - bottom);
    out[T(1, 0)] = 0.0f;
    out[T(1, 2)] = 0.0f;
    out[T(1, 3)] = 0.0f;

    out[T(2, 2)] = -2.0f / (zfar - znear);
    out[T(2, 0)] = 0.0f;
    out[T(2, 1)] = 0.0f;
    out[T(2, 3)] = 0.0f;

    out[T(3, 0)] = -(right + left) / (right - left);
    out[T(3, 1)] = -(top + bottom) / (top - bottom);
    out[T(3, 2)] = -(zfar + znear) / (zfar - znear);
    out[T(3, 3)] = 1.0f;

#undef T
}

static const char* vertex_shader =
"#version 130\n"
"in vec3 i_position;\n"
"in vec4 i_color;\n"
"out vec4 v_color;\n"
"uniform mat4 u_projection_matrix;\n"
"void main() {\n"
"    v_color = i_color;\n"
"    gl_Position = u_projection_matrix * vec4( i_position, 1.0 );\n"
"}\n";

static const char* fragment_shader =
"#version 130\n"
"in vec4 v_color;\n"
"out vec4 o_color;\n"
"void main() {\n"
"    o_color = v_color;\n"
"}\n";

void initSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
    window = SDL_CreateWindow("qPlayStation", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if (window == NULL)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
    screenSurface = SDL_GetWindowSurface(window);
}

void initOpenGL()
{
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    glContext = SDL_GL_CreateContext(window);
    GLenum err = glewInit();

    GLuint vs, fs, program;

    vs = glCreateShader(GL_VERTEX_SHADER);
    fs = glCreateShader(GL_FRAGMENT_SHADER);

    int length = strlen(vertex_shader);
    glShaderSource(vs, 1, (const GLchar**)&vertex_shader, &length);
    glCompileShader(vs);

    GLint status;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        fprintf(stderr, "vertex shader compilation failed\n");
        exit(1);
    }

    length = strlen(fragment_shader);
    glShaderSource(fs, 1, (const GLchar**)&fragment_shader, &length);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        fprintf(stderr, "fragment shader compilation failed\n");
        exit(1);
    }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glBindAttribLocation(program, 0, "i_position");
    glBindAttribLocation(program, 1, "i_color");
    glLinkProgram(program);

    glUseProgram(program);

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    GLuint vbo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)(4 * sizeof(float)));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, 0);

    const GLfloat g_vertex_buffer_data[] = {
        /*  R, G, B, A, X, Y  */
            1, 0, 0, 1, 0, 0, 0,
            0, 1, 0, 1, WINDOW_WIDTH, 0, 0,
            0, 0, 1, 1, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

    t_mat4x4 projection_matrix;
    mat4x4_ortho(projection_matrix, 0.0f, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT, 0.0f, 0.0f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(program, "u_projection_matrix"), 1, GL_FALSE, projection_matrix);
}

void cleanExit()
{

}

// Arg 1 = BIOS path, Arg 2 = Game Path
int main(int argc, char* args[])
{
    if (argc < 2)
    {
        logging::fatal("need BIOS path", logging::logSource::qPS);
    }
    /*if (argc < 3) //only need BIOS for now
    {
        logging::fatal("need game path", logging::logSource::qPS);
    }*/

    initSDL();
    initOpenGL();

    bios* BIOS = new bios(args[1]);
    memory* Memory = new memory(BIOS);
    cpu* CPU = new cpu(Memory);

    int exitCode = 0;

    try
    {
        SDL_Event event;
        bool running = true;
        while (running)
        {
            glClear(GL_COLOR_BUFFER_BIT);

            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_QUIT: running = false; break;
                }
            }

            CPU->step();

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            SDL_GL_SwapWindow(window);
            SDL_Delay(13);
        }
    }
    catch (int e)
    {
        //just continue to the cleanup
        exitCode = 1;
    }

    delete(BIOS);
    delete(Memory);
    delete(CPU);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}