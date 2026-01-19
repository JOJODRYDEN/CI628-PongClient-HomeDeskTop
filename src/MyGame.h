#ifndef __MY_GAME_H__
#define __MY_GAME_H__

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <bitset>

#include "SDL.h"

struct Point {
    int x, y;
    Point(int x = 0, int y = 0) : x(x), y(y) {}
};

struct Site {
    Point center;
    SDL_Color neutralColor;
    bool hasCastle;
    bool hasGoldMine;
    bool hasBarracks;

    Site(int x, int y);
};

struct Player {
    Point position;
    Point targetPosition;
    int playerNumber;
    bool isMoving;
    int currentSite;
    bool isCapturing;
    float captureProgress;

    Player(int num = 1) : position(400, 300), targetPosition(400, 300),
        playerNumber(num), isMoving(false), currentSite(-1),
        isCapturing(false), captureProgress(0.0f) {
    }
};

struct GameData {
    std::vector<Site> sites;

    uint8_t player1Ownership;
    uint8_t player2Ownership;

    int player1Score;
    int player2Score;

    int player1Gold;
    int player1Levies;
    int player2Gold;
    int player2Levies;

    Player player1;
    Player player2;

    bool inCombat;
    int combatSite;
    float combatTimer;
    bool canRetreat;

    bool gameOver;      
    int winner;

    GameData() : player1Ownership(0), player2Ownership(0),
        player1Score(0), player2Score(0),
        player1Gold(0), player1Levies(0),
        player2Gold(0), player2Levies(0),
        player1(1), player2(2),
        inCombat(false), combatSite(-1), combatTimer(0.0f), canRetreat(false) {
    }

    bool isPlayer1Owner(int siteIndex) const {
        return (player1Ownership & (1 << siteIndex)) != 0;
    }

    bool isPlayer2Owner(int siteIndex) const {
        return (player2Ownership & (1 << siteIndex)) != 0;
    }

    bool isNeutral(int siteIndex) const {
        return !isPlayer1Owner(siteIndex) && !isPlayer2Owner(siteIndex);
    }

    void setPlayer1Owner(int siteIndex, bool owned) {
        if (owned) {
            player1Ownership |= (1 << siteIndex);
            player2Ownership &= ~(1 << siteIndex);
        }
        else {
            player1Ownership &= ~(1 << siteIndex);
        }
    }

    void setPlayer2Owner(int siteIndex, bool owned) {
        if (owned) {
            player2Ownership |= (1 << siteIndex);
            player1Ownership &= ~(1 << siteIndex);
        }
        else {
            player2Ownership &= ~(1 << siteIndex);
        }
    }

    void setNeutral(int siteIndex) {
        player1Ownership &= ~(1 << siteIndex);
        player2Ownership &= ~(1 << siteIndex);
    }
};

extern GameData game_data;

enum GameState {
    LOBBY,
    WAITING,
    PLAYING
};

class MyGame {

private:
    const int SCREEN_WIDTH = 800;
    const int SCREEN_HEIGHT = 600;

    float deltaTime;
    int myPlayerNumber;
    GameState gameState;
    int selectedRoom;
    int roomPlayerCounts[3];

    float distance(int x1, int y1, int x2, int y2);
    int findClosestSite(int x, int y);
    void renderPlayer(SDL_Renderer* renderer, Player& player);
    void renderUI(SDL_Renderer* renderer);
    void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, int size);
    void renderLobby(SDL_Renderer* renderer);
    void renderWaiting(SDL_Renderer* renderer);
    //void sendSitePositions(); left over from when host initilized site positions (report). 
    void renderBuildMenu(SDL_Renderer* renderer, int siteIndex);
    void renderCaptureBar(SDL_Renderer* renderer, Player& player);
    void renderCombatUI(SDL_Renderer* renderer);
    void renderGameOver(SDL_Renderer* renderer);
    bool isPlayerOnSite(int siteIndex);

    SDL_Color getSiteColor(int siteIndex);

public:
    std::vector<std::string> messages;

    MyGame(int playerNum = 1) : myPlayerNumber(playerNum), gameState(LOBBY), selectedRoom(-1) {
        roomPlayerCounts[0] = 0;
        roomPlayerCounts[1] = 0;
        roomPlayerCounts[2] = 0;
    }

    void initialize();
    void on_receive(std::string cmd, std::vector<std::string>& args);
    void send(std::string message);
    void input(SDL_Event& event);
    void update(float dt);
    void render(SDL_Renderer* renderer);
    int getPlayerNumber() const { return myPlayerNumber; }
};

#endif