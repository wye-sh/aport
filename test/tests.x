
#include <aport/aport.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <cstdlib>

using namespace std;

#define STRINGIFY_CAPACITY 8192

template<typename T>
string stringify(aport::tree<T>& Tree) {
  // Create a temporary file
  char Filename[] = "build/capture_XXXXXX";
  int Fd = mkstemp(Filename);
  if (Fd == -1)
    return "";

  // Redirect stdout to the temporary file
  fflush(stdout);
  int StdoutFd = dup(STDOUT_FILENO);
  dup2(Fd, STDOUT_FILENO);

  // Print the tree
  Tree.print();

  // Flush and restore stdout
  fflush(stdout);
  dup2(StdoutFd, STDOUT_FILENO);
  close(StdoutFd);
  close(Fd);

  // Read the captured output
  std::ifstream File(Filename);
  std::string CapturedOutput
    ((std::istreambuf_iterator<char>(File)),
     std::istreambuf_iterator<char>());
  File.close();

  // Remove the temporary file
  std::remove(Filename);

  return CapturedOutput;
}

template<typename T>
void compare (aport::tree<T>& Tree, string Compare) {
  Compare.erase(0, Compare.find_first_not_of(" \t\n\r\f\v"));
  REQUIRE(stringify(Tree) == Compare);
}

TEST_CASE("@", "[info]") {
  if (!filesystem::is_directory("build")) {
    fprintf(stderr, "Must run tests from project root.\n");
    exit(1);
  }
  
  printf("AportRadixMode: ");
  if constexpr (aport::AportRadixMode) {
    printf("Enabled");
  } else {
    printf("Disabled");
  }
  printf(".\n");
}

TEST_CASE("Insertion", "[test]") {
  aport::tree<int> NameToAge;
  
  // Start with "al"
  NameToAge.insert("al", 0);

  // Disambiguate at "a"
  NameToAge.insert("arnold", 1);
  
  compare(NameToAge, R"(
`a`
 `l`: 0
 `rnold`: 1
)");
  REQUIRE(NameToAge.length() == 2);
  
  // Insert where data already is
  NameToAge.insert("arnold", 3);
  
  compare(NameToAge, R"(
`a`
 `l`: 0
 `rnold`: 3
)");

  // Insert with a prefix that is the same as a full key
  NameToAge.insert("alfred", 2);
  compare(NameToAge, R"(
`a`
 `l`: 0
  `fred`: 2
 `rnold`: 3
)");

  // Do some random splits
  NameToAge.insert("alfre", 12);
  NameToAge.insert("arnie", 13);
  compare(NameToAge, R"(
`a`
 `l`: 0
  `fre`: 12
   `d`: 2
 `rn`
  `ie`: 13
  `old`: 3
)");

  // Insert on the first "a" and add "arn" and modify arnold
  NameToAge.insert("a",      99);
  NameToAge.insert("arn",    12);
  NameToAge.insert("arnold", 36);
  compare(NameToAge, R"(
`a`: 99
 `l`: 0
  `fre`: 12
   `d`: 2
 `rn`: 12
  `ie`: 13
  `old`: 36
)");
  REQUIRE(NameToAge.length() == 7);

  // Empty string
  NameToAge.insert("", 23);
  compare(NameToAge, R"(
``: 23
`a`: 99
 `l`: 0
  `fre`: 12
   `d`: 2
 `rn`: 12
  `ie`: 13
  `old`: 36
)");
  REQUIRE(NameToAge.length() == 8);
}

TEST_CASE("Contains", "[test]") {
  aport::tree<int> NameToAge;
  
  NameToAge.insert("a",      0);
  NameToAge.insert("al",     0);
  NameToAge.insert("arnold", 0);
  NameToAge.insert("bea",    0);
  NameToAge.insert("bee",    0);
  NameToAge.insert("be",     0);
  NameToAge.insert("bar",    0);
  NameToAge.insert("cot",    0);

  REQUIRE(NameToAge.contains("a"));
  REQUIRE(NameToAge.contains("al"));
  REQUIRE(NameToAge.contains("arnold"));
  REQUIRE(NameToAge.contains("bea"));
  REQUIRE(NameToAge.contains("bee"));
  REQUIRE(NameToAge.contains("be"));
  REQUIRE(NameToAge.contains("bar"));
  REQUIRE(NameToAge.contains("cot"));

  // Also make sure that nodes that do not contain data are not considered as
  // being contained by the tree
  REQUIRE(!NameToAge.contains("b"));
  
  // Also check some random ones that are clearly not in the tree
  REQUIRE(!NameToAge.contains("car"));
  REQUIRE(!NameToAge.contains("cart"));
  REQUIRE(!NameToAge.contains("cots"));
  REQUIRE(!NameToAge.contains("co"));
  REQUIRE(!NameToAge.contains("hello"));
}

TEST_CASE("Deletion and Clearing", "[test]") {
  aport::tree<int> NameToAge;
  
  NameToAge.insert("a",      1);
  NameToAge.insert("al",     2);
  NameToAge.insert("arnold", 3);
  NameToAge.insert("bea",    4);
  NameToAge.insert("bee",    5);
  NameToAge.insert("bar",    6);
  NameToAge.insert("cot",    7);

  // First, make sure that the tree is what we expect it to be, so we can safely
  // make assumptions about erasures
  compare(NameToAge, R"(
`a`: 1
 `l`: 2
 `rnold`: 3
`b`
 `ar`: 6
 `e`
  `a`: 4
  `e`: 5
`cot`: 7
)");
  REQUIRE(NameToAge.length() == 7);

  // Now, erase "bea"
  NameToAge.erase("bea");
  compare(NameToAge, R"(
`a`: 1
 `l`: 2
 `rnold`: 3
`b`
 `ar`: 6
 `ee`: 5
`cot`: 7
)");
  REQUIRE(NameToAge.length() == 6);

  // Now, we insert data into "b"
  NameToAge.insert("b", 200);
  // Then we erase "bar" to make sure that no combining will occur
  NameToAge.erase("bar");
  compare(NameToAge, R"(
`a`: 1
 `l`: 2
 `rnold`: 3
`b`: 200
 `ee`: 5
`cot`: 7
)");
  REQUIRE(NameToAge.length() == 6);

  // Let's now erase "b" again, to make sure that "bee" becomes its own node
  NameToAge.erase("b");
  compare(NameToAge, R"(
`a`: 1
 `l`: 2
 `rnold`: 3
`bee`: 5
`cot`: 7
)");

  // Set root and make sure it was added to the map
  NameToAge.insert("", 6006);
  compare(NameToAge, R"(
``: 6006
`a`: 1
 `l`: 2
 `rnold`: 3
`bee`: 5
`cot`: 7
)");
  
  //  Then we remove root again to make sure root deletions work
  NameToAge.erase("");
  compare(NameToAge, R"(
`a`: 1
 `l`: 2
 `rnold`: 3
`bee`: 5
`cot`: 7
)");
  REQUIRE(NameToAge.length() == 5);

  // Clear the tree, then do the root operation again
  NameToAge.clear();
  NameToAge.insert("", 1);
  compare(NameToAge, R"(
``: 1
)");
  NameToAge.erase("");
  compare(NameToAge, "");
}

TEST_CASE("Iteration", "[test]") {
  aport::tree<int> NameToAge;
  
  NameToAge.insert("a",      1);
  NameToAge.insert("al",     2);
  NameToAge.insert("arnold", 3);
  NameToAge.insert("bea",    4);
  NameToAge.insert("bee",    5);
  NameToAge.insert("bar",    6);
  NameToAge.insert("cot",    7);
  NameToAge.erase("bar");

  for (auto [ key, value ] : NameToAge)
    // Verify that all the values are present
    value = 12;

  compare(NameToAge, R"(
`a`: 12
 `l`: 12
 `rnold`: 12
`be`
 `a`: 12
 `e`: 12
`cot`: 12
)");

  REQUIRE(NameToAge.length() == 6);
  
  // Cake sure the right number of elements are iterated
  size_t n = 0;
  for (auto _ : NameToAge)
    ++ n;
  REQUIRE(n == NameToAge.length());
  
  // Deletion during iteration
  for (auto I = NameToAge.begin(); I != NameToAge.end();)
    I = NameToAge.erase(I);
  REQUIRE(NameToAge.length() == 0);
}

TEST_CASE("Retrieval", "[test]") {
  aport::tree<int> NameToAge;
  
  NameToAge.insert("a",      1);
  NameToAge.insert("al",     2);
  NameToAge.insert("arnold", 3);
  NameToAge.insert("bea",    4);
  NameToAge.insert("bee",    5);
  NameToAge.insert("bar",    6);
  NameToAge.insert("cot",    7);
  
  REQUIRE(NameToAge.get("cot") == 7);
  REQUIRE(NameToAge.get("bar") == 6);
  REQUIRE(NameToAge.get("bee") == 5);
  REQUIRE(NameToAge.get("bea") == 4);
  REQUIRE_THROWS_AS(NameToAge.get("q"),     aport::no_such_key);
  REQUIRE_THROWS_AS(NameToAge.get("bark"),  aport::no_such_key);
  REQUIRE_THROWS_AS(NameToAge.get("arnie"), aport::no_such_key);
  REQUIRE_THROWS_AS(NameToAge.get("cars"),  aport::no_such_key);
  REQUIRE_THROWS_AS(NameToAge.get("bet"),   aport::no_such_key);

  if constexpr (aport::AportRadixMode) {
    // Tests specific to radix mode, as these would pass in aport mode also
    // since aport mode is optimistic
    
    REQUIRE_THROWS_AS(NameToAge.get("argold"), aport::no_such_key);
    REQUIRE_THROWS_AS(NameToAge.get("bat"),    aport::no_such_key);
    REQUIRE_THROWS_AS(NameToAge.get("cor"),    aport::no_such_key);
  } else {
    // Test specific to aport mode

    REQUIRE(NameToAge.get("argold") == 3);
    REQUIRE(NameToAge.get("bat")    == 6);
    REQUIRE(NameToAge.get("cor")    == 7);
  }
}

TEST_CASE("Retreival (Insert If Necessary)", "[test]") {
  aport::tree<int> NameToAge;

  NameToAge["hello"] = 12;
  NameToAge["test"]  = 15;

  REQUIRE(NameToAge["hello"] == 12);
  REQUIRE(NameToAge["test"]  == 15);

  NameToAge["hello"] = 13;

  REQUIRE(NameToAge["hello"] == 13);
  REQUIRE(NameToAge.length() == 2);

  NameToAge["he"] = 77;

  REQUIRE(NameToAge["hello"] == 13);
  REQUIRE(NameToAge["he"]    == 77);
  
  compare(NameToAge, R"(
`he`: 77
 `llo`: 13
`test`: 15
)");
  
  REQUIRE(NameToAge.length() == 3);
}

TEST_CASE("Copy and Move", "copy-and-move") {
  aport::tree<int> Tree;

  Tree["one"]   = 1;
  Tree["two"]   = 1;
  Tree["three"] = 2;
  Tree["four"]  = 3;
  Tree["five"]  = 5;
  Tree["six"]   = 8;
  Tree["seven"] = 13;
  Tree["eight"] = 21;

  compare(Tree, R"(
`eight`: 21
`f`
 `ive`: 5
 `our`: 3
`one`: 1
`s`
 `even`: 13
 `ix`: 8
`t`
 `hree`: 2
 `wo`: 1
)");

  // Verify that std::move works
  aport::tree<int> MoveInto;
  MoveInto = std::move(Tree);

  compare(MoveInto, R"(
`eight`: 21
`f`
 `ive`: 5
 `our`: 3
`one`: 1
`s`
 `even`: 13
 `ix`: 8
`t`
 `hree`: 2
 `wo`: 1
)");

  // Verify copying works
  aport::tree<int> Copy;
  Copy = MoveInto;

  compare(MoveInto, R"(
`eight`: 21
`f`
 `ive`: 5
 `our`: 3
`one`: 1
`s`
 `even`: 13
 `ix`: 8
`t`
 `hree`: 2
 `wo`: 1
)");

  compare(Copy, R"(
`eight`: 21
`f`
 `ive`: 5
 `our`: 3
`one`: 1
`s`
 `even`: 13
 `ix`: 8
`t`
 `hree`: 2
 `wo`: 1
)");
  REQUIRE(Copy.length() == MoveInto.length());
  int N = 0;
  for (const auto &[ _1, _2 ] : Copy)
    ++ N;
  REQUIRE(Copy.length() == N);
}
