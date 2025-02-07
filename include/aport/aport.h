//                                  Written by
//                              Alexander Hållenius
//                              Copyright (c) 2025
//                                      ~~~
//        APORT (A Proximate Optimistic Radix Tree) is a high-performance
//      variant of the radix tree written in C++ that employs an optimistic
//      retrieval strategy: whiles tandard operations (insertion, deletion,
//     and inclusion checks) work exactly like a radix tree, retrieval only
//     verifies characters at disambiguation points, significantly improving
//       lookup speed at the expense of accuracy. This makes it ideal for
//        performance-critical applications where keys share long common
//               prefixes and some loss in accuracy is acceptable.

#pragma once
#ifndef _APORT_APORT_H
#define _APORT_APORT_H

#include <string>
#include <cstring>
#include <algorithm>
#include <optional>
#include <tuple>
#include <memory>
#include <unordered_map>
#include <cstdio>
#include <stack>
#include <map>
#include <list>
#include <exception>
#include <format>

/**
 * PATH UNREACHABLE
 * ----------------
 *  Macro for versions prior to C++23 to do what the [[unreachable]] attribute
 *  does.
 */
#ifndef PATH_UNREACHABLE
#  if defined(_MSC_VER)
#    define PATH_UNREACHABLE() __assume(0)
#  else
#    define PATH_UNREACHABLE() __builtin_unreachable()
#  endif
#endif

namespace aport {

using namespace std;

/**
 * APORT RADIX MODE
 * ----------------
 *  option(APORT_RADIX_MODE) - specifies if full radix matching should be used,
 *  effectively turning the aport tree into a radix tree. This can be used to
 *  help track down malformed retrieval strings or to convert the project to use
 *  radix trees if the project did not meet the criteria for aport trees.
 */
#ifdef APORT_RADIX_MODE
constexpr bool AportRadixMode = true;
#else
constexpr bool AportRadixMode = false;
#endif

/**
 * HAS TO STRING
 * -------------
 *  Trait to check if `T` has a `to_string` function.
 */
template <typename T>
concept has_to_string = requires(T x) {
  { to_string(x) } -> same_as<string>;
};

/**
 * NO SUCH KEY (EXCEPTION)
 * -----------------------
 *  When thrown, it denotes that the key that was attempted to be retrieved does
 *  not exist in the structure.
 */
struct no_such_key: exception {
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  PUBLIC INSTANCE VARIABLES  /////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  string KeyName;
  string Message;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  /**
   * ...
   */
  no_such_key (string KeyName): KeyName((KeyName)) {
    Message = format("No such key: \"{}\".", this->KeyName);
  } // `no_such_key ()`

  /**
   * ...
   */
  const char *what () const noexcept override {
    return Message.c_str();
  } // `what ()`
}; // `no_such_key`

/**
 * TREE
 * ----
 *  The APORT tree container, into which data can be inserted or erased, looked
 *  up or checked for inclusion. The type template parameter `T` specifies the
 *  type stored in the tree.
 */
template<typename T>
struct tree {
  // Out-of-line definition
  struct iterator;

  /**
   * ...
   */
  tree () {
    Root = make_unique<node>("");
  } // `tree ()`

  /**
   * Copy constructor.
   */
  tree (const tree &Other) {
    // Mappings
    unordered_map<const node *, node *> OtherNodeToNode;
    
    // Copies `Prefix` and `Data` of `OtherNode`, but not the char to node table
    const auto shallow_copy = [&](const node *OtherNode) {
      unique_ptr<node> Node = make_unique<node>(OtherNode->Prefix);
      if (OtherNode->Data) // Copy over data (if any)
	Node->Data = *OtherNode->Data;
      OtherNodeToNode[OtherNode] = &*Node;
      return std::move(Node);
    }; // `shallow_copy ()`

    // Copy over root
    Root = shallow_copy(&*Other.Root);

    // Set up stack and use it
    struct entry { node *Node; const node *OtherNode; };
    stack<entry> Stack;
    Stack.push({ &*Root, &*Other.Root });
    while (!Stack.empty()) {
      auto [ Node, OtherNode ] = Stack.top();
      Stack.pop();
      
      // Go over all the children of the two equivalently positioned nodes
      for (const auto &[ FirstChar, OtherChild ] : OtherNode->FirstCharToNode) {
	unique_ptr<node> Child = shallow_copy(&*OtherChild);
	Stack.push({ &*Child, &*OtherChild }); // Push children onto stack
	Node->FirstCharToNode[FirstChar] = std::move(Child);
      }
    }

    // Populate `Nodes` and `NodeToNodesIterator`
    for (auto I = Other.Nodes.rbegin(); I != Other.Nodes.rend(); ++ I) {
      const auto &[ OtherKey, OtherNode ] = *I;
      node *NodePtr = OtherNodeToNode[&*OtherNode];
      Nodes.emplace_front(OtherKey, NodePtr);
      NodeToNodesIterator[NodePtr] = Nodes.begin();
    }
    
    // Transfer over `Length`
    Length = Other.Length;
  } // `tree ()`

  /**
   * Move constructor.
   */
  tree (tree &&Other) noexcept {
    Root                = std::move(Other.Root);
    Length              = Other.Length;
    Nodes               = std::move(Other.Nodes);
    NodeToNodesIterator = std::move(Other.NodeToNodesIterator);
  } // `tree ()`

  /**
   * Copy assignment.
   */
  tree &operator=(const tree &Other) {
    if (this != &Other) {
      this->~tree();
      new (this) tree(Other);
    }
    return *this;
  } // `operator= ()`

  /**
   * Move assignment.
   */
  tree &operator= (tree &&Other) noexcept {
    if (this != &Other) {
      this->~tree();                     // Call destructor
      new (this) tree(std::move(Other)); // Move constructor placement new
    }
    return *this;
  } // `operator= ()`
  
  /**
   * Inserts an object `value` of type `T` into the tree at key `key`.
   */
  void insert (string Key, T Value) {
    using comparison_result = node::comparison_result;

    // Traverse the tree, disambiguating along the way
    char   *KeyPtr       = const_cast<char *>(Key.c_str());
    size_t  KeyPtrLength = Key.length();
    node   *Parent       = nullptr;
    node   *Node         = &*Root;
    char    NodeAccessor = '\0';

    for (;;) {
      auto [ Result, NMatched ] = Node->template compare_prefixes<true>
	(KeyPtr, KeyPtrLength);
      // Increment string position with match length
      KeyPtr       += NMatched;
      KeyPtrLength -= NMatched;
      switch (Result) {
      case comparison_result::no_match:
	PATH_UNREACHABLE();

      case comparison_result::prefix_full_match: {
        // The entirety of the prefix matched, we must now check and see if
	// there is a binding per the new leading char of the key
        NodeAccessor = *KeyPtr;
        if (Node->FirstCharToNode.contains(NodeAccessor)) {
          // There is another node available for us to follow, so follow it
          Parent = Node; // Store previous node as parent to the new current one
          Node   = &*Node->FirstCharToNode[NodeAccessor];
          continue; // [[CONTINUE]]
        } else {
          // No node down this path already exists, therefore we create a new
          // one to store the remaining data
          unique_ptr<node> NewNode = make_unique<node>
            (string(KeyPtr, KeyPtrLength), std::move(Value));
          Track(&*NewNode, Key);
          Node->FirstCharToNode[NodeAccessor] = std::move(NewNode);
          ++ Length;
        }
      } return; // [[RETURN]]
        // ^- prefix_full_match

      case comparison_result::partial_match: {
        // The entirety of the string matched, but prefix still has more left on
        // it, meaning we need to split the current node into two from the place
        // where the two character sequences differ

        // This becomes the new node where `node` currently is
        unique_ptr<node> IntermediateNode = make_unique<node>
          (string(Node->Prefix.c_str(), NMatched));

        // This is the current node as it is inside the map
        unique_ptr<node> CurrentNode = std::move
          (Parent->FirstCharToNode[NodeAccessor]);
        CurrentNode->Prefix.erase(0, NMatched); // Move prefix forward
        // Insert current node into intermediate node
        IntermediateNode->FirstCharToNode[CurrentNode->Prefix[0]] =
          std::move(CurrentNode);

        if (KeyPtrLength > 0) {
          // This is the new node we create for the disamiguation for string
          unique_ptr<node> NewNode = make_unique<node>
            (string(KeyPtr, KeyPtrLength), std::move(Value));
          // Insert new node into intermediate node
          Track(&*NewNode, Key);
          IntermediateNode->FirstCharToNode[NewNode->Prefix[0]] =
            std::move(NewNode);
        } else {
          // No string left, insert value into the intermediate node that was
          // created
          Track(&*IntermediateNode, Key);
          IntermediateNode->Data = std::move(Value);
        }

        // Insert intermediate node into map
        Parent->FirstCharToNode[NodeAccessor] =
          std::move(IntermediateNode);
        ++ Length;
      } return; // [[RETURN]]
        // ^- partial_match

      case comparison_result::exact_match:
        // An exact match means the current node is supposed to receive the value
        // we are inserting
        if (!(bool) Node->Data)
          ++ Length;
        Node->Data = std::move(Value);
        Track(Node, Key);
        // length stays unchanged
        return; // [[RETURN]]
        // ^- exact_match
      }
    }
  } // `insert ()`

  /**
   * Erases the entry where the key is `key`.
   */
  void erase (string Key) {
    using comparison_result = node::comparison_result;

    // Traverse the tree, disambiguating along the way
    char   *KeyPtr       = const_cast<char *>(Key.c_str());
    size_t  KeyPtrLength = Key.length();
    node   *Parent       = nullptr;
    node   *ParentParent = nullptr;
    node   *Node         = &*Root;
    char    NodeAccessor = '\0';

    for (;;) {
      auto [ Result, NMatched ] = Node->template compare_prefixes<true>
	(KeyPtr, KeyPtrLength);
      // Increment string position with match length
      KeyPtr       += NMatched;
      KeyPtrLength -= NMatched;
      switch (Result) {
      case comparison_result::no_match:
      case comparison_result::partial_match:
        return; // [[RETURN]]
        // ^- no_match, partial_match

      case comparison_result::prefix_full_match: {
        // The entirety of prefix is matched, but string still has more length
        // to it
        NodeAccessor = *KeyPtr;
        if (Node->FirstCharToNode.contains(NodeAccessor)) {
          // There is another node available for us to follow, so follow it
          ParentParent = Parent; // Store previous parent as parent's parent
          Parent       = Node;   // Store previous node as parent
          Node         = &*Node->FirstCharToNode[NodeAccessor];
          continue; // [[CONTINUE]]
        }
      } return; // [[RETURN]]
        // ^- prefix_full_match

      case comparison_result::exact_match: {
        // We are removing `node` from `parent` (at `node_accessor`)
        Untrack(&*Node);
        Node->Data.reset();

        // Only if `node` is not the root node might we be required to
        // reorganize the nodes
        if (Node != &*Root) {
          size_t NChildren = Node->FirstCharToNode.size();
          if (NChildren == 0) {
            // When `node` has no children, we remove it from the parent
            Parent->FirstCharToNode.erase(NodeAccessor);

            // When a node is erased, the `parent` might be left with only a
            // single element, in which case, the `parent` can be combined with
            // the only remaining child (we also make sure the parent does not
            // contain data and that the parent's parent exists!)
            if (ParentParent && !(bool) Parent->Data && Parent->FirstCharToNode.size() == 1) {
              auto &[ C, TreeChild ] = *Parent->FirstCharToNode.begin();
              unique_ptr<node> Child = std::move(TreeChild);
              Child->Prefix = Parent->Prefix + Child->Prefix;
              ParentParent->FirstCharToNode[Child->Prefix[0]] =
                std::move(Child);
            }
          } else if (NChildren == 1) {
            // When `node` has a single child, we can expand the child to
            // feature the prefix of `node` and then replace `node` with
            // its child on the `parent`
            auto &[ _, TreeChild ] = *Node->FirstCharToNode.begin();
            unique_ptr<node> Child = std::move(TreeChild);
            Child->Prefix = Node->Prefix + Child->Prefix;
            Parent->FirstCharToNode[NodeAccessor] = std::move(Child);
          } else /*if (n_children > 1)*/ {
            // When `node` has more than one child, `node` is still required for
            // disambiguation; thus, do nothing
          }
        }
	
        -- Length;
      } return; // [[RETURN]]
        // ^- exact_match
      }
    }
  } // `erase ()`

  /**
   * Erases an element using an iterator `Iterator`. Returns an iterator to the
   * element after the one that was erased. It is undefined behaviour to call
   * this using an iterator that does not point to an element.
   */
  iterator erase (iterator Iterator);
  // ^- Out-of-line definition because iterator needs to be fully formed

  /**
   * Checks if tree contains an entry whose key is `Key`.
   */
  bool contains (string Key) {
    using comparison_result = node::comparison_result;

    // Traverse the tree, disambiguating along the way
    char   *KeyPtr       = const_cast<char *>(Key.c_str());
    size_t  KeyPtrLength = Key.length();
    node   *Node         = &*Root;

    for (;;) {
      auto [ Result, NMatched ] = Node->template compare_prefixes<true>
        (KeyPtr, KeyPtrLength);
      // Increment string position with match length
      KeyPtr       += NMatched;
      KeyPtrLength -= NMatched;
      switch (Result) {
      case comparison_result::no_match:
        return false;
        // ^- no_match

      case comparison_result::prefix_full_match: {
        // The entirety of prefix is matched, but string still has more length
        // to it
        char NodeAccessor = *KeyPtr;
        if (Node->FirstCharToNode.contains(NodeAccessor)) {
          // There is another node available for us to follow, so follow it
          Node = &*Node->FirstCharToNode[NodeAccessor];
          continue; // [[CONTINUE]]
        }
      } return false; // [[RETURN]]
        // ^- prefix_full_match

      case comparison_result::partial_match:
        return false;
        // ^- partial_match

      case comparison_result::exact_match:
        return (bool) Node->Data; // [[RETURN]]
        // ^- exact_match
      }
    }
  } // `contains ()`

  /**
   * Returns the data of type `T` from the tree node at `Key` if it exists,
   * otherwise if no data or key exists, throws an `aport::no_such_key`
   * exception.
   */
  T &get (string Key) {
    using comparison_result = node::comparison_result;

    // Traverse the tree, disambiguating along the way
    char   *KeyPtr       = const_cast<char *>(Key.c_str());
    size_t  KeyPtrLength = Key.length();
    node   *Node         = &*Root;

    for (;;) {
      auto [ Result, NMatched ] =
	Node->template compare_prefixes<AportRadixMode>(KeyPtr, KeyPtrLength);
      // Increment string position with match length
      KeyPtr       += NMatched;
      KeyPtrLength -= NMatched;
      switch (Result) {
      case comparison_result::no_match:
      case comparison_result::partial_match:
	throw no_such_key(Key); // [[THROW]]
        // ^- no_match, partial_match

      case comparison_result::prefix_full_match: {
        // The entirety of prefix is matched, but string still has more length
        // to it
        char NodeAccessor = *KeyPtr;
        if (Node->FirstCharToNode.contains(NodeAccessor)) {
          // There is another node available for us to follow, so follow it
          Node = &*Node->FirstCharToNode[NodeAccessor];
          continue; // [[CONTINUE]]
        }

	throw no_such_key(Key); // [[THROW]]
      }
        // ^- prefix_full_match

      case comparison_result::exact_match:
	if (Node->Data)
	  return *Node->Data;
	else
	  throw no_such_key(Key); // [[RETURN]]
        // ^- exact_match
      }
    }
  } // `get ()`

  /**
   * Returns the data of type `T` from the tree node at `Key` if it exists,
   * otherwise creates it, returning the data of type `T` from the newly
   * inserted node. This uses radix functionality all the way through, so
   * if radix performance is desired, first inserting and then using `get` is
   * required.
   */
  T &operator[] (string Key) {
    using comparison_result = node::comparison_result;

    // Traverse the tree, disambiguating along the way
    char   *KeyPtr       = const_cast<char *>(Key.c_str());
    size_t  KeyPtrLength = Key.length();
    node   *Parent       = nullptr;
    node   *Node         = &*Root;
    char    NodeAccessor = '\0';

    for (;;) {
      auto [ Result, NMatched ] = Node->template compare_prefixes<true>
	(KeyPtr, KeyPtrLength);
      // Increment string position with match length
      KeyPtr       += NMatched;
      KeyPtrLength -= NMatched;
      switch (Result) {
      case comparison_result::no_match:
	PATH_UNREACHABLE();
	// ^- no_match

      case comparison_result::prefix_full_match: {
        // The entirety of the prefix matched, we must now check and see if there
        // is a binding per the new leading char of the key
        NodeAccessor = *KeyPtr;
        if (Node->FirstCharToNode.contains(NodeAccessor)) {
          // There is another node available for us to follow, so follow it
          Parent = Node; // Store previous node as parent to the new current one
          Node   = &*Node->FirstCharToNode[NodeAccessor];
          continue; // [[CONTINUE]]
        } else {
          // No node down this path already exists, therefore we create a new
          // one to store the remaining data
          unique_ptr<node> NewNode = make_unique<node>
            (string(KeyPtr, KeyPtrLength), T());
	  T *Return = &*NewNode->Data;
          Track(&*NewNode, Key);
          Node->FirstCharToNode[NodeAccessor] = std::move(NewNode);
          ++ Length;
	  return *Return; // [[RETURN]]
        }
      }
        // ^- prefix_full_match

      case comparison_result::partial_match: {
        // The entirety of the string matched, but prefix still has more left on
        // it, meaning we need to split the current node into two from the place
        // where the two character sequences differ

        // This becomes the new node where `node` currently is
        unique_ptr<node> IntermediateNode = make_unique<node>
          (string(Node->Prefix.c_str(), NMatched));

        // This is the current node as it is inside the map
        unique_ptr<node> CurrentNode = std::move
          (Parent->FirstCharToNode[NodeAccessor]);
        CurrentNode->Prefix.erase(0, NMatched); // Move prefix forward
        // Insert current node into intermediate node
        IntermediateNode->FirstCharToNode[CurrentNode->Prefix[0]] =
          std::move(CurrentNode);

	T *Return;
        if (KeyPtrLength > 0) {
          // This is the new node we create for the disamiguation for string
          unique_ptr<node> NewNode = make_unique<node>
            (string(KeyPtr, KeyPtrLength), T());
	  Return = &*NewNode->Data;
          // Insert new node into intermediate node
          Track(&*NewNode, Key);
          IntermediateNode->FirstCharToNode[NewNode->Prefix[0]] =
            std::move(NewNode);
        } else {
          // No string left, insert value into the intermediate node that was
          // created
          Track(&*IntermediateNode, Key);
	  if (!(bool) IntermediateNode->Data)
	    IntermediateNode->Data = T();
          Return = &*IntermediateNode->Data;
        }

        // Insert intermediate node into map
        Parent->FirstCharToNode[NodeAccessor] =
          std::move(IntermediateNode);
        ++ Length;

	return *Return;
      } // ^- partial_match

      case comparison_result::exact_match:
        // An exact match means the current node is supposed to receive the value
        // we are inserting
        if (!(bool) Node->Data) {
          ++ Length;
	  Node->Data = T();
	}
        Track(Node, Key);
        // length stays unchanged
        return *Node->Data; // [[RETURN]]
        // ^- exact_match
      }
    }
  } // `operator[] ()`

  /**
   * Clears all the content inside the tree.
   */
  void clear () {
    Root = make_unique<node>("");
  } // `Clear ()`

  /**
   * Returns the number of entries stored inside this tree.
   */
  size_t length () {
    return Length;
  }

  /**
   * Visual representation of the tree. Could be useful for someone outside of
   * testing, so rather than making it local to the root test file, we are wri-
   * ting it here.
   */
  void print () {
    /**
     * PRINT () :: ENTRY
     * -----------------
     *  Stack entry.
     */
    struct entry {
      node *Node;
      int   Level;
    }; // `entry`

    stack<entry> Stack;
    Stack.push({ &*Root, -1 });

    while (!Stack.empty()) {
      auto [ Node, Level ] = Stack.top();
      Stack.pop();

      // Print current level
      if (!Node->Prefix.empty() || Node->Data) {
        for (int i = 0; i < Level; ++ i)
          printf(" ");
        printf("`%s`", Node->Prefix.c_str());
        if constexpr (has_to_string<T>) {
          optional<T> &NodeData = Node->Data;
          if (NodeData)
            printf(": %s", to_string(*NodeData).c_str());
        }
        printf("\n");
      }

      // Add all children
      map<char, node *> FirstCharToNode;
      for (auto &[ FirstChar, Child ] : Node->FirstCharToNode)
        FirstCharToNode[FirstChar] = &*Child;
      for (auto i = FirstCharToNode.rbegin()
             ; i != FirstCharToNode.rend()
             ; ++ i) {
        auto [ _, Child ] = *i;
        Stack.push({ Child, Level + 1 });
      }
    }
  } // `Print ()`

  iterator begin () {
    return iterator(Nodes.begin());
  } // `begin ()`

  iterator end () {
    return iterator(Nodes.end());
  } // `end ()`

private: ///////////////////////////////////////////////////////////////////////
  /**
   * TREE :: NODE
   * ------------
   *  Point of disambiguation for strings. For instance, we may have one string
   *  key "hello" in our tree and also "helium", making our first node store the
   *  segment "hel", then linking to nodes for the letter "l" and "i" respect-
   *  ively, as is typical for a radix tree implementation.
   */
  struct node {
    /**
     * TREE :: NODE :: COMPARISON RESULT
     * ---------------------------------
     *  Result when using the comparison method.
     */
    enum class comparison_result {
      no_match,
      // ^- Means that prefix had a symbol that the string did not have
      prefix_full_match,
      // ^- Means prefix was matched in full but string still has a tail
      partial_match,
      // ^- Means string was matched in full but prefix still has a tail
      exact_match
      // ^- Means prefix and string matched perfectly (in length also)
    }; // `comparison_result`

    ////////////////////////////////////////////////////////////////////////////
    ///////////////////////  PUBLIC INSTANCE VARIABLES  ////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    optional<T>                           Data;
    string                                Prefix;
    unordered_map<char, unique_ptr<node>> FirstCharToNode;
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    /**
     * ...
     */
    node (const string &Prefix): Prefix((Prefix)) {
      // ...
    } // `node ()`

    /**
     * ...
     */
    node (const string &Prefix, T Data)
      : Prefix((Prefix)),
	Data  (std::move(Data)) {
      // ...
    } // `node ()`

    /**
     * Returns a pair consisting of a `comparison_result`
     */
    template<bool UseRadix>
    pair<comparison_result, size_t> compare_prefixes
      (const char *String,
       size_t      StringLength) {
      comparison_result Result;

      size_t PrefixLength = Prefix.length();
      int NMatched;

      if constexpr (UseRadix) {
	size_t MinLength = min(PrefixLength, StringLength);
	// TODO: modify `matches` based on if we need aport behaviour
	NMatched = 0;
	char *P = const_cast<char *>(Prefix.c_str());
	char *S = const_cast<char *>(String);
	for (; *P == *S && NMatched < MinLength; ++ P, ++ S, ++ NMatched)
	  ; // ^- Figure out how many characters match in the two strings

	if (PrefixLength == NMatched) {
	  if (StringLength == NMatched)
	    Result = comparison_result::exact_match;
	  else
	    Result = comparison_result::prefix_full_match;
	} else if (NMatched == 0)
	  Result = comparison_result::no_match;
	else
	  Result = comparison_result::partial_match;
      } else {
	if (PrefixLength < StringLength) {
	  Result   = comparison_result::prefix_full_match;
	  NMatched = PrefixLength;
	} else if (PrefixLength == StringLength) {
	  Result   = comparison_result::exact_match;
	  NMatched = PrefixLength; // (which is `StringLength` here)
	} else {
	  Result   = comparison_result::no_match;
	  NMatched = 0;
	}
      }
      return { Result, NMatched };
    } // `compare_prefixes ()`
  }; // `node`

  struct node_info {
    string  Key;
    node   *Node;
  }; // `node_info`

  friend struct iterator;

  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  PRIVATE INSTANCE VARIABLES  ////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  unique_ptr<node>                      Root;
  size_t                                Length {};
  list<node_info>                       Nodes;
  using nodes_iterator = decltype(Nodes)::iterator;
  unordered_map<node *, nodes_iterator> NodeToNodesIterator;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  /** Insert into `Nodes` and `node_to_node_info` */
  void Track (node *Node, string Key) {
    Nodes.emplace_front(Key, Node);
    if (NodeToNodesIterator.contains(Node))
      // If there already is an iterator for `node`, remove node from `nodes`
      // using mapping
      Nodes.erase(NodeToNodesIterator[Node]);
    NodeToNodesIterator[Node] = Nodes.begin();
  } // `Track ()`

  /** Erase from `Nodes` and `node_to_node_info` */
  void Untrack (node *Node) {
    auto E = NodeToNodesIterator.extract(Node);
    Nodes.erase(E.mapped());
  } // `Untrack ()`
}; // `tree`

/**
 * TREE :: ITERATOR
 * ----------------
 *  Forward iterator for APORT tree.
 */
template<typename T>
struct tree<T>::iterator {
  struct result {
    string &Key;
    T      &Value;
  }; // `result`

  /**
   * ...
   */
  iterator (decltype(tree::Nodes)::iterator Iterator): Iterator(Iterator) {
    // ...
  } // `iterator ()`

  /**
   * ...
   */
  result operator* () {
    auto &NodeInfo = *Iterator;
    return result
      (NodeInfo.Key,
       *NodeInfo.Node->Data);
  }

  /**
   * ...
   */
  iterator &operator++ () {
    ++ Iterator;
    return *this;
  } // `operator++ ()`

  /**
   * ...
   */
  iterator operator++ (int) {
    iterator Copy = *this;
    ++ (*this);
    return Copy;
  } // `operator++ (int)`

  /**
   * ...
   */
  bool operator== (const iterator &Other) const {
    return Iterator == Other.Iterator;
  } // `operator== ()`

  /**
   * ...
   */
  bool operator!= (const iterator &Other) const {
    return !(*this == Other);
  } // `operator!= ()`

private: ///////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  PRIVATE INSTANCE VARIABLES  ////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  decltype(tree::Nodes)::iterator Iterator;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
}; // `iterator`

template<typename T>
tree<T>::iterator tree<T>::erase (iterator Iterator) {
  auto [ key, value ] = *Iterator;
  ++ Iterator;
  erase(key);
  return Iterator;
} // `erase ()`

} // namespace aport

#endif
