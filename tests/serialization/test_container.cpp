#include "app/ProjectFile.h"
#include "app/SampleProject.h"
#include "serialization/TnpContainer.h"
#include "serialization/TnpCrypto.h"
#include "utilities/FileSystem.h"
#include "utilities/Uuid.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace tnp;

namespace {

/// A temporary directory that removes itself, so tests never leave files behind.
class ScratchDirectory {
public:
    ScratchDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                ("tnp-test-" + Uuid::generate().toString().substr(0, 8));
        std::filesystem::create_directories(path_);
    }
    ~ScratchDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;

    [[nodiscard]] std::filesystem::path file(const std::string& name) const { return path_ / name; }

private:
    std::filesystem::path path_;
};

ByteBuffer bytesOf(std::string_view text) { return ByteBuffer{text.begin(), text.end()}; }

} // namespace

TEST_CASE("Containers round-trip their entries", "[serialization][container]") {
    serial::TnpContainer container;
    container.addTextEntry("manifest.json", R"({"format":"tnp-container"})");
    container.addTextEntry("project.tnpjson", R"({"tnp":{"format":"project"}})");
    container.addEntry("blob.bin", ByteBuffer{1, 2, 3, 4, 5});

    const ByteBuffer encoded = container.serialize();
    CHECK(serial::TnpContainer::looksLikeContainer(encoded));

    auto parsed = serial::TnpContainer::parse(encoded);
    REQUIRE(parsed.isOk());

    CHECK(parsed.value().entries().size() == 3);
    CHECK(parsed.value().findText("manifest.json") == R"({"format":"tnp-container"})");
    CHECK(*parsed.value().find("blob.bin") == ByteBuffer{1, 2, 3, 4, 5});
    CHECK(parsed.value().find("missing") == nullptr);
    CHECK_FALSE(parsed.value().findText("missing").has_value());
}

TEST_CASE("An empty container is valid", "[serialization][container]") {
    const serial::TnpContainer container;
    auto parsed = serial::TnpContainer::parse(container.serialize());

    REQUIRE(parsed.isOk());
    CHECK(parsed.value().empty());
}

TEST_CASE("Adding an entry twice replaces it", "[serialization][container]") {
    serial::TnpContainer container;
    container.addTextEntry("a.txt", "first");
    container.addTextEntry("a.txt", "second");

    CHECK(container.entries().size() == 1);
    CHECK(container.findText("a.txt") == "second");
}

TEST_CASE("A corrupt container is refused, not partially loaded", "[serialization][container]") {
    serial::TnpContainer container;
    container.addTextEntry("project.tnpjson", "the quick brown fox jumps over the lazy dog");
    ByteBuffer encoded = container.serialize();

    SECTION("a wrong magic number") {
        encoded[0] = 'X';
        CHECK_FALSE(serial::TnpContainer::parse(encoded).isOk());
        CHECK_FALSE(serial::TnpContainer::looksLikeContainer(encoded));
    }
    SECTION("a flipped bit inside an entry fails its checksum") {
        encoded.back() ^= 0xFF;
        auto parsed = serial::TnpContainer::parse(encoded);
        REQUIRE_FALSE(parsed.isOk());
        CHECK(parsed.message().find("corrupt") != std::string::npos);
    }
    SECTION("a truncated file") {
        encoded.resize(encoded.size() / 2);
        CHECK_FALSE(serial::TnpContainer::parse(encoded).isOk());
    }
    SECTION("a version from the future") {
        encoded[5] = 99;
        auto parsed = serial::TnpContainer::parse(encoded);
        REQUIRE_FALSE(parsed.isOk());
        CHECK(parsed.message().find("newer build") != std::string::npos);
    }
    SECTION("an implausible entry count") {
        encoded[8] = 0xFF;
        encoded[9] = 0xFF;
        CHECK_FALSE(serial::TnpContainer::parse(encoded).isOk());
    }
    SECTION("empty input") {
        CHECK_FALSE(serial::TnpContainer::parse(ByteBuffer{}).isOk());
        CHECK_FALSE(serial::TnpContainer::parse(bytesOf("TNP")).isOk());
    }
}

TEST_CASE("Projects save and load through both file formats", "[serialization][files]") {
    const ScratchDirectory scratch;

    core::Project original;
    app::buildSampleProject(original);

    SECTION("the .tnp container") {
        const auto path = scratch.file("project.tnp");
        REQUIRE(app::saveProject(path, original).isOk());
        CHECK(app::formatForPath(path) == app::ProjectFormat::Container);

        core::Project loaded;
        auto report = app::loadProject(path, loaded);
        REQUIRE(report.isOk());
        CHECK(loaded.network().deviceCount() == original.network().deviceCount());
        CHECK(loaded.metadata().name == original.metadata().name);

        // The container also carries a readable manifest.
        const auto bytes = files::readBinaryFile(path);
        REQUIRE(bytes.isOk());
        auto container = serial::TnpContainer::parse(bytes.value());
        REQUIRE(container.isOk());
        CHECK(container.value().findText(serial::TnpContainer::kManifestEntry).has_value());
    }

    SECTION("the .tnpjson document") {
        const auto path = scratch.file("project.tnpjson");
        REQUIRE(app::saveProject(path, original).isOk());
        CHECK(app::formatForPath(path) == app::ProjectFormat::Json);

        // It really is readable JSON.
        const auto text = files::readTextFile(path);
        REQUIRE(text.isOk());
        CHECK(text.value().find("\"tnp\"") != std::string::npos);
        CHECK(text.value().find('\n') != std::string::npos);

        core::Project loaded;
        REQUIRE(app::loadProject(path, loaded).isOk());
        CHECK(loaded.network().deviceCount() == original.network().deviceCount());
    }

    SECTION("content wins over the extension") {
        const auto path = scratch.file("renamed.tnpjson");
        // Write a container under a .tnpjson name; loading must still work.
        core::Project source;
        app::buildSampleProject(source);
        REQUIRE(app::saveProject(scratch.file("real.tnp"), source).isOk());

        const auto bytes = files::readBinaryFile(scratch.file("real.tnp"));
        REQUIRE(bytes.isOk());
        REQUIRE(files::writeBinaryFileAtomic(path, bytes.value()).isOk());

        core::Project loaded;
        REQUIRE(app::loadProject(path, loaded).isOk());
        CHECK(loaded.network().deviceCount() == source.network().deviceCount());
    }
}

TEST_CASE("Saving is atomic", "[serialization][files]") {
    const ScratchDirectory scratch;
    const auto path = scratch.file("project.tnp");

    core::Project original;
    app::buildSampleProject(original);
    REQUIRE(app::saveProject(path, original).isOk());

    const auto firstSize = std::filesystem::file_size(path);

    // Saving again must replace the file, not append to it or leave a temporary.
    REQUIRE(app::saveProject(path, original).isOk());
    CHECK(std::filesystem::file_size(path) == firstSize);
    CHECK_FALSE(std::filesystem::exists(path.string() + ".tnptmp"));
}

TEST_CASE("Encryption reports that it is not implemented", "[serialization][crypto]") {
    // The point of this test is that TNP never pretends: a caller must be able to
    // find out that .tnpenc does nothing, rather than receive fake ciphertext.
    CHECK_FALSE(serial::isEncryptionAvailable());
    CHECK_FALSE(serial::encryptionUnavailableReason().empty());

    auto encrypted = serial::encryptContainer(ByteBuffer{1, 2, 3}, "hunter2");
    CHECK_FALSE(encrypted.isOk());
    CHECK(encrypted.message().find("not implemented") != std::string::npos);

    CHECK_FALSE(serial::decryptContainer(ByteBuffer{1, 2, 3}, "hunter2").isOk());

    const ScratchDirectory scratch;
    core::Project project;
    CHECK_FALSE(app::saveProject(scratch.file("secret.tnpenc"), project).isOk());
}
