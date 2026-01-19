#include "MyGame.h"

GameData game_data;

Site::Site(int x, int y) : center(x, y), hasCastle(false), hasGoldMine(false), hasBarracks(false) {
    neutralColor = {
        static_cast<Uint8>(100 + (x % 155)),
        static_cast<Uint8>(100 + (y % 155)),
        static_cast<Uint8>(100 + ((x + y) % 155)),
        255
    };
}

float MyGame::distance(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

int MyGame::findClosestSite(int x, int y) {
    if (game_data.sites.size() != 8) {
        return 0;
    }

    int closest = 0;
    float minDist = std::numeric_limits<float>::max();

    for (int i = 0; i < 8; i++) {
        float dist = distance(x, y, game_data.sites[i].center.x, game_data.sites[i].center.y);
        if (dist < minDist) {
            minDist = dist;
            closest = i;
        }
    }

    return closest;
}

SDL_Color MyGame::getSiteColor(int siteIndex) {
    if (game_data.isPlayer1Owner(siteIndex)) {
        return { 100, 100, 255, 180 };
    }
    else if (game_data.isPlayer2Owner(siteIndex)) {
        return { 255, 100, 100, 180 };
    }
    else {
        SDL_Color neutralColor = game_data.sites[siteIndex].neutralColor;
        neutralColor.a = 180;
        return neutralColor;
    }
}

void MyGame::initialize() {
    deltaTime = 0.016f;
    gameState = LOBBY;
    selectedRoom = -1;

    std::cout << "========================================" << std::endl;
    std::cout << "CLIENT INITIALIZATION" << std::endl;
    std::cout << "This client is Player: " << myPlayerNumber << std::endl;
    std::cout << "========================================" << std::endl;

    game_data.sites.clear();
    game_data.player1Ownership = 0;
    game_data.player2Ownership = 0;

    game_data.player1Gold = 0;
    game_data.player1Levies = 0;
    game_data.player2Gold = 0;
    game_data.player2Levies = 0;

    game_data.inCombat = false;
    game_data.combatSite = -1;
    game_data.combatTimer = 0.0f;
    game_data.canRetreat = false;

    game_data.player1.position = Point(0, 0);
    game_data.player1.targetPosition = Point(0, 0);
    game_data.player2.position = Point(0, 0);
    game_data.player2.targetPosition = Point(0, 0);

    game_data.gameOver = false;
    game_data.winner = 0;

    std::cout << "Waiting for lobby information from server......" << std::endl;
}


void MyGame::on_receive(std::string cmd, std::vector<std::string>& args) {
    std::cout << "CLIENT RECEIVED: " << cmd << " with " << args.size() << " args" << std::endl;

    if (cmd == "LOBBY_INFO") {
        if (args.size() == 3) {
            roomPlayerCounts[0] = stoi(args.at(0));
            roomPlayerCounts[1] = stoi(args.at(1));
            roomPlayerCounts[2] = stoi(args.at(2));
            std::cout << "=== Received Lobby Info ===" << std::endl;
            std::cout << "Room 1: " << roomPlayerCounts[0] << "/2 players" << std::endl;
            std::cout << "Room 2: " << roomPlayerCounts[1] << "/2 players" << std::endl;
            std::cout << "Room 3: " << roomPlayerCounts[2] << "/2 players" << std::endl;
        }
    }
    else if (cmd == "JOINED_ROOM") {
        if (args.size() >= 2) {
            int roomIndex = stoi(args.at(0));
            myPlayerNumber = stoi(args.at(1));
            gameState = WAITING;
            std::cout << "=== Joined Room " << (roomIndex + 1) << " as Player " << myPlayerNumber << " ===" << std::endl;
            std::cout << "Game state set to WAITING" << std::endl;
            std::cout << "Waiting for opponent..." << std::endl;
        }
    }
    else if (cmd == "ROOM_FULL") {
        if (args.size() >= 1) {
            int roomIndex = stoi(args.at(0));
            std::cout << "Room " << (roomIndex + 1) << " is full!" << std::endl;
            gameState = LOBBY;
        }
    }
    else if (cmd == "GAME_START") {
        gameState = PLAYING;
        std::cout << "=== GAME STARTING ===" << std::endl;
        std::cout << "Game state set to PLAYING" << std::endl;
    }
    else if (cmd == "SITE_POSITIONS") {
        if (args.size() == 16) {
            std::cout << "=== Receiving Site Positions from Server ===" << std::endl;

            game_data.sites.clear();

            for (int i = 0; i < 8; i++) {
                int x = stoi(args.at(i * 2));
                int y = stoi(args.at(i * 2 + 1));
                game_data.sites.push_back(Site(x, y));
                std::cout << "Site " << i << ": (" << x << ", " << y << ")" << std::endl;
            }

            game_data.player1.position = game_data.sites[0].center;
            game_data.player1.targetPosition = game_data.sites[0].center;
            game_data.player2.position = game_data.sites[7].center;
            game_data.player2.targetPosition = game_data.sites[7].center;

            std::cout << "Site positions synchronized with server" << std::endl;
        }
    }
    else if (cmd == "OWNERSHIP") {
        if (args.size() >= 2) {
            try {
                uint8_t oldP1 = game_data.player1Ownership;
                uint8_t oldP2 = game_data.player2Ownership;

                game_data.player1Ownership = static_cast<uint8_t>(std::stoi(args.at(0)));
                game_data.player2Ownership = static_cast<uint8_t>(std::stoi(args.at(1)));

                if (oldP1 != game_data.player1Ownership || oldP2 != game_data.player2Ownership) {
                    std::cout << "=== OWNERSHIP UPDATE ===" << std::endl;
                    std::cout << "Player 1 ownership: " << std::bitset<8>(game_data.player1Ownership) << std::endl;
                    std::cout << "Player 2 ownership: " << std::bitset<8>(game_data.player2Ownership) << std::endl;

                    for (int i = 0; i < 8; i++) {
                        bool wasP1 = (oldP1 & (1 << i)) != 0;
                        bool wasP2 = (oldP2 & (1 << i)) != 0;
                        bool isP1 = game_data.isPlayer1Owner(i);
                        bool isP2 = game_data.isPlayer2Owner(i);

                        if (wasP1 != isP1 || wasP2 != isP2) {
                            std::cout << "Site " << i << " changed: ";
                            if (wasP1) std::cout << "P1";
                            else if (wasP2) std::cout << "P2";
                            else std::cout << "Neutral";
                            std::cout << " -> ";
                            if (isP1) std::cout << "P1";
                            else if (isP2) std::cout << "P2";
                            else std::cout << "Neutral";
                            std::cout << std::endl;
                        }
                    }
                    std::cout << "=======================" << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing OWNERSHIP: " << e.what() << std::endl;
            }
        }
    }
    else if (cmd == "SCORES") {
        if (args.size() == 2) {
            game_data.player1Score = stoi(args.at(0));
            game_data.player2Score = stoi(args.at(1));
        }
    }
    else if (cmd == "RESOURCES") {
        if (args.size() >= 4) {
            game_data.player1Gold = stoi(args.at(0));
            game_data.player1Levies = stoi(args.at(1));
            game_data.player2Gold = stoi(args.at(2));
            game_data.player2Levies = stoi(args.at(3));
        }
    }
    else if (cmd == "PLAYER_POS") {
        if (args.size() >= 3) {
            int playerNum = stoi(args.at(0));
            int x = stoi(args.at(1));
            int y = stoi(args.at(2));

            //Only update opponent's target, let client interpolate smoothly
            if (playerNum != myPlayerNumber) {
                if (playerNum == 1) {
                    game_data.player1.targetPosition.x = x;
                    game_data.player1.targetPosition.y = y;
                    game_data.player1.isMoving = true;
                }
                else if (playerNum == 2) {
                    game_data.player2.targetPosition.x = x;
                    game_data.player2.targetPosition.y = y;
                    game_data.player2.isMoving = true;
                }
            }
            //For our own player, update target when server confirms (reconciliation)
            else {
                if (playerNum == 1) {
                    //Server says we've arrived, which snaps to exact position (might need to tweek later)
                    game_data.player1.position.x = x;
                    game_data.player1.position.y = y;
                    game_data.player1.targetPosition.x = x;
                    game_data.player1.targetPosition.y = y;
                    game_data.player1.isMoving = false;
                }
                else if (playerNum == 2) {
                    game_data.player2.position.x = x;
                    game_data.player2.position.y = y;
                    game_data.player2.targetPosition.x = x;
                    game_data.player2.targetPosition.y = y;
                    game_data.player2.isMoving = false;
                }
            }
        }
    }
    else if (cmd == "BUILDINGS") {
        if (args.size() >= 3) {
            try {
                uint8_t castles = static_cast<uint8_t>(std::stoi(args.at(0)));
                uint8_t goldMines = static_cast<uint8_t>(std::stoi(args.at(1)));
                uint8_t barracks = static_cast<uint8_t>(std::stoi(args.at(2)));

                std::cout << "=== BUILDINGS UPDATE ===" << std::endl;
                for (int i = 0; i < 8; i++) {
                    game_data.sites[i].hasCastle = (castles & (1 << i)) != 0;
                    game_data.sites[i].hasGoldMine = (goldMines & (1 << i)) != 0;
                    game_data.sites[i].hasBarracks = (barracks & (1 << i)) != 0;

                    if (game_data.sites[i].hasCastle || game_data.sites[i].hasGoldMine || game_data.sites[i].hasBarracks) {
                        std::cout << "Site " << i << ": ";
                        if (game_data.sites[i].hasCastle) std::cout << "Castle ";
                        if (game_data.sites[i].hasGoldMine) std::cout << "GoldMine ";
                        if (game_data.sites[i].hasBarracks) std::cout << "Barracks";
                        std::cout << std::endl;
                    }
                }
                std::cout << "========================" << std::endl;
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing BUILDINGS: " << e.what() << std::endl;
            }
        }
    }
    else if (cmd == "PLAYER_STATES") {
        if (args.size() >= 1) {
            try {
                uint8_t states = static_cast<uint8_t>(std::stoi(args.at(0)));

                game_data.player1.isMoving = (states & (1 << 0)) != 0;
                game_data.player2.isMoving = (states & (1 << 1)) != 0;
                game_data.player1.isCapturing = (states & (1 << 2)) != 0;
                game_data.player2.isCapturing = (states & (1 << 3)) != 0;
                game_data.inCombat = (states & (1 << 4)) != 0;
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing PLAYER_STATES: " << e.what() << std::endl;
            }
        }
    }
    else if (cmd == "COMBAT_STATE") {
        if (args.size() >= 2) {
            try {
                uint8_t state = static_cast<uint8_t>(std::stoi(args.at(0)));
                game_data.combatTimer = std::stof(args.at(1));

                game_data.inCombat = (state & (1 << 3)) != 0;
                if (game_data.inCombat) {
                    game_data.combatSite = state & 0x07;  //Extract bits 0-2
                    game_data.canRetreat = (state & (1 << 4)) != 0;
                }
                else {
                    game_data.combatSite = -1;
                    game_data.canRetreat = false;
                }
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing COMBAT_STATE: " << e.what() << std::endl;
            }
        }
    }
    else if (cmd == "FULL_STATE") {
        if (args.size() >= 18) {
            try {
                //Ownership (indices 0-1)
                game_data.player1Ownership = static_cast<uint8_t>(std::stoi(args.at(0)));
                game_data.player2Ownership = static_cast<uint8_t>(std::stoi(args.at(1)));

                //Buildings (indices 2-4)
                uint8_t castles = static_cast<uint8_t>(std::stoi(args.at(2)));
                uint8_t goldMines = static_cast<uint8_t>(std::stoi(args.at(3)));
                uint8_t barracks = static_cast<uint8_t>(std::stoi(args.at(4)));

                for (int i = 0; i < 8; i++) {
                    game_data.sites[i].hasCastle = (castles & (1 << i)) != 0;
                    game_data.sites[i].hasGoldMine = (goldMines & (1 << i)) != 0;
                    game_data.sites[i].hasBarracks = (barracks & (1 << i)) != 0;
                }

                //Player states (index 5)
                uint8_t states = static_cast<uint8_t>(std::stoi(args.at(5)));
                game_data.player1.isMoving = (states & (1 << 0)) != 0;
                game_data.player2.isMoving = (states & (1 << 1)) != 0;
                game_data.player1.isCapturing = (states & (1 << 2)) != 0;
                game_data.player2.isCapturing = (states & (1 << 3)) != 0;
                game_data.inCombat = (states & (1 << 4)) != 0;

                //Scores (indices 6-7)
                game_data.player1Score = stoi(args.at(6));
                game_data.player2Score = stoi(args.at(7));

                //Resources (indices 8-11)
                game_data.player1Gold = stoi(args.at(8));
                game_data.player1Levies = stoi(args.at(9));
                game_data.player2Gold = stoi(args.at(10));
                game_data.player2Levies = stoi(args.at(11));

                //Positions (indices 12-15)
                game_data.player1.targetPosition.x = stoi(args.at(12));
                game_data.player1.targetPosition.y = stoi(args.at(13));
                game_data.player2.targetPosition.x = stoi(args.at(14));
                game_data.player2.targetPosition.y = stoi(args.at(15));

                //Combat state (indices 16-17)
                uint8_t combatState = static_cast<uint8_t>(std::stoi(args.at(16)));
                game_data.combatTimer = std::stof(args.at(17));

                game_data.inCombat = (combatState & (1 << 3)) != 0;
                if (game_data.inCombat) {
                    game_data.combatSite = combatState & 0x07;
                    game_data.canRetreat = (combatState & (1 << 4)) != 0;
                }

                std::cout << "=== FULL STATE RECEIVED ===" << std::endl;
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing FULL_STATE: " << e.what() << std::endl;
            }
        }
    }
    else if (cmd == "GAME_OVER") {
        if (args.size() >= 1) {
            game_data.gameOver = true;
            game_data.winner = stoi(args.at(0));
            std::cout << "=== GAME OVER - Player " << game_data.winner << " wins ===" << std::endl;
        }
    }
    else if (cmd == "COMBAT_START") {
        if (args.size() >= 1) {
            try {
                game_data.combatSite = std::stoi(args.at(0));
                game_data.inCombat = true;
                game_data.combatTimer = 0.0f;
                game_data.canRetreat = false;
                std::cout << "=== COMBAT STARTED at site " << game_data.combatSite << " ===" << std::endl;
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing COMBAT_START: " << e.what() << std::endl;
            }
        }
    }
    else if (cmd == "COMBAT_INTERRUPT") {
        game_data.inCombat = false;
        game_data.combatSite = -1;
        game_data.combatTimer = 0.0f;
        game_data.canRetreat = false;
        std::cout << "=== COMBAT INTERRUPTED ===" << std::endl;
    }
    else if (cmd == "COMBAT_END") {
        if (args.size() >= 1) {
            try {
                int winner = std::stoi(args.at(0));
                game_data.inCombat = false;
                game_data.combatSite = -1;
                game_data.combatTimer = 0.0f;
                game_data.canRetreat = false;
                std::cout << "=== COMBAT ENDED - Player " << winner << " victorious ===" << std::endl;
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing COMBAT_END: " << e.what() << std::endl;
            }
        }
    }
    else if (cmd == "RETREAT") {
        if (args.size() >= 2) {
            try {
                int playerNum = std::stoi(args.at(0));
                int retreatSite = std::stoi(args.at(1));
                std::cout << "=== Player " << playerNum << " retreated to site " << retreatSite << " ===" << std::endl;
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing RETREAT: " << e.what() << std::endl;
            }
        }
    }
    else if (cmd == "POSITIONS") {
        if (args.size() >= 4) {
            try {
                int serverP1X = std::stoi(args.at(0));
                int serverP1Y = std::stoi(args.at(1));
                int serverP2X = std::stoi(args.at(2));
                int serverP2Y = std::stoi(args.at(3));

                //Server reconciliation, only correct if significantly off and not moving
                const float RECONCILIATION_THRESHOLD = 50.0f;  // Increased threshold

                //Only reconcile Player 1 if we're not the one controlling them or if really far off
                if (myPlayerNumber != 1 || !game_data.player1.isMoving) {
                    float p1Diff = distance(game_data.player1.position.x, game_data.player1.position.y,
                        serverP1X, serverP1Y);
                    if (p1Diff > RECONCILIATION_THRESHOLD) {
                        game_data.player1.position.x = serverP1X;
                        game_data.player1.position.y = serverP1Y;
                        std::cout << "[RECONCILIATION] P1 position corrected by server (diff: " << p1Diff << ")" << std::endl;
                    }
                }

                //Same thing but for player 2
                if (myPlayerNumber != 2 || !game_data.player2.isMoving) {
                    float p2Diff = distance(game_data.player2.position.x, game_data.player2.position.y,
                        serverP2X, serverP2Y);
                    if (p2Diff > RECONCILIATION_THRESHOLD) {
                        game_data.player2.position.x = serverP2X;
                        game_data.player2.position.y = serverP2Y;
                        std::cout << "[RECONCILIATION] P2 position corrected by server (diff: " << p2Diff << ")" << std::endl;
                    }
                }
            }
            catch (const std::exception& e) {
                std::cout << "ERROR parsing POSITIONS: " << e.what() << std::endl;
            }
        }
    }
}

void MyGame::send(std::string message) {
    messages.push_back(message);
}

bool MyGame::isPlayerOnSite(int siteIndex) {
    if (myPlayerNumber == 1) {
        return game_data.player1.currentSite == siteIndex;
    }
    else {
        return game_data.player2.currentSite == siteIndex;
    }
}

void MyGame::input(SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int mouseX = event.button.x;
        int mouseY = event.button.y;

        if (gameState == LOBBY) {
            //Check for room selection buttons
            for (int i = 0; i < 3; i++) {
                int btnX = SCREEN_WIDTH / 2 - 150;
                int btnY = 200 + (i * 80);
                int btnW = 300;
                int btnH = 60;

                if (mouseX >= btnX && mouseX <= btnX + btnW &&
                    mouseY >= btnY && mouseY <= btnY + btnH) {

                    if (roomPlayerCounts[i] < 2) {
                        std::string msg = "JOIN_ROOM," + std::to_string(i);
                        send(msg);
                        selectedRoom = i;
                        std::cout << "Requesting to join Room " << (i + 1) << std::endl;
                    }
                    else {
                        std::cout << "Room " << (i + 1) << " is full!" << std::endl;
                    }
                    return;
                }
            }
            return;
        }

        if (gameState == WAITING) {
            return;
        }

        if (game_data.sites.size() != 8) {
            return;
        }

        Player& myPlayer = (myPlayerNumber == 1) ? game_data.player1 : game_data.player2;

        if (game_data.inCombat && game_data.canRetreat) {
            int btnW = 200;
            int btnH = 60;
            int retreatBtnX = SCREEN_WIDTH / 2 - 100;
            int retreatBtnY = SCREEN_HEIGHT - 80;


            int clickPadding = 5;
            if (mouseX >= (retreatBtnX - clickPadding) && mouseX <= (retreatBtnX + btnW + clickPadding) &&
                mouseY >= (retreatBtnY - clickPadding) && mouseY <= (retreatBtnY + btnH + clickPadding)) {

                std::string msg = "RETREAT," + std::to_string(myPlayerNumber);
                send(msg);
                std::cout << "Requested retreat from combat" << std::endl;
                return;
            }
        }

        //Don't allow normal actions during combat
        if (game_data.inCombat) {
            std::cout << "Cannot perform actions during combat" << std::endl;
            return;
        }

        //Check if clicking on build menu buttons
        if (myPlayer.currentSite >= 0 && myPlayer.currentSite < 8) {
            Site& site = game_data.sites[myPlayer.currentSite];
            int menuX = site.center.x - 120;
            int menuY = site.center.y + 30;

            //Castle button
            if (mouseX >= menuX && mouseX < menuX + 75 &&
                mouseY >= menuY && mouseY < menuY + 25) {

                std::string msg = "BUILD_CASTLE," + std::to_string(myPlayerNumber) + "," +
                    std::to_string(myPlayer.currentSite);
                send(msg);
                std::cout << "Requested to build castle on site " << myPlayer.currentSite << std::endl;
                return;
            }

            //Gold Mine button
            if (mouseX >= menuX + 80 && mouseX < menuX + 155 &&
                mouseY >= menuY && mouseY < menuY + 25) {

                std::string msg = "BUILD_GOLD_MINE," + std::to_string(myPlayerNumber) + "," +
                    std::to_string(myPlayer.currentSite);
                send(msg);
                std::cout << "Requested to build gold mine on site " << myPlayer.currentSite << std::endl;
                return;
            }

            //Barracks button
            if (mouseX >= menuX + 160 && mouseX < menuX + 235 &&
                mouseY >= menuY && mouseY < menuY + 25) {

                std::string msg = "BUILD_BARRACKS," + std::to_string(myPlayerNumber) + "," +
                    std::to_string(myPlayer.currentSite);
                send(msg);
                std::cout << "Requested to build barracks on site " << myPlayer.currentSite << std::endl;
                return;
            }
        }

        //CLIENT-SIDE PREDICTION FOR MOVEMENT
        int siteIndex = findClosestSite(mouseX, mouseY);

        if (siteIndex >= 0 && siteIndex < 8) {
            Point target = game_data.sites[siteIndex].center;

            std::string msg = "MOVE," + std::to_string(myPlayerNumber) + "," +
                std::to_string(target.x) + "," + std::to_string(target.y) + "," +
                std::to_string(deltaTime);
            send(msg);

            //Client-Side Prediction: Immediately start moving our player locally
            //This provides instant visual feedback while we wait for server confirmation
            if (myPlayerNumber == 1) {
                game_data.player1.targetPosition = target;
                game_data.player1.isMoving = true;
                std::cout << "[CLIENT PREDICTION] Player 1 immediately moving to site " << siteIndex << std::endl;
            }
            else {
                game_data.player2.targetPosition = target;
                game_data.player2.isMoving = true;
                std::cout << "[CLIENT PREDICTION] Player 2 immediately moving to site " << siteIndex << std::endl;
            }
        }
    }
}

void MyGame::update(float dt) {
    deltaTime = dt;

    if (gameState != PLAYING) {
        return;
    }

    //Update combat timer locally to save on amount of messages being sent
    if (game_data.inCombat) {
        game_data.combatTimer += dt;
        if (game_data.combatTimer >= 5.0f) {
            game_data.canRetreat = true;
        }
    }

    const float MOVEMENT_SPEED = 300.0f;


    if (game_data.player1.isMoving) {
        float dist = distance(game_data.player1.position.x, game_data.player1.position.y,
            game_data.player1.targetPosition.x, game_data.player1.targetPosition.y);

        if (dist < 5.0f) {
 
            game_data.player1.position = game_data.player1.targetPosition;
            game_data.player1.isMoving = false;
        }
        else {

            int dx = game_data.player1.targetPosition.x - game_data.player1.position.x;
            int dy = game_data.player1.targetPosition.y - game_data.player1.position.y;

            float moveX = (dx / dist) * MOVEMENT_SPEED * deltaTime;
            float moveY = (dy / dist) * MOVEMENT_SPEED * deltaTime;

            game_data.player1.position.x += static_cast<int>(moveX);
            game_data.player1.position.y += static_cast<int>(moveY);
        }
    }


    if (game_data.player2.isMoving) {
        float dist = distance(game_data.player2.position.x, game_data.player2.position.y,
            game_data.player2.targetPosition.x, game_data.player2.targetPosition.y);

        if (dist < 5.0f) {

            game_data.player2.position = game_data.player2.targetPosition;
            game_data.player2.isMoving = false;
        }
        else {

            int dx = game_data.player2.targetPosition.x - game_data.player2.position.x;
            int dy = game_data.player2.targetPosition.y - game_data.player2.position.y;

            float moveX = (dx / dist) * MOVEMENT_SPEED * deltaTime;
            float moveY = (dy / dist) * MOVEMENT_SPEED * deltaTime;

            game_data.player2.position.x += static_cast<int>(moveX);
            game_data.player2.position.y += static_cast<int>(moveY);
        }
    }

    if (game_data.sites.size() == 8) {
        if (!game_data.player1.isMoving) {
            int closestSite = findClosestSite(game_data.player1.position.x, game_data.player1.position.y);
            float distToSite = distance(game_data.player1.position.x, game_data.player1.position.y,
                game_data.sites[closestSite].center.x, game_data.sites[closestSite].center.y);

            if (distToSite < 50.0f) {
                game_data.player1.currentSite = closestSite;
            }
            else {
                game_data.player1.currentSite = -1;
            }
        }
        else {

            game_data.player1.currentSite = -1;
        }

        if (!game_data.player2.isMoving) {
            int closestSite = findClosestSite(game_data.player2.position.x, game_data.player2.position.y);
            float distToSite = distance(game_data.player2.position.x, game_data.player2.position.y,
                game_data.sites[closestSite].center.x, game_data.sites[closestSite].center.y);

            if (distToSite < 50.0f) {
                game_data.player2.currentSite = closestSite;
            }
            else {
                game_data.player2.currentSite = -1;
            }
        }
        else {

            game_data.player2.currentSite = -1;
        }
    }

    //Update capture progress locally
    if (game_data.player1.isCapturing) {
        game_data.player1.captureProgress += dt / 10.0f;
        if (game_data.player1.captureProgress >= 1.0f) {
            game_data.player1.captureProgress = 1.0f;
        }
    }
    else {
        game_data.player1.captureProgress = 0.0f;
    }

    if (game_data.player2.isCapturing) {
        game_data.player2.captureProgress += dt / 10.0f;
        if (game_data.player2.captureProgress >= 1.0f) {
            game_data.player2.captureProgress = 1.0f;
        }
    }
    else {
        game_data.player2.captureProgress = 0.0f;
    }
}

void MyGame::renderPlayer(SDL_Renderer* renderer, Player& player) {
    int radius = 15;

    if (player.playerNumber == 1) {
        SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);
    }
    else {
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
    }

    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer,
                    player.position.x + dx,
                    player.position.y + dy);
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    radius = 17;
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            int distSq = dx * dx + dy * dy;
            if (distSq <= (radius * radius) && distSq >= ((radius - 3) * (radius - 3))) {
                SDL_RenderDrawPoint(renderer,
                    player.position.x + dx,
                    player.position.y + dy);
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int w = -2; w <= 2; w++) {
        for (int h = -2; h <= 2; h++) {
            if (w * w + h * h <= 4) {
                SDL_RenderDrawPoint(renderer,
                    player.position.x + w,
                    player.position.y + h);
            }
        }
    }
}

void MyGame::renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, int size) {
    static const bool digitPatterns[13][7][5] = {
        {{0,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,0}},
        {{0,0,1,0,0}, {0,1,1,0,0}, {0,0,1,0,0}, {0,0,1,0,0}, {0,0,1,0,0}, {0,0,1,0,0}, {0,1,1,1,0}},
        {{0,1,1,1,0}, {1,0,0,0,1}, {0,0,0,0,1}, {0,0,0,1,0}, {0,0,1,0,0}, {0,1,0,0,0}, {1,1,1,1,1}},
        {{1,1,1,1,1}, {0,0,0,1,0}, {0,0,1,0,0}, {0,0,0,1,0}, {0,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,0}},
        {{0,0,0,1,0}, {0,0,1,1,0}, {0,1,0,1,0}, {1,0,0,1,0}, {1,1,1,1,1}, {0,0,0,1,0}, {0,0,0,1,0}},
        {{1,1,1,1,1}, {1,0,0,0,0}, {1,1,1,1,0}, {0,0,0,0,1}, {0,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,0}},
        {{0,0,1,1,0}, {0,1,0,0,0}, {1,0,0,0,0}, {1,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,0}},
        {{1,1,1,1,1}, {0,0,0,0,1}, {0,0,0,1,0}, {0,0,1,0,0}, {0,1,0,0,0}, {0,1,0,0,0}, {0,1,0,0,0}},
        {{0,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,0}},
        {{0,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,1}, {0,0,0,0,1}, {0,0,0,1,0}, {0,1,1,0,0}},
        {{1,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {1,1,1,1,0}, {1,0,0,0,0}, {1,0,0,0,0}, {1,0,0,0,0}},
        {{0,0,0,0,0}, {0,0,1,0,0}, {0,0,1,0,0}, {0,0,0,0,0}, {0,0,1,0,0}, {0,0,1,0,0}, {0,0,0,0,0}},
        {{0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}}
    };

    int cursorX = x;
    for (char c : text) {
        int patternIdx = -1;

        if (c >= '0' && c <= '9') {
            patternIdx = c - '0';
        }
        else if (c == 'P') {
            patternIdx = 10;
        }
        else if (c == ':') {
            patternIdx = 11;
        }
        else if (c == ' ') {
            patternIdx = 12;
        }

        if (patternIdx >= 0) {
            for (int row = 0; row < 7; row++) {
                for (int col = 0; col < 5; col++) {
                    if (digitPatterns[patternIdx][row][col]) {
                        SDL_Rect rect = { cursorX + col * size, y + row * size, size, size };
                        SDL_RenderFillRect(renderer, &rect);
                    }
                }
            }
        }

        cursorX += 6 * size;
    }
}

void MyGame::renderLobby(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    renderText(renderer, "SELECT A ROOM", SCREEN_WIDTH / 2 - 100, 80, 4);

    for (int i = 0; i < 3; i++) {
        int btnX = SCREEN_WIDTH / 2 - 150;
        int btnY = 200 + (i * 80);
        int btnW = 300;
        int btnH = 60;

        if (roomPlayerCounts[i] >= 2) {
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        }
        else if (roomPlayerCounts[i] == 1) {
            SDL_SetRenderDrawColor(renderer, 200, 200, 50, 255);
        }
        else {
            SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
        }

        SDL_Rect btnRect = { btnX, btnY, btnW, btnH };
        SDL_RenderFillRect(renderer, &btnRect);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &btnRect);

        std::string roomText = "ROOM " + std::to_string(i + 1);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        renderText(renderer, roomText, btnX + 20, btnY + 10, 3);

        std::string playerText = std::to_string(roomPlayerCounts[i]) + "2 PLAYERS";
        renderText(renderer, playerText, btnX + 80, btnY + 35, 2);
    }
}

void MyGame::renderWaiting(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    renderText(renderer, "WAITING FOR", SCREEN_WIDTH / 2 - 100, 250, 4);
    renderText(renderer, "OPPONENT", SCREEN_WIDTH / 2 - 70, 300, 4);

    std::string playerText = "YOU ARE PLAYER " + std::to_string(myPlayerNumber);
    renderText(renderer, playerText, SCREEN_WIDTH / 2 - 140, 380, 3);
}

void MyGame::renderBuildMenu(SDL_Renderer* renderer, int siteIndex) {
    Site& site = game_data.sites[siteIndex];

    int menuX = site.center.x - 120;  
    int menuY = site.center.y + 30;

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 230);
    SDL_Rect bgRect = { menuX - 5, menuY - 5, 245, 35 };  
    SDL_RenderFillRect(renderer, &bgRect);

    // Castle button (brown)
    SDL_SetRenderDrawColor(renderer, 150, 100, 50, 255);
    SDL_Rect castleBtn = { menuX, menuY, 75, 25 };
    SDL_RenderFillRect(renderer, &castleBtn);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    renderText(renderer, "1250", menuX + 5, menuY + 5, 2);

    // Gold Mine button (gold)
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    SDL_Rect goldMineBtn = { menuX + 80, menuY, 75, 25 };
    SDL_RenderFillRect(renderer, &goldMineBtn);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    renderText(renderer, "500", menuX + 90, menuY + 5, 2);

    // Barracks button (gray/silver)
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_Rect barracksBtn = { menuX + 160, menuY, 75, 25 };
    SDL_RenderFillRect(renderer, &barracksBtn);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    renderText(renderer, "750", menuX + 170, menuY + 5, 2);
}

void MyGame::renderCaptureBar(SDL_Renderer* renderer, Player& player) {
    if (!player.isCapturing) return;

    int barWidth = 60;
    int barHeight = 8;
    int barX = player.position.x - barWidth / 2;
    int barY = player.position.y - 30;

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_Rect bgRect = { barX, barY, barWidth, barHeight };
    SDL_RenderFillRect(renderer, &bgRect);

    SDL_SetRenderDrawColor(renderer, 255, 200, 50, 255);
    SDL_Rect progressRect = { barX, barY, static_cast<int>(barWidth * player.captureProgress), barHeight };
    SDL_RenderFillRect(renderer, &progressRect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &bgRect);
}

void MyGame::renderUI(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255);
    SDL_Rect p1Rect = { 10, 10, 80, 40 };
    SDL_RenderFillRect(renderer, &p1Rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    std::string p1Text = "P1: " + std::to_string(game_data.player1Score);
    renderText(renderer, p1Text, 20, 20, 3);

    SDL_SetRenderDrawColor(renderer, 150, 50, 50, 255);
    SDL_Rect p2Rect = { SCREEN_WIDTH - 90, 10, 80, 40 };
    SDL_RenderFillRect(renderer, &p2Rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    std::string p2Text = "P2: " + std::to_string(game_data.player2Score);
    renderText(renderer, p2Text, SCREEN_WIDTH - 80, 20, 3);

    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    SDL_Rect p1GoldRect = { 10, 55, 100, 25 };
    SDL_RenderFillRect(renderer, &p1GoldRect);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    std::string p1GoldText = std::to_string(game_data.player1Gold);
    renderText(renderer, p1GoldText, 20, 60, 2);

    SDL_SetRenderDrawColor(renderer, 192, 192, 192, 255);
    SDL_Rect p1LevyRect = { 10, 85, 100, 25 };
    SDL_RenderFillRect(renderer, &p1LevyRect);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    std::string p1LevyText = std::to_string(game_data.player1Levies);
    renderText(renderer, p1LevyText, 20, 90, 2);

    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
    SDL_Rect p2GoldRect = { SCREEN_WIDTH - 110, 55, 100, 25 };
    SDL_RenderFillRect(renderer, &p2GoldRect);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    std::string p2GoldText = std::to_string(game_data.player2Gold);
    renderText(renderer, p2GoldText, SCREEN_WIDTH - 100, 60, 2);

    SDL_SetRenderDrawColor(renderer, 192, 192, 192, 255);
    SDL_Rect p2LevyRect = { SCREEN_WIDTH - 110, 85, 100, 25 };
    SDL_RenderFillRect(renderer, &p2LevyRect);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    std::string p2LevyText = std::to_string(game_data.player2Levies);
    renderText(renderer, p2LevyText, SCREEN_WIDTH - 100, 90, 2);

    if (myPlayerNumber == 1) {
        SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);
    }
    else {
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
    }
    SDL_Rect indicatorRect = { SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT - 50, 120, 40 };
    SDL_RenderFillRect(renderer, &indicatorRect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    std::string indicatorText = "P" + std::to_string(myPlayerNumber) + ": YOU";
    renderText(renderer, indicatorText, SCREEN_WIDTH / 2 - 40, SCREEN_HEIGHT - 40, 3);
}

void MyGame::renderCombatUI(SDL_Renderer* renderer) {
    if (!game_data.inCombat) return;

    int btnW = 200;
    int btnH = 60;
    int btnX = SCREEN_WIDTH / 2 - 100;  //Centered horizontally
    int btnY = SCREEN_HEIGHT - 80;      //Near bottom of screen


    if (game_data.canRetreat) {

        SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
    }
    else {

        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 220);
    }

    SDL_Rect btn = { btnX, btnY, btnW, btnH };
    SDL_RenderFillRect(renderer, &btn);


    if (game_data.canRetreat) {
        SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
    }
    else {
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    }
    for (int i = 0; i < 2; i++) {
        SDL_Rect border = { btnX - i, btnY - i, btnW + (i * 2), btnH + (i * 2) };
        SDL_RenderDrawRect(renderer, &border);
    }

        // Show countdown timer ABOVE the button (smaller, no text)
        int remaining = static_cast<int>(5.0f - game_data.combatTimer);
        if (remaining < 0) remaining = 0;

        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
        std::string countdownText = std::to_string(remaining);
        int countdownX = btnX + 90;
        int countdownY = btnY - 30;  // Above the button
        renderText(renderer, countdownText, countdownX, countdownY, 3);  // Smaller size

    
}

void MyGame::renderGameOver(SDL_Renderer* renderer) {
    int winner = game_data.winner;

    //Use fixed screen dimensions to ensure consistency
    int boxWidth = 300;
    int boxHeight = 100;
    int boxX = 400 - boxWidth / 2;  
    int boxY = 300 - boxHeight / 2;  

    if (winner == myPlayerNumber) {
        //Green background for winner
        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
        SDL_Rect winRect = { boxX, boxY, boxWidth, boxHeight };
        SDL_RenderFillRect(renderer, &winRect);
    }
    else {
        //Red background for loser
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
        SDL_Rect loseRect = { boxX, boxY, boxWidth, boxHeight };
        SDL_RenderFillRect(renderer, &loseRect);
    }


    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect borderRect = { boxX, boxY, boxWidth, boxHeight };
    SDL_RenderDrawRect(renderer, &borderRect);

    std::string winnerText = "P" + std::to_string(winner);
    int textSize = 10;
    int textX = 340;
    int textY = 265;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    renderText(renderer, winnerText, textX, textY, textSize);
}

void MyGame::render(SDL_Renderer* renderer) {
    if (gameState == LOBBY) {
        renderLobby(renderer);
        return;
    }

    if (game_data.gameOver) {
        renderGameOver(renderer);
        return;
    }

    if (gameState == WAITING) {
        renderWaiting(renderer);
        return;
    }

    if (game_data.sites.size() != 8) {
        return;
    }

    int step = 2;

    for (int y = 0; y < SCREEN_HEIGHT; y += step) {
        for (int x = 0; x < SCREEN_WIDTH; x += step) {
            int siteIndex = findClosestSite(x, y);
            SDL_Color color = getSiteColor(siteIndex);

            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_Rect rect = { x, y, step, step };
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    for (int i = 0; i < 8; i++) {
        Site& site = game_data.sites[i];

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        int radius = 8;
        for (int w = 0; w < radius * 2; w++) {
            for (int h = 0; h < radius * 2; h++) {
                int dx = radius - w;
                int dy = radius - h;
                if ((dx * dx + dy * dy) <= (radius * radius)) {
                    SDL_RenderDrawPoint(renderer,
                        site.center.x + dx,
                        site.center.y + dy);
                }
            }
        }

        if (game_data.isPlayer1Owner(i)) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        }
        else if (game_data.isPlayer2Owner(i)) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        }
        else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        }

        radius = 10;
        for (int w = 0; w < radius * 2; w++) {
            for (int h = 0; h < radius * 2; h++) {
                int dx = radius - w;
                int dy = radius - h;
                int distSq = dx * dx + dy * dy;
                if (distSq <= (radius * radius) && distSq >= ((radius - 2) * (radius - 2))) {
                    SDL_RenderDrawPoint(renderer,
                        site.center.x + dx,
                        site.center.y + dy);
                }
            }
        }

        if (site.hasCastle) {
            SDL_SetRenderDrawColor(renderer, 150, 100, 50, 255);
            SDL_Rect castleRect = { site.center.x - 26, site.center.y - 6, 12, 12 };
            SDL_RenderFillRect(renderer, &castleRect);

            for (int b = 0; b < 3; b++) {
                SDL_Rect battlement = { site.center.x - 6 + (b * 5), site.center.y - 18, 3, 3 };
                SDL_RenderFillRect(renderer, &battlement);
            }
        }

        if (site.hasGoldMine) {
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
            int gmRadius = 7;
            for (int w = 0; w < gmRadius * 2; w++) {
                for (int h = 0; h < gmRadius * 2; h++) {
                    int dx = gmRadius - w;
                    int dy = gmRadius - h;
                    if ((dx * dx + dy * dy) <= (gmRadius * gmRadius)) {
                        SDL_RenderDrawPoint(renderer,
                            site.center.x + dx,
                            site.center.y + 15 + dy);
                    }
                }
            }

            SDL_SetRenderDrawColor(renderer, 255, 255, 200, 255);
            for (int w = -2; w <= 2; w++) {
                for (int h = -2; h <= 2; h++) {
                    if (w * w + h * h <= 4) {
                        SDL_RenderDrawPoint(renderer,
                            site.center.x + w,
                            site.center.y + 15 + h);
                    }
                }
            }

            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
            SDL_Rect goldRect = { site.center.x - 3, site.center.y + 20, 6, 6 };
            SDL_RenderFillRect(renderer, &goldRect);
        }

        if (site.hasBarracks) {
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);

            SDL_Rect barracksRect = { site.center.x - 8, site.center.y - 25, 16, 10 };
            SDL_RenderFillRect(renderer, &barracksRect);


            SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255);
            SDL_Rect doorRect = { site.center.x - 2, site.center.y - 18, 4, 6 };
            SDL_RenderFillRect(renderer, &doorRect);
        }

        if (game_data.inCombat && i == game_data.combatSite) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            int highlightRadius = 25;
            for (int angle = 0; angle < 360; angle += 3) {
                float rad = angle * 3.14159f / 180.0f;
                int x1 = site.center.x + static_cast<int>(highlightRadius * cos(rad));
                int y1 = site.center.y + static_cast<int>(highlightRadius * sin(rad));
                for (int thick = 0; thick < 3; thick++) {
                    SDL_RenderDrawPoint(renderer, x1 + thick, y1);
                    SDL_RenderDrawPoint(renderer, x1, y1 + thick);
                }
            }
        }
    }

    renderPlayer(renderer, game_data.player1);
    renderPlayer(renderer, game_data.player2);

    renderCaptureBar(renderer, game_data.player1);
    renderCaptureBar(renderer, game_data.player2);

    Player& myPlayer = (myPlayerNumber == 1) ? game_data.player1 : game_data.player2;
    if (myPlayer.currentSite >= 0 && myPlayer.currentSite < 8 && !game_data.inCombat) {
        renderBuildMenu(renderer, myPlayer.currentSite);
    }

    renderUI(renderer);
    renderCombatUI(renderer);
}