#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tokenizer.h"
#include "index.h"
#include "trie.h"
#include "file_loader.h"
#include "avl_tree.h"

#define MAX_LINE 8192

static int g_doc_count = 0;
static int g_doc_cap = 0;
static char** g_doc_labels = NULL; /* 1-indexed */
static char** g_doc_texts = NULL;  /* 1-indexed */
static long long g_total_tokens_indexed = 0;
static AVLNode* g_avl_root = NULL; /* Metadata Tree (AVL) */

static int ensure_doc_capacity(int needed_docID) {
    if (needed_docID <= g_doc_cap) return 1;
    int new_cap = (g_doc_cap > 0) ? g_doc_cap : 128;
    while (new_cap < needed_docID) {
        if (new_cap > 1 << 28) return 0;
        new_cap *= 2;
    }

    char** new_labels = (char**)realloc(g_doc_labels, (size_t)(new_cap + 1) * sizeof(char*));
    if (!new_labels) return 0;
    char** new_texts = (char**)realloc(g_doc_texts, (size_t)(new_cap + 1) * sizeof(char*));
    if (!new_texts) {
        /* Best-effort: keep existing arrays unchanged on failure. */
        /* Note: realloc already updated new_labels pointer; we must not lose it. */
        /* This is still safe because g_doc_labels already points to new_labels. */
        g_doc_labels = new_labels;
        return 0;
    }

    for (int i = g_doc_cap + 1; i <= new_cap; i++) {
        new_labels[i] = NULL;
        new_texts[i] = NULL;
    }

    g_doc_labels = new_labels;
    g_doc_texts = new_texts;
    g_doc_cap = new_cap;
    return 1;
}

static int is_stopword(const char* w) {
    /* Optional stopword filtering (small, safe list for legal text). */
    static const char* stop[] = {
        "the","a","an","and","of","to","in","for","on","with","by","at","from","as","is","are","was","were",
        "be","been","being","that","this","these","those","it","its"
    };
    int n = (int)(sizeof(stop) / sizeof(stop[0]));
    for (int i = 0; i < n; i++) {
        if (strcmp(w, stop[i]) == 0) return 1;
    }
    return 0;
}

char* normalize_token(char* token) {
    if (!token) return NULL;
    /*
    Current normalization:
    - lowercase
    - remove punctuation (keep [A-Za-z0-9_])
    Future: stemming/lemmatization can be plugged here.
    */
    int w = 0;
    for (int r = 0; token[r] != '\0'; r++) {
        unsigned char ch = (unsigned char)token[r];
        if (isalnum(ch) || ch == '_') {
            token[w++] = (char)tolower(ch);
        }
    }
    token[w] = '\0';
    return token;
}

int tokenize_text(const char* text, char tokens[][WORD_MAX_LEN], int max_tokens) {
    if (!text || !tokens || max_tokens <= 0) return 0;
    int out = 0;

    char cur[WORD_MAX_LEN];
    int len = 0;

    for (int i = 0; ; i++) {
        unsigned char ch = (unsigned char)text[i];
        int done = (ch == 0);

        if (!done && (isalnum(ch) || ch == '_')) {
            if (len < WORD_MAX_LEN - 1) {
                cur[len++] = (char)tolower(ch);
            }
        } else {
            if (len > 0) {
                cur[len] = '\0';
                normalize_token(cur);
                if (!is_stopword(cur)) {
                    strncpy(tokens[out], cur, WORD_MAX_LEN - 1);
                    tokens[out][WORD_MAX_LEN - 1] = '\0';
                    out++;
                    if (out >= max_tokens) return out;
                }
                len = 0;
            }
            if (done) break;
        }
    }

    return out;
}

void tokenize_free_tokens(char** tokens, int count) {
    if (!tokens) return;
    for (int i = 0; i < count; i++) free(tokens[i]);
    free(tokens);
}

int tokenize_text_dynamic(const char* text, char*** out_tokens, int* out_count) {
    if (!out_tokens || !out_count) return 0;
    *out_tokens = NULL;
    *out_count = 0;
    if (!text) return 1;

    int cap = 64;
    char** toks = (char**)malloc((size_t)cap * sizeof(char*));
    if (!toks) return 0;
    int n = 0;

    char cur[WORD_MAX_LEN];
    int len = 0;

    for (int i = 0; ; i++) {
        unsigned char ch = (unsigned char)text[i];
        int done = (ch == 0);

        if (!done && (isalnum(ch) || ch == '_')) {
            if (len < WORD_MAX_LEN - 1) cur[len++] = (char)tolower(ch);
        } else {
            if (len > 0) {
                cur[len] = '\0';
                normalize_token(cur);
                if (!is_stopword(cur) && cur[0] != '\0') {
                    if (n >= cap) {
                        cap *= 2;
                        char** grown = (char**)realloc(toks, (size_t)cap * sizeof(char*));
                        if (!grown) {
                            tokenize_free_tokens(toks, n);
                            return 0;
                        }
                        toks = grown;
                    }
                    toks[n] = (char*)malloc(strlen(cur) + 1);
                    if (!toks[n]) {
                        tokenize_free_tokens(toks, n);
                        return 0;
                    }
                    strcpy(toks[n], cur);
                    n++;
                }
                len = 0;
            }
            if (done) break;
        }
    }

    *out_tokens = toks;
    *out_count = n;
    return 1;
}

static void free_docs(void) {
    for (int i = 1; i <= g_doc_count; i++) {
        free(g_doc_labels ? g_doc_labels[i] : NULL);
        free(g_doc_texts ? g_doc_texts[i] : NULL);
        if (g_doc_labels) g_doc_labels[i] = NULL;
        g_doc_texts[i] = NULL;
    }
    g_doc_count = 0;
    g_total_tokens_indexed = 0;
    avl_free(g_avl_root);
    g_avl_root = NULL;
}

int get_document_count(void) {
    return g_doc_count;
}

const char* get_document_label(int docID) {
    if (docID <= 0 || docID > g_doc_count) return NULL;
    return g_doc_labels ? g_doc_labels[docID] : NULL;
}

const char* get_document_text(int docID) {
    if (docID <= 0 || docID > g_doc_count) return NULL;
    return g_doc_texts ? g_doc_texts[docID] : NULL;
}

int get_docID_by_label(const char* label) {
    if (!label) return -1;
    AVLNode* node = avl_search(g_avl_root, label);
    if (node) {
        return node->docID;
    }
    return -1;
}

void print_metadata_tree(void) {
    printf("\n===== AVL METADATA TREE STRUCTURE =====\n");
    if (!g_avl_root) {
        printf("(Tree is empty)\n");
    } else {
        avl_print_visual(g_avl_root, 0);
    }
    printf("========================================\n\n");
}

long long get_total_tokens_indexed(void) {
    return g_total_tokens_indexed;
}

static void rtrim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static void index_doc_text(const char* label, const char* text) {
    if (!text || !text[0]) return;
    if (!ensure_doc_capacity(g_doc_count + 1)) return;

    g_doc_count++;
    int docID = g_doc_count;

    if (label && label[0]) {
        size_t n = strlen(label);
        if (n > (size_t)DOC_LABEL_MAX_LEN - 1) n = (size_t)DOC_LABEL_MAX_LEN - 1;
        g_doc_labels[docID] = (char*)malloc(n + 1);
        if (g_doc_labels[docID]) {
            memcpy(g_doc_labels[docID], label, n);
            g_doc_labels[docID][n] = '\0';
        }
    } else {
        char tmp[DOC_LABEL_MAX_LEN];
        snprintf(tmp, sizeof(tmp), "DOC%d", docID);
        g_doc_labels[docID] = (char*)malloc(strlen(tmp) + 1);
        if (g_doc_labels[docID]) {
            strcpy(g_doc_labels[docID], tmp);
        }
    }

    /* Unit 1: Insert into AVL Metadata Tree for fast searching by name */
    const char* last_s = strrchr(g_doc_labels[docID], '\\');
    if (!last_s) last_s = strrchr(g_doc_labels[docID], '/');
    const char* filename = last_s ? last_s + 1 : g_doc_labels[docID];
    
    g_avl_root = avl_insert(g_avl_root, filename, docID);

    g_doc_texts[docID] = (char*)malloc(strlen(text) + 1);
    if (g_doc_texts[docID]) strcpy(g_doc_texts[docID], text);

    char** toks = NULL;
    int n = 0;
    if (!tokenize_text_dynamic(text, &toks, &n)) return;
    for (int i = 0; i < n; i++) {
        g_total_tokens_indexed += 1;
        insert_word(toks[i], docID);
        insert_trie(toks[i]);
    }
    tokenize_free_tokens(toks, n);
}

static void file_loader_sink(const char* label, const char* text, void* user) {
    (void)user;
    index_doc_text(label, text);
}

void tokenize_document(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Error: cannot open %s\n", filename);
        return;
    }

    free_docs();

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        rtrim_newline(line);
        if (line[0] == '\0') continue;

        char* colon = strchr(line, ':');
        if (!colon) continue;

        if (!ensure_doc_capacity(g_doc_count + 1)) break;
        g_doc_count++;
        int docID = g_doc_count;

        /* label: everything before ':' */
        int label_len = (int)(colon - line);
        if (label_len <= 0) {
            char tmp[DOC_LABEL_MAX_LEN];
            snprintf(tmp, sizeof(tmp), "DOC%d", docID);
            g_doc_labels[docID] = (char*)malloc(strlen(tmp) + 1);
            if (g_doc_labels[docID]) strcpy(g_doc_labels[docID], tmp);
        } else {
            int copy = label_len;
            if (copy > DOC_LABEL_MAX_LEN - 1) copy = DOC_LABEL_MAX_LEN - 1;
            g_doc_labels[docID] = (char*)malloc((size_t)copy + 1);
            if (g_doc_labels[docID]) {
                memcpy(g_doc_labels[docID], line, (size_t)copy);
                g_doc_labels[docID][copy] = '\0';
            }
        }

        /* Unit 1: AVL Metadata indexing */
        const char* last_s = strrchr(g_doc_labels[docID], '\\');
        if (!last_s) last_s = strrchr(g_doc_labels[docID], '/');
        const char* filename = last_s ? last_s + 1 : g_doc_labels[docID];

        g_avl_root = avl_insert(g_avl_root, filename, docID);

        /* store full text (after ':') for display/snippets if desired */
        const char* body = colon + 1;
        while (*body == ' ' || *body == '\t') body++;
        g_doc_texts[docID] = (char*)malloc(strlen(body) + 1);
        if (g_doc_texts[docID]) strcpy(g_doc_texts[docID], body);

        char** toks = NULL;
        int n = 0;
        if (!tokenize_text_dynamic(body, &toks, &n)) continue;
        for (int i = 0; i < n; i++) {
            g_total_tokens_indexed += 1;
            insert_word(toks[i], docID);
            insert_trie(toks[i]);
        }
        tokenize_free_tokens(toks, n);
    }

    fclose(f);
}

void tokenize_directory(const char* root_dir) {
    if (!root_dir || !root_dir[0]) return;

    free_docs();

    file_loader_set_sink(file_loader_sink, NULL);
    (void)load_documents_from_directory(root_dir);
}

