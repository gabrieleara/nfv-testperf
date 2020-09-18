#ifndef SRBM_HPP
#define SRBM_HPP

#include <cstdint>
#include <vector>

using byte_t = uint8_t;
using buffer_t = byte_t *;

/**
 * Simple ring buffer manager.
 *
 * Users of this class can get (even using multiple requests) a set of buffers
 * to be used freely until release.
 *
 * Notice that releases are handled in an orderly fashion: the first buffer
 * requested will be the first buffer to be released when the appropriate method
 * is called.
 * */
class srbm
{
private:
    using buffer_type = std::vector<byte_t>;
    using ring_type = std::vector<std::vector<byte_t>>;

public:
    using bsize_type = buffer_type::size_type;
    using rsize_type = ring_type::size_type;

    /**
     *  The size of the ring.
     * */
    const rsize_type rsize;

    /**
     * Constructs a new ring buffer manager composed by rsize buffers
     * of size bsize.
     *
     * Buffers are all the same size and no different buffer size can be
     * requested dynamically.
     *
     * @param rsize     How many buffers there will be.
     * @param bsize     The size that each buffer will have.
     * */
    srbm(rsize_type rsize, bsize_type bsize)
        : rsize(rsize),
          free_buffers(rsize),
          ring(rsize, buffer_type(bsize, 0)){};

    // TODO: copy, move and all constructors

    /**
     * Destructor for the ring class.
     * Default destructor is fine
     * */
    ~srbm() = default;

    /**
     * Request a set of at most howmany buffers from the
     *
     * @param buffers   The array to be filled with buffer pointers.
     *                  Must be at least howmany elements long.
     * @param howmany   The number of buffers to be requested.
     *
     * @returns         The number of released buffers (may be less than howmany
     *                  if there are not enough free buffers in the ring).
     * */
    inline rsize_type request_buffers(buffer_t buffers[], rsize_type howmany)
    {
        if (howmany > free_buffers)
            howmany = free_buffers;

        if (howmany = 0)
            return 0;

        auto first_buffer = head;
        auto howmany_first_round = howmany;

        free_buffers -= howmany;
        head += howmany;
        if (head >= rsize)
        {
            head -= rsize;
            howmany_first_round = rsize - first_buffer;
        }

        decltype(in_idx) out_idx = 0;
        for (auto in_idx = first_buffer; out_idx < howmany_first_round; ++in_idx, ++out_idx)
        {
            buffers[out_idx] = ring[in_idx].data();
        }

        for (auto in_idx = 0; out_idx < howmany; ++in_idx, ++out_idx)
        {
            buffers[out_idx] = ring[in_idx].data();
        }

        return howmany;
    }

    /**
     * Release at most howmany buffers taken from the ring.
     * This works in-order, which means that release order
     * follows acquisition order.
     *
     * @param howmany the number of buffers to be released.
     *
     * @returns the number of released buffers.
     * */
    inline rsize_type release_buffers(rsize_type howmany)
    {
        const auto busy_buffers = rsize - free_buffers;
        if (howmany > busy_buffers)
            howmany = busy_buffers;

        free_buffers += howmany;
        return howmany;
    }

private:
    ring_type ring;
    rsize_type head = 0;
    rsize_type free_buffers;
};

#endif // SRBM_HPP
