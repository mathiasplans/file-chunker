#include <cstring>
#include <bit>
#include "filec.hpp"

namespace filec {

template <std::unsigned_integral T>
void pagemap<T>::update_current_ui() {
  this->update_current_ui(this->current_ui);
}

template <std::unsigned_integral T>
void pagemap<T>::update_current_ui(size_t start_index) {
  for (this->current_ui = start_index; this->current_ui < this->data_size; ++this->current_ui) {
    if (this->bits[this->current_ui] != this->max)
      break;
  }
}

template <std::unsigned_integral T>
size_t pagemap<T>::upper_index(size_t index) {
  return index / this->T_bits;
}

template <std::unsigned_integral T>
size_t pagemap<T>::lower_index(size_t index) {
  return index % this->T_bits;
}

template <std::unsigned_integral T>
size_t pagemap<T>::li_search(T cell) {
  uintmax_t wm = cell;
  size_t msize = this->T_bits / 2;
  uintmax_t mask = this->max >> msize;
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

template <std::unsigned_integral T>
pagemap<T>::pagemap(size_t size, size_t chunk_size) : size(size), chunk_size(chunk_size), current_ui(0) {
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
  this->data_size = this->chunk_count / this->T_bits;

  // How many chunks are in the last word if there are some spare
  this->last_data_size = this->chunk_count - this->data_size * this->T_bits;

  // If there are spare chunks, add one more word
  if (this->last_data_size != 0)
    this->data_size += 1;

  else
    this->last_data_size = this->T_bits;

  // Allocate the memory for the pagemap. Bits here are mapped to each chunk.
  this->bits = new T[this->data_size]();

  // Create the mask for the last word
  this->last_mask = this->max >> (this->T_bits - this->last_data_size);

  // How many bytes is the pagemap
  this->byte_size = this->chunk_count / 8 + ((this->chunk_count % 8) != 0);
}

template <std::unsigned_integral T>
pagemap<T>::~pagemap() {
  delete this->bits;
}

template <std::unsigned_integral T>
void pagemap<T>::set(size_t index) {
  this->bits[this->upper_index(index)] |= this->min_bit << this->lower_index(index);
}

template <std::unsigned_integral T>
void pagemap<T>::set(const uint8_t *data) {
  memcpy(this->bits, data, this->byte_size);

  // Bring the ui to correct position
  this->update_current_ui(0);
}

template <std::unsigned_integral T>
bool pagemap<T>::operator[](size_t index) {
  return (this->bits[this->upper_index(index)] >> this->lower_index(index)) & 1;
}

template <std::unsigned_integral T>
bool pagemap<T>::complete() {
  this->update_current_ui();
  return this->current_ui + 1 == this->data_size && this->bits[this->current_ui] == this->last_mask;
}

template <std::unsigned_integral T>
size_t pagemap<T>::first_missing() {
  // Iterate until find non-saturated
  this->update_current_ui();

  // Use binary search to get the lower index
  size_t li = li_search(this->bits[this->current_ui]);

  return this->T_bits * this->current_ui + li;
}

template <std::unsigned_integral T>
size_t pagemap<T>::get_chunk_count() {
  return this->chunk_count;
}

template <std::unsigned_integral T>
size_t pagemap<T>::get_last_chunk_size() {
  return this->last_chunk_size;
}

template <std::unsigned_integral T>
uint8_t *pagemap<T>::data() {
  return reinterpret_cast<uint8_t *>(this->bits);
}

template <std::unsigned_integral T>
size_t pagemap<T>::length() {
  return this->byte_size;
}

template class pagemap<uint8_t>;
template class pagemap<uint16_t>;
template class pagemap<uint32_t>;

template <std::unsigned_integral T>
chunker<T>::chunker(std::fstream &file, size_t start, size_t size, size_t chunk_size)
  : fstream(file), start(start), size(size), chunk_size(chunk_size), current_read_index(0) {
  this->fstream.seekp(start);
  this->fstream.seekg(start);

  this->pm = new pagemap<T>(size, chunk_size);
}

template <std::unsigned_integral T>
chunker<T>::~chunker() {
  delete this->pm;
}

template <std::unsigned_integral T>
chunker<T> &chunker<T>::operator<<(const chunk &b) {
  this->fstream.clear();
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

template <std::unsigned_integral T>
chunker<T> &chunker<T>::operator>>(chunk &b) {
  this->fstream.clear();
  this->fstream.seekg(this->start + this->current_read_index * this->chunk_size);

  this->read_size = this->chunk_size;

  // If it is the last chunk, the read size migth be different
  if (this->current_read_index == this->pm->get_chunk_count() - 1)
    this->read_size = this->pm->get_last_chunk_size();

  // Read from the file
  this->fstream.read(reinterpret_cast<char *>(b.data), this->read_size);

  // Set the chunk index
  b.chunk_id = this->current_read_index;

  // Mark the completion of the chunk
  this->pm->set(this->current_read_index);
  this->current_read_index = this->pm->first_missing();

  return *this;
}

template <std::unsigned_integral T>
bool chunker<T>::complete() {
  return this->pm->complete();
}

template <std::unsigned_integral T>
size_t chunker<T>::get_chunk_size() {
  return this->read_size + sizeof(uint16_t);
}

template <std::unsigned_integral T>
uint8_t *chunker<T>::get_pagemap() {
  return this->pm->data();
}

template <std::unsigned_integral T>
size_t chunker<T>::get_pagemap_length() {
  return this->pm->length();
}

template <std::unsigned_integral T>
void chunker<T>::set_pagemap(const uint8_t *pm) {
  this->pm->set(pm);
  this->current_read_index = this->pm->first_missing();
}

template class chunker<uint8_t>;
template class chunker<uint16_t>;
template class chunker<uint32_t>;

}; // namespace filechunker
