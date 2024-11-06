#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <vector>
#include <chrono>
#include <thread>
#include <random>

// TODO
// ! Add logic for digesting more than 1 food at once - give snake local set of pointers to digested nodes !
// ! Add option to enter command line args
// ! Add FPS display
// Add game states
// Deallocate all nodes after game over
// Represent game board as graph of Nodes:
// - every x,y coordinate on board is a node in the graph
// - make every Node have type attribute:
//      - blank, border, snake, food
// - Initialize all board nodes and set their types. Instead of creating distinct nodes for snake and food, calculate their positions and
//   convert them to row/col so that the corresponding nodes in the graph can have their types set accordingly. This way, all the nodes that
//   exist are contained in the graph.
// - every node has at most 4 neighbors:
//      - above, below, left, right (can be nullptr)
// - draw game board by drawing each node of the graph.
// - use BFS to find shortest path from snake's head to food
// Add ASCII UI
// Add collision detection and game over conditions where game ends before snake is draw overlapping border/itself
// Add play, pause, reset, etc...
// Add score and speed increases
// Add obstacles/new maps

enum Direction
{ UP, DOWN, LEFT, RIGHT };

enum NodeType {BLANK, BORDER, SNAKE, FOOD};

struct Node
{
    int xPos, yPos;
    Node *next = nullptr;
    Node *prev = nullptr;
    Direction dir;
    NodeType type;
};

struct Snake
{
    Node *head = nullptr;
    Node *tail = nullptr;
    Node *digFood = nullptr;
    std::vector<Node*> body;
};

struct GameBoard
{
    int boardWidth, boardHeight;
    std::vector< std::vector<Node*> > graph;
};

// Constants
const int BOARD_WIDTH = 40;
const int BOARD_HEIGHT = 20;

// Function prototypes
void setNonCanonicalMode();
bool kbhit();
Node *createNode(int x, int y);
bool checkWindowSize(int &width, int &height);
void drawGameBoard(int &width, int &height, int x, int y);
void drawSnake(Snake &snake);
void moveSnake(Snake &snake);
void growSnake(Snake &snake);
void drawFood(Snake &snake, Node *food, std::uniform_int_distribution<> &xDist, std::uniform_int_distribution<> &yDist, std::mt19937 &seed, int screenW);
void resetTerminal(termios &t);
bool checkForCollision(int screenW, int screenH, Snake &snake);


int main()
{
    // Initialize game data
    int windowW, windowH;

    if (!checkWindowSize(windowW, windowH))
    {
        std::cout << "Window size needs to be at least 80px x 30px. Aborting..." << std::endl;
        return 1;
    }

    struct termios oldt;
    tcgetattr(STDIN_FILENO, &oldt); // Save old settings
    setNonCanonicalMode(); // Set new settings
    std::random_device rd; // Obtain a random number from hardware
    std::mt19937 eng(rd()); // Seed the generator
    std::cout << "\033[?25l"; // Hide the cursor

    int startX, startY;
    int score;
    Snake snake;
    std::vector<std::vector<Node*>> gameBoard;
    std::string scoreDisplay = "Score: 0";
    std::string title[5] = {{"#####  #   #   ###   #   #  #####"},
                            {"#      ##  #  #   #  # ##   #"},
                            {"#####  # # #  #####  ##     ###"},
                            {"    #  #  ##  #   #  # ##   #"},
                            {"#####  #   #  #   #  #   #  #####"}};

    startX = windowW / 2 - BOARD_WIDTH / 2;
    startY = windowH / 2 - BOARD_HEIGHT / 2;

    if (startX % 2 != 0)
        --startX;

    std::uniform_int_distribution<> xDistr(startX + 2, startX + BOARD_WIDTH - 2);
    std::uniform_int_distribution<> yDistr(startY + 1, startY + BOARD_HEIGHT - 2);

    snake.body.push_back(createNode(startX + BOARD_WIDTH / 2, startY + BOARD_HEIGHT / 2));
    snake.body.push_back(createNode(startX + BOARD_WIDTH / 2 - 1, startY + BOARD_HEIGHT / 2));
    snake.body.push_back(createNode(startX + BOARD_WIDTH / 2 - 2, startY + BOARD_HEIGHT / 2));
    snake.head = snake.body[0];
    snake.tail = snake.body[2];
    snake.head->next = snake.body[1];
    snake.head->next->prev = snake.head;
    snake.head->next->next = snake.tail;
    snake.tail->prev = snake.head->next;
    for (Node *n : snake.body)
        n->dir = RIGHT;

    Node *food = createNode(xDistr(eng), yDistr(eng));

    system("clear");

    drawFood(snake, food, xDistr, yDistr, eng, windowW);
    drawSnake(snake);

    // Draw Game Board
    int titleY = startY - 6;
    for (std::string s : title)
    {
        std::cout << "\033[" << titleY << ";" << startX -15 + BOARD_WIDTH/2 << "H\033[32m" << s << "\033[0m";
        ++titleY;
    }

    drawGameBoard(windowW, windowH, startX, startY);

    std::cout << "\033[" << startY + 1 + BOARD_HEIGHT << ";" << startX - 3 + BOARD_WIDTH/2 << "H\033[32m" << scoreDisplay << "\033[0m";

    // Start game
    auto lastUpdateTime = std::chrono::high_resolution_clock::now();
    auto lastRenderTime = std::chrono::high_resolution_clock::now();
    char ch;
    bool gameActive = true;

    while (ch != 'q' && gameActive)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime);
        auto elapsedRenderTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastRenderTime);

            if (kbhit())
            {
                ch = getchar(); // Get the key pressed
                switch (ch)
                {
                    case 'a' : snake.head->dir = LEFT; break;
                    case 'd' : snake.head->dir = RIGHT; break;
                    case 'w' : snake.head->dir = UP; break;
                    case 's' : snake.head->dir = DOWN; break;
                    case 'q' : break;
                }
            }

        if (elapsedUpdateTime.count() >= 100)
        {

            moveSnake(snake);

            if (!checkForCollision(startX, startY, snake))
                gameActive = false;

            if (snake.digFood != nullptr)
                snake.digFood = snake.digFood->next;

            lastUpdateTime = currentTime;
        }

        if (elapsedRenderTime.count() >= 10)
        {
            if (snake.head->xPos == food->xPos && snake.head->yPos == food->yPos)
            {
                drawFood(snake, food, xDistr, yDistr, eng, windowW);
                snake.digFood = snake.head->next;

                // Update score
                score += 10;
                scoreDisplay.clear();
                scoreDisplay = "Score: " + std::to_string(score);
                std::cout << "\033[" << startY + 1 + BOARD_HEIGHT << ";" << startX - 3 + BOARD_WIDTH/2 << "H\033[32m" << scoreDisplay << "\033[0m";
            }

            if (snake.digFood == snake.tail)
            {
                snake.digFood = nullptr;
                growSnake(snake);
            }

            drawSnake(snake);

            //std::cout << "FPS: " + std::to_string(1000.0 / elapsedTime.count()) << std::endl;
            std::cout << std::flush;
            lastRenderTime = currentTime;
        }
    }

    drawSnake(snake);

    // Deallocate memory for all nodes created
    for (Node *n : snake.body)
        delete n;
    delete food;

    // Return terminal to default state
    std::cout << "\033[?25h"; // Show the cursor
    std::cout << "\033[" << startY - 1 + BOARD_HEIGHT / 2 << ";" << startX - 4 + BOARD_WIDTH / 2 << "H" << "Game Over" << "\033[0m" << std::endl;
    std::cout << "\033[" << windowH << "H";
    resetTerminal(oldt);

    return 0;
}

Node *createNode(int x, int y)
{
    Node *n = new Node;
    n->xPos = x;
    n->yPos = y;
    n->prev = nullptr;
    n->next = nullptr;
    return n;
}

bool checkWindowSize(int &width, int &height)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    width = w.ws_col;
    height = w.ws_row;

    if (width >= BOARD_WIDTH * 2 && height >= BOARD_HEIGHT * 2)
        return true;
    else
        return false;
}

void drawGameBoard(int &width, int &height, int x, int y)
{
    for (int i = y; i < BOARD_HEIGHT + y; ++i)
    {
        for (int j = x; j < BOARD_WIDTH + x; ++j)
        {
            if (i == y || i == BOARD_HEIGHT + y - 1 || j == x || j == BOARD_WIDTH + x - 1)
                std::cout << "\033[" << i << ";" << j << "H\033[34m#\033[0m";


        }
    }
}

void drawSnake(Snake &snake)
{
    char c;
    for (Node *n : snake.body)
    {
        if (n == snake.head)
        {
            switch (n->dir)
            {
                case UP   : c = '^'; break;
                case DOWN : c = 'v'; break;
                case LEFT : c = '<'; break;
                case RIGHT: c = '>'; break;
            }
        }
        else if (n == snake.tail)
            c = '~';
        else if (n == snake.digFood)
            c = '@';
        else
            c = '*';
        std::cout << "\033[" << n->yPos << ";" << n->xPos << "H" << "\033[32m" << c << "\033[0m";

    }
}

void moveSnake(Snake &snake)
{
    for (Node *n : snake.body)
        std::cout << "\033[" << n->yPos << ";" << n->xPos << "H ";

    for (int i = snake.body.size() - 1; i > 0; --i)
    {
        snake.body[i]->xPos = snake.body[i]->prev->xPos;
        snake.body[i]->yPos = snake.body[i]->prev->yPos;
        snake.body[i]->dir = snake.body[i]->prev->dir;
    }

    switch (snake.head->dir)
    {
        case UP    : snake.head->yPos--; break;
        case DOWN  : snake.head->yPos++; break;
        case LEFT  : snake.head->xPos -= 2; break;
        case RIGHT : snake.head->xPos += 2; break;
    }
}

void growSnake(Snake &snake)
{
    int newX, newY;

    switch (snake.tail->dir)
    {
        case UP :    newX = snake.tail->xPos; newY = snake.tail->yPos - 1; break;
        case DOWN :  newX = snake.tail->xPos; newY = snake.tail->yPos + 1; break;
        case LEFT :  newX = snake.tail->xPos - 1; newY = snake.tail->yPos; break;
        case RIGHT:  newX = snake.tail->xPos + 1; newY = snake.tail->yPos; break;
    }

    std::vector<Node*>::iterator it = snake.body.end() - 1;
    snake.body.insert(it, createNode(newX, newY));
    Node *n = snake.body[snake.body.size() - 2];
    n->prev = snake.tail->prev;
    n->next = snake.tail;
    snake.tail->prev->next = n;
    snake.tail->prev = n;
}

bool checkFoodPos(Snake &snake, int fx, int fy)
{
    for (Node *n : snake.body)
    {
        if (n->xPos == fx && n->yPos == fy)
            return false;
    }

    return true;
}

void drawFood(Snake &snake, Node *food, std::uniform_int_distribution<> &xDist, std::uniform_int_distribution<> &yDist, std::mt19937 &seed, int screenW)
{
    int foodX, foodY;

    do
    {
        foodX = xDist(seed);
        foodY = yDist(seed);

        if (foodX % 2 != 0)
            --foodX;
    }
    while (!checkFoodPos(snake, foodX, foodY));

    std::cout << "\033[" << food->yPos << ";" << food->xPos << "H ";

    food->xPos = foodX;
    food->yPos = foodY;

    std::cout << "\033[" << food->yPos << ";" << food->xPos << "H" << "\033[31m" << '@' << "\033[0m";

}

// Check to see if snake head has collided with either one of its own nodes or a game board boundary.
bool checkForCollision(int minX, int minY, Snake &snake)
{
    if (snake.head->xPos <= minX || snake.head->xPos >= minX + BOARD_WIDTH ||
        snake.head->yPos <= minY || snake.head->yPos >= minY + BOARD_HEIGHT - 1)
        return false;

    for (int i = 1; i < snake.body.size(); ++i)
    {
        if (snake.head->xPos == snake.body[i]->xPos && snake.head->yPos == snake.body[i]->yPos)
            return false;
    }

    return true;
}

// Function to set terminal to non-canonical mode
void setNonCanonicalMode() {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &newt);
    newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

// Function to check if a key has been pressed
bool kbhit() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int bytesWaiting = 0;
    ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return bytesWaiting > 0;
}

void resetTerminal(struct termios &oldt) { tcsetattr(STDIN_FILENO, TCSANOW, &oldt); }

