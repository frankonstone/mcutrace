#include <mcutrace/source_annotations.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <algorithm>
#include <string>

TEST(source_annotations, imports_file_scope, "REQ-0089", "REQ-0090") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/include/widget.hpp",
        .base_directory = "/work/project",
        .content = "// @req-file REQ-0001 REQ-0002\n#pragma once\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->nodes[0].id, std::string("source:/work/project/include/widget.hpp"));
    ASSERT_EQ(result->nodes[1].kind, mcutrace::NodeKind::implementation);
    ASSERT_EQ(result->nodes[1].id,
              std::string("implementation:/work/project/include/widget.hpp#file"));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->edges[0].source_id, result->nodes[1].id);
    ASSERT_EQ(result->edges[0].type.kind, mcutrace::RelationshipKind::implements);
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("file"));
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0001"));
}

TEST(source_annotations, binds_to_class, "REQ-0089", "REQ-0091", "REQ-0092") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/include/widget.hpp",
        .base_directory = "/work/project",
        .content = "// @req REQ-0003\nclass Widget final {\n};\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->nodes[1].kind, mcutrace::NodeKind::implementation);
    ASSERT_NE(result->nodes[1].id.find("#class:Widget@"), std::string::npos);
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].source_id, result->nodes[1].id);
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0003"));
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("class"));
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("Widget"));
    ASSERT_TRUE(result->edges[0].source.has_value());
    ASSERT_EQ(result->edges[0].source->line, static_cast<std::uint32_t>(1));
}

TEST(source_annotations, binds_to_struct, "REQ-0091", "REQ-0092") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/include/widget.hpp",
        .base_directory = "/work/project",
        .content = "// @req REQ-0003\nstruct WidgetData {\n};\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("struct"));
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("WidgetData"));
}

TEST(source_annotations, binds_to_enum, "REQ-0091", "REQ-0092") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/include/widget.hpp",
        .base_directory = "/work/project",
        .content = "// @req REQ-0003\nenum class WidgetState { idle, running };\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("enum"));
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("WidgetState"));
}

TEST(source_annotations, binds_to_method_definition, "REQ-0089", "REQ-0091", "REQ-0092") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/src/widget.cpp",
        .base_directory = "/work/project",
        .content = "// @req REQ-0004 REQ-0005\nstd::expected<void, Error>\nWidget::start(int value)\n{\n    return {};\n}\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("method"));
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("Widget::start"));
}

TEST(source_annotations, binds_to_function_declaration_in_header, "REQ-0089", "REQ-0091") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/include/api.hpp",
        .base_directory = "/work/project",
        .content = "// @req REQ-0006\n[[nodiscard]]\nstd::expected<int, Error> parse_value(std::string_view text);\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("function"));
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("parse_value"));
}

TEST(source_annotations, gives_overloads_distinct_implementation_identities, "REQ-0095") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/src/widget.cpp",
        .base_directory = "/work/project",
        .content =
            "// @req REQ-0001\n"
            "void configure(int value);\n"
            "// @req REQ-0002\n"
            "void configure(std::string_view value);\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(result->nodes[1].kind, mcutrace::NodeKind::implementation);
    ASSERT_EQ(result->nodes[2].kind, mcutrace::NodeKind::implementation);
    ASSERT_NE(result->nodes[1].id, result->nodes[2].id);
    ASSERT_EQ(result->nodes[1].label, std::string("function configure"));
    ASSERT_EQ(result->nodes[2].label, std::string("function configure"));

    const auto repeated = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(repeated.has_value());
    ASSERT_EQ(repeated->nodes[1].id, result->nodes[1].id);
    ASSERT_EQ(repeated->nodes[2].id, result->nodes[2].id);
}

TEST(source_annotations, diagnoses_malformed_requirement, "REQ-0089") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/src/foo.cpp",
        .content = "// @req REQ-1\nvoid foo();\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(0));
    ASSERT_TRUE(result->diagnostics.size() >= static_cast<std::size_t>(1));
    ASSERT_EQ(result->diagnostics[0].code, std::string("source.annotation.invalid_requirement"));
}

TEST(source_annotations, diagnoses_unsupported_variable_target, "REQ-0091") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/src/foo.cpp",
        .content = "// @req REQ-0007\nconstexpr int value = 7;\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(0));
    ASSERT_EQ(result->diagnostics.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->diagnostics[0].code, std::string("source.annotation.unsupported_target"));
}

TEST(source_annotations, selects_scanners_from_normalized_extensions,
     "REQ-0192", "REQ-0193", "REQ-0194", "REQ-0195") {
    ASSERT_EQ(mcutrace::source_language_for_path("src/component.cpp"),
              mcutrace::SourceLanguage::cpp);
    ASSERT_EQ(mcutrace::source_language_for_path("include\\component.hxx"),
              mcutrace::SourceLanguage::cpp);
    ASSERT_EQ(mcutrace::source_language_for_path("src/extension.js"),
              mcutrace::SourceLanguage::javascript);
    ASSERT_EQ(mcutrace::source_language_for_path("src/module.mjs"),
              mcutrace::SourceLanguage::javascript);
    ASSERT_EQ(mcutrace::source_language_for_path("src/module.cjs"),
              mcutrace::SourceLanguage::javascript);
    ASSERT_EQ(mcutrace::source_language_for_path("src/component.CPP"),
              mcutrace::SourceLanguage::unsupported);
    ASSERT_EQ(mcutrace::source_language_for_path("src/component.jsx"),
              mcutrace::SourceLanguage::unsupported);
}

TEST(source_annotations, preserves_file_scope_for_unsupported_languages,
     "REQ-0090", "REQ-0193") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/notes.md",
        .base_directory = "/work/project",
        .content = "<!-- // @req-file REQ-0001 -->\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), std::size_t{1});
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("file"));
    ASSERT_EQ(result->diagnostics.size(), std::size_t{0});
}

TEST(source_annotations, binds_javascript_declaration_forms,
     "REQ-0195", "REQ-0196", "REQ-0198") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/src/component.js",
        .base_directory = "/work/project",
        .content =
            "// @req REQ-0001\n"
            "export async function* load() {}\n"
            "// @req REQ-0002\n"
            "export default class Widget {\n"
            "  // @req REQ-0003\n"
            "  constructor() {}\n"
            "  // @req REQ-0004\n"
            "  static async run() {}\n"
            "  // @req REQ-0005\n"
            "  get value() { return 1; }\n"
            "  // @req REQ-0006\n"
            "  set value(next) {}\n"
            "  // @req REQ-0007\n"
            "  [methodName]() {}\n"
            "}\n"
            "// @req REQ-0008\n"
            "const handler = function named() {};\n"
            "// @req REQ-0009\n"
            "let arrow = async (value) => value;\n"
            "// @req REQ-0010\n"
            "var concise = value => value;\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), std::size_t{10});
    ASSERT_EQ(result->diagnostics.size(), std::size_t{0});
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("function"));
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("load"));
    ASSERT_EQ(result->edges[1].provenance.scope, std::string("class"));
    ASSERT_EQ(result->edges[1].provenance.symbol, std::string("Widget"));
    ASSERT_EQ(result->edges[2].provenance.scope, std::string("method"));
    ASSERT_EQ(result->edges[2].provenance.symbol, std::string("constructor"));
    ASSERT_EQ(result->edges[6].provenance.symbol, std::string("[methodName]"));
    ASSERT_EQ(result->edges[7].provenance.symbol, std::string("handler"));
    ASSERT_EQ(result->edges[8].provenance.symbol, std::string("arrow"));
    ASSERT_EQ(result->edges[9].provenance.symbol, std::string("concise"));
}

TEST(source_annotations, ignores_javascript_marker_text_outside_comments,
     "REQ-0195", "REQ-0197") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/src/lexical.mjs",
        .content =
            "const quoted = \"// @req REQ-0001\";\n"
            "const single = '// @req REQ-0002';\n"
            "const template = `text // @req REQ-0003 ${\"/* @req REQ-0004 */\"}`;\n"
            "const multiline = `first line\n// @req REQ-0004\nlast line`;\n"
            "const pattern = /[/*] @req REQ-0005/gi;\n"
            R"(if (true) /\/\/ @req REQ-0008/.test(pattern);)"
            "\n"
            "/* @req REQ-0006 */\n"
            "// an intervening comment\n"
            "function actual() {}\n"
            "/* @req REQ-0007 */const minified=()=>0;\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), std::size_t{2});
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0006"));
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("actual"));
    ASSERT_EQ(result->edges[1].target_id, std::string("REQ-0007"));
    ASSERT_EQ(result->edges[1].provenance.symbol, std::string("minified"));
}

TEST(source_annotations, uses_javascript_lexical_scope_in_stable_identity,
     "REQ-0095", "REQ-0198") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/src/scopes.cjs",
        .content =
            "class First {\n"
            "  // @req REQ-0001\n"
            "  update() {}\n"
            "}\n"
            "class Second {\n"
            "  // @req REQ-0002\n"
            "  update() {}\n"
            "}\n"
            "function outer() {\n"
            "  // @req REQ-0003\n"
            "  function nested() {}\n"
            "}\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), std::size_t{4});
    ASSERT_NE(result->nodes[1].id, result->nodes[2].id);
    ASSERT_NE(result->nodes[1].id.find("#method:update@"), std::string::npos);
    ASSERT_NE(result->nodes[3].id.find("#function:nested@"), std::string::npos);

    const auto repeated = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(repeated.has_value());
    ASSERT_EQ(repeated->nodes[1].id, result->nodes[1].id);
    ASSERT_EQ(repeated->nodes[2].id, result->nodes[2].id);
    ASSERT_EQ(repeated->nodes[3].id, result->nodes[3].id);

    const auto reformatted = mcutrace::import_source_annotations(mcutrace::ArtifactInput{
        .path = input.path,
        .content =
            "class First{\n// @req REQ-0001\nupdate ( ) { }\n}\n"
            "class Second {\n// @req REQ-0002\nupdate( ){}\n}\n"
            "function outer ( ) {\n// @req REQ-0003\nfunction nested ( ) {}\n}\n",
    });
    ASSERT_TRUE(reformatted.has_value());
    ASSERT_EQ(reformatted->nodes[1].id, result->nodes[1].id);
    ASSERT_EQ(reformatted->nodes[2].id, result->nodes[2].id);
    ASSERT_EQ(reformatted->nodes[3].id, result->nodes[3].id);

    const auto changed_shape = mcutrace::import_source_annotations(mcutrace::ArtifactInput{
        .path = input.path,
        .content = "class First {\n// @req REQ-0001\nupdate(value) {}\n}\n",
    });
    ASSERT_TRUE(changed_shape.has_value());
    ASSERT_NE(changed_shape->nodes[1].id, result->nodes[1].id);
}

TEST(source_annotations, diagnoses_unsupported_languages_and_javascript_targets,
     "REQ-0193", "REQ-0196") {
    const auto unsupported = mcutrace::import_source_annotations(mcutrace::ArtifactInput{
        .path = "/work/project/src/component.ts",
        .content = "// @req REQ-0001\nfunction component() {}\n",
    });
    ASSERT_TRUE(unsupported.has_value());
    ASSERT_EQ(unsupported->edges.size(), std::size_t{0});
    ASSERT_EQ(unsupported->diagnostics[0].code,
              std::string("source.annotation.unsupported_language"));

    const auto anonymous = mcutrace::import_source_annotations(mcutrace::ArtifactInput{
        .path = "/work/project/src/component.js",
        .content = "// @req REQ-0001\nexport default function () {};\n",
    });
    ASSERT_TRUE(anonymous.has_value());
    ASSERT_EQ(anonymous->diagnostics[0].code,
              std::string("source.annotation.unsupported_target"));

    const auto missing = mcutrace::import_source_annotations(mcutrace::ArtifactInput{
        .path = "/work/project/src/component.js",
        .content = "// @req REQ-0001\n",
    });
    ASSERT_TRUE(missing.has_value());
    ASSERT_EQ(missing->diagnostics[0].code, std::string("source.annotation.missing_target"));
}

TEST(source_annotations, distinguishes_nested_declarations_in_sibling_methods,
     "REQ-0194", "REQ-0196", "REQ-0198", "REQ-0199") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/src/nested.js",
        .content =
            "class Container {\n"
            "  first() {\n"
            "    // @req REQ-0001\n"
            "    function helper() {}\n"
            "  }\n"
            "  second() {\n"
            "    // @req REQ-0002\n"
            "    function helper() {}\n"
            "  }\n"
            "}\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), std::size_t{3});
    ASSERT_NE(result->nodes[1].id, result->nodes[2].id);
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("helper"));
    ASSERT_EQ(result->edges[1].provenance.symbol, std::string("helper"));

    const auto javascript = mcutrace::import_source_annotations(mcutrace::ArtifactInput{
        .path = "/work/project/src/component.js",
        .content = "// @req REQ-0001\nclass Component {}\n",
    });
    const auto cpp = mcutrace::import_source_annotations(mcutrace::ArtifactInput{
        .path = "/work/project/src/component.cpp",
        .content = "// @req REQ-0002\nclass Component final {};\n",
    });
    ASSERT_TRUE(javascript.has_value());
    ASSERT_TRUE(cpp.has_value());
    ASSERT_EQ(javascript->edges[0].provenance.scope, std::string("class"));
    ASSERT_EQ(cpp->edges[0].provenance.scope, std::string("class"));
    ASSERT_NE(javascript->edges[0].source_id, cpp->edges[0].source_id);
    ASSERT_EQ(javascript->diagnostics.size(), std::size_t{0});
    ASSERT_EQ(cpp->diagnostics.size(), std::size_t{0});
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
