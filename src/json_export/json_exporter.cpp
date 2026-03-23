#include "json_exporter.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

static json span_to_range(const Span& span) {
  int row = std::max(0, (int)span.row - 1);
  int s_col = std::max(0, (int)span.start_col - 1);
  int e_col = std::max(0, (int)span.end_col - 1);
  return {{"start", {{"line", row}, {"character", s_col}}},
          {"end", {{"line", row}, {"character", e_col}}}};
}

/* standard hover on a node */
static void emit_hover(json& hover, const AstPtr& node, const std::string& contents, Type* type = nullptr) {
  json h;
  h["range"] = span_to_range(node->token->get_span());
  h["contents"] = contents;
  if (type) {
    h["type"] = type->to_string();
    h["bytes"] = type->get_byte_size();
  }
  hover.push_back(h);
}

static void collect_hover(const AstPtr& node, json& hover, const SymTab* symtab) {
  if (!node || !node->token) return;

  // Ident
  if (auto ident = dynamic_cast<const IdentifierNode*>(node)) {
    json h;
    h["range"] = span_to_range(ident->token->get_span());
    h["contents"] = ident->name_str;
    if (ident->expr_type) {
      h["type"] = ident->expr_type->to_string();
      h["bytes"] = ident->expr_type->get_byte_size();
    }
    if (auto var_sym = symtab->lookup<Variable>(ident->name_str, ident->scope_id)) {
      h["def_scope_id"] = var_sym->scope_id;
      h["modifier"] = variable_borrowed_state_to_string(var_sym->modifier);
    }
    hover.push_back(h);
  }


  // Literals
  else if (auto lit = dynamic_cast<const IntegerLiteralNode*>(node)) {
    emit_hover(hover, node, std::to_string(lit->value), lit->expr_type);
  } else if (auto lit = dynamic_cast<const FloatLiteralNode*>(node)) {
    emit_hover(hover, node, std::to_string(lit->value), lit->expr_type);
  } else if (auto lit = dynamic_cast<const StringLiteralNode*>(node)) {
    emit_hover(hover, node, lit->value, lit->expr_type);
  } else if (auto lit = dynamic_cast<const BoolLiteralNode*>(node)) {
    emit_hover(hover, node, lit->value ? "true" : "false", lit->expr_type);
  } else if (auto lit = dynamic_cast<const NullLiteralNode*>(node)) {
    emit_hover(hover, node, "null", lit->expr_type);
  }

  else if (auto struct_lit = dynamic_cast<const StructLiteralNode*>(node)) {
    json h;
    h["range"] = span_to_range(struct_lit->token->get_span());
    h["contents"] = struct_lit->token->get_lexeme();
    h["type"] = struct_lit->struct_decl->type->to_string();
    h["bytes"] = struct_lit->struct_decl->struct_size;
    h["def_scope_id"] = struct_lit->struct_decl->scope_id;
    hover.push_back(h);

    for (const StructFieldInitPtr& init : struct_lit->initializers) {
      if (init && init->field) collect_hover(init->field, hover, symtab);
      if (init && init->value) collect_hover(init->value, hover, symtab);
    }
  }

  // ExpressionNode: check for sub-expressions
  if (auto assign = dynamic_cast<const AssignmentNode*>(node)) {
    collect_hover(assign->lvalue, hover, symtab);
    collect_hover(assign->rvalue, hover, symtab);
  } else if (auto imp_cast = dynamic_cast<const ImplicitCastNode*>(node)) {
    collect_hover(imp_cast->expression, hover, symtab);
  } else if (auto exp_cast = dynamic_cast<const ExplicitCastNode*>(node)) {
    collect_hover(exp_cast->expression, hover, symtab);
  } else if (auto bin = dynamic_cast<const BinaryOpExprNode*>(node)) {
    collect_hover(bin->left, hover, symtab);
    collect_hover(bin->right, hover, symtab);
  } else if (auto un = dynamic_cast<UnaryExprNode*>(node)) {
    collect_hover(un->operand, hover, symtab);
  } else if (auto call = dynamic_cast<const FunctionCallNode*>(node)) {
    collect_hover(call->callee, hover, symtab);
    for (const ArgPtr& arg : call->arguments) {
      if (arg && arg->expression) collect_hover(arg->expression, hover, symtab);
    }
  } else if (auto field = dynamic_cast<const FieldAccessNode*>(node)) {
    collect_hover(field->object, hover, symtab);
    collect_hover(field->field, hover, symtab);
  } else if (auto arr = dynamic_cast<const ArrayIndexNode*>(node)) {
    collect_hover(arr->object, hover, symtab);
    collect_hover(arr->index, hover, symtab);
  } else if (auto group = dynamic_cast<const GroupedExprNode*>(node)) {
    collect_hover(group->expression, hover, symtab);
  } else if (auto new_expr = dynamic_cast<const NewExprNode*>(node)) {
    if (new_expr->allocation_specifier) {
      collect_hover(new_expr->allocation_specifier, hover, symtab);
    }
  }

  // StatementNode: check for sub-statements/expressions
  else if (auto var = dynamic_cast<const VariableDeclNode*>(node)) {
    if (var->name) {
      collect_hover(var->name, hover, symtab);
    }
    if (var->initializer) {
      collect_hover(var->initializer, hover, symtab);
    }
  } else if (auto block = dynamic_cast<const BlockNode*>(node)) {
    for (const StmtPtr& stmt : block->statements) {
      collect_hover(stmt, hover, symtab);
    }
  } else if (auto ifs = dynamic_cast<const IfStmtNode*>(node)) {
    collect_hover(ifs->condition, hover, symtab);
    if (ifs->then_branch) {
      collect_hover(ifs->then_branch, hover, symtab);
    }
    if (ifs->else_branch) {
      collect_hover(ifs->else_branch, hover, symtab);
    }
  } else if (auto fors = dynamic_cast<const ForStmtNode*>(node)) {
    if (fors->initializer) {
      if (std::holds_alternative<ExprPtr>(*fors->initializer)) {
        collect_hover(std::get<ExprPtr>(*fors->initializer), hover, symtab);
      } else if (std::holds_alternative<StmtPtr>(*fors->initializer)) {
        collect_hover(std::get<StmtPtr>(*fors->initializer), hover, symtab);
      }
    }
    if (fors->condition) collect_hover(fors->condition, hover, symtab);
    if (fors->iteration) collect_hover(fors->iteration, hover, symtab);
    if (fors->body) collect_hover(fors->body, hover, symtab);
  } else if (auto whiles = dynamic_cast<const WhileStmtNode*>(node)) {
    if (whiles->condition) collect_hover(whiles->condition, hover, symtab);
    if (whiles->body) collect_hover(whiles->body, hover, symtab);
  } else if (auto sw = dynamic_cast<const SwitchStmtNode*>(node)) {
    if (sw->expression) {
      collect_hover(sw->expression, hover, symtab);
    }
    for (const CasePtr& c : sw->cases) {
      if (c) {
        collect_hover(c, hover, symtab);
      }
    }
  } else if (auto cs = dynamic_cast<const CaseNode*>(node)) {
    if (cs->value) collect_hover(cs->value, hover, symtab);
    if (cs->body) collect_hover(cs->body, hover, symtab);
  } else if (auto print = dynamic_cast<const PrintStmtNode*>(node)) {
    for (const ExprPtr& e : print->expressions) {
      collect_hover(e, hover, symtab);
    }
  } else if (auto exprstmt = dynamic_cast<const ExpressionStatementNode*>(node)) {
    if (exprstmt->expression) {
      collect_hover(exprstmt->expression, hover, symtab);
    }
  } else if (auto ret = dynamic_cast<const ReturnStmtNode*>(node)) {
    if (ret->value) {
      collect_hover(ret->value, hover, symtab);
    }
  } else if (auto free_stmt = dynamic_cast<const FreeStmtNode*>(node)) {
    if (free_stmt->expression) {
      collect_hover(free_stmt->expression, hover, symtab);
    }
  } else if (auto err = dynamic_cast<const ErrorStmtNode*>(node)) {
    for (const ExprPtr& e : err->expressions) {
      collect_hover(e, hover, symtab);
    }
  } else if (auto exit = dynamic_cast<const ExitStmtNode*>(node)) {
    if (exit->exit_code) {
      collect_hover(exit->exit_code, hover, symtab);
    }
  } else if (auto asmblk = dynamic_cast<const AsmBlockNode*>(node)) {
    // no sub-nodes
  } else if (auto struct_field = dynamic_cast<const StructFieldNode*>(node)) {
    if (struct_field->name) {
      collect_hover(struct_field->name, hover, symtab);
    }
  } else if (auto struct_decl = dynamic_cast<const StructDeclNode*>(node)) {
    for (const StructFieldPtr& f : struct_decl->fields) {
      if (f) {
        collect_hover(f, hover, symtab);
      }
    }
  } else if (auto param = dynamic_cast<const ParamNode*>(node)) {
    if (param->name) {
      collect_hover(param->name, hover, symtab);
    }
  } else if (auto func = dynamic_cast<const FunctionDeclNode*>(node)) {
    if (func->name) collect_hover(func->name, hover, symtab);
    if (func->retvar_name) collect_hover(func->retvar_name, hover, symtab);
    for (const auto& p : func->params) {
      if (p) {
        collect_hover(p, hover, symtab);
      }
    }
    if (func->body) collect_hover(func->body, hover, symtab);
  }
}

static void collect_definitions(const SymTab* symtab, json& definitions) {
  if (!symtab) return;

  const std::vector<Scope>& scopes = symtab->get_scopes();
  for (size_t scope_id = 0; scope_id < scopes.size(); ++scope_id) {
    const Scope& scope = scopes[scope_id];
    // variables and types
    for (const auto& [name, symbol] : scope.get_symbols()) {
      bool is_var = symbol.kind == Symbol::Variable;

      json def;
      def["name"] = name;
      def["kind"] = is_var ? "variable" : "type";
      def["scope_id"] = scope_id;

      if (is_var) {
        Variable* var = symbol.as<Variable>();
        if (var && var->type) {
          def["type"] = var->type->to_string();
        }
        def["def_range"] = span_to_range(var->span);
        definitions.push_back(def);
      } else if (symbol.kind == Symbol::Type) {
        Type* type = symbol.as<Type>();
        // only add structs
        if (type && type->is<Type::Named>()) {
          const auto& named_type = type->as<Type::Named>();
          StructDeclPtr struct_decl =
              symtab->lookup_struct(named_type.identifier, scope_id);
          if (struct_decl && struct_decl->token) {
            def["type"] = type->to_string();
            def["def_range"] = span_to_range(struct_decl->token->get_span());
            definitions.push_back(def);
          }
        }
      }
    }
  }
}

std::string JsonExporter::export_to_json() {
  json result;

  // == Diagnostics ==
  json diagnostics = json::array();
  for (const Diagnostic& d : m_logger->get_fatals()) {
    diagnostics.push_back({{"range", span_to_range(d.span)},
                           {"message", d.to_string()},
                           {"severity", 3}});
    // mark as information to LSP, as it is usually an internal error
  }
  for (const Diagnostic& d : m_logger->get_errors()) {
    diagnostics.push_back({{"range", span_to_range(d.span)},
                           {"message", d.to_string()},
                           {"severity", 1}});
  }
  for (const Diagnostic& d : m_logger->get_warnings()) {
    diagnostics.push_back({{"range", span_to_range(d.span)},
                           {"message", d.to_string()},
                           {"severity", 2}});
  }
  result["diagnostics"] = diagnostics;

  // == Goto Definitions ==
  json definitions = json::array();
  collect_definitions(m_symtab, definitions);
  result["definitions"] = definitions;

  // == Hover ==
  json hover = json::array();
  if (m_ast_nodes) {
    for (const AstPtr& node : *m_ast_nodes) {
      collect_hover(node, hover, m_symtab);
    }
  }
  result["hover"] = hover;

  return result.dump(2);
}
