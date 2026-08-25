#include "serialization/ProjectSerializer.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;

namespace {

const serial::ProjectSerializer& serializer() {
    static const serial::ProjectSerializer instance;
    return instance;
}

/// A minimal valid document, as a base for mutation.
std::string minimalDocument() {
    return R"({
  "tnp": {"format": "project", "version": "1.0"},
  "metadata": {"name": "Test"},
  "network": {"devices": [], "links": []}
})";
}

} // namespace

TEST_CASE("Invalid JSON is refused with an explanation", "[serialization][errors]") {
    Project project;

    auto result = serializer().read("this is not json", project);
    REQUIRE_FALSE(result.isOk());
    CHECK(result.message().find("JSON") != std::string::npos);

    CHECK_FALSE(serializer().read("", project).isOk());
    CHECK_FALSE(serializer().read("[1,2,3]", project).isOk()); // an array is not a project
    CHECK_FALSE(serializer().read("42", project).isOk());
}

TEST_CASE("A document of the wrong kind is refused", "[serialization][errors]") {
    Project project;

    auto result = serializer().read(R"({"tnp":{"format":"capture","version":"1.0"}})", project);
    REQUIRE_FALSE(result.isOk());
    CHECK(result.message().find("not a TNP project") != std::string::npos);
}

TEST_CASE("A future major version is refused rather than misread", "[serialization][errors]") {
    Project project;

    auto result = serializer().read(R"({"tnp":{"format":"project","version":"9.0"}})", project);
    REQUIRE_FALSE(result.isOk());
    CHECK(result.message().find("cannot be read") != std::string::npos);
}

TEST_CASE("A newer minor version loads with a warning", "[serialization][errors]") {
    Project project;

    auto result = serializer().read(R"({
      "tnp": {"format": "project", "version": "1.99"},
      "metadata": {"name": "From the future"},
      "network": {"devices": [], "links": []}
    })",
                                    project);

    REQUIRE(result.isOk());
    CHECK(result.value().writtenByNewerBuild);
    CHECK(project.metadata().name == "From the future");
}

TEST_CASE("A malformed field costs the field, not the project", "[serialization][errors]") {
    Project project;

    auto result = serializer().read(R"({
      "tnp": {"format": "project", "version": "1.0"},
      "metadata": {"name": "Partly broken"},
      "network": {
        "devices": [
          {
            "id": "not-a-uuid",
            "type": "Router",
            "name": "R1",
            "interfaces": [
              {"id": "11111111-1111-4111-8111-111111111111", "name": "Gi0/0",
               "type": "GigabitEthernet", "ipv4": ["999.1.1.1/24", "10.0.0.1/24"]}
            ]
          },
          {"type": "Nonsense", "name": "Ghost"}
        ],
        "links": []
      }
    })",
                                    project);

    REQUIRE(result.isOk());
    CHECK_FALSE(result.value().warnings.empty());

    // The router loaded with a generated identifier and its one valid address.
    REQUIRE(project.network().deviceCount() == 1);
    const Device* router = project.network().findDeviceByName("R1");
    REQUIRE(router != nullptr);
    CHECK(router->id().isValid());

    const Interface* port = router->findInterfaceByName("Gi0/0");
    REQUIRE(port != nullptr);
    CHECK(port->ipv4Addresses().size() == 1);
    CHECK(port->ipv4Addresses().front().address().toString() == "10.0.0.1");
}

TEST_CASE("Fields of the wrong type fall back to defaults", "[serialization][errors]") {
    Project project;

    auto result = serializer().read(R"({
      "tnp": {"format": "project", "version": "1.0"},
      "metadata": {"name": 42, "tags": "not-a-list"},
      "simulation": {"speedMultiplier": "fast", "traceHistoryLimit": true},
      "network": {"devices": "not-a-list", "links": []},
      "layout": {"view": {"zoom": -5}}
    })",
                                    project);

    REQUIRE(result.isOk());
    CHECK_FALSE(result.value().warnings.empty());
    CHECK(project.metadata().name == "Untitled Project");
    CHECK(project.simulationSettings().speedMultiplier == 1.0);
    CHECK(project.layout().viewZoom == 1.0f); // a negative zoom is unusable
}

TEST_CASE("Links pointing at missing interfaces are dropped with a warning",
          "[serialization][errors]") {
    Project project;

    auto result = serializer().read(R"({
      "tnp": {"format": "project", "version": "1.0"},
      "metadata": {"name": "Dangling"},
      "network": {
        "devices": [],
        "links": [
          {"id": "22222222-2222-4222-8222-222222222222",
           "a": {"interface": "33333333-3333-4333-8333-333333333333"},
           "b": {"interface": "44444444-4444-4444-8444-444444444444"}}
        ]
      }
    })",
                                    project);

    REQUIRE(result.isOk());
    CHECK(project.network().linkCount() == 0);
    CHECK_FALSE(result.value().warnings.empty());
}

TEST_CASE("Layout and test entries for missing devices are pruned", "[serialization][errors]") {
    Project project;

    auto result = serializer().read(R"({
      "tnp": {"format": "project", "version": "1.0"},
      "metadata": {"name": "Orphans"},
      "network": {"devices": [], "links": []},
      "layout": {"devices": [{"device": "55555555-5555-4555-8555-555555555555", "x": 10, "y": 20}]},
      "tests": [{"id": "66666666-6666-4666-8666-666666666666", "name": "ghost",
                 "source": "55555555-5555-4555-8555-555555555555"}]
    })",
                                    project);

    REQUIRE(result.isOk());
    CHECK(project.layout().placements().empty());
    CHECK(project.tests().empty());
}

TEST_CASE("A failed load leaves the project untouched", "[serialization][errors]") {
    Project project;
    project.metadata().name = "Original";

    REQUIRE_FALSE(serializer().read("{ broken", project).isOk());
    CHECK(project.metadata().name == "Original");

    REQUIRE_FALSE(serializer().read(R"({"tnp":{"format":"project","version":"9.0"}})", project).isOk());
    CHECK(project.metadata().name == "Original");
}

TEST_CASE("A minimal document loads", "[serialization]") {
    Project project;
    auto result = serializer().read(minimalDocument(), project);

    REQUIRE(result.isOk());
    CHECK(project.metadata().name == "Test");
    CHECK(project.network().deviceCount() == 0);
    CHECK(result.value().fileVersion == kCurrentProjectVersion);
}
