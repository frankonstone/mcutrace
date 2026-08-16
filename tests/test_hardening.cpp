#include <mcutrace/assembly.hpp>
#include <mcutrace/importer.hpp>
#include <mcutrace/validation.hpp>
#include <mcutest/mcutest.hpp>

#include <cstddef>
#include <expected>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

class VendorImporter final : public mcutrace::Importer {
  public:
    [[nodiscard]] mcutrace::ImporterInfo info() const override {
        return mcutrace::ImporterInfo{
            .name = "vendor-example",
            .producer = "vendor",
            .supported_versions = {"1"},
        };
    }

    [[nodiscard]] std::expected<mcutrace::InputFormat, mcutrace::ImportError>
    identify(const mcutrace::ArtifactInput& input) const override {
        if (input.content.find("\"format\": \"vendor-results\"") == std::string::npos) {
            return std::unexpected(mcutrace::ImportError{
                .code = mcutrace::ImportErrorCode::unrecognized_format,
                .detail = "not a vendor-results artifact",
            });
        }
        if (input.content.find("\"version\": 1") == std::string::npos) {
            return std::unexpected(mcutrace::ImportError{
                .code = mcutrace::ImportErrorCode::unsupported_version,
                .detail = "unsupported vendor-results version",
            });
        }
        return mcutrace::InputFormat{
            .producer = "vendor",
            .schema = "vendor-results",
            .version = "1",
        };
    }

    [[nodiscard]] std::expected<mcutrace::ImportFragment, mcutrace::ImportError>
    import(const mcutrace::ArtifactInput& input) const override {
        auto format = identify(input);
        if (!format) return std::unexpected(format.error());

        mcutrace::ImportFragment fragment{.format = *format};
        fragment.nodes.push_back(mcutrace::Node{
            .id = "test:vendor:boot_test",
            .kind = mcutrace::NodeKind::test,
            .label = "boot_test",
            .evidence_state = mcutrace::EvidenceState::passed,
        });
        fragment.edges.push_back(mcutrace::Edge{
            .source_id = "test:vendor:boot_test",
            .target_id = "REQ-0042",
            .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies),
            .provenance = mcutrace::Provenance{
                .importer = "vendor-example",
                .artifact = input.path,
            },
        });
        return fragment;
    }
};

std::string requirement_id(std::size_t value) {
    std::string result = "REQ-";
    result.push_back(static_cast<char>('0' + (value / 1000U) % 10U));
    result.push_back(static_cast<char>('0' + (value / 100U) % 10U));
    result.push_back(static_cast<char>('0' + (value / 10U) % 10U));
    result.push_back(static_cast<char>('0' + value % 10U));
    return result;
}

}  // namespace

TEST(hardening, third_party_importer_uses_public_contract) {
    std::ifstream stream(MCUTRACE_THIRD_PARTY_FIXTURE, std::ios::binary);
    ASSERT_TRUE(static_cast<bool>(stream));
    const std::string content(std::istreambuf_iterator<char>(stream),
                              std::istreambuf_iterator<char>());

    const VendorImporter importer;
    const auto result = importer.import(mcutrace::ArtifactInput{
        .path = "third-party-results.json",
        .content = content,
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->format.schema, std::string("vendor-results"));
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0042"));
}

TEST(hardening, assembles_large_graph_deterministically) {
    constexpr std::size_t kCount = 1000;
    std::vector<mcutrace::Requirement> requirements;
    requirements.reserve(kCount);
    mcutrace::ImportFragment fragment{
        .format = mcutrace::InputFormat{.producer = "stress", .schema = "stress", .version = "1"},
    };
    fragment.nodes.reserve(kCount);
    fragment.edges.reserve(kCount);

    for (std::size_t index = 1; index <= kCount; ++index) {
        const std::string req_id = requirement_id(index);
        requirements.push_back(mcutrace::Requirement{
            .id = req_id,
            .title = "requirement",
            .source = mcutrace::SourceLocation{.path = "requirements.md", .line = static_cast<std::uint32_t>(index)},
            .heading_level = 3,
        });
        const std::string test_id = "test:stress:" + req_id;
        fragment.nodes.push_back(mcutrace::Node{
            .id = test_id,
            .kind = mcutrace::NodeKind::test,
            .label = test_id,
            .evidence_state = mcutrace::EvidenceState::passed,
        });
        fragment.edges.push_back(mcutrace::Edge{
            .source_id = test_id,
            .target_id = req_id,
            .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies),
            .provenance = mcutrace::Provenance{.importer = "stress", .artifact = "stress.json"},
        });
    }

    const std::vector<mcutrace::ImportFragment> fragments{fragment};
    const auto first = mcutrace::assemble_trace(requirements, fragments);
    const auto second = mcutrace::assemble_trace(requirements, fragments);

    ASSERT_EQ(first.graph.nodes().size(), static_cast<std::size_t>(2000));
    ASSERT_EQ(first.graph.edges().size(), kCount);
    ASSERT_EQ(first.graph.nodes(), second.graph.nodes());
    ASSERT_EQ(first.graph.edges(), second.graph.edges());

    mcutrace::ValidationPolicy policy;
    policy.missing_implementation.enabled = false;
    policy.missing_coverage.enabled = false;
    const auto validation = mcutrace::validate_trace(first, policy);
    ASSERT_FALSE(validation.failed);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::StdoutOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
