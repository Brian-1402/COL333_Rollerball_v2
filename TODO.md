## Tasks

- Re-assess weights

## Bugs

- that weird heap buffer overflow in a specific game between depth 5 vs depth 4, when sorting moveset is activated.

## Evaluation function

Ideas:

- Distance checking
- Search evals *(too computationally expensive)*
  (i.e. requires get_legal_moves() which is the computation of one extra depth of search)
  - Range of movement (basically branching factor number)
    - can be called only for specific moves like rook move or knight move, to saev on get_legal_move() calls.

- iterative deepening:
  - with storage of tree traversals
  - orders nodes using previously computed eval functions (cheaper than regular ordering since do_move() need not be called again for getting eval)

- Store tree traversals across game moves (storing in .hpp)

- Do ML for calculating weights.

### Distance checking

#### For rooks:

- Calculate distance from king.
- should prioritise only moving to a ring sector, rather than physically closer to king

#### For pawns:

- incremental distance is better.

#### For bishop:

- not sure

### Adding temporal logic

- (if we get time)
- Will require a new data structure which gets updated with every move, amortising any high time complexities.


