#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index.h"

#define HASH_SIZE 4096

static WordEntry *g_table[HASH_SIZE];

static unsigned long hash_word(const char *s)
{
    unsigned long h = 5381UL;
    int c;
    while ((c = (unsigned char)*s++) != 0)
    {
        h = ((h << 5) + h) + (unsigned long)c;
    }
    return h;
}

void index_init(void)
{
    for (int i = 0; i < HASH_SIZE; i++)
        g_table[i] = NULL;
}

static void free_positions(PositionNode *pos)
{
    while (pos)
    {
        PositionNode *next = pos->next;
        free(pos);
        pos = next;
    }
}

static void free_postings(Posting *p)
{
    while (p)
    {
        Posting *next = p->next;
        free_positions(p->positions);
        p->positions = NULL;
        free(p);
        p = next;
    }
}

void index_free(void)
{
    for (int i = 0; i < HASH_SIZE; i++)
    {
        WordEntry *e = g_table[i];
        while (e)
        {
            WordEntry *next = e->next;
            free_postings(e->postingList);
            free(e);
            e = next;
        }
        g_table[i] = NULL;
    }
}

static WordEntry *find_entry(const char *word, unsigned long *out_bucket)
{
    unsigned long h = hash_word(word) % (unsigned long)HASH_SIZE;
    if (out_bucket)
        *out_bucket = h;
    WordEntry *cur = g_table[h];
    while (cur)
    {
        if (strcmp(cur->word, word) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

static Posting *find_posting(Posting *head, int docID, Posting **out_prev)
{
    Posting *prev = NULL;
    Posting *cur = head;
    while (cur && cur->docID < docID)
    {
        prev = cur;
        cur = cur->next;
    }
    if (out_prev)
        *out_prev = prev;
    if (cur && cur->docID == docID)
        return cur;
    return NULL;
}

void insert_word(char *word, int docID)
{
    insert_term(word, docID, -1);
}

void insert_term(const char *word, int docID, int position)
{
    if (!word || !word[0])
        return;
    if (docID <= 0)
        return;

    unsigned long bucket = 0;
    WordEntry *e = find_entry(word, &bucket);
    if (!e)
    {
        e = (WordEntry *)malloc(sizeof(WordEntry));
        if (!e)
            return;
        memset(e, 0, sizeof(WordEntry));
        strncpy(e->word, word, WORD_MAX_LEN - 1);
        e->documentFrequency = 0;
        e->postingList = NULL;
        e->next = g_table[bucket];
        g_table[bucket] = e;
    }

    Posting *prev = NULL;
    Posting *p = find_posting(e->postingList, docID, &prev);
    if (p)
    {
        p->frequency += 1;
        if (position >= 0)
        {
            /* Positional indexing is prepared but not used yet. */
            PositionNode *node = (PositionNode *)malloc(sizeof(PositionNode));
            if (!node)
                return;
            node->position = position;
            node->next = p->positions;
            p->positions = node;
        }
        return;
    }

    /* Insert new posting in sorted order by docID for fast intersections. */
    Posting *n = (Posting *)malloc(sizeof(Posting));
    if (!n)
        return;
    n->docID = docID;
    n->frequency = 1;
    n->positions = NULL;
    if (position >= 0)
    {
        PositionNode *node = (PositionNode *)malloc(sizeof(PositionNode));
        if (!node)
        {
            free(n);
            return;
        }
        node->position = position;
        node->next = NULL;
        n->positions = node;
    }
    e->documentFrequency += 1;
    if (!prev)
    {
        n->next = e->postingList;
        e->postingList = n;
    }
    else
    {
        n->next = prev->next;
        prev->next = n;
    }
}

Posting *get_postings(char *word)
{
    if (!word || !word[0])
        return NULL;
    WordEntry *e = find_entry(word, NULL);
    return e ? e->postingList : NULL;
}

int get_document_frequency(const char *word)
{
    if (!word || !word[0])
        return 0;
    WordEntry *e = find_entry(word, NULL);
    return e ? e->documentFrequency : 0;
}

int get_vocabulary_size(void)
{
    int count = 0;
    for (int i = 0; i < HASH_SIZE; i++)
    {
        for (WordEntry *e = g_table[i]; e; e = e->next)
            count++;
    }
    return count;
}

void debug_word(const char *word) {
    if (!word) return;
    unsigned long bucket = 0;
    WordEntry *e = find_entry(word, &bucket);
    
    printf("\n--- INVERTED INDEX DEBUG: '%s' ---\n", word);
    if (!e) {
        printf("Word not found in index.\n");
        return;
    }

    printf("Hash Bucket: %lu\n", bucket);
    printf("Doc Frequency: %d\n", e->documentFrequency);
    printf("Posting List (Linked List):\n");
    
    Posting *p = e->postingList;
    while (p) {
        printf("  [DocID: %d | Freq: %d] --> ", p->docID, p->frequency);
        p = p->next;
    }
    printf("NULL\n\n");
}

void debug_print_index()
{
    printf("\n===== FULL INVERTED INDEX =====\n");

    for (int i = 0; i < HASH_SIZE; i++)
    {
        if (g_table[i] != NULL)
        {
            printf("\n[Bucket %d]\n", i);

            WordEntry *e = g_table[i];

            while (e)
            {
                printf("  Word: %s | DF: %d | Hash: %lu",
                       e->word,
                       e->documentFrequency,
                       hash_word(e->word) % HASH_SIZE);

                // show linkage clearly
                if (e->next)
                {
                    printf("  ---> next");
                }

                printf("\n");

                Posting *p = e->postingList;
                while (p)
                {
                    printf("    -> DocID: %d | Freq: %d\n", p->docID, p->frequency);

                    PositionNode *pos = p->positions;
                    if (pos)
                    {
                        printf("       Positions: ");
                        while (pos)
                        {
                            printf("%d ", pos->position);
                            pos = pos->next;
                        }
                        printf("\n");
                    }

                    p = p->next;
                }

                e = e->next;
            }
        }
    }
}