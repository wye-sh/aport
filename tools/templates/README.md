# APORT

APORT (A Proximate Optimistic Radix Tree) is a high-performance variant of the
radix tree written in C++ that employs an optimistic retrieval strategy: while
standard operations (insertion, deletion, and inclusion checks) work exactly
like a radix tree, retrieval only verifies characters at disambiguation points,
significantly improving lookup speed at the expense of accuracy. This makes it
ideal for performance-critical applications where keys share long common
prefixes and some loss in accuracy is acceptable.

## Table of Contents
- [How It Works](#how-it-works)
- [Installation](#installation)
  - [Options](#options)
- [Quick Start](#quick-start)
- [Documentation](#documentation)

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
  GIT_TAG v1.2.0 # (latest version)
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

## Quick Start

To get started:

```cpp
// The value can be any type, not just `int`
aport::tree<int> Tree;

// Insert multiple entries with similar disambiguation points
Tree.insert("arnold", 12);
Tree.insert("arbold", 13);
Tree.insert("arcold", 14);
Tree.insert("arwold", 15);

try {
  // get() is the only method that uses the optimized lookup
  int &Arbold = Tree.get("arbold");
} catch (aport::no_such_key &Exception) {
  printf("No such key.\n");
}

// Remove entries
Tree.erase("arbold");
Tree.erase("arwold");

// Range-based for loop; `Key` and `Value` are references
for (auto [ Key, Value ] : Tree)
printf("[%s]: %d\n", Key.c_str(), Value);
// Will print:
// $ [arnold]: 12
// $ [arcold]: 14

// Delete the rest of the entries
for (auto I = Tree.begin(); I != Tree.end(); ++ I)
  I = Tree.erase(I);
```

## Documentation
<template:toc>

<template:body>
