#include "CompressedStepReader.hpp"

#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>

#ifdef OOPENCAL_VIEWER_WITH_BLOSC2
#include <blosc2.h>

namespace
{
/** Initialises C-Blosc2 once per process (thread-safe) and releases it at exit. */
struct Blosc2Runtime
{
    Blosc2Runtime() { blosc2_init(); }
    ~Blosc2Runtime() { blosc2_destroy(); }
    Blosc2Runtime(const Blosc2Runtime&) = delete;
    Blosc2Runtime& operator=(const Blosc2Runtime&) = delete;
};

void ensureBlosc2Initialised()
{
    static const Blosc2Runtime runtime;
}

struct Blosc2ContextDeleter
{
    void operator()(blosc2_context* context) const { blosc2_free_ctx(context); }
};
} // namespace

std::vector<char> ReaderHelpers::readCompressedStep(std::istream& in, std::size_t expectedBytes)
{
    ensureBlosc2Initialised();

    CompressedStepHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof header);
    if (in.gcount() != static_cast<std::streamsize>(sizeof header))
        throw std::runtime_error("Compressed step: cannot read the step header (truncated file?)");

    if (header.originalSize != expectedBytes)
    {
        throw std::runtime_error(std::format(
            "Compressed step holds {} bytes, but the model's cell type needs {} bytes "
            "(is the cell class of the loaded plugin the same as the one used by the simulation?)",
            header.originalSize, expectedBytes));
    }

    constexpr std::uint64_t maxChunk = static_cast<std::uint64_t>(BLOSC2_MAX_BUFFERSIZE);
    if (header.originalSize > maxChunk || header.compressedSize > maxChunk + BLOSC2_MAX_OVERHEAD)
        throw std::runtime_error("Compressed step: header contains an implausible size (corrupted file?)");

    std::vector<char> compressed(header.compressedSize);
    in.read(compressed.data(), static_cast<std::streamsize>(compressed.size()));
    if (in.gcount() != static_cast<std::streamsize>(compressed.size()))
    {
        throw std::runtime_error(std::format("Compressed step: expected {} compressed bytes, got {} (truncated file?)",
                                             compressed.size(), in.gcount()));
    }

    std::vector<char> decompressed(expectedBytes);

    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dparams.nthreads = 1; // the Viewer already decodes one node per thread
    std::unique_ptr<blosc2_context, Blosc2ContextDeleter> context(blosc2_create_dctx(dparams));
    if (! context)
        throw std::runtime_error("Compressed step: blosc2_create_dctx failed");

    const int decompressedBytes = blosc2_decompress_ctx(context.get(),
                                                        compressed.data(),
                                                        static_cast<int32_t>(compressed.size()),
                                                        decompressed.data(),
                                                        static_cast<int32_t>(decompressed.size()));
    if (decompressedBytes < 0 || static_cast<std::uint64_t>(decompressedBytes) != expectedBytes)
    {
        throw std::runtime_error(std::format("Compressed step: Blosc2 decompression failed (code {}, expected {} bytes)",
                                             decompressedBytes, expectedBytes));
    }

    return decompressed;
}

#else // !OOPENCAL_VIEWER_WITH_BLOSC2

std::vector<char> ReaderHelpers::readCompressedStep(std::istream&, std::size_t)
{
    throw std::runtime_error("This Viewer was built without C-Blosc2 support, so it cannot read mode=compressed "
                             "data. Rebuild it with -DVIEWER_WITH_BLOSC2=ON.");
}

#endif // OOPENCAL_VIEWER_WITH_BLOSC2
