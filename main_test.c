#include <windows.h>

#include "util/opengl/glFunctions.c"
#include "util/opengl/openglProgram.c"
#include "util/font.c"
#include "./util/win32.c"
#include "game.c"
#include "tiles.c"

u32 isRunning = 1;
u32 isFullscreen = 0;
V3f view;
V3f playerPos = {500, 500};

typedef enum
{
    W,
    S,
    A,
    D,
    KeySize
} Key;

Key keys[KeySize] = {0};

LRESULT OnEvent(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        isRunning = 0;
        break;

    case WM_PAINT:
        PAINTSTRUCT paint = {0};
        HDC paintDc = BeginPaint(window, &paint);
        EndPaint(window, &paint);
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            isRunning = 0;
        }
        if (wParam == VK_F11)
        {
            isFullscreen = isFullscreen == 0 ? 1 : 0;
            SetFullscreen(window, isFullscreen);
        }

        if (wParam == 'W')
            keys[W] = 1;
        if (wParam == 'S')
            keys[S] = 1;
        if (wParam == 'A')
            keys[A] = 1;
        if (wParam == 'D')
            keys[D] = 1;

        break;
    case WM_KEYUP:
        if (wParam == 'W')
            keys[W] = 0;
        if (wParam == 'S')
            keys[S] = 0;
        if (wParam == 'A')
            keys[A] = 0;
        if (wParam == 'D')
            keys[D] = 0;
        break;
    case WM_SIZE:
        view.x = LOWORD(lParam);
        view.y = HIWORD(lParam);

        glViewport(0, 0, view.x, view.y);
        break;
    }
    return DefWindowProc(window, message, wParam, lParam);
}

#define POINTS_PER_VERTEX 5
#define VERTICES_IN_TEXTURE 6
GLuint vertexBuffer;
GLuint vertexArray;

void InitVertices()
{
    glGenBuffers(1, &vertexBuffer);

    float xN = -0.5f;
    float yN = -0.5;
    float size = 1.0f;

    // clang-format off
    float cubeVertices[] = {
    //   X   Y                Z       //U  V
        xN, yN,               0,      0, 0,
        xN, yN + size,        0,      0, 1,
        xN + size, yN,        0,      1, 0,

        xN + size, yN,        0,      1, 0,
        xN + size, yN + size, 0,      1, 1,
        xN, yN + size,        0,      0, 1,
    };
    // clang-format on

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    size_t stride = POINTS_PER_VERTEX * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
typedef struct MatrixLocations
{
    GLint projection;
    GLint view;
    GLint model;
    GLint color;
    GLuint program;
} MatrixLocations;

MatrixLocations pureProgramLocations;
MatrixLocations textureProgramLocations;

void LocateProgram(MatrixLocations *pureProgramLocations)
{
    GLuint program = pureProgramLocations->program;
    pureProgramLocations->projection = glGetUniformLocation(program, "projection");
    pureProgramLocations->view = glGetUniformLocation(program, "view");
    pureProgramLocations->model = glGetUniformLocation(program, "model");
    pureProgramLocations->color = glGetUniformLocation(program, "color");
}

f32 tileSize = 80;
f32 playerSize = 60;

void DrawTile(u32 x, u32 y)
{
    glUniform3f(pureProgramLocations.color, bg.r, bg.g, bg.b);

    Mat4 model = Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), (V3f){tileSize / 2 + x * tileSize, tileSize / 2 + y * tileSize, 0}), (V3f){tileSize, tileSize, 0});

    glUniformMatrix4fv(pureProgramLocations.model, 1, GL_TRUE, model.values);
    glDrawArrays(GL_TRIANGLES, 0, VERTICES_IN_TEXTURE);
}

void LoadTexture(MyBitmap *bitmap, GLuint *texture)
{
    // FileContent file = ReadMyFileImp(path);
    // MyBitmap bitmap = {0};
    // ParseBmpFile(&file, &bitmap);

    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width, bitmap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    // VirtualFreeMemory(file.content);
}

void WinMainCRTStartup()
{
    PreventWindowsDPIScaling();

    HINSTANCE instance = GetModuleHandle(0);

    HWND window = OpenWindow(OnEvent, black, "Game");
    HDC dc = GetDC(window);
    Win32InitOpenGL(window);
    InitFunctions();

    InitVertices();

    InitFontSystem();
    InitTiles();

    wglSwapIntervalEXT(0);

    pureProgramLocations.program = CreateProgram(".\\shaders\\pure_vertex.glsl", ".\\shaders\\pure_fragment.glsl");
    textureProgramLocations.program = CreateProgram(".\\shaders\\texture_vertex.glsl", ".\\shaders\\texture_fragment.glsl");

    LocateProgram(&pureProgramLocations);
    LocateProgram(&textureProgramLocations);

    i64 frequency = GetPerfFrequency();
    i64 start = GetPerfCounter();

    Arena fontArena = CreateArena(MB(2));
    FontData font = {0};

    InitFontData(&font, FontInfoClearType("Consolas", 22, 0xfff0f0f0, 0xff1e1e1e), &fontArena);

    currentFont = &font;
    GLuint textures[MAX_CHAR_CODE] = {0};
    for (char ch = ' '; ch < MAX_CHAR_CODE; ch++)
    {
        LoadTexture(&font.textures[ch], &textures[ch]);
    }

    while (isRunning)
    {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        V2f shift = {0};
        if (keys[W] == 1)
            shift.y = 1;
        if (keys[S] == 1)
            shift.y = -1;
        if (keys[A] == 1)
            shift.x = -1;
        if (keys[D] == 1)
            shift.x = 1;

        if (shift.x != 0 && shift.y != 0)
        {
            shift.x *= ONE_OVER_SQUARE_ROOT_OF_TWO;
            shift.y *= ONE_OVER_SQUARE_ROOT_OF_TWO;
        }

        f32 playerSpeed = 700;

        i64 end = GetPerfCounter();
        float frameSec = ((end - start) / (float)frequency);

        playerPos.y += shift.y * playerSpeed * frameSec;
        playerPos.x += shift.x * playerSpeed * frameSec;

        glClearColor(black.r, black.g, black.b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(pureProgramLocations.program);

        Mat4 projection = CreateScreenProjection(view.x, view.y);
        glUniformMatrix4fv(pureProgramLocations.projection, 1, GL_TRUE, projection.values);

        Mat4 viewMatrix = Mat4TranslateV3f(Mat4Identity(), V3fMult(V3fSub(playerPos, V3fMult(view, 0.5f)), -1.0f));
        glUniformMatrix4fv(pureProgramLocations.view, 1, GL_TRUE, viewMatrix.values);

        for (int y = 0; y < worldHeight; y++)
        {
            for (int x = 0; x < worldWidth; x++)
            {
                if (tiles[y * worldWidth + x] != Hole)
                    DrawTile(x, y);
            }
        }

        glUniform3f(pureProgramLocations.color, 1, 1, 1);
        Mat4 model = Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), playerPos), (V3f){playerSize, playerSize, 0});
        glUniformMatrix4fv(pureProgramLocations.model, 1, GL_TRUE, model.values);
        glDrawArrays(GL_TRIANGLES, 0, VERTICES_IN_TEXTURE);

        glUseProgram(textureProgramLocations.program);
        glUniformMatrix4fv(textureProgramLocations.projection, 1, GL_TRUE, projection.values);
        glUniformMatrix4fv(textureProgramLocations.view, 1, GL_TRUE, viewMatrix.values);

        char message[250] = {0};
        i32 pos = 0;
        message[pos++] = 'X';
        message[pos++] = ':';
        pos += AppendI32((i32)playerPos.x, &message[pos]);
        message[pos++] = '\0';

        i32 p = 0;
        f32 x = 200;
        while (message[p] != '\0')
        {
            char cha = message[p];
            MyBitmap *ch = &font.textures[cha];
            x += ch->width / 2;
            Mat4 textModel = Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), (V3f){x, 20, 0}), (V3f){ch->width, ch->height, 0});
            glUniformMatrix4fv(textureProgramLocations.model, 1, GL_TRUE, textModel.values);

            glBindTexture(GL_TEXTURE_2D, textures[cha]);
            glDrawArrays(GL_TRIANGLES, 0, VERTICES_IN_TEXTURE);

            x += ch->width / 2 + GetKerningValue(cha, message[p + 1]);
            p++;
        }

        SwapBuffers(dc);

        start = end;
    }

    ExitProcess(0);
}