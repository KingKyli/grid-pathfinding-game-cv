#include "Map.h"
#include <iterator>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Για το demo χρησιμοποιείται ένα πολύ μικρό/"χειροποίητο" parsing JSON.
// Αν το project μεγαλώσει, έχει νόημα να μπει κανονική JSON βιβλιοθήκη.

namespace grid {

bool Map::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if(!in) return false;
    // Πολύ μικρός custom parser για το format `maps/*.json` του παραδείγματος.
    // Περιμένει πεδία: width, height, grid (array από strings τύπου "..##..").
    std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    size_t wpos = all.find("\"width\"");
    size_t hpos = all.find("\"height\"");
    if(wpos==std::string::npos || hpos==std::string::npos) return false;
    // Πρόχειρη εξαγωγή αριθμού.
    auto extractInt = [&](size_t pos)->int{
        size_t colon = all.find(':', pos);
        if(colon==std::string::npos) return 0;
        size_t start = all.find_first_of("0123456789", colon);
        size_t end = all.find_first_not_of("0123456789", start);
        return std::stoi(all.substr(start, end-start));
    };
    w = extractInt(wpos);
    h = extractInt(hpos);
    cells.assign(w*h, 0);
    // Διάβασε τις γραμμές μέσα στο grid array.
    size_t gridPos = all.find("\"grid\"");
    if(gridPos==std::string::npos) return true; // no grid αρα empty
    size_t bracket = all.find('[', gridPos);
    size_t cur = bracket+1;
    int row = 0;
    while(cur < all.size() && row < h) {
        size_t quote1 = all.find('"', cur);
        if(quote1==std::string::npos) break;
        size_t quote2 = all.find('"', quote1+1);
        if(quote2==std::string::npos) break;
        std::string line = all.substr(quote1+1, quote2-quote1-1);
        for(int x=0; x<std::min((int)line.size(), w); ++x) {
            cells[row*w + x] = (line[x] == '#') ? 1 : 0;
        }
        row++;
        cur = quote2+1;
    }
    return true;
}

bool Map::isFree(const Point& p) const {
    if(!inBounds(p)) return false;
    return cells[index(p)] == 0;
}

bool Map::setCell(const Point& p, int value) {
    if (!inBounds(p)) return false;
    cells[index(p)] = value;
    return true;
}

std::vector<Point> Map::neighbors(const Point& p) const {
    std::vector<Point> out;
    static const int dx[4] = {1,-1,0,0};
    static const int dy[4] = {0,0,1,-1};
    for(int i=0;i<4;++i) {
        Point n{p.x + dx[i], p.y + dy[i]};
        if(inBounds(n) && isFree(n)) out.push_back(n);
    }
    return out;
}

} 
