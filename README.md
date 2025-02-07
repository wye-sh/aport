# APORT

APORT (A Proximate Optimistic Radix Tree) is a high-performance variant of the
radix tree written in C++ that employs an optimistic retrieval strategy: while
standard operations (insertion, deletion, and inclusion checks) work exactly
like a radix tree, retrieval only verifies characters at disambiguation points,
significantly improving lookup speed at the expense of accuracy. This makes it
ideal for performance-critical applications where keys share long common
prefixes and some loss in accuracy is acceptable.

## Table of Contents
- [APORT](#aport)
  - [How It Works](#how-it-works)
  - [Installation](#installation)
    - [Options](#options)
  - [Documentation](#documentation)
    - [Creation](#creation)
    - [insert(key, value)](#insertkey-value)
    - [contains(key)](#containskey)
    - [erase(key) / erase(iterator)](#erasekey--eraseiterator)
    - [get(key)](#getkey)
	- [operator\[\](key)](#operatorkey)
    - [Iteration](#iteration)
    - [length()](#length)

## How It Works

For example, if we have an entry in our tree `"arnold" → 3`, the only
disambiguation point is at `'a'`. Assuming no other entries exist in the tree,
looking up "astrid" will retrieve a reference to our value `3`, because the
only disambiguation point matched and the keys share the same length. If we
then insert another entry `"andrew" → 4`, we now have two disambiguation
points: one at `'a'` and another furcating `'r'` and `'n'`. Now looking up
`"astrid"` would fail (since `'s'` matches neither `'r'` or `'n'`), but lookups
like `"arbold"` or `"answer"` would retrieve a reference to `'3'` and `'4'`
respectively.

This behaviour emphasizes the importance of using correct keys, as the addition
of new entries can introduce new disambiguation points, potentially changing
which keys match against existing entries.

## Installation

If you're using CMake, add the following to your `CMakeLists.txt`:
```cmake
include(FetchContent)
FetchContent_Declare(
  aport
  GIT_REPOSITORY https://github.com/wye-sh/aport
  GIT_TAG v1.1.3 # (latest version)
)
FetchContent_MakeAvailable(aport)

# Link against aport in your project
target_link_libraries(<your_target> PRIVATE aport)
```

Otherwise, you can clone the repository and include the headers manually in
your project.

### Options

You have the opportunity for further customization before your call to
`FetchContent_MakeAvailable()`.
```cmake
# If on, APORT will behave exactly like a normal radix tree
set(APORT_RADIX_MODE <OFF|ON>) # Default: OFF
```

## Documentation

APORT follows the container design patterns of the C++ Standard Libary.

### Creation
To get started:
```cpp
// The value can be any type, not just `int`
aport::tree<int> Tree;
```

### insert(key, value)
Inserts a key-value pair into the tree.
```cpp
// Insert multiple entries with similar disambiugation points
Tree.insert("arnold", 12);
Tree.insert("arbold", 13);
Tree.insert("arcold", 14);
Tree.insert("arwold", 15);
```

### contains(key)
Checks if key exists in the tree.
```cpp
// Remember that this uses a radix algorithm
if (Tree.contains("arwold"))
  printf("Wow, Arwold!\n");
```

### erase(key) / erase(iterator)
Removes an entry from the tree.
```cpp
// Deletes the entry `"arcold"` and its data
Tree.erase("arcold");

// Or if you have an iterator
Tree.erase(<iterator>);
```

### get(key)
Returns a reference to the value associated with the key, or throws an 
`aport::no_such_key` exception if it could not be located.
```cpp
int &Arwold = Tree.get("arwold");
// Since `Arwold` is a reference, modifications affect the stored value
Arwold = 19;
```

### operator\[](key)
Like `get()` but default-constructs a new value if the key doesn't exist. 
**Note** that this operation must use exact radix tree matching and cannot use
optimistic retrieval. It is also worth noting that this still is much faster
than first calling `contains()` and inserting if necessary and then calling
`get()`.
```cpp
// If "thomas" doesn't exist, this creates him with value 0
int &Thomas = Tree["thomas"];
// But we could just as easily create him or his friend and give him a value
// right away
Tree["brian"] = 34;
```

### Iteration
The tree supports standard iterator operations. The order of the elements for
iteration is undefined.
```cpp
// Range-based for loop (`Key` and `Value` are references)
for (auto [ Key, Value ] : Tree)
  ; // Process key-value pair

// Iterator-based deletion
for (auto I = Tree.begin(); I != Tree.end();)
  I = Tree.erase(I);
```

### length()
Returns the number of elements in the tree.
```cpp
size_t Count = Tree.length();
```
