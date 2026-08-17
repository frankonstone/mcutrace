#include <mcutrace/source_annotations.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(source_annotations, imports_file_scope, "REQ-0088", "REQ-0089") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/include/widget.hpp",
        .base_directory = "/work/project",
        .content = "// @req-file REQ-0001 REQ-0002\n#pragma once\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->nodes[0].id, std::string("source:/work/project/include/widget.hpp"));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->edges[0].type.kind, mcutrace::RelationshipKind::implements);
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("file"));
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0001"));
}

TEST(source_annotations, binds_to_class, "REQ-0088", "REQ-0090", "REQ-0091") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/include/widget.hpp",
        .base_directory = "/work/project",
        .content = "// @req REQ-0003\nclass Widget final {\n};\n",
    };

    const auto result = mcutrace::import_source_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0003"));
    ASSERT_EQ(result->edges[0].provenance.scope, std::string("class"));
    ASSERT_EQ(result->edges[0].provenance.symbol, std::string("Widget"));
    ASSERT_TRUE(result->edges[0].source.has_value());
    ASSERT_EQ(result->edges[0].source->line, static_cast<std::uint32_t>(1));
}

TEST(source_annotations, binds_to_method_definition, "REQ-0088", "REQ-0090", "REQ-0091") {
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

TEST(source_annotations, binds_to_function_declaration_in_header, "REQ-0088", "REQ-0090") {
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

TEST(source_annotations, diagnoses_malformed_requirement, "REQ-0088") {
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

TEST(source_annotations, diagnoses_unsupported_variable_target, "REQ-0090") {
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

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
