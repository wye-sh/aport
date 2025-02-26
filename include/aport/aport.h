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

/*
 * PATH_UNREACHABLE()
 *   Macro for versions prior to C++23 to do what the [[unreachable]] attribute
 *   does.
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

/*
 * AportRadixMode
 *   option(APORT_RADIX_MODE) - specifies if full radix matching should be used,
 *   effectively turning the aport tree into a radix tree. This can be used to
 *   help track down malformed retrieval strings or to convert the project to
 *   use radix trees if the project did not meet the criteria for aport trees.
 */
#ifdef APORT_RADIX_MODE
constexpr bool AportRadixMode = true;
#else
constexpr bool AportRadixMode = false;
#endif

/*
 * concept has_to_string
 *   Trait to check if `T` has a `to_string` function.
 */
template <typename T>
concept has_to_string = requires(T x) {
  { to_string(x) } -> same_as<string>;
};

/**---------------------------------------------------------------------------
 * struct no_such_key
 *   When thrown, it denotes that the key that was attempted to be retrieved
 *   does not exist in the structure.
 *---------------------------------------------------------------------------**/
struct no_such_key: exception {
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  public instance variables  /////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  string KeyName;
  string Message;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  /*
   * ...
   */
  no_such_key (string KeyName): KeyName((KeyName)) {
    Message = format("No such key: \"{}\".", this->KeyName);
  } // no_such_key()

  /*
   * ...
   */
  const char *what () const noexcept override {
    return Message.c_str();
  } // what()
}; // struct no_such_key

/**---------------------------------------------------------------------------
 * struct tree
 *   [movable] The APORT tree container, into which data can be inserted or
 *   erased, looked up or checked for inclusion. <T> specifies the type stored
 *   in the tree.
 *---------------------------------------------------------------------------**/
template<typename T>
struct tree {
  /**-------------------------------------------------------------------------
   * struct iterator
   *   Forward iterator.
   *-------------------------------------------------------------------------**/
  struct iterator;
  
  /*
   * ...
   */
  tree () {
    Root = make_unique<node>("");
  } // `tree ()`

  /*
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
  } // tree()
  
  /*
   * Move constructor.
   */
  tree (tree &&Other) noexcept {
    Root                = std::move(Other.Root);
    Length              = Other.Length;
    Nodes               = std::move(Other.Nodes);
    NodeToNodesIterator = std::move(Other.NodeToNodesIterator);
  } // tree()

  /*
   * Copy assignment.
   */
  tree &operator=(const tree &Other) {
    if (this != &Other) {
      this->~tree();
      new (this) tree(Other);
    }
    return *this;
  } // operator=()

  /*
   * Move assignment.
   */
  tree &operator= (tree &&Other) noexcept {
    if (this != &Other) {
      this->~tree();                     // Call destructor
      new (this) tree(std::move(Other)); // Move constructor placement new
    }
    return *this;
  } // operator=()
  
  /**-------------------------------------------------------------------------
   * insert()
   *   Inserts an object of type `T` into the tree at `Key`. <Key> is the
   *   location the inserted `Value` should be retrievable per, and <Value>
   *   is the value to be inserted per `Key`.
   *-------------------------------------------------------------------------**/
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
          track(&*NewNode, Key);
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
          track(&*NewNode, Key);
          IntermediateNode->FirstCharToNode[NewNode->Prefix[0]] =
            std::move(NewNode);
        } else {
          // No string left, insert value into the intermediate node that was
          // created
          track(&*IntermediateNode, Key);
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
        track(Node, Key);
        // length stays unchanged
        return; // [[RETURN]]
        // ^- exact_match
      }
    }
  } // insert()

  /**-------------------------------------------------------------------------
   * erase()
   *   Erases the entry at location `Key`. <Key> is the location at which an
   *   entry should be deleted from the tree.
   *-------------------------------------------------------------------------**/
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
        untrack(&*Node);
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
  } // erase()

  /**-------------------------------------------------------------------------
   * erase()
   *   Erases an element using an iterator `Iterator`. It is undefined
   *   behaviour to call this using an iterator that does not point to an
   *   element. <Iterator> is an iterator to the element you want to delete.
   *   <Returns> an iterator to the element after the one that was erased.
   *-------------------------------------------------------------------------**/
  iterator erase (iterator Iterator);
  // ^- Out-of-line definition because iterator needs to be fully formed

  /**-------------------------------------------------------------------------
   * contains()
   *   Checks if tree contains an entry whose key is `Key`. <Key> is the key to
   *   check for to see if it is contained by the tree. <Returns> `true` if
   *   entry per `Key` exists in the tree, and `false` otherwise.
   *-------------------------------------------------------------------------**/
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
  
  /**-------------------------------------------------------------------------
   * get()
   *   [throws] Retrieve data of type `T` from tree node at `Key` if it exists.
   *   <Key> is the location to retrieve data from. <Returns> the data of type
   *   `T` at location `Key` (if it exists), otherwise if no data or key exists,
   *   throws `no_such_key` exception.
   *-------------------------------------------------------------------------**/
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

  /**-------------------------------------------------------------------------
   * operator[]()
   *   Retrieve the data of type `T` from tree node at `Key` if it exists,
   *   otherwise creates it, returning the data of type `T` from the newly
   *   inserted node. <Key> is the location to retrieve data from (or insert
   *   data into). <Returns> the data of type `T` at location `Key` (if it
   *   exists), otherwise creates it and returns the newly created data.
   *-------------------------------------------------------------------------**/
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
          track(&*NewNode, Key);
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
          track(&*NewNode, Key);
          IntermediateNode->FirstCharToNode[NewNode->Prefix[0]] =
            std::move(NewNode);
        } else {
          // No string left, insert value into the intermediate node that was
          // created
          track(&*IntermediateNode, Key);
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
        track(Node, Key);
        // length stays unchanged
        return *Node->Data; // [[RETURN]]
        // ^- exact_match
      }
    }
  } // operator[]()

  /**-------------------------------------------------------------------------
   * clear()
   *   Clears all the content inside the tree.
   *-------------------------------------------------------------------------**/
  void clear () {
    Root = make_unique<node>("");
  } // clear()

  /**-------------------------------------------------------------------------
   * length()
   *   Returns the number of entries stored inside the tree. <Returns> the
   *   number of entries stored inside the tree.
   *-------------------------------------------------------------------------**/
  size_t length () {
    return Length;
  } // length()

  /**-------------------------------------------------------------------------
   * print()
   *   Outputs a visual representation of the tree. Could be useful for someone
   *   outside testing, so rather than making it local to the root test file,
   *   it has been provided here directly.
   *-------------------------------------------------------------------------**/
  void print () {
    struct entry {
      node *Node;
      int   Level;
    }; // struct entry

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
  } // print()

  /**-------------------------------------------------------------------------
   * begin()
   *   Begin iterator.
   *-------------------------------------------------------------------------**/
  iterator begin () {
    return iterator(Nodes.begin());
  } // begin()
  
  /**-------------------------------------------------------------------------
   * end()
   *   End iterator.
   *-------------------------------------------------------------------------**/
  iterator end () {
    return iterator(Nodes.end());
  } // end()

private: ///////////////////////////////////////////////////////////////////////
  /*
   * struct node
   *   Point of disambiguation for strings. For instance, we may have one
   *   string key "hello" in our tree and also "helium", making our first node
   *   store the segment "hel", then linking to nodes for the letter "l" and
   *   "i" respectively, as is typical for a radix tree implementation.
   */
  struct node {
    /*
     * enum class comparison_result
     *   Result when using the comparison method.
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
    }; // enum class comparison_result

    ////////////////////////////////////////////////////////////////////////////
    ///////////////////////  public instance variables  ////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    optional<T>                           Data;
    string                                Prefix;
    unordered_map<char, unique_ptr<node>> FirstCharToNode;
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    /*
     * ...
     */
    node (const string &Prefix): Prefix((Prefix)) {
      // ...
    } // node()

    /*
     * ...
     */
    node (const string &Prefix, T &&Data)
      : Prefix((Prefix)),
	Data  (std::forward<T>(Data)) {
      // ...
    } // node()

    /*
     * compare_prefixes()
     *   Returns a pair consisting of a `comparison_result`
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
    } // compare_prefixes()
  }; // struct node

  /*
   * struct node_info
   *   Information about a node for iteration, storing `Key` alongside it for
   *   quick lookup.
   */
  struct node_info {
    string  Key;
    node   *Node;
  }; // struct node_info

  friend struct iterator;

  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  private instance variables  ////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  unique_ptr<node>                      Root;
  size_t                                Length {};
  list<node_info>                       Nodes;
  using nodes_iterator = decltype(Nodes)::iterator;
  unordered_map<node *, nodes_iterator> NodeToNodesIterator;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  /*
   * track()
   *   Insert into `Nodes` and `node_to_node_info`.
   */
  void track (node *Node, string Key) {
    Nodes.emplace_front(Key, Node);
    if (NodeToNodesIterator.contains(Node))
      // If there already is an iterator for `node`, remove node from `nodes`
      // using mapping
      Nodes.erase(NodeToNodesIterator[Node]);
    NodeToNodesIterator[Node] = Nodes.begin();
  } // track()

  /*
   * untrack()
   *   Erase from `Nodes` and `node_to_node_info`.
   */
  void untrack (node *Node) {
    auto E = NodeToNodesIterator.extract(Node);
    Nodes.erase(E.mapped());
  } // untrack()
}; // `tree`


////////////////////////////////////////////////////////////////////////////////
//  TREE :: ITERATOR  //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct tree<T>::iterator {
  /*
   * struct result
   *   The type returned through dereferencing.
   */
  struct result {
    string &Key;
    T      &Value;
  }; // struct result

  /*
   * ...
   */
  iterator (decltype(tree::Nodes)::iterator Iterator): Iterator(Iterator) {
    // ...
  } // iterator()

  /*
   * ...
   */
  result operator* () {
    auto &NodeInfo = *Iterator;
    return result
      (NodeInfo.Key,
       *NodeInfo.Node->Data);
  } // operator*()

  /*
   * ...
   */
  iterator &operator++ () {
    ++ Iterator;
    return *this;
  } // operator++()

  /*
   * ...
   */
  iterator operator++ (int) {
    iterator Copy = *this;
    ++ (*this);
    return Copy;
  } // operator++(int)

  /*
   * ...
   */
  bool operator== (const iterator &Other) const {
    return Iterator == Other.Iterator;
  } // operator==()

  /*
   * ...
   */
  bool operator!= (const iterator &Other) const {
    return !(*this == Other);
  } // operator!=()

private: ///////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////  PRIVATE INSTANCE VARIABLES  ////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  decltype(tree::Nodes)::iterator Iterator;
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
}; // struct iterator


////////////////////////////////////////////////////////////////////////////////
//  TREE :: ERASE  /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

template<typename T>
tree<T>::iterator tree<T>::erase (iterator Iterator) {
  auto [ key, value ] = *Iterator;
  ++ Iterator;
  erase(key);
  return Iterator;
} // erase()

} // namespace aport

#endif
