Ice-sliding puzzle generator
============================

The puzzle
----------

The ice-sliding puzzle is played on a 2D grid. The goal is to reach a certain target cell.
You can move in any cardinal direction, but because ice is slippery you only stop moving when you hit an obstacle or the edge of the grid.

The generator
-------
What this program tries to do is find puzzles that need as many moves as possible.
It includes several search strategies:

 * Brute force search: tries all possible placements of obstacles.
 * Greedy optimization: improve the puzzle by greedily moving obstacles and the start location.
 * Simulated annealing
 * Abstracted puzzles: for large grids, we can abstract the location of obstacles, because it doesn't matter how many blank rows/columns we leave between obstacles.

Outputs
-------

With a 6 by 7 grid, the best puzzles are

<table><tr><td>

    3
    11
    ┌───┐■S
    └───┼┐│
    ■  E│││
       ││││
    ┌──┼┘││
    └──┘■└┘
</td><td>

    4
    13
    S■┌┐   
    │┌┼┼─┐■
    └┼┼┼─┼┐
    ■│││ E│
    ┌┼┘│  │
    └┘■└──┘
</td><td>

    5
    16
    ┌──┐■┌┐
    │  │┌┼┘
    │■┌┼┼┘■
    │┌┼┼┘■S
    ││└┼─E│
    └┘■└──┘
</td><td>

    6
    17
    ┌┐■S──┐
    └┼┐■┌─┘
    ■│└─┼┐■
    ┌┘■E┼┼┐
    │   │└┘
    └───┘■ 
</td><td>

    7
    19
    ┌──┐■┌┐
    │E■└┐││
    └┼┐■└┼┘
    ■└┼┐ │■
    ┌─┼┼─┼┐
    S■└┘■└┘
</td><td>

    8        obstacles
    20       moves
    ┌─S■┌─┐
    └┐■┌┼─┘
    ■│■└┼┐■
    ┌┼┐■└┼┐
    ││└┐■E│
    └┘■└──┘
</td></tr></table>
These are proven optimal by brute-force search.

With a sufficiently large grid, the best puzzles are:

    With 1 obstacle: 5 moves
    S          ■┌─────────┐
    │           │         │
    │           │         │
    │           E         │
    │                     │
    └─────────────────────┘

    With 2 obstacles: 8 moves
    S          ■┌─────────┐
    │          ┌┼─────────┘
    │          ││          
    │          ││          
    │          │└─────E    
    └──────────┘■          

    With 3 obstacles: 11 moves
    S          ■┌─────────┐
    │          ┌┼─────────┘
    │          ││          
    │          ││E        ■
    │          │││         
    │          │└┼────────┐
    └──────────┘■└────────┘

    With 4 obstacles: 13 moves
    ┌─────────┐■┌─────────┐
    │         │┌┼─────────┘
    │         │││          
    │         │││E        ■
    │         ││││         
    │         ││└┼────────┐
    S    ■    └┘■└────────┘

    With 5 obstacles: 17 moves
    S────────────┐■┌──────┐
    ■┌───────────┼┐│      │
     │           │││      │
     │           │││      │
     └──────E    │││      │
     ■┌──────────┼┼┘      │
     ┌┼──────────┼┘■      │
     ││          │        │
     ││          │        │
    ┌┼┘          │        │
    └┘■          └────────┘

    With 6 obstacles: 19 moves
    S────────────┐■┌──────┐
    ■┌───────────┼┐│      │
    ┌┼───E       │││      │
    ││           │││      │
    └┘           │││      │
    ■■┌──────────┼┼┘      │
     ┌┼──────────┼┘■      │
     ││          │        │
     ││          │        │
    ┌┼┘          │        │
    └┘■          └────────┘

I am not sure if the following are optimal:

    With 7 obstacles: 22 moves
    ┌┐     ■S────┐■┌──────┐
    └┼─────┐■┌───┼┐│      │
    ■└─────┼┐│   │││      │
           │││   │││      │
           E││   │││      │
    ┌───────┼┼───┼┼┘      │
    └───────┼┼───┼┘■      │
    ■       ││   │        │
            ││   │        │
    ┌───────┼┘   │        │
    └───────┘■   └────────┘

    With 8 obstacles: 25 moves
    ┌┐     ■S────┐■┌──────┐
    └┼─────┐■┌───┼┐│      │
    ■└─────┼┐│   │││      │
           │││   │││      │
        E──┘││   │││      │
           ■└┘   │││      │
             ■┌──┼┼┘      │
             ┌┼──┼┘■      │
             ││  │        │
    ┌────────┼┘  │        │
    └────────┘■  └────────┘
    or
    ┌──────┐  ■┌──────────┐
    └───┐■┌┼──┐│          │
    ■┌──┼┐││  ││          │
    ┌┼──┼┼┼┼──┼┼──────────┘
    ││  ││││  ││          
    ││  ││E│  ││       
    ││  ││ │  ││      
    ││  ││ │  ││       
    ││  ││ │  │└─────────┐■
    │└──┼┼─┼──┘■┌────────┼┐
    S■  └┘■└────┘   ■    └┘

    With 9 obstacles: 28 moves
    ┌─────┐■┌───S    ■   ┌┐
    │     │┌┼──┐■┌───────┼┘
    │     │││  │┌┼───────┘■
    └─────┼┼┼──┼┼┼────────┐
          │││  E││        │
          │││   ││        │
    ■┌────┘││   ││        │
    ┌┼───┐■└┼───┼┘        │
    ││   │┌─┼───┘■        │
    ││   ││ │             │
    └┘ ■ └┘■└─────────────┘

    With 10 obstacles: 30 moves
    ┌┐    ■  ┌────────┐■┌─┐
    ││    ┌──┘■┌──────┼┐│ │
    └┼────┘■┌─┐│      │││ │
    ■│ E───┐│ ││      │││ │
     │     ││ ││      │││ │
    ┌┼─────┼┘ ││      │││ │
    └┼─────┘■ ││      │││ │
    ■└──────┐ ││      │││ │
            │ ││      │││ │
    ┌───────┼─┼┼──────┼┼┘ │
    └───────┘■└┘  ■   └┘■ S