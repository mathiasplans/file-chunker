#include <cstring>
#include <bit>
#include "filec.hpp"

// This code only works on little endian machines currently
static_assert(std::endian::native == std::endian::little);

namespace filec {

static const uint8_t g_byte_bits = 8;
static const uint8_t g_uintmax_bits = sizeof(uintmax_t) * g_byte_bits;
static const uintmax_t g_min_bit = uintmax_t(1);
static const uintmax_t g_max_bit = g_min_bit << (g_uintmax_bits - 1);
static const uintmax_t g_max = ~uintmax_t(0);

static inline size_t upper_index(size_t index) {
  return index / g_uintmax_bits;
}

static inline size_t lower_index(size_t index) {
  return index % g_uintmax_bits;
}

void pagemap::update_current_ui() {
  this->update_current_ui(this->current_ui);
}

void pagemap::update_current_ui(size_t start_index) {
  for (this->current_ui = start_index; this->current_ui < this->data_size; ++this->current_ui) {
    if (this->bits[this->current_ui] != g_max)
      break;
  }
}

pagemap::pagemap(size_t size, size_t chunk_size) : size(size), chunk_size(chunk_size), current_ui(0) {
  // How many chunks are required for given size and chunk size
  this->chunk_count = this->size / this->chunk_size;

  // How many bytes are in the last chunk if all the size does not fit into chunks
  this->last_chunk_size = this->size - this->chunk_count * this->chunk_size;

  // If there are spare bytes, add one spare chunk
  if (this->last_chunk_size != 0)
    this->chunk_count += 1;

  // If all the bytes fit into chunks, the last chunk is full
  else
    this->last_chunk_size = this->chunk_size;

  // How many words do we need to hold the pagemap
  this->data_size = this->chunk_count / g_uintmax_bits;

  // How many chunks are in the last word if there are some spare
  this->last_data_size = this->chunk_count - this->data_size * g_uintmax_bits;

  // If there are spare chunks, add one more word
  if (this->last_data_size != 0)
    this->data_size += 1;

  else
    this->last_data_size = g_uintmax_bits;

  // Allocate the memory for the pagemap. Bits here are mapped to each chunk.
  this->bits = new uintmax_t[this->data_size]();

  // Create the mask for the last word
  this->last_mask = g_max >> (g_uintmax_bits - this->last_data_size);

  // How many bytes is the pagemap
  this->byte_size = this->chunk_count / g_byte_bits + ((this->chunk_count % g_byte_bits) != 0);
}

pagemap::~pagemap() {
  delete this->bits;
}

void pagemap::set(size_t index) {
  this->bits[upper_index(index)] |= g_min_bit << lower_index(index);
}

void pagemap::set(const uint8_t *data) {
  memcpy(this->bits, data, this->byte_size);

  // Bring the ui to correct position
  this->update_current_ui(0);
}

bool pagemap::operator[](size_t index) {
  return (this->bits[upper_index(index)] >> lower_index(index)) & 1;
}

size_t li_search(uintmax_t cell) {
  uintmax_t wm = cell;
  size_t msize = g_uintmax_bits / 2;
  uintmax_t mask = g_max >> msize;
  uintmax_t conj = 0;
  size_t index = 0;

  while (wm && msize) {
    conj = wm & mask;

    // Saturated
    if (conj == mask) {
      index += msize;
      wm >>= msize;
    }

    msize >>= 1;
    mask >>= msize;
  }

  return index;
}

bool pagemap::complete() {
  this->update_current_ui();
  return this->current_ui + 1 == this->data_size && this->bits[this->current_ui] == this->last_mask;
}

size_t pagemap::first_missing() {
  // Iterate until find non-saturated
  this->update_current_ui();

  // Use binary search to get the lower index
  size_t li = li_search(this->bits[this->current_ui]);

  return g_uintmax_bits * this->current_ui + li;
}

size_t pagemap::get_chunk_count() {
  return this->chunk_count;
}

size_t pagemap::get_last_chunk_size() {
  return this->last_chunk_size;
}

uint8_t *pagemap::data() {
  return reinterpret_cast<uint8_t *>(this->bits);
}

size_t pagemap::length() {
  return this->byte_size;
}

chunker::chunker(std::fstream &file, size_t start, size_t size, size_t chunk_size)
  : fstream(file), start(start), size(size), chunk_size(chunk_size), current_read_index(0) {
  this->fstream.seekp(start);
  this->fstream.seekg(start);

  this->pm = new pagemap(size, chunk_size);
}

chunker::~chunker() {
  delete this->pm;
}

chunker &chunker::operator<<(const chunk &b) {
  this->fstream.seekp(this->start + b.chunk_id * this->chunk_size);

  size_t write_size = this->chunk_size;

  // If it is the last chunk, the write size might be different
  if (b.chunk_id == this->pm->get_chunk_count() - 1)
    write_size = this->pm->get_last_chunk_size();

  // Write to file
  this->fstream.write(reinterpret_cast<const char *>(b.data), write_size);

  this->pm->set(b.chunk_id);

  return *this;
}

chunker &chunker::operator>>(chunk &b) {
  this->fstream.seekg(this->start + this->current_read_index * this->chunk_size);

  size_t read_size = this->chunk_size;

  // If it is the last chunk, the read size migth be different
  if (this->current_read_index == this->pm->get_chunk_count() - 1)
    read_size = this->pm->get_last_chunk_size();

  // Read from the file
  this->fstream.read(reinterpret_cast<char *>(b.data), read_size);

  // Set the chunk index
  b.chunk_id = this->current_read_index;

  // Mark the completion of the chunk
  this->pm->set(this->current_read_index);
  this->current_read_index = this->pm->first_missing();

  return *this;
}

bool chunker::complete() {
  return this->pm->complete();
}

uint8_t *chunker::get_pagemap() {
  return this->pm->data();
}

size_t chunker::get_pagemap_length() {
  return this->pm->length();
}

void chunker::set_pagemap(const uint8_t *pm) {
  this->pm->set(pm);
  this->current_read_index = this->pm->first_missing();
}

}; // namespace filechunker
