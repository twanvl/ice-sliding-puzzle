// ----------------------------------------------------------------------------
// Ice sliding puzzle maker
// Search for puzzles that need the most moves to solve.
// ----------------------------------------------------------------------------

#include <iostream>
#include <random>
#include <algorithm>
#include <limits>
#include <assert.h>
#include <stdint.h>

using Distance = uint8_t;
const int GLOBAL_BUFFER_SIZE = 64*64;
const bool USE_SENTINEL_OPTIMIZATION = true;

std::default_random_engine rng;
std::uniform_real_distribution<double> uniform(0.0, 1.0);
int random_range(int n) {
  std::uniform_int_distribution<int> distribution(0,n-1);
  return distribution(rng);
}
double random_double() {
  return uniform(rng);
}

// ----------------------------------------------------------------------------
// Parameters
// ----------------------------------------------------------------------------

// We pass parameters via template arguments, so the compiler can optimize stuff for us.
template <int MAX_W_, int MAX_H_, bool EDGES_ARE_WALLS_ = true>
struct Params {
  static const bool EDGES_ARE_WALLS = EDGES_ARE_WALLS_;
  static const bool SENTINELS = USE_SENTINEL_OPTIMIZATION && EDGES_ARE_WALLS; // can we use sentinels as an optimization?
  static const int MAX_W = SENTINELS ? MAX_W_ - 1 : MAX_W_;
  static const int MAX_H = MAX_H_;
  static const int ROW_STRIDE = MAX_W_;
  static const int BUFFER_SIZE = ROW_STRIDE * MAX_H; // size needed for buffers (that don't store sentinels)
};

// ----------------------------------------------------------------------------
// Puzzles + solver
// ----------------------------------------------------------------------------

// We encode coordinates as x + y * MAX_W
template <typename Params>
struct Coord {
private:
  int pos;
public:
  inline Coord() {}
  inline constexpr Coord(int pos) : pos(pos) {}
  inline constexpr Coord(int x, int y) : pos(x + y*Params::ROW_STRIDE) {}
  inline constexpr operator int() const {
    return pos;
  }

  inline constexpr int row() const {
    return (pos / Params::ROW_STRIDE);
  }
  inline constexpr int col() const {
    return pos % Params::ROW_STRIDE;
  }
  inline constexpr int with_row(int row) const {
    return Coord(col(), row);
  }
  inline constexpr int with_col(int col) const {
    return Coord(col, row());
  }
  
  inline constexpr Coord next(int w) {
    int next = pos + 1;
    if (next % Params::ROW_STRIDE == w) {
      next = next - w + Params::ROW_STRIDE;
    }
    return next;
  }
};

// A puzzle is a grid of obstacles, with a start point.
// We don't need to store the end point, because we calculate the distance to all points.
template <typename Params>
struct Puzzle {
private:
  // We store the grid with sentinel obstacles values at the walls.
  // The actual obstacles are located at grid[Coord(x,y+1)]
  // this means that the max puzzle size is MAX_W-1 by MAX_H
  bool grid[Params::ROW_STRIDE * (Params::SENTINELS ? Params::MAX_H+2 : Params::MAX_H)];
  void init_sentinels() {
    if (Params::SENTINELS) {
      std::fill_n(&grid[0], Params::ROW_STRIDE, true);
      std::fill_n(&grid[(h+1)*Params::ROW_STRIDE], Params::ROW_STRIDE, true);
      for (int y=0; y<h; ++y) {
        grid[(y+1)*Params::ROW_STRIDE-1] = true;
        grid[(y+1)*Params::ROW_STRIDE+w] = true;
      }
    }
  }
public:
  using Coord = ::Coord<Params>;
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
      if (pos % Params::ROW_STRIDE == w) {
        pos = pos - w + Params::ROW_STRIDE;
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
    return iterator(h * Params::ROW_STRIDE, w);
  }
  
  inline bool operator [] (Coord pos) const {
    return grid[pos + (Params::SENTINELS ? Params::ROW_STRIDE : 0)];
  }
  inline bool& operator [] (Coord pos) {
    return grid[pos + (Params::SENTINELS ? Params::ROW_STRIDE : 0)];
  }
  void clear() {
    std::fill_n(grid + (Params::SENTINELS ? Params::ROW_STRIDE : 0), h*Params::ROW_STRIDE, false);
    init_sentinels();
  }
  
  Puzzle(int w, int h) : w(w), h(h) {
    assert(w > 0 && w <= Params::MAX_W);
    assert(h > 0 && h <= Params::MAX_H);
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
        (*this)[pos] = (c == '*' || c == '#');
        if (c == '0' || c == 's' || c == 'S') start = pos;
      }
      h++;
      y++;
    }
    init_sentinels();
  }
  
  Coord random_coord() const {
    int x = random_range(w);
    int y = random_range(h);
    return Coord(x,y);
  }
  Coord random_empty_coord() const {
    while (true) {
      Coord coord = random_coord();
      if (!(*this)[coord] && coord != start) return coord;
    }
  }

  int count_obstacles() const {
    int obstacles = 0;
    for (Coord c : *this) {
      if ((*this)[c]) obstacles++;
    }
    return obstacles;
  }
};

// ----------------------------------------------------------------------------
// Distance calculation / solver
// ----------------------------------------------------------------------------

const Distance UNREACHABLE = std::numeric_limits<Distance>::max() - 1;
Distance dists[GLOBAL_BUFFER_SIZE];
Distance pass_dists[GLOBAL_BUFFER_SIZE];
int come_from[GLOBAL_BUFFER_SIZE];

// Returns maximum distance that can be traveled to reach any point
template <bool track_come_from = false, typename Params>
int max_distance(Puzzle<Params> const& puzzle) {
  using Coord = ::Coord<Params>;
  Coord queue[Params::MAX_W*Params::MAX_H];
  int queue_start = 0, queue_end = 0;
  Distance max_dist = 0;

  static_assert(Params::BUFFER_SIZE <= GLOBAL_BUFFER_SIZE);
  std::fill_n(dists,      Params::ROW_STRIDE*puzzle.h, UNREACHABLE);
  std::fill_n(pass_dists, Params::ROW_STRIDE*puzzle.h, UNREACHABLE);
  
  queue[queue_end++] = puzzle.start;
  dists[puzzle.start] = pass_dists[puzzle.start] = 0;
  
  while (queue_start < queue_end) {
    Coord pos = queue[queue_start++];
    const Distance dist = dists[pos];
    const Distance next_dist = dist + 1;
    // check move in all four directions
    auto check_in_direction = [&](int delta, int bound) {
      Coord p = pos;
      while (true) {
        // is next point free?
        Coord p2 = p + delta;
        if (!Params::EDGES_ARE_WALLS && p2 == bound) {
          // can't stop at the edge
          return;
        }
        // with sentinels we don't need bounds checking anymore
        if (!Params::SENTINELS && p2 == bound) break;
        if (puzzle[p2]) break;
        if (pass_dists[p2] > next_dist) {
          pass_dists[p2] = next_dist;
          if (track_come_from) come_from[p2] = pos;
          max_dist = next_dist; // we could stop here
        }
        p = p2;
      }
      if (dists[p] > next_dist) {
        dists[p] = next_dist;
        queue[queue_end++] = p;
      }
    };
    check_in_direction(-1, pos.with_col(-1));
    check_in_direction(+1, pos.with_col(puzzle.w));
    check_in_direction(-Params::ROW_STRIDE, pos.with_row(-1));
    check_in_direction(+Params::ROW_STRIDE, pos.with_row(puzzle.h));
  }
  return max_dist;
}

// ----------------------------------------------------------------------------
// Visualization
// ----------------------------------------------------------------------------

enum class Style {
  PUZZLE_ONLY,
  DISTANCES,
  BOX_DRAWING
};

template <typename Params>
Coord<Params> find_goal(Puzzle<Params> const& puzzle, int distance) {
  // requires that max_distance() has been called to fill pass_dists
  for (auto pos : puzzle) {
    if (pass_dists[pos] == distance) return pos;
  }
  return puzzle.start;
}

template <typename Params>
void show_path(Puzzle<Params> const& puzzle, Coord<Params> goal, const char** path) {
  using Coord = ::Coord<Params>;
  const char* clear = ".";
  std::fill_n(path, Params::BUFFER_SIZE, clear);
  path[goal] = "E";
  auto pos = goal;
  while (pos != puzzle.start) {
    Coord from = come_from[pos];
    if (from == pos) return; // shouldn't happen
    bool horizontal = from.row() == pos.row();
    int dir = horizontal ? (from < pos ? -1 : 1) : (from < pos ? -Params::ROW_STRIDE : Params::ROW_STRIDE);
    while (pos != from) {
      pos = pos + dir;
      if (path[pos] != clear) {
        path[pos] = "┼";
      } else {
        path[pos] = horizontal ? "─" : "│";
      }
    }
    Coord next_from = come_from[from];
    if (dir == -1)                  path[pos] = next_from < pos ? "└" : "┌";
    if (dir ==  1)                  path[pos] = next_from < pos ? "┘" : "┐";
    if (dir == -Params::ROW_STRIDE) path[pos] = next_from < pos ? "┐" : "┌";
    if (dir ==  Params::ROW_STRIDE) path[pos] = next_from < pos ? "┘" : "└";
  }
}

template <typename Params>
void show(Puzzle<Params> const& puzzle, Style style = Style::BOX_DRAWING, bool ansi_color = true) {
  std::ostream& out = std::cout;
  const char* CLEAR = ansi_color ? "\033[0m" : "";
  const char* GREEN = ansi_color ? "\033[32;1m" : "";
  const char* BLUE = ansi_color ? "\033[34;1m" : "";
  const char* YELLOW = ansi_color ? "\033[33;1m" : "";
  int max_dist = max_distance<true>(puzzle);
  auto goal = find_goal(puzzle, max_dist);
  const char* box_drawing[Params::BUFFER_SIZE];
  show_path(puzzle, goal, box_drawing);
  out << puzzle.w << "×" << puzzle.h << " puzzle, " << puzzle.count_obstacles() << " obstacles, " << max_dist << " moves" << std::endl;
  for (int y=0; y<puzzle.h; ++y) {
    for (int x=0; x<puzzle.w; ++x) {
      auto pos = Coord<Params>(x,y);
      int dist = pass_dists[pos];
      if (puzzle[pos]) {
        out << YELLOW << (style == Style::BOX_DRAWING ? "■" : "#") << CLEAR;
      } else if (dist == 0) {
        out << GREEN << "S" << CLEAR;
      } else if (pos == goal) {
        out << BLUE << "E" << CLEAR;
      } else if (style == Style::BOX_DRAWING) {
        out << box_drawing[pos];
      } else if (dist >= UNREACHABLE || style == Style::PUZZLE_ONLY) {
        out << '.';
      } else {
        if (dist == max_dist) out << BLUE;
        if (dist < 10) {
          out << dist;
        } else {
          out << (char)(dist - 10 + 'a');
        }
        if (dist == max_dist) out << CLEAR;
      }
    }
    out << std::endl;
  }
}

// ----------------------------------------------------------------------------
// Greedy puzzle maker
// ----------------------------------------------------------------------------

template <typename Params, typename F>
void for_single_changes(Puzzle<Params> const& puzzle, bool swaps, bool reachable_only, F fun) {
  using Coord = ::Coord<Params>;
  // find out which cells are reachable
  int reachable[Params::BUFFER_SIZE];
  if (reachable_only) {
    max_distance(puzzle);
    std::copy(pass_dists, pass_dists+Params::ROW_STRIDE*puzzle.h, reachable);
  }
  // for each obstacle, consider moving it to any location, and call fun
  auto puzzle_new = puzzle;
  for (auto obstacle : puzzle) {
    if (puzzle[obstacle]) {
      puzzle_new[obstacle] = false;
      for (auto alt : puzzle) {
        if (reachable_only && reachable[alt] == UNREACHABLE) {
          // optimization: no path reaches this cell, so placing an obstacle here is useless
          continue;
        }
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
    for (auto alt : puzzle) {
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

template <typename Params>
Puzzle<Params> greedy_optimize(Puzzle<Params> const& initial, bool verbose = false) {
  auto best = initial;
  int best_score = max_distance(best);
  const bool accept_same_score = false;
  const int BUDGET = accept_same_score ? 10 : 1;
  const bool USE_SWAPS = false;
  const bool REACHABLE_ONLY = true;
  int budget = BUDGET;
  
  while (budget > 0) {
    budget--;
    auto cur = best;
    int num_equiv = 1; // number of puzzles with the same score as best
    bool swaps = USE_SWAPS && (budget == BUDGET || budget == 0);
    for_single_changes(cur, swaps, REACHABLE_ONLY, [&](Puzzle<Params> const& p) {
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

template <typename Params>
Puzzle<Params> greedy_optimize_from_random(int w, int h, int obstacles = 8, const bool verbose = false) {
  const int RUNS = 10000;
  Puzzle<Params> best(w,h);
  int best_score = 0;
  
  for (int i=0; i < RUNS; ++i) {
    // initialize
    Puzzle<Params> puzzle(w,h);
    for (int j=0; j < obstacles; ++j) {
      puzzle[puzzle.random_coord()] = true;
    }
    puzzle.start = puzzle.random_empty_coord();
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
// Simulated annealing
// ----------------------------------------------------------------------------

// Find and remove the i-th obstacle
template <typename Params>
void remove_obstacle(Puzzle<Params>& puzzle, int i) {
  for (auto pos : puzzle) {
    if (puzzle[pos]) {
      if (i == 0) {
        puzzle[pos] = false;
        return;
      }
      i--;
    }
  }
}

template <typename Params>
void random_change(Puzzle<Params>& puzzle, int num_obstacles) {
  // move an obstacle or a the start location
  int to_remove = random_range(num_obstacles+1);
  if (to_remove == num_obstacles) {
    puzzle.start = puzzle.random_empty_coord();
  } else {
    remove_obstacle(puzzle, to_remove);
    puzzle[puzzle.random_empty_coord()] = true;
  }
}

template <typename Params>
Puzzle<Params> make_random_puzzle(int w, int h, int obstacles) {
  Puzzle<Params> puzzle(w,h);
  puzzle.start = puzzle.random_coord();
  for (int i = 0; i < obstacles; ++i) {
    puzzle[puzzle.random_empty_coord()] = true;
  }
  return puzzle;
}

template <typename Params>
Puzzle<Params> simulated_annealing_search(int w, int h, int obstacles, int verbose=0) {
  Puzzle<Params> best(w,h);
  int best_score = 0;

  const int RUNS = 10;
  const int STEP_PER_TEMPERATURE = 100 * obstacles;
  const double TEMPERATURE_INITIAL = 0.1;
  const double TEMPERATURE_FINAL = 1e-5;
  const double TEMPERATURE_STEP = 1 / 1.003;
  
  for (int i=0; i < RUNS; ++i) {
    auto puzzle = make_random_puzzle<Params>(w,h,obstacles);
    int score = max_distance(puzzle);
    for (double temp = TEMPERATURE_INITIAL; temp >= TEMPERATURE_FINAL; temp *= TEMPERATURE_STEP) {
      int n_accept = 0, n_reject = 0;
      for (int i=0; i < STEP_PER_TEMPERATURE; ++i) {
        // change
        auto prev_puzzle = puzzle;
        int prev_score = score;
        random_change(puzzle, obstacles);
        // compare with best
        score = max_distance(puzzle);
        if (score > best_score) {
          best_score = score;
          best = puzzle;
          if (verbose) show(best);
        }
        // compare with previous
        bool accept = random_double() < exp(temp * (score - prev_score));
        if (!accept) {
          n_accept++;
          score = prev_score;
          puzzle = prev_puzzle;
        } else {
          n_reject++;
        }
      }
      if (verbose >= 2) {
        std::cout << "at " << temp << "  " << (double)n_accept/n_reject << " accepted" << std::endl;
      }
    }
  }
  return best;
}

// ----------------------------------------------------------------------------
// Exhaustive search
// ----------------------------------------------------------------------------

template <typename Params>
struct SkipStartIterator {
  using base_iterator = typename Puzzle<Params>::iterator;
  base_iterator it;
  int start;
  explicit SkipStartIterator(Puzzle<Params> const& p)
    : it(p.begin()), start(p.start)
  {
    if (*it == start) ++it;
  }
  void operator ++ () {
    ++it;
    if (*it == start) ++it;
  }
  Coord<Params> operator * () const {
    return *it;
  }
  bool operator == (base_iterator const& that) const {
    return it == that;
  }
  bool operator != (base_iterator const& that) const {
    return it != that;
  }
};

// move obstacles to next configuration
// in (reversed) lexicographical order
template <typename Params>
bool next_puzzle(Puzzle<Params>& p) {
  // change "0001110" to "1100001"
  // find obstacle
  auto obstacle = SkipStartIterator<Params>(p);
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
  auto it = SkipStartIterator<Params>(p);
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
template <typename Params>
void first_puzzle(Puzzle<Params>& p, int obstacles) {
  p.clear();
  auto it = SkipStartIterator<Params>(p);
  while (obstacles > 0 && it != p.end()) {
    p[*it] = true;
    --obstacles;
    ++it;
  }
}

template <typename Params>
Puzzle<Params> brute_force_search(int w, int h, int obstacles = 8, const bool verbose = false) {
  Puzzle<Params> best(w,h);
  int best_score = -1;
  
  Puzzle<Params> puzzle(w,h);
  for (auto start_coord : puzzle) {
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

// A puzzle where obstacles are placed relative to each other
// Obstacles and start location are placed from left to right
// vertical positions are placed top to bottom, and we use a permutation to pick a vertical location
//
// We have num_objects-1 obstacles and 1 start location.
// There are num_objects+1 horizontal and vertical relative positions (between walls and objects)
template <int MAX_OBSTACLES = 64>
struct RelativePuzzle {
  int num_objects;
  RelativePosition horizontal_pos[MAX_OBSTACLES];
  RelativePosition vertical_pos[MAX_OBSTACLES];
  int permutation[MAX_OBSTACLES];
  int start_index;

  // require:
  //   * with o obstacles, have o+1 objects, o+2 relative positions
  //   * // don't overlap wall
  //     horizontal_pos[0] != SAME && horizontal_pos[o+1] != SAME
  //     vertical_pos[0] != SAME && vertical_pos[o+1] != SAME
  //   * if horizontal_pos[i] == SAME, then
  //       not all j in perm[i]..perm[i+1] have vertical_pos[j] == SAME
  //       permutation[i] < permutation[i+1]
  //   * for uniqueness: permutation[0] <= n/2
  //       otherwise we could vertical flip
  //   * for uniqueness: start_index <= n/2
  //       otherwise we could horizontal flip
  
  static void to_coords(const RelativePosition* rel_pos, int n, int* coords, int& w) {
    int x = -1;
    for (int i=0; i<n+1; ++i) {
      if      (rel_pos[i] == RelativePosition::SAME) x += 0;
      else if (rel_pos[i] == RelativePosition::NEXT) x += 1;
      else if (rel_pos[i] == RelativePosition::SKIP) x += 4;
      coords[i] = x;
    }
    w = coords[n];
  }
  
  template <typename Params>
  bool to_puzzle(Puzzle<Params>& puzzle) const {
    int x_coords[MAX_OBSTACLES];
    int y_coords[MAX_OBSTACLES];
    to_coords(horizontal_pos, num_objects, x_coords, puzzle.w);
    to_coords(vertical_pos, num_objects, y_coords, puzzle.h);
    if (puzzle.w == 0 || puzzle.w > Params::MAX_W-1 || x_coords[0] == -1) return false;
    if (puzzle.h == 0 || puzzle.h > Params::MAX_H   || y_coords[0] == -1) return false;
    puzzle.clear();
    for (int i=0; i<num_objects; ++i) {
      auto pos = Coord<Params>(x_coords[i], y_coords[permutation[i]]);
      if (i == start_index) {
        puzzle.start = pos;
      } else {
        puzzle[pos] = true;
      }
    }
    return true;
  }
};

RelativePuzzle<> first_relative_puzzle(int obstacles, bool allow_same) {
  RelativePuzzle<> p;
  p.num_objects = obstacles + 1;
  p.start_index = 0;
  for (int i = 0; i < p.num_objects+1; ++i) {
    auto pos = i == 0 || i == p.num_objects || !allow_same ? RelativePosition::NEXT : RelativePosition::SAME;
    p.horizontal_pos[i] = pos;
    p.vertical_pos[i] = pos;
  }
  for (int i = 0; i < p.num_objects; ++i) {
    p.permutation[i] = i;
  }
  return p;
}

bool next_relative_pos(RelativePosition& p, bool at_wall) {
  if (p == RelativePosition::SAME) {
    p = RelativePosition::NEXT;
    return true;
  } else if (p == RelativePosition::NEXT) {
    p = RelativePosition::SKIP;
    return true;
  } else {
    p = at_wall ? RelativePosition::NEXT : RelativePosition::SAME;
    return false;
  }
}
template <int O>
bool next_relative_puzzle(RelativePuzzle<O>& p, bool allow_same) {
  // next start location
  ++p.start_index;
  if (p.start_index*2 < p.num_objects) return true;
  p.start_index = 0;
  // next permutation?
  if (std::next_permutation(p.permutation, p.permutation + p.num_objects) && p.permutation[0]*2 <= p.num_objects) {
    return true;
  }
  for (int i = 0; i < p.num_objects; ++i) {
    p.permutation[i] = i;
  }
  // next vertical/horizontal pos
  for (int i = 0; i < p.num_objects+1; ++i) {
    if (next_relative_pos(p.horizontal_pos[i], i == 0 || i == p.num_objects || !allow_same)) return true;
    if (next_relative_pos(p.vertical_pos[i], i == 0 || i == p.num_objects || !allow_same)) return true;
  }
  return false;
}

std::ostream& operator << (std::ostream& out, RelativePosition pos) {
  return out << (pos == RelativePosition::SAME ? '0' : pos == RelativePosition::NEXT ? '1' : '2');
}
template <int O>
std::ostream& operator << (std::ostream& out, RelativePuzzle<O> const& rp) {
  out << "RP: " << rp.num_objects << " start " << rp.start_index << std::endl;
  out << "horz: "; for (int i=0; i<rp.num_objects+1; ++i) {out << rp.horizontal_pos[i];} out << std::endl;
  out << "vert: "; for (int i=0; i<rp.num_objects+1; ++i) {out << rp.vertical_pos[i];} out << std::endl;
  out << "perm: "; for (int i=0; i<rp.num_objects; ++i) {out << rp.permutation[i] << " ";} out << std::endl;
  return out;
}

template <typename Params>
Puzzle<Params> relative_puzzle_search(int obstacles = 8, bool allow_same = false, const int verbose = 2) {
  Puzzle<Params> best(1,1);
  int best_score = -1;
  
  RelativePuzzle<> rp = first_relative_puzzle(obstacles,allow_same);
  Puzzle<Params> puzzle(1,1);
  long long count = 0;
  while (true) {
    count++;
    rp.to_puzzle(puzzle);
    if (verbose >= 3) {
      std::cout << rp;
    }
    if (verbose >= 4) show(puzzle);
    int score = max_distance(puzzle);
    if (score > best_score) {
      best_score = score;
      best = puzzle;
      if (verbose) {
        show(best);
        if (verbose >= 2) std::cout << rp;
      }
    }
    if (!next_relative_puzzle(rp,allow_same)) break;
  }
  if (verbose) std::cout << count << " puzzles tried" << std::endl;
  return best;
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------

using SimpleParams = Params<16, 16>;
Puzzle<SimpleParams> test_puzzle({
    ".0#....",
    ".#..#..",
    ".#.....",
    ".#...#.",
    ".#.#...",
    ".......",
  });

Puzzle<SimpleParams> test_puzzle2({
    "0...#...",
    "#.......",
    ".......#",
    "........",
    "........",
    "........",
  });

int main() {
  const bool edges_are_walls = true;
  const int w = 7, h = 6;
  //const int w = 8, h = 8;
  //const int w = 10, h = 10;
  //const int w = 16, h = 16;
  //const int w = 30, h = 30;
  //const int w = 30, h = 10;
  const int min_obstacle = 2, max_obstacle = 5;
  //const int min_obstacle = 9, max_obstacle = 11;
  //const int min_obstacle = 7, max_obstacle = 20;
  const bool brute_force = true;
  const bool simulated_annealing = false;
  const bool verbose = false;
  using Params = ::Params<w+1,h,edges_are_walls>;
  //using Params = ::Params<64,64,edges_are_walls>;
  
  if (false) {
    relative_puzzle_search<Params>(min_obstacle);
    return EXIT_SUCCESS;
  }
  
  for (int o = min_obstacle; o <= max_obstacle; ++o) {
    std::cout << "=============" << std::endl;
    auto puzzle =
      brute_force ?
        brute_force_search<Params>(w,h,o,verbose) :
      simulated_annealing ?
        simulated_annealing_search<Params>(w,h,o,verbose) :
        greedy_optimize_from_random<Params>(w,h,o,verbose);
    show(puzzle);
  }
  
  return EXIT_SUCCESS;
}

