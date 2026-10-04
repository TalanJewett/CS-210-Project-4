// JDoodle: Libraries = SDL2; enable GUI and Interactive.
// Assignment 4: Linked Lists and Monopoly (GUI version)
//
// Controls:
//   Click the buttons on the right (or press SPACE for the main action).
//   Click the dice to roll. Click any board space to inspect it, then use
//   BUILD / SELL / MORTGAGE buttons on your own properties.
//   Press A to let the computer play one step for whoever is up (hold A to fast-forward).
#include <SDL2/SDL.h>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

const double PI = 3.14159265358979;

// =====================================================================
// PART 1: CIRCULAR SINGLY LINKED LIST (no std::list)
// =====================================================================
enum SpaceType { GO_SPACE, STREET, RAILROAD, UTILITY, TAX, CHANCE, CHEST, JAIL, FREE_PARKING, GO_TO_JAIL };

struct SpaceInfo {
    string name, label;   // full name, short label drawn on the tile
    SpaceType type;
    int cost;             // purchase price (or tax amount)
    int group;            // color group 0-7, 8 = railroads, 9 = utilities, -1 = none
    int houseCost;
    int rent[6];          // base rent, 1-4 houses, hotel
};

struct Space {
    string name, label;
    SpaceType type;
    int cost;
    int owner;            // -1 = bank, 0 = P1, 1 = P2
    Space* next;
    int group, houseCost, rent[6];
    int houses;           // 0-4 houses, 5 = hotel
    bool mortgaged;
    int index;
};

class Board {
public:
    Board() = default;
    Board(const Board&) = delete;
    Board& operator=(const Board&) = delete;
    ~Board() { clear(); }

    // Add to the end. O(1) because we keep a tail pointer.
    void add(const SpaceInfo& info) {
        Space* s = new Space;
        s->name = info.name;
        s->label = info.label;
        s->type = info.type;
        s->cost = info.cost;
        s->owner = -1;
        s->group = info.group;
        s->houseCost = info.houseCost;
        for (int i = 0; i < 6; i++) s->rent[i] = info.rent[i];
        s->houses = 0;
        s->mortgaged = false;
        s->index = count;
        if (!head) head = tail = s;
        else {
            tail->next = s;
            tail = s;
        }
        tail->next = head;  // keep it circular
        count++;
    }

    // Find by name. O(n).
    Space* search(const string& name) const {
        if (!head) return nullptr;
        Space* cur = head;
        do {
            if (cur->name == name) return cur;
            cur = cur->next;
        } while (cur != head);
        return nullptr;
    }

    // Remove by name. O(n): we must walk to find the node and its predecessor.
    bool remove(const string& name) {
        if (!head) return false;
        Space* prev = tail;
        Space* cur = head;
        for (int i = 0; i < count; i++) {
            if (cur->name == name) {
                if (count == 1) head = tail = nullptr;
                else {
                    prev->next = cur->next;
                    if (cur == head) head = cur->next;
                    if (cur == tail) tail = prev;
                }
                delete cur;
                count--;
                renumber();
                return true;
            }
            prev = cur;
            cur = cur->next;
        }
        return false;
    }

    // Print every node once. O(n).
    void print() const {
        if (!head) {
            cout << "  (empty list)\n";
            return;
        }
        const Space* cur = head;
        do {
            cout << "  [" << cur->index << "] " << cur->name;
            if (cur->type == STREET || cur->type == RAILROAD || cur->type == UTILITY) {
                cout << "  $" << cur->cost << "  owner: "
                     << (cur->owner < 0 ? string("bank") : "P" + to_string(cur->owner + 1));
                if (cur->houses == 5) cout << "  hotel";
                else if (cur->houses > 0) cout << "  houses: " << cur->houses;
                if (cur->mortgaged) cout << "  (mortgaged)";
            }
            cout << '\n';
            cur = cur->next;
        } while (cur != head);
    }

    // Walk to a position. O(n).
    Space* at(int index) const {
        Space* cur = head;
        for (int i = 0; i < index; i++) cur = cur->next;
        return cur;
    }

    Space* first() const { return head; }
    int size() const { return count; }

    void clear() {
        for (int i = 0; i < count; i++) {
            Space* next = head->next;
            delete head;
            head = next;
        }
        head = tail = nullptr;
        count = 0;
    }

private:
    void renumber() {
        Space* cur = head;
        for (int i = 0; i < count; i++, cur = cur->next) cur->index = i;
    }
    Space* head = nullptr;
    Space* tail = nullptr;
    int count = 0;
};

void check(bool ok, const string& what) { cout << (ok ? "  PASS: " : "  FAIL: ") << what << '\n'; }

void testLinkedList() {
    cout << "=== PART 1: LINKED LIST TESTS ===\n";
    Board t;
    t.add({"Alpha", "A", STREET, 100, 0, 50, {1, 2, 3, 4, 5, 6}});
    t.add({"Beta", "B", STREET, 200, 0, 50, {1, 2, 3, 4, 5, 6}});
    t.add({"Gamma", "G", STREET, 300, 0, 50, {1, 2, 3, 4, 5, 6}});
    t.print();
    check(t.size() == 3, "add three items");
    check(t.search("Beta") && t.search("Beta")->cost == 200, "search finds Beta");
    check(t.search("Zeta") == nullptr, "search misses Zeta");
    check(t.remove("Beta") && !t.search("Beta") && t.size() == 2, "remove middle item");
    check(t.first()->next->next == t.first(), "still circular after removing middle");
    check(t.remove("Alpha") && t.first()->name == "Gamma", "remove head item");
    check(t.first()->next == t.first(), "single node points to itself");
    check(!t.remove("Zeta"), "removing a missing item returns false");
    t.print();
}

// =====================================================================
// PART 2: MONOPOLY
// =====================================================================
const SpaceInfo BOARD_DATA[] = {
    {"GO", "GO", GO_SPACE, 0, -1, 0, {}},
    {"Cedar Ave", "CEDR", STREET, 60, 0, 50, {2, 10, 30, 90, 160, 250}},
    {"Community Chest", "CHST", CHEST, 0, -1, 0, {}},
    {"Birch St", "BRCH", STREET, 60, 0, 50, {4, 20, 60, 180, 320, 450}},
    {"Income Tax", "TAX", TAX, 200, -1, 0, {}},
    {"North Rail", "RR1", RAILROAD, 200, 8, 0, {}},
    {"Harbor Rd", "HRBR", STREET, 100, 1, 50, {6, 30, 90, 270, 400, 550}},
    {"Chance", "CHNC", CHANCE, 0, -1, 0, {}},
    {"Pier Way", "PIER", STREET, 100, 1, 50, {6, 30, 90, 270, 400, 550}},
    {"Dock St", "DOCK", STREET, 120, 1, 50, {8, 40, 100, 300, 450, 600}},
    {"Jail", "JAIL", JAIL, 0, -1, 0, {}},
    {"Canyon Dr", "CNYN", STREET, 140, 2, 100, {10, 50, 150, 450, 625, 750}},
    {"Power Co", "PWR", UTILITY, 150, 9, 0, {}},
    {"Ridge Rd", "RDGE", STREET, 140, 2, 100, {10, 50, 150, 450, 625, 750}},
    {"Summit Ave", "SMIT", STREET, 160, 2, 100, {12, 60, 180, 500, 700, 900}},
    {"East Rail", "RR2", RAILROAD, 200, 8, 0, {}},
    {"Dusk Blvd", "DUSK", STREET, 180, 3, 100, {14, 70, 200, 550, 750, 950}},
    {"Community Chest", "CHST", CHEST, 0, -1, 0, {}},
    {"Ember St", "EMBR", STREET, 180, 3, 100, {14, 70, 200, 550, 750, 950}},
    {"Amber Way", "AMBR", STREET, 200, 3, 100, {16, 80, 220, 600, 800, 1000}},
    {"Free Parking", "FREE", FREE_PARKING, 0, -1, 0, {}},
    {"Ruby Rd", "RUBY", STREET, 220, 4, 150, {18, 90, 250, 700, 875, 1050}},
    {"Chance", "CHNC", CHANCE, 0, -1, 0, {}},
    {"Garnet Ave", "GRNT", STREET, 220, 4, 150, {18, 90, 250, 700, 875, 1050}},
    {"Cherry Ln", "CHRY", STREET, 240, 4, 150, {20, 100, 300, 750, 925, 1100}},
    {"South Rail", "RR3", RAILROAD, 200, 8, 0, {}},
    {"Gold St", "GOLD", STREET, 260, 5, 150, {22, 110, 330, 800, 975, 1150}},
    {"Lemon Ave", "LEMN", STREET, 260, 5, 150, {22, 110, 330, 800, 975, 1150}},
    {"Water Co", "WTR", UTILITY, 150, 9, 0, {}},
    {"Sunny Way", "SUNY", STREET, 280, 5, 150, {24, 120, 360, 850, 1025, 1200}},
    {"Go To Jail", "GO\nJAIL", GO_TO_JAIL, 0, -1, 0, {}},
    {"Fern Ave", "FERN", STREET, 300, 6, 200, {26, 130, 390, 900, 1100, 1275}},
    {"Ivy Ln", "IVY", STREET, 300, 6, 200, {26, 130, 390, 900, 1100, 1275}},
    {"Community Chest", "CHST", CHEST, 0, -1, 0, {}},
    {"Jade Blvd", "JADE", STREET, 320, 6, 200, {28, 150, 450, 1000, 1200, 1400}},
    {"West Rail", "RR4", RAILROAD, 200, 8, 0, {}},
    {"Chance", "CHNC", CHANCE, 0, -1, 0, {}},
    {"Ocean Dr", "OCEN", STREET, 350, 7, 200, {35, 175, 500, 1100, 1300, 1500}},
    {"Luxury Tax", "LUX", TAX, 100, -1, 0, {}},
    {"Skyline Pl", "SKY", STREET, 400, 7, 200, {50, 200, 600, 1400, 1700, 2000}},
};

const int BOARD_SIZE = 40, JAIL_INDEX = 10, GO_MONEY = 200, JAIL_FINE = 50, START_MONEY = 1500;

const SDL_Color GROUP_COLOR[10] = {
    {140, 85, 55, 255}, {150, 205, 240, 255}, {215, 60, 150, 255}, {245, 145, 30, 255}, {225, 35, 40, 255},
    {250, 220, 30, 255}, {30, 160, 80, 255}, {30, 80, 190, 255}, {60, 60, 60, 255}, {170, 170, 170, 255}};
const SDL_Color PLAYER_COLOR[2] = {{35, 125, 245, 255}, {235, 65, 75, 255}};

// ---------- drawing helpers ----------
// Tiny 3x5 font: no extra font library or font files needed.
const string GLYPHS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$-+:/.!?,()'=";
const char* FONT[] = {
    "111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
    "111100111001111", "111100111101111", "111001001001001", "111101111101111", "111101111001111",
    "010101111101101", "110101110101110", "011100100100011", "110101101101110", "111100110100111",
    "111100110100100", "011100101101011", "101101111101101", "111010010010111", "001001001101010",
    "101101110101101", "100100100100111", "101111111101101", "110101101101101", "010101101101010",
    "110101110100100", "010101101110011", "110101110101101", "011100010001110", "111010010010010",
    "101101101101111", "101101101101010", "101101111111101", "101101010101101", "101101010010010",
    "111001010100111", "011110010011110", "000000111000000", "000010111010000", "000010000010000",
    "001001010100100", "000000000000010", "010010010000010", "111001011000010", "000000000010100",
    "001010010010001", "100010010010100", "010010000000000", "000111000111000"};

int textWidth(const string& s, int size) { return s.empty() ? 0 : int(s.size()) * 4 * size - size; }

void text(SDL_Renderer* r, const string& words, int x, int y, int size) {
    for (char ch : words) {
        size_t g = GLYPHS.find(char(toupper(static_cast<unsigned char>(ch))));
        if (g != string::npos)
            for (int i = 0; i < 15; i++)
                if (FONT[g][i] == '1') {
                    SDL_Rect pixel = {x + i % 3 * size, y + i / 3 * size, size, size};
                    SDL_RenderFillRect(r, &pixel);
                }
        x += 4 * size;
    }
}

void centerText(SDL_Renderer* r, const string& s, int cx, int y, int size) {
    size_t start = 0;
    while (true) {
        size_t end = s.find('\n', start);
        string line = s.substr(start, end == string::npos ? string::npos : end - start);
        text(r, line, cx - textWidth(line, size) / 2, y, size);
        if (end == string::npos) break;
        start = end + 1;
        y += 6 * size + 1;
    }
}

void setColor(SDL_Renderer* r, SDL_Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }
void fill(SDL_Renderer* r, int x, int y, int w, int h) {
    SDL_Rect q = {x, y, w, h};
    SDL_RenderFillRect(r, &q);
}
void frame(SDL_Renderer* r, int x, int y, int w, int h, int thick) {
    for (int i = 0; i < thick; i++) {
        SDL_Rect q = {x + i, y + i, w - 2 * i, h - 2 * i};
        SDL_RenderDrawRect(r, &q);
    }
}

void drawDie(SDL_Renderer* r, int x, int y, int value) {
    static const char* PIPS[7] = {"000000000", "000010000", "100000001", "100010001",
                                  "101000101", "101010101", "101101101"};
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    fill(r, x, y, 56, 56);
    SDL_SetRenderDrawColor(r, 25, 45, 40, 255);
    frame(r, x, y, 56, 56, 2);
    for (int i = 0; i < 9; i++)
        if (PIPS[value][i] == '1') fill(r, x + 9 + (i % 3) * 15, y + 9 + (i / 3) * 15, 9, 9);
}

// Put each space around a circle, clockwise from the top.
SDL_Point position(int index, int radius = 330) {
    double angle = index * 2 * PI / BOARD_SIZE - PI / 2;
    return {360 + int(radius * cos(angle)), 360 + int(radius * sin(angle))};
}

// ---------- game rules ----------
enum Phase { ROLL, BUY, AUCTION, END, DEBT, OVER };
enum Action { A_ROLL, A_PAY_FINE, A_USE_CARD, A_BUY, A_AUCTION, A_BID10, A_BID50, A_BID100, A_DROP,
              A_END, A_BUILD, A_SELL, A_MORTGAGE, A_UNMORTGAGE, A_BANKRUPT };
enum CardKind { MOVE_TO, MONEY, CARD_JAIL, JAIL_FREE, BACK_THREE, REPAIRS, PAY_EACH, COLLECT_EACH };

struct Card { string text; CardKind kind; int a, b, holder; };
struct Deck { vector<Card> cards; vector<int> order; int next = 0; };
struct Button { SDL_Rect rect; string label; Action action; };

string P(int p) { return "P" + to_string(p + 1); }
string cash(int m) { return "$" + to_string(m); }

class Game {
public:
    Board board;
    Space* pos[2];
    int money[2] = {START_MONEY, START_MONEY};
    bool inJail[2] = {false, false};
    int jailTries[2] = {0, 0};
    int creditor[2] = {-1, -1};  // who a player last paid (-1 = bank), used for bankruptcy
    int turn = 0, turnNumber = 1, doublesCount = 0, die1 = 1, die2 = 1;
    bool extraRoll = false;
    Phase phase = ROLL, returnPhase = END;
    int debtor = 0, winner = -1;
    int bid = 0, bidder = 0, highBidder = -1, noBidDrops = 0;
    Space* selected;
    Deck chance, chest;
    deque<string> logLines;

    Game() {
        for (const SpaceInfo& info : BOARD_DATA) board.add(info);
        pos[0] = pos[1] = selected = board.first();
        chance.cards = {
            {"ADVANCE TO GO", MOVE_TO, 0, 0, -1},
            {"ADVANCE TO SKYLINE PL", MOVE_TO, 39, 0, -1},
            {"ADVANCE TO RUBY RD", MOVE_TO, 21, 0, -1},
            {"ADVANCE TO NORTH RAIL", MOVE_TO, 5, 0, -1},
            {"GO BACK 3 SPACES", BACK_THREE, 0, 0, -1},
            {"GO TO JAIL", CARD_JAIL, 0, 0, -1},
            {"GET OUT OF JAIL FREE", JAIL_FREE, 0, 0, -1},
            {"BANK PAYS YOU $50", MONEY, 50, 0, -1},
            {"SPEEDING FINE: PAY $15", MONEY, -15, 0, -1},
            {"REPAIRS: $25 PER HOUSE, $100 PER HOTEL", REPAIRS, 25, 100, -1},
            {"CHAIRMAN: PAY EACH PLAYER $50", PAY_EACH, 50, 0, -1}};
        chest.cards = {
            {"ADVANCE TO GO", MOVE_TO, 0, 0, -1},
            {"BANK ERROR: COLLECT $200", MONEY, 200, 0, -1},
            {"DOCTOR FEE: PAY $50", MONEY, -50, 0, -1},
            {"GO TO JAIL", CARD_JAIL, 0, 0, -1},
            {"GET OUT OF JAIL FREE", JAIL_FREE, 0, 0, -1},
            {"TAX REFUND: COLLECT $20", MONEY, 20, 0, -1},
            {"HOSPITAL: PAY $100", MONEY, -100, 0, -1},
            {"BIRTHDAY: COLLECT $10 FROM EACH", COLLECT_EACH, 10, 0, -1},
            {"STREET REPAIRS: $40 PER HOUSE, $115 PER HOTEL", REPAIRS, 40, 115, -1},
            {"INHERITANCE: COLLECT $100", MONEY, 100, 0, -1}};
        shuffleDeck(chance);
        shuffleDeck(chest);
        log("--- TURN 1: P1 ---");
    }

    int actor() const { return phase == DEBT ? debtor : phase == AUCTION ? bidder : turn; }

    // Prints to the console and keeps the last lines for the on-screen log.
    void log(const string& msg) {
        cout << msg << '\n';
        string m = msg;
        for (char& c : m) c = char(toupper(static_cast<unsigned char>(c)));
        while (m.size() > 34) {
            size_t cut = m.rfind(' ', 34);
            if (cut == string::npos || cut == 0) cut = 34;
            logLines.push_back(m.substr(0, cut));
            m = "  " + m.substr(cut + (m[cut] == ' ' ? 1 : 0));
        }
        logLines.push_back(m);
        while (logLines.size() > 20) logLines.pop_front();
    }

    void shuffleDeck(Deck& d) {
        d.order.clear();
        for (int i = 0; i < int(d.cards.size()); i++) d.order.push_back(i);
        for (int i = int(d.order.size()) - 1; i > 0; i--) swap(d.order[i], d.order[rand() % (i + 1)]);
        d.next = 0;
    }

    int cardsHeld(int p) const {
        int n = 0;
        for (const Card& c : chance.cards) n += c.holder == p;
        for (const Card& c : chest.cards) n += c.holder == p;
        return n;
    }

    // ----- property queries (each walks the list once: O(n)) -----
    bool ownsGroup(int p, int g) const {
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next)
            if (s->group == g && s->owner != p) return false;
        return true;
    }
    int countOwned(int p, int g) const {
        int n = 0;
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next) n += s->group == g && s->owner == p;
        return n;
    }
    int groupMinHouses(int g) const {
        int m = 5;
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next)
            if (s->group == g && s->houses < m) m = s->houses;
        return m;
    }
    int groupMaxHouses(int g) const {
        int m = 0;
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next)
            if (s->group == g && s->houses > m) m = s->houses;
        return m;
    }
    bool groupMortgaged(int g) const {
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next)
            if (s->group == g && s->mortgaged) return true;
        return false;
    }
    int propsOwned(int p) const {
        int n = 0;
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next) n += s->owner == p;
        return n;
    }
    int netWorth(int p) const {
        int total = money[p];
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next)
            if (s->owner == p) total += (s->mortgaged ? s->cost / 2 : s->cost) + s->houses * s->houseCost;
        return total;
    }
    // Most cash a player could raise by selling buildings and mortgaging.
    int raisable(int p) const {
        int total = 0;
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next)
            if (s->owner == p) total += s->houses * s->houseCost / 2 + (s->mortgaged ? 0 : s->cost / 2);
        return total;
    }

    int rentFor(const Space* s) const {
        if (s->type == STREET) {
            if (s->houses > 0) return s->rent[s->houses];
            return ownsGroup(s->owner, s->group) ? s->rent[0] * 2 : s->rent[0];  // full set doubles base rent
        }
        int n = countOwned(s->owner, s->group);
        if (s->type == RAILROAD) return 25 << (n - 1);  // 25, 50, 100, 200
        return (die1 + die2) * (n == 2 ? 10 : 4);       // utilities
    }
    int unmortgageCost(const Space* s) const { return (s->cost / 2 * 11 + 9) / 10; }  // mortgage + 10%

    // ----- what the acting player may do with the selected space -----
    bool ownedByActor(const Space* s) const { return s && s->owner >= 0 && s->owner == actor(); }
    bool canBuild(const Space* s) const {
        return (phase == ROLL || phase == END) && ownedByActor(s) && s->type == STREET &&
               ownsGroup(s->owner, s->group) && !groupMortgaged(s->group) && s->houses < 5 &&
               s->houses == groupMinHouses(s->group) && money[s->owner] >= s->houseCost;
    }
    bool canSell(const Space* s) const {
        return (phase == ROLL || phase == BUY || phase == END || phase == DEBT) && ownedByActor(s) &&
               s->houses > 0 && s->houses == groupMaxHouses(s->group);
    }
    bool canMortgage(const Space* s) const {
        return (phase == ROLL || phase == BUY || phase == END || phase == DEBT) && ownedByActor(s) &&
               !s->mortgaged && (s->type != STREET || groupMaxHouses(s->group) == 0);
    }
    bool canUnmortgage(const Space* s) const {
        return (phase == ROLL || phase == END) && ownedByActor(s) && s->mortgaged &&
               money[s->owner] >= unmortgageCost(s);
    }

    // ----- money and movement -----
    void pay(int from, int to, int amount) {  // -1 means the bank
        if (from >= 0) {
            money[from] -= amount;
            creditor[from] = to;
        }
        if (to >= 0) money[to] += amount;
    }

    void moveForward(int steps) {  // O(steps): one pointer hop per space
        for (int i = 0; i < steps; i++) {
            pos[turn] = pos[turn]->next;
            if (pos[turn] == board.first()) {
                pay(-1, turn, GO_MONEY);
                log(P(turn) + " PASSED GO +$200");
            }
        }
    }

    void sendToJail() {
        pos[turn] = board.at(JAIL_INDEX);  // straight to jail: no GO money
        inJail[turn] = true;
        jailTries[turn] = 0;
        doublesCount = 0;
        extraRoll = false;
        phase = END;
        log(P(turn) + " GOES TO JAIL");
    }

    void landOn() {
        Space* s = pos[turn];
        selected = s;
        phase = END;
        log(P(turn) + " LANDED ON " + s->name);
        switch (s->type) {
        case TAX:
            pay(turn, -1, s->cost);
            log(P(turn) + " PAID " + cash(s->cost) + " TAX");
            break;
        case GO_TO_JAIL: sendToJail(); break;
        case CHANCE: drawCard(chance, "CHANCE"); break;
        case CHEST: drawCard(chest, "CHEST"); break;
        case JAIL:
            if (!inJail[turn]) log("JUST VISITING");
            break;
        case STREET:
        case RAILROAD:
        case UTILITY:
            if (s->owner < 0) phase = BUY;
            else if (s->owner != turn && s->mortgaged) log("MORTGAGED, NO RENT");
            else if (s->owner != turn) {
                int rent = rentFor(s);
                pay(turn, s->owner, rent);
                log(P(turn) + " PAID " + P(s->owner) + " " + cash(rent) + " RENT");
            }
            break;
        default: break;
        }
    }

    void drawCard(Deck& d, const string& deckName) {
        int idx;
        do {  // skip a Get Out of Jail Free card someone is holding
            idx = d.order[d.next];
            d.next = (d.next + 1) % int(d.order.size());
        } while (d.cards[idx].holder >= 0);
        Card& c = d.cards[idx];
        log(deckName + ": " + c.text);
        switch (c.kind) {
        case MOVE_TO:
            moveForward((c.a - pos[turn]->index + BOARD_SIZE) % BOARD_SIZE);
            landOn();
            break;
        case MONEY:
            if (c.a >= 0) pay(-1, turn, c.a);
            else pay(turn, -1, -c.a);
            break;
        case CARD_JAIL: sendToJail(); break;
        case JAIL_FREE: c.holder = turn; break;
        case BACK_THREE:  // singly linked: going back 3 = going forward 37, without passing GO
            for (int i = 0; i < BOARD_SIZE - 3; i++) pos[turn] = pos[turn]->next;
            landOn();
            break;
        case REPAIRS: {
            int houses = 0, hotels = 0;
            const Space* s = board.first();
            for (int i = 0; i < board.size(); i++, s = s->next)
                if (s->owner == turn) {
                    if (s->houses == 5) hotels++;
                    else houses += s->houses;
                }
            int cost = houses * c.a + hotels * c.b;
            pay(turn, -1, cost);
            log(P(turn) + " PAID " + cash(cost) + " FOR REPAIRS");
            break;
        }
        case PAY_EACH: pay(turn, 1 - turn, c.a); break;
        case COLLECT_EACH: pay(1 - turn, turn, c.a); break;
        }
    }

    void roll() {
        die1 = rand() % 6 + 1;
        die2 = rand() % 6 + 1;
        bool doubles = die1 == die2;
        log(P(turn) + " ROLLED " + to_string(die1) + " + " + to_string(die2) + (doubles ? " DOUBLES" : ""));
        if (inJail[turn]) {
            if (doubles) {
                inJail[turn] = false;
                log(P(turn) + " ROLLED OUT OF JAIL");
            } else if (++jailTries[turn] == 3) {
                pay(turn, -1, JAIL_FINE);
                inJail[turn] = false;
                log("3RD MISS: " + P(turn) + " PAYS $50 AND MOVES");
            } else {
                log("NO DOUBLES, STAYS IN JAIL (" + to_string(jailTries[turn]) + "/3)");
                extraRoll = false;
                phase = END;
                return;
            }
            moveForward(die1 + die2);
            landOn();
            extraRoll = false;  // leaving jail on doubles does not give another roll
            return;
        }
        if (doubles && ++doublesCount == 3) {
            log("3 DOUBLES IN A ROW!");
            sendToJail();
            return;
        }
        moveForward(die1 + die2);
        landOn();
        extraRoll = doubles && !inJail[turn];
    }

    void printStandings() const {
        cout << "  [after turn " << turnNumber << "] ";
        for (int p = 0; p < 2; p++)
            cout << P(p) << " $" << money[p] << ", " << propsOwned(p) << " properties, net worth $"
                 << netWorth(p) << (inJail[p] ? " (in jail)" : "") << (p == 0 ? "  |  " : "\n");
    }

    void endTurn() {
        printStandings();
        turn = 1 - turn;
        turnNumber++;
        doublesCount = 0;
        extraRoll = false;
        phase = ROLL;
        selected = pos[turn];
        log("--- TURN " + to_string(turnNumber) + ": " + P(turn) + " ---");
    }

    void bankrupt(int p) {
        int to = creditor[p];
        log(P(p) + " IS BANKRUPT!");
        int buildingCash = 0;
        Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next)
            if (s->owner == p) {
                buildingCash += s->houses * s->houseCost / 2;  // buildings go back to the bank at half price
                s->houses = 0;
                s->owner = to;                                 // creditor (or bank) takes the deeds
                if (to < 0) s->mortgaged = false;
            }
        if (to >= 0) money[to] += money[p] + buildingCash;     // creditor only gets what the debtor really had
        money[p] = 0;
        winner = 1 - p;
        phase = OVER;
        log(P(winner) + " WINS THE GAME!");
        cout << "\n=== GAME OVER ===\n";
        printStandings();
        cout << "Final board:\n";
        board.print();
    }

    // A player below $0 must sell/mortgage; if even that can't cover it, they're out.
    void checkDebt() {
        if (phase == OVER) return;
        for (int p : {turn, 1 - turn}) {
            if (money[p] >= 0) continue;
            if (money[p] + raisable(p) < 0) {
                bankrupt(p);
                return;
            }
            if (phase != DEBT) {
                returnPhase = phase;
                log(P(p) + " OWES " + cash(-money[p]) + ": SELL OR MORTGAGE");
            }
            phase = DEBT;
            debtor = p;
            return;
        }
        if (phase == DEBT) {
            phase = returnPhase;
            log("DEBT PAID OFF");
        }
    }

    void doAction(Action a) {
        if (phase == OVER) return;
        int me = actor();
        Space* here = pos[turn];
        Space* s = selected;
        switch (a) {
        case A_ROLL: roll(); break;
        case A_PAY_FINE:
            pay(turn, -1, JAIL_FINE);
            inJail[turn] = false;
            log(P(turn) + " PAID $50 TO LEAVE JAIL");
            break;
        case A_USE_CARD: {
            bool used = false;
            for (Deck* d : {&chance, &chest})
                for (Card& c : d->cards)
                    if (!used && c.holder == turn) {
                        c.holder = -1;
                        used = true;
                    }
            inJail[turn] = false;
            log(P(turn) + " USED GET OUT OF JAIL FREE");
            break;
        }
        case A_BUY:
            pay(turn, -1, here->cost);
            here->owner = turn;
            phase = END;
            log(P(turn) + " BOUGHT " + here->name + " FOR " + cash(here->cost));
            break;
        case A_AUCTION:
            log(P(turn) + " PASSED. AUCTION FOR " + here->name);
            phase = AUCTION;
            bid = 0;
            highBidder = -1;
            bidder = 1 - turn;
            noBidDrops = 0;
            break;
        case A_BID10:
        case A_BID50:
        case A_BID100:
            bid += a == A_BID10 ? 10 : a == A_BID50 ? 50 : 100;
            highBidder = bidder;
            log(P(bidder) + " BIDS " + cash(bid));
            bidder = 1 - bidder;
            break;
        case A_DROP:
            log(P(bidder) + " DROPS OUT");
            if (highBidder >= 0) {
                pay(highBidder, -1, bid);
                here->owner = highBidder;
                phase = END;
                log(P(highBidder) + " WINS " + here->name + " FOR " + cash(bid));
            } else if (++noBidDrops == 2) {
                log("NO BIDS. BANK KEEPS IT");
                phase = END;
            } else bidder = 1 - bidder;
            break;
        case A_END: endTurn(); break;
        case A_BUILD:
            pay(me, -1, s->houseCost);
            s->houses++;
            log(P(me) + (s->houses == 5 ? " BUILT A HOTEL ON " : " BUILT A HOUSE ON ") + s->name);
            break;
        case A_SELL:
            log(P(me) + (s->houses == 5 ? " SOLD A HOTEL ON " : " SOLD A HOUSE ON ") + s->name);
            s->houses--;
            pay(-1, me, s->houseCost / 2);
            break;
        case A_MORTGAGE:
            s->mortgaged = true;
            pay(-1, me, s->cost / 2);
            log(P(me) + " MORTGAGED " + s->name + " +" + cash(s->cost / 2));
            break;
        case A_UNMORTGAGE:
            pay(me, -1, unmortgageCost(s));
            s->mortgaged = false;
            log(P(me) + " UNMORTGAGED " + s->name);
            break;
        case A_BANKRUPT: bankrupt(me); return;
        }
        checkDebt();
    }

    vector<Button> buttons() const {
        vector<Button> list;
        auto add = [&](const string& label, Action a) {
            int i = int(list.size());
            list.push_back({{736 + (i % 2) * 146, 268 + (i / 2) * 38, 138, 30}, label, a});
        };
        switch (phase) {
        case ROLL:
            if (inJail[turn]) {
                add("ROLL DOUBLES", A_ROLL);
                if (money[turn] >= JAIL_FINE) add("PAY $50", A_PAY_FINE);
                if (cardsHeld(turn) > 0) add("USE CARD", A_USE_CARD);
            } else add("ROLL DICE", A_ROLL);
            break;
        case BUY:
            if (money[turn] >= pos[turn]->cost) add("BUY " + cash(pos[turn]->cost), A_BUY);
            add("AUCTION", A_AUCTION);
            break;
        case AUCTION:
            if (money[bidder] >= bid + 10) add("BID +$10", A_BID10);
            if (money[bidder] >= bid + 50) add("BID +$50", A_BID50);
            if (money[bidder] >= bid + 100) add("BID +$100", A_BID100);
            add("DROP OUT", A_DROP);
            break;
        case END: add(extraRoll ? "ROLL AGAIN" : "END TURN", extraRoll ? A_ROLL : A_END); break;
        default: break;
        }
        const Space* s = selected;
        if (canBuild(s)) add(s->houses == 4 ? "BUILD HOTEL" : "BUILD HOUSE", A_BUILD);
        if (canSell(s)) add(s->houses == 5 ? "SELL HOTEL" : "SELL HOUSE", A_SELL);
        if (canMortgage(s)) add("MORTGAGE +" + cash(s->cost / 2), A_MORTGAGE);
        if (canUnmortgage(s)) add("UNMORT " + cash(unmortgageCost(s)), A_UNMORTGAGE);
        if (phase == DEBT) add("BANKRUPT", A_BANKRUPT);
        return list;
    }

    void primaryAction() {  // SPACE key
        if (phase == DEBT || phase == OVER) return;
        vector<Button> b = buttons();
        if (!b.empty()) doAction(b[0].action);
    }

    // A simple computer player, handy for simulating turns quickly.
    void autoStep() {
        switch (phase) {
        case ROLL: doAction(inJail[turn] && cardsHeld(turn) > 0 ? A_USE_CARD : A_ROLL); break;
        case BUY: doAction(money[turn] >= pos[turn]->cost ? A_BUY : A_AUCTION); break;
        case AUCTION:
            doAction(bid + 10 <= pos[turn]->cost && money[bidder] - bid - 10 >= 100 ? A_BID10 : A_DROP);
            break;
        case END: {
            bool built = true;
            while (built && phase == END) {
                built = false;
                Space* s = board.first();
                for (int i = 0; i < board.size(); i++, s = s->next) {
                    selected = s;
                    if (canBuild(s) && money[turn] - s->houseCost >= 150) {
                        doAction(A_BUILD);
                        built = true;
                    }
                }
            }
            selected = pos[turn];
            doAction(extraRoll ? A_ROLL : A_END);
            break;
        }
        case DEBT:
            while (phase == DEBT) {  // sell buildings first, then mortgage
                Space* pick = nullptr;
                Action act = A_BANKRUPT;
                Space* s = board.first();
                for (int i = 0; i < board.size() && !pick; i++, s = s->next)
                    if (canSell(s)) pick = s, act = A_SELL;
                s = board.first();
                for (int i = 0; i < board.size() && !pick; i++, s = s->next)
                    if (canMortgage(s)) pick = s, act = A_MORTGAGE;
                if (pick) selected = pick;
                doAction(act);
            }
            break;
        case OVER: break;
        }
    }

    void clickBoard(int x, int y) {
        Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next) {
            SDL_Point p = position(s->index);
            if (abs(x - p.x) <= 17 && abs(y - p.y) <= 17) {
                selected = s;
                return;
            }
        }
    }

    string phaseText() const {
        switch (phase) {
        case ROLL:
            return inJail[turn] ? P(turn) + " IN JAIL: TRY " + to_string(jailTries[turn] + 1) + "/3"
                                : P(turn) + ": ROLL THE DICE";
        case BUY: return P(turn) + ": BUY IT OR AUCTION IT";
        case AUCTION:
            return "BID " + cash(bid) + (highBidder >= 0 ? " BY " + P(highBidder) : "") + ", " + P(bidder) +
                   " TO ACT";
        case END: return extraRoll ? "DOUBLES! " + P(turn) + " ROLLS AGAIN" : P(turn) + ": BUILD OR END TURN";
        case DEBT: return P(debtor) + " MUST RAISE " + cash(-money[debtor]);
        case OVER: return "GAME OVER: " + P(winner) + " WINS";
        }
        return "";
    }

    vector<string> infoLines(const Space* s) const {
        vector<string> out;
        string owner = s->owner < 0 ? "BANK" : P(s->owner);
        string ownerLine = "OWNER " + owner + (s->mortgaged ? "  MORTGAGED" : "");
        switch (s->type) {
        case STREET:
            out.push_back("PRICE " + cash(s->cost) + "  HOUSE " + cash(s->houseCost));
            out.push_back(ownerLine);
            out.push_back("RENT " + cash(s->rent[0]) + "  FULL SET " + cash(s->rent[0] * 2));
            out.push_back("1H " + cash(s->rent[1]) + "  2H " + cash(s->rent[2]) + "  3H " + cash(s->rent[3]));
            out.push_back("4H " + cash(s->rent[4]) + "  HOTEL " + cash(s->rent[5]));
            out.push_back(s->houses == 5 ? "BUILT: HOTEL" : "BUILT: " + to_string(s->houses) + " HOUSES");
            break;
        case RAILROAD:
            out.push_back("PRICE " + cash(s->cost));
            out.push_back(ownerLine);
            out.push_back("RENT $25 / 50 / 100 / 200");
            out.push_back("BY NUMBER OF RAILROADS OWNED");
            break;
        case UTILITY:
            out.push_back("PRICE " + cash(s->cost));
            out.push_back(ownerLine);
            out.push_back("RENT 4X DICE, 10X IF BOTH OWNED");
            break;
        case TAX: out.push_back("PAY " + cash(s->cost) + " TO THE BANK"); break;
        case GO_SPACE: out.push_back("COLLECT $200 EVERY TIME"); out.push_back("YOU PASS OR LAND ON GO"); break;
        case JAIL:
            out.push_back("JUST VISITING IF YOU LAND HERE");
            out.push_back("GET OUT: ROLL DOUBLES, PAY $50,");
            out.push_back("OR USE A GET OUT OF JAIL CARD");
            out.push_back("MUST PAY $50 AFTER 3 MISSES");
            break;
        case GO_TO_JAIL: out.push_back("GO DIRECTLY TO JAIL"); out.push_back("DO NOT PASS GO, NO $200"); break;
        case FREE_PARKING: out.push_back("NOTHING HAPPENS HERE"); break;
        case CHANCE:
        case CHEST: out.push_back("DRAW A CARD AND FOLLOW IT"); break;
        }
        if (s->owner >= 0 && !s->mortgaged && s->type != UTILITY) out.push_back("RENT NOW " + cash(rentFor(s)));
        return out;
    }

    void drawTile(SDL_Renderer* r, const Space* s) const {
        SDL_Point p = position(s->index);
        int x = p.x - 17, y = p.y - 17;
        if (s->mortgaged) SDL_SetRenderDrawColor(r, 150, 150, 140, 255);
        else SDL_SetRenderDrawColor(r, 239, 237, 219, 255);
        fill(r, x, y, 34, 34);
        if (s->group >= 0) {
            setColor(r, GROUP_COLOR[s->group]);
            fill(r, x, y, 34, 6);
        }
        SDL_SetRenderDrawColor(r, 25, 45, 40, 255);
        bool twoLines = s->label.find('\n') != string::npos;
        centerText(r, s->label, p.x, twoLines ? p.y - 12 : p.y - 5, 2);
        if (s->houses == 5) {
            SDL_SetRenderDrawColor(r, 210, 30, 40, 255);
            fill(r, p.x - 7, y + 26, 14, 5);
        } else {
            SDL_SetRenderDrawColor(r, 30, 150, 60, 255);
            for (int i = 0; i < s->houses; i++) fill(r, x + 4 + i * 7, y + 26, 5, 5);
        }
        if (s->owner >= 0) {
            setColor(r, PLAYER_COLOR[s->owner]);
            frame(r, x, y, 34, 34, 2);
        }
        if (s == selected) {
            SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
            frame(r, x - 4, y - 4, 42, 42, 2);
        }
    }

    void draw(SDL_Renderer* r) const {
        SDL_SetRenderDrawColor(r, 22, 65, 56, 255);
        SDL_RenderClear(r);

        // Circular path behind the spaces.
        SDL_SetRenderDrawColor(r, 110, 150, 130, 255);
        for (int i = 0; i < BOARD_SIZE; i++) {
            SDL_Point a = position(i), b = position((i + 1) % BOARD_SIZE);
            SDL_RenderDrawLine(r, a.x, a.y, b.x, b.y);
        }

        // Walk the linked list to draw the board.
        const Space* s = board.first();
        for (int i = 0; i < board.size(); i++, s = s->next) drawTile(r, s);

        // Player tokens (jailed players get bars).
        for (int j = 0; j < 2; j++) {
            if (phase == OVER && j != winner) continue;
            SDL_Point p = position(pos[j]->index, 290);
            int x = p.x - 14 + j * 16, y = p.y - 6;
            setColor(r, PLAYER_COLOR[j]);
            fill(r, x, y, 12, 12);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            frame(r, x, y, 12, 12, 1);
            if (inJail[j]) {
                SDL_SetRenderDrawColor(r, 25, 45, 40, 255);
                fill(r, x + 3, y, 2, 12);
                fill(r, x + 7, y, 2, 12);
            }
        }

        // Center: turn, whose move, dice, hints.
        SDL_SetRenderDrawColor(r, 170, 205, 185, 255);
        centerText(r, "TURN " + to_string(turnNumber), 360, 196, 2);
        if (phase == OVER) {
            setColor(r, PLAYER_COLOR[winner]);
            centerText(r, P(winner) + " WINS!", 360, 222, 6);
        } else {
            setColor(r, PLAYER_COLOR[actor()]);
            string who = phase == DEBT ? " IN DEBT" : phase == AUCTION ? " TO BID" : " TO PLAY";
            centerText(r, P(actor()) + who, 360, 226, 4);
        }
        drawDie(r, 296, 326, die1);
        drawDie(r, 368, 326, die2);
        SDL_SetRenderDrawColor(r, 170, 205, 185, 255);
        centerText(r, "SPACE: MAIN ACTION", 360, 410, 2);
        centerText(r, "A: AUTO-PLAY ONE STEP", 360, 426, 2);
        centerText(r, "CLICK A SPACE TO INSPECT", 360, 442, 2);

        // Side panel.
        SDL_SetRenderDrawColor(r, 14, 42, 37, 255);
        fill(r, 720, 0, 320, 720);
        for (int p = 0; p < 2; p++) {
            int y = 14 + p * 56;
            if (p == turn && phase != OVER) {
                SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
                fill(r, 726, y, 4, 34);
            }
            setColor(r, PLAYER_COLOR[p]);
            text(r, P(p) + "  " + cash(money[p]), 736, y, 3);
            SDL_SetRenderDrawColor(r, 200, 215, 205, 255);
            string status = to_string(propsOwned(p)) + " PROPS  WORTH " + cash(netWorth(p));
            if (inJail[p]) status += "  JAIL";
            if (cardsHeld(p) > 0) status += "  CARD";
            text(r, status, 736, y + 22, 2);
        }
        SDL_SetRenderDrawColor(r, 60, 100, 88, 255);
        fill(r, 730, 126, 300, 1);

        if (selected) {
            if (selected->group >= 0) {
                setColor(r, GROUP_COLOR[selected->group]);
                fill(r, 736, 138, 10, 10);
            }
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            text(r, selected->name, 752, 138, 2);
            SDL_SetRenderDrawColor(r, 200, 215, 205, 255);
            int y = 158;
            for (const string& line : infoLines(selected)) {
                text(r, line, 736, y, 2);
                y += 14;
            }
        }

        for (const Button& b : buttons()) {
            SDL_SetRenderDrawColor(r, 236, 233, 210, 255);
            SDL_RenderFillRect(r, &b.rect);
            SDL_SetRenderDrawColor(r, 25, 45, 40, 255);
            centerText(r, b.label, b.rect.x + b.rect.w / 2, b.rect.y + 10, 2);
        }

        SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
        text(r, phaseText(), 736, 392, 2);
        SDL_SetRenderDrawColor(r, 60, 100, 88, 255);
        fill(r, 730, 410, 300, 1);
        SDL_SetRenderDrawColor(r, 200, 215, 205, 255);
        int y = 420;
        for (const string& line : logLines) {
            text(r, line, 736, y, 2);
            y += 14;
        }
    }
};

int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    testLinkedList();

    cout << "\n=== PART 2: MONOPOLY ===\n";
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        cerr << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Monopoly | SPACE: action | A: auto | click spaces to inspect",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1040, 720, 0);
    SDL_Renderer* r = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : nullptr;
    if (!r) {
        cerr << SDL_GetError() << '\n';
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Game game;
    cout << "Board (circular linked list):\n";
    game.board.print();
    cout << "\n--- TURN 1: P1 ---\n";

    SDL_Rect diceArea = {296, 326, 128, 56};
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE && !event.key.repeat) game.primaryAction();
                if (event.key.keysym.sym == SDLK_a) game.autoStep();  // hold A to fast-forward
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_Point mouse = {event.button.x, event.button.y};
                bool handled = false;
                for (const Button& b : game.buttons())
                    if (SDL_PointInRect(&mouse, &b.rect)) {
                        game.doAction(b.action);
                        handled = true;
                        break;
                    }
                if (!handled && SDL_PointInRect(&mouse, &diceArea)) {
                    if (game.phase == ROLL || (game.phase == END && game.extraRoll)) game.doAction(A_ROLL);
                    handled = true;
                }
                if (!handled) game.clickBoard(mouse.x, mouse.y);
            }
        }
        game.draw(r);
        SDL_RenderPresent(r);
        SDL_Delay(16);
    }

    if (game.phase != OVER) {
        cout << "\n=== WINDOW CLOSED: FINAL RESULTS ===\n";
        game.printStandings();
        cout << "Final board:\n";
        game.board.print();
    }
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(window);
    SDL_Quit();
    // The Board destructor deletes each node exactly once (it stops after count nodes).
}
