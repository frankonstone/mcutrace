#include <mcucov/mcucov.hpp>
#include <mcucov/sinks/buffered.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

struct FileSink final {
    std::FILE* file = nullptr;

    bool MCUCOV_NO_INSTRUMENT write(const std::uint8_t* bytes, std::size_t size) noexcept {
        return file != nullptr && std::fwrite(bytes, 1, size, file) == size;
    }
};

struct CoverageDump final {
    ~CoverageDump() {
        const char* path = std::getenv("MCUTRACE_MCUCOV_CAPTURE");
        if (path == nullptr || *path == '\0') return;

        std::FILE* file = std::fopen(path, "wb");
        if (file == nullptr) return;

        FileSink file_sink{.file = file};
        mcucov::BufferedSink<FileSink> sink(file_sink);
        (void)mcucov::dump(sink);
        (void)std::fclose(file);
    }
};

CoverageDump coverage_dump;

}  // namespace
