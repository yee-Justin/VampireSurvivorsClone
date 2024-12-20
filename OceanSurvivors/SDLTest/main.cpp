/**
* Author: Justin Yee
* Assignment: Ocean Survivors
* Date due: December 11, 2024, 2:00pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f


#define GLOBAL_LEVEL_WIDTH 20.0f
#define GLOBAL_LEVEL_HEIGHT 20.0f
#define LEVEL_LEFT_EDGE -3.0f
#define LEVEL_BOTTOM_EDGE -3.0f


#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL_mixer.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"
#include "Map.h"
#include "Utility.h"
#include "Scene.h"
#include "Start.h"
#include "LevelA.h"
#include "LevelB.h"
#include "LevelC.h"

// ————— CONSTANTS ————— //
constexpr int WINDOW_WIDTH = 640 * 2,
WINDOW_HEIGHT = 480 * 2;

constexpr float BG_RED = 0.0f,
BG_BLUE = .50f,
BG_GREEN = 1.0f,
BG_OPACITY = 1.0f;

constexpr int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

constexpr char GAME_WINDOW_NAME[] = "Ocean Survivors";

constexpr char HEALTH_FILEPATH[] = "assets/health.png", //From https://adwitr.itch.io/pixel-health-bar-asset-pack-2 (edited for color)
BOX_FILEPATH[] = "assets/bluebox.png", //just a blue box made in paint
TEXT_FILEPATH[] = "assets/font1.png",
BACKGROUND_FILEPATH[] = "assets/background.png", //From https://opengameart.org/content/water-textures
WIN_SFX_FILEPATH[] = "assets/win.wav", // From https://opengameart.org/content/win-music-3
LOSE_SFX_FILEPATH[] = "assets/lose.wav"; // From https://opengameart.org/content/game-over-trumpet-sfx


//stuff that is used for the entire game
GLuint text_texture_id;

Entity* health;
Entity* box;
Entity* background;

Mix_Chunk* win_sfx;
Mix_Chunk* lose_sfx;

bool win_sound = true;
bool lose_sound = true;


//level up stuff
int level = 1;
int levelup_requirement = 10;


//adding both fragment and lit glsls
constexpr char  V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl",
V_LIT_SHADER_PATH[] = "shaders/vertex_lit.glsl",
F_LIT_SHADER_PATH[] = "shaders/fragment_lit.glsl";

constexpr float MILLISECONDS_IN_SECOND = 1000.0;

constexpr int NUMBER_OF_TEXTURES = 1;
constexpr GLint LEVEL_OF_DETAIL = 0;
constexpr GLint TEXTURE_BORDER = 0;

// ————— VARIABLES ————— //
Scene* g_current_scene;

LevelA* g_level_a;
LevelB* g_level_b;
LevelC* g_level_c;

Start* g_start;

enum AppStatus { RUNNING, TERMINATED };

SDL_Window* g_display_window;

AppStatus g_app_status = RUNNING;
ShaderProgram g_shader_program, menu_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f,
g_accumulator = 0.0f;


//calculates mouse position
enum Coordinate { x_coordinate, y_coordinate };

const Coordinate X_COORDINATE = x_coordinate;
const Coordinate Y_COORDINATE = y_coordinate;

const float ORTHO_WIDTH = 90.0f,
ORTHO_HEIGHT = 120.0f;

float get_screen_to_ortho(float coordinate, Coordinate axis)
{
    switch (axis)
    {
    case x_coordinate: return ((coordinate / WINDOW_WIDTH) * ORTHO_WIDTH) - (ORTHO_WIDTH / 2.0);
    case y_coordinate: return (((WINDOW_HEIGHT - coordinate) / WINDOW_HEIGHT) * ORTHO_HEIGHT) - (ORTHO_HEIGHT / 2.0);
    default: return 0.0f;
    }
}

void initialise();
void process_input();
void update();
void render();
void shutdown();
void level_clear();

void switch_to_scene(Scene* scene)
{
    g_current_scene = scene;
    //if the current scene is level b or level c, we update player using previous level information
	if (g_current_scene == g_level_b || g_current_scene == g_level_c)
	{
		level_clear();
	}
    g_current_scene->initialise();
    g_current_scene->turn_on();
}

void initialise()
{
    // ————— GENERAL ————— //
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow(GAME_WINDOW_NAME,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);
    if (context == nullptr)
    {
        LOG("ERROR: Could not create OpenGL context.\n");
        shutdown();
    }

#ifdef _WINDOWS
    glewInit();
#endif

    // ————— VIDEO SETUP ————— //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);


    //setting up a shader for the game and for the menu
    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);
	menu_shader_program.load(V_LIT_SHADER_PATH, F_LIT_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-10.0f, 10.0f, -7.5f, 7.5f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);
	menu_shader_program.set_projection_matrix(g_projection_matrix);
	menu_shader_program.set_view_matrix(g_view_matrix);

	//starts as the menu shader
    glUseProgram(menu_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    //seeds random number generator
    srand((unsigned)time(0));

    // ————— Show Start ————— //
    g_start = new Start();
    switch_to_scene(g_start);

    //Load health and text and the box
    text_texture_id = Utility::load_texture(TEXT_FILEPATH);
    GLuint health_texture_id = Utility::load_texture(HEALTH_FILEPATH);
	GLuint box_texture_id = Utility::load_texture(BOX_FILEPATH);
    GLuint background_texture_id = Utility::load_texture(BACKGROUND_FILEPATH);

    health = new Entity(health_texture_id, 0.0f, 1, 1, HEALTH);
    box = new Entity(box_texture_id, 0.0f, 1, 1, OBJECT);
    background = new Entity(background_texture_id, 0.0f, 1, 1, OBJECT);


	//load sound effects
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
    win_sfx = Mix_LoadWAV(WIN_SFX_FILEPATH);
    lose_sfx = Mix_LoadWAV(LOSE_SFX_FILEPATH);

    // ————— BLENDING ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    //resets player position when the player actually exists and the level is on
    if (g_current_scene != g_start && g_current_scene->level_on)
    {
        g_current_scene->get_state().player->set_movement(glm::vec3(0.0f));
    }

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_app_status = TERMINATED;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) 
            {
            
            case SDLK_q:
                // Quit the game with a keystroke
                g_app_status = TERMINATED;
                break;
			
            case SDLK_RETURN:
				// Start the game with a keystroke
				if (g_current_scene == g_start)
				{
					g_level_a = new LevelA();
					switch_to_scene(g_level_a);
				}
				break;

            case SDLK_p:
				// Pause the game
				if (g_current_scene != g_start)
				{
					g_current_scene->turn_off();
				}
				break;
				
            case SDLK_SPACE:
				// Unpause the game
                if (g_current_scene != g_start && !g_current_scene->level_on && !g_current_scene->levelup)
                {
                    g_current_scene->turn_on();
                }
                break;

            case SDLK_l:
				// Level up
				if (g_current_scene != g_start)
				{
                    g_current_scene->get_state().player->add_exp(levelup_requirement);
				}
				break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);


    if (!g_current_scene->win)
    {
        //when the game is playing
        if (g_current_scene != g_start && g_current_scene->level_on)
        {
            //move left right up down
            if (key_state[SDL_SCANCODE_A])
            {
                g_current_scene->get_state().player->move_left();
            }
            if (key_state[SDL_SCANCODE_D])
            {
                g_current_scene->get_state().player->move_right();
            }
            if (key_state[SDL_SCANCODE_W])
            {
                g_current_scene->get_state().player->move_up();
            }
            if (key_state[SDL_SCANCODE_S])
            {
                g_current_scene->get_state().player->move_down();
            }

            //quickly moves to the next levels
            if (key_state[SDL_SCANCODE_K])
            {
                // Instant win
                if (g_current_scene != g_start)
                {
                    g_current_scene->kill();
                }
            }

            //normalize movement
            if (glm::length(g_current_scene->get_state().player->get_movement()) > 5.0f)
            {
                g_current_scene->get_state().player->normalise_movement();
            }
        }



        //level up options
        if (g_current_scene->levelup)
        {
            if (key_state[SDL_SCANCODE_1])
            {
                g_current_scene->get_state().player->levelup(1);
                g_current_scene->levelup = false;
                g_current_scene->turn_on();
            }
            if (key_state[SDL_SCANCODE_2])
            {
                g_current_scene->get_state().player->levelup(2);
                g_current_scene->levelup = false;
                g_current_scene->turn_on();
            }
            if (key_state[SDL_SCANCODE_3])
            {
                g_current_scene->get_state().player->levelup(3);
                g_current_scene->levelup = false;
                g_current_scene->turn_on();
            }
            if (key_state[SDL_SCANCODE_4])
            {
                g_current_scene->get_state().player->levelup(4);
                g_current_scene->levelup = false;
                g_current_scene->turn_on();
            }
            if (key_state[SDL_SCANCODE_5])
            {
                g_current_scene->get_state().player->levelup(5);
                g_current_scene->levelup = false;
                g_current_scene->turn_on();
            }
            if (key_state[SDL_SCANCODE_6])
            {
                g_current_scene->get_state().player->levelup(6);
                g_current_scene->levelup = false;
                g_current_scene->turn_on();
            }
        }
    }
}

void update()
{
    
    // ————— DELTA TIME / FIXED TIME STEP CALCULATION ————— //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    //the background is always updated
    background->set_position(glm::vec3(10.0f, -10.0f, 0.0f));
    background->set_scale(glm::vec3(40.0f, 40.0f, 1.0f));
	background->update(0, NULL, NULL, 0, g_current_scene->get_state().map);

    //updates the start screen when the current scene is start
	if (g_current_scene == g_start)
	{
		g_current_scene->update(delta_time);
	}

    //updates while the scene is on
    if (g_current_scene->level_on)
    {


        //game stuff
        if (g_current_scene != g_start )
        {

            // lose win sound effects
            if (g_current_scene->get_state().player->get_health() <= 0.0f && lose_sound)
            {
                Mix_PlayChannel(-1, lose_sfx, 0);
                lose_sound = false;
            }

            if (g_current_scene == g_level_c && g_current_scene->win && win_sound)
            {
                Mix_PlayChannel(-1, win_sfx, 0);
				win_sound = false;
            }


            //updating game
            delta_time += g_accumulator;

            if (delta_time < FIXED_TIMESTEP)
            {
                g_accumulator = delta_time;
                return;
            }

            //grabs a scaled up position of the mouse position
            int x, y;

            SDL_GetMouseState(&x, &y);

            if (!g_current_scene->get_state().bubble->get_is_active())
            {
                g_current_scene->mouse_position = glm::vec3(5 * get_screen_to_ortho(x, X_COORDINATE), 5 * get_screen_to_ortho(y, Y_COORDINATE), 0.0f);
            }

			// ————— FIXED TIMESTEP LOOP ————— //
            while (delta_time >= FIXED_TIMESTEP)
            {
                // ————— UPDATING THE SCENE (i.e. map, character, enemies...) ————— //
                g_current_scene->update(FIXED_TIMESTEP);

                //levelup logic
                if (g_current_scene->get_state().player->get_current_exp() >= levelup_requirement)
                {
                    levelup_requirement *= 1.25;
                    levelup_requirement += 1;
                    level++;
                    g_current_scene->get_state().player->set_current_exp(0);

                    g_current_scene->levelup = true;
                    g_current_scene->turn_off();
                }

				//level clear logic
                if (g_current_scene == g_level_a && g_level_a->clear)
                {
                    
                    g_level_b = new LevelB();
                    switch_to_scene(g_level_b);
                }

                if (g_current_scene == g_level_b && g_level_b->clear)
                {
                    
                    g_level_c = new LevelC();
                    switch_to_scene(g_level_c);
                }

                delta_time -= FIXED_TIMESTEP;
            }

            g_accumulator = delta_time;

            //setting health bar
            health->set_position(glm::vec3(g_current_scene->get_state().player->get_position().x - 10, g_current_scene->get_state().player->get_position().y + 5.4, 0));
            if (g_current_scene->get_state().player->get_health() > 0)
            {
                health->set_scale(glm::vec3(8.0f * g_current_scene->get_state().player->get_health(), 0.15f, 0.0f));
            }
            else
            {
                health->set_scale(glm::vec3(0.0f, 0.0f, 0.0f));
            }
            health->update(0, NULL, NULL, 0, g_current_scene->get_state().map);
            
			//sets box position and scale for when it can be rendered
            box->set_position(glm::vec3(g_current_scene->get_state().player->get_position().x, g_current_scene->get_state().player->get_position().y, 0));
            box->set_scale(glm::vec3(18.0f, 7.0f, 0.0f));
            box->update(0, NULL, NULL, 0, g_current_scene->get_state().map);

            // ————— PLAYER CAMERA ————— //
            g_view_matrix = glm::mat4(1.0f);
            g_view_matrix = glm::translate(g_view_matrix, glm::vec3(-g_current_scene->get_state().player->get_position().x, -g_current_scene->get_state().player->get_position().y, 0));
        }
    }

    //if game is not on, set delta_time to 0
    else
    {
        delta_time = 0;
        g_accumulator = 0;
        return;
    }
    
}

void render()
{
    //renders menu shaders and scene
    if (g_current_scene == g_start)
    {

        menu_shader_program.set_view_matrix(g_view_matrix);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(menu_shader_program.get_program_id());
        g_current_scene->render(&menu_shader_program);
    }

    //renders game shaders and scenes
    if (g_current_scene != g_start)
    {
        g_shader_program.set_view_matrix(g_view_matrix);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(g_shader_program.get_program_id());

        background->render(&g_shader_program);
        g_current_scene->render(&g_shader_program);

        //renders the scene when the game is playing
        //box will be rendered after the scene on levelup so everything else will be on top of it
        if (g_current_scene->levelup)
        {
            box->render(&g_shader_program);
        }

        //renders very basic UI
        health->render(&g_shader_program);

        std::string chealth = "HP: ";
        chealth += std::to_string(g_current_scene->get_state().player->get_current_health());
        chealth += "/";
        chealth += std::to_string(g_current_scene->get_state().player->get_max_health());

        std::string kills = "Kills: ";
        kills += std::to_string(g_current_scene->get_state().player->get_kills());

        std::string exp = "Level: ";
        exp += std::to_string(level);
        exp += "  EXP: ";
        exp += std::to_string(g_current_scene->get_state().player->get_current_exp());
        exp += "/";
        exp += std::to_string(levelup_requirement);

        Utility::draw_text(&g_shader_program, text_texture_id, chealth,
            .5, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 9.4, g_current_scene->get_state().player->get_position().y + 6.4, 0));

        Utility::draw_text(&g_shader_program, text_texture_id, kills,
            .5, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 9.4, g_current_scene->get_state().player->get_position().y + 4.4, 0));

        Utility::draw_text(&g_shader_program, text_texture_id, exp,
            .5, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 2.4, g_current_scene->get_state().player->get_position().y + 6.4, 0));

        //renders a lose message when the players health reaches 0
        if (g_current_scene->get_state().player->get_health() <= 0.0f)
        {
            Utility::draw_text(&g_shader_program, text_texture_id, "You Lose :(",
                .75, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 3, g_current_scene->get_state().player->get_position().y + 1, 0.0f));
        }

        //renders a win message when the final stage is cleared
        if (g_current_scene == g_level_c && g_current_scene->win)
        {
            Utility::draw_text(&g_shader_program, text_texture_id, "You WIN!!!!!!!",
                .75, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 3, g_current_scene->get_state().player->get_position().y + 1, 0.0f));
        }

        //displays basic levelup ui
        if (g_current_scene->levelup)
        {
            Utility::draw_text(&g_shader_program, text_texture_id, "LEVEL UP",
                .75, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 2.75, g_current_scene->get_state().player->get_position().y + 3, 0.0f));
            Utility::draw_text(&g_shader_program, text_texture_id, "1: Health Boost",
                .45, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 7, g_current_scene->get_state().player->get_position().y + 1.5, 0.0f));
            Utility::draw_text(&g_shader_program, text_texture_id, "2: Damage Boost",
                .45, .01, glm::vec3(g_current_scene->get_state().player->get_position().x + 1, g_current_scene->get_state().player->get_position().y + 1.5, 0.0f));
            Utility::draw_text(&g_shader_program, text_texture_id, "3: Speed Boost",
                .45, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 7, g_current_scene->get_state().player->get_position().y - .5, 0.0f));
            Utility::draw_text(&g_shader_program, text_texture_id, "4: Pierce Boost",
                .45, .01, glm::vec3(g_current_scene->get_state().player->get_position().x + 1, g_current_scene->get_state().player->get_position().y - .5, 0.0f));
            Utility::draw_text(&g_shader_program, text_texture_id, "5: Range Boost",
                .45, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 7, g_current_scene->get_state().player->get_position().y - 2.5, 0.0f));
            Utility::draw_text(&g_shader_program, text_texture_id, "6: Firerate Boost",
                .45, .01, glm::vec3(g_current_scene->get_state().player->get_position().x + 1, g_current_scene->get_state().player->get_position().y - 2.5, 0.0f));
        }

        //renders pause message
        if (g_current_scene->get_state().player->get_paused() && !g_current_scene->win && !g_current_scene->levelup)
        {
            Utility::draw_text(&g_shader_program, text_texture_id, "PAUSED",
                .75, .01, glm::vec3(g_current_scene->get_state().player->get_position().x - 2, g_current_scene->get_state().player->get_position().y + 1, 0.0f));
        }
    }
    SDL_GL_SwapWindow(g_display_window);
}
void level_clear()
{
    //saves stats for the next scene from the previous
    if (g_current_scene == g_level_b)
    {
        g_level_b->max_health = g_level_a->get_state().player->get_max_health();
        g_level_b->current_health = g_level_a->get_state().player->get_current_health();
        g_level_b->damage = g_level_a->get_state().player->get_damage();
        g_level_b->speed = g_level_a->get_state().player->get_speed();
        g_level_b->pierce = g_level_a->get_state().player->get_pierce();
        g_level_b->projectile_speed = g_level_a->get_state().player->get_projectile_speed();
        g_level_b->projectile_rate = g_level_a->get_state().player->get_rate();
        g_level_b->projectile_timer = g_level_a->get_state().player->get_timer();
        g_level_b->projectile_time = g_level_a->get_state().player->get_projectile_time();
        g_level_b->current_exp = g_level_a->get_state().player->get_current_exp();
    }

    if (g_current_scene == g_level_c)
    {
        g_level_c->max_health = g_level_b->get_state().player->get_max_health();
        g_level_c->current_health = g_level_b->get_state().player->get_current_health();
        g_level_c->damage = g_level_b->get_state().player->get_damage();
        g_level_c->speed = g_level_b->get_state().player->get_speed();
        g_level_c->pierce = g_level_b->get_state().player->get_pierce();
        g_level_c->projectile_speed = g_level_b->get_state().player->get_projectile_speed();
        g_level_c->projectile_rate = g_level_b->get_state().player->get_rate();
        g_level_c->projectile_timer = g_level_b->get_state().player->get_timer();
        g_level_c->projectile_time = g_level_b->get_state().player->get_timer();
        g_level_c->current_exp = g_level_b->get_state().player->get_current_exp();
    }
}
void shutdown()
{
    SDL_Quit();

	delete g_level_a;
    delete g_level_b;
    delete g_level_c;
    delete g_start;
    delete health;
    delete box;
    Mix_FreeChunk(win_sfx);
    Mix_FreeChunk(lose_sfx);
}

// ————— GAME LOOP ————— //
int main(int argc, char* argv[])
{
    initialise();

    while (g_app_status == RUNNING)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}
