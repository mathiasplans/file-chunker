#include <fstream>

namespace filec {

/**
 * Data type for data chunks in a file.
 * This will be output by the chunker and has
 * to be the input for the chunker.
 *
 * If the chunk is available in bare memory (as 16 bits
 * of chunk ID and x amount of bytes as data), then that
 * memory region can be viewed as a chunk by using chunk::at
 * function. This is for handling data that is coming from
 * external sources, such as SPI or I2C data packets.
 */
struct chunk {
  uint16_t chunk_id;
  uint8_t data[1];

  /**
   * View a memory region as a chunk.
   * Note that the chunk becomes invalid
   * when the memory region is deallocated.
   * @param place pointer to the memory region with the chunk data
   */
  static chunk &at(void *place) {
    return *new(place) chunk();
  };
};

/**
 * Bitmap for keeping track of already sent/received
 * chunks by the the chunker
 */
class pagemap {
private:
  size_t size;
  size_t data_size;
  size_t byte_size;
  size_t chunk_size;
  size_t chunk_count;
  size_t last_chunk_size;
  size_t last_data_size;
  size_t current_ui;
  uintmax_t last_mask;
  uintmax_t *bits;

  // Update the upper index by starting
  // from the current location
  void update_current_ui();

  // Update the upper index by starting
  // from the specified location
  void update_current_ui(size_t start_index);

  // Default constructor is forbidden
  pagemap() = delete;

public:
  /**
   * Constructs the pagemap. Two arguments
   * are required:
   * @param size - the size of the transfer in bytes
   * @param chunk_size - the size of a chunk in bytes
   */
  pagemap(size_t size, size_t chunk_size);

  ~pagemap();

  /**
   * Mark a chunk as processed
   * by its ID
   * @param index chunk ID
   */
  void set(size_t index);

  /**
   * Update the full pagemap. It
   * is important that the memory
   * pointed to by the *data has
   * the same length as this pagemap.
   *
   * To get the length of this pagemap,
   * use pagemap::length method.
   *
   * @param data pointer to raw pagemap
   */
  void set(const uint8_t *data);

  /**
   * Get the status of the chunk
   * by its index. If true is returned,
   * the chunk as already been sent/received.
   * Otherwise, the chunk is still in
   * processing queue.
   * @param index chunk ID
   */
  bool operator[](size_t index);

  /**
   * Return true if all the chunks have been
   * processed.
   */
  bool complete();

  /**
   * Get the first chunk that has not been
   * processed yet.
   */
  size_t first_missing();

  /**
   * Get how many chunks are in the pagemap
   */
  size_t get_chunk_count();

  /**
   * Get the size of the last chunk in bytes.
   * When chunk size is large, it might happen
   * that the last chunk is smaller than
   * the chunk size. In that case, this number
   * indicates how many bytes have to be transferred
   * instead.
   */
  size_t get_last_chunk_size();

  /**
   * Get the pointer to the raw pagemap as a byte array.
   * This array has the length that is returned by
   * the method pagemap::length()
   */
  uint8_t *data();

  /**
   * Get the length of the
   * pagemap. This is the amount of
   * bytes required to store all of
   * the information of the pagemap.
   */
  size_t length();
};

/**
 * Chunking and merging object
 */
class chunker {
private:
  std::fstream &fstream;
  size_t start;
  size_t size;
  size_t chunk_size;
  pagemap *pm;
  size_t current_read_index;
  size_t read_size;

  // default constructor is forbidden
  chunker() = delete;

public:
  /**
   * Constructs a chunker from a file stream, start index, size of the read/write,
   * and the size of the chunk.
   * @param file file stream that has been opened with binary mode and in or out
   * @param start start byte index of the read/write
   * @param size size of the read/write in bytes
   * @param chunk_size size of the transfer chunks in bytes
   */
  chunker(std::fstream &file, size_t start, size_t size, size_t chunk_size);

  ~chunker();

  /**
   * Chunker input.
   * @param b input chunk to be stored in file
   */
  chunker& operator<<(const chunk &b);

  /**
   * Chunker output
   * @param b output chunk that was read from the file stream
   */
  chunker& operator>>(chunk &b);

  /**
   * Get the size of the last chunk
   * that was acquited with >>
   */
  size_t get_chunk_size();

  /**
   * Return if all chunks are processed
   */
  bool complete();

  /**
   * Get the raw byte array of the pagemap
   */
  uint8_t* get_pagemap();

  /**
   * Get the length of the pagemap in bytes
   */
  size_t get_pagemap_length();

  /**
   * Set the pagemap from raw data
   * @param pm pagemap as a byte array
   */
  void set_pagemap(const uint8_t *pm);
};

}; // namespace filechunker
