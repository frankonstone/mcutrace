#include <mcutrace/importer.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <string>
#include <utility>

namespace {

class ExampleImporter final : public mcutrace::Importer {
  public:
    [[nodiscard]] mcutrace::ImporterInfo info() const override {
        return mcutrace::ImporterInfo{
            .name = "example-json",
            .producer = "example",
            .supported_versions = {"1"},
        };
    }

    [[nodiscard]] std::expected<mcutrace::InputFormat, mcutrace::ImportError>
    identify(const mcutrace::ArtifactInput& input) const override {
        if (input.content.find("\"producer\":\"example\"") == std::string::npos) {
            return std::unexpected(mcutrace::ImportError{
                .code = mcutrace::ImportErrorCode::unrecognized_format,
                .detail = "not an example artifact",
            });
        }
        const std::string version =
            input.content.find("\"version\":\"2\"") != std::string::npos ? "2" : "1";
        return mcutrace::InputFormat{
            .producer = "example",
            .schema = "example-result",
            .version = version,
        };
    }

    [[nodiscard]] std::expected<mcutrace::ImportFragment, mcutrace::ImportError>
    import(const mcutrace::ArtifactInput& input) const override {
        auto format = identify(input);
        if (!format) return std::unexpected(format.error());

        auto supported = mcutrace::require_supported_version(info(), *format);
        if (!supported) return std::unexpected(supported.error());

        auto path = mcutrace::normalize_artifact_path(input.path, input.base_directory);
        if (!path) return std::unexpected(path.error());

        mcutrace::ImportFragment result;
        result.format = *format;
        result.artifacts.push_back(mcutrace::preserve_json_artifact(
            "artifact:" + *path, "example.raw", input.content,
            mcutrace::SourceLocation{.path = *path, .line = 1, .column = 1}));
        if (input.content.find("\"malformed\":true") != std::string::npos) {
            result.diagnostics.push_back(mcutrace::Diagnostic{
                .code = "example.malformed_entry",
                .severity = mcutrace::Severity::warning,
                .message = "one example entry was malformed",
                .source = mcutrace::SourceLocation{.path = *path, .line = 3, .column = 1},
            });
        }
        return result;
    }
};

}  // namespace

TEST(importer, normalizes_relative_paths_deterministically, "REQ-0043", "REQ-0044") {
    const auto path = mcutrace::normalize_artifact_path("reports/../out/result.json", "/project");
    ASSERT_TRUE(path.has_value());
    ASSERT_EQ(*path, std::string("/project/out/result.json"));
}

TEST(importer, rejects_empty_artifact_paths, "REQ-0035", "REQ-0043", "REQ-0071") {
    const auto path = mcutrace::normalize_artifact_path("");
    ASSERT_FALSE(path.has_value());
    ASSERT_EQ(path.error().code, mcutrace::ImportErrorCode::invalid_artifact);
}

TEST(importer, reports_unsupported_versions_explicitly, "REQ-0041", "REQ-0042") {
    const ExampleImporter importer;
    const mcutrace::InputFormat format{
        .producer = "example",
        .schema = "example-result",
        .version = "2",
    };
    const auto result = mcutrace::require_supported_version(importer.info(), format);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, mcutrace::ImportErrorCode::unsupported_version);
}

TEST(importer, ingests_external_json_artifacts, "REQ-0002", "REQ-0033", "REQ-0034") {
    const ExampleImporter importer;
    const mcutrace::ArtifactInput input{
        .path = "result.json",
        .base_directory = "/project/out",
        .content = "{\"producer\":\"example\",\"version\":\"1\"}",
    };
    const auto result = importer.import(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->format.schema, std::string("example-result"));
    ASSERT_EQ(result->artifacts.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->artifacts.front().payload, input.content);
}

TEST(importer, keeps_partial_entry_failures_as_diagnostics, "REQ-0035", "REQ-0036", "REQ-0058") {
    const ExampleImporter importer;
    const mcutrace::ArtifactInput input{
        .path = "result.json",
        .base_directory = "/project/out",
        .content = "{\"producer\":\"example\",\"version\":\"1\",\"malformed\":true}",
    };
    const auto result = importer.import(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->format.version, std::string("1"));
    ASSERT_EQ(result->diagnostics.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->artifacts.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->artifacts.front().payload, input.content);
    ASSERT_EQ(result->artifacts.front().source->path, std::string("/project/out/result.json"));
}

TEST(importer, preserves_unknown_json_payload_without_loss, "REQ-0026", "REQ-0032", "REQ-0040") {
    const std::string payload = "{\"vendor_extension\":{\"answer\":42}}";
    const auto artifact = mcutrace::preserve_json_artifact("vendor:1", "vendor.extension", payload);
    ASSERT_EQ(artifact.media_type, std::string("application/json"));
    ASSERT_EQ(artifact.payload, payload);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
