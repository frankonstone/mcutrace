#include <mcutrace/build_annotations.hpp>

#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <cstdint>
#include <string>

TEST(build_annotations, imports_cmake_build_evidence, "REQ-0081") {
    const mcutrace::ArtifactInput input{
        .path = "cmake/CMakeLists.txt",
        .base_directory = "/work/project",
        .content = "# @req REQ-0001 REQ-0042\nproject(example)\n",
    };

    const auto result = mcutrace::import_build_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->diagnostics.size(), std::size_t{0});
    ASSERT_EQ(result->nodes.size(), std::size_t{1});
    ASSERT_EQ(result->nodes[0].id,
              std::string("artifact:cmake:/work/project/cmake/CMakeLists.txt"));
    ASSERT_EQ(result->nodes[0].kind, mcutrace::NodeKind::artifact);
    ASSERT_EQ(result->nodes[0].source->line, std::uint32_t{1});
    ASSERT_EQ(result->edges.size(), std::size_t{2});
    ASSERT_EQ(result->edges[0].source_id, result->nodes[0].id);
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0001"));
    ASSERT_EQ(result->edges[0].type.kind, mcutrace::RelationshipKind::verifies);
    ASSERT_EQ(result->edges[0].source->line, std::uint32_t{1});
    ASSERT_EQ(result->edges[1].target_id, std::string("REQ-0042"));
}

TEST(build_annotations, diagnoses_invalid_or_empty_annotations, "REQ-0081") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/CMakeLists.txt",
        .content = "# @req REQ-1\n# @req\n",
    };

    const auto result = mcutrace::import_build_annotations(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), std::size_t{0});
    ASSERT_EQ(result->edges.size(), std::size_t{0});
    ASSERT_EQ(result->diagnostics.size(), std::size_t{3});
    ASSERT_EQ(result->diagnostics[0].code,
              std::string("build.annotation.invalid_requirement"));
    ASSERT_EQ(result->diagnostics[1].code,
              std::string("build.annotation.missing_requirement"));
    ASSERT_EQ(result->diagnostics[2].code,
              std::string("build.annotation.missing_requirement"));
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
