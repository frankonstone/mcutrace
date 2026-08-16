#include <mcutrace/model.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(model, exposes_stable_node_kind_names) {
    ASSERT_EQ(std::string(mcutrace::node_kind_name(mcutrace::NodeKind::requirement)),
              std::string("requirement"));
    ASSERT_EQ(std::string(mcutrace::node_kind_name(mcutrace::NodeKind::test)),
              std::string("test"));
}
