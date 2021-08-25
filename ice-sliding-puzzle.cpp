// ----------------------------------------------------------------------------
// Ice sliding puzzle maker
// ----------------------------------------------------------------------------

#include <iostream>
#include <random>
#include <limits.h>
#include <assert.h>

//const int MAX_W = 16;
//const int MAX_H = 16;
const int MAX_W = 32;
const int MAX_H = 32;
const int UNREACHABLE = MAX_W * MAX_H + 1;

std::default_random_engine rng;
int random_range(int n) {
  std::uniform_int_distribution<int> distribution(0,n-1);
  return distribution(rng);
}

// ----------------------------------------------------------------------------
// Puzzles + solver
// ----------------------------------------------------------------------------

// We encode coordinates as x + y * MAX_W
struct Coord {
private:
  int pos;
public:
  inline Coord() {}
  inline constexpr Coord(int pos) : pos(pos) {}
  inline constexpr Coord(int x, int y) : pos(x + y*MAX_W) {}
  inline constexpr operator int() const { return pos; }
  inline constexpr int row() const { return (pos / MAX_W); }
  inline constexpr int col() const { return pos % MAX_W; }
  
  inline constexpr Coord next(int w) {
    int next = pos + 1;
    if (next % MAX_W == w) {
      next = next - w + MAX_W;
    }
    return next;
  }
};

// A puzzle is a grid of obstacles, with a start point.
// We don't need to store the end point, because we calculate the distance to all points.
struct Puzzle {
  bool grid[MAX_W*MAX_H];
  int w,h;
  Coord start = 0;

  // iterator over all coordinates in the grid
  struct iterator {
  private:
    int pos, w;
  public:
    iterator(int pos, int w) : pos(pos), w(w) {}
    
    void operator ++() {
      ++pos;
      if (pos % MAX_W == w) {
        pos = pos - w + MAX_W;
      }
    }
    Coord operator * () const {
      return pos;
    }
    bool operator == (iterator const& that) const {
      return pos == that.pos;
    }
    bool operator != (iterator const& that) const {
      return pos != that.pos;
    }
  };

  using const_iterator = iterator;
  iterator begin() const {
    return iterator(0,w);
  }
  iterator end() const {
    return iterator(h * MAX_W, w);
  }
  
  inline bool operator [] (Coord pos) const {
    return grid[pos];
  }
  inline bool& operator [] (Coord pos) {
    return grid[pos];
  }
  void clear() {
    std::fill_n(grid, MAX_W*MAX_H, false);
  }
  
  Puzzle(int w, int h) : w(w), h(h) {
    clear();
  }
  Puzzle(std::initializer_list<std::string> const& data) {
    w = 0;
    h = 0;
    int y = 0;
    for (std::string const& row : data) {
      if (h == 0) {
        w = (int)row.size();
      } else {
        assert(w == (int)row.size());
      }
      for (int x=0; x<w; ++x) {
        char c = row[x];
        Coord pos = Coord(x,y);
        grid[pos] = (c == '*' || c == '#');
        if (c == '0' || c == 's' || c == 'S') start = pos;
      }
      h++;
      y++;
    }
  }
  
  Coord random_coord() const {
    int x = random_range(w);
    int y = random_range(h);
    return Coord(x,y);
  }
};

// ----------------------------------------------------------------------------
// Distance calculation / solver
// ----------------------------------------------------------------------------

int dists[MAX_W*MAX_H];
int pass_dists[MAX_W*MAX_H];
// Returns maximum distance that can be traveled to reach any point
int max_distance(Puzzle const& g) {
  Coord queue[MAX_W*MAX_H];
  int queue_start = 0, queue_end = 0;
  int max_dist = 0;
  
  std::fill_n(dists, MAX_W*MAX_H, UNREACHABLE);
  std::fill_n(pass_dists, MAX_W*MAX_H, UNREACHABLE);
  
  queue[queue_end++] = g.start;
  dists[g.start] = pass_dists[g.start] = 0;
  
  while (queue_start < queue_end) {
    Coord pos = queue[queue_start++];
    int dist = dists[pos];
    int col = pos.col(), row = pos.row()*MAX_W;
    // all four movement directions
    int ds[] = {-1,1, -MAX_W,MAX_W};
    Coord bounds[] = {row-1, row+g.w, (-1)*MAX_W + col, g.h*MAX_W + col};
    // move in that direction
    for (int i=0; i<4; ++i) {
      int p = pos;
      while (true) {
        // is next point free?
        int p2 = p + ds[i];
        if (p2 == bounds[i]) break;
        if (g.grid[p2]) break;
        if (pass_dists[p2] > dist + 1) {
          pass_dists[p2] = dist + 1;
          max_dist = dist + 1; // we could stop here
        }
        p = p2;
      }
      if (dists[p] > dist + 1) {
        dists[p] = dist + 1;
        queue[queue_end++] = p;
      }
    }
  }
  return max_dist;
}

void show(Puzzle const& g) {
  int max_dist = max_distance(g);
  std::ostream& out = std::cout;
  const char* CLEAR = "\033[0m";
  const char* GREEN = "\033[32;1m";
  const char* BLUE = "\033[34;1m";
  const char* YELLOW = "\033[33;1m";
  out << max_dist << std::endl;
  for (int y=0; y<g.h; ++y) {
    for (int x=0; x<g.w; ++x) {
      Coord coord = Coord(x,y);
      int dist = pass_dists[coord];
      if (g[coord]) {
        out << YELLOW << '#' << CLEAR;
      } else if (dist >= UNREACHABLE) {
        out << '.';
      } else {
        if (dist == 0) out << GREEN;
        if (dist == max_dist) out << BLUE;
        if (dist < 10) {
          out << dist;
        } else {
          out << (char)(dist - 10 + 'a');
        }
        if (dist == 0 || dist == max_dist) out << CLEAR;
      }
    }
    out << std::endl;
  }
}

// ----------------------------------------------------------------------------
// Greedy puzzle maker
// ----------------------------------------------------------------------------

template <typename F>
void for_single_changes(Puzzle const& puzzle, bool swaps, F fun) {
  // for each obstacle, consider moving it to any location, and call fun
  Puzzle puzzle_new = puzzle;
  for (Coord obstacle : puzzle) {
    if (puzzle[obstacle]) {
      puzzle_new[obstacle] = false;
      for (Coord alt : puzzle) {
        if (!puzzle[alt] && alt != puzzle.start) {
          puzzle_new[alt] = true;
          fun(puzzle_new);
          puzzle_new[alt] = false;
        }
      }
      puzzle_new[obstacle] = true;
    }
  }
  // consider new start location
  {
    for (Coord alt : puzzle) {
      if (!puzzle[alt] && alt != puzzle.start) {
        puzzle_new.start = alt;
        fun(puzzle_new);
      }
    }
  }
  // swap rows/columns
  if (swaps) {
    for (int x1 = 0; x1 < puzzle.w; ++x1) {
      for (int x2 = x1+1; x2 < puzzle.w; ++x2) {
        puzzle_new = puzzle;
        for (int y = 0; y < puzzle.h; ++y) {
          std::swap(puzzle_new[Coord(x1,y)], puzzle_new[Coord(x2,y)]);
        }
        if (puzzle.start.col() == x1) {
          puzzle_new.start = Coord(x2, puzzle.start.row());
        } else if (puzzle.start.col() == x2) {
          puzzle_new.start = Coord(x1, puzzle.start.row());
        }
        fun(puzzle_new);
      }
    }
    for (int y1 = 0; y1 < puzzle.h; ++y1) {
      for (int y2 = y1+1; y2 < puzzle.h; ++y2) {
        puzzle_new = puzzle;
        for (int x = 0; x < puzzle.w; ++x) {
          std::swap(puzzle_new[Coord(x,y1)], puzzle_new[Coord(x,y2)]);
        }
        if (puzzle.start.row() == y1) {
          puzzle_new.start = Coord(puzzle.start.col(), y2);
        } else if (puzzle.start.row() == y2) {
          puzzle_new.start = Coord(puzzle.start.col(), y1);
        }
        fun(puzzle_new);
      }
    }
  }
}

Puzzle greedy_optimize(Puzzle const& initial, bool verbose = false) {
  Puzzle best = initial;
  int best_score = max_distance(best);
  const bool accept_same_score = false;
  const int BUDGET = accept_same_score ? 10 : 1;
  const bool USE_SWAPS = false;
  int budget = BUDGET;
  
  while (budget > 0) {
    budget--;
    Puzzle cur = best;
    int num_equiv = 1; // number of puzzles with the same score as best
    bool swaps = USE_SWAPS && (budget == BUDGET || budget == 0);
    for_single_changes(cur, swaps, [&](Puzzle const& p) {
      int score = max_distance(p);
      if (score > best_score) {
        best = p;
        best_score = score;
        budget = BUDGET;
        if (verbose) {
          show(best);
          std::cout << std::endl;
        }
      } else if (accept_same_score && score == best_score) {
        num_equiv++;
        // accept with probability 1/num_equiv
        if (random_range(num_equiv) == 0) {
          best = p;
        }
      }
    });
  }
  return best;
}

Puzzle greedy_optimize_from_random(int w, int h, int obstacles = 8, const bool verbose = false) {
  const int RUNS = 10000;
  Puzzle best(w,h);
  int best_score = 0;
  
  for (int i=0; i < RUNS; ++i) {
    // initialize
    Puzzle puzzle(w,h);
    for (int j=0; j < obstacles; ++j) {
      puzzle[puzzle.random_coord()] = true;
    }
    do {
      puzzle.start = puzzle.random_coord();
    } while (puzzle[puzzle.start]);
    // optimize
    puzzle = greedy_optimize(puzzle);
    int score = max_distance(puzzle);
    if (score > best_score) {
      best_score = score;
      best = puzzle;
      if (verbose) show(best);
    }
  }
  return best;
}

// ----------------------------------------------------------------------------
// Exhaustive search
// ----------------------------------------------------------------------------

struct SkipStartIterator {
  Puzzle::iterator it;
  int start;
  explicit SkipStartIterator(Puzzle const& p)
    : it(p.begin()), start(p.start)
  {
    if (*it == start) ++it;
  }
  void operator ++ () {
    ++it;
    if (*it == start) ++it;
  }
  Coord operator * () const {
    return *it;
  }
  bool operator == (Puzzle::iterator const& that) const {
    return it == that;
  }
  bool operator != (Puzzle::iterator const& that) const {
    return it != that;
  }
};

// move obstacles to next configuration
// in (reversed) lexicographical order
bool next_puzzle(Puzzle& p) {
  // change "0001110" to "1100001"
  // find obstacle
  auto obstacle = SkipStartIterator(p);
  while (true) {
    if (obstacle == p.end()) return false; // no obstacles
    if (p[*obstacle]) break;
    ++obstacle;
  }
  // find clear space after obstacles
  int num_obstacles = 0;
  auto first_clear = obstacle;
  while (true) {
    if (first_clear == p.end()) return false; // last puzzle in ordering
    if (!p[*first_clear]) break;
    ++first_clear;
    ++num_obstacles;
  }
  // move over
  auto it = SkipStartIterator(p);
  for (int i=0; i<num_obstacles-1; ++i) {
    p[*obstacle] = false;
    p[*it] = true;
    ++obstacle;
    ++it;
  }
  p[*obstacle] = false;
  p[*first_clear] = true;
  return true;
}

// first puzzle configuration
void first_puzzle(Puzzle& p, int obstacles) {
  p.clear();
  auto it = SkipStartIterator(p);
  while (obstacles > 0 && it != p.end()) {
    p[*it] = true;
    --obstacles;
    ++it;
  }
}

Puzzle brute_force_solve(int w, int h, int obstacles = 8, const bool verbose = false) {
  Puzzle best(w,h);
  int best_score = -1;
  
  Puzzle puzzle(w,h);
  for (Coord start_coord : puzzle) {
    // By mirror symmetry, we only need to consider start coordinates in top-left quadrant
    // If w==h, by transposition we only need the upper diagonal
    if (start_coord.col()*2 > w || start_coord.row()*2 > h || (w == h && start_coord.row() > start_coord.col())) {
      if (verbose) std::cout << "Skip " << start_coord << " (" << start_coord.col() << "," << start_coord.row() << ")" << std::endl;
      continue;
    }
    puzzle.start = start_coord;
    first_puzzle(puzzle, obstacles);
    if (verbose) std::cout << "Start " << start_coord << " (" << start_coord.col() << "," << start_coord.row() << ")" << std::endl;
    while (true) {
      int score = max_distance(puzzle);
      if (score > best_score) {
        best_score = score;
        best = puzzle;
        if (verbose) show(best);
      }
      if (!next_puzzle(puzzle)) break;
    }
  }
  return best;
}

// ----------------------------------------------------------------------------
// Relative position based puzzle
// ----------------------------------------------------------------------------


enum class RelativePosition {
  SAME,
  NEXT,
  SKIP,
};

const int MAX_OBSTACLES = MAX_W*MAX_H;

// A puzzle where obstacles are placed relative to each other
// Obstacles and start location are placed from left to right
// vertical positions are placed top to bottom, and we use a permutation to pick a vertical location
struct RelativePuzzle {
  int num_obstacles;
  RelativePosition horizontal_pos[MAX_OBSTACLES];
  RelativePosition vertical_pos[MAX_OBSTACLES];
  int permutation[MAX_OBSTACLES];
  int start_index;

  // require:
  //   * with o obstacles, have o+1 points, o+2 relative positions
  //   * // don't overlap wall
  //     horizontal_pos[0] != SAME && horizontal_pos[o+1] != SAME
  //     vertical_pos[0] != SAME && vertical_pos[o+1] != SAME
  //   * if horizontal_pos[i] == SAME, then
  //       vertical_pos[perm[i]] != SAME  (?)
  //       permutation[i] < permutation[i+1]
  //   * for uniqueness: permutation[0] <= n/2
  //       otherwise we could vertical flip
  //   * for uniqueness: start_index <= n/2
  //       otherwise we could horizontal flip
  //
  
  static void to_coords(const RelativePosition* rel_pos, int n, Coord* coords, int& w) {
    int x = -1;
    for (int i=0; i<n+1; ++i) {
      if      (rel_pos[i] == RelativePosition::SAME) x += 0;
      else if (rel_pos[i] == RelativePosition::NEXT) x += 1;
      else if (rel_pos[i] == RelativePosition::SKIP) x += 3;
      coords[i] = x;
    }
    w = coords[n];
  }
  
  bool to_puzzle(Puzzle& puzzle) const {
    Coord x_coords[MAX_OBSTACLES];
    Coord y_coords[MAX_OBSTACLES];
    to_coords(horizontal_pos, num_obstacles+1, x_coords, puzzle.w);
    to_coords(vertical_pos, num_obstacles+1, y_coords, puzzle.h);
    if (puzzle.w == 0 || puzzle.w > MAX_W || x_coords[0] == -1) return false;
    if (puzzle.h == 0 || puzzle.h > MAX_H || y_coords[0] == -1) return false;
    puzzle.clear();
    for (int i=0; i<num_obstacles+1; ++i) {
      auto pos = Coord(x_coords[i], y_coords[permutation[i]]);
      if (i == start_index) {
        puzzle.start = pos;
      } else {
        puzzle[pos] = true;
      }
    }
    return true;
  }
};



// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------

Puzzle test_puzzle({
    ".0#....",
    ".#..#..",
    ".#.....",
    ".#...#.",
    ".#.#...",
    ".......",
  });

Puzzle test_puzzle2({
    "0...#...",
    "#.......",
    ".......#",
    "........",
    "........",
    "........",
  });

int main() {
  //const int w = 7, h = 6;
  //const int w = 8, h = 8;
  const int w = 32, h = 32;
  const int min_obstacle = 8, max_obstacle = 15;
  const bool brute_force = false;
  const bool verbose = true;
  
  for (int o = min_obstacle; o <= max_obstacle; ++o) {
    std::cout << "=============" << std::endl;
    Puzzle p = brute_force ? brute_force_solve(w,h,o,verbose)
                           : greedy_optimize_from_random(w,h,o,verbose);
    int score = max_distance(p);
    show(p);
    std::cout << "With " << o << " obstacles: " << score << " steps" << std::endl;
  }
  
  return EXIT_SUCCESS;
}

