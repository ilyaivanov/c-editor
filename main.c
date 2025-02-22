#include <windows.h>
#include <Windowsx.h>
#include "util/opengl/glFunctions.c"
#include "util/opengl/openglProgram.c"
#include "util/sincos.c"
#include "util/win32.c"
#include "util/bmp.c"
#include <math.h>
#include "util/atan.c"
#include "sound.c"

u32 isRunning = 1;

u32 isFullscreen = 0;
u32 isADown = 0;
u32 isDDown = 0;
u32 isSDown = 0;
u32 isWDown = 0;
BITMAPINFO bitmapInfo = {0};
MyBitmap bitmap = {0};

V2i view;
V3f playerPos;
V3f mouse;

GLuint vertexBuffer;
GLuint vertexArray;

#define POINTS_PER_VERTEX 5
#define VERTICES_IN_TEXTURE 6
#define PIXELS_PER_TEXEL 8
#define TILE_SIZE 16

float playerSpeed = 700;

void UpdateScale(HWND window)
{
    HDC dc = GetDC(window);
}

void DrawBitmap(HDC dc)
{
    StretchDIBits(dc,
                  0, 0, bitmap.width, bitmap.height,
                  0, 0, bitmap.width, bitmap.height,
                  bitmap.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

typedef struct Bullet
{
    V3f pos;
    V3f direction;
    float speed;
    u32 isAlive;
} Bullet;

Bullet bullets[1024] = {0};

V3f GetCameraPos()
{
    return (V3f){playerPos.x, playerPos.y, 0.0f};
}

LRESULT OnEvent(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{

    V3f cameraPos;
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        isRunning = 0;
        break;

    case WM_LBUTTONDOWN:
        Bullet *bullet;
        for (int i = 0; i < ArrayLength(bullets); i++)
        {
            if (!bullets[i].isAlive)
            {
                bullet = &bullets[i];
                break;
            }
        }
        bullet->isAlive = 1;
        cameraPos = GetCameraPos();

        V3f mouseWorldPos = {0};

        mouseWorldPos.x = mouse.x - view.x / 2 + cameraPos.x;
        mouseWorldPos.y = cameraPos.y + (view.y - mouse.y - view.y / 2);

        bullet->direction = V3fNormalize(V3fSub(mouseWorldPos, playerPos));
        bullet->pos = playerPos;
        bullet->speed = 2500;

        PlayFile(Fire);

        break;

    case WM_MOUSEMOVE:
        mouse.x = GET_X_LPARAM(lParam);
        mouse.y = GET_Y_LPARAM(lParam);

        // cameraPos = GetCameraPos();

        // V3f r = V3fAdd(V3fSub(mouse, (V3f){(f32)view.x / 2, (f32)view.y / 2, 0.0f}), cameraPos);

        // char message[255] = {0};
        // u32 pos = 0;
        // message[pos++] = 'M';
        // message[pos++] = ':';
        // pos += AppendI32((i32)mouse.x, message + pos);
        // message[pos++] = ' ';
        // message[pos++] = 'C';
        // message[pos++] = ':';
        // pos += AppendI32((i32)cameraPos.x, message + pos);
        // message[pos++] = ' ';
        // message[pos++] = 'r';
        // message[pos++] = ':';
        // pos += AppendI32((i32)r.x, message + pos);
        // message[pos++] = '\n';
        // message[pos++] = '\0';
        // OutputDebugStringA(message);

        break;

    case WM_KEYDOWN:
        if (wParam == VK_F11)
        {
            isFullscreen = isFullscreen == 0 ? 1 : 0;
            SetFullscreen(window, isFullscreen);
        }

        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            isRunning = 0;
        }
        if (wParam == 'S')
        {
            isSDown = 1;
        }
        if (wParam == 'W')
        {
            isWDown = 1;
        }
        if (wParam == 'A')
        {
            isADown = 1;
        }
        if (wParam == 'D')
        {
            isDDown = 1;
        }

        if (wParam == 'Z')
            PlayFile(One);
        if (wParam == 'X')
            PlayFile(Two);
        if (wParam == 'C')
            PlayFile(Go);
        break;

    case WM_KEYUP:
        if (wParam == 'S')
        {
            isSDown = 0;
        }
        if (wParam == 'W')
        {
            isWDown = 0;
        }
        if (wParam == 'A')
        {
            isADown = 0;
        }
        if (wParam == 'D')
        {
            isDDown = 0;
        }
        break;

    case WM_SIZE:
        view.x = LOWORD(lParam);
        view.y = HIWORD(lParam);

        UpdateScale(window);

        // GetClientRect(window, &rect);
        glViewport(0, 0, view.x, view.y);

        // if (bitmap.pixels)
        // {
        //     VirtualFree(bitmap.pixels, 0, MEM_RELEASE);
        // }

        // bitmap.width = rect.right - rect.left;
        // bitmap.height = rect.bottom - rect.top;
        // bitmap.bytesPerPixel = 4;

        // bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
        // bitmapInfo.bmiHeader.biBitCount = bitmap.bytesPerPixel * 8;
        // bitmapInfo.bmiHeader.biWidth = bitmap.width;
        // bitmapInfo.bmiHeader.biHeight = -bitmap.height; // makes rows go up, instead of going down by default
        // bitmapInfo.bmiHeader.biPlanes = 1;
        // bitmapInfo.bmiHeader.biCompression = BI_RGB;

        // i32 size = bitmap.width * bitmap.height * bitmap.bytesPerPixel;
        // bitmap.pixels = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);

        // glViewport(0, 0, width, height);
        InvalidateRect(window, NULL, TRUE);
        // HDC dc = GetDC(window);

        break;

    case WM_PAINT:
        PAINTSTRUCT paint = {0};
        HDC paintDc = BeginPaint(window, &paint);
        // Draw(state);
        // SwapBuffers(paintDc);
        DrawBitmap(paintDc);
        EndPaint(window, &paint);
        break;
    }
    return DefWindowProc(window, message, wParam, lParam);
}
inline i64 GetPerfFrequency()
{
    LARGE_INTEGER res;
    QueryPerformanceFrequency(&res);
    return res.QuadPart;
}

inline i64 GetPerfCounter()
{
    LARGE_INTEGER res;
    QueryPerformanceCounter(&res);
    return res.QuadPart;
}

void DrawTexture(GLuint texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLES, 0, VERTICES_IN_TEXTURE);
}

void InitVertices()
{
    glGenBuffers(1, &vertexBuffer);
    glGenVertexArrays(1, &vertexArray);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

    glBindVertexArray(vertexArray);

    size_t stride = POINTS_PER_VERTEX * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    float xN = -0.5f;
    float yN = -0.5;
    float size = 1.0f;

    // clang-format off
    float cubeVertices[] = {
    //   X   Y                Z       U  V
        xN, yN,               1,      0, 0,
        xN, yN + size,        1,      0, 1,
        xN + size, yN,        1,      1, 0,

        xN + size, yN,        1,      1, 0,
        xN + size, yN + size, 1,      1, 1,
        xN, yN + size,        1,      0, 1,
    };
    // clang-format on

    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void LoadTexture(char *path, GLuint *texture)
{
    FileContent file = ReadMyFileImp(path);
    MyBitmap bitmap = {0};
    ParseBmpFile(&file, &bitmap);

    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width, bitmap.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    VirtualFreeMemory(file.content);
}

GLuint vertexBuffer;
GLuint vertexArray;

void WinMainCRTStartup()
{
    PreventWindowsDPIScaling();
    // InitAppMemory();

    HINSTANCE instance = GetModuleHandle(0);
    HWND window = OpenWindow(OnEvent, (V3f){0.3, 0.3, 0.3});
    HDC dc = GetDC(window);

    float f = 0;

    Win32InitOpenGL(window);

    InitFunctions();

    InitSound(window);

    if (isFullscreen)
        SetFullscreen(window, isFullscreen);

    GLuint baseProgram = CreateProgram("..\\shaders\\shader_vertex.glsl", "..\\shaders\\shader_fragment.glsl");
    GLuint pureProgram = CreateProgram("..\\shaders\\pure_vertex.glsl", "..\\shaders\\pure_fragment.glsl");

    timeBeginPeriod(1);
    wglSwapIntervalEXT(0);

    V3f color = {1.0, 1.0, 1.0};

    u64 startTime = GetPerfCounter();
    u64 endTime = 0;
    u64 frequency = GetPerfFrequency();

    InitVertices();

    //
    // TERXTURE
    //

    GLint projectionLocation = glGetUniformLocation(baseProgram, "projection");
    GLint viewMatrixLocation = glGetUniformLocation(baseProgram, "view");
    GLint modelMatrixLocation = glGetUniformLocation(baseProgram, "model");

    GLint projectionLocation2 = glGetUniformLocation(pureProgram, "projection");
    GLint modelMatrixLocation2 = glGetUniformLocation(pureProgram, "model");
    GLint colorLocation = glGetUniformLocation(pureProgram, "color");

    GLuint idle;
    LoadTexture("..\\textures\\Idle_idle_0.bmp", &idle);

    GLuint idle2;
    LoadTexture("..\\textures\\Idle_idle_1.bmp", &idle2);

    GLuint idle3;
    LoadTexture("..\\textures\\Idle_idle_2.bmp", &idle3);

    GLuint gun;
    LoadTexture("..\\textures\\gun.bmp", &gun);

    GLuint bulletTexture;
    LoadTexture("..\\textures\\bullet.bmp", &bulletTexture);

    GLuint floor1;
    LoadTexture("..\\textures\\floor\\floor1.bmp", &floor1);

    GLuint floor2;
    LoadTexture("..\\textures\\floor\\floor2.bmp", &floor2);

    GLuint top;
    LoadTexture("..\\textures\\top.bmp", &top);
    GLuint wall;
    LoadTexture("..\\textures\\wall.bmp", &wall);
    GLuint wallShadowLeft;
    LoadTexture("..\\textures\\wall_shadow_left.bmp", &wallShadowLeft);
    GLuint wallShadowRight;
    LoadTexture("..\\textures\\wall_shadow_right.bmp", &wallShadowRight);
    GLuint shadowTop;
    LoadTexture("..\\textures\\shadow_top.bmp", &shadowTop);
    GLuint shadowTopLeft;
    LoadTexture("..\\textures\\shadow_top_left.bmp", &shadowTopLeft);
    GLuint shadowTopRight;
    LoadTexture("..\\textures\\shadow_top_right.bmp", &shadowTopRight);
    GLuint shadowLeft;
    LoadTexture("..\\textures\\shadow_left.bmp", &shadowLeft);
    GLuint shadowRight;
    LoadTexture("..\\textures\\shadow_right.bmp", &shadowRight);
    GLuint flag;
    LoadTexture("..\\textures\\flag.bmp", &flag);

    GLuint holeCenter;
    LoadTexture("..\\textures\\holes\\hole_center.bmp", &holeCenter);

    GLuint holeLeft;
    LoadTexture("..\\textures\\holes\\hole_left.bmp", &holeLeft);

    GLuint holeRight;
    LoadTexture("..\\textures\\holes\\hole_right.bmp", &holeRight);

    GLuint holeBottomLeft;
    LoadTexture("..\\textures\\holes\\hole_bottom_left.bmp", &holeBottomLeft);

    GLuint holeBottomRight;
    LoadTexture("..\\textures\\holes\\hole_bottom_right.bmp", &holeBottomRight);

    GLuint holeBottom;
    LoadTexture("..\\textures\\holes\\hole_bottom.bmp", &holeBottom);

    int isPlayerRunning = 0;
    int currentAnim = 0;
    float runAnimationSpeed = 80;
    float idleAnimationSpeed = 150;
    float timeToNextFrame = idleAnimationSpeed;
    GLuint idles[] = {idle, idle2, idle2, idle3, idle3};
    GLuint runsAnimation[7] = {0};

    LoadTexture("..\\textures\\run_0.bmp", &runsAnimation[0]);
    LoadTexture("..\\textures\\run_1.bmp", &runsAnimation[1]);
    LoadTexture("..\\textures\\run_2.bmp", &runsAnimation[2]);
    LoadTexture("..\\textures\\run_3.bmp", &runsAnimation[3]);
    LoadTexture("..\\textures\\run_4.bmp", &runsAnimation[4]);
    LoadTexture("..\\textures\\run_5.bmp", &runsAnimation[5]);
    LoadTexture("..\\textures\\run_6.bmp", &runsAnimation[6]);
    LoadTexture("..\\textures\\run_7.bmp", &runsAnimation[7]);

    FileContent map = ReadMyFileImp("..\\map.txt");

    int tileX = 0;
    int tileY = 0;

    int playerTileX = 0;
    int playerTileY = 0;
    for (i32 i = 0; i <= map.size; i++)
    {

        if (map.content[i] == 's')
        {
            playerTileX = tileX;
            playerTileY = tileY;
        }

        if (map.content[i] == '\n')
        {
            tileY++;
            tileX = 0;
        }
        else
        {
            tileX++;
        }
    }

    int tileRowsCount = tileY + 1;
    int tileColsCount = tileX + 1;

    playerPos.x = playerTileX * TILE_SIZE * PIXELS_PER_TEXEL;
    playerPos.y = (tileRowsCount - playerTileY - 1) * TILE_SIZE * PIXELS_PER_TEXEL + 10 * PIXELS_PER_TEXEL;

    while (isRunning)
    {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        endTime = GetPerfCounter();

        float frameSec = ((endTime - startTime) / (float)frequency);

        V2f shift = {0};

        if (isADown)
            shift.x = -1;

        if (isDDown)
            shift.x = 1;

        if (isWDown)
            shift.y = 1;

        if (isSDown)
            shift.y = -1;

        if (shift.x != 0 && shift.y != 0)
        {
            shift.x *= ONE_OVER_SQUARE_ROOT_OF_TWO;
            shift.y *= ONE_OVER_SQUARE_ROOT_OF_TWO;
        }

        int isLookingLeft = (mouse.x < view.x / 2);

        // when running backwards
        int startRunIndex = ArrayLength(runsAnimation) - 1;
        int runIndexDelta = -1;

        // when running forward
        if ((isLookingLeft && shift.x < 0) || (!isLookingLeft && shift.x > 0))
        {
            startRunIndex = 0;
            runIndexDelta = 1;
        }

        if ((shift.x != 0 || shift.y != 0) && !isPlayerRunning)
        {
            // Starting to run
            currentAnim = startRunIndex;
            timeToNextFrame = runAnimationSpeed;
            isPlayerRunning = 1;
        }
        else if ((shift.x == 0 && shift.y == 0) && isPlayerRunning)
        {
            // Starting to idle
            currentAnim = 0;
            timeToNextFrame = idleAnimationSpeed;
            isPlayerRunning = 0;
        }

        timeToNextFrame -= frameSec * 1000;

        if (timeToNextFrame <= 0)
        {

            if (!isPlayerRunning)
            {
                currentAnim++;
                if (currentAnim >= ArrayLength(idles))
                    currentAnim = 0;
            }
            else
            {
                currentAnim += runIndexDelta;
                // if (currentAnim >= ArrayLength(runsAnimation))
                //     currentAnim = 0;
                if (currentAnim < 0 || currentAnim > ArrayLength(runsAnimation) - 1)
                    currentAnim = startRunIndex;
            }

            timeToNextFrame = isPlayerRunning ? runAnimationSpeed : idleAnimationSpeed;
        }

        playerPos.x += shift.x * frameSec * playerSpeed;
        playerPos.y += shift.y * frameSec * playerSpeed;

        glUseProgram(baseProgram);

        glClearColor(184.0f / 255, 111.0f / 255, 80.0f / 255, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Mat4 projection = CreateScreenProjection(view);
        glUniformMatrix4fv(projectionLocation, 1, GL_TRUE, projection.values);

        V3f halfView = (V3f){(f32)view.x / 2, (f32)view.y / 2, 0.0f};
        V3f cameraPos = V3fMult(V3fSub(GetCameraPos(), halfView), -1);

        Mat4 viewMatrix = Mat4TranslateV3f(Mat4Identity(), cameraPos);
        glUniformMatrix4fv(viewMatrixLocation, 1, GL_TRUE, viewMatrix.values);

        V3f floorScale = {PIXELS_PER_TEXEL * TILE_SIZE, PIXELS_PER_TEXEL * TILE_SIZE, 1.0f};

        int tileX = 0;
        int tileY = 0;

        for (i32 i = 0; i <= map.size; i++)
        {
            char ch = map.content[i];

            GLuint textureId = 0;

#define tileOnPrevLine (map.content[i - tileColsCount])
#define tileOnPrevCell (map.content[i - 1])
#define tileOnNextCell (map.content[i + 1])

            if (ch == 'H' && i > tileColsCount && tileOnNextCell == ' ' && tileOnPrevLine == ' ')
                textureId = holeBottomLeft;
            else if (ch == 'H' && i > tileColsCount && tileOnPrevCell == ' ' && tileOnPrevLine == ' ')
                textureId = holeBottomRight;
            else if (ch == 'H' && i > 0 && tileOnPrevLine == ' ')
                textureId = holeBottom;
            else if (ch == 'H' && i > 0 && tileOnPrevCell == ' ')
                textureId = holeRight;
            else if (ch == 'H' && i > 0 && tileOnNextCell == ' ')
                textureId = holeLeft;

            else if (ch == 'H')
                textureId = holeCenter;
            else if ((ch == ' ' || ch == 's') && i > tileColsCount && (tileOnPrevLine == 'W' || tileOnPrevLine == 'F') && (tileOnPrevCell == 'X' || tileOnPrevCell == 'W'))
                textureId = shadowTopLeft;
            else if ((ch == ' ' || ch == 's') && i > tileColsCount && i < map.size - 1 && (tileOnPrevLine == 'W' || tileOnPrevLine == 'F') && (tileOnNextCell == 'X' || tileOnNextCell == 'W'))
                textureId = shadowTopRight;
            else if ((ch == ' ' || ch == 's') && i > tileColsCount && (tileOnPrevLine == 'W' || tileOnPrevLine == 'F'))
                textureId = shadowTop;
            else if ((ch == ' ' || ch == 's') && i > 1 && (tileOnPrevCell == 'X' || tileOnPrevCell == 'W' || tileOnPrevCell == 'F'))
                textureId = shadowLeft;
            else if ((ch == ' ' || ch == 's') && i > 1 && (tileOnNextCell == 'X' || tileOnNextCell == 'W' || tileOnNextCell == 'F'))
                textureId = shadowRight;
            else if (ch == 'W' && i > 0 && map.content[i - 1] == 'X')
                textureId = wallShadowLeft;
            else if (ch == 'W' && i < map.size - 1 && map.content[i + 1] == 'X')
                textureId = wallShadowRight;
            else if (ch == 'X')
                textureId = top;
            else if (ch == 'W')
                textureId = wall;
            else if (ch == 'F')
                textureId = flag;

            if (textureId > 0)
            {
                float x = tileX * TILE_SIZE * PIXELS_PER_TEXEL;
                float y = (tileRowsCount - tileY - 1) * TILE_SIZE * PIXELS_PER_TEXEL;
                V3f position = {x, y, 0.0f};
                Mat4 modelMatrix = Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), position), floorScale);
                glUniformMatrix4fv(modelMatrixLocation, 1, GL_TRUE, modelMatrix.values);
                DrawTexture(textureId);
            }

            if (ch == '\n')
            {
                tileY++;
                tileX = 0;
            }
            else
            {
                tileX++;
            }
        }

        V3f charScale = {PIXELS_PER_TEXEL * 40.0f, PIXELS_PER_TEXEL * 40.0f, 1.0f};

        if (mouse.x - cameraPos.x < playerPos.x)
            charScale.x *= -1;

        V3f playerPosMat = {playerPos.x, playerPos.y, 0.0f};
        Mat4 modelMatrix = Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), playerPosMat), charScale);
        glUniformMatrix4fv(modelMatrixLocation, 1, GL_TRUE, modelMatrix.values);

        GLuint *anims = isPlayerRunning ? runsAnimation : idles;
        DrawTexture(anims[currentAnim]);

        V3f playerNorm = V3fNormalize(V3fSub(cameraPos, mouse));

        if (charScale.x < 0)
            charScale.x *= -1;

        V2f properMouse = {mouse.x - (f32)view.x / 2, view.y - mouse.y - (f32)view.y / 2};

        float angle = 0;

        if (properMouse.x == 0)
        {
            if (properMouse.y > 0)
                angle = PI / 2;
            else
                angle = -PI / 2;
        }
        else
        {
            angle = my_atan(properMouse.y / properMouse.x);
        }

        if (properMouse.x < 0)
        {
            angle = (angle * -1) - PI;
            charScale.y *= -1;
        }

        modelMatrix = RotateAroundZ(Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), playerPosMat), charScale), angle);
        glUniformMatrix4fv(modelMatrixLocation, 1, GL_TRUE, modelMatrix.values);
        DrawTexture(gun);

        for (int i = 0; i < ArrayLength(bullets); i++)
        {
            Bullet *bullet = &bullets[i];

            if (bullet->isAlive)
            {
                bullet->pos.x += bullet->direction.x * bullet->speed * frameSec;
                bullet->pos.y += bullet->direction.y * bullet->speed * frameSec;

                charScale = (V3f){PIXELS_PER_TEXEL * 40.0f, PIXELS_PER_TEXEL * 40.0f, 1.0f};
                float bulletAngle = my_atan(bullet->direction.y / bullet->direction.x);
                if (bullet->direction.x < 0)
                    bulletAngle -= PI;
                modelMatrix = RotateAroundZ(Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), bullets[i].pos), charScale), bulletAngle);
                glUniformMatrix4fv(modelMatrixLocation, 1, GL_TRUE, modelMatrix.values);

                DrawTexture(bulletTexture);
            }
        }

        UpdateSound();

        glUseProgram(pureProgram);
        glUniformMatrix4fv(projectionLocation2, 1, GL_TRUE, projection.values);

        int padding = 16;

        f32 C = ((f32)view.x - (f32)padding * 2) / SoundOutput.SecondaryBufferSize;

        for (int i = 0; i < ArrayLength(cursors); i++)
        {
            CursorInfo *info = &cursors[i];

            V3f playColor = {1.0f, 1.0f, 1.0f};
            f32 playX = info->play * C;

            V3f writeColor = {1.0f, 0.3f, 0.3f};
            f32 writeX = info->write * C;

            // Display play
            modelMatrix = Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), (V3f){playX, view.y / 2, 0}), (V3f){2, view.y - padding * 2, 1});
            glUniformMatrix4fv(modelMatrixLocation2, 1, GL_TRUE, modelMatrix.values);

            glUniform3f(colorLocation, playColor.r, playColor.g, playColor.b);

            glUniformMatrix4fv(modelMatrixLocation2, 1, GL_TRUE, modelMatrix.values);

            glBindVertexArray(vertexArray);
            glDrawArrays(GL_TRIANGLES, 0, VERTICES_IN_TEXTURE);

            // Display write
            modelMatrix = Mat4ScaleV3f(Mat4TranslateV3f(Mat4Identity(), (V3f){writeX, view.y / 2, 0}), (V3f){2, view.y - padding * 2, 1});
            glUniformMatrix4fv(modelMatrixLocation2, 1, GL_TRUE, modelMatrix.values);

            glUniform3f(colorLocation, writeColor.r, writeColor.g, writeColor.b);

            glUniformMatrix4fv(modelMatrixLocation2, 1, GL_TRUE, modelMatrix.values);

            glBindVertexArray(vertexArray);
            glDrawArrays(GL_TRIANGLES, 0, VERTICES_IN_TEXTURE);
        }

        SwapBuffers(dc);

        f += frameSec;

        // i32 frameMicroSec = (i32)((endTime - startTime) * 1000.0f * 1000.0f / (float)frequency);
        // char perfMessageBuffer[255];
        // Formati32(frameMicroSec, perfMessageBuffer);
        // OutputDebugStringA(perfMessageBuffer);

        startTime = endTime;
    }

    ExitProcess(0);
}
