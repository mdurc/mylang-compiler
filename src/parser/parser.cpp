#include "parser.h"
#include <string_view>

struct ParsePanic : public std::runtime_error {
  ParsePanic() : std::runtime_error("ParsePanic") {}
};

#define _panic_mode() throw ParsePanic();
#define _assert(cond, msg)                                                   \
  do {                                                                       \
    if (!(cond)) {                                                           \
      m_logger->report(Diag::Fatal(current()->get_span(), std::string("Internal: ") + msg)); \
      throw std::runtime_error(msg); /* compiler crash */                    \
    }                                                                        \
  } while (false)

#define _consume(_type)    \
  do {                     \
    if (!consume(_type)) { \
      _panic_mode()        \
    }                      \
  } while (0)

#define _alloc(_type, ...) m_arena->make<_type>(__VA_ARGS__)

std::string_view Parser::intern_string(const std::string& str) {
  char* mem = static_cast<char*>(m_arena->allocate(str.size() + 1, 1));
  std::memcpy(mem, str.c_str(), str.size());
  mem[str.size()] = '\0';
  return std::string_view(mem, str.size());
}

// == Movement Operations through Tokens ==
const Token* Parser::current() {
  if (m_pos < m_tokens.size()) {
    return &m_tokens[m_pos];
  }
  m_logger->report(Diag::Error(m_tokens.back().get_span(), "Attempting to access current position when out of bounds"));
  _panic_mode();
}

// return token at the m_pos in the parser and then increment pos
const Token* Parser::advance() {
  if (m_pos < m_tokens.size()) {
    return &m_tokens[m_pos++];
  }
  m_logger->report(Diag::Error(m_tokens.back().get_span(), "Attempting to access current position and advance when out of bounds"));
  _panic_mode();
}

// consume an expected type character at current position and increment position
bool Parser::consume(TokenType type) {
  if (match(type)) {
    advance();
    return true;
  }
  const Token* curr = current();
  m_logger->report(Diag::ExpectedToken(curr->get_span(), token_type_to_string(type), token_type_to_string(curr->get_type())));
  return false;
}

// check if the type of the Token at m_pos of the parser equals type
bool Parser::match(TokenType type) const {
  return m_pos < m_tokens.size() && m_tokens[m_pos].get_type() == type;
}

bool Parser::peek_next(TokenType type) const {
  if (m_pos + 1 < m_tokens.size()) {
    return m_tokens[m_pos + 1].get_type() == type;
  }
  return false;
}

// == Panic Mode Recovery ==
bool Parser::is_sync_point(const Token* tok) const {
  if (!tok) return false;
  switch (tok->get_type()) {
    case TokenType::SEMICOLON:
    case TokenType::FUNC:
    case TokenType::IF:
    case TokenType::STRUCT:
    case TokenType::WHILE:
    case TokenType::FOR:
    case TokenType::RETURN:
    case TokenType::READ:
    case TokenType::PRINT:
    case TokenType::LBRACE: return true;
    default: return false;
  }
}

void Parser::synchronize() {
  while (m_pos < m_tokens.size()) {
    if (is_sync_point(current())) {
      if (current()->get_type() == TokenType::SEMICOLON) {
        advance();
      }
      return;
    }
    advance();
  }
}

std::vector<AstPtr> Parser::parse_program(SymTab* symtab, const std::vector<Token>& tokens) {
  m_symtab = symtab;
  m_tokens = tokens;
  m_pos = 0;
  m_root.clear();

  while (m_pos < m_tokens.size()) {
    AstPtr node = parse_toplevel_declaration();
    if (node) {
      m_root.push_back(node);
    }
  }

  return m_root;
}

AstPtr Parser::parse_toplevel_declaration() {
  try {
    if (match(TokenType::STRUCT)) {
      return parse_struct_decl();
    } else if (match(TokenType::FUNC)) {
      return parse_function_decl();
    } else {
      return parse_statement();
    }
  } catch (const ParsePanic&) {
    synchronize();
    return _alloc(PoisonStmtNode, current(), m_symtab->current_scope());
  }
}

// Structs
// <StructDecl> ::= 'struct' Identifier '{' <StructFields>? '}'
AstPtr Parser::parse_struct_decl() {
  _consume(TokenType::STRUCT);

  const Token* name_tok = current();
  _consume(TokenType::IDENTIFIER);

  std::string_view type_name = name_tok->get_lexeme();
  size_t scope_id = m_symtab->current_scope();

  Type* struct_type = _alloc(Type, Type::Named(type_name), scope_id, Type::PTR_SIZE);

  // declare it and see if it already exists (in which an error should occur)
  Type* sym_struct_type = m_symtab->declare_type(type_name, struct_type, scope_id);
  if (!sym_struct_type) {
    m_logger->report(Diag::DuplicateDeclaration(name_tok->get_span(), std::string(type_name)));
    return nullptr;
  }

  // now link the struct declaration node to the symbol table for the IR
  StructDeclPtr struct_node =
      _alloc(StructDeclNode, name_tok, m_symtab->current_scope(), sym_struct_type,
           std::vector<StructFieldPtr>{});
  if (!m_symtab->declare_struct(type_name, struct_node, scope_id)) {
    m_logger->report(Diag::DuplicateDeclaration(name_tok->get_span(), "struct " + std::string(type_name)));
    _panic_mode();
  }

  m_symtab->enter_scope(); // new scope for the contents

  _consume(TokenType::LBRACE);

  if (!match(TokenType::RBRACE)) {
    do {
      struct_node->fields.push_back(parse_struct_field());
    } while (match(TokenType::COMMA) && advance());
  }

  _consume(TokenType::RBRACE);

  m_symtab->exit_scope();

  return struct_node;
}

// <StructField> ::= Identifier ':' <Type>
StructFieldPtr Parser::parse_struct_field() {
  const Token* name_tok = current();
  _consume(TokenType::IDENTIFIER);

  IdentPtr name_ident = _alloc(IdentifierNode, name_tok,
                             m_symtab->current_scope(), name_tok->get_lexeme());
  _consume(TokenType::COLON);

  Type* type = parse_type();

  // declare it as a variable within the current scope which is struct decl
  Variable* field_var = _alloc(Variable,
      name_tok->get_lexeme(), name_tok->get_span(),
      BorrowState::MutablyOwned, type, m_symtab->current_scope());

  // check if the field is a duplicate to another field
  if (!m_symtab->declare_variable(field_var->name, field_var, field_var->scope_id)) {
    m_logger->report(Diag::DuplicateDeclaration(name_tok->get_span(), std::string(name_tok->get_lexeme())));
    _panic_mode();
  }

  return _alloc(StructFieldNode, name_tok, m_symtab->current_scope(), name_ident, type);
}

// Functions
// <FunctionDecl> ::= 'func' Identifier '(' <Params>? ')' <ReturnType>?
// <Block>
FuncDeclPtr Parser::parse_function_decl() {
  const Token* func_tok = current();
  _consume(TokenType::FUNC);

  const Token* name_tok = current();
  _consume(TokenType::IDENTIFIER);

  IdentPtr name_ident = _alloc(IdentifierNode, name_tok,
                             m_symtab->current_scope(), name_tok->get_lexeme());

  m_symtab->enter_scope();

  _consume(TokenType::LPAREN);

  std::vector<ParamPtr> params_vec;
  if (!match(TokenType::RPAREN)) {
    do {
      params_vec.push_back(parse_function_param());
    } while (match(TokenType::COMMA) && advance());
  }

  _consume(TokenType::RPAREN);

  std::pair<IdentPtr, Type*> return_t = {nullptr, nullptr};
  if (match(TokenType::RETURNS)) {
    return_t = parse_function_return_type();
  } else {
    return_t.second = m_symtab->lookup<Type>("u0", 0);
  }
  _assert(return_t.second != nullptr, "func return type should be non-null");

  BlockPtr body_block = parse_block(false);

  m_symtab->exit_scope();

  // Construct FunctionType for the symbol table declaration
  std::vector<Type::ParamInfo> ft_params;
  for (const ParamPtr& ast_param : params_vec) {
    ft_params.push_back({ast_param->type, ast_param->modifier});
  }

  // make it at the global scope, as we don't allow member functions
  Type ft(Type::Function(std::move(ft_params), return_t.second), 0);
  std::string_view name = intern_string(ft.to_string());

  // do not redeclare function families
  Type* fn_type = m_symtab->lookup<Type>(name, 0);
  if (!fn_type) {
    // it doesn't exist, so we can add it as a new func-type
    fn_type = m_symtab->declare_type(name, _alloc(Type, ft), 0);
  }

  // declare it as a var with the associated type (also at global scope)
  Variable* var = _alloc(Variable, name_tok->get_lexeme(), name_tok->get_span(),
               BorrowState::ImmutablyOwned, fn_type, 0);
  if (!m_symtab->declare_variable(var->name, var, 0)) {
    m_logger->report(Diag::DuplicateDeclaration(name_tok->get_span(), std::string(name_tok->get_lexeme())));
    _panic_mode();
  }

  return _alloc(FunctionDeclNode, func_tok, m_symtab->current_scope(), name_ident,
              std::move(params_vec), return_t.first, return_t.second, body_block);
}

BorrowState Parser::parse_function_param_prefix() {
  BorrowState modifier = BorrowState::ImmutablyBorrowed; // default
  if (match(TokenType::MUT)) {
    modifier = BorrowState::MutablyBorrowed;
    advance();
  } else if (match(TokenType::IMM)) {
    advance(); // imm borrowed is already the default
  } else if (match(TokenType::TAKE)) {
    advance();
    modifier = BorrowState::ImmutablyOwned; // Take ==> imm owned by default
    if (match(TokenType::MUT)) {
      modifier = BorrowState::MutablyOwned;
      advance();
    } else if (match(TokenType::IMM)) {
      advance();
    }
  }
  return modifier;
}

// <Param> ::= <FunctionParamPrefix>? Identifier ':' <Type>
ParamPtr Parser::parse_function_param() {
  const Token* first_tok = current();

  BorrowState modifier = parse_function_param_prefix();

  const Token* name_tok = current();
  _consume(TokenType::IDENTIFIER);

  IdentPtr name_ident = _alloc(IdentifierNode, name_tok,
                             m_symtab->current_scope(), name_tok->get_lexeme());

  _consume(TokenType::COLON);

  Type* type_val = parse_type();

  // declare the parameter as a variable in current scope
  Variable* param_var = _alloc(Variable, name_tok->get_lexeme(), name_tok->get_span(), modifier,
                     type_val, m_symtab->current_scope());
  if (!m_symtab->declare_variable(param_var->name, param_var, param_var->scope_id)) {
    m_logger->report(Diag::DuplicateDeclaration(name_tok->get_span(), std::string(name_tok->get_lexeme())));
    _panic_mode();
  }
  return _alloc(ParamNode, first_tok, m_symtab->current_scope(), modifier, name_ident, type_val);
}

// <ReturnType> ::= 'returns' '(' Identifier ':' <Type> ')'
std::pair<IdentPtr, Type*> Parser::parse_function_return_type() {
  _consume(TokenType::RETURNS);
  _consume(TokenType::LPAREN);

  const Token* ident_tok = current();
  _consume(TokenType::IDENTIFIER);

  _consume(TokenType::COLON);

  Type* type_val = parse_type();

  _consume(TokenType::RPAREN);

  // MutablyOwned is required
  size_t scope = m_symtab->current_scope();
  Variable* return_var = _alloc(Variable, ident_tok->get_lexeme(), ident_tok->get_span(),
                      BorrowState::MutablyOwned, type_val, scope, true);
  if (!m_symtab->declare_variable(return_var->name, return_var, return_var->scope_id)) {
    m_logger->report(Diag::DuplicateDeclaration(ident_tok->get_span(), std::string(ident_tok->get_lexeme())));
    _panic_mode();
  }

  IdentPtr ident = _alloc(IdentifierNode, ident_tok, scope, ident_tok->get_lexeme());
  return {ident, type_val};
}

bool Parser::is_next_var_decl() {
  if (match(TokenType::MUT) || match(TokenType::IMM)) {
    return true;
  } else if (match(TokenType::IDENTIFIER)) {
    // with either colon or walrus it will be a variable declaration
    if ((peek_next(TokenType::COLON) || peek_next(TokenType::WALRUS))) {
      return true;
    }
  }
  return false;
}

// Statements
StmtPtr Parser::parse_statement() {
  try {
    // Handle case where the identifier can be the start of a var declaration
    if (is_next_var_decl()) {
      return parse_var_decl();
    }

    TokenType type = current()->get_type();
    switch (type) {
      case TokenType::IF: return parse_if_stmt();
      case TokenType::FOR: return parse_for_stmt();
      case TokenType::WHILE: return parse_while_stmt();
      case TokenType::SWITCH: return parse_switch_stmt();
      case TokenType::READ:
      {
        const Token* read_tok = advance();
        ExprPtr expr = parse_expression(); // should be an identifier, verified by type checker
        _consume(TokenType::SEMICOLON);
        return _alloc(ReadStmtNode, read_tok, m_symtab->current_scope(), expr);
      }
      case TokenType::PRINT: return parse_print_stmt();
      case TokenType::LBRACE: return parse_block(true);
      case TokenType::RETURN: return parse_return_stmt();
      case TokenType::BREAK: return parse_break_stmt();
      case TokenType::CONTINUE: return parse_continue_stmt();
      case TokenType::FREE: return parse_free_stmt();
      case TokenType::ERROR_KW: return parse_error_stmt();
      case TokenType::EXIT_KW: return parse_exit_stmt();
      case TokenType::ASM: return parse_asm_block();
      default:
      {
        // <Expr> ';'
        const Token* expr_start_tok = current();
        ExprPtr expr = parse_expression();

        // return the expression stmt
        _consume(TokenType::SEMICOLON);
        return _alloc(ExpressionStatementNode, expr_start_tok, m_symtab->current_scope(), expr);
      }
    }
  } catch (const ParsePanic&) {
    synchronize();
    return _alloc(PoisonStmtNode, current(), m_symtab->current_scope());
  }
}

// <VarDecl> ::= <TypePrefix>? Identifier ( ( ':' <Type> ( '=' <Expr> )? ) | (
// ':=' <Expr> ) )
StmtPtr Parser::parse_var_decl() {
  const Token* start_tok = current();

  bool is_mutable = false; // imm by default
  if (match(TokenType::MUT)) {
    advance();
    is_mutable = true;
  } else if (match(TokenType::IMM)) {
    advance();
  }

  const Token* name_tok = current();
  _consume(TokenType::IDENTIFIER);

  IdentPtr var_name = _alloc(IdentifierNode, name_tok, m_symtab->current_scope(),
                           name_tok->get_lexeme());

  Type* type_val = nullptr;
  ExprPtr initializer = nullptr;

  if (match(TokenType::COLON)) { // ':' <Type> ( '=' <Expr> )?
    // explicit type declaration
    advance();
    type_val = parse_type();
    if (match(TokenType::EQUAL)) {
      advance();
      initializer = parse_expression();
    }
  } else if (match(TokenType::WALRUS)) { // ':=' <Expr>
    // inferred type declaration (type_val is nullptr)
    advance();
    initializer = parse_expression();
  } else {
    m_logger->report(Diag::ExpectedToken(current()->get_span(), ": | :=", token_type_to_string(current()->get_type())));
    _panic_mode();
  }

  _consume(TokenType::SEMICOLON);

  BorrowState bs = is_mutable ? BorrowState::MutablyOwned : BorrowState::ImmutablyOwned;

  Variable* var_sym = _alloc(Variable, name_tok->get_lexeme(), name_tok->get_span(), bs, type_val,
                   m_symtab->current_scope());

  // check if it is a duplicate variable
  if (!m_symtab->declare_variable(var_sym->name, var_sym, var_sym->scope_id)) {
    m_logger->report(Diag::DuplicateDeclaration(name_tok->get_span(), std::string(name_tok->get_lexeme())));
    _panic_mode();
  }

  return _alloc(VariableDeclNode, start_tok, m_symtab->current_scope(),
              is_mutable, var_name, type_val, initializer);
}

// <IfStmt> ::= 'if' '(' <Expr> ')' <Block> ( 'else' ( <Block> | <IfStmt> ) )?
StmtPtr Parser::parse_if_stmt() {
  const Token* if_tok = current();
  _consume(TokenType::IF);
  _consume(TokenType::LPAREN);

  ExprPtr condition = parse_expression();
  _consume(TokenType::RPAREN);

  BlockPtr then_branch = parse_block(true);

  BlockPtr else_branch = nullptr;
  if (match(TokenType::ELSE)) {
    advance();
    if (match(TokenType::IF)) {
      // else if block
      StmtPtr else_if_stmt = parse_if_stmt();
      const Token* else_if_tok = else_if_stmt->token;
      else_branch = _alloc(BlockNode, else_if_tok, m_symtab->current_scope(),
                         std::vector<StmtPtr>{else_if_stmt});
    } else {
      // else block
      else_branch = parse_block(true);
    }
  }
  return _alloc(IfStmtNode, if_tok, m_symtab->current_scope(), condition,
              then_branch, else_branch);
}

// <ForStmt> ::= 'for' '(' <Expr>? ';' <Expr>? ';' <Expr>? ')' <Block>
StmtPtr Parser::parse_for_stmt() {
  const Token* for_tok = current();
  _consume(TokenType::FOR);

  m_symtab->enter_scope();

  _consume(TokenType::LPAREN);

  std::optional<std::variant<ExprPtr, StmtPtr>> initializer = std::nullopt;
  bool has_initializer = !match(TokenType::SEMICOLON);
  if (has_initializer && is_next_var_decl()) {
    initializer = parse_var_decl(); // consumes the semicolon
  } else {
    if (has_initializer) {
      initializer = parse_expression();
      // does not consume the semicolon
    }
    _consume(TokenType::SEMICOLON);
  }

  ExprPtr condition = nullptr;
  if (!match(TokenType::SEMICOLON)) {
    condition = parse_expression();
  }
  _consume(TokenType::SEMICOLON);

  ExprPtr iteration = nullptr;
  if (!match(TokenType::RPAREN)) {
    iteration = parse_expression();
  }
  _consume(TokenType::RPAREN);

  BlockPtr body = parse_block(true);

  m_symtab->exit_scope();

  return _alloc(ForStmtNode, for_tok, m_symtab->current_scope(),
              std::move(initializer), condition, iteration, body);
}

// <WhileStmt> ::= 'while' '(' <Expr> ')' <Block>
StmtPtr Parser::parse_while_stmt() {
  const Token* while_tok = current();
  _consume(TokenType::WHILE);

  _consume(TokenType::LPAREN);

  ExprPtr condition = parse_expression();
  _consume(TokenType::RPAREN);

  BlockPtr body = parse_block(true);

  return _alloc(WhileStmtNode, while_tok, m_symtab->current_scope(), condition, body);
}

// <SwitchStmt> ::= 'switch' '(' <Expr> ')' '{' <Case>* '}'
StmtPtr Parser::parse_switch_stmt() {
  const Token* switch_tok = current();
  _consume(TokenType::SWITCH);

  _consume(TokenType::LPAREN);

  ExprPtr expression = parse_expression();
  _consume(TokenType::RPAREN);

  _consume(TokenType::LBRACE);

  std::vector<CasePtr> cases;
  while (!match(TokenType::RBRACE)) {
    // <Case> ::= 'case' <Expr> ':' <Block> | 'default' ':' <Block>
    const Token* case_tok = current();
    ExprPtr value = nullptr;
    if (match(TokenType::CASE)) {
      advance();
      value = parse_expression();
    } else if (match(TokenType::DEFAULT)) {
      advance(); // value remains nullptr for default
    } else {
      m_logger->report(Diag::ExpectedToken(current()->get_span(), "case | default", token_type_to_string(current()->get_type())));
      _panic_mode();
    }

    _consume(TokenType::COLON);

    BlockPtr body = parse_block(true); // each case will have its own scope

    cases.push_back(
        _alloc(CaseNode, case_tok, m_symtab->current_scope(), body, value));
  }

  _consume(TokenType::RBRACE);

  return _alloc(SwitchStmtNode, switch_tok, m_symtab->current_scope(), expression,
              std::move(cases));
}

// <PrintStmt> ::= 'print'  <Expr> ( ',' <Expr> )*
StmtPtr Parser::parse_print_stmt() {
  const Token* print_tok = advance();

  std::vector<ExprPtr> exprs;
  do {
    exprs.push_back(parse_expression());
  } while (match(TokenType::COMMA) && advance());
  _consume(TokenType::SEMICOLON);
  return _alloc(PrintStmtNode, print_tok, m_symtab->current_scope(), exprs);
}

// == Expression Parsing ==
ExprPtr Parser::parse_expression() {
  // start with lowest precedence in recursive descent
  return parse_assignment();
}

// <ReturnStmt> ::= 'return' <Expr>?
StmtPtr Parser::parse_return_stmt() {
  const Token* ret_tok = current();
  _consume(TokenType::RETURN);

  ExprPtr value = nullptr;
  if (!match(TokenType::SEMICOLON)) {
    value = parse_expression();
  }
  _consume(TokenType::SEMICOLON);

  return _alloc(ReturnStmtNode, ret_tok, m_symtab->current_scope(), value);
}

// <BreakStmt> ::= 'break'
StmtPtr Parser::parse_break_stmt() {
  const Token* break_tok = current();
  _consume(TokenType::BREAK);
  _consume(TokenType::SEMICOLON);
  return _alloc(BreakStmtNode, break_tok, m_symtab->current_scope());
}

// <ContinueStmt> ::= 'continue'
StmtPtr Parser::parse_continue_stmt() {
  const Token* cont_tok = current();
  _consume(TokenType::CONTINUE);
  _consume(TokenType::SEMICOLON);
  return _alloc(ContinueStmtNode, cont_tok, m_symtab->current_scope());
}

// <FreeStmt> ::= 'free' '[]'? <Expr>
StmtPtr Parser::parse_free_stmt() {
  const Token* free_tok = current();
  _consume(TokenType::FREE);

  bool is_array = false;
  if (match(TokenType::LBRACK)) {
    advance();
    _consume(TokenType::RBRACK);

    is_array = true;
  }

  ExprPtr expr = parse_expression();
  _consume(TokenType::SEMICOLON);

  return _alloc(FreeStmtNode, free_tok, m_symtab->current_scope(), is_array, expr);
}

// <ErrorStmt> ::= 'Error' String
StmtPtr Parser::parse_error_stmt() {
  const Token* error_tok = current();
  _consume(TokenType::ERROR_KW);

  const Token* str_tok = current();
  _consume(TokenType::STRING_LITERAL);

  if (!str_tok->is_literal()) {
    m_logger->report(Diag::Error(str_tok->get_span(), "Internal Error: Expected string literal token to have string data"));
    return nullptr;
  }
  const std::string& msg = str_tok->get_string_val();

  _consume(TokenType::SEMICOLON);

  return _alloc(ErrorStmtNode, error_tok, m_symtab->current_scope(), msg);
}

// <ExitStmt> ::= 'exit' (1 | 0)
StmtPtr Parser::parse_exit_stmt() {
  const Token* exit_tok = current();
  _consume(TokenType::EXIT_KW);

  const Token* int_tok = current();
  _consume(TokenType::INT_LITERAL);
  _consume(TokenType::SEMICOLON);

  _assert(int_tok->is_literal(), "exit stmt token should be an integer");

  return _alloc(ExitStmtNode, exit_tok, m_symtab->current_scope(),
              int_tok->get_int_val());
}

// <Block> ::= '{' <Stmt>* '}'
BlockPtr Parser::parse_block(bool create_scope) {
  const Token* lbrace_tok = current();

  if (create_scope) {
    m_symtab->enter_scope();
  }

  _consume(TokenType::LBRACE);
  std::vector<StmtPtr> statements_vec;
  while (!match(TokenType::RBRACE) && m_pos < m_tokens.size()) {
    statements_vec.push_back(parse_statement());
  }
  _consume(TokenType::RBRACE);

  if (create_scope) {
    m_symtab->exit_scope();
  }

  return _alloc(BlockNode, lbrace_tok, m_symtab->current_scope(),
              std::move(statements_vec));
}

// <Stmt> ::= ... | 'asm' <Block> ';'
StmtPtr Parser::parse_asm_block() {
  const Token* asm_tok = current();
  _consume(TokenType::ASM);

  Span prev_span = current()->get_span();
  _consume(TokenType::LBRACE);

  std::string asm_body_str; // raw string for now

  bool is_first = true;
  int brace_level = 1;
  while (m_pos < m_tokens.size()) {
    const Token* current_tok = current();
    TokenType current_type = current_tok->get_type();

    if (current_type == TokenType::LBRACE) {
      brace_level++;
    } else if (current_type == TokenType::RBRACE) {
      brace_level--;
    }

    if (brace_level == 0) {
      // final closing RBRACE
      break;
    }

    const Span& curr_span = current_tok->get_span();

    // Add newlines if current token is on a new line relative to the prev
    if (!is_first) {
      if (curr_span.row > prev_span.row) {
        for (size_t i = 0; i < curr_span.row - prev_span.row; ++i) {
          asm_body_str += '\n';
        }
      } else {
        // curr token is on the same line, separate by space
        asm_body_str += ' ';
      }
    }
    is_first = false;

    asm_body_str += current_tok->get_lexeme();
    prev_span = curr_span;
    advance();
  }

  if (brace_level != 0 || m_pos >= m_tokens.size()) {
    m_logger->report(Diag::ExpectedToken(current()->get_span(), "}", "<none>"));
    _panic_mode();
  }

  _consume(TokenType::RBRACE);
  _consume(TokenType::SEMICOLON);

  return _alloc(AsmBlockNode, asm_tok, m_symtab->current_scope(), asm_body_str);
}

// Expression Hierarchy
ExprPtr Parser::parse_assignment() {
  ExprPtr left = parse_logic_or();

  if (match(TokenType::EQUAL)) {
    const Token* tok = advance();
    ExprPtr right = parse_assignment();
    return _alloc(AssignmentNode, tok, m_symtab->current_scope(), left, right);
  }

  return left;
}

// <LogicalOrExpr> ::= <LogicalAndExpr> ( 'or' <LogicalAndExpr> )*
ExprPtr Parser::parse_logic_or() {
  ExprPtr expr = parse_logic_and();
  while (match(TokenType::OR)) {
    const Token* op_tok = advance();
    ExprPtr right = parse_logic_and();
    expr = _alloc(BinaryOpExprNode, op_tok, m_symtab->current_scope(),
                BinOperator::LogicalOr, expr, right);
  }
  return expr;
}

// <LogicalAndExpr> ::= <EqualityExpr> ( 'and' <EqualityExpr> )*
ExprPtr Parser::parse_logic_and() {
  ExprPtr expr = parse_equality();
  while (match(TokenType::AND)) {
    const Token* op_tok = advance();
    ExprPtr right = parse_equality();
    expr = _alloc(BinaryOpExprNode, op_tok, m_symtab->current_scope(),
                BinOperator::LogicalAnd, expr, right);
  }
  return expr;
}

// <EqualityExpr> ::= <RelationalExpr> ( ( '==' | '!=' ) <RelationalExpr> )*
ExprPtr Parser::parse_equality() {
  ExprPtr expr = parse_relational();
  while (match(TokenType::EQUAL_EQUAL) || match(TokenType::BANG_EQUAL)) {
    const Token* op_tok = advance();
    BinOperator op = (op_tok->get_type() == TokenType::EQUAL_EQUAL)
                         ? BinOperator::Equal
                         : BinOperator::NotEqual;
    ExprPtr right = parse_relational();
    expr = _alloc(BinaryOpExprNode, op_tok, m_symtab->current_scope(), op, expr, right);
  }
  return expr;
}

// <RelationalExpr> ::= <AdditiveExpr> ( ( '<' | '>' | '<=' | '>=' )
// <AdditiveExpr> )*
ExprPtr Parser::parse_relational() {
  ExprPtr expr = parse_additive();
  while (match(TokenType::LANGLE) || match(TokenType::RANGLE) ||
         match(TokenType::LESS_EQUAL) || match(TokenType::GREATER_EQUAL)) {
    const Token* op_tok = advance();

    BinOperator op;
    switch (op_tok->get_type()) {
      case TokenType::LANGLE: op = BinOperator::LessThan; break;
      case TokenType::RANGLE: op = BinOperator::GreaterThan; break;
      case TokenType::LESS_EQUAL: op = BinOperator::LessEqual; break;
      case TokenType::GREATER_EQUAL: op = BinOperator::GreaterEqual; break;
      default: {
        m_logger->report(Diag::Error(op_tok->get_span(), "Unknown binary operator type"));
        _panic_mode();
      }
    }

    ExprPtr right = parse_additive();
    expr = _alloc(BinaryOpExprNode, op_tok, m_symtab->current_scope(), op, expr, right);
  }
  return expr;
}

// <AdditiveExpr> ::= <MultiplicativeExpr> ( ( '+' | '-') <MultiplicativeExpr>
// )*
ExprPtr Parser::parse_additive() {
  ExprPtr expr = parse_multiplicative();
  while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
    const Token* op_tok = advance();
    BinOperator op = (op_tok->get_type() == TokenType::PLUS)
                         ? BinOperator::Plus
                         : BinOperator::Minus;

    ExprPtr right = parse_multiplicative();
    expr = _alloc(BinaryOpExprNode, op_tok, m_symtab->current_scope(), op, expr, right);
  }
  return expr;
}

// <MultiplicativeExpr> ::= <UnaryExpr> ( ( '*' | '/' | '%' ) <UnaryExpr> )*
ExprPtr Parser::parse_multiplicative() {
  ExprPtr expr = parse_unary();
  while (match(TokenType::STAR) || match(TokenType::SLASH) ||
         match(TokenType::MODULO)) {
    const Token* op_tok = advance();

    BinOperator op;
    switch (op_tok->get_type()) {
      case TokenType::STAR: op = BinOperator::Multiply; break;
      case TokenType::SLASH: op = BinOperator::Divide; break;
      case TokenType::MODULO: op = BinOperator::Modulo; break;
      default: {
        m_logger->report(Diag::Error(op_tok->get_span(), "Unknown multiplicative binary operator type"));
        _panic_mode();
      }
    }

    ExprPtr right = parse_unary();
    expr = _alloc(BinaryOpExprNode, op_tok, m_symtab->current_scope(), op, expr, right);
  }
  return expr;
}

// <UnaryExpr> ::= ( '&' <TypePrefix>? | '*' | '!' ) <UnaryExpr> | <PostfixExpr>
ExprPtr Parser::parse_unary() {
  const Token* op_tok = current();

  TokenType type = current()->get_type();
  switch (type) {
    case TokenType::AMPERSAND: {
      advance();
      UnaryOperator op = UnaryOperator::AddressOf;
      if (match(TokenType::MUT)) {
        advance();
        op = UnaryOperator::AddressOfMut;
      } else if (match(TokenType::IMM)) {
        advance(); // still just AddressOf which implies immutable ref
      }
      // recur in order to find joined unary chains
      ExprPtr operand = parse_unary();
      return _alloc(UnaryExprNode, op_tok, m_symtab->current_scope(), op, operand);
    }
    case TokenType::STAR: {
      // dereference operation
      advance();
      ExprPtr operand = parse_unary();
      return _alloc(UnaryExprNode, op_tok, m_symtab->current_scope(),
                  UnaryOperator::Dereference, operand);
    }
    case TokenType::BANG: {
      advance();
      ExprPtr operand = parse_unary();
      return _alloc(UnaryExprNode, op_tok, m_symtab->current_scope(),
                  UnaryOperator::LogicalNot, operand);
    }
    case TokenType::MINUS: {
      advance();
      ExprPtr operand = parse_unary();
      return _alloc(UnaryExprNode, op_tok, m_symtab->current_scope(),
                  UnaryOperator::Negate, operand);
    }
    default: break;
  }
  return parse_postfix();
}

// <PostfixExpr> ::= <PrimaryExpr> ( <PostfixSuffix> )*
ExprPtr Parser::parse_postfix() {
  ExprPtr expr = parse_primary();

  while (true) {
    const Token* op_tok = current();

    TokenType type = current()->get_type();
    switch (type) {
      case TokenType::LBRACK: {
        // Array Index: '[' <Expr> ']'
        advance();
        ExprPtr index_expr = parse_expression();

        _consume(TokenType::RBRACK);
        expr = _alloc(ArrayIndexNode, op_tok, m_symtab->current_scope(), expr,
                    index_expr);
        break;
      }
      case TokenType::LPAREN: {
        // function call
        advance();
        std::vector<ArgPtr> args_vec = parse_args();
        _consume(TokenType::RPAREN);

        expr = _alloc(FunctionCallNode, op_tok, m_symtab->current_scope(), expr,
                    std::move(args_vec));
        break;
      }
      case TokenType::DOT: {
        // field access via dot
        advance();
        const Token* field_name_tok = current();
        _consume(TokenType::IDENTIFIER);

        IdentPtr field_ident =
            _alloc(IdentifierNode, field_name_tok, m_symtab->current_scope(),
                 field_name_tok->get_lexeme());
        expr = _alloc(FieldAccessNode, op_tok, m_symtab->current_scope(), expr,
                    field_ident);
        break;
      }
      default: return expr;
    }
  }
  return expr;
}

// <Args> ::= <Arg> ( ',' <Arg> )*
std::vector<ArgPtr> Parser::parse_args() {
  std::vector<ArgPtr> args_vec;
  if (match(TokenType::RPAREN)) {
    return args_vec;
  }

  do {
    // <Arg> ::= 'give'? <Expr>
    const Token* arg_tok = current();
    bool is_give = false;
    if (match(TokenType::GIVE)) {
      advance();
      is_give = true;
    }

    ExprPtr arg_expr = parse_expression();
    args_vec.push_back(_alloc(ArgumentNode, arg_tok, m_symtab->current_scope(),
                            is_give, arg_expr));
  } while (match(TokenType::COMMA) && advance());

  return args_vec;
}

// <Type> ::= <BasicType> | <StructType> | <PointerType> | <FunctionType>
Type* Parser::parse_type() {
  const Token* type_start_tok = current();
  if (is_basic_type(type_start_tok->get_type())) {
    // Basic types
    advance();
    Type* t = m_symtab->lookup<Type>(type_start_tok->get_lexeme(), 0);
    _assert(t != nullptr, "basic primitive type should be found in the symtab");
    return t;
  }
  if (match(TokenType::IDENTIFIER)) {
    // Struct type
    advance();
    std::string_view name = type_start_tok->get_lexeme();
    size_t scope = m_symtab->current_scope();
    Type* t = m_symtab->lookup<Type>(name, scope);
    if (t == nullptr) {
      m_logger->report(Diag::TypeNotFound(type_start_tok->get_span(), std::string(name)));
      _panic_mode();
    }
    return t;
  } else if (match(TokenType::PTR)) {
    // Pointer type
    // <PointerType> ::= 'ptr' '<' <TypePrefix>? <Type> '>'
    advance();
    _consume(TokenType::LANGLE);

    bool is_mutable = false; // imm by default
    if (match(TokenType::MUT)) {
      advance();
      is_mutable = true;
    } else if (match(TokenType::IMM)) {
      advance();
    }

    Type* pointee = parse_type();
    _consume(TokenType::RANGLE);

    // we do not check the symbol table for this because ptrs of any valid
    // pointee types are allowed. We will add it to the symbol table though.

    Type search_t(Type::Pointer(is_mutable, pointee), m_symtab->current_scope());
    std::string_view name = intern_string(search_t.to_string());

    size_t scope = search_t.get_scope_id();
    Type* ptr_type = m_symtab->lookup<Type>(name, scope);
    if (!ptr_type) {
      // it doesn't exist, so we can add
      ptr_type = m_symtab->declare_type(name, _alloc(Type, search_t), scope);
    }
    return ptr_type;
  } else if (match(TokenType::FUNC)) {
    // <FunctionType> ::= 'func' '(' (<Type> ( ',' <Type> )*)? ')' '->' <Type>
    advance();
    _consume(TokenType::LPAREN);

    std::vector<Type::ParamInfo> param_types;
    if (!match(TokenType::RPAREN)) {
      do {
        size_t pos = m_pos;
        BorrowState modifier = parse_function_param_prefix();
        if (pos != m_pos) {
          // we must have parsed the prefix
          _consume(TokenType::COLON);
        }
        param_types.push_back({parse_type(), modifier});
      } while (match(TokenType::COMMA) && advance());
    }
    _consume(TokenType::RPAREN);

    _consume(TokenType::ARROW);

    Type* ret_type = parse_type();

    // functions are declared at scope 0
    Type func_type(Type::Function(std::move(param_types), ret_type), 0);
    std::string_view name = intern_string(func_type.to_string());
    Type* t = m_symtab->lookup<Type>(name, 0);
    if (t == nullptr) {
      // the function type should already have been declared
      m_logger->report(Diag::TypeNotFound(type_start_tok->get_span(), std::string(name)));
      _panic_mode();
    }
    return t;
  }

  m_logger->report(Diag::ExpectedToken(type_start_tok->get_span(), "<type>", token_type_to_string(type_start_tok->get_type())));
  _panic_mode();
}

// Primary Expressions
ExprPtr Parser::parse_primary() {
  const Token* tok = current();

  if (match(TokenType::INT_LITERAL) || match(TokenType::FLOAT_LITERAL) ||
      match(TokenType::STRING_LITERAL) || match(TokenType::CHAR_LITERAL) ||
      match(TokenType::TRUE) || match(TokenType::FALSE) ||
      match(TokenType::NULL_)) {
    return parse_primitive_literal();
  } else if (match(TokenType::IDENTIFIER)) {
    // could be variable identifier or struct literal

    // if it exists as a type, then it is a struct because that is the only
    // other named type that can exist besides primatives
    std::string_view name = tok->get_lexeme();
    size_t scope = m_symtab->current_scope();
    StructDeclPtr struct_decl = m_symtab->lookup_struct(name, scope);
    if (struct_decl) {
      return parse_struct_literal(struct_decl);
    }

    // otherwise it is an identifier
    advance();
    return _alloc(IdentifierNode, tok, scope, name);
  } else if (match(TokenType::LPAREN)) {
    // Grouped Expression: '(' <Expr> ')'
    advance();
    ExprPtr grouped_expr = parse_expression();
    _consume(TokenType::RPAREN);

    return _alloc(GroupedExprNode, tok, m_symtab->current_scope(), grouped_expr);
  } else if (match(TokenType::NEW)) {
    return parse_new_expr();
  }

  // Otherwise it is invalid
  m_logger->report(Diag::ExpectedToken(tok->get_span(), "<primary>", token_type_to_string(tok->get_type())));
  _panic_mode();
}

// <PrimitiveLiteral> ::= Integer | Float | String | Bool | 'null'
ExprPtr Parser::parse_primitive_literal() {
  const Token* tok = current();

  TokenType type = current()->get_type();
  advance();
  switch (type) {
    case TokenType::INT_LITERAL:
      return _alloc(IntegerLiteralNode, tok, m_symtab->current_scope(),
                  tok->get_int_val());
    case TokenType::FLOAT_LITERAL:
      return _alloc(FloatLiteralNode, tok, m_symtab->current_scope(),
                  tok->get_float_val());
    case TokenType::STRING_LITERAL:
      return _alloc(StringLiteralNode, tok, m_symtab->current_scope(),
                  tok->get_string_val());
    case TokenType::CHAR_LITERAL:
      return _alloc(CharLiteralNode, tok, m_symtab->current_scope(),
                  tok->get_int_val());
    case TokenType::TRUE:
      return _alloc(BoolLiteralNode, tok, m_symtab->current_scope(), true);
    case TokenType::FALSE:
      return _alloc(BoolLiteralNode, tok, m_symtab->current_scope(), false);
    case TokenType::NULL_:
      return _alloc(NullLiteralNode, tok, m_symtab->current_scope());
    default: break;
  }

  m_logger->report(Diag::ExpectedToken(tok->get_span(), "<literal>", token_type_to_string(tok->get_type())));
  _panic_mode();
}

// <StructLiteral> ::= Identifier '(' ( Identifier '=' <Expr> ( ',' Identifier
// '=' <Expr> )* )? ')'
ExprPtr Parser::parse_struct_literal(StructDeclPtr struct_decl) {
  const Token* type_name_tok = current(); // this is the Type that is passed
  _consume(TokenType::IDENTIFIER);

  _consume(TokenType::LBRACE);

  std::vector<StructFieldInitPtr> initializers_vec;
  if (!match(TokenType::RBRACE)) {
    do {
      const Token* field_name_tok = current();
      _consume(TokenType::IDENTIFIER);

      IdentPtr field_ident =
          _alloc(IdentifierNode, field_name_tok, m_symtab->current_scope(),
               field_name_tok->get_lexeme());

      _consume(TokenType::EQUAL);

      ExprPtr value_expr = parse_expression();

      initializers_vec.push_back(_alloc(StructFieldInitializerNode,
                                      field_name_tok, m_symtab->current_scope(),
                                      field_ident, value_expr));
    } while (match(TokenType::COMMA) && advance());
  }

  _consume(TokenType::RBRACE);

  return _alloc(StructLiteralNode, type_name_tok, m_symtab->current_scope(),
              struct_decl, std::move(initializers_vec));
}

// <NewExpr> ::= 'new' '<'<TypePrefix>? <Type>'>'
// ( '['<Expr>']' | '('(<StructLiteral> | <Expr>)?')' )

ExprPtr Parser::parse_new_expr() {
  const Token* new_tok = current();
  _consume(TokenType::NEW);

  _consume(TokenType::LANGLE);

  bool is_memory_mutable = false; // imm by default
  if (match(TokenType::MUT)) {
    advance();
    is_memory_mutable = true;
  } else if (match(TokenType::IMM)) {
    advance();
  }

  Type* allocated_type = parse_type();

  _consume(TokenType::RANGLE);

  bool is_array = false;
  ExprPtr specifier = nullptr;
  if (match(TokenType::LBRACK)) {
    // Array allocation: '[' <Expr> ']'
    is_array = true;
    advance();
    specifier = parse_expression();
    _consume(TokenType::RBRACK);
  } else if (match(TokenType::LPAREN)) {
    // Constructor call: '(' <Expr>? ')'
    advance();
    if (!match(TokenType::RPAREN)) {
      // Check if the current token is the start of a declared Struct Type
      std::string_view name = current()->get_lexeme();
      size_t scope = m_symtab->current_scope();
      StructDeclPtr struct_decl = m_symtab->lookup_struct(name, scope);
      if (struct_decl) {
        // This must be the start of a struct literal
        specifier = parse_struct_literal(struct_decl);
      } else {
        // It is a standard constructor type (primitives, literals, and
        // expressions that resolve to those)
        specifier = parse_expression();
      }
    }
    _consume(TokenType::RPAREN);
  } else {
    m_logger->report(Diag::ExpectedToken(current()->get_span(), "[ | < | (", token_type_to_string(current()->get_type())));
    _panic_mode();
  }
  return _alloc(NewExprNode, new_tok, m_symtab->current_scope(),
              is_memory_mutable, is_array, allocated_type, specifier);
}
