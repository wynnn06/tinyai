#include "./dfa.hpp"
#include "./nfa.hpp"
#include "./regex_parser.hpp"
#include "./engine.hpp"
#include <iostream>
#include <string>
#include <vector>

int main()
{
  std::cout << "Starting Lexer Engine Test..." << std::endl;
  std::vector<RegexPattern> patterns = {
    RegexPattern("[a-zA-Z][a-zA-Z0-9_]*", "IDENTIFIER"),
    RegexPattern("#[^\n]*",
                 "COMMENT"), // Match everything except newline after #
    RegexPattern("\\\"\\\"\\\"[\\s\\S]*\\\"\\\"\\\"", "MULTILINE_STRING"),
    RegexPattern("\\\"[^\\\"\\n]*\\\"", "STRING"),
    RegexPattern("\\d+", "NUMBER"),
    RegexPattern("\\s+", "WHITESPACE"),
    RegexPattern("\\d+\\.\\d+", "FLOAT"),
    RegexPattern("\\+", "PLUS"),
    RegexPattern("\\*", "STAR"),
    RegexPattern("\\*\\*", "POWER"),
    RegexPattern("-", "MINUS"),
    RegexPattern("/", "SLASH"),
    RegexPattern("==", "EQEQ"),
    RegexPattern("=", "EQUALS")
  };

  std::cout << "Creating NFA..." << std::endl;
  NFA combinedNFA;
  // Create a single global start and accept for the combined NFA
  // (so we can safely add epsilon transitions from/to each fragment)
  NFAState *globalStart = combinedNFA.createState();
  NFAState *globalAccept = combinedNFA.createState();
  std::cout << "Global Start State ID: " << globalStart->id << std::endl;
  std::cout << "Global Accept State ID: " << globalAccept->id << std::endl;
  globalStart->isAccept = false;
  globalAccept->isAccept = true;
  combinedNFA.startState = globalStart;
  combinedNFA.acceptState = globalAccept;

  // Combine each regex pattern into a single e-NFA
  ThompsonConstruction thompson(combinedNFA);
  for (const auto &pattern : patterns) {
    std::cout << "Processing pattern: " << pattern.pattern
              << " (token type: " << pattern.tokenType << ")" << std::endl;

    RegexLexer lexer(pattern.pattern);
    std::vector<Token> tokens = lexer.tokenize();
    std::cout << "Lexed " << tokens.size() << " tokens." << std::endl;

    std::cout << "Tokens for pattern \"" << pattern.pattern
              << "\":" << std::endl;
    for (auto &tk : tokens) {
      if (tk.type == TokenType::CHAR)
        std::cout << " CHAR('"
                  << (tk.value == '\n' ? "\\n" : std::string(1, tk.value))
                  << "')";
      else if (tk.type == TokenType::CHAR_CLASS)
        std::cout << " CHAR_CLASS";
      else if (tk.type == TokenType::LPAREN)
        std::cout << " LPAREN";
      else if (tk.type == TokenType::RPAREN)
        std::cout << " RPAREN";
      else if (tk.type == TokenType::STAR)
        std::cout << " STAR";
      else if (tk.type == TokenType::PIPE)
        std::cout << " PIPE";
      else if (tk.type == TokenType::PLUS)
        std::cout << " PLUS";
      else if (tk.type == TokenType::QUESTION)
        std::cout << " QUESTION";
      else if (tk.type == TokenType::END)
        std::cout << " END";
    }
    std::cout << std::endl;

    RegexParser parser(tokens);
    auto ASTroot = parser.parse();
    std::cout << "Parsed AST." << std::endl;

    auto fragment = thompson.build(ASTroot);
    std::cout << "Built NFA fragment." << std::endl;

    // Ensure fragment's accept is not left marked as final
    if (fragment.accept) {
      fragment.accept->isAccept = false;
      fragment.accept->tokenType = pattern.tokenType;
      std::cout << "Set fragment accept state ID " << fragment.accept->id
                << " token type to " << pattern.tokenType << std::endl;
    }

    // Link global start -> fragment.start and fragment.accept -> global accept
    combinedNFA.addTransition(globalStart, fragment.start, EPSILON);
    combinedNFA.addTransition(fragment.accept, globalAccept, EPSILON);
    std::cout << "Linked fragment to global NFA." << std::endl;
  }

  std::cout << "Combined NFA for patterns: ";
  for (const auto &pattern : patterns) {
    std::cout << pattern.pattern << " ";
  }
  std::cout << std::endl;

  combinedNFA.print();

  std::cout << "Converting combined NFA to DFA..." << std::endl;
  SubsetConstruction converter(combinedNFA);
  DFA dfa = converter.convert();
  std::cout << "DFA constructed with " << dfa.states.size() << " states."
            << std::endl;

  std::cout << "Building transition table..." << std::endl;
  TransitionTableBuilder tableBuilder(dfa);
  TransitionTable table = tableBuilder.build();
  std::cout << "Transition table built with " << table.table.size()
            << " states and " << table.alphabet.size() << " symbols."
            << std::endl;

  std::cout << "Matching strings: " << std::endl;
  std::vector<std::string> testStrings = {
    "myVariable", // match
    "12345", // match
    "   ", // match
    "myVariable2", // match
    "my_Variable", // match
    "0myVariable", // reject
    "my Variable", // reject
    "123abc", // reject
    "12.34", // match
    ".567", // reject
    "89.", // reject
    "+", // match
    "*", // match
    "**", // match
    "-", // match
    "/", // match
    "==", // match EQEQ
    "=", // match EQUALS
    "!==", // reject
    "===", // reject
    "# this is a comment", // match
    "# comment with newline\nnext line", // reject
    "\"\"\"This is a\nmultiline string\"\"\"", // match
    "\"\"\"Unterminated string, should reject", // reject
    "\"\"\"Simple multiline\"\"\"", // should match
    "\"\"\"Multi\nline\nstring\"\"\"", // should match
    "\"\"\"String with \"quotes\" inside\"\"\"", // should match
    "\"\"\"String with \"\"double quotes\"\" inside\"\"\"", // should match
    "\"regular string\"", // match
    "\"unterminated string, should reject" // reject
  };
  for (const auto &str : testStrings) {
    MatchResult match = table.matches(str);
    std::cout << "Test against: " << str << " -> "
              << (match.matched ? "ACCEPT" : "REJECT")
              << (match.matched ? " (" + *match.tokenType + ")" : "")
              << std::endl;
  }

  return 0;
}