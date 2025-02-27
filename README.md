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
- namespace aport
  - [struct aport::no_such_key](#struct-aportno_such_key)
  - [struct aport::tree](#struct-aporttree)
    - [struct tree::iterator](#struct-treeiterator)
    - [tree::insert()](#treeinsert)
    - [tree::erase()](#treeerase)
    - [tree::erase()](#treeerase-1)
    - [tree::contains()](#treecontains)
    - [tree::get()](#treeget)
    - [tree::operator\[\]()](#treeoperator)
    - [tree::query()](#treequery)
    - [tree::clear()](#treeclear)
    - [tree::length()](#treelength)
    - [tree::print()](#treeprint)
    - [tree::begin()](#treebegin)
    - [tree::end()](#treeend)

##

### struct aport::no_such_key
```cpp
struct no_such_key;
```
When thrown, it denotes that the key that was attempted to be retrieved does not exist in the structure.

##

### struct aport::tree
`movable`
```cpp
template<typename T>
struct tree;
```
The APORT tree container, into which data can be inserted or erased, looked up or checked for inclusion.

#### Template Parameters
- `T`: Specifies the type stored in the tree.

##

### struct tree::iterator
```cpp
struct iterator;
```
Forward iterator.

##

### tree::insert()
```cpp
void insert (string Key, T Value);
```
Inserts an object of type `T` into the tree at `Key`.

#### Parameters
- `Key`: Location the inserted `Value` should be retrievable per.
- `Value`: Value to be inserted per `Key`.

##

### tree::erase()
```cpp
void erase (string Key);
```
Erases the entry at location `Key`.

#### Parameters
- `Key`: Location at which an entry should be deleted from the tree.

##

### tree::erase()
```cpp
iterator erase (iterator Iterator);
```
Erases an element using an iterator `Iterator`. It is undefined behaviour to call this using an iterator that does not point to an element.

#### Parameters
- `Iterator`: Is an iterator to the element you want to delete.

#### Returns
An iterator to the element after the one that was erased.

##

### tree::contains()
```cpp
bool contains (string Key);
```
Checks if tree contains an entry whose key is `Key`.

#### Parameters
- `Key`: Key to check for to see if it is contained by the tree.

#### Returns
`true` if entry per `Key` exists in the tree, and `false` otherwise.

##

### tree::get()
`throws`
```cpp
T &get (string Key);
```
Retrieve data of type `T` from tree node at `Key` if it exists.

#### Parameters
- `Key`: Location to retrieve data from.

#### Returns
Data of type `T` at location `Key` (if it exists), otherwise if no data or key exists, throws `no_such_key` exception.

##

### tree::operator\[\]()
```cpp
T &operator[] (string Key);
```
Retrieve the data of type `T` from tree node at `Key` if it exists, otherwise creates it, returning the data of type `T` from the newly inserted node.

#### Parameters
- `Key`: Location to retrieve data from (or insert data into).

#### Returns
Data of type `T` at location `Key` (if it exists), otherwise creates it and returns the newly created data.

##

### tree::query()
```cpp
vector<T *> query (string String);
```
Retrieves all entries that match against query string `String`, supporting wildcard expressions using "\*". For instance, if two entries are stored in the tree "astrid" and "arnold", the query string "a\*d" would retrieve both entries.

#### Parameters
- `String`: Query string that is used to determine what is returned.

#### Returns
A vector containing all entries that matched against query string `String`.

##

### tree::clear()
```cpp
void clear ();
```
Clears all the content inside the tree.

##

### tree::length()
```cpp
size_t length ();
```
Returns the number of entries stored inside the tree.

#### Returns
Number of entries stored inside the tree.

##

### tree::print()
```cpp
void print ();
```
Outputs a visual representation of the tree. Could be useful for someone outside testing, so rather than making it local to the root test file, it has been provided here directly.

##

### tree::begin()
```cpp
iterator begin ();
```
Begin iterator.

##

### tree::end()
```cpp
iterator end ();
```
End iterator.
