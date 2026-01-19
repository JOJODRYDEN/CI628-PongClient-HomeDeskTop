#include "SDL_net.h"
#include "MyGame.h"

using namespace std;

const char* IP_NAME = "localhost";
const Uint16 PORT = 55555;

bool is_running = true;

MyGame* game = nullptr;

static int on_receive(void* socket_ptr) {
    TCPsocket socket = (TCPsocket)socket_ptr;

    const int message_length = 1024;

    char message[message_length];
    int received;

    do {
        received = SDLNet_TCP_Recv(socket, message, message_length);
        message[received] = '\0';

        char* pch = strtok(message, ",");

        string cmd(pch);

        vector<string> args;

        while (pch != NULL) {
            pch = strtok(NULL, ",");

            if (pch != NULL) {
                args.push_back(string(pch));
            }
        }

        game->on_receive(cmd, args);

        if (cmd == "exit") {
            break;
        }

    } while (received > 0 && is_running);

    return 0;
}

static int on_send(void* socket_ptr) {
    TCPsocket socket = (TCPsocket)socket_ptr;

    while (is_running) {
        if (game->messages.size() > 0) {
            for (auto m : game->messages) {
                string message;

                //Check if this is a special command that should be sent directly
                if (m.substr(0, 9) == "JOIN_ROOM" ||
                    m.substr(0, 18) == "PLAYER_CURRENT_POS" ||
                    m.substr(0, 12) == "BUILD_CASTLE" ||
                    m.substr(0, 15) == "BUILD_GOLD_MINE" ||
                    m.substr(0, 14) == "BUILD_BARRACKS" ||
                    m.substr(0, 7) == "RETREAT") {
                    message = m;
                }
                else {
                   
                    message = "CLIENT_DATA," + m;
                }

                cout << "Sending_TCP: " << message << endl;
                SDLNet_TCP_Send(socket, message.c_str(), message.length());
            }

            game->messages.clear();
        }

        SDL_Delay(1);
    }

    return 0;
}

void loop(SDL_Renderer* renderer) {
    SDL_Event event;

    Uint32 lastTime = SDL_GetTicks();
    float deltaTime = 0.0f;

    while (is_running) {
        Uint32 currentTime = SDL_GetTicks();
        deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        if (deltaTime > 0.05f) {
            deltaTime = 0.05f;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    is_running = false;
                    break;

                default:
                    break;
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN) {
                game->input(event);
            }

            if (event.type == SDL_QUIT) {
                is_running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        game->update(deltaTime);

        game->render(renderer);

        SDL_RenderPresent(renderer);
    }
}

int run_game() {
    std::string windowTitle = "RTS Client - Lobby System";

    SDL_Window* window = SDL_CreateWindow(
        windowTitle.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    if (nullptr == window) {
        std::cout << "Failed to create window" << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (nullptr == renderer) {
        std::cout << "Failed to create renderer" << SDL_GetError() << std::endl;
        return -1;
    }

    loop(renderer);

    return 0;
}

int main(int argc, char** argv) {

    std::cout << "==================================" << std::endl;
    std::cout << "Starting RTS Client - Lobby System" << std::endl;
    std::cout << "==================================" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO) == -1) {
        printf("SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    if (SDLNet_Init() == -1) {
        printf("SDLNet_Init: %s\n", SDLNet_GetError());
        exit(2);
    }

    game = new MyGame(1);  // Player number will be assigned by server

    game->initialize();

    IPaddress ip;

    if (SDLNet_ResolveHost(&ip, IP_NAME, PORT) == -1) {
        printf("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
        exit(3);
    }

    TCPsocket socket = SDLNet_TCP_Open(&ip);

    if (!socket) {
        printf("SDLNet_TCP_Open: %s\n", SDLNet_GetError());
        exit(4);
    }

    SDL_CreateThread(on_receive, "ConnectionReceiveThread", (void*)socket);
    SDL_CreateThread(on_send, "ConnectionSendThread", (void*)socket);

    run_game();

    delete game;

    SDLNet_TCP_Close(socket);

    SDLNet_Quit();

    SDL_Quit();

    return 0;
}