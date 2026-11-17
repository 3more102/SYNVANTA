#include "test.hpp"
#include "synvanta/lexer.hpp"
#include <string>
TEST_CASE(lexer_recognizes_keywords_symbols_and_based_literals){
    synvanta::Lexer lexer("module m(input logic [7:0] a); assign y = a ^ 8'h0f; endmodule","lexer.sv");
    const auto tokens=lexer.tokenize();
    REQUIRE(tokens.size()>15);REQUIRE(tokens[0].kind==synvanta::TokenKind::Module);
    REQUIRE(tokens[3].kind==synvanta::TokenKind::Input);REQUIRE(tokens.back().kind==synvanta::TokenKind::EndOfFile);
    REQUIRE(lexer.diagnostics().empty());
}
TEST_CASE(lexer_skips_line_and_block_comments){
    synvanta::Lexer lexer("// header\nmodule /* inline */ m(); /* tail */ endmodule","comments.sv");
    const auto tokens=lexer.tokenize();REQUIRE(tokens.size()==7);
    REQUIRE(tokens[0].kind==synvanta::TokenKind::Module);REQUIRE(tokens[1].lexeme=="m");
    REQUIRE(tokens[5].kind==synvanta::TokenKind::Endmodule);REQUIRE(lexer.diagnostics().empty());
}
TEST_CASE(lexer_reports_unexpected_characters){
    synvanta::Lexer lexer("module m(); @ endmodule","bad.sv");(void)lexer.tokenize();
    REQUIRE(lexer.diagnostics().size()==1);REQUIRE(lexer.diagnostics().front().code=="LEX0001");
}
TEST_CASE(lexer_separates_ternary_question_from_sized_binary_literal) {
    synvanta::Lexer lexer("module m(); assign y=1'b1?a:b; endmodule", "ternary.sv");
    const auto tokens = lexer.tokenize();
    REQUIRE(lexer.diagnostics().empty());
    bool found_literal = false;
    bool found_question = false;
    for (const auto& token : tokens) {
        found_literal = found_literal ||
            (token.kind == synvanta::TokenKind::Number && token.lexeme == "1'b1");
        found_question = found_question || token.kind == synvanta::TokenKind::Question;
    }
    REQUIRE(found_literal);
    REQUIRE(found_question);
}
