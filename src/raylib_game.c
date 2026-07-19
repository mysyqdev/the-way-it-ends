/*******************************************************************************************
*
*   raylib gamejam template
*
*   Code licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2026 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>      // Emscripten library
#endif

#include <stdio.h>                          // Required for: printf()
#include <stdlib.h>                         // Required for: 
#include <string.h>                         // Required for:
#include <math.h>

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum { 
    SCREEN_TITLE, 
    SCREEN_GAMEPLAY, 
    SCREEN_ENDING
} GameScreen;

// TODO: Define your custom data types here
#define SKYCOLOR (Color){ 41, 173, 255, 255 }
#define GROUNDOLOR (Color){ 171, 82, 54, 255 }

typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    NONE
} Direction;

typedef struct Rabbit {
    Rectangle rec;
    Vector2 velocity;
    Texture2D image;
    bool isGrounded;
    Direction dir;
} Rabbit;

typedef struct Cloud {
    Rectangle rec;
    float velocity;
} Cloud;

typedef struct Phantom {
    Rectangle rec;
    float velocity;
    Texture2D image;
} Phantom;

typedef struct Ground {
    Rectangle rec;
    Texture2D image;
} Ground;

typedef struct Flower {
    Rectangle rec;
    Texture2D image;
    Vector2 velocity;
} Flower;

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------
static const int virtualWidth = 128;
static const int virtualHeight = 128;
static const int screenWidth = 768;
static const int screenHeight = 768;

static const int groundWidth = 24;

static RenderTexture2D target = { 0 };  // Render texture to render our game
static int frameCounter = 0;

// TODO: Define global variables here, recommended to make them static

static Rabbit rabbit = { 0 };

static const int g = 250;
static const float cloudsSpeed = 50.0f;
static const int rabbitSpeedX = 50;

static const int MAX_BOXES = 6;
static Rectangle boxes[] = {
    (Rectangle){ 29, 52, 24, 8 },
    (Rectangle){ 63, 82, 32, 8 },
    (Rectangle){ 37, 104, 19, 8 },
    (Rectangle){ 45, 134, 24, 8 },
    (Rectangle){ 30, 160, 19, 8 },
    (Rectangle){ 50, 200, 24, 8 },
};

static const int MAX_GROUNDS = 3;
static Ground leftGrounds[MAX_GROUNDS];
static Ground rightGrounds[MAX_GROUNDS];

static const int MAX_PHANTOMS = 4;
static Phantom phantoms[MAX_PHANTOMS];

static const int MAX_FLOWERS = 5;
static Flower flowers[MAX_FLOWERS];

static Rectangle lowestBox;
static Ground lowestGround;

static Cloud cloud = { 0 };

Camera2D camera = { 0 };

Music music;

GameScreen gameScreen = SCREEN_TITLE;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void);      // Update and Draw one frame
static void ApplyGravity(float);
static void ResolveRabbitCollision(float);
static void PlayerMovement(float);
static void RecycleBoxes(void);
static void UpdatePhantoms(float);
static void UpdateGrounds(void);
static void UpdateFlowers(float);
static void RestartGame(void);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messages
#endif

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "The Way It Ends");
    
    // TODO: Load resources / Initialize variables at this point
    rabbit.rec = (Rectangle){ 50, 0 , 7, 8};
    rabbit.velocity = (Vector2){ 0.0f, 0.0f };
    rabbit.isGrounded = false;
    rabbit.image = LoadTexture("resources/rabbit.png");
    rabbit.dir = NONE;

    Texture2D leftGroundImage = LoadTexture("resources/groundLeft1.png");
    Texture2D rightGroundImage = LoadTexture("resources/groundRight1.png");

    Texture2D flowerImage = LoadTexture("resources/flower.png");

    for (int i = 0; i < MAX_FLOWERS; i++)
    {
        Flower flower;
        flower.rec = (Rectangle){ GetRandomValue(groundWidth, virtualWidth - groundWidth - 3), virtualHeight + GetRandomValue(0, 50), 3, 3};
        flower.image = flowerImage;
        flower.velocity = (Vector2){ 0.0f, -30.0f };
        flowers[i] = flower;
    }

    for (int i = 0; i < MAX_GROUNDS; i++)
    {
        Ground leftGround;
        leftGround.rec = (Rectangle){0, -virtualHeight + (virtualHeight * i), groundWidth, virtualHeight};
        leftGround.image = leftGroundImage;
        leftGrounds[i] = leftGround;

        Ground rightGround;
        rightGround.rec = (Rectangle){virtualWidth - groundWidth, -virtualHeight + (virtualHeight * i), groundWidth, virtualHeight};
        rightGround.image = rightGroundImage;
        rightGrounds[i] = rightGround;
    }
    lowestGround = leftGrounds[MAX_GROUNDS - 1];

    lowestBox = boxes[MAX_BOXES - 1];

    cloud.rec = (Rectangle){groundWidth, -virtualHeight * 2, virtualWidth - (2 * groundWidth), virtualHeight + 10};
    cloud.velocity = cloudsSpeed;

    // init phantoms
    for (int i = 0; i < MAX_PHANTOMS; i++)
    {
        float velocity = -30.0f;
        Rectangle rec = (Rectangle){ GetRandomValue(groundWidth, virtualWidth - groundWidth), virtualHeight * 2 + GetRandomValue(0, 30), 8, 9};
        Phantom phantom = (Phantom){ rec, velocity };
        phantoms[i] = phantom;
    }

    camera.target = (Vector2){ virtualWidth/2.0f, rabbit.rec.y + rabbit.rec.height / 2 };
    camera.offset = (Vector2){ virtualWidth/2.0f, virtualHeight/2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    InitAudioDevice();
    music = LoadMusicStream("resources/bitterDays.mp3");
    PlayMusicStream(music);
    
    // Render texture to draw, enables screen scaling
    // NOTE: If screen is scaled, mouse input should be scaled proportionally
    target = LoadRenderTexture(virtualWidth, virtualHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60);     // Set our game frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button
    {
        UpdateDrawFrame();
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(target);
    
    // TODO: Unload all loaded resources at this point
    UnloadTexture(rabbit.image);
    UnloadTexture(leftGroundImage);
    UnloadTexture(rightGroundImage);
    UnloadMusicStream(music);

    CloseAudioDevice();

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------------
// Update and draw frame
void UpdateDrawFrame(void)
{
    // Update
    //----------------------------------------------------------------------------------
    // TODO: Update variables / Implement example logic at this point

    switch (gameScreen)
    {
        case SCREEN_TITLE:
            if (IsKeyDown(KEY_SPACE))
            {
                gameScreen = SCREEN_GAMEPLAY;
            }
            break;
        case SCREEN_GAMEPLAY:
            UpdateMusicStream(music);

            frameCounter++;
            float dt = GetFrameTime();

            cloud.rec.y += cloud.velocity * dt;
            UpdateFlowers(dt);

            PlayerMovement(dt);
            ApplyGravity(dt);
            
            switch (rabbit.dir)
            {
                case LEFT:
                    rabbit.velocity.x = -rabbitSpeedX;
                    break;
                case RIGHT:
                    rabbit.velocity.x = rabbitSpeedX;
                    break;
                case NONE:
                    rabbit.velocity.x = 0;
                    break;
                default:
                    break;
                
            }

            RecycleBoxes();

            UpdatePhantoms(dt);
            UpdateGrounds();

            ResolveRabbitCollision(dt);
            camera.target = (Vector2){virtualWidth/2.0f, rabbit.rec.y + rabbit.rec.height / 2 + 10};

            break;
        case SCREEN_ENDING:
            if (IsKeyDown(KEY_SPACE))
            {
                RestartGame();
                gameScreen = SCREEN_GAMEPLAY;
            }
            break;
    }

    // LOG("x: %f\n", rabbit.rec.x);
    // LOG("y: %f\n", rabbit.rec.y);
    // LOG("velocity x: %f\n", rabbit.velocity.x);
    // LOG("velocity y: %f\n", rabbit.velocity.y);
    // if (rabbit.isGrounded) LOG("RABBIT IS GROUNDED\n");
    // else LOG("RABBIT NOT GROUNDED\n");

    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    // Render game screen to a texture, 
    // it could be useful for scaling or further shader postprocessing
    BeginTextureMode(target);   
        ClearBackground(SKYCOLOR);

        BeginMode2D(camera);

        for (int i = 0; i < MAX_FLOWERS; i++)
        {
            DrawTexture(flowers[i].image , flowers[i].rec.x, flowers[i].rec.y, WHITE);
        }

        for (int i = 0; i < MAX_BOXES; i++)
        {
            DrawRectangleRec(boxes[i], GROUNDOLOR);
        }

        for (int i = 0; i < MAX_GROUNDS; i++)
        {
            DrawTexture(leftGrounds[i].image, leftGrounds[i].rec.x, leftGrounds[i].rec.y, WHITE);
            DrawTexture(rightGrounds[i].image, rightGrounds[i].rec.x, rightGrounds[i].rec.y, WHITE);
        }

        DrawTextureV(rabbit.image, (Vector2){ rabbit.rec.x, rabbit.rec.y }, WHITE);

        for (int i = 0; i < MAX_PHANTOMS; i++)
        {
            DrawRectangleRec(phantoms[i].rec, PURPLE);
        }

        DrawRectangleRec(cloud.rec, RAYWHITE);

        EndMode2D();

        DrawLine(groundWidth, 0, groundWidth, virtualHeight, BLACK);
        DrawLine(virtualWidth - groundWidth + 1, 0, virtualWidth - groundWidth + 1, virtualHeight, BLACK); // Why?
        
    EndTextureMode();
    
    // Render to screen (main framebuffer)
    BeginDrawing();
        ClearBackground(RAYWHITE);
        
        // Draw render texture to screen, scaled if required
        DrawTexturePro(target.texture, (Rectangle){ 0, 0, (float)target.texture.width, -(float)target.texture.height }, 
            (Rectangle){ 0, 0, (float)screenWidth, (float)screenHeight }, (Vector2){ 0, 0 }, 0.0f, WHITE);

        // TODO: Draw everything that requires to be drawn at this point, maybe UI?
        switch (gameScreen)
        {
            case SCREEN_TITLE:
                DrawText("THE WAY IT ENDS", 40, screenHeight / 2 - 100, 70, RAYWHITE);
                break;
            case SCREEN_GAMEPLAY:
                DrawText(TextFormat("%d", (int)rabbit.rec.y), 108 * 6, 4 * 6, 24, RAYWHITE);
                break;
            case SCREEN_ENDING:
                DrawText("THIS ISN'T WHERE", 40, screenHeight / 2 - 100, 70, RAYWHITE);
                DrawText("WE MEANT TO BE", 60, screenHeight / 2 - 20, 70, RAYWHITE);
        }

    EndDrawing();
    //----------------------------------------------------------------------------------  
}

void ApplyGravity(float dt)
{
    rabbit.velocity.y += g * dt;
}

void ResolveRabbitCollision(float dt)
{
    rabbit.rec.x += rabbit.velocity.x * dt;
    if (rabbit.rec.x < groundWidth) rabbit.rec.x = groundWidth;
    else if (rabbit.rec.x > virtualWidth - groundWidth - rabbit.rec.width) rabbit.rec.x = virtualWidth - groundWidth - rabbit.rec.width;

    for (int i = 0; i < MAX_BOXES; i++)
    {
        if (CheckCollisionRecs(rabbit.rec, boxes[i]))
        {
            LOG(" -- HORIZONTAL COLLISION --\n");
            if (rabbit.velocity.x > 0)
            {
                rabbit.rec.x = boxes[i].x - rabbit.rec.width;
            }
            else if (rabbit.velocity.x < 0)
            {
                rabbit.rec.x = boxes[i].x + boxes[i].width;
            }
            rabbit.velocity.x = 0;
        }
    }

    rabbit.rec.y += rabbit.velocity.y * dt;
    for (int i = 0; i < MAX_BOXES; i++)
    {
        if (CheckCollisionRecs(rabbit.rec, boxes[i]))
        {
            LOG(" -- VERTICAL COLLISION --\n");
            if (rabbit.velocity.y >= 0)
            {
                LOG("FALL DOWN\n");
                rabbit.isGrounded = true;
                rabbit.rec.y = boxes[i].y - rabbit.rec.height;
            }
            else if (rabbit.velocity.y < 0)
            {
                rabbit.rec.y = boxes[i].y + boxes[i].height;
            }
            rabbit.velocity.y = 0;

            break;
        }
        else
        {
            LOG(" -- NO VERTICAL COLLISION --\n");
            rabbit.isGrounded = false;
        }
    }

    if (CheckCollisionRecs(rabbit.rec, cloud.rec))
    {
        gameScreen = SCREEN_ENDING;
        StopMusicStream(music);
    }

    for (int i = 0; i < MAX_PHANTOMS; i++)
    {
        if (CheckCollisionRecs(rabbit.rec, phantoms[i].rec))
        {
            gameScreen = SCREEN_ENDING;
            StopMusicStream(music);
            break;
        }
    }
}

void PlayerMovement(float dt)
{
    rabbit.dir = NONE;

    if (IsKeyDown(KEY_A)) 
    {
        LOG("-- MOVE LEFT --\n");
        rabbit.dir = LEFT;
    }

    if (IsKeyDown(KEY_D)) 
    {
        LOG("-- MOVE RIGHT --\n");
        if (rabbit.dir == LEFT) rabbit.dir = NONE;
        else rabbit.dir = RIGHT;
    }

    if (IsKeyDown(KEY_SPACE) && rabbit.isGrounded) 
    {
        rabbit.velocity.y = -150;
        rabbit.isGrounded = false;
    }
}

void RecycleBoxes(void)
{
    for (int i = 0; i < MAX_BOXES; i++)
    {
        if (boxes[i].y <= camera.target.y - camera.offset.y - 50)
        {
            boxes[i].x = GetRandomValue(groundWidth, virtualWidth - groundWidth - boxes[i].width);
            boxes[i].y = lowestBox.y + lowestBox.height + GetRandomValue(10, 50);
            lowestBox = boxes[i];
        }
    }
}

void UpdatePhantoms(float dt)
{
    for (int i = 0; i < MAX_PHANTOMS; i++)
    {
        phantoms[i].rec.y += phantoms[i].velocity * dt;

        if (phantoms[i].rec.y <= camera.target.y - camera.offset.y - 50 )
        {
            phantoms[i].rec.x = GetRandomValue(groundWidth, virtualWidth - groundWidth - phantoms[i].rec.width);
            phantoms[i].rec.y = rabbit.rec.y + virtualHeight + GetRandomValue(0, 50);
            phantoms[i].velocity = -GetRandomValue(25, 35);
        }
    }
}

void UpdateGrounds(void)
{
    for (int i = 0; i < MAX_GROUNDS; i++)
    {
        if (camera.target.y - camera.offset.y - leftGrounds[i].rec.y >= virtualHeight * 2)
        {
            leftGrounds[i].rec.y = lowestGround.rec.y + lowestGround.rec.height;
            rightGrounds[i].rec.y = lowestGround.rec.y + lowestGround.rec.height;
            lowestGround = leftGrounds[i];
        }
    }
}

void UpdateFlowers(float dt)
{
    for (int i = 0; i < MAX_FLOWERS; i++)
    {
        flowers[i].velocity.y = -fmaxf(30.0f, rabbit.velocity.y / 2);
        flowers[i].velocity.x = GetRandomValue(-10, 10);
        flowers[i].rec.x += flowers[i].velocity.x * dt;
        flowers[i].rec.y += flowers[i].velocity.y * dt;

        if (camera.target.y - camera.offset.y - flowers[i].rec.y >= virtualHeight)
        {
            flowers[i].rec.x = GetRandomValue(groundWidth, virtualWidth - groundWidth - 3);
            flowers[i].rec.y = rabbit.rec.y + virtualHeight / 2 + GetRandomValue(30, 200);
        }
    }
}

void RestartGame(void)
{
    rabbit.rec = (Rectangle){ 50, 0 , 7, 8};
    rabbit.velocity = (Vector2){ 0.0f, 0.0f };
    rabbit.isGrounded = false;
    rabbit.dir = NONE;

    for (int i = 0; i < MAX_FLOWERS; i++)
    {
        flowers[i].rec = (Rectangle){ GetRandomValue(groundWidth, virtualWidth - groundWidth - 3), virtualHeight + GetRandomValue(0, 50), 3, 3};
        flowers[i].velocity = (Vector2){ 0.0f, -30.0f };
    }

    for (int i = 0; i < MAX_GROUNDS; i++)
    {
        leftGrounds[i].rec = (Rectangle){0, -virtualHeight + (virtualHeight * i), groundWidth, virtualHeight};
        rightGrounds[i].rec = (Rectangle){virtualWidth - groundWidth, -virtualHeight + (virtualHeight * i), groundWidth, virtualHeight};
    }
    lowestGround = leftGrounds[MAX_GROUNDS - 1];

    boxes[0] = (Rectangle){ 29, 52, 24, 8 };
    boxes[1] = (Rectangle){ 63, 82, 32, 8 };
    boxes[2] = (Rectangle){ 37, 104, 19, 8 };
    boxes[3] = (Rectangle){ 45, 134, 24, 8 };
    boxes[4] = (Rectangle){ 30, 160, 19, 8 };
    boxes[5] = (Rectangle){ 50, 200, 24, 8 };

    lowestBox = boxes[MAX_BOXES - 1];

    cloud.rec = (Rectangle){groundWidth, -virtualHeight * 2, virtualWidth - (2 * groundWidth), virtualHeight + 10};
    cloud.velocity = cloudsSpeed;

    for (int i = 0; i < MAX_PHANTOMS; i++)
    {
        phantoms[i].rec = (Rectangle){ GetRandomValue(groundWidth, virtualWidth - groundWidth - 8), virtualHeight * 2 + GetRandomValue(0, 30), 8, 9};
    }

    camera.target = (Vector2){ virtualWidth/2.0f, rabbit.rec.y + rabbit.rec.height / 2 };
    PlayMusicStream(music);
}