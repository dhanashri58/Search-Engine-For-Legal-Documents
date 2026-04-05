#ifndef INDEX_H
#define INDEX_H

#include "structures.h"

void index_init(void);
void index_free(void);

void insert_word(char* word, int docID);
/* Future-ready API: position >= 0 enables positional postings (unused for now). */
void insert_term(const char* word, int docID, int position);
Posting* get_postings(char* word);
int get_document_frequency(const char* word);
int get_vocabulary_size(void);
void debug_print_index();

#endif

