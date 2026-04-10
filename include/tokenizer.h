#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "structures.h"

/* Index all documents contained in the input file. */
void tokenize_document(const char* filename);

/* Index supported document files under a root directory (recursively on Windows). */
void tokenize_directory(const char* root_dir);

/* Tokenize arbitrary text into normalized terms (lowercase, punctuation removed). */
int tokenize_text(const char* text, char tokens[][WORD_MAX_LEN], int max_tokens);

/* Placeholder for future stemming/lemmatization. Current behavior: lowercasing + punctuation removal only. */
char* normalize_token(char* token);

/*
Dynamic tokenization helper used internally to remove hard limits.
Returns 1 on success, 0 on allocation failure.
Caller owns returned `tokens` and must free via tokenize_free_tokens().
*/
int tokenize_text_dynamic(const char* text, char*** out_tokens, int* out_count);
void tokenize_free_tokens(char** tokens, int count);

/* Document metadata populated by tokenize_document(). */
int get_document_count(void);
const char* get_document_label(int docID);
const char* get_document_text(int docID);
int get_docID_by_label(const char* label);
void print_metadata_tree(void);

/* Total number of tokens processed during indexing. */
long long get_total_tokens_indexed(void);

#endif

