The model is a CyberiadaML hierarchical state machine (UML statecharts):

- A document holds one or more state machines. A state machine holds the
  elements: states, pseudostates, comments and the transitions between them.
- A state has a name, unique among its siblings, and a list of actions. A
  state with children is composite; its children are states, pseudostates
  and comments of their own. A state may hold an initial pseudostate that
  marks the child to enter first.
- The pseudostates: initial (one per level, the entry point),
  choice (a branching point with guarded outgoing transitions), terminate,
  and shallow/deep history (restore the last active substate of a composite).
  The final state is similar to pseudostates by drawing and using in diagrams.
- A transition goes from a source (a state, the initial pseudostate or a
  choice) to a target (a state, a final state, a choice, a terminate or a
  history pseudostate) of the same state machine. A history pseudostate may
  also be the source of one optional default transition. It carries at most
  one action.
  The format does not distinguish local and external transitions: every
  transition is local once saved, whatever the editor marks in memory.
- The texts are edited on the canvas the way a user does: a double click on a
  title, an action, a transition label or a comment opens an inline editor,
  the keys type into it and the edit is written back when it ends (`commit`).
  An action keeps its `entry / ` or `exit / ` prefix while the behaviour is
  edited. The text verbs need the text shown.
- The action notation: `entry/ behaviour` and `exit/ behaviour` are the
  activities of a state; `TRIGGER [guard]/ behaviour` is a reaction to an
  event, on a state (internal) or on a transition. The guard is optional and
  not allowed on entry and exit. A transition action needs a trigger except
  on the transition from the initial pseudostate, written `/ behaviour`.
  The behaviour is free text, usually variable operations (`red++`) or
  function calls (`motor_up()`). The behaviour text can be written in a popular
  programming language like C or Python, in a special pseudolanguage, or in plain text.
- A comment carries a text; a formal comment is machine-readable. A comment
  may point at other elements (subjects), by element, by a fragment of the
  target's name or by a fragment of its data.
- Geometry: `x y w h` is a rect, `x y` a point. The point of an element is
  its centre. A top-level element (under the state machine) is placed in
  absolute coordinates; a nested element is placed relative to the centre
  of its parent state. A parent must contain its children with a margin;
  the editor grows a parent that is too small.
- Ids are unique for the whole document and given by the editor: top-level elements `n0`,
  `n1`, ... in the order of creation, nested ones `parent::n0`, ..., transitions `src-tgt`
  or `src-tgt#N`. The dump you receive shows every id.
